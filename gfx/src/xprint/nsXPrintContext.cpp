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
 * Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include "xlibrgb.h"
#include "nsXPrintContext.h"
#include "xprintutil.h"

/* DEBUG: use 
 * % export NSPR_LOG_MODULES=nsXPrintContext:5 
 * to see these debug messages... 
 */
static PRLogModuleInfo *nsXPrintContextLM = PR_NewLogModule("nsXPrintContext");

#ifdef __SUNPRO_C
extern "C" /* Make Sun Workshop and other conformant compilers happy... :-) */
#endif
static 
int xerror_handler(Display *display, XErrorEvent *ev) 
{
    /* this should _never_ be happen... but if this happens - debug mode or not - scream !!! */
    char errmsg[80];
    XGetErrorText(display, ev->error_code, errmsg, sizeof(errmsg));
    fprintf(stderr, "lib_xprint: Warning (X Error) -  %s\n", errmsg);
    return 0;
}

/** ---------------------------------------------------
 *  Default Constructor
 */
nsXPrintContext::nsXPrintContext()
{
   PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::nsXPrintContext()\n"));
   
   mPDisplay    = (Display *)nsnull;
   mPContext    = (XPContext)None;
   mScreen      = (Screen *)nsnull;
   mVisual      = (Visual *)nsnull;
   mGC          = (GC)None;
   mDrawable    = (Drawable)None;
   mDepth       = 0;
   mAlphaPixmap = (Pixmap)None;
   mImagePixmap = (Pixmap)None;
}

/** ---------------------------------------------------
 *  Destructor
 */
nsXPrintContext::~nsXPrintContext()
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::~nsXPrintContext()\n"));

  // end the document
  if( mPDisplay != nsnull )
    EndDocument();
}

#ifdef HACK_PRINTONSCREEN
// debug: "print" on display server for quick debugging
// see sleep() in nsXPrintContext::EndDocument()
#define XPRINTONSCREEN (!strcmp("xprint_preview",(aSpec->GetCommand(&buf),buf)))
#endif /* HACK_PRINTONSCREEN */

NS_IMETHODIMP 
nsXPrintContext::Init(nsIDeviceContextSpecXP *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::Init()\n"));

  int   prefDepth = 24; /* 24 or 8 */
  char *buf;

/* print on screen(="normal" Xserver) is "DEBUG" for now... 
 * maybe usefull for preview, too... 
 */
#ifdef HACK_PRINTONSCREEN
  if( XPRINTONSCREEN )
  {
    mPDisplay  = (Display *)XOpenDisplay(nsnull);
    mScreen = XDefaultScreenOfDisplay(mPDisplay);
    xlib_rgb_init_with_depth(mPDisplay, mScreen, prefDepth);
    mScreenNumber = XDefaultScreen(mPDisplay);
    SetupWindow(0, 0, 1200, 1200);
    mPrintResolution = 300;
    mTextZoom = 1.0f; 
    XMapWindow(mPDisplay, mDrawable);
  }
  else
#endif /* HACK_PRINTONSCREEN */  
  {
    unsigned short width, height;
    XRectangle rect;

    if( NS_FAILED( XPU_TRACE(SetupPrintContext(aSpec)) ) )
      return NS_ERROR_FAILURE;
    
    mScreen = XpGetScreenOfContext(mPDisplay, mPContext);
    mScreenNumber = XScreenNumberOfScreen(mScreen);
    xlib_rgb_init_with_depth(mPDisplay, mScreen, prefDepth);

    XpGetPageDimensions(mPDisplay, mPContext, &width, &height, &rect);
    SetupWindow(rect.x, rect.y, rect.width, rect.height);

    mTextZoom = 2.4f; // should this be printer_DPI / display_DPI ?
    XMapWindow(mPDisplay, mDrawable);
  }
  
  /* Set error handler and sync
   * ToDo: unset handler after all is done - what about handler "stacking" ?
   */
  (void)XSetErrorHandler(xerror_handler);
  XSynchronize(mPDisplay, True);
  
  return NS_OK;
}

