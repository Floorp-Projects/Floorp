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

#include <gdk-pixbuf/gdk-pixbuf.h>

NS_IMPL_ISUPPORTS1(nsImageFrame, nsIImageFrame)

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
  if (mFormat >= nsIGFXFormat::RGB_A1 && mFormat <= nsIGFXFormat::BGR_A8)
    mAlphaData = new ImageData;

  switch (aFormat) {
  case nsIGFXFormat::BGR:
  case nsIGFXFormat::RGB:
    mImageData.depth = 24;
    break;
  case nsIGFXFormat::BGRA:
  case nsIGFXFormat::RGBA:
    mImageData.depth = 32;
    break;

  case nsIGFXFormat::BGR_A1:
  case nsIGFXFormat::RGB_A1:
    mImageData.depth = 24;
    mAlphaData->depth = 1;
    mAlphaData->bytesPerRow = (((mRect.width + 7) / 8) + 3) & ~0x3;
    break;
  case nsIGFXFormat::BGR_A8:
  case nsIGFXFormat::RGB_A8:
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

  return NS_OK;
}



nsresult nsImageFrame::DrawImage(GdkDrawable *dest, const GdkGC *gc, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
#if 0
  GdkPixmap *image = gdk_pixmap_new(mSurface->GetDrawable(), width, height, gdk_rgb_get_visual()->depth);
#endif

  gdk_draw_rgb_image(dest, NS_CONST_CAST(GdkGC *, gc),
                     (aDestPoint->x + aSrcRect->x), (aDestPoint->y + aSrcRect->y),
                     mRect.width, aSrcRect->height,
                     GDK_RGB_DITHER_MAX,
                     mImageData.data + (aSrcRect->y * mImageData.bytesPerRow),
                     mImageData.bytesPerRow);

#if 0
  printf(" (%f, %f), (%i, %i), %i, %i\n}\n", pt.x, pt.y, x, y, width, height);

  gdk_window_copy_area(GDK_ROOT_PARENT(), mGC, 0, 0,
                       image, 0, 0, width, height);

  gdk_window_copy_area(mSurface->GetDrawable(), mGC, pt.x + x, pt.y + y,
                       image, sr.x, 0, sr.width, height);

  gdk_pixmap_unref(image);
#endif

  return NS_OK;
}

nsresult nsImageFrame::DrawScaledImage(GdkDrawable *drawable, const GdkGC *gc, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  nsTransform2D trans;
  trans.SetToScale((float(mRect.width) / float(aDestRect->width)), (float(mRect.height) / float(aDestRect->height)));

  nsRect source(*aSrcRect);
  nsRect dest(*aDestRect);

  trans.TransformCoord(&source.x, &source.y, &source.width, &source.height);
//  trans.TransformCoord(&dest.x, &dest.y, &dest.width, &dest.height);



  GdkPixbuf *pb =
    gdk_pixbuf_new_from_data(mImageData.data + (source.y * mImageData.bytesPerRow),
                             GDK_COLORSPACE_RGB,
                             PR_FALSE,
                             8,
                             mRect.width, source.height,
                             mImageData.bytesPerRow,
                             NULL, NULL);


  GdkPixbuf *npb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, PR_FALSE, 8, dest.width, dest.height);

  gdk_pixbuf_scale(pb, npb,
                   0, 0,
                   dest.width, dest.height,
                   0, 0,
                   double(double(aDestRect->width) / double(mRect.width)),
                   double(double(aDestRect->height) / double(mRect.height)),
                   GDK_INTERP_BILINEAR);
                   

  gdk_pixbuf_unref(pb);

  gdk_pixbuf_render_to_drawable(npb,
                                drawable, gc,
                                0, 0,
                                (aDestRect->x + aSrcRect->x),
                                (aDestRect->y + aSrcRect->y),
                                dest.width, dest.height,
                                GDK_RGB_DITHER_MAX,
                                0, 0);


  gdk_pixbuf_unref(npb);

#if 0

  gdk_draw_rgb_image(drawable, NS_CONST_CAST(GdkGC *, gc),
                     (aDestRect->x + aSrcRect->x), (aDestRect->y + aSrcRect->y),
                     mRect.width, source.height,
                     GDK_RGB_DITHER_MAX,
                     mImageData.data + (source.y * mImageData.bytesPerRow),
                     mImageData.bytesPerRow);
#endif

#if 0
  printf(" (%f, %f), (%i, %i), %i, %i\n}\n", pt.x, pt.y, x, y, width, height);

  gdk_window_copy_area(GDK_ROOT_PARENT(), mGC, 0, 0,
                       image, 0, 0, width, height);

  gdk_window_copy_area(mSurface->GetDrawable(), mGC, pt.x + x, pt.y + y,
                       image, sr.x, 0, sr.width, height);

  gdk_pixmap_unref(image);
#endif

  return NS_OK;
}
