/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab:
 *
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 *
 * Contributor(s): Brian Stell <bstell@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "gfx-config.h"
#include "nsCRT.h"
#include "nspr.h"
#include "nsAntiAliasedGlyph.h"
#include "nsX11AlphaBlend.h"

#define ENABLE_X11ALPHA_BLEND_PRINTF 1

#if ENABLE_X11ALPHA_BLEND_PRINTF
static PRUint32 gX11AlphaBlendDebug;
# include <stdio.h>
#define NS_X11_ALPHA_BLEND_DEBUG   0x01

#define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
              if (gX11AlphaBlendDebug & (type)) { \
                printf x ; \
                printf(", %s %d\n", __FILE__, __LINE__); \
              } \
            PR_END_MACRO 

#else /* ENABLE_X11ALPHA_BLEND_PRINTF */

#define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
            PR_END_MACRO 

#endif /* ENABLE_X11ALPHA_BLEND_PRINTF */

#define X11ALPHA_BLEND_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_X11_ALPHA_BLEND_DEBUG)

static void dummy_BlendPixel(XImage *, int x, int y, nscolor color, int a);
#ifdef DEBUG
static void nsBlendPixel555   (XImage *, int x, int y, nscolor color, int a);
static void nsBlendPixel555_br(XImage *, int x, int y, nscolor color, int a);
static void nsBlendPixel565   (XImage *, int x, int y, nscolor color, int a);
static void nsBlendPixel565_br(XImage *, int x, int y, nscolor color, int a);
static void nsBlendPixel888_lsb(XImage *, int x, int y, nscolor color, int a);
static void nsBlendPixel888_msb(XImage *, int x, int y, nscolor color, int a);
static void nsBlendPixel0888   (XImage *, int x, int y, nscolor color, int a);
static void nsBlendPixel0888_br(XImage *, int x, int y, nscolor color, int a);
#else
# define nsBlendPixel555     dummy_BlendPixel
# define nsBlendPixel555_br  dummy_BlendPixel
# define nsBlendPixel565     dummy_BlendPixel
# define nsBlendPixel565_br  dummy_BlendPixel
# define nsBlendPixel888_lsb dummy_BlendPixel
# define nsBlendPixel888_msb dummy_BlendPixel
# define nsBlendPixel0888    dummy_BlendPixel
# define nsBlendPixel0888_br dummy_BlendPixel
#endif

static void nsBlendMonoImage555   (XImage*,nsAntiAliasedGlyph*,
                                   PRUint8*, nscolor, int, int);
static void nsBlendMonoImage555_br(XImage*,nsAntiAliasedGlyph*,
                                   PRUint8*, nscolor, int, int);
static void nsBlendMonoImage565   (XImage*,nsAntiAliasedGlyph*,
                                   PRUint8*, nscolor, int, int);
static void nsBlendMonoImage565_br(XImage*,nsAntiAliasedGlyph*,
                                   PRUint8*, nscolor, int, int);
static void nsBlendMonoImage888_lsb(XImage*,nsAntiAliasedGlyph*,
                                    PRUint8*, nscolor, int, int);
static void nsBlendMonoImage888_msb(XImage*,nsAntiAliasedGlyph*,
                                    PRUint8*, nscolor, int, int);
static void nsBlendMonoImage0888(XImage*,nsAntiAliasedGlyph*,
                                 PRUint8*, nscolor, int, int);
static void nsBlendMonoImage0888_br(XImage*,nsAntiAliasedGlyph*,
                                 PRUint8*, nscolor, int, int);
static nscolor nsPixelToNscolor555   (unsigned long aPixel);
static nscolor nsPixelToNscolor565   (unsigned long aPixel);
static nscolor nsPixelToNscolor888_lsb(unsigned long aPixel);
static nscolor nsPixelToNscolor888_msb(unsigned long aPixel);

static void dummy_BlendMonoImage(XImage *, nsAntiAliasedGlyph *, PRUint8*,
                                 nscolor, int, int);
static nscolor dummy_PixelToNSColor(unsigned long);

// sPixelToNSColor
//
// X11AlphaBlend Globals
//
PRBool         nsX11AlphaBlend::sAvailable;
PRUint16       nsX11AlphaBlend::sBitmapPad;
PRUint16       nsX11AlphaBlend::sBitsPerPixel;
blendGlyph     nsX11AlphaBlend::sBlendMonoImage;
blendPixel     nsX11AlphaBlend::sBlendPixel;
PRUint16       nsX11AlphaBlend::sBytesPerPixel;
int            nsX11AlphaBlend::sDepth;
PRBool         nsX11AlphaBlend::sInited;
pixelToNSColor nsX11AlphaBlend::sPixelToNSColor;

