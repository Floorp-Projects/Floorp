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

/*modified by Netscape*/

#ifndef _PPM_H_
#define _PPM_H_

/*#include "pgm.h" JUDGE*/
typedef unsigned char gray;

typedef gray pixval;

#ifdef PPM_PACKCOLORS

#define PPM_MAXMAXVAL 1023
typedef uint32 pixel;
#define PPM_GETR(p) (((p) & 0x3ff00000) >> 20)
#define PPM_GETG(p) (((p) & 0xffc00) >> 10)
#define PPM_GETB(p) ((p) & 0x3ff)
#define PPM_ASSIGN(p,red,grn,blu) (p) = ((pixel) (red) << 20) | ((pixel) (grn) << 10) | (pixel) (blu)
#define PPM_EQUAL(p,q) ((p) == (q))

#else /*PPM_PACKCOLORS*/

#define PPM_MAXMAXVAL PGM_MAXMAXVAL
typedef struct
    {
    pixval r, g, b;
    } pixel;
#define PPM_GETR(p) ((p).r)
#define PPM_GETG(p) ((p).g)
#define PPM_GETB(p) ((p).b)
#define PPM_ASSIGN(p,red,grn,blu) do { (p).r = (red); (p).g = (grn); (p).b = (blu); } while ( 0 )
#define PPM_EQUAL(p,q) ( (p).r == (q).r && (p).g == (q).g && (p).b == (q).b )

#endif /*PPM_PACKCOLORS*/


/* Magic constants. */

#define PPM_MAGIC1 'P'
#define PPM_MAGIC2 '3'
#define RPPM_MAGIC2 '6'
#define PPM_FORMAT (PPM_MAGIC1 * 256 + PPM_MAGIC2)
#define RPPM_FORMAT (PPM_MAGIC1 * 256 + RPPM_MAGIC2)
#define PPM_TYPE PPM_FORMAT


/* Macro for turning a format number into a type number. */

#define PPM_FORMAT_TYPE(f) ((f) == PPM_FORMAT || (f) == RPPM_FORMAT ? PPM_TYPE : PGM_FORMAT_TYPE(f))


/* Declarations of routines. */

void ppm_init ARGS(( int16* argcP, char* argv[] ));

#define ppm_allocarray( cols, rows ) ((pixel**) pm_allocarray( cols, rows, sizeof(pixel) ))
#define ppm_allocrow( cols ) ((pixel*) pm_allocrow( cols, sizeof(pixel) ))
#define ppm_freearray( pixels, rows ) pm_freearray( (char**) pixels, rows )
#define ppm_freerow( pixelrow ) pm_freerow( (char*) pixelrow )

pixel** ppm_readppm ARGS(( FILE* file, int16* colsP, int16* rowsP, pixval* maxvalP ));
void ppm_readppminit ARGS(( FILE* file, int16* colsP, int16* rowsP, pixval* maxvalP, int16* formatP ));
void ppm_readppmrow ARGS(( FILE* file, pixel* pixelrow, int16 cols, pixval maxval, int16 format ));

void ppm_writeppm ARGS(( FILE* file, pixel** pixels, int16 cols, int16 rows, pixval maxval, int16 forceplain ));
void ppm_writeppminit ARGS(( FILE* file, int16 cols, int16 rows, pixval maxval, int16 forceplain ));
void ppm_writeppmrow ARGS(( FILE* file, pixel* pixelrow, int16 cols, pixval maxval, int16 forceplain ));

pixel ppm_parsecolor ARGS(( char* colorname, pixval maxval ));
char* ppm_colorname ARGS(( pixel* colorP, pixval maxval, int16 hexok ));

extern pixval ppm_pbmmaxval;
/* This is the maxval used when a PPM program reads a PBM file.  Normally
** it is 1; however, for some programs, a larger value gives better results
*/


/* Color scaling macro -- to make writing ppmtowhatever easier. */

#define PPM_DEPTH(newp,p,oldmaxval,newmaxval) \
    PPM_ASSIGN( (newp), \
	( (int16) PPM_GETR(p) * (newmaxval) + (oldmaxval) / 2 ) / (oldmaxval), \
	( (int16) PPM_GETG(p) * (newmaxval) + (oldmaxval) / 2 ) / (oldmaxval), \
	( (int16) PPM_GETB(p) * (newmaxval) + (oldmaxval) / 2 ) / (oldmaxval) )


/* Luminance macro. */

#define PPM_LUMIN(p) ( 0.299 * PPM_GETR(p) + 0.587 * PPM_GETG(p) + 0.114 * PPM_GETB(p) )

#endif /*_PPM_H_*/
