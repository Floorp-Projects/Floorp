/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Peter Hartshorn <peter@igelaus.com.au>
 */

#include "nsImageXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "xlibrgb.h"
#include "prlog.h"

#define IsFlagSet(a,b) ((a) & (b))

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

static PRLogModuleInfo *ImageXlibLM = PR_NewLogModule("ImageXlib");

nsImageXlib::nsImageXlib()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG, ("nsImageXlib::nsImageXlib()\n"));
  NS_INIT_REFCNT();
  mImageBits = nsnull;
  mAlphaBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mRowBytes = 0;
  mSizeImage = 0;
  mNumBytesPixel = 0;
  mImagePixmap = nsnull;
  mAlphaPixmap = nsnull;
  mAlphaDepth = 0;
  mAlphaRowBytes = 0;
  mAlphaWidth = 0;
  mAlphaHeight = 0;
  mLocation.x = 0;
  mLocation.y = 0;
  mDisplay = nsnull;
  mConvertedBits = nsnull;
}

nsImageXlib::~nsImageXlib()
{
  PR_LOG(ImageXlibLM, PR_LOG_DEBUG,("nsImageXlib::nsImageXlib()\n"));
  if (nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }
  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;

    if (mAlphaPixmap != nsnull) 
    {
      // The display cant be null.  It gets fetched from the drawing 
      // surface used to create the pixmap.  It gets assigned once
      // in Draw()
      NS_ASSERTION(nsnull != mDisplay,"display is null.");

#ifdef XLIB_PIXMAP_DEBUG
      printf("XFreePixmap(display = %p)\n",mDisplay);
#endif

      XFreePixmap(mDisplay, mAlphaPixmap);

    }
  }

  if (mImagePixmap != 0) 
  {
    NS_ASSERTION(nsnull != mDisplay,"display is null.");

#ifdef XLIB_PIXMAP_DEBUG
    printf("XFreePixmap(display = %p)\n",mDisplay);
#endif

    XFreePixmap(mDisplay, mImagePixmap);
  }
}

NS_IMPL_ISUPPORTS(nsImageXlib, kIImageIID);

