/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "nsImageGTK.h"
#include "nsRenderingContextGTK.h"

#include "nspr.h"

#define IsFlagSet(a,b) (a & b)

#undef CHEAP_PERFORMANCE_MEASUREMENT

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

//------------------------------------------------------------

nsImageGTK::nsImageGTK()
{
  NS_INIT_REFCNT();
  mImageBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mAlphaBits = nsnull;
  mAlphaPixmap = nsnull;
}

//------------------------------------------------------------

nsImageGTK::~nsImageGTK()
{
  if(nsnull != mImageBits) {
    delete[] (PRUint8*)mImageBits;
    mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] (PRUint8*)mAlphaBits;
    mAlphaBits = nsnull;
    if (nsnull != mAlphaPixmap) {
      gdk_pixmap_unref(mAlphaPixmap);
    }
  }
}

NS_IMPL_ISUPPORTS(nsImageGTK, kIImageIID);

//------------------------------------------------------------

nsresult
 nsImageGTK::Init(PRInt32 aWidth, PRInt32 aHeight,
                  PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  g_return_val_if_fail ((aWidth != 0) || (aHeight != 0), NS_ERROR_FAILURE);

  if (nsnull != mImageBits) {
   delete[] (PRUint8*)mImageBits;
   mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] (PRUint8*)mAlphaBits;
    mAlphaBits = nsnull;
    if (nsnull != mAlphaPixmap) {
      gdk_pixmap_unref(mAlphaPixmap);
      mAlphaPixmap = nsnull;
    }
  }

  if (24 == aDepth) {
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;

  // create the memory for the image
  ComputMetrics();

  mImageBits = (PRUint8*) new PRUint8[mSizeImage];

  switch(aMaskRequirements)
  {
    case nsMaskRequirements_kNoMask:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      break;

    case nsMaskRequirements_kNeeds1Bit:
      mAlphaRowBytes = (aWidth + 7) / 8;
      mAlphaDepth = 1;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

      mAlphaBits = new unsigned char[mAlphaRowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;

    case nsMaskRequirements_kNeeds8Bit:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      g_print("TODO: want an 8bit mask for an image..\n");
      break;
  }

  return NS_OK;
}

//------------------------------------------------------------

void nsImageGTK::ComputMetrics()
{
  mRowBytes = CalcBytesSpan(mWidth);
  mSizeImage = mRowBytes * mHeight;
}

PRInt32
nsImageGTK::GetBytesPix()
{
  NS_NOTYETIMPLEMENTED("somebody called this thang");
  return 0;/* XXX */
}

PRInt32
nsImageGTK::GetHeight()
{
  return mHeight;
}

PRInt32
nsImageGTK::GetWidth()
{
  return mWidth;
}

PRUint8*
nsImageGTK::GetBits()
{
  return mImageBits;
}

void*
nsImageGTK::GetBitInfo()
{
  return nsnull;
}

PRInt32
nsImageGTK::GetLineStride()
{
  return mRowBytes;
}

nsColorMap*
nsImageGTK::GetColorMap()
{
  return nsnull;
}

PRBool
nsImageGTK::IsOptimized()
{
  return PR_TRUE;
}

PRUint8*
nsImageGTK::GetAlphaBits()
{
  return mAlphaBits;
}

PRInt32
nsImageGTK::GetAlphaWidth()
{
  return mAlphaWidth;
}

PRInt32
nsImageGTK::GetAlphaHeight()
{
  return mAlphaHeight;
}

PRInt32
nsImageGTK::GetAlphaLineStride()
{
  return mAlphaRowBytes;
}

nsIImage*
nsImageGTK::DuplicateImage()
{
  return nsnull;
}

void
nsImageGTK::SetAlphaLevel(PRInt32 aAlphaLevel)
{
}

PRInt32
nsImageGTK::GetAlphaLevel()
{
  return 0;
}

void
nsImageGTK::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
}

//------------------------------------------------------------

PRInt32  nsImageGTK::CalcBytesSpan(PRUint32  aWidth)
{
  PRInt32 spanbytes;

  spanbytes = (aWidth * mDepth) >> 5;

  if (((PRUint32)aWidth * mDepth) & 0x1F)
    spanbytes++;
  spanbytes <<= 2;
  return(spanbytes);
}

//------------------------------------------------------------

