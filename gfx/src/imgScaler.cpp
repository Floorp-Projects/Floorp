/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Tomas Mšller.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tomas Mšller
 *   Tim Rowley <tor@cs.brown.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include "imgScaler.h"

// Scaling code from Graphics Gems book III                     
// http://www.acm.org/pubs/tog/GraphicsGems/gemsiii/fastBitmap.c
//
// License states - "All code here can be used without restrictions."
// http://www.acm.org/pubs/tog/GraphicsGems/

/*
Fast Bitmap Stretching
Tomas Mšller
*/

static void
Stretch32(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	  unsigned yr, unsigned yw,
	  unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	  unsigned char *aSrcImage, unsigned aSrcStride,
	  unsigned char *aDstImage, unsigned aDstStride);

static void
Stretch24(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	  unsigned yr, unsigned yw,
	  unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	  unsigned char *aSrcImage, unsigned aSrcStride,
	  unsigned char *aDstImage, unsigned aDstStride);

static void
Stretch8(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	 unsigned yr, unsigned yw,
	 unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	 unsigned char *aSrcImage, unsigned aSrcStride,
	 unsigned char *aDstImage, unsigned aDstStride);

static void
Stretch1(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	 unsigned yr, unsigned yw,
	 unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	 unsigned char *aSrcImage, unsigned aSrcStride,
	 unsigned char *aDstImage, unsigned aDstStride);

/**********************************************************
 RectStretch enlarges or diminishes a source rectangle of a bitmap to
 a destination rectangle. The source rectangle is selected by the two
 points (xs1,ys1) and (xs2,ys2), and the destination rectangle by
 (xd1,yd1) and (xd2,yd2). Since readability of source-code is wanted,
 some optimizations have been left out for the reader: It's possible
 to read one line at a time, by first stretching in x-direction and
 then stretching that bitmap in y-direction.

 Entry:
    aSrcWidth, aSrcHeight - size of entire original image
    aDstWidth, aDstHeight - size of entire scaled image

    aStartColumn, aStartRow - upper corner of desired area (inclusive)
    aEndColumn, aEndRow - bottom corner of desired area (inclusive)
    
    unsigned char *aSrcImage, aSrcStride - start of original image data
    unsigned char *aDstStride, aDstStride - start of desired area image data

    unsigned aDepth - bit depth of image (24, 8, or 1)

**********************************************************/
NS_GFX_(void)
RectStretch(unsigned aSrcWidth, unsigned aSrcHeight,
	    unsigned aDstWidth, unsigned aDstHeight,
	    unsigned aStartColumn, unsigned aStartRow,
	    unsigned aEndColumn, unsigned aEndRow,
	    unsigned char *aSrcImage, unsigned aSrcStride,
	    unsigned char *aDstImage, unsigned aDstStride,
	    unsigned aDepth)
{
    int e;
    unsigned dx, dy;
    void (*Stretch)(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
		    unsigned yr, unsigned yw,
		    unsigned aStartRow, unsigned aStartColumn,
		    unsigned aEndColumn,
		    unsigned char *aSrcImage, unsigned aSrcStride,
		    unsigned char *aDstImage, unsigned aDstStride);

    unsigned xs1, ys1, xs2, ys2, xd1, yd1, xd2, yd2;

    xs1 = ys1 = xd1 = yd1 = 0;
    xs2 = aSrcWidth  - 1;
    ys2 = aSrcHeight - 1;
    xd2 = aDstWidth  - 1;
    yd2 = aDstHeight - 1;

//    fprintf(stderr, "RS (%d %d)-(%d %d) (%d %d)-(%d %d) %d %d %d\n",
//	    xs1, ys1, xs2, ys2, xd1, yd1, xd2, yd2,
//	    aSrcStride, aDstStride, aDepth);

    switch (aDepth) {
    case 32:
	Stretch = Stretch32;
	break;
    case 24:
	Stretch = Stretch24;
	break;
    case 8:
	Stretch = Stretch8;
	break;
    case 1:
	Stretch = Stretch1;
	break;
    default:
	return;
    }
    dx = yd2 - yd1;
    dy = ys2 - ys1;
    e = dy - dx;
    dy += 1;
    if (!dx)
	dx = 1;
    for (yd1 = 0; yd1 <= aEndRow; yd1++) {
	if (yd1 >= aStartRow)
	    Stretch(xd1, xd2, xs1, xs2, ys1, yd1,
		    aStartRow, aStartColumn, aEndColumn,
		    aSrcImage, aSrcStride, aDstImage, aDstStride);
	while (e >= 0) {
	    ys1++;
	    e -= dx;
	}
	e += dy;
    }
}