nsresult nsImageXlib::Init(PRInt32 aWidth, PRInt32 aHeight,
                           PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  if (nsnull != mImageBits) {
    delete[] mImageBits;
    mImageBits = nsnull;
  }
  if (nsnull != mAlphaBits) {
    delete[] mAlphaBits;
    mAlphaBits = nsnull;;
  }
  if (nsnull != mAlphaPixmap) {
    XFreePixmap(mDisplay, mAlphaPixmap);
    mAlphaPixmap = nsnull;
  }

  SetDecodedRect(0,0,0,0);

  if (nsnull != mImagePixmap) {
    XFreePixmap(mDisplay, mImagePixmap);
    mImagePixmap = nsnull;
  }

  if (24 == aDepth)
  {
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;
  mIsTopToBottom = PR_TRUE;

  // Create the memory for the image
  ComputeMetrics();

  mImageBits = (PRUint8*)new PRUint8[mSizeImage];

  switch(aMaskRequirements)
  {
    case nsMaskRequirements_kNoMask:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      break;

    case nsMaskRequirements_kNeeds1Bit:
      mAlphaRowBytes = (aWidth  + 7) / 8;
      mAlphaDepth = 1;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

      mAlphaBits = new unsigned char[mAlphaRowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;

    case nsMaskRequirements_kNeeds8Bit:
      mAlphaRowBytes = aWidth;
      mAlphaDepth = 8;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
      mAlphaBits = new unsigned char[mAlphaRowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;
  }
  return NS_OK;
}

//---------------------------------------------------------------------

PRInt32 nsImageXlib::GetHeight()
{
  return mHeight;
}

PRInt32 nsImageXlib::GetWidth()
{
  return mWidth;
}

PRUint8 *nsImageXlib::GetBits()
{
  return mImageBits;
}

void *nsImageXlib::GetBitInfo()
{
  return nsnull;
}

PRInt32 nsImageXlib::GetLineStride()
{
  return mRowBytes;
}

nsColorMap *nsImageXlib::GetColorMap()
{
  return nsnull;
}

PRBool nsImageXlib::IsOptimized()
{
  return PR_TRUE;
}

PRUint8 *nsImageXlib::GetAlphaBits()
{
  return mAlphaBits;
}

PRInt32 nsImageXlib::GetAlphaWidth()
{
  return mAlphaWidth;
}

PRInt32 nsImageXlib::GetAlphaHeight()
{
  return mAlphaHeight;
}

PRInt32 nsImageXlib::GetAlphaLineStride()
{
  return mAlphaRowBytes;
}

nsIImage *nsImageXlib::DuplicateImage()
{
  return nsnull;
}

void nsImageXlib::SetAlphaLevel(PRInt32 aAlphaLevel)
{
}

PRInt32 nsImageXlib::GetAlphaLevel()
{
  return 0;
}

void nsImageXlib::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
}

//-----------------------------------------------------------------------

// Set up the palette to the passed in color array, RGB only in this array
void nsImageXlib::ImageUpdated(nsIDeviceContext *aContext,
                               PRUint8 aFlags,
                               nsRect *aUpdateRect)
{
  if (nsImageUpdateFlags_kBitsChanged & aFlags) {
    if (mAlphaPixmap != nsnull) {
      XFreePixmap(mDisplay, mAlphaPixmap);
      mAlphaPixmap = nsnull;
    }
    if (mImagePixmap != nsnull) {
      XFreePixmap(mDisplay, mImagePixmap);
      mImagePixmap = nsnull;
    }
  }
  mFlags = aFlags;
}

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP
nsImageXlib::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                  PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  if (aSurface == nsnull)
    return NS_ERROR_FAILURE;

  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;

  if (mDisplay == nsnull)
    mDisplay = drawing->GetDisplay();

  xlib_draw_rgb_image(drawing->GetDrawable(),
                      drawing->GetGC(),
                      aDX, aDY, aDWidth, aDHeight,
                      XLIB_RGB_DITHER_MAX,
                      mImageBits + mRowBytes * aSY + 3 * aDX,
                      mRowBytes);

  return NS_OK;
}

// -----------------------------------------------------------------
// 8-bit alpha composite drawing

static unsigned
findIndex32(unsigned mask)
{
  switch (mask)
  {
    case 0xff:
      return 3;
    case 0xff00:
      return 2;
    case 0xff0000:
      return 1;
    default:
      return 0;
  }
}

static unsigned
findIndex24(unsigned mask)
{
  switch(mask)
  {
    case 0xff:
      return 2;
    case 0xff00:
      return 1;
    default:
      return 0;
  }
}

// 32-bit (888) truecolor convert/composite function
void nsImageXlib::DrawComposited32(PRBool isLSB, PRBool flipBytes,
                                   unsigned offsetX, unsigned offsetY,
                                   unsigned width, unsigned height,
                                   XImage *ximage, unsigned char *readData)
{
  Visual *visual      = xlib_rgb_get_visual();
  unsigned redIndex   = findIndex32(visual->red_mask);
  unsigned greenIndex = findIndex32(visual->green_mask);
  unsigned blueIndex  = findIndex32(visual->blue_mask);

  if (flipBytes^isLSB)
  {
    redIndex   = 3-redIndex;
    greenIndex = 3-greenIndex;
    blueIndex  = 3-blueIndex;
  }

  for (unsigned y=0; y<height; y++)
  {
    unsigned char *baseRow   = (unsigned char *)ximage->data
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = mImageBits   +(y+offsetY)*mRowBytes+3*offsetX;
    unsigned char *alphaRow  = mAlphaBits   +(y+offsetY)*mAlphaRowBytes+offsetX;

    for (unsigned i=0; i<width;
         i++, baseRow+=4, targetRow+=3, imageRow+=3, alphaRow++)
    {
      targetRow[0] = (unsigned(baseRow[redIndex])   * (255-*alphaRow) +
                      unsigned(imageRow[0]) * *alphaRow) >> 8;
      targetRow[1] = (unsigned(baseRow[greenIndex]) * (255-*alphaRow) +
                      unsigned(imageRow[1]) * *alphaRow) >> 8;
      targetRow[2] = (unsigned(baseRow[blueIndex])  * (255-*alphaRow) +
                      unsigned(imageRow[2]) * *alphaRow) >> 8;
    }
  }
}

// 24-bit (888) truecolor convert/composite function
void
nsImageXlib::DrawComposited24(PRBool isLSB, PRBool flipBytes,
                             unsigned offsetX, unsigned offsetY,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData)
{
  Visual *visual      = xlib_rgb_get_visual();
  unsigned redIndex   = findIndex24(visual->red_mask);
  unsigned greenIndex = findIndex24(visual->green_mask);
  unsigned blueIndex  = findIndex24(visual->blue_mask);

  if (flipBytes^isLSB) {
    redIndex   = 2-redIndex;
    greenIndex = 2-greenIndex;
    blueIndex  = 2-blueIndex;
  }

  for (unsigned y=0; y<height; y++) {
    unsigned char *baseRow   = (unsigned char *)ximage->data
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = mImageBits   +(y+offsetY)*mRowBytes+3*offsetX;
    unsigned char *alphaRow  = mAlphaBits   +(y+offsetY)*mAlphaRowBytes+offsetX;

    for (unsigned i=0; i<width;
         i++, baseRow+=3, targetRow+=3, imageRow+=3, alphaRow++) {
      targetRow[0] =
        (unsigned(baseRow[redIndex])   * (255-*alphaRow) +
         unsigned(imageRow[0]) * *alphaRow) >> 8;
      targetRow[1] =
        (unsigned(baseRow[greenIndex]) * (255-*alphaRow) +
         unsigned(imageRow[1]) * *alphaRow) >> 8;
      targetRow[2] =
        (unsigned(baseRow[blueIndex])  * (255-*alphaRow) +
         unsigned(imageRow[2]) * *alphaRow) >> 8;
    }
  }
}

unsigned nsImageXlib::scaled6[1<<6] = {
  3,   7,  11,  15,  19,  23,  27,  31,  35,  39,  43,  47,  51,  55,  59,  63,
 67,  71,  75,  79,  83,  87,  91,  95,  99, 103, 107, 111, 115, 119, 123, 127,
131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179, 183, 187, 191,
195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
};

unsigned nsImageXlib::scaled5[1<<5] = {
  7,  15,  23,  31,  39,  47,  55,  63,  71,  79,  87,  95, 103, 111, 119, 127,
135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239, 247, 255
};

// 16-bit ([56][56][56]) truecolor convert/composite function
void
nsImageXlib::DrawComposited16(PRBool isLSB, PRBool flipBytes,
                             unsigned offsetX, unsigned offsetY,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData)
{
  Visual *visual       = xlib_rgb_get_visual();

  unsigned *redScale   = (xlib_get_prec_from_mask(visual->red_mask)   == 5)
                          ? scaled5 : scaled6;
  unsigned *greenScale = (xlib_get_prec_from_mask(visual->green_mask) == 5)
                          ? scaled5 : scaled6;
  unsigned *blueScale  = (xlib_get_prec_from_mask(visual->blue_mask)  == 5)
                          ? scaled5 : scaled6;

  for (unsigned y=0; y<height; y++) {
    unsigned char *baseRow   = (unsigned char *)ximage->data
                                            +y*ximage->bytes_per_line;
    unsigned char *targetRow = readData     +3*(y*ximage->width);
    unsigned char *imageRow  = mImageBits   +(y+offsetY)*mRowBytes+3*offsetX;
    unsigned char *alphaRow  = mAlphaBits   +(y+offsetY)*mAlphaRowBytes+offsetX;
    for (unsigned i=0; i<width;
         i++, baseRow+=2, targetRow+=3, imageRow+=3, alphaRow++) {
      unsigned pix;
      if (flipBytes) {
        unsigned char tmp[2];
        tmp[0] = baseRow[1];
        tmp[1] = baseRow[0];
        pix = *((short *)tmp);
      } else
        pix = *((short *)baseRow);
      targetRow[0] =
        (redScale[(pix&visual->red_mask) >>
                  xlib_get_shift_from_mask(visual->red_mask)] *
         (255-*alphaRow) + unsigned(imageRow[0]) * *alphaRow) >> 8;
      targetRow[1] =
        (greenScale[(pix&visual->green_mask) >>
                  xlib_get_shift_from_mask(visual->green_mask)] *
         (255-*alphaRow) + unsigned(imageRow[1]) * *alphaRow) >> 8;
      targetRow[2] =
        (blueScale[(pix&visual->blue_mask) >>
                  xlib_get_shift_from_mask(visual->blue_mask)] *
         (255-*alphaRow) + unsigned(imageRow[2]) * *alphaRow) >> 8;
    }
  }
}
// Generic convert/composite function
void
nsImageXlib::DrawCompositedGeneral(PRBool isLSB, PRBool flipBytes,
                                  unsigned offsetX, unsigned offsetY,
                                  unsigned width, unsigned height,
                                  XImage *ximage, unsigned char *readData)
{
  Visual *visual     = xlib_rgb_get_visual();
  Colormap colormap = xlib_rgb_get_cmap();

  unsigned char *target = readData;

  // flip bytes
  if (flipBytes && (ximage->bits_per_pixel>=16)) {
    for (int row=0; row<ximage->height; row++) {
      unsigned char *ptr =
        (unsigned char*)ximage->data + row*ximage->bytes_per_line;
      if (ximage->bits_per_pixel==24) {  // Aurgh....
        for (int col=0;
             col<ximage->bytes_per_line;
             col+=(ximage->bits_per_pixel/8)) {
          unsigned char tmp;
          tmp = *ptr;
          *ptr = *(ptr+2);
          *(ptr+2) = tmp;
          ptr+=3;
        }
        continue;
      }

      for (int col=0;
               col<ximage->bytes_per_line;
               col+=(ximage->bits_per_pixel/8)) {
        unsigned char tmp;
        switch (ximage->bits_per_pixel) {
        case 16:
          tmp = *ptr;
          *ptr = *(ptr+1);
          *(ptr+1) = tmp;
          ptr+=2;
          break;
        case 32:
          tmp = *ptr;
          *ptr = *(ptr+3);
          *(ptr+3) = tmp;
          tmp = *(ptr+1);
          *(ptr+1) = *(ptr+2);
          *(ptr+2) = tmp;
          ptr+=4;
          break;
        }
      }
    }
  }

  unsigned redScale, greenScale, blueScale, redFill, greenFill, blueFill;
  redScale =   8-xlib_get_prec_from_mask(visual->red_mask);
  greenScale = 8-xlib_get_prec_from_mask(visual->green_mask);
  blueScale =  8-xlib_get_prec_from_mask(visual->blue_mask);
  redFill =   0xff>>xlib_get_prec_from_mask(visual->red_mask);
  greenFill = 0xff>>xlib_get_prec_from_mask(visual->green_mask);
  blueFill =  0xff>>xlib_get_prec_from_mask(visual->blue_mask);

  for (int row=0; row<ximage->height; row++) {
    unsigned char *ptr =
      (unsigned char *)ximage->data + row*ximage->bytes_per_line;
    for (int col=0; col<ximage->width; col++) {
      unsigned pix;
      switch (ximage->bits_per_pixel) {
      case 1:
        pix = (*ptr>>(col%8))&1;
        if ((col%8)==7)
          ptr++;
        break;
      case 4:
        pix = (col&1)?(*ptr>>4):(*ptr&0xf);
        if (col&1)
          ptr++;
        break;
      case 8:
        pix = *ptr++;
        break;
      case 16:
        pix = *((short *)ptr);
        ptr+=2;
        break;
      case 24:
        if (isLSB)
          pix = (*(ptr+2)<<16) | (*(ptr+1)<<8) | *ptr;
        else
          pix = (*ptr<<16) | (*(ptr+1)<<8) | *(ptr+2);
        ptr+=3;
        break;
      case 32:
        pix = *((unsigned *)ptr);
        ptr+=4;
        break;
      }

      *target++ =
        redFill|((pix&visual->red_mask) >>
                 xlib_get_shift_from_mask(visual->red_mask))<<redScale;
      *target++ =
        greenFill|((pix&visual->green_mask) >>
                 xlib_get_shift_from_mask(visual->green_mask))<<greenScale;
      *target++ =
        blueFill|((pix&visual->blue_mask) >>
                 xlib_get_shift_from_mask(visual->blue_mask))<<blueScale;
    }
  }

  // now composite
  for (unsigned y=0; y<height; y++) {
    unsigned char *targetRow = readData+3*y*width;
    unsigned char *imageRow  = mImageBits   +(y+offsetY)*mRowBytes+3*offsetX;
    unsigned char *alphaRow  = mAlphaBits   +(y+offsetY)*mAlphaRowBytes+offsetX;  
    for (unsigned i=0; i<width; i++) {
      targetRow[3*i] =   (unsigned(targetRow[3*i])*(255-alphaRow[i]) +
                           unsigned(imageRow[3*i])*alphaRow[i])>>8;
      targetRow[3*i+1] = (unsigned(targetRow[3*i+1])*(255-alphaRow[i]) +
                           unsigned(imageRow[3*i+1])*alphaRow[i])>>8;
      targetRow[3*i+2] = (unsigned(targetRow[3*i+2])*(255-alphaRow[i]) +
                           unsigned(imageRow[3*i+2])*alphaRow[i])>>8;
    }
  }
}

void
nsImageXlib::DrawComposited(nsIRenderingContext &aContext,
                            nsDrawingSurface aSurface,
                            PRInt32 aX, PRInt32 aY,
                            PRInt32 aWidth, PRInt32 aHeight)
{
  if ((aWidth != mWidth) || (aHeight != mHeight))
  {
    aWidth = mWidth;
    aHeight = mHeight;
  }

  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;
  Visual *visual = xlib_rgb_get_visual();

  Display *dpy = drawing->GetDisplay();
  Drawable drawable = drawing->GetDrawable();

  // I hate clipping too :)
  PRUint32 surfaceWidth, surfaceHeight;
  drawing->GetDimensions(&surfaceWidth, &surfaceHeight);

  int readX, readY;
  unsigned readWidth, readHeight, destX, destY;

  readX = aX; readY = aY;
  destX = 0; destY = 0;
  if ((aY>=(int)surfaceHeight) || (aX>=(int)surfaceWidth) ||
      (aY+aHeight<=0) || (aX+aWidth<=0))
  {
    // this should never happen if the layout engine is sane,
    // as it means we are trying to draw an image which is outside
    // the drawing surface.

    return;
  }

  if (aX<0) {
    readX = 0;   readWidth = aWidth+aX;    destX = -aX;
  } else {
    readX = aX;  readWidth = aWidth;       destX = 0;
  }

  if (aY<0) {
    readY = 0;   readHeight = aHeight+aY;  destY = -aY; 
  } else { 
    readY = aY;  readHeight = aHeight;     destY = 0;
  }

  if (readX+readWidth > surfaceWidth)
  readWidth = surfaceWidth-readX;                                             
  if (readY+readHeight > surfaceHeight)
    readHeight = surfaceHeight-readY;

  XImage *ximage = XGetImage(dpy, drawable,
                             readX, readY, readWidth, readHeight,
                             AllPlanes, ZPixmap);
  unsigned char *readData = new unsigned char[3*readWidth*readHeight];

  PRBool isLSB;
  unsigned test = 1;
  isLSB = (((char *)&test)[0]) ? 1 : 0;

  PRBool flipBytes =
    ( isLSB && ximage->byte_order != LSBFirst) ||
    (!isLSB && ximage->byte_order == LSBFirst);

  if ((ximage->bits_per_pixel==32) &&
      (xlib_get_prec_from_mask(visual->red_mask) == 8) &&
      (xlib_get_prec_from_mask(visual->green_mask) == 8) &&
      (xlib_get_prec_from_mask(visual->blue_mask) == 8))
    DrawComposited32(isLSB, flipBytes, destX, destY, readWidth, readHeight,
                     ximage, readData);
  else if ((ximage->bits_per_pixel==24) &&
      (xlib_get_prec_from_mask(visual->red_mask) == 8) &&
      (xlib_get_prec_from_mask(visual->green_mask) == 8) &&
      (xlib_get_prec_from_mask(visual->blue_mask) == 8))
    DrawComposited24(isLSB, flipBytes, destX, destY, readWidth, readHeight,
                     ximage, readData);
  else if ((ximage->bits_per_pixel==16) &&
      (xlib_get_prec_from_mask(visual->red_mask) == 6) &&
      (xlib_get_prec_from_mask(visual->green_mask) == 6) &&
      (xlib_get_prec_from_mask(visual->blue_mask) == 6))
    DrawComposited16(isLSB, flipBytes, destX, destY, readWidth, readHeight,
                     ximage, readData);
  else
    DrawCompositedGeneral(isLSB, flipBytes, destX, destY, readWidth, readHeight,
                          ximage, readData);

  GC imageGC;
  imageGC = XCreateGC(dpy, drawing->GetDrawable(), 0, NULL);
  xlib_draw_rgb_image(drawing->GetDrawable(),
                      imageGC,
                      readX, readY, readWidth, readHeight,
                      XLIB_RGB_DITHER_MAX,
                      readData, 3*readWidth);
  XFreeGC(dpy, imageGC);
  delete[] readData;
}

void nsImageXlib::CreateAlphaBitmap(PRInt32 aWidth, PRInt32 aHeight,
                                    nsDrawingSurface aSurface)
{
  XImage *x_image = nsnull;
  Display *dpy = nsnull;
  Visual *visual = nsnull;
  GC gc;
  XGCValues gcv;

  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;

  if (mDisplay == nsnull)
    mDisplay = drawing->GetDisplay();

  // Create gc clip-mask on demand
  if (mAlphaBits != nsnull)
  {
    if (mAlphaPixmap == nsnull)
    {
      mAlphaPixmap = XCreatePixmap(mDisplay,
                               RootWindow(mDisplay, drawing->GetScreenNumber()),
                               aWidth, aHeight, 1);
    }

    dpy = mDisplay;
    visual = drawing->GetVisual();

    x_image = XCreateImage(dpy, visual, 1, XYPixmap, 0, (char *)mAlphaBits,
                         aWidth, aHeight, 32, mAlphaRowBytes);

    x_image->bits_per_pixel=1;
    x_image->bitmap_bit_order = MSBFirst;
    x_image->byte_order = MSBFirst;

    memset(&gcv, 0, sizeof(XGCValues));
    gcv.function = GXcopy;
    gc = XCreateGC(dpy, mAlphaPixmap, GCFunction, &gcv);
    XPutImage(dpy, mAlphaPixmap,
              gc,
              x_image, 
              0,0,0,0,
              aWidth, aHeight);
    XFreeGC(dpy, gc);

    // done with the temp image
    x_image->data = 0; //Don't free the IL_Pixmap's bits
    XDestroyImage(x_image);
  }
}

void nsImageXlib::CreateOffscreenPixmap(PRInt32 aWidth, PRInt32 aHeight,
                                        nsDrawingSurface aDrawing)
{
  if (mImagePixmap == nsnull)
  {
    nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aDrawing;
    if (mDisplay == nsnull)
      mDisplay = drawing->GetDisplay();

    mImagePixmap = XCreatePixmap(mDisplay,
                               RootWindow(mDisplay, drawing->GetScreenNumber()),
                               aWidth,
                               aHeight,
                               drawing->GetDepth());
    XSetClipOrigin(mDisplay, drawing->GetGC(), 0, 0);
    XSetClipMask(mDisplay, drawing->GetGC(), None);
  }
}

void nsImageXlib::DrawImageOffscreen(PRInt32 validX, PRInt32 validY,
                                     PRInt32 validWidth, PRInt32 validHeight,
                                     nsDrawingSurface aDrawing)
{
  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aDrawing;

  xlib_draw_rgb_image(mImagePixmap,
                      drawing->GetGC(),
                      validX, validY, validWidth, validHeight,
                      XLIB_RGB_DITHER_MAX,
                      mImageBits, mRowBytes);
}

void nsImageXlib::SetupGCForAlpha(GC aGC, PRInt32 aX, PRInt32 aY)
{
  if (mAlphaPixmap)
  {
    XGCValues xvalues;
    memset(&xvalues, 0, sizeof(XGCValues));
    unsigned long xvalues_mask = 0;
    xvalues.clip_x_origin = aX;
    xvalues.clip_y_origin = aY;
    xvalues.clip_mask = mAlphaPixmap;
    xvalues_mask = GCClipXOrigin | GCClipYOrigin | GCClipMask;

    XChangeGC(mDisplay, aGC, xvalues_mask, &xvalues);
  }
}

// Draw the bitmap. This draw just has destination coordinates
NS_IMETHODIMP
nsImageXlib::Draw(nsIRenderingContext &aContext,
                  nsDrawingSurface aSurface,
                  PRInt32 aX, PRInt32 aY,
                  PRInt32 aWidth, PRInt32 aHeight)
{
  if (aSurface == nsnull)
    return NS_ERROR_FAILURE;

  if (mAlphaDepth == 8)
  {
    DrawComposited(aContext, aSurface, aX, aY, aWidth, aHeight);
    return NS_OK;
  }

  if ((aWidth != mWidth) || (aHeight != mHeight))
  {
    aWidth = mWidth;
    aHeight = mHeight;
  }

  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;

  if (mDisplay == nsnull)
    mDisplay = drawing->GetDisplay();

  PRInt32
    validX = 0,
    validY = 0,
    validWidth = aWidth,
    validHeight = aHeight;

  if ((mDecodedY2 < aHeight)) {
    validHeight = mDecodedY2 - mDecodedY1;
  }
  if ((mDecodedX2 < aWidth)) {
    validWidth = mDecodedX2 - mDecodedX1;
  }
  if ((mDecodedY1 > 0)) {
    validHeight -= mDecodedY1;
    validY = mDecodedY1;
  }
  if ((mDecodedX1 > 0)) {
    validHeight -= mDecodedX1;
    validX = mDecodedX1;
  }

  CreateAlphaBitmap(validWidth, validHeight, aSurface);

  CreateOffscreenPixmap(validWidth, validHeight, aSurface);
  DrawImageOffscreen(validX, validY, validWidth, validHeight, aSurface);

  GC copyGC;
  if (mAlphaPixmap)
  {
    copyGC = XCreateGC(mDisplay, drawing->GetDrawable(), 0, NULL);
    XCopyGC(mDisplay, drawing->GetGC(), 0xffffffff, copyGC);
    SetupGCForAlpha(copyGC, aX, aY);
  } else 
  {
    copyGC = drawing->GetGC();
  }

  XCopyArea(mDisplay, mImagePixmap, drawing->GetDrawable(),
            copyGC, validX, validY,
            validWidth, validHeight,
            aX, aY);

  if (mAlphaPixmap)
    XFreeGC(mDisplay, copyGC);
  mFlags = 0;
  return NS_OK;
}

void nsImageXlib::TilePixmap(Pixmap src, Pixmap dest, PRInt32 aSXOffset,
                             PRInt32 aSYOffset, const nsRect &destRect,
                             const nsRect &clipRect, PRBool useClip)
{
  GC gc;
  XGCValues values;
  unsigned long valuesMask;
  memset(&values, 0, sizeof(XGCValues));
  values.fill_style = FillTiled;
  values.tile = src;
  values.ts_x_origin = destRect.x - aSXOffset;
  values.ts_y_origin = destRect.y - aSYOffset;
  valuesMask = GCTile | GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle;
  gc = XCreateGC(mDisplay, src, valuesMask, &values);

  if (useClip)
  {
    // FIXME: set the clipping area here
  }

  XFillRectangle(mDisplay, dest, gc, destRect.x, destRect.y,
                 destRect.width, destRect.height);

  XFreeGC(mDisplay, gc);
}

NS_IMETHODIMP nsImageXlib::DrawTile(nsIRenderingContext &aContext,
                                    nsDrawingSurface aSurface,
                                    nsRect &aSrcRect,
                                    nsRect &aTileRect)
{
  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;

  PRInt32
    aY0 = aTileRect.y,
    aX0 = aTileRect.x,
    aY1 = aTileRect.y + aTileRect.height,
    aX1 = aTileRect.x + aTileRect.width;

  for (PRInt32 y = aY0; y < aY1; y+=aSrcRect.height)
    for (PRInt32 x = aX0; x < aX1; x+=aSrcRect.width)
      Draw(aContext, aSurface, x, y, aSrcRect.width, aSrcRect.height);
  return NS_OK;
}

NS_IMETHODIMP nsImageXlib::DrawTile(nsIRenderingContext &aContext,
                                    nsDrawingSurface aSurface,
                                    PRInt32 aSXOffset, PRInt32 aSYOffset,
                                    const nsRect &aTileRect)
{
  nsDrawingSurfaceXlib *drawing = (nsDrawingSurfaceXlib*)aSurface;

  if (mDisplay == nsnull)
    mDisplay = drawing->GetDisplay();

  PRInt32
    validX = 0,
    validY = 0,
    validWidth = mWidth,
    validHeight = mHeight;

  // limit the image rectangle to the size of the image data which
  // has been validated.
  if ((mDecodedY2 < mHeight)) {
    validHeight = mDecodedY2 - mDecodedY1;
  }
  if ((mDecodedX2 < mWidth)) {
    validWidth = mDecodedX2 - mDecodedX1;
  }
  if ((mDecodedY1 > 0)) { 
    validHeight -= mDecodedY1;
    validY = mDecodedY1;
  }
  if ((mDecodedX1 > 0)) {
    validWidth -= mDecodedX1;
    validX = mDecodedX1;
  }

  if (mAlphaDepth == 8) {
    PRInt32 aY0 = aTileRect.y - aSYOffset,
            aX0 = aTileRect.x - aSXOffset,
            aY1 = aTileRect.y + aTileRect.height,
            aX1 = aTileRect.x + aTileRect.width;

    // Set up clipping and call Draw().
    PRBool clipState;
    aContext.PushState();
    aContext.SetClipRect(aTileRect, nsClipCombine_kIntersect,
                         clipState);

    for (PRInt32 y = aY0; y < aY1; y+=validHeight)
      for (PRInt32 x = aX0; x < aX1; x+=validWidth)
        Draw(aContext,aSurface,x,y,validWidth,validHeight);

    aContext.PopState(clipState);

    return NS_OK;
  }

  CreateOffscreenPixmap(validWidth, validHeight, aSurface);
  DrawImageOffscreen(validX, validY, validWidth, validHeight, aSurface);

  if (mAlphaDepth == 1)
  {
    Pixmap tileImg;
    Pixmap tileMask;

    CreateAlphaBitmap(validWidth, validHeight, aSurface);

    nsRect tmpRect(0,0,aTileRect.width, aTileRect.height);

    tileImg = XCreatePixmap(mDisplay, mImagePixmap,
                            aTileRect.width, aTileRect.height,
                            drawing->GetDepth());
    TilePixmap(mImagePixmap, tileImg, aSXOffset, aSYOffset, tmpRect,
               tmpRect, PR_FALSE);

    // tile alpha mask
    tileMask = XCreatePixmap(mDisplay, mAlphaPixmap,
                             aTileRect.width, aTileRect.height, mAlphaDepth);
    TilePixmap(mAlphaPixmap, tileMask, aSXOffset, aSYOffset, tmpRect,
               tmpRect, PR_FALSE);

    GC fgc;
    XGCValues values;
    unsigned long valuesMask;

    memset(&values, 0, sizeof(XGCValues));
    values.clip_mask = tileMask;
    values.clip_x_origin = aTileRect.x;
    values.clip_y_origin = aTileRect.y;
    valuesMask = GCClipXOrigin | GCClipYOrigin | GCClipMask;
    fgc = XCreateGC(mDisplay, drawing->GetDrawable(), valuesMask, &values);

    XCopyArea(mDisplay, tileImg, drawing->GetDrawable(),
              fgc, 0,0,
              aTileRect.width, aTileRect.height,
              aTileRect.x, aTileRect.y);

    XFreePixmap(mDisplay, tileImg);
    XFreePixmap(mDisplay, tileMask);
    XFreeGC(mDisplay, fgc);
  } else
  {
    nsRect clipRect;
    PRBool isValid;

    aContext.GetClipRect(clipRect, isValid);

    TilePixmap(mImagePixmap, drawing->GetDrawable(), aSXOffset, aSYOffset,
               aTileRect, clipRect, PR_FALSE);
  }

  mFlags = 0;
  return NS_OK;
}


//----------------------------------------------------------------------
nsresult nsImageXlib::Optimize(nsIDeviceContext *aContext)
{
  return NS_OK;
}

//----------------------------------------------------------------------
// Lock the image pixels.
NS_IMETHODIMP
nsImageXlib::LockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//---------------------------------------------------------------------
// unlock the image pixels. Implement this if you need it.
NS_IMETHODIMP
nsImageXlib::UnlockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//---------------------------------------------------------------------
// Set the decoded dimens of the image
NS_IMETHODIMP
nsImageXlib::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2)
{
  mDecodedX1 = x1;
  mDecodedY1 = y1;
  mDecodedX2 = x2;
  mDecodedY2 = y2;
  return NS_OK;
}