// set up the palette to the passed in color array, RGB only in this array
void
nsImageGTK::ImageUpdated(nsIDeviceContext *aContext,
                         PRUint8 aFlags,
                         nsRect *aUpdateRect)
{

  if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, aFlags)){
    if (nsnull != mAlphaPixmap) {
      gdk_pixmap_unref(mAlphaPixmap);
      mAlphaPixmap = nsnull;
    }
  }
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define HAIRY_CONVERT_888
#endif

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
static PRTime gConvertTime, gStartTime, gPixmapTime, gEndTime;
#endif

#ifdef HAIRY_CONVERT_888
#include "prtime.h"

// This is really ugly. Mozilla uses BGR image data, while
// gdk_rgb uses RGB data. So before we draw an image
// we copy it to a temp buffer and swap R and B.
//
// The code here comes from gdk_rgb_convert_888_lsb.
//
static void
moz_gdk_draw_bgr_image (GdkDrawable *drawable,
                        GdkGC *gc,
                        gint x,
                        gint y,
                        gint width,
                        gint height,
                        GdkRgbDither dith,
                        guchar *rgb_buf,
                        gint rowstride)
{
  int tx, ty;

  guchar *tmp_buf;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  int r, g, b;


  bpl = (width * 3 + 3) & ~0x3;
  tmp_buf = (guchar *)g_malloc (bpl * height);

  bptr = rgb_buf;
  obuf = tmp_buf;
  for (ty = 0; ty < height; ty++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
        {
          for (tx = 0; tx < width; tx++)
            {
              r = bp2[0];
              g = bp2[1];
              b = bp2[2];
              *obptr++ = b;
              *obptr++ = g;
              *obptr++ = r;
              bp2 += 3;
            }
        }
      else
        {
          for (tx = 0; tx < width - 3; tx += 4)
            {
              guint32 r1b0g0r0;
              guint32 g2r2b1g1;
              guint32 b3g3r3b2;

              r1b0g0r0 = ((guint32 *)bp2)[0];
              g2r2b1g1 = ((guint32 *)bp2)[1];
              b3g3r3b2 = ((guint32 *)bp2)[2];
              ((guint32 *)obptr)[0] =
                (r1b0g0r0 & 0xff00) |
                ((r1b0g0r0 & 0xff0000) >> 16) |
                (((g2r2b1g1 & 0xff00) | (r1b0g0r0 & 0xff)) << 16);
              ((guint32 *)obptr)[1] =
                (g2r2b1g1 & 0xff0000ff) |
                ((r1b0g0r0 & 0xff000000) >> 16) |
                ((b3g3r3b2 & 0xff) << 16);
              ((guint32 *)obptr)[2] =
                (((g2r2b1g1 & 0xff0000) | (b3g3r3b2 & 0xff000000)) >> 16) |
                ((b3g3r3b2 & 0xff00) << 16) |
                ((b3g3r3b2 & 0xff0000));
              bp2 += 12;
              obptr += 12;
            }
          for (; tx < width; tx++)
            {
              r = bp2[0];
              g = bp2[1];
              b = bp2[2];
              *obptr++ = b;
              *obptr++ = g;
              *obptr++ = r;
              bp2 += 3;
            }
        }
      bptr += rowstride;
      obuf += bpl;
    }

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
  gConvertTime = PR_Now();
#endif

  gdk_draw_rgb_image (drawable, gc, x, y, width, height,
                      dith, tmp_buf, bpl);

  g_free (tmp_buf);
}

#else

static void
moz_gdk_draw_bgr_image (GdkDrawable *drawable,
                    GdkGC *gc,
                    gint x,
                    gint y,
                    gint width,
                    gint height,
                    GdkRgbDither dith,
                    guchar *rgb_buf,
                    gint rowstride)
{ 
  int tx, ty;
  guchar *tmp_buf;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  int r, g, b;
  
  bpl = (width * 3 + 3) & ~0x3;
  tmp_buf = (guchar *)g_malloc (bpl * height);

  bptr = rgb_buf;
  obuf = tmp_buf;

  for (ty = 0; ty < height; ty++)
    { 
      bp2 = bptr; 
      for (tx = 0; tx < width; tx++)
        { 
          r = bp2[0];
          g = bp2[1];
          b = bp2[2];
          obuf[tx * 3] = b;
          obuf[tx * 3 + 1] = g;
          obuf[tx * 3 + 2] = r;
          bp2 += 3;
        }
      bptr += rowstride;
      obuf += bpl;
    }

  gdk_draw_rgb_image (drawable, gc, x, y, width, height,
                      dith, tmp_buf, bpl);

  g_free (tmp_buf);
}

