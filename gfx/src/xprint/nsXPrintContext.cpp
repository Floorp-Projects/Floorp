/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#include <stdio.h>
#include <stdlib.h>
#include "xlibrgb.h"
#include "nsXPrintContext.h"

#undef XPRINT_ON_SCREEN

Display * nsXPrintContext::mDisplay = (Display *)0;

static int xerror_handler(Display *display, XErrorEvent *ev) {
    char errmsg[80];
    XGetErrorText(display, ev->error_code, errmsg, 80);
    fprintf(stderr, "lib_xprint: Warning (X Error) -  %s\n", errmsg);
    return 0;
}

/** ---------------------------------------------------
 *  Default Constructor
 */
nsXPrintContext::nsXPrintContext()
{
   mPContext = (XPContext )0;
   mPrintServerName = (char *)0;
   mPrinterName     = (char *)0;
   mScreen = (Screen *)0;
   mVisual = (Visual *)0;
   mGC     = (GC )0;
   mDrawable = (Drawable )0;
   mDepth = 0;
   mAlphaPixmap = 0;
   mImagePixmap = 0;
}

/** ---------------------------------------------------
 *  Destructor
 */
nsXPrintContext::~nsXPrintContext()
{
  // end the document
  EndDocument();
  // Cleanup things allocated along the way
  XpDestroyContext(mDisplay, mPContext);
  XCloseDisplay(mDisplay);
}

NS_IMETHODIMP 
nsXPrintContext::Init(nsIDeviceContextSpecXP *aSpec)
{
  int prefDepth = 8;
#ifdef XPRINT_ON_SCREEN
  if (nsnull == mDisplay)
     mDisplay  = (Display *)XOpenDisplay(NULL);
  mScreen = XDefaultScreenOfDisplay(mDisplay);
  xlib_rgb_init_with_depth(mDisplay, mScreen, prefDepth);
  mScreenNumber = XDefaultScreen(mDisplay);
  SetupWindow(0, 0, 1000, 1100);
  mPrintResolution = 300;
  mTextZoom = 1.0f; 
  XMapWindow(mDisplay, mDrawable);
#else
  char *printservername = (char *)0;
  if (!(printservername =  getenv("XPDISPLAY"))) {
	printservername = strdup("localhost:1");
  }
  if (nsnull == mDisplay) {
     if (!(mDisplay = XOpenDisplay(printservername))) {
        fprintf(stderr,"failed to open display '%s'\n", printservername);
	return NS_ERROR_FAILURE;
     }
  }
  unsigned short width, height;
  XRectangle rect;

  SetupPrintContext(aSpec);
  mScreen = XpGetScreenOfContext(mDisplay, mPContext);
  mScreenNumber = XScreenNumberOfScreen(mScreen);
  xlib_rgb_init_with_depth(mDisplay, mScreen, prefDepth);

  XpGetPageDimensions(mDisplay, mPContext, &width, &height, &rect);
  SetupWindow(rect.x, rect.y, rect.width, rect.height);

  // mGC =  XDefaultGCOfScreen(mScreen);
  mTextZoom = 2.0f; 
#endif
  mPDisplay = mDisplay; 
  (void)XSetErrorHandler(xerror_handler);
  XSynchronize(mDisplay, True);
  return NS_OK;
}

NS_IMETHODIMP
nsXPrintContext::SetupWindow(int x, int y, int width, int height)
{
  XSetWindowAttributes xattributes;
  long xattributes_mask;
  Window parent_win;
  XVisualInfo *visual_info;
  unsigned long gcmask;
  XGCValues gcvalues;

  mWidth = width;
  mHeight = height;
  visual_info = xlib_rgb_get_visual_info();

  parent_win = RootWindow(mDisplay, mScreenNumber);
  xattributes.background_pixel = WhitePixel (mDisplay, mScreenNumber);
  xattributes.border_pixel = BlackPixel (mDisplay, mScreenNumber);
  xattributes_mask |= CWBorderPixel | CWBackPixel;

#ifdef _USE_PRIMITIVE_CALL_
  mVisual = visual_info->visual;
  mDepth = visual_info->depth;
 
  mDrawable = (Drawable) XCreateWindow(mDisplay, parent_win, x, y, width, 
			height, 2,
			mDepth, InputOutput, mVisual, xattributes_mask, 
			&xattributes );
#else
   mDrawable = (Drawable)XCreateSimpleWindow(mDisplay,
                                parent_win,
                                x, y,
                                width, height,
                                0, BlackPixel(mDisplay, mScreenNumber),
                                WhitePixel(mDisplay, mScreenNumber));
  mDepth  = XDefaultDepth(mDisplay, mScreenNumber);
  mVisual = XDefaultVisual(mDisplay, mScreenNumber);
#endif
  gcmask = GCBackground | GCForeground | GCFunction ;
  gcvalues.background = WhitePixel(mDisplay, mScreenNumber);
  gcvalues.foreground = BlackPixel(mDisplay, mScreenNumber);
  gcvalues.function = GXcopy;
  mGC     = XCreateGC(mDisplay, mDrawable, gcmask, &gcvalues);
 return NS_OK;
}

