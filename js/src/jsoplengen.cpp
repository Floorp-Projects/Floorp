/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
    const char  *name;
    int         length;
} pairs[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)               \
    { #op, length } ,
#include "jsopcode.tbl"
#undef OPDEF
};

int
main(int argc, char **argv)
{
    FILE *fp;
    size_t maxNameWidth, i, nameWidth, tabStop;
    int lengthGap;

    static const char prefix[] = "#define ";
    static const char suffix[] = "_LENGTH";
    static const size_t tabWidth = 8;
    static const size_t prefixWidth = sizeof(prefix) - 1;
    static const size_t suffixWidth = sizeof(suffix) - 1;

    if (argc != 2) {
        fputs("Bad usage\n", stderr);
        return EXIT_FAILURE;
    }

    fp = fopen(argv[1], "w");
    if (!fp) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    fputs("/*\n"
          " * Automatically generated header with JS opcode length constants.\n"
          " *\n"
          " * Do not edit it, alter jsopcode.tbl instead.\n"
          " */\n",
          fp);

    /*
     * Print
     *
     * #define name_LENGTH length
     *
     * with all length values aligned on the same column. The column is at the
     * second character position after a tab-stop with the first position
     * reserved for the minus sign of variable-length opcodes.
     */
    maxNameWidth = 0;
    for (i = 0; i != sizeof pairs / sizeof pairs[0]; ++i) {
        nameWidth = strlen(pairs[i].name);
        if (maxNameWidth < nameWidth)
            maxNameWidth = nameWidth;
    }

    tabStop = prefixWidth + maxNameWidth + suffixWidth + 1;
    tabStop = (tabStop + tabWidth - 1) / tabWidth * tabWidth;
    for (i = 0; i != sizeof pairs / sizeof pairs[0]; ++i) {
        lengthGap = (int) (tabStop - prefixWidth - strlen(pairs[i].name) -
                           suffixWidth);
        fprintf(fp, "%s%s%s%*c%2d\n",
                prefix, pairs[i].name, suffix, lengthGap, ' ',
                pairs[i].length);
        if (ferror(fp)) {
            perror("fclose");
            exit(EXIT_FAILURE);
        }
    }

    if (fclose(fp)) {
        perror("fclose");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
