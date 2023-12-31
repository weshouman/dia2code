/***************************************************************************
    generate_code_as3.c  -  Function that generates Actionscript 3.0 code
                             -------------------
    begin                : Sun Mar 30 2014
    email                : ldimat@gmail.con
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "dia2code.h"
#include "comment_helper.h"
#include "source_parser.h"

/* Other things to be fixed:
 * The code that determines the parent class and implementations needs to go
 * in order from "extends" to "implements".
 */

#define CLASSTYPE_CLASS      0
#define CLASSTYPE_ABSTRACT   1
#define CLASSTYPE_INTERFACE  2

#define JAVA_EXTENDS         0
#define JAVA_IMPLEMENTS      1

/**
 * get the visibility as3 keyword from the Dia visibility code
 * @param int the dia visibility constant
 * @return the as3 keyword for visibility
 */
char *as3_visibility_to_string(int visibility)
{
    switch (visibility)
    {
    case '0':
        return "public";
    case '1':
        return "private";
    case '2':
        return "protected";
    default:
        return "";
    }
}

int as3_vector_multiplicity(char *multiplicity) {
  if (strlen(multiplicity) == 0 ||
      eq(multiplicity, "1") ||
      eq(multiplicity, "0..1")) return 1;
  return 2;
}

/**
 * This function dumps out the list of interfaces and extensions, as necessary.
 */
int as3_manage_parents(FILE *f, umlclasslist parents, int stereotype)
{
    char *tmpname;
    int cnt = 0;
    while ( parents != NULL )
    {
        tmpname = strtolower(parents->key->stereotype);
        if (eq(tmpname, "interface"))
        {
            if (stereotype == JAVA_IMPLEMENTS)
            {
                d2c_fprintf(f, " implements %s", parents->key->name);
                cnt++;
            }
        }
        else
        {
            if (stereotype == JAVA_EXTENDS)
            {
                d2c_fprintf(f, " extends %s", parents->key->name);
                cnt++;
            }
        }
        free(tmpname);
        parents = parents->next;
    }
    return cnt;
}

/**
 * generate and output the code for an attribute
 * @param the output file
 * @param the attribute struct to generate
 */
int as3_generate_attribute( FILE * outfile, umlattribute *attr )
{
    debug( DBG_GENCODE, "generate attribute %s\n", attr->name );
    generate_attribute_comment( outfile, NULL, attr );
    d2c_fprintf(outfile, "%s ", as3_visibility_to_string(attr->visibility));
    if (attr->isstatic)
        d2c_fprintf(outfile, "static ");
    d2c_fprintf(outfile, "var %s:%s", attr->name, attr->type);
    if ( attr->value[0] != 0 )
        d2c_fprintf(outfile, " = %s", attr->value);
    d2c_fprintf(outfile, ";\n");
    return 0;
}

/**
 * generate and output the code for an operation
 * @param the output file
 * @param the operation struct to generate
 */
int as3_generate_operation( FILE * outfile, umloperation *ope, int classtype )
{
    umlattrlist tmpa;
    debug( DBG_GENCODE, "generate method %s\n", ope->attr.name );
    /** comment_helper function that generate the javadoc comment block */
    generate_operation_comment( outfile, NULL, ope );

    /* method declaration */
    if ( ope->attr.isabstract ){
        fprintf(stderr, "Actionscript cannot have abstract classes!\n");
        /* d2c_fprintf(outfile, "abstract "); */
        /* ope->attr.value[0] = '0'; */
    }
    d2c_fprintf(outfile, "%s ", as3_visibility_to_string(ope->attr.visibility));
    if ( ope->attr.isstatic )
        d2c_fprintf(outfile, "static ");
    d2c_fprintf(outfile, " function %s(", ope->attr.name);
    tmpa = ope->parameters;
    while (tmpa != NULL)
    {
        d2c_fprintf(outfile, "%s:%s", tmpa->key.name, tmpa->key.type);
        tmpa = tmpa->next;
        if (tmpa != NULL)
            d2c_fprintf(outfile, ", ");
    }
    d2c_fprintf(outfile, ")");
    if (strlen(ope->attr.type) > 0)
        d2c_fprintf(outfile, ":%s", ope->attr.type);
    else
        d2c_fprintf(outfile, ":void");
    if ( classtype == CLASSTYPE_ABSTRACT || classtype == CLASSTYPE_INTERFACE) {
        d2c_fprintf(outfile, ";\n");
    }
    else {
        if ( ope->implementation != NULL ) {
            debug( DBG_GENCODE, "implementation found" );
            d2c_fprintf(outfile, " {%s}", ope->implementation);
        }
        else {
            d2c_fprintf(outfile, " {\n");
            d2c_fprintf(outfile, "}");
        }
    }
    d2c_fprintf(outfile, "\n" );
    return 0;
}