/**********************************************************
 Stretches a horizontal source line onto a horizontal destination
 line. Used by RectStretch.

 Entry:
	x1,x2 - x-coordinates of the destination line
	y1,y2 - x-coordinates of the source line
	yr    - y-coordinate of source line
	yw    - y-coordinate of destination line
**********************************************************/

static void
Stretch32(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	  unsigned yr, unsigned yw,
	  unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	  unsigned char *aSrcImage, unsigned aSrcStride,
	  unsigned char *aDstImage, unsigned aDstStride)
{
    int e;
    unsigned dx, dy, d;
    unsigned char *src, *dst;

    dx = x2 - x1;
    dy = y2 - y1;
    e = dy - dx;
    dy += 1;
    src = aSrcImage + yr * aSrcStride + 4 * y1;
    dst = aDstImage + (yw - aStartRow) * aDstStride;
    if (!dx)
	dx = 1;
    for (d = 0; d <= aEndColumn; d++) {
	if (d >= aStartColumn) {
	    *dst++ = src[0];
	    *dst++ = src[1];
	    *dst++ = src[2];
	    *dst++ = src[3];
	}
	while (e >= 0) {
	    src += 4;
	    e -= dx;
	}
	e += dy;
    }
}

static void
Stretch24(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	  unsigned yr, unsigned yw,
	  unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	  unsigned char *aSrcImage, unsigned aSrcStride,
	  unsigned char *aDstImage, unsigned aDstStride)
{
    int e;
    unsigned dx, dy, d;
    unsigned char *src, *dst;

    dx = x2 - x1;
    dy = y2 - y1;
    e = dy - dx;
    dy += 1;
    src = aSrcImage + yr * aSrcStride + 3 * y1;
    dst = aDstImage + (yw - aStartRow) * aDstStride;
    if (!dx)
	dx = 1;
    for (d = 0; d <= aEndColumn; d++) {
	if (d >= aStartColumn) {
	    *dst++ = src[0];
	    *dst++ = src[1];
	    *dst++ = src[2];
	}
	while (e >= 0) {
	    src += 3;
	    e -= dx;
	}
	e += dy;
    }
}

static void
Stretch8(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	 unsigned yr, unsigned yw,
	 unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	 unsigned char *aSrcImage, unsigned aSrcStride,
	 unsigned char *aDstImage, unsigned aDstStride)
{
    int e;
    unsigned dx, dy, d;
    unsigned char *src, *dst;

    dx = x2 - x1;
    dy = y2 - y1;
    e = dy - dx;
    dy += 1;
    src = aSrcImage + yr * aSrcStride + y1;
    dst = aDstImage + (yw - aStartRow) * aDstStride;
    if (!dx)
	dx = 1;
    for (d = 0; d <= aEndColumn; d++) {
	if (d >= aStartColumn) {
	    *dst = *src;
	    dst++;
	}
	while (e >= 0) {
	    src++;
	    e -= dx;
	}
	e += dy;
    }
}

static void
Stretch1(unsigned x1, unsigned x2, unsigned y1, unsigned y2,
	 unsigned yr, unsigned yw,
	 unsigned aStartRow, unsigned aStartColumn, unsigned aEndColumn,
	 unsigned char *aSrcImage, unsigned aSrcStride,
	 unsigned char *aDstImage, unsigned aDstStride)
{
    int e;
    unsigned dx, dy, d;

    dx = x2 - x1;
    dy = y2 - y1;
    e = dy - dx;
    dy += 1;
    if (!dx)
	dx = 1;
    for (d = 0; d <= aEndColumn; d++) {
	if ((d >= aStartColumn) &&
	    (*(aSrcImage + yr * aSrcStride + (y1 >> 3)) & 1 << (7 - y1 & 0x7)))
	    *(aDstImage +
	      (yw - aStartRow) * aDstStride +
	      ((x1 - aStartColumn) >> 3)) 
		|= 1 << (7 - x1 & 0x7);
	while (e >= 0) {
	    y1++;
	    e -= dx;
	}
	x1++;
	e += dy;
    }
}
