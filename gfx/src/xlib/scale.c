/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2:
 *
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Syd Logan <syd@netscape.com>
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Xlibified by <timecop@japan.co.jp>
 */

#include "drawers.h"

#include <stdio.h>
#include <string.h>
#include "nspr.h"
/* why do I need this define? */
#define USE_MOZILLA_TYPES
#include "xlibrgb.h"

XImage *
NNScaleImage(Display *display, XImage *img,
             double factorX, double factorY,
             PRInt32 aSWidth, PRInt32 aSHeight,
             PRInt32 newWidth, PRInt32 newHeight)
{
  XImage *newImg;
  PRUint32 pixel;
  PRInt16 i, j, xsrc, ysrc;
  PRUint32 size;
  char *data;

  newImg = XCreateImage(display, DefaultVisual(display, 0),
                        img->depth,
                        img->format,
                        0, 0, newWidth, newHeight,
                        img->bitmap_pad, 0);

  if (!newImg) {
    return NULL;
  }

  size = newHeight * newImg->bytes_per_line;
  data = (char *) PR_Malloc(size); 
  newImg->data = data;

  if (!data) {
    XDestroyImage(newImg);
    return NULL;
  }

  if (factorX == 1) {
    /* an obvious optimization is to bypass XPutPixel 
       and to memcpy rows in img->data if factorX is 1
    */
     
    char *dptr, *sptr;
    PRUint32 rowsize;
 
    dptr = newImg->data; 
    rowsize = img->bytes_per_line;
    for (i = 0; i < newHeight; i++) {
      ysrc = (PRInt16) (i * factorY);
      sptr = img->data + ysrc * rowsize;
      memcpy(dptr, sptr, PR_MIN(rowsize, newImg->bytes_per_line));
      dptr += newImg->bytes_per_line;
    }
  } else {
    for (i = 0; i < newWidth; i++) {
      xsrc = (PRInt16) (i * factorX);
      for (j = 0; j < newHeight; j++) {
        ysrc = (PRInt16) (j * factorY);
        pixel = XGetPixel(img, xsrc, ysrc);
        XPutPixel(newImg, i, j, pixel);
      }
    }
  }
  return newImg;
}


static PRBool
DoScale(Display *aDisplay,
        Drawable aDest,
        GC aGC,
        Drawable aSrc,
        PRInt32 aSrcWidth,
        PRInt32 aSrcHeight,
        PRInt32 aSX,
        PRInt32 aSY,
        PRInt32 aSWidth,
        PRInt32 aSHeight,
        PRInt32 aDX,
        PRInt32 aDY,
        PRInt32 aDWidth,
        PRInt32 aDHeight)
{
  XImage *srcImg = (XImage*)NULL;
  XImage *dstImg = (XImage*)NULL;

  double factorX = (double)aSrcWidth / (double)aDWidth;
  double factorY = (double)aSrcHeight / (double)aDHeight;

  srcImg = XGetImage(aDisplay, aSrc,
                     aSX, aSY, aSrcWidth, aSrcHeight,
                     AllPlanes, ZPixmap);

  if (!srcImg)
    return PR_FALSE;

  dstImg = NNScaleImage(aDisplay, srcImg, factorX, factorY,
			aSWidth, aSHeight,
			aDWidth, aDHeight);

  XDestroyImage(srcImg);
  srcImg = (XImage*)NULL;

  if (!dstImg)
    return PR_FALSE;

  XPutImage(aDisplay, aDest, aGC, dstImg,
            0, 0,
            aDX, aDY,
            aDWidth, aDHeight);

  PR_Free(dstImg->data); /* we allocated data, don't let Xlib free */
  dstImg->data = (char *) NULL; /* setting NULL is important!! */

  XDestroyImage(dstImg);

  return PR_TRUE;
}

