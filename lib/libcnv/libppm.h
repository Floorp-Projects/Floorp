/* libppm.h -
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

#ifndef _LIBPPM_H_
#define _LIBPPM_H_

/* Here are some routines internal to the ppm library. */

void ppm_readppminitrest ARGS(( FILE* file, int16* colsP, int16* rowsP, pixval* maxvalP ));

#endif /*_LIBPPM_H_*/
