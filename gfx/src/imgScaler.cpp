/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Tomas Mšller
 * Portions created by Tomas Mšller are 
 * Copyright (C) 2001 Tomas Mšller.  Rights Reserved.
 * 
 * Contributor(s): 
 *   Tomas Mšller
 *   Tim Rowley <tor@cs.brown.edu>
 */

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

#define sign(x) ((x)>0 ? 1:-1)

void RectStretch(long xs1,long ys1,long xs2,long ys2,
		 long xd1,long yd1,long xd2,long yd2,
		 unsigned char *aSrcImage, unsigned aSrcStride,
		 unsigned char *aDstImage, unsigned aDstStride,
		 unsigned aDepth);

static void Stretch24(long x1,long x2,long y1,long y2,long yr,long yw,
                      unsigned char *aSrcImage, unsigned aSrcStride,
                      unsigned char *aDstImage, unsigned aDstStride);
static void Stretch8(long x1,long x2,long y1,long y2,long yr,long yw,
                     unsigned char *aSrcImage, unsigned aSrcStride,
                     unsigned char *aDstImage, unsigned aDstStride);
static void Stretch1(long x1,long x2,long y1,long y2,long yr,long yw,
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
	xs1,ys1 - first point of source rectangle
	xs2,ys2 - second point of source rectangle
	xd1,yd1 - first point of destination rectangle
	xd2,yd2 - second point of destination rectangle
**********************************************************/
void
RectStretch(long xs1,long ys1,long xs2,long ys2,
	    long xd1,long yd1,long xd2,long yd2,
	    unsigned char *aSrcImage, unsigned aSrcStride,
	    unsigned char *aDstImage, unsigned aDstStride,
	    unsigned aDepth)
{
  long dx,dy,e,d,dx2;
  short sx,sy;
  void (*Stretch)(long x1,long x2,long y1,long y2,long yr,long yw,
		  unsigned char *aSrcImage, unsigned aSrcStride,
		  unsigned char *aDstImage, unsigned aDstStride);

//  fprintf(stderr, "(%ld %ld)-(%ld %ld) (%ld %ld)-(%ld %ld) %d %d %d\n",
//          xs1, ys1, xs2, ys2, xd1, yd1, xd2, yd2,
//          aSrcStride, aDstStride, aDepth);

  switch (aDepth) {
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
  dx = abs((int)(yd2-yd1));
  dy = abs((int)(ys2-ys1));
  sx = sign(yd2-yd1);
  sy = sign(ys2-ys1);
  e = dy-dx;
  dx2 = dx;
  dy += 1;
  if (!dx2) dx2=1;
  for (d=0; d<=dx; d++) {
    Stretch(xd1,xd2,xs1,xs2,ys1,yd1,aSrcImage,aSrcStride,aDstImage,aDstStride);
    while (e>=0) {
      ys1 += sy;
      e -= dx2;
    }
    yd1 += sx;
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
Stretch24(long x1,long x2,long y1,long y2,long yr,long yw,
	  unsigned char *aSrcImage, unsigned aSrcStride,
	  unsigned char *aDstImage, unsigned aDstStride)
{
  long dx,dy,e,d,dx2;
  short sx,sy;
  unsigned char *src, *dst;
  dx = abs((int)(x2-x1));
  dy = abs((int)(y2-y1));
  sx = 3*sign(x2-x1);
  sy = 3*sign(y2-y1);
  e = dy-dx;
  dx2 = dx;
  dy += 1;
  src=aSrcImage+yr*aSrcStride+3*y1;
  dst=aDstImage+yw*aDstStride+3*x1;
  if (!dx2) dx2=1;
  for (d=0; d<=dx; d++) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    while (e>=0) {
      src += sy;
      e -= dx2;
    }
    dst += sx;
    e += dy;
  }
}

static void
Stretch8(long x1,long x2,long y1,long y2,long yr,long yw,
	  unsigned char *aSrcImage, unsigned aSrcStride,
	  unsigned char *aDstImage, unsigned aDstStride)
{
  long dx,dy,e,d,dx2;
  short sx,sy;
  unsigned char *src, *dst;
  dx = abs((int)(x2-x1));
  dy = abs((int)(y2-y1));
  sx = sign(x2-x1);
  sy = sign(y2-y1);
  e = dy-dx;
  dx2 = dx;
  dy += 1;
  src=aSrcImage+yr*aSrcStride+y1;
  dst=aDstImage+yw*aDstStride+x1;
  if (!dx2) dx2=1;
  for (d=0; d<=dx; d++) {
    *dst = *src;
    while (e>=0) {
      src += sy;
      e -= dx2;
    }
    dst += sx;
    e += dy;
  }
}

static void
Stretch1(long x1,long x2,long y1,long y2,long yr,long yw,
	 unsigned char *aSrcImage, unsigned aSrcStride,
	 unsigned char *aDstImage, unsigned aDstStride)
{
  long dx,dy,e,d,dx2;
  short sx,sy;
  dx = abs((int)(x2-x1));
  dy = abs((int)(y2-y1));
  sx = sign(x2-x1);
  sy = sign(y2-y1);
  e = dy-dx;
  dx2 = dx;
  dy += 1;
  if (!dx2) dx2=1;
  for (d=0; d<=dx; d++) {
    if (*(aSrcImage+yr*aSrcStride+(y1>>3)) & 1<<(7-y1&0x7))
      *(aDstImage+yw*aDstStride+(x1>>3)) |= 1<<(7-x1&0x7);
    while (e>=0) {
      y1 += sy;
      e -= dx2;
    }
    x1 += sx;
    e += dy;
  }
}