PRBool
DrawScaledImageNN(Display *display,
                  Drawable aDest,
                  GC aGC,
                  Drawable aSrc,
                  Drawable aSrcMask,
                  PRInt32 aSrcWidth,
                  PRInt32 aSrcHeight,
                  PRInt32 aSX,
                  PRInt32 aSY,
                  PRInt32 aSWidth,
                  PRInt32 aSHeight,
                  PRInt32 aDX,
                  PRInt32 aDY,
                  PRInt32 aDWidth,
                  PRInt32 aDHeight)
{
  Pixmap alphaMask = 0;
  GC gc = 0;

  if (aSrcMask) {
    GC tmpgc;

    alphaMask = XCreatePixmap(display,
        DefaultRootWindow(display), aDWidth, aDHeight, 1);

    tmpgc = XCreateGC(display, alphaMask, 0, NULL);

    DoScale(display,
            alphaMask,      /* dest */
            tmpgc,
            aSrcMask,       /* src  */
            aSrcWidth, aSrcHeight,
            aSX, aSY,
            aSWidth, aSHeight,
            aDX, aDY,
            aDWidth, aDHeight);

    XFreeGC(display, tmpgc);

    gc = XCreateGC(display, aDest, 0, NULL);
    XCopyGC(display, aGC, GCFunction, gc);

    XSetClipMask(display, gc, alphaMask);
    XSetClipOrigin(display, gc, 0, 0);
  }

  DoScale(display,
          aDest,
          gc ? gc : aGC,
          aSrc,
          aSrcWidth, aSrcHeight,
          aSX, aSY,
          aSWidth, aSHeight,
          aDX, aDY,
          aDWidth, aDHeight);

  if (gc)
    XFreeGC(display, gc);
  if (alphaMask)
    XFreePixmap(display, alphaMask);

  return PR_TRUE;
}

#define SCALE_SHIFT 16

static void
pixops_scale_nearest (unsigned char *dest_buf,
                      int            render_x0,
                      int            render_y0,
                      int            render_x1,
                      int            render_y1,
                      int            dest_rowstride,
                      int            dest_channels,
                      const unsigned char *src_buf,
                      int            src_width,
                      int            src_height,
                      int            src_rowstride,
                      int            src_channels,
                      double         scale_x,
                      double         scale_y)
{
  int i, j;
  int x;
  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;

  printf("%f,%f\n", scale_x, scale_y);

  PR_ASSERT(src_channels == dest_channels);

  for (i = 0; i < (render_y1 - render_y0); i++) {
    const unsigned char *src  = src_buf + (((i + render_y0) * y_step + y_step / 2) >> SCALE_SHIFT) * src_rowstride;
    unsigned char *dest = dest_buf + i * dest_rowstride;

    x = render_x0 * x_step + x_step / 2;

    for (j=0; j < (render_x1 - render_x0); j++) {
      const unsigned char *p = src + (x >> SCALE_SHIFT) * src_channels;

      dest[0] = p[0];
      dest[1] = p[1];
      dest[2] = p[2];

      dest += dest_channels;
      x += x_step;
    }
  }
}

PRBool
DrawScaledImageBitsNN(Display *display,
                      Drawable aDest,
                      GC aGC,
                      const PRUint8 *aSrc,
                      PRInt32 aBytesPerRow,
                      const PRUint8 *aSrcMask,
                      PRInt32 aSrcWidth,
                      PRInt32 aSrcHeight,
                      PRInt32 aSX,
                      PRInt32 aSY,
                      PRInt32 aSWidth,
                      PRInt32 aSHeight,
                      PRInt32 aDX,
                      PRInt32 aDY,
                      PRInt32 aDWidth,
                      PRInt32 aDHeight)
{
  const unsigned char *inBuffer = (const unsigned char *)aSrc;
  PRInt32 destBytesPerRow;
  PRInt32 size = 0;
  unsigned char *outBuffer = (unsigned char *)NULL;

  printf("scaling image from %dx%d to %dx%d\n", aSrcWidth, aSrcHeight, aDWidth, aDHeight);
  printf("source: (%d,%d) -- dest: (%d,%d)\n", aSX, aSY, aDX, aDY);

  destBytesPerRow = (aDWidth * 24) >> 5;
  if ((aDWidth * 3) & 0x1F) {
    destBytesPerRow++;
  }
  destBytesPerRow <<= 2;

  size = destBytesPerRow * aDHeight;

  printf("making buffer for dest... %d bytes\n", size);

  outBuffer = (unsigned char *)PR_Calloc(size, 1);
  if (!outBuffer)
    return PR_FALSE;

  pixops_scale_nearest(outBuffer,                               /* out buffer */
                       0, 0,                                    /* dest_x, dest_y */
                       aDWidth, aDHeight,                       /* dest_width, dest_height */
                       destBytesPerRow, 3,                      /* dest_row_stride, dest_num_chans */
                       inBuffer,                                /* in buffer */
                       aSrcWidth, aSrcHeight,                   /* src_width, src_height */
                       aBytesPerRow, 3,                         /* src_row_stride, src_num_chans */
                       (double)aSrcWidth / (double)aDWidth,     /* scale_x */
                       (double)aSrcHeight / (double)aDHeight);  /* scale_y */                       
                       

  xlib_draw_rgb_image(aDest, aGC,
                     aDX, aDY, aDWidth, aDHeight,
                     XLIB_RGB_DITHER_MAX,
                     outBuffer, destBytesPerRow);
  PR_Free(outBuffer);

  return PR_TRUE;
}
