/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImageFrame.h"

#include "nsTransform2D.h"

#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "drawers.h"

NS_IMPL_ISUPPORTS1(nsImageFrame, gfxIImageFrame)

nsImageFrame::nsImageFrame() :
  mAlphaData(nsnull),
  mInitalized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImageFrame::~nsImageFrame()
{
  /* destructor code */
  delete mAlphaData;
}

/* void init (in nscoord aX, in nscoord aY, in nscoord aWidth, in nscoord aHeight, in gfx_format aFormat); */
NS_IMETHODIMP nsImageFrame::Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, gfx_format aFormat)
{
  if (aWidth <= 0 || aHeight <= 0) {
    printf("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  if (mInitalized)
    return NS_ERROR_FAILURE;

  mInitalized = PR_TRUE;

  mRect.SetRect(aX, aY, aWidth, aHeight);
  mFormat = aFormat;

  // XXX this makes an assumption about what values these have and what is between them.. i'm being bad.
  if (mFormat >= gfxIFormats::RGB_A1 && mFormat <= gfxIFormats::BGR_A8)
    mAlphaData = new ImageData;

  switch (aFormat) {
  case gfxIFormats::BGR:
  case gfxIFormats::RGB:
    mImageData.depth = 24;
    break;
  case gfxIFormats::BGRA:
  case gfxIFormats::RGBA:
    mImageData.depth = 32;
    break;

  case gfxIFormats::BGR_A1:
  case gfxIFormats::RGB_A1:
    mImageData.depth = 24;
    mAlphaData->depth = 1;
    mAlphaData->bytesPerRow = (((mRect.width + 7) / 8) + 3) & ~0x3;
    break;
  case gfxIFormats::BGR_A8:
  case gfxIFormats::RGB_A8:
    mImageData.depth = 24;
    mAlphaData->depth = 8;
    mAlphaData->bytesPerRow = (mRect.width + 3) & ~0x3;
    break;

  default:
    printf("unsupposed gfx_format\n");
    break;
  }


  mImageData.bytesPerRow = (mRect.width * mImageData.depth) >> 5;

  if ((mRect.width * mImageData.depth) & 0x1F)
    mImageData.bytesPerRow++;
  mImageData.bytesPerRow <<= 2;


  mImageData.length = mImageData.bytesPerRow * mRect.height;
  mImageData.data = new PRUint8[mImageData.length];

  if (mAlphaData) {
    mAlphaData->length = mAlphaData->bytesPerRow * mRect.height;
    mAlphaData->data = new PRUint8[mAlphaData->length];
  }

  return NS_OK;
}

/* readonly attribute nscoord x; */
NS_IMETHODIMP nsImageFrame::GetX(nscoord *aX)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aX = mRect.x;
  return NS_OK;
}

/* readonly attribute nscoord y; */
NS_IMETHODIMP nsImageFrame::GetY(nscoord *aY)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aY = mRect.y;
  return NS_OK;
}


/* readonly attribute nscoord width; */
NS_IMETHODIMP nsImageFrame::GetWidth(nscoord *aWidth)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aWidth = mRect.width;
  return NS_OK;
}

/* readonly attribute nscoord height; */
NS_IMETHODIMP nsImageFrame::GetHeight(nscoord *aHeight)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aHeight = mRect.height;
  return NS_OK;
}

/* readonly attribute nsRect rect; */
NS_IMETHODIMP nsImageFrame::GetRect(nsRect **aRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;

  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

//  *aRect = mRect;
  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP nsImageFrame::GetFormat(gfx_format *aFormat)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFormat = mFormat;
  return NS_OK;
}

/* attribute long timeout; */
NS_IMETHODIMP nsImageFrame::GetTimeout(PRInt32 *aTimeout)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsImageFrame::SetTimeout(PRInt32 aTimeout)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute unsigned long imageBytesPerRow; */
NS_IMETHODIMP nsImageFrame::GetImageBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mImageData.bytesPerRow;
  return NS_OK;
}

/* readonly attribute unsigned long imageDataLength; */
NS_IMETHODIMP nsImageFrame::GetImageDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mImageData.length;
  return NS_OK;
}