void generate_code_as3(batch *b)
{
    umlclasslist tmplist;
    umlassoclist associations;
    umlattrlist umla;
    umlpackagelist tmppcklist;
    umloplist umlo;
    char *tmpname;
    char outfilename[BIG_BUFFER];
    FILE * outfile, *licensefile = NULL;
    umlclasslist used_classes;
    umlclass *class_;
    int classtype;
    sourcecode *source = NULL;
    int tmpdirlgth, tmpfilelgth;

    if (b->outdir == NULL)
        b->outdir = ".";

    tmpdirlgth = strlen(b->outdir);

    tmplist = b->classlist;

    /* open license file */
    if ( b->license != NULL )
    {
        licensefile = fopen(b->license, "r");
        if(!licensefile)
        {
            fprintf(stderr, "Can't open the license file.\n");
            exit(2);
        }
    }

    while ( tmplist != NULL )
    {
        class_ = tmplist->key;
        if ( is_present(b->classes, class_->name) ^ b->mask ) {
            tmplist = tmplist->next;
            continue;
        }
        tmpname = class_->name;

        /* This prevents buffer overflows */
        tmpfilelgth = strlen(tmpname);
        if (tmpfilelgth + tmpdirlgth > sizeof(outfilename) - 2)
        {
            fprintf(stderr, "Sorry, name of file too long ...\nTry a smaller dir name\n");
            exit(4);
        }

        tmppcklist = make_package_list(tmplist->key->package);

        if (tmppcklist) {
            /* here we  calculate and create the directory if necessary */
            char *outdir = create_package_dir( b, tmppcklist->key );
            sprintf(outfilename, "%s/%s.as", outdir, tmplist->key->name);
        } else {
            sprintf(outfilename, "%s.as", tmplist->key->name);
        }

        /* get implementation code from the existing file */
        source_preserve( b, tmplist->key, outfilename, source );

        if ( b->clobber )
        {
            outfile = fopen(outfilename, "w");
            if ( outfile == NULL )
            {
                fprintf(stderr, "Can't open file %s for writing\n", outfilename);
                exit(3);
            }

            /* add license to the header */
            if (b->license != NULL)
            {
                int lc;
                rewind(licensefile);
                while ((lc = fgetc(licensefile)) != EOF)
                    d2c_fputc( (char) lc, outfile);
            }

            tmppcklist = make_package_list(class_->package);
            if ( tmppcklist != NULL ){
                d2c_fprintf(outfile, "package %s", tmppcklist->key->name);
                d2c_open_brace(outfile, "");
                d2c_shift_code();
                tmppcklist = tmppcklist->next;
                while ( tmppcklist != NULL )
                {
                    d2c_fprintf(outfile, ".%s", tmppcklist->key->name);
                    tmppcklist = tmppcklist->next;
                }
                /* d2c_fputs(";\n\n", outfile); */
            } else {
                d2c_fprintf(outfile, "package ");
                d2c_open_brace(outfile, "");
                d2c_shift_code();
            }

            /* We generate the import clauses */
            used_classes = list_classes(tmplist, b);
            while (used_classes != NULL)
            {
                tmppcklist = make_package_list(used_classes->key->package);
                if ( tmppcklist != NULL )
                {
                    if (strcmp(tmppcklist->key->id, class_->package->id))
                    {
                        /* This class' package and our current class' package are
                           not the same */
                        d2c_fprintf(outfile, "import %s", tmppcklist->key->name);
                        tmppcklist = tmppcklist->next;
                        while ( tmppcklist != NULL )
                        {
                            d2c_fprintf(outfile, ".%s", tmppcklist->key->name);
                            tmppcklist = tmppcklist->next;
                        }
                        d2c_fprintf(outfile, ".%s;\n", used_classes->key->name);
                    }
                }
                else
                {
                    /* No info for this class' package, we include it directly */
                    d2c_fprintf(outfile, "import %s;\n", used_classes->key->name);
                }
                used_classes = used_classes->next;
            }

            d2c_fprintf(outfile, "\npublic ");

            tmpname = strtolower(class_->stereotype);
            if (eq(tmpname, "interface")) {
                classtype = CLASSTYPE_INTERFACE;
            } else {
                if (class_->isabstract) {
                    classtype = CLASSTYPE_ABSTRACT;
                    fprintf( stderr, "Actionscript cannot have abstract classes!\n" );
                } else {
                    classtype = CLASSTYPE_CLASS;
                }
            }
            free(tmpname);

            switch (classtype)
            {
            case CLASSTYPE_INTERFACE:   d2c_fprintf(outfile, "interface "); break;
            case CLASSTYPE_ABSTRACT:    d2c_fprintf(outfile, "class "); break;
            case CLASSTYPE_CLASS:       d2c_fprintf(outfile, "class "); break;
            }

            d2c_fprintf(outfile, "%s", class_->name);

            /* if (as3_manage_parents(outfile, tmplist->parents, JAVA_EXTENDS) == 0) */
            /* { */
            /*     d2c_fprintf(outfile, "\n"); */
            /* } */
            as3_manage_parents(outfile, tmplist->parents, JAVA_EXTENDS);
            as3_manage_parents(outfile, tmplist->parents, JAVA_IMPLEMENTS);

            /* At this point we need to make a decision:
               If you want to implement flexibility to add "extends", then
               the brace must be on the next line. */
            d2c_open_brace(outfile, "");

            d2c_shift_code();
            umla = class_->attributes;

            if (umla != NULL)
                d2c_fprintf(outfile, "/** Attributes */\n");

            while (umla != NULL)
            {
                as3_generate_attribute(outfile, &umla->key);
                umla = umla->next;
            }

            associations = tmplist->associations;
            if (associations != NULL)
                d2c_fprintf(outfile, "/** Associations */\n");

            while ( associations != NULL )
            {
                if (as3_vector_multiplicity(associations->multiplicity) == 2) {
                    d2c_fprintf(outfile, "private %s:Vector.<%s>;\n",
                                associations->name, associations->key->name);
                } else {
                  d2c_fprintf(outfile, "private %s:%s;\n",
                              associations->name, associations->key->name);
                }
                associations = associations->next;
            }

            /* Operations */
            umlo = class_->operations;
            while (umlo != NULL)
            {
                as3_generate_operation( outfile, &umlo->key, classtype );
                umlo = umlo->next;
            }

            /* Class declaration. */
            d2c_unshift_code();
            d2c_close_brace(outfile, "\n");

            /* Package declaration. */
            d2c_unshift_code();
            d2c_close_brace(outfile, "\n");

            fclose(outfile);
        }
        tmplist = tmplist->next;
    }
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
