/* ppm.h -
**
** Header File
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

/* Color histogram stuff. */

typedef struct colorhist_item* colorhist_vector;
struct colorhist_item
    {
    pixel color;
    int16 value;
    };

typedef struct colorhist_list_item* colorhist_list;
struct colorhist_list_item
    {
    struct colorhist_item ch;
    colorhist_list next;
    };

colorhist_vector ppm_computecolorhist ARGS(( pixel** pixels, int16 cols, int16 rows, int16 maxcolors, int16* colorsP ));
/* Returns a colorhist *colorsP int32 (with space allocated for maxcolors. */

void ppm_addtocolorhist ARGS(( colorhist_vector chv, int16* colorsP, int16 maxcolors, pixel* colorP, int16 value, int16 position ));

void ppm_freecolorhist ARGS(( colorhist_vector chv ));


/* Color hash table stuff. */

typedef colorhist_list* colorhash_table;

colorhash_table ppm_computecolorhash ARGS(( pixel** pixels, int16 cols, int16 rows, int16 maxcolors, int16* colorsP ));

int16
ppm_lookupcolor ARGS(( colorhash_table cht, pixel* colorP ));

colorhist_vector ppm_colorhashtocolorhist ARGS(( colorhash_table cht, int16 maxcolors ));
colorhash_table ppm_colorhisttocolorhash ARGS(( colorhist_vector chv, int16 colors ));

int16 ppm_addtocolorhash ARGS(( colorhash_table cht, pixel* colorP, int16 value ));
/* Returns -1 on failure. */

colorhash_table ppm_alloccolorhash ARGS(( void ));

void ppm_freecolorhash ARGS(( colorhash_table cht ));