/* void getImageData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP nsImageFrame::GetImageData(PRUint8 **aData, PRUint32 *length)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aData = mImageData.data;
  *length = mImageData.length;

  return NS_OK;
}

/* void setImageData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImageFrame::SetImageData(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  if (((PRUint32)offset + length) > mImageData.length)
    return NS_ERROR_FAILURE;

  memcpy(mImageData.data + offset, data, length);

  mImageData.dataChanged = PR_TRUE;

  return NS_OK;
}

/* readonly attribute unsigned long alphaBytesPerRow; */
NS_IMETHODIMP nsImageFrame::GetAlphaBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mAlphaData->bytesPerRow;
  return NS_OK;
}

/* readonly attribute unsigned long alphaDataLength; */
NS_IMETHODIMP nsImageFrame::GetAlphaDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mAlphaData->length;
  return NS_OK;
}

/* void getAlphaData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP nsImageFrame::GetAlphaData(PRUint8 **aBits, PRUint32 *length)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  *aBits = mAlphaData->data;
  *length = mAlphaData->length;

  return NS_OK;
}

/* void setAlphaData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImageFrame::SetAlphaData(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  if (((PRUint32)offset + length) > mAlphaData->length)
    return NS_ERROR_FAILURE;

  memcpy(mAlphaData->data + offset, data, length);

  mAlphaData->dataChanged = PR_TRUE;

  return NS_OK;
}


GdkBitmap *
nsImageFrame::GetAlphaBitmap()
{
  if (!mAlphaData)
    return nsnull;

  if (!mAlphaData->pixmap)
    mAlphaData->pixmap = gdk_pixmap_new(nsnull, mRect.width, mRect.height, 1);

  if (!mAlphaData->dataChanged)
    return gdk_pixmap_ref(mAlphaData->pixmap);

  XImage *x_image = nsnull;
  Pixmap xpixmap = 0;
  Display *dpy = nsnull;
  Visual *visual = nsnull;

  /* get the X primitives */
  dpy = GDK_WINDOW_XDISPLAY(mAlphaData->pixmap);

  /* this is the depth of the pixmap that we are going to draw to.
     It's always a bitmap.  We're doing alpha here folks. */
  visual = GDK_VISUAL_XVISUAL(gdk_rgb_get_visual());

  // Make an image out of the alpha-bits created by the image library
  x_image = XCreateImage(dpy, visual,
                         1, /* visual depth...1 for bitmaps */
                         XYPixmap,
                         0, /* x offset, XXX fix this */
                         (char *)mAlphaData->data,  /* cast away our sign. */
                         mRect.width,
                         mRect.height,
                         32,/* bitmap pad */
                         mAlphaData->bytesPerRow); /* bytes per line */

  x_image->bits_per_pixel=1;

  /* Image library always places pixels left-to-right MSB to LSB */
  x_image->bitmap_bit_order = MSBFirst;

  /* This definition doesn't depend on client byte ordering
     because the image library ensures that the bytes in
     bitmask data are arranged left to right on the screen,
     low to high address in memory. */
  x_image->byte_order = MSBFirst;
#if defined(IS_LITTLE_ENDIAN)
  // no, it's still MSB XXX check on this!!
  //      x_image->byte_order = LSBFirst;
#elif defined (IS_BIG_ENDIAN)
  x_image->byte_order = MSBFirst;
#else
#error ERROR! Endianness is unknown;
#endif

  // Write into the pixemap that is underneath gdk's mAlphaPixmap
  // the image we just created.
  xpixmap = GDK_WINDOW_XWINDOW(mAlphaData->pixmap);

  GdkGC *gc1bit = gdk_gc_new(mAlphaData->pixmap);

  XPutImage(dpy, xpixmap, GDK_GC_XGC(gc1bit), x_image, 0, 0, 0, 0,
            mRect.width, mRect.height);

  gdk_gc_unref(gc1bit);

  // Now we are done with the temporary image
  x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
  XDestroyImage(x_image);

  mAlphaData->dataChanged = PR_FALSE;

  return gdk_pixmap_ref(mAlphaData->pixmap);
}