void
nsX11AlphaBlend::ClearGlobals()
{
  sAvailable          = PR_FALSE;
  sBitmapPad          = 0;
  sBitsPerPixel       = 0;
  sBlendMonoImage     = dummy_BlendMonoImage;
  sBlendPixel         = dummy_BlendPixel;
  sBytesPerPixel      = 1;
  sDepth              = 0;
  sInited             = PR_FALSE;
  sPixelToNSColor     = dummy_PixelToNSColor;
}

void
nsX11AlphaBlendFreeGlobals()
{
  nsX11AlphaBlend::FreeGlobals();
}

void
nsX11AlphaBlend::FreeGlobals()
{
  ClearGlobals();
}

nsresult
nsX11AlphaBlendInitGlobals(Display *aDisplay)
{
  X11ALPHA_BLEND_PRINTF(("initialize X11AlphaBlend"));

  nsresult rv = nsX11AlphaBlend::InitGlobals(aDisplay);

  return rv;
}

nsresult
nsX11AlphaBlend::InitGlobals(Display *aDisplay)
{
  NS_ASSERTION(sInited==PR_FALSE, "InitGlobals called more than once");
  // set all the globals to default values
#if ENABLE_X11ALPHA_BLEND_PRINTF
  char* debug = PR_GetEnv("NS_ALPHA_BLEND_DEBUG");
  if (debug) {
    PR_sscanf(debug, "%lX", &gX11AlphaBlendDebug);
  }
#endif

  ClearGlobals();
  if (!InitLibrary(aDisplay))
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

PRBool
nsX11AlphaBlend::InitLibrary(Display *aDisplay)
{
  if (sInited)
    return sAvailable;

  sInited = PR_TRUE;

  Visual *visual = DefaultVisual(aDisplay,DefaultScreen(aDisplay));
  if (visual->c_class != TrueColor) {
    X11ALPHA_BLEND_PRINTF(("unsuppored visual class %d", visual->c_class));
    return PR_FALSE;
  }

  //
  // Get an XImage to get the image information
  //
  Window root_win = RootWindow(aDisplay, DefaultScreen(aDisplay));
  XImage *img = XGetImage(aDisplay, root_win, 0, 0, 1, 1, 0xffffffff, ZPixmap);
  NS_ASSERTION(img, "InitGlobals: XGetImage failed");
  if (!img)
    return PR_FALSE;
  int byte_order = img->byte_order;
  sBitmapPad     = img->bitmap_pad;
  sBitsPerPixel  = img->bits_per_pixel;
  sDepth         = img->depth;
  int blue_mask  = img->blue_mask;
  int green_mask = img->green_mask;
  int red_mask   = img->red_mask;
  XDestroyImage(img);

  PRBool same_byte_order;
#ifdef IS_LITTLE_ENDIAN
  X11ALPHA_BLEND_PRINTF(("endian           = little"));
  same_byte_order = (byte_order == LSBFirst);
#elif IS_BIG_ENDIAN
  X11ALPHA_BLEND_PRINTF(("endian           = big"));
  same_byte_order = (byte_order == MSBFirst);
#else
#  error neither IS_LITTLE_ENDIAN or IS_BIG_ENDIAN is defined
#endif

  X11ALPHA_BLEND_PRINTF(("byte_order       = %s", byte_order?"MSB":"LSB"));
  X11ALPHA_BLEND_PRINTF(("same_byte_order  = %d", same_byte_order));

  X11ALPHA_BLEND_PRINTF(("sBitmapPad       = %d", sBitmapPad));
  X11ALPHA_BLEND_PRINTF(("sDepth           = %d", sDepth));
  X11ALPHA_BLEND_PRINTF(("sBitsPerPixel    = %d", sBitsPerPixel));

  if (sBitsPerPixel <= 16)
    sBytesPerPixel = 2;
  else if (sBitsPerPixel <= 32)
    sBytesPerPixel = 4;
  else {
    X11ALPHA_BLEND_PRINTF(("sBitsPerPixel %d: not supported", sBitsPerPixel));
    return PR_FALSE;
  }
  X11ALPHA_BLEND_PRINTF(("sBytesPerPixel   = %d", sBytesPerPixel));

  if (sBitsPerPixel==16)  {
    if ((red_mask==0x7C00) && (green_mask==0x3E0) && (blue_mask==0x1F)) {
      // 555
      sAvailable           = PR_TRUE;
      if (same_byte_order) {
        sPixelToNSColor = &nsPixelToNscolor555;
        sBlendPixel     = &nsBlendPixel555;
        sBlendMonoImage = &nsBlendMonoImage555;
      }
      else {
        sPixelToNSColor = &nsPixelToNscolor555;
        sBlendPixel     = &nsBlendPixel555_br;
        sBlendMonoImage = &nsBlendMonoImage555_br;
      }
    }
    else if ((red_mask==0xF800) && (green_mask==0x7E0) && (blue_mask==0x1F)) {
      // 565
      sAvailable           = PR_TRUE;
      if (same_byte_order) {
        sPixelToNSColor = &nsPixelToNscolor565;
        sBlendPixel     = &nsBlendPixel565;
        sBlendMonoImage = &nsBlendMonoImage565;
      }
      else {
        sPixelToNSColor = &nsPixelToNscolor565;
        sBlendPixel     = &nsBlendPixel565_br;
        sBlendMonoImage = &nsBlendMonoImage565_br;
      }
    }
  }
  else if (sBitsPerPixel==24)  {
    if ((red_mask==0xFF0000) && (green_mask==0xFF00) && (blue_mask==0xFF)) {
      // 888
      sAvailable           = PR_TRUE;
      if (same_byte_order) {
        sPixelToNSColor = &nsPixelToNscolor888_lsb;
        sBlendPixel     = &nsBlendPixel888_lsb;
        sBlendMonoImage = &nsBlendMonoImage888_lsb;
      }
      else {
        sPixelToNSColor = &nsPixelToNscolor888_msb;
        sBlendPixel     = &nsBlendPixel888_msb;
        sBlendMonoImage = &nsBlendMonoImage888_msb;
      }
    }
  }
  else if (sBitsPerPixel==32)  {
      // 0888
      sAvailable           = PR_TRUE;
      if (same_byte_order) {
        sPixelToNSColor = &nsPixelToNscolor888_lsb;
        sBlendPixel     = &nsBlendPixel0888;
        sBlendMonoImage = &nsBlendMonoImage0888;
      }
      else {
        sPixelToNSColor = &nsPixelToNscolor888_lsb;
        sBlendPixel     = &nsBlendPixel0888_br;
        sBlendMonoImage = &nsBlendMonoImage0888_br;
      }
  }
  else {
    sAvailable = PR_FALSE;
    NS_ASSERTION(0, "X11AlphaBlend: unsupported framebuffer depth");
    goto nsX11AlphaBlend__InitLibrary_error;
  }
  return sAvailable;

nsX11AlphaBlend__InitLibrary_error:
  // clean everything up but note that init was called
  FreeGlobals();
  sInited = PR_TRUE;
  return(sAvailable);
}

nscolor
nsX11AlphaBlend::PixelToNSColor(unsigned long aPixel)
{
  nscolor color = (*sPixelToNSColor)(aPixel);
  return color;
}

///////////////////////////////////////////////////////////////////////
//
// miscellaneous routines in alphabetic order
//
///////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void
AADrawBox(XImage *aXImage,
        PRInt32 aX1, PRInt32 aY1,
        PRInt32 aWidth, PRInt32 aHeight,
        nscolor aColor, PRUint8 aAlpha)
{
  PRInt32 i;
  blendPixel blendPixelFunc = nsX11AlphaBlend::GetBlendPixel();
  if (aWidth<=0 || aHeight<=0)
    return;
  for (i=1; i<aWidth-1; i++) {
    if (i%16 == 0) continue; // let the underlaying color show
    (*blendPixelFunc)(aXImage, i+aX1, aY1, aColor, aAlpha);
    (*blendPixelFunc)(aXImage, i+aX1, aY1+aHeight-1, aColor, aAlpha);
  }
  for (i=0; i<aHeight; i++) {
    if (i%16 == 0) continue; // let the underlaying color show
    (*blendPixelFunc)(aXImage, aX1, i+aY1, aColor, aAlpha);
    (*blendPixelFunc)(aXImage, aX1+aWidth-1, i+aY1, aColor, aAlpha);
  }
}
#endif

static void
dummy_BlendPixel(XImage *, int x, int y, nscolor color, int a)
{
}

static void
dummy_BlendMonoImage(XImage *, nsAntiAliasedGlyph *, PRUint8*, nscolor,
                     int, int)
{
}

static nscolor
dummy_PixelToNSColor(unsigned long color)
{
  return 0;
}

XImage*
nsX11AlphaBlend::GetBackground(Display *aDisplay, int aScreen,
                               Drawable aDrawable,
                               PRInt32 aX, PRInt32 aY,
                               PRUint32 aWidth,  PRUint32 aHeight)
{
  PRBool any_offscreen = PR_FALSE;
  XImage *ximage;

  //
  // get the background
  //
  // bound the request to inside the window
  PRInt32 x_skip = 0;
  if (aX < 0) {
    x_skip = -aX;
    any_offscreen = PR_TRUE;
  }
  PRInt32 y_skip = 0;
  if (aY < 0) {
    y_skip = -(aY);
    any_offscreen = PR_TRUE;
  }
  PRInt32 copy_width  = aWidth  - x_skip;
  PRInt32 copy_height = aHeight - y_skip;

  Window root;
  int win_x, win_y;
  unsigned int win_width, win_height, win_border_width, win_depth;
  if (!XGetGeometry(aDisplay, aDrawable, &root, &win_x, &win_y,
                    &win_width, &win_height, &win_border_width, &win_depth)) {
    NS_ASSERTION(0, "XGetGeometry");
  }
  if ((PRUint32)(aX+x_skip+aWidth) > win_width) {
    copy_width = MIN(copy_width, (int)win_width - (aX+x_skip));
    any_offscreen = PR_TRUE;
  }
  if ((PRUint32)(aY+y_skip+aHeight) > win_height) {
    copy_height = MIN(copy_height, (int)win_height - (aY+y_skip));
    any_offscreen = PR_TRUE;
  }

  PRUint32 root_win_width, root_win_height;
  root_win_width  = DisplayWidth(aDisplay, aScreen);
  root_win_height = DisplayHeight(aDisplay, aScreen);

  if ((PRUint32)(aX+x_skip+aWidth) > root_win_width) {
    copy_width = MIN(copy_width, (int)root_win_width - (aX+x_skip));
    any_offscreen = PR_TRUE;
  }
  if ((PRUint32)(aY+y_skip+aHeight) > root_win_height) {
    copy_height = MIN(copy_height, (int)root_win_height - (aY+y_skip));
    any_offscreen = PR_TRUE;
  }
  if ((copy_width<=0) || (copy_height<=0))
    return nsnull; // nothing visible

  // get the background image
  // if any part is off screen XGetImage will fail, so we XCreateImage 
  // the image and use XGetSubImage to get the available background pixels
  if (any_offscreen) {
    char *data = (char *)nsMemory::Alloc(aWidth * aHeight * sBytesPerPixel);
    if (!data) {
      return nsnull;
    }
    XImage *super_ximage = XCreateImage(aDisplay,
                                 DefaultVisual(aDisplay, aScreen),
                                 DefaultDepth(aDisplay, aScreen), ZPixmap,
                                 0, data, aWidth, aHeight,
                                 sBitmapPad, aWidth*sBytesPerPixel);
    if (!super_ximage) {
      NS_ASSERTION(super_ximage, "failed to create the super image");
      return nsnull;
    }
    ximage = XGetSubImage(aDisplay, aDrawable,
                          aX+x_skip, aY+y_skip, 
                          copy_width, copy_height,
                          AllPlanes, ZPixmap, super_ximage, x_skip, y_skip);
    if (!ximage) {
      NS_ASSERTION(ximage, "failed to get the sub image");
      XDestroyImage(super_ximage);
      return nsnull;
    }
     ximage = super_ximage;
  }
  else {
    ximage = XGetImage(aDisplay, aDrawable, aX, aY, aWidth, aHeight,
                       AllPlanes, ZPixmap);
  }

  NS_ASSERTION(ximage, "failed to get the image");
  return ximage;
}

//
// 24 bit color, accessed in a 32 bit number
//
static void
nsBlendMonoImage0888(XImage *ximage, nsAntiAliasedGlyph * glyph,
                     PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint32 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  NS_ASSERTION(((ximage->data-(char*)0)&3)==0,"possible alignment error");
  NS_ASSERTION((ximage->bytes_per_line&3)==0,"possible alignment error");
  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 4*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint32 *image_p = (PRUint32 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p++,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image
      PRUint32 dst_pixel = *image_p;
      PRUint32 hibits = dst_pixel & 0xFF000000;
      if (src_a == 255) {
        *image_p = hibits | (r << 16) | (g << 8) + b;
        continue;
      }
      dst_a = 255 - src_a;

      PRUint32 red   = ((r*src_a) + (((dst_pixel>>16)&0xFF) * dst_a)) >> 8;
      PRUint32 green = ((g*src_a) + (((dst_pixel>>8) &0xFF) * dst_a)) >> 8;
      PRUint32 blue  = ((b*src_a) + (( dst_pixel     &0xFF) * dst_a)) >> 8;
      *image_p = hibits | (red << 16) | (green << 8) | blue;
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

//
// 24 bit color, accessed in a 32 bit number
//
static void
nsBlendMonoImage0888_br(XImage *ximage, nsAntiAliasedGlyph * glyph,
                     PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint32 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 4*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint32 *image_p = (PRUint32 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p++,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image
      PRUint32 dst_pixel = *image_p;
      PRUint32 lowbits = dst_pixel & 0x000000FF;
      if (src_a == 255) {
        *image_p = (b << 24) | (g << 16) + (r << 8) | lowbits;
        continue;
      }
      dst_a = 255 - src_a;

      PRUint32 red   = ((r*src_a) + (((dst_pixel>> 8) &0xFF) * dst_a)) >> 8;
      PRUint32 green = ((g*src_a) + (((dst_pixel>>16) &0xFF) * dst_a)) >> 8;
      PRUint32 blue  = ((b*src_a) + (((dst_pixel>>24) &0xFF) * dst_a)) >> 8;
      *image_p = (blue << 24) | (green << 16) + (red << 8) | lowbits;
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

//
// 15 bit color, accessed in a 16 bit number
//
static void
nsBlendMonoImage555(XImage *ximage, nsAntiAliasedGlyph * glyph,
                    PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint16 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 2*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint16 *image_p = (PRUint16 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p++,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image

      if (src_a == 255) {
        *image_p = ((r&0xF8)<<7) | ((g&0xF8)<<2) | ((b) >> 3);
                   // the long version
                   //(((r>>3)&0x1F)<<10) | (((g>>3)&0x1F)<<5) | (((b>>3)&0x1F));
        continue;
      }
      dst_a = 255 - src_a;

      PRUint16 dst_pixel = *image_p;
      PRUint16 red   = ((r*src_a) + (((dst_pixel>>7)&0xF8) * dst_a)) >> 8;
      PRUint16 green = ((g*src_a) + (((dst_pixel>>2)&0xF8) * dst_a)) >> 8;
      PRUint16 blue  = ((b*src_a) + (((dst_pixel<<3)&0xF8) * dst_a)) >> 8;
      *image_p = ((red&0xF8)<<7) | ((green&0xF8)<<2) | ((blue) >> 3);
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

//
// 15 bit color, accessed in a 16 bit number, byte reversed
//
static void
nsBlendMonoImage555_br(XImage *ximage, nsAntiAliasedGlyph * glyph,
                    PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint16 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 2*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint16 *image_p = (PRUint16 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p++,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image

      if (src_a == 255) {
        *image_p =  ((r&0xF8)   >> 1) |
                    ((g&0xC0)>> 6) | ((g&0x38)<<10) |
                    ((b&0xF8)<< 5);
        continue;
      }
      dst_a = 255 - src_a;

      PRUint16 dst_pixel = *image_p;
      // unreversed: --:R7:R6:R5:R4:R3:G7:G6  G5:G4:G3:B7:B6:B5:B4:B3
      //   reversed: G5:G4:G3:B7:B6:B5:B4:B3  --:R7:R6:R5:R4:R3:G7:G6
      PRUint16 pixel_r =  (dst_pixel>>1) & 0xF8;
      PRUint16 pixel_g = ((dst_pixel<<6) & 0xC0) | ((dst_pixel>>10) & 0x38);
      PRUint16 pixel_b =  (dst_pixel>>5) & 0xF8;

      PRUint16 red   = ((r*src_a) + (pixel_r * dst_a)) >> 8;
      PRUint16 green = ((g*src_a) + (pixel_g * dst_a)) >> 8;
      PRUint16 blue  = ((b*src_a) + (pixel_b * dst_a)) >> 8;
      *image_p = ((red  &0xF8)>> 1) |
                 ((green&0xC0)>> 6) | ((green&0x38)<<10) |
                 ((blue &0xF8)<< 5);
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

//
// 16 bit color, accessed in a 16 bit number
//
static void
nsBlendMonoImage565(XImage *ximage, nsAntiAliasedGlyph * glyph,
                    PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint16 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 2*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint16 *image_p = (PRUint16 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p++,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image

      if (src_a == 255) {
        *image_p = ((r&0xF8)<<8) | ((g&0xFC)<<3) | ((b) >> 3);
        continue;
      }
      dst_a = 255 - src_a;

      PRUint32 dst_pixel = *image_p;
      PRUint16 red   = ((r*src_a) + (((dst_pixel>>8)&0xF8) * dst_a)) >> 8;
      PRUint16 green = ((g*src_a) + (((dst_pixel>>3)&0xFC) * dst_a)) >> 8;
      PRUint16 blue  = ((b*src_a) + (((dst_pixel<<3)&0xF8) * dst_a)) >> 8;
      *image_p = ((red&0xF8)<<8) | ((green&0xFC)<<3) | ((blue) >> 3);
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

//
// 16 bit color, accessed in a 16 bit number, byte reversed
//
static void
nsBlendMonoImage565_br(XImage *ximage, nsAntiAliasedGlyph * glyph,
                    PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint16 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 2*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint16 *image_p = (PRUint16 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p++,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image
      if (src_a == 255) {
        *image_p =   (r&0xF8) |
                    ((g&0xE0)>> 5) | ((g&0x1C)<<11) |
                    ((b&0xF8)<< 5);
        continue;
      }
      dst_a = 255 - src_a;

      PRUint16 dst_pixel = *image_p;
      // unreversed: R7:R6:R5:R4:R3:G7:G6:G5  G4:G3:G2:B7:B6:B5:B4:B3
      //   reversed: G4:G3:G2:B7:B6:B5:B4:B3  R7:R6:R5:R4:R3:G7:G6:G5
      PRUint16 pixel_r =  (dst_pixel)    & 0xF8;
      PRUint16 pixel_g = ((dst_pixel<<5) & 0xE0) | ((dst_pixel>>11) & 0x1C);
      PRUint16 pixel_b =  (dst_pixel>>5) & 0xF8;

      PRUint16 red   = ((r*src_a) + (pixel_r * dst_a)) >> 8;
      PRUint16 green = ((g*src_a) + (pixel_g * dst_a)) >> 8;
      PRUint16 blue  = ((b*src_a) + (pixel_b * dst_a)) >> 8;
      *image_p =  (red  &0xF8) |
                 ((green&0xE0)>> 5) | ((green&0x1C)<<11) |
                 ((blue &0xF8)<< 5);
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

//
// 24 bit color, accessed in a 3*8 bit numbers, little endian
//
static void
nsBlendMonoImage888_lsb(XImage *ximage, nsAntiAliasedGlyph * glyph,
                   PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint32 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 3*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint8 *image_p = (PRUint8 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p+=3,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image

      if (src_a == 255) {
        image_p[2] = r;
        image_p[1] = g;
        image_p[0] = b;
        continue;
      }
      dst_a = 255 - src_a;
      image_p[2] = ((r*src_a) + (image_p[2]*dst_a)) >> 8;
      image_p[1] = ((g*src_a) + (image_p[1]*dst_a)) >> 8;
      image_p[0] = ((b*src_a) + (image_p[0]*dst_a)) >> 8;
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

//
// 24 bit color, accessed in a 3*8 bit numbers, big endian
//
static void
nsBlendMonoImage888_msb(XImage *ximage, nsAntiAliasedGlyph * glyph,
                   PRUint8 *aWeightTable, nscolor color, int xOff, int yOff)
{
  PRUint32 src_a, dst_a;

  int xfer_width  = MIN((int)glyph->GetWidth(),  ximage->width-xOff);
  int xfer_height = MIN((int)glyph->GetHeight(), ximage->height-yOff);
  NS_ASSERTION(xfer_width==(int)glyph->GetWidth(), "image not wide enough");
  NS_ASSERTION(xfer_height==(int)glyph->GetHeight(), "image not tall enough");
  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *glyph_p = glyph->GetBuffer();
  PRUint8 *imageLineStart = (PRUint8 *)ximage->data
                             + 3*xOff + (yOff * ximage->bytes_per_line);

  for (int row=0; row<xfer_height; row++) {
    PRUint8 *image_p = (PRUint8 *)imageLineStart;
    for (int j=0; j<xfer_width; j++,image_p+=3,glyph_p++) {
      src_a = *glyph_p;
      if (src_a == 0)
        continue;
      src_a = aWeightTable[src_a]; // weight the image

      if (src_a == 255) {
        image_p[0] = r;
        image_p[1] = g;
        image_p[2] = b;
        continue;
      }
      dst_a = 255 - src_a;

      image_p[0] = ((r*src_a) + (image_p[2]*dst_a)) >> 8;
      image_p[1] = ((g*src_a) + (image_p[1]*dst_a)) >> 8;
      image_p[2] = ((b*src_a) + (image_p[0]*dst_a)) >> 8;
    }
    glyph_p += -xfer_width + glyph->GetBufferWidth();
    imageLineStart += ximage->bytes_per_line;
  }
}

#ifdef DEBUG
static void
nsBlendPixel0888(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint32 *pPixel = ((PRUint32*)(ximage->data+(y*ximage->bytes_per_line)))+x;
  PRUint32 pixel = *pPixel;
  PRUint32 hibits = pixel & 0xFF000000;
  if (src_a >= 255) {
    *pPixel = hibits | (r << 16) | (g << 8) | b;
    return;
  }

  PRUint32 dst_a = 255 - src_a;
  PRUint32 red   = ((r*src_a) + (((pixel>>16)&0xFF) * dst_a)) >> 8;
  PRUint32 green = ((g*src_a) + (((pixel>>8) &0xFF) * dst_a)) >> 8;
  PRUint32 blue  = ((b*src_a) + (( pixel     &0xFF) * dst_a)) >> 8;
  *pPixel = hibits | (red << 16) | (green << 8) | blue;
}

static void
nsBlendPixel0888_br(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint32 *pPixel = ((PRUint32*)(ximage->data+(y*ximage->bytes_per_line)))+x;
  PRUint32 pixel = *pPixel;
  PRUint32 lowbits = pixel & 0xFF000000;
  if (src_a >= 255) {
    *pPixel = lowbits | (b << 24) | (g << 16) | (r<<8);
    return;
  }

  PRUint32 dst_a = 255 - src_a;
  PRUint32 red   = ((r*src_a) + (((pixel>>16)&0xFF) * dst_a)) >> 8;
  PRUint32 green = ((g*src_a) + (((pixel>>8) &0xFF) * dst_a)) >> 8;
  PRUint32 blue  = ((b*src_a) + (( pixel     &0xFF) * dst_a)) >> 8;
  *pPixel = lowbits | (blue << 24) | (green << 16) | (red<<8);
}

static void
nsBlendPixel555(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint16 *pPixel = ((PRUint16*)(ximage->data+(y*ximage->bytes_per_line)))+x;
  PRUint16 pixel = *pPixel;
  if (src_a >= 255) {
    *pPixel = ((r&0xF8)<<7) + ((g&0xF8)<<2) + ((b)>>3);
    return;
  }

  PRUint16 dst_a = 255 - src_a;

  PRUint16 red   = ((r*src_a) + (((pixel>>7) &0xF8) * dst_a)) >> 8;
  PRUint16 green = ((g*src_a) + (((pixel>>2) &0xF8) * dst_a)) >> 8;
  PRUint16 blue  = ((b*src_a) + (((pixel<<3) &0xF8) * dst_a)) >> 8;
  *pPixel = ((red&0xF8)<<7) + ((green&0xF8)<<2) + ((blue)>>3);
}

static void
nsBlendPixel555_br(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint16 *pPixel = ((PRUint16*)(ximage->data+(y*ximage->bytes_per_line)))+x;
  PRUint16 pixel = *pPixel;
  if (src_a >= 255) {
    *pPixel = ((r&0xF8)   >> 1) |
              ((g&0xC0)>> 6) | ((g&0x38)<<10) |
              ((b&0xF8)<< 5);
    return;
  }

  PRUint16 dst_a = 255 - src_a;
  // unreversed: --:R7:R6:R5:R4:R3:G7:G6  G5:G4:G3:B7:B6:B5:B4:B3
  //   reversed: G5:G4:G3:B7:B6:B5:B4:B3  --:R7:R6:R5:R4:R3:G7:G6
  PRUint16 pixel_r =  (pixel>>1) & 0xF8;
  PRUint16 pixel_g = ((pixel<<6) & 0xC0) | ((pixel>>10) & 0x38);
  PRUint16 pixel_b =  (pixel>>5) & 0xF8;

  PRUint16 red   = ((r*src_a) + (pixel_r * dst_a)) >> 8;
  PRUint16 green = ((g*src_a) + (pixel_g * dst_a)) >> 8;
  PRUint16 blue  = ((b*src_a) + (pixel_b * dst_a)) >> 8;
  *pPixel = ((red&0xF8)   >> 1) |
            ((green&0xC0)>> 6) | ((green&0x38)<<10) |
            ((blue&0xF8)<< 5);
}

static void
nsBlendPixel565(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint16 *pPixel = ((PRUint16*)(ximage->data+(y*ximage->bytes_per_line)))+x;
  PRUint16 pixel = *pPixel;
  if (src_a >= 255) {
    *pPixel = ((r&0xF8)<<8) + ((g&0xFC)<<3) + ((b)>>3);
    return;
  }

  PRUint16 dst_a = 255 - src_a;

  PRUint16 red   = ((r*src_a) + (((pixel>>8) &0xF8) * dst_a)) >> 8;
  PRUint16 green = ((g*src_a) + (((pixel>>3) &0xFC) * dst_a)) >> 8;
  PRUint16 blue  = ((b*src_a) + (((pixel<<3) &0xF8) * dst_a)) >> 8;
  *pPixel = ((red&0xF8)<<8) + ((green&0xFC)<<3) + ((blue)>>3);
}

static void
nsBlendPixel565_br(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint16 *pPixel = ((PRUint16*)(ximage->data+(y*ximage->bytes_per_line)))+x;
  PRUint16 pixel = *pPixel;
  if (src_a >= 255) {
    *pPixel =   (r&0xF8) |
               ((g&0xE0)>> 5) | ((g&0x1C)<<11) |
               ((b&0xF8)<< 5);
    return;
  }

  PRUint16 dst_a = 255 - src_a;
  // unreversed: R7:R6:R5:R4:R3:G7:G6:G5  G4:G3:G2:B7:B6:B5:B4:B3
  //   reversed: G4:G3:G2:B7:B6:B5:B4:B3  R7:R6:R5:R4:R3:G7:G6:G5
  PRUint16 pixel_r =  (pixel)    & 0xF8;
  PRUint16 pixel_g = ((pixel<<5) & 0xE0) | ((pixel>>11) & 0x1C);
  PRUint16 pixel_b =  (pixel>>5) & 0xF8;

  PRUint16 red   = ((r*src_a) + (pixel_r * dst_a)) >> 8;
  PRUint16 green = ((g*src_a) + (pixel_g * dst_a)) >> 8;
  PRUint16 blue  = ((b*src_a) + (pixel_b * dst_a)) >> 8;
  *pPixel =   (red&0xF8) |
             ((green&0xE0)>> 5) | ((green&0x1C)<<11) |
             ((blue&0xF8)<< 5);
}

static void
nsBlendPixel888_lsb(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *pPixel = ((PRUint8*)(ximage->data+(y*ximage->bytes_per_line))) + 3*x;
  if (src_a >= 255) {
    pPixel[2] = r;
    pPixel[1] = g;
    pPixel[0] = b;
    return;
  }

  PRUint32 dst_a = 255 - src_a;
  pPixel[2] = ((r*src_a) + (pPixel[2] * dst_a)) >> 8;
  pPixel[1] = ((g*src_a) + (pPixel[1] * dst_a)) >> 8;
  pPixel[0] = ((b*src_a) + (pPixel[0] * dst_a)) >> 8;
}

static void
nsBlendPixel888_msb(XImage *ximage, int x, int y, nscolor color, int src_a)
{
  NS_ASSERTION((src_a>=0)&&(src_a<=255), "Invalid alpha");
  if (src_a <= 0)
    return;

  NS_ASSERTION(x<ximage->width,  "x out of bounds");
  NS_ASSERTION(y<ximage->height, "y out of bounds");
  if ((x >= ximage->width) || (y >= ximage->height))
    return;

  PRUint16 r = NS_GET_R(color);
  PRUint16 g = NS_GET_G(color);
  PRUint16 b = NS_GET_B(color);

  PRUint8 *pPixel = ((PRUint8*)(ximage->data+(y*ximage->bytes_per_line))) + 3*x;
  if (src_a >= 255) {
    pPixel[0] = r;
    pPixel[1] = g;
    pPixel[2] = b;
    return;
  }

  PRUint32 dst_a = 255 - src_a;
  pPixel[0] = ((r*src_a) + (pPixel[0] * dst_a)) >> 8;
  pPixel[1] = ((g*src_a) + (pPixel[1] * dst_a)) >> 8;
  pPixel[2] = ((b*src_a) + (pPixel[2] * dst_a)) >> 8;
}
#endif


nscolor
nsPixelToNscolor555(unsigned long aPixel)
{
  int r = (aPixel>>7)&0xF8;
  int g = (aPixel>>2)&0xF8;
  int b = (aPixel<<3)&0xF8;
  nscolor color = NS_RGB(r,g,b);
  return color;
}

nscolor
nsPixelToNscolor565(unsigned long aPixel)
{
  int r = (aPixel>>8)&0xF8;
  int g = (aPixel>>3)&0xFC;
  int b = (aPixel<<3)&0xF8;
  nscolor color = NS_RGB(r,g,b);
  return color;
}

nscolor
nsPixelToNscolor888_lsb(unsigned long aPixel)
{
  int r = (aPixel>>16)&0xFF;
  int g = (aPixel>> 8)&0xFF;
  int b = (aPixel    )&0xFF;
  nscolor color = NS_RGB(r,g,b);
  return color;
}

nscolor
nsPixelToNscolor888_msb(unsigned long aPixel)
{
  int r = (aPixel    )&0xFF;
  int g = (aPixel>> 8)&0xFF;
  int b = (aPixel>>16)&0xFF;
  nscolor color = NS_RGB(r,g,b);
  return color;
}