NS_IMETHODIMP
nsXPrintContext::SetupWindow(int x, int y, int width, int height)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::SetupWindow: x=%d y=%d width=%d height=%d\n", 
         x, y, width, height));

  XSetWindowAttributes xattributes;
  long xattributes_mask;
  Window parent_win;
  XVisualInfo *visual_info;
  unsigned long gcmask;
  XGCValues gcvalues;

  mWidth = width;
  mHeight = height;
  visual_info = xlib_rgb_get_visual_info();

  parent_win = RootWindow(mPDisplay, mScreenNumber);
  xattributes.background_pixel = WhitePixel (mPDisplay, mScreenNumber);
  xattributes.border_pixel = BlackPixel (mPDisplay, mScreenNumber);
  xattributes_mask |= CWBorderPixel | CWBackPixel;

  mDrawable = (Drawable)XCreateSimpleWindow(mPDisplay,
                                parent_win,
                                x, y,
                                width, height,
                                0, BlackPixel(mPDisplay, mScreenNumber),
                                WhitePixel(mPDisplay, mScreenNumber));
  mDepth  = XDefaultDepth(mPDisplay, mScreenNumber);
  mVisual = XDefaultVisual(mPDisplay, mScreenNumber);

  gcmask = GCBackground | GCForeground | GCFunction ;
  gcvalues.background = WhitePixel(mPDisplay, mScreenNumber);
  gcvalues.foreground = BlackPixel(mPDisplay, mScreenNumber);
  gcvalues.function = GXcopy;
  mGC     = XCreateGC(mPDisplay, mDrawable, gcmask, &gcvalues); /* ToDo: Check for error */
  return NS_OK;
}


NS_IMETHODIMP
nsXPrintContext::SetupPrintContext(nsIDeviceContextSpecXP *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::SetupPrintContext()\n"));
  
  int printSize;
  float top, bottom, left, right;
  char *buf;

  // Get the Attributes
  aSpec->GetToPrinter(mIsAPrinter);
  aSpec->GetSize(printSize);
  aSpec->GetTopMargin(top);
  aSpec->GetBottomMargin(bottom);
  aSpec->GetLeftMargin(left);
  aSpec->GetRightMargin(right);

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::SetupPrintContext: borders top=%f, bottom=%f, left=%f, right=%f\n", 
         top, bottom, left, right));

  /* get destination printer (we need this when printing to file as
   * the printer DDX in Xprt generates the data...) 
   */
  aSpec->GetCommand(&buf);
  
  /* Are we "printing" to a file instead to the print queue ? */
  if (!mIsAPrinter) 
  {
    /* ToDo: Guess a matching file extension of user did not set one - Xprint 
     * currrently may use %s.ps, %s.pcl, %s.xwd, %s.pdf etc.
     * Best idea would be to ask a "datatyping" service (like CDE's datatyping 
     * functions) what to-do...
     */
    aSpec->GetPath(&mPrintFile);

    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("print to file '%s'\n", XPU_NULLXSTR(mPrintFile)));
    
    if( (mPrintFile == nsnull) || (strlen(mPrintFile) == 0) )
      return NS_ERROR_FAILURE;
  }
     
  /* get printer, either by "name" (foobar) or "name@display" (foobar@gaja:5)
   * ToDo: report error to user (dialog)
   */
  if( XpuGetPrinter(buf, &mPDisplay, &mPContext) != 1 )
    return NS_ERROR_FAILURE;

  /* set printer context
   * WARNING: after this point it is no longer allows to change job attributes
   * only after the XpSetContext() call the alllication is allowed to make 
   * assumptions about available fonts - otherwise you may get the wrong 
   * ones !!
   */
  XPU_TRACE(XpSetContext(mPDisplay, mPContext));

  /* get default printer reolution
   * (note: if's AFAIK a Xprt configuration error when "default-printer-resolution" 
   * cannot be obtained - return with an error to avoid the case that we#re working 
   * with a faulty printer config...
   * ToDo: Report error to user (dialog)
   */
  if( XpuGetOneLongAttribute(mPDisplay, mPContext, XPDocAttr, "default-printer-resolution", &mPrintResolution) != 1 )
    return NS_ERROR_FAILURE;

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("print resolution %ld\n", (long)mPrintResolution));
  
  /* We want to get events when Xp(Start|End)(Job|Page) requests are processed... */  
  XpSelectInput(mPDisplay, mPContext, XPPrintMask);  

  // Set the Document Attributes
  // XpSetAttributes(mPDisplay,mPContext, XPDocAttr,(char *)"*content-orientation: landscape",XPAttrMerge);

  return NS_OK;
}  
  