//#define USE_XIE 1
#define USE_PIXBUF 1

nsresult nsImageFrame::DrawImage(GdkDrawable *aDest, const GdkGC *aGC, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
#ifdef USE_PIXBUF

  if (!mImageData.pixbuf) {
    PRBool hasAlpha = (mFormat == gfxIFormats::RGBA);
    mImageData.pixbuf = gdk_pixbuf_new_from_data(mImageData.data,
                                                 GDK_COLORSPACE_RGB,
                                                 hasAlpha,
                                                 8,
                                                 mRect.width, mRect.height,
                                                 mImageData.bytesPerRow,
                                                 NULL, NULL);
  }


  if (mFormat == gfxIFormats::RGBA) {
    GdkPixbuf *destPb =
      gdk_pixbuf_get_from_drawable(nsnull,
                                   aDest, gdk_rgb_get_cmap(),
                                   aDestPoint->x + aSrcRect->x,
                                   aDestPoint->y + aSrcRect->y,
                                   0, 0,
                                   aSrcRect->width, aSrcRect->height);

    if (!destPb) {
      printf("get failed!\n");
      return NS_ERROR_FAILURE;
    }

    GdkPixbuf *tmpPb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, PR_TRUE, 8,
                                      aSrcRect->width, aSrcRect->height);

    gdk_pixbuf_copy_area(mImageData.pixbuf,
                         aSrcRect->x, aSrcRect->y,
                         aSrcRect->width, aSrcRect->height,
                         tmpPb,
                         0, 0);

    gdk_pixbuf_composite(tmpPb, destPb,
                         0, 0,
                         aSrcRect->width, aSrcRect->height,
                         0.0, 0.0,
                         1.0, 1.0, GDK_INTERP_NEAREST, 255);

    gdk_pixbuf_render_to_drawable(destPb,
                                  aDest, NS_CONST_CAST(GdkGC*, aGC),
                                  0, 0,
                                  (aDestPoint->x + aSrcRect->x),
                                  (aDestPoint->y + aSrcRect->y),
                                  aSrcRect->width, aSrcRect->height,
                                  GDK_RGB_DITHER_MAX, 0, 0);

    gdk_pixbuf_unref(tmpPb);
    gdk_pixbuf_unref(destPb);

  } else if (mFormat == gfxIFormats::RGB) {

    gdk_pixbuf_render_to_drawable(mImageData.pixbuf,
                                  aDest, NS_CONST_CAST(GdkGC*, aGC),
                                  aSrcRect->x, aSrcRect->y,
                                  (aDestPoint->x + aSrcRect->x),
                                  (aDestPoint->y + aSrcRect->y),
                                  aSrcRect->width, aSrcRect->height,
                                  GDK_RGB_DITHER_MAX, 0, 0);

  } else if (mFormat == gfxIFormats::RGB_A1) {

    GdkBitmap *alphaMask = GetAlphaBitmap();
    GdkGC *gc = nsnull;

    if (alphaMask) {
      gc = gdk_gc_new(aDest);

      gdk_gc_copy(gc, NS_CONST_CAST(GdkGC*, aGC));
      gdk_gc_set_clip_mask(gc, alphaMask);
      gdk_gc_set_clip_origin(gc,
                             aDestPoint->x,
                             aDestPoint->y);
    }

    if (!gc) {
      gc = NS_CONST_CAST(GdkGC *, aGC);
      gdk_gc_ref(gc);
    }

    gdk_pixbuf_render_to_drawable(mImageData.pixbuf,
                                  aDest, gc,
                                  aSrcRect->x, aSrcRect->y,
                                  (aDestPoint->x + aSrcRect->x),
                                  (aDestPoint->y + aSrcRect->y),
                                  aSrcRect->width, aSrcRect->height,
                                  GDK_RGB_DITHER_MAX, 0, 0);

    if (alphaMask) gdk_pixmap_unref(alphaMask);
    gdk_gc_unref(gc);
  }