#endif


//------------------------------------------------------------

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP
nsImageGTK::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  g_return_val_if_fail ((aSurface != nsnull), NS_ERROR_FAILURE);

  nsDrawingSurfaceGTK *drawing = (nsDrawingSurfaceGTK*)aSurface;

  moz_gdk_draw_bgr_image (drawing->GetDrawable(),
                          drawing->GetGC(),
                          aDX, aDY, aDWidth, aDHeight,
                          GDK_RGB_DITHER_MAX,
                          mImageBits + mRowBytes * aSY + 3 * aDX,
                          mRowBytes);

  return NS_OK;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP
nsImageGTK::Draw(nsIRenderingContext &aContext,
                 nsDrawingSurface aSurface,
                 PRInt32 aX, PRInt32 aY,
                 PRInt32 aWidth, PRInt32 aHeight)
{
  g_return_val_if_fail ((aSurface != nsnull), NS_ERROR_FAILURE);

  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  if ((aWidth != mWidth) || (aHeight != mHeight)) {
    aWidth = mWidth;
    aHeight = mHeight;
  }

  nsDrawingSurfaceGTK* drawing = (nsDrawingSurfaceGTK*) aSurface;

  XImage *x_image = nsnull;
  Pixmap pixmap = 0;
  Display *dpy = nsnull;
  Visual *visual = nsnull;
  GC      gc;
  XGCValues gcv;

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
  gStartTime = gPixmapTime = PR_Now();
#endif

  // Create gc clip-mask on demand
  if ((mAlphaBits != nsnull) && (nsnull == mAlphaPixmap))
  {
    mAlphaPixmap = gdk_pixmap_new(nsnull, aWidth, aHeight, 1);

    /* get the X primitives */
    dpy = GDK_WINDOW_XDISPLAY(mAlphaPixmap);

    /* this is the depth of the pixmap that we are going to draw to.
       It's always a bitmap.  We're doing alpha here folks. */
    visual = GDK_VISUAL_XVISUAL(gdk_rgb_get_visual());

    // Make an image out of the alpha-bits created by the image library
    x_image = XCreateImage(dpy, visual,
                           1, /* visual depth...1 for bitmaps */
                           XYPixmap,
                           0, /* x offset, XXX fix this */
                           (char *)mAlphaBits,  /* cast away our sign. */
                           aWidth,
                           aHeight,
                           32,/* bitmap pad */
                           mAlphaRowBytes); /* bytes per line */

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
    pixmap = GDK_WINDOW_XWINDOW(mAlphaPixmap);
    memset(&gcv, 0, sizeof(XGCValues));
    gcv.function = GXcopy;
    gc = XCreateGC(dpy, pixmap, GCFunction, &gcv);
    XPutImage(dpy, pixmap, gc, x_image, 0, 0, 0, 0,
              aWidth, aHeight);
    XFreeGC(dpy, gc);

    // Now we are done with the temporary image
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
    gPixmapTime = PR_Now();
#endif
  }

  if (nsnull != mAlphaPixmap)
  {
    // Setup gc to use the given alpha-pixmap for clipping
    gdk_gc_set_clip_mask(drawing->GetGC(), mAlphaPixmap);
    gdk_gc_set_clip_origin(drawing->GetGC(), aX, aY);
  }

  moz_gdk_draw_bgr_image (drawing->GetDrawable(),
                          drawing->GetGC(),
                          aX, aY, aWidth, aHeight,
                          GDK_RGB_DITHER_MAX,
                          mImageBits, mRowBytes);

  if (mAlphaPixmap != nsnull)
  {
    // Revert gc to its old clip-mask and origin
    gdk_gc_set_clip_origin(drawing->GetGC(), 0, 0);
    gdk_gc_set_clip_mask(drawing->GetGC(), nsnull);
  }

#ifdef CHEAP_PERFORMANCE_MEASUREMENT
  gEndTime = PR_Now();
  printf("nsImageGTK: for %d,%d image, total=%lld pixmap=%lld, cvt=%lld\n",
         aWidth, aHeight,
         gEndTime - gStartTime,
         gPixmapTime - gStartTime,
         gConvertTime - gPixmapTime);
#endif

  return NS_OK;
}


//------------------------------------------------------------

nsresult nsImageGTK::Optimize(nsIDeviceContext* aContext)
{
  return NS_OK;
}