/* MyConvertUCS2ToLocalEncoding:
 * Note that this function is _only_ a _hack_ until RFE 73446 
 * (http://bugzilla.mozilla.org/show_bug.cgi?id=73446 - "RFE: 
 * Need NS_ConvertUCS2ToLocalEncoding() and 
 * NS_ConvertLocalEncodingToUCS2()") gets implemented...
 * Below we need COMPOUNT_TEXT which usually is the same as the Xserver's 
 * local encoding - this hack should at least work for C/POSIX 
 * and *.UTF-8 locales...
 */
static 
const char *MyConvertUCS2ToLocalEncoding( PRUnichar *str )
{
  /* Use strdup() to avoid any silly effects... 
   */
  return PL_strdup(NS_ConvertUCS2toUTF8(str).get());
}  


NS_IMETHODIMP
nsXPrintContext::BeginDocument( PRUnichar *aTitle )
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::BeginDocument()\n"));
  
  char *s = nsnull, 
       *job_title;
       
  if( aTitle != nsnull )
    job_title = s = (char *)MyConvertUCS2ToLocalEncoding(aTitle); 
  else
    job_title = "Mozilla document without title";  

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::BeginDocument: document title: '%s'\n", XPU_NULLXSTR(job_title)));
  
  /* Set the Job Attributes */
  XpuSetJobTitle(mPDisplay, mPContext, job_title);
  
  if( s != nsnull ) 
    PL_strfree(s);

  // Check the output type
  if(mIsAPrinter) 
  {
    XPU_TRACE(XpStartJob(mPDisplay, XPSpool));
    XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, XPStartJobNotify));
  } 
  else 
  {   
    XPU_TRACE(XpStartJob(mPDisplay, XPGetData));
    
    if( XPU_TRACE(mXpuPrintToFileHandle = XpuPrintToFile(mPDisplay, mPContext, mPrintFile)) == nsnull )
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
             ("nsXPrintContext::BeginDocument(): XpuPrintToFile failure %s/(%d)\n", 
             strerror(errno), errno));

      return NS_ERROR_FAILURE;
    }
    
    XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, XPStartJobNotify));
  } 
  
  return NS_OK;
}  

NS_IMETHODIMP
nsXPrintContext::BeginPage()
{
  // Move the print window according to the given margin
  // XMoveWindow(mPDisplay, mDrawable, 100, 100);
  
  XPU_TRACE(XpStartPage(mPDisplay, mDrawable));
  XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, XPStartPageNotify));

  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::EndPage()
{
  XPU_TRACE(XpEndPage(mPDisplay));
  XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, XPEndPageNotify));
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::EndDocument()
{
  XPU_TRACE(XpEndJob(mPDisplay));
  XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, XPEndJobNotify));

  /* Are we printing to a file ? */
  if( !mIsAPrinter )
  {
    NS_ASSERTION(nsnull != mXpuPrintToFileHandle, "mXpuPrintToFileHandle is null.");
    
    if( XPU_TRACE(XpuWaitForPrintFileChild(mXpuPrintToFileHandle)) == XPGetDocFinished )
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuWaitForPrintFileChild returned success.\n"));
    }
  }
    
#ifdef HACK_PRINTONSCREEN        
  /* HACK: sleep 15secs if we're displaying to an "display" server
   * see nsXPrintContext::Init and XprintOnScreen macro above - this is only used if 
   * we are printing to a normal Xserver and not to a Xprint server (e.g. in rare 
   * rare cases...)
   */
  if( XpuCheckExtension(mPDisplay) != 1 )
  {
    sleep(15);
  }
#endif /* HACK_PRINTONSCREEN */  

  // Cleanup things allocated along the way
  XpDestroyContext(mPDisplay, mPContext);
  mPContext = nsnull;
  XCloseDisplay(mPDisplay);
  mPDisplay = nsnull;
    
  return NS_OK;
}

NS_IMETHODIMP
nsXPrintContext::DrawImage(nsIImage *aImage,
                PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  PRUint8 *image_bits = aImage->GetBits();
  PRInt32  row_bytes  = aImage->GetLineStride();
   
  // XpSetImageResolution(mPDisplay, mPContext, new_res, &prev_res);
  xlib_draw_gray_image(mDrawable,
                       mGC,
                       aDX, aDY, aDWidth, aDHeight,
                       XLIB_RGB_DITHER_MAX,
                       image_bits + row_bytes * aSY + 3 * aDX,
                       row_bytes);

  // XpSetImageResolution(mPDisplay, mPContext, prev_res, &new_res);
  return NS_OK;
}


// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP
nsXPrintContext::DrawImage(nsIImage *aImage,
                           PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  PRInt32  width         = aImage->GetWidth();
  PRInt32  height        = aImage->GetHeight();
  PRUint8 *alphaBits     = aImage->GetAlphaBits();
  PRInt32  alphaRowBytes = aImage->GetAlphaLineStride();
  PRUint8 *image_bits    = aImage->GetBits();
  PRInt32  row_bytes     = aImage->GetLineStride();

  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  if ((aWidth != width) || (aHeight != height)) 
  {
     aWidth = width;
     aHeight = height;
  }

  XImage    *x_image = nsnull;
  GC         gc;
  XGCValues  gcv;

  // Create gc clip-mask on demand
  if ((alphaBits != nsnull) && (mAlphaPixmap == None)) {
    if (mAlphaPixmap==None) {
      mAlphaPixmap = XCreatePixmap(mPDisplay, 
                                RootWindow(mPDisplay, mScreenNumber),
                                aWidth, aHeight, 1); /* ToDo: Check for error */
    }

    /* Make an image out of the alpha-bits created by the image library (ToDo: check for error) */
    x_image = XCreateImage(mPDisplay, mVisual,
                           1,                 /* visual depth...1 for bitmaps */
                           XYPixmap,
                           0,                 /* x offset, XXX fix this */
                           (char *)alphaBits, /* cast away our sign. */
                           aWidth,
                           aHeight,
                           32,                /* bitmap pad */
                           alphaRowBytes);    /* bytes per line */

    x_image->bits_per_pixel = 1;

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
    gc = XCreateGC(mPDisplay, mAlphaPixmap, GCFunction, &gcv);

    XPutImage(mPDisplay, mAlphaPixmap, gc, x_image, 0, 0, 0, 0,
              aWidth, aHeight);
    XFreeGC(mPDisplay, gc);

    // Now we are done with the temporary image
    x_image->data = 0;          /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);
  }
  
  if (mImagePixmap == None) {
    // Create an off screen pixmap to hold the image bits.
    mImagePixmap = XCreatePixmap(mPDisplay,
                                 RootWindow(mPDisplay, mScreenNumber),
                                 aWidth, aHeight,
                                 mDepth);
    XSetClipOrigin(mPDisplay, mGC, 0, 0);
    XSetClipMask(mPDisplay, mGC, None);
    GC gc;
    XGCValues xvalues;
    unsigned long xvalues_mask;

    xvalues.function           = GXcopy;
    xvalues.fill_style         = FillSolid;
    xvalues.arc_mode           = ArcPieSlice;
    xvalues.subwindow_mode     = ClipByChildren;
    xvalues.graphics_exposures = True;
    xvalues_mask = GCFunction | GCFillStyle | GCArcMode | GCSubwindowMode | GCGraphicsExposures;

    gc = XCreateGC(mPDisplay, mImagePixmap, xvalues_mask, &xvalues); 

    // XpSetImageResolution(mPDisplay, mPContext, new_res, &prev_res);
    xlib_draw_rgb_image(mImagePixmap,
                        gc,
                        0, 0, aWidth, aHeight,
                        XLIB_RGB_DITHER_NONE,
                        image_bits, row_bytes);
    // XpSetImageResolution(mPDisplay, mPContext, prev_res, &new_res);
  }
  
  if (mAlphaPixmap != None)
  {
    // set up the gc to use the alpha pixmap for clipping
    XSetClipOrigin(mPDisplay, mGC, aX, aY);
    XSetClipMask(mPDisplay, mGC, mAlphaPixmap);
  }

  // copy our off screen pixmap onto the window.
  XCopyArea(mPDisplay,                 // display
            mImagePixmap,              // source
            mDrawable,                 // dest
            mGC,                       // GC
            0, 0,                      // xsrc, ysrc
            aWidth, aHeight,           // width, height
            aX, aY);                   // xdest, ydest

  if (mAlphaPixmap != None) 
  {
    XSetClipOrigin(mPDisplay, mGC, 0, 0);
    XSetClipMask(mPDisplay, mGC, None);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXPrintContext::GetPrintResolution(int &aPrintResolution) const
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::GetPrintResolution()\n"));
  
  aPrintResolution = mPrintResolution;
  return NS_OK;
}

NS_IMETHODIMP nsXPrintContext::SetForegroundColor(nscolor aColor)
{
  xlib_rgb_gc_set_foreground(mGC,
                             NS_RGB(NS_GET_B(aColor), NS_GET_G(aColor), NS_GET_R(aColor)));
  return NS_OK;
}