#else

  GdkBitmap *alphaMask = GetAlphaBitmap();
  GdkGC *gc = nsnull;

  if (alphaMask) {
    gc = gdk_gc_new(aDest);

    gdk_gc_copy(gc, NS_CONST_CAST(GdkGC*, aGC));
    gdk_gc_set_clip_mask(gc, alphaMask);
    gdk_gc_set_clip_origin(gc,
                           aDestPoint->x + aSrcRect->x,
                           aDestPoint->y + aSrcRect->y);
  }

  if (!gc) {
    gc = NS_CONST_CAST(GdkGC *, aGC);
    gdk_gc_ref(gc);
  }

  gdk_draw_rgb_image(aDest, gc,
                     aDestPoint->x, (aDestPoint->y + aSrcRect->y),
                     mRect.width, aSrcRect->height,
                     GDK_RGB_DITHER_MAX,
                     mImageData.data + (aSrcRect->y * mImageData.bytesPerRow),
                     mImageData.bytesPerRow);

  if (alphaMask) gdk_pixmap_unref(alphaMask);

  gdk_gc_unref(gc);

#endif

  return NS_OK;
}

nsresult nsImageFrame::DrawScaledImage(GdkDrawable *aDrawable, const GdkGC *aGC, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  nsTransform2D trans;
  trans.SetToScale((float(mRect.width) / float(aDestRect->width)), (float(mRect.height) / float(aDestRect->height)));

  nsRect source(*aSrcRect);
  nsRect dest(*aDestRect);

  trans.TransformCoord(&source.x, &source.y, &source.width, &source.height);



#ifdef USE_XIE
  if (!mImageData.pixmap)
    mImageData.pixmap = gdk_pixmap_new(aDrawable, mRect.width, mRect.height, gdk_rgb_get_visual()->depth);

  Display *display = GDK_WINDOW_XDISPLAY(aDrawable);

  

  GdkGC *gdkgc = gdk_gc_new(aDrawable);
  gdk_draw_rgb_image(mImageData.pixmap, gdkgc,
                     0, source.y,
                     mRect.width, source.height,
                     GDK_RGB_DITHER_MAX,
                     mImageData.data,
                     mImageData.bytesPerRow);


  GdkBitmap *alphaMask = GetAlphaBitmap();

  DrawScaledImageXIE(display, aDrawable, NS_CONST_CAST(GdkGC*, aGC),
                     mImageData.pixmap, alphaMask,
                     mRect.width, mRect.height,
                     source.x, source.y, source.width, source.height,
                     dest.x, dest.y, dest.width, dest.height);


  if (alphaMask)
    gdk_pixmap_unref(alphaMask);

  gdk_gc_unref(gdkgc);

#endif


#ifdef USE_PIXBUF

  if (!mImageData.pixbuf)
    mImageData.pixbuf = gdk_pixbuf_new_from_data(mImageData.data,
                                                 GDK_COLORSPACE_RGB,
                                                 PR_FALSE,
                                                 8,
                                                 mRect.width, mRect.height,
                                                 mImageData.bytesPerRow,
                                                 NULL, NULL);

  GdkPixbuf *npb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, PR_FALSE, 8, dest.width, dest.height);

  gdk_pixbuf_scale(mImageData.pixbuf, npb,
                   0, 0,
                   dest.width, dest.height,
                   0, 0,
                   double(double(aDestRect->width) / double(mRect.width)),
                   double(double(aDestRect->height) / double(mRect.height)),
                   //                    GDK_INTERP_BILINEAR);
                   GDK_INTERP_NEAREST);


  GdkBitmap *alphaMask = GetAlphaBitmap();

  gdk_pixbuf_render_to_drawable(npb,
                                aDrawable, NS_CONST_CAST(GdkGC*, aGC),
                                aSrcRect->x, aSrcRect->y,
                                aDestRect->x, aDestRect->y,
                                aSrcRect->width, aSrcRect->height,
                                GDK_RGB_DITHER_MAX,
                                0, 0);


  gdk_pixbuf_unref(npb);
#endif

  return NS_OK;
}