NS_IMETHODIMP
nsXPrintContext::SetupPrintContext(nsIDeviceContextSpecXP *aSpec)
{
  XPPrinterList plist;
  int 		plistcnt;
  PRBool isAPrinter;
  int printSize;
  float top, bottom, left, right;
  char *buf;
  char cbuf[128];
  const int loglength = 128;
  char logname[loglength];

  // Get the Attributes
  aSpec->GetToPrinter( isAPrinter );
  aSpec->GetSize( printSize );
  aSpec->GetTopMargin( top );
  aSpec->GetBottomMargin( bottom );
  aSpec->GetLeftMargin( left );
  aSpec->GetRightMargin( right );

  
  // Check the output type
  if (isAPrinter == PR_TRUE) {
     aSpec->GetCommand( &buf );
  } else {
     aSpec->GetPath( &buf );
  }
     
  plist     =  XpGetPrinterList(mDisplay, buf, &plistcnt);
  mPContext =  XpCreateContext(mDisplay, plist[0].name );
  XpFreePrinterList(plist);
  XpSetContext(mDisplay, mPContext);

  // Set the Job attribute
  if (!getlogin_r(logname, loglength)) {
     sprintf(logname, "Mozilla-User");
  }
  sprintf(cbuf,"*job-owner: %s", logname);
  XpSetAttributes(mDisplay, mPContext,
                  XPJobAttr,(char *)cbuf,XPAttrMerge);

  // Set the Document Attributes
  // XpSetAttributes(mDisplay,mPContext, XPDocAttr,(char *)"*content-orientation: landscape",XPAttrMerge);

  mPrintResolution = 300;
  
  char *print_resolution = XpGetOneAttribute(mDisplay, mPContext, XPDocAttr,
					(char *)"default-printer-resolution");
  if (print_resolution) {
     char *tmp_str = strrchr(print_resolution, ':');
     if (tmp_str) {
        tmp_str++;
        mPrintResolution = atoi(tmp_str);
     }
     XFree(print_resolution);
  }
  // Check the output type
  if (isAPrinter == PR_TRUE) {
     XpStartJob(mDisplay, XPSpool );
  } else {
     XpStartJob(mDisplay, XPGetData );
  } 
  
  return NS_OK;
}  

NS_IMETHODIMP
nsXPrintContext::BeginDocument()
{
  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::BeginPage()
{
  XpStartPage(mDisplay, mDrawable);
  // Move the print window according to the given margin
  // XMoveWindow(mDisplay, mDrawable, 100, 100);
  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::EndPage()
{
  XpEndPage(mDisplay);
  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::EndDocument()
{
  XFlush(mDisplay);
  XpEndJob(mDisplay);
  // Cleanup things allocated along the way
  XpDestroyContext(mDisplay, mPContext);
  // XCloseDisplay(mDisplay);
  return NS_OK;
}

NS_IMETHODIMP
nsXPrintContext::DrawImage(nsIImage *aImage,
		PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
		PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
   PRUint8 *image_bits = aImage->GetBits();
   PRInt32 row_bytes   = aImage->GetLineStride();
   
  // XpSetImageResolution(mDisplay, mPContext, new_res, &prev_res);
   xlib_draw_gray_image(mDrawable,
                      mGC,
                      aDX, aDY, aDWidth, aDHeight,
                      XLIB_RGB_DITHER_MAX,
                      image_bits + row_bytes * aSY + 3 * aDX,
                      row_bytes);

  // XpSetImageResolution(mDisplay, mPContext, prev_res, &new_res);
  return NS_OK;
}


// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP
nsXPrintContext::DrawImage(nsIImage *aImage,
                 PRInt32 aX, PRInt32 aY,
                 PRInt32 aWidth, PRInt32 aHeight)
{
  PRInt32 width 	= aImage->GetWidth();
  PRInt32 height 	= aImage->GetHeight();
  PRUint8 *alphaBits 	= aImage->GetAlphaBits();
  PRInt32  alphaRowBytes = aImage->GetAlphaLineStride();
  PRUint8 *image_bits 	= aImage->GetBits();
  PRInt32 row_bytes   	= aImage->GetLineStride();

  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  if ((aWidth != width) || (aHeight != height)) {
     aWidth = width;
     aHeight = height;
  }

  XImage *x_image = nsnull;
  GC      gc;
  XGCValues gcv;

  // Create gc clip-mask on demand
  if ((alphaBits != nsnull) && (mAlphaPixmap == 0)) {
    if (!mAlphaPixmap) {
      mAlphaPixmap = XCreatePixmap(mDisplay, 
				RootWindow(mDisplay, mScreenNumber),
				aWidth, aHeight, 1);
    }

    // Make an image out of the alpha-bits created by the image library
    x_image = XCreateImage(mDisplay, mVisual,
                           1, /* visual depth...1 for bitmaps */
                           XYPixmap,
                           0, /* x offset, XXX fix this */
                           (char *)alphaBits,  /* cast away our sign. */
                           aWidth,
                           aHeight,
                           32,/* bitmap pad */
                           alphaRowBytes); /* bytes per line */

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
    memset(&gcv, 0, sizeof(XGCValues));
    gcv.function = GXcopy;
    gc = XCreateGC(mDisplay, mAlphaPixmap, GCFunction, &gcv);

    XPutImage(mDisplay, mAlphaPixmap, gc, x_image, 0, 0, 0, 0,
              aWidth, aHeight);
    XFreeGC(mDisplay, gc);

    // Now we are done with the temporary image
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);
  }
  
  if (nsnull == mImagePixmap) {
    // Create an off screen pixmap to hold the image bits.
    mImagePixmap = XCreatePixmap(mDisplay,
				RootWindow(mDisplay, mScreenNumber),
                                aWidth, aHeight,
                                mDepth);
    XSetClipOrigin(mDisplay, mGC, 0, 0);
    XSetClipMask(mDisplay, mGC, None);
    GC gc;
    XGCValues xvalues;
    unsigned long xvalues_mask;

    xvalues.function = GXcopy;
    xvalues.fill_style = FillSolid;
    xvalues.arc_mode = ArcPieSlice;
    xvalues.subwindow_mode = ClipByChildren;
    xvalues.graphics_exposures = True;
    xvalues_mask = GCFunction | GCFillStyle | GCArcMode | GCSubwindowMode | GCGraphicsExposures;

    gc = XCreateGC(mDisplay, mImagePixmap, xvalues_mask, &xvalues); 
    // XpSetImageResolution(mDisplay, mPContext, new_res, &prev_res);
#ifdef _USE_PRIMITIVE_CALL_
    xlib_draw_gray_image(mImagePixmap,
                      gc,
                      0, 0, aWidth, aHeight,
                      XLIB_RGB_DITHER_MAX,
                      image_bits,
                      row_bytes);
#else
    xlib_draw_rgb_image (mImagePixmap,
                         gc,
                         0, 0, aWidth, aHeight,
                         XLIB_RGB_DITHER_NONE,
                         image_bits, row_bytes);
#endif
    // XpSetImageResolution(mDisplay, mPContext, prev_res, &new_res);
  }
  if (nsnull  != mAlphaPixmap)
  {
    // set up the gc to use the alpha pixmap for clipping
    XSetClipOrigin(mDisplay, mGC, aX, aY);
    XSetClipMask(mDisplay, mGC, mAlphaPixmap);
  }

  // copy our off screen pixmap onto the window.
  XCopyArea(mDisplay,                  // display
            mImagePixmap,              // source
            mDrawable,    		// dest
            mGC,          		// GC
            0, 0,                    // xsrc, ysrc
            aWidth, aHeight,           // width, height
            aX, aY);                     // xdest, ydest

  if (mAlphaPixmap != nsnull) {
    XSetClipOrigin(mDisplay, mGC, 0, 0);
    XSetClipMask(mDisplay, mGC, None);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXPrintContext::GetPrintResolution(int &aPrintResolution) const
{
  aPrintResolution = mPrintResolution;
  return NS_OK;
}

NS_IMETHODIMP nsXPrintContext::SetForegroundColor(nscolor aColor)
{
  
  xlib_rgb_gc_set_foreground(mGC,
                NS_RGB(NS_GET_B(aColor), NS_GET_G(aColor), NS_GET_R(aColor)));
  return NS_OK;
}
