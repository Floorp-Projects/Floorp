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

/* Note: Sun Workshop 6 may have problems when mixing <string,h> 
 * and X includes. This was fixed by recent Xsun patches which 
 * are part of the "recommended patches" for Solaris. 
 * See bug 88703...
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> /* for strerror & memset& memset  */
#include "xlibrgb.h"
#include "nsXPrintContext.h"
#include "nsDeviceContextXP.h"
#include "xprintutil.h"
#include "prenv.h" /* for PR_GetEnv */

/* misc defines */
//#define HACK_PRINTONSCREEN 1
#define XPRINT_MAKE_24BIT_VISUAL_AVAILABLE_FOR_TESTING 1

#ifdef XPRINT_NOT_YET /* ToDo: make this dynamically */
#define NS_XPRINT_RGB_DITHER XLIB_RGB_DITHER_NONE
#else
#define NS_XPRINT_RGB_DITHER ((mDepth>12)?(XLIB_RGB_DITHER_NONE):(XLIB_RGB_DITHER_MAX))
#endif

#ifdef PR_LOGGING 
/* DEBUG: use 
 * % export NSPR_LOG_MODULES=nsXPrintContext:5 
 * to see these debug messages... 
 */
static PRLogModuleInfo *nsXPrintContextLM = PR_NewLogModule("nsXPrintContext");
#endif /* PR_LOGGING */

static 
int xerror_handler( Display *display, XErrorEvent *ev )
{
    /* this should _never_ be happen... but if this happens - debug mode or not - scream !!! */
    char errmsg[80];
    XGetErrorText(display, ev->error_code, errmsg, sizeof(errmsg));
    fprintf(stderr, "nsGfxXprintModule: Warning (X Error) -  %s\n", errmsg);
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
  mDrawable    = (Drawable)None;
  mDepth       = 0;
  mIsGrayscale = PR_FALSE; /* default is color output */
  mIsAPrinter  = PR_TRUE;  /* default destination is printer */
}

/** ---------------------------------------------------
 *  Destructor
 */
nsXPrintContext::~nsXPrintContext()
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::~nsXPrintContext()\n"));

  // end the document
  if( mPDisplay != nsnull )
  {
    XPU_TRACE(XpDestroyContext(mPDisplay, mPContext));

    // Cleanup things allocated along the way
    XPU_TRACE(xlib_rgb_detach());
    
    XPU_TRACE(XCloseDisplay(mPDisplay));
    
    mPContext = nsnull;
    mPDisplay = nsnull;
  }
  
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::~nsXPrintContext() done.\n"));
}

NS_IMETHODIMP 
nsXPrintContext::Init(nsDeviceContextXp *dc, nsIDeviceContextSpecXp *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::Init()\n"));

  int   prefDepth = 8;  /* 24 or 8 for PS DDX, 24, 8, 1 for PCL DDX... 
                         * I wish current Xprt would have a 1bit/8bit StaticGray 
                         * visual for the PS DDX... ;-( 
                         */
  char *buf;

/* I can't get any other visual than the 8bit peudocolor one working... BAD...
 * This env var allows others to test this without hacking their own binaries...
 */
#ifdef XPRINT_MAKE_24BIT_VISUAL_AVAILABLE_FOR_TESTING  
  if( PR_GetEnv("MOZILLA_XPRINT_EXPERIMENTAL_USE_24BIT_VISUAL") != nsnull )
  {
    prefDepth = 24;
  }
#endif /* XPRINT_MAKE_24BIT_VISUAL_AVAILABLE_FOR_TESTING */  

/* print on screen(="normal" Xserver) is "DEBUG" for now... 
 * maybe usefull for preview, too... 
 */
#ifdef HACK_PRINTONSCREEN
  // debug: "print" on display server for quick debugging
  // see sleep() in nsXPrintContext::EndDocument()
  if( !strcmp("xprint_preview",(aSpec->GetCommand(&buf),buf)) )
  {
    mPDisplay  = (Display *)XOpenDisplay(nsnull);
    mScreen = XDefaultScreenOfDisplay(mPDisplay);
    mScreenNumber = XScreenNumberOfScreen(mScreen);
    xlib_rgb_init_with_depth(mPDisplay, mScreen, prefDepth);
    xlib_disallow_image_tiling(TRUE);

    SetupWindow(0, 0, 1200, 1200);
    mPrintResolution = 91 /* or 301 - intentionally forcing scaling */;
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
    xlib_disallow_image_tiling(TRUE);

    XpGetPageDimensions(mPDisplay, mPContext, &width, &height, &rect);
    SetupWindow(rect.x, rect.y, rect.width, rect.height);

    XMapWindow(mPDisplay, mDrawable);
  }
  
  mContext = dc;
    
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

  Window                 parent_win;
  XVisualInfo           *visual_info;
  XSetWindowAttributes   xattributes;
  long                   xattributes_mask;
  unsigned long          background,
                         foreground;
  
  mWidth  = width;
  mHeight = height;

  visual_info = xlib_rgb_get_visual_info();
  mVisual     = xlib_rgb_get_visual();
  mDepth      = xlib_rgb_get_depth();

  background = XWhitePixel(mPDisplay, mScreenNumber);
  foreground = XBlackPixel(mPDisplay, mScreenNumber);  
  parent_win = XRootWindow(mPDisplay, mScreenNumber);                                         
                                             
  xattributes.background_pixel = background;
  xattributes.border_pixel     = foreground;
  xattributes.colormap         = xlib_rgb_get_cmap();
  xattributes_mask             = CWBorderPixel | CWBackPixel;
  if( xattributes.colormap )
    xattributes_mask |= CWColormap;

  mDrawable = (Drawable)XCreateWindow(mPDisplay, parent_win, x, y,
                                      width, height, 0,
                                      mDepth, InputOutput, mVisual, xattributes_mask,
                                      &xattributes);

  /* %p would be better instead of %lx for pointers - but does PR_LOG() support that ? */
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG,
         ("nsXPrintContext::SetupWindow: mDepth=%d, mScreenNumber=%d, colormap=%lx, mDrawable=%lx\n",
         (int)mDepth, (int)mScreenNumber, (long)xattributes.colormap, (long)mDrawable));
         
  return NS_OK;
}


NS_IMETHODIMP
nsXPrintContext::SetupPrintContext(nsIDeviceContextSpecXp *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::SetupPrintContext()\n"));
  
  int    printSize;
  float  top, bottom, left, right;
  char  *buf;

  // Get the Attributes
  aSpec->GetToPrinter(mIsAPrinter);
  aSpec->GetGrayscale(mIsGrayscale);
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
    
#ifdef XPRINT_DEBUG_SOMETIMES_USEFULL
  dumpXpAttributes(mPDisplay, mPContext);
#endif /* DEBUG */

  // Set the Document Attributes
  // XpSetAttributes(mPDisplay,mPContext, XPDocAttr,(char *)"*content-orientation: landscape",XPAttrMerge);
  // or
  // XpuSetContentOrientation(mPDisplay,mPContext, XPDocAttr, "landscape");
  
  /* set printer context
   * WARNING: after this point it is no longer allows to change job attributes
   * only after the XpSetContext() call the alllication is allowed to make 
   * assumptions about available fonts - otherwise you may get the wrong 
   * ones !!
   */
  XPU_TRACE(XpSetContext(mPDisplay, mPContext));

  /* get default printer resolution. May fail if Xprt is misconfigured.
   * ToDo: Report error to user (dialog)
   */
  if( XpuGetResolution(mPDisplay, mPContext, &mPrintResolution) == False )
    return NS_ERROR_FAILURE;

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("print resolution %ld\n", (long)mPrintResolution));
  
  /* We want to get events when Xp(Start|End)(Job|Page) requests are processed... */  
  XpSelectInput(mPDisplay, mPContext, XPPrintMask);  

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

#ifdef HACK_PRINTONSCREEN        
  /* HACK: sleep 15secs if we're displaying to a "display" server
   * see nsXPrintContext::Init and XprintOnScreen macro above - this is only used if 
   * we are printing to a normal Xserver and not to a Xprint server (e.g. in rare 
   * rare cases...)
   */
  if( XpuCheckExtension(mPDisplay) != 1 )
  {
    sleep(15);
  }
#endif /* HACK_PRINTONSCREEN */
  
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
  /* HACK: sleep 15secs if we're displaying to a "display" server
   * see nsXPrintContext::Init and XprintOnScreen macro above - this is only used if 
   * we are printing to a normal Xserver and not to a Xprint server (e.g. in rare 
   * rare cases...)
   */
  if( XpuCheckExtension(mPDisplay) != 1 )
  {
    sleep(15);
  }
#endif /* HACK_PRINTONSCREEN */
    
  return NS_OK;
}

static
XImage *
GetScaledXImage(XImage *img,
                double factorX, double factorY,
                unsigned short aSWidth,  unsigned short aSHeight,
                unsigned short newWidth, unsigned short newHeight)
{
  XImage         *newImg;
  unsigned short  dx, dy, sx, sy;
  
  if ((newImg = (XImage *)malloc(sizeof(XImage))) == nsnull)
    return nsnull;

  /* this is _evil_ code... I don't know how to make it better... ;-(( */
  memset(newImg, 0, sizeof(XImage));
  newImg->width            = newWidth;
  newImg->height           = newHeight;
  newImg->format           = img->format;
  newImg->byte_order       = img->byte_order;
  newImg->bitmap_unit      = img->bitmap_unit;
  newImg->bitmap_bit_order = img->bitmap_bit_order;
  newImg->bitmap_pad       = img->bitmap_pad;
  newImg->depth            = img->depth;
  newImg->bits_per_pixel   = img->bits_per_pixel;
  newImg->xoffset          = 0;
  newImg->depth            = img->depth;
  newImg->data             = nsnull;
  newImg->bytes_per_line   = 0; /* XInitImage() will calculate the correct value */
  if(!XInitImage(newImg))
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("GetScaledXImage: XInitImage() failure\n"));
    XDestroyImage(newImg);
    return nsnull;
  }

  newImg->data = (char *)malloc((newHeight * newImg->bytes_per_line)+16); 

  if (!newImg->data) {
    XDestroyImage(newImg);
    return nsnull;
  }
  
  for(dx = 0 ; dx < newWidth ; dx++)
  {
    sx = dx * factorX;
    for(dy = 0 ; dy < newHeight ; dy++) 
    {
      sy = dy * factorY;
#ifdef XPRINT_USE_SLOW_BUT_EASY_CODE      
      XPutPixel(newImg, dx, dy, XGetPixel(img, sx, sy));
#else
      /* quick&dirty - but this is still legal because XInitImage() has 
       * initalized the function pointers
       * This shortcut avoids tons of test in XGetPixel()/XPutPixel() 
       * which are not neccesary here...
       */
      (*newImg->f.put_pixel)(newImg, dx, dy, (*img->f.get_pixel)(img, sx, sy));  
#endif /* XPRINT_USE_SLOW_BUT_EASY_CODE */
    }
  }
  
  return newImg;
}


NS_IMETHODIMP
nsXPrintContext::DrawImage(xGC *xgc, nsIImage *aImage,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::DrawImage(%d/%d/%d/%d - %d/%d/%d/%d)\n", 
          (int)aSX, (int)aSY, (int)aSWidth, (int)aSHeight,
          (int)aDX, (int)aDY, (int)aDWidth, (int)aDHeight));
  
  double   scaler;
  int      prev_res = 0,
           dummy;
  long     imageResolution;
  PRInt32  aDWidth_scaled;
  PRInt32  aDHeight_scaled;
  nsresult rv = NS_OK;
  
  PRInt32 aSrcWidth  = aImage->GetWidth();
  PRInt32 aSrcHeight = aImage->GetHeight();
  
  if( (aSrcWidth == 0) || (aSrcHeight == 0) ||
      (aSWidth == 0)   || (aSHeight == 0) ||
      (aDWidth == 0)   || (aDHeight == 0) )
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG,
          ("nsXPrintContext::DrawImage(): Image with zero source||dest width||height supressed\n"));
    return NS_OK;
  }

  float pixelscaler = 1.0;
  mContext->GetCanonicalPixelScale(pixelscaler);
  scaler = 1.0 / pixelscaler;

  imageResolution = (double)mPrintResolution * scaler;
  aDWidth_scaled  = (double)aDWidth  * scaler;
  aDHeight_scaled = (double)aDHeight * scaler;
  
  /* set image resolution */
  if( XpSetImageResolution(mPDisplay, mPContext, imageResolution, &prev_res) )
  {
    /* draw image, Xprt will do the main scaling part... */
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
           ("Xp scaling res=%d, aSWidth=%d, aSHeight=%d, aDWidth_scaled=%d, aDHeight_scaled=%d\n", 
            (int)imageResolution, (int)aSWidth, (int)aSHeight, (int)aDWidth_scaled, (int)aDHeight_scaled));
    
    if( (aSX != 0) || (aSY != 0) || (aSWidth != aDWidth_scaled) || (aSHeight != aDHeight_scaled) )
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("using DrawImageBitsScaled()\n"));
      rv = DrawImageBitsScaled(xgc, aImage, aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth_scaled, aDHeight_scaled);
    }
    else
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("using DrawImage() [shortcut]\n"));
      rv = DrawImage(xgc, aImage, aDX, aDY, aDWidth_scaled, aDHeight_scaled);
    }
    
    /* reset image resolution to previous resolution */
    (void)XpSetImageResolution(mPDisplay, mPContext, prev_res, &dummy);
  }
  else /* fallback */
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("BAD BAD local scaling... ;-((\n"));
    /* reset image resolution to previous value to be sure... */
    (void)XpSetImageResolution(mPDisplay, mPContext, prev_res, &dummy);
    
    /* scale image on our side (bad) */
    rv = DrawImageBitsScaled(xgc, aImage, aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth, aDHeight);
  }
  
  return rv;
}  

// use DeviceContextImpl :: GetCanonicalPixelScale(float &aScale) 
// to get the pixel scale of the device context
nsresult
nsXPrintContext::DrawImageBitsScaled(xGC *xgc, nsIImage *aImage,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::DrawImageBitsScaled(%d/%d/%d/%d - %d/%d/%d/%d)\n", 
          (int)aSX, (int)aSY, (int)aSWidth, (int)aSHeight,
          (int)aDX, (int)aDY, (int)aDWidth, (int)aDHeight));

  nsresult rv = NS_OK;
  
  PRUint8 *image_bits    = aImage->GetBits();
  PRInt32  row_bytes     = aImage->GetLineStride();
  PRUint8 *alphaBits     = aImage->GetAlphaBits();
  PRInt32  alphaRowBytes = aImage->GetAlphaLineStride();
  
  XImage *srcImg        = nsnull;
  XImage *srcAlphaImg   = nsnull;
  XImage *subImg        = nsnull;
  XImage *subAlphaImg   = nsnull;
  XImage *dstImg        = nsnull;
  XImage *dstAlphaImg   = nsnull;
    
  PRInt32 aSrcWidth  = aImage->GetWidth();
  PRInt32 aSrcHeight = aImage->GetHeight();

  double factorX = (double)aSrcWidth  / (double)aDWidth;
  double factorY = (double)aSrcHeight / (double)aDHeight;
  
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("DrawImageBitsScaled: factorX=%f, factorY=%f\n", factorX, factorY));
  
  srcImg = (XImage *)malloc(sizeof(XImage));
  if( srcImg == nsnull )
    return NS_ERROR_OUT_OF_MEMORY;  // BUG: We should free all other stuff at this point, too
  memset(srcImg, 0, sizeof(XImage));
  srcImg->width            = aSrcWidth;
  srcImg->height           = aSrcHeight;
  srcImg->format           = ZPixmap;
  srcImg->byte_order       = MSBFirst;
  srcImg->bitmap_unit      = 32;
  srcImg->bitmap_bit_order = MSBFirst;
  srcImg->red_mask         = srcImg->green_mask = srcImg->blue_mask = 0;
  srcImg->bits_per_pixel   = 24; /* R8G8B8 packed. Thanks to "tor" for that hint... */
  srcImg->xoffset          = 0;
  srcImg->bitmap_pad       = 32;
  srcImg->depth            = 24;
  srcImg->data             = (char *)image_bits;
  srcImg->bytes_per_line   = row_bytes;

  if( !XInitImage(srcImg) )
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XInitImage failure... ;-(\n"));

  if( (aSX != 0) || (aSY != 0) || (aSrcWidth != aSWidth) || (aSrcHeight != aSHeight))
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
           ("  XSubImage(srcImg, aSX=%d, aSY=%d, aSWidth=%d, aSHeight=%d);\n", 
            (int)aSX, (int)aSY, (int)aSWidth, (int)aSHeight));                          
    subImg = XSubImage(srcImg, aSX, aSY, aSWidth, aSHeight);
    
    srcImg->data = nsnull; /* we do not own this piece of memory */
    XDestroyImage(srcImg);
    srcImg = subImg;       
  }

  dstImg = GetScaledXImage(srcImg, factorX, factorY,
                           aSWidth, aSHeight,
                           aDWidth, aDHeight);

  if (!dstImg)
  {
    return PR_FALSE;
  }
      
  if(alphaBits != nsnull)
  {
    srcAlphaImg = (XImage *)malloc(sizeof(XImage));
    if( srcAlphaImg == nsnull )
      return NS_ERROR_OUT_OF_MEMORY; // BUG: We should free all other stuff at this point, too
    memset(srcAlphaImg, 0, sizeof(XImage));
    srcAlphaImg->width            = aSrcWidth;
    srcAlphaImg->height           = aSrcHeight;
    srcAlphaImg->format           = XYPixmap;
    srcAlphaImg->byte_order       = MSBFirst;
    srcAlphaImg->bitmap_unit      = 32;
    srcAlphaImg->bitmap_bit_order = MSBFirst;
    srcAlphaImg->red_mask         = srcImg->green_mask = srcImg->blue_mask = 0;
    srcAlphaImg->bits_per_pixel   = 1;
    srcAlphaImg->xoffset          = 0;
    srcAlphaImg->bitmap_pad       = 32;
    srcAlphaImg->depth            = 1;
    srcAlphaImg->data             = (char *)alphaBits;
    srcAlphaImg->bytes_per_line   = alphaRowBytes;

    if( !XInitImage(srcAlphaImg) )
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XInitImage failure... ;-(\n"));

    if( (aSX != 0) || (aSY != 0) || (aSrcWidth != aSWidth) || (aSrcHeight != aSHeight) )
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
             ("  XSubImage(srcAlphaImg, aSX=%d, aSY=%d, aSWidth=%d, aSHeight=%d);\n", 
              (int)aSX, (int)aSY, (int)aSWidth, (int)aSHeight));
      subAlphaImg = XSubImage(srcAlphaImg, aSX, aSY, aSWidth, aSHeight);                            
      if (!subAlphaImg)
        return PR_FALSE;
        
       srcAlphaImg->data = nsnull; /* we do not own this piece of memory */
       XDestroyImage(srcAlphaImg);
       srcAlphaImg = subAlphaImg;        
    }   

    dstAlphaImg = GetScaledXImage(srcAlphaImg, factorX, factorY,
                                  aSWidth, aSHeight,
                                  aDWidth, aDHeight);

    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("rendering ALPHA image !!\n"));
    rv = DrawImageBits(xgc,
                       (PRUint8 *)dstAlphaImg->data, dstAlphaImg->bytes_per_line, 
                       (PRUint8 *)dstImg->data,      dstImg->bytes_per_line, 
                       aDX, aDY, aDWidth, aDHeight);

    XDestroyImage(dstAlphaImg);
  }
  else
  {
    /* shortcut */
    xlib_draw_rgb_image(mDrawable,
                        *xgc,
                        aDX, aDY, aDWidth, aDHeight,
                        NS_XPRINT_RGB_DITHER,
                        (unsigned char *)dstImg->data, dstImg->bytes_per_line);
    rv = NS_OK;                        
  }

  if( srcImg != nsnull )
  {
    if( subImg == nsnull )
    {
      srcImg->data = nsnull; /* we do not own this piece of memory */
      XDestroyImage(srcImg);
    }
    else
    {
      XDestroyImage(subImg);
    }  
  }      

  if( srcAlphaImg != nsnull )
  {
    if( subAlphaImg == nsnull )
    {
      srcAlphaImg->data = nsnull; /* we do not own this piece of memory */
      XDestroyImage(srcAlphaImg);
    }
    else
    {
      XDestroyImage(subAlphaImg);
    }  
  }  
  

  XDestroyImage(dstImg);
  
  return rv;
}


NS_IMETHODIMP
nsXPrintContext::DrawImage(xGC *xgc, nsIImage *aImage,
                           PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::DrawImage(%d/%d/%d/%d)\n",
         (int)aX, (int)aY, (int)aWidth, (int)aHeight));
         
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
    aWidth  = width;
    aHeight = height;
  }

  if( (aWidth == 0) || (aHeight == 0) )
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG,
           ("nsXPrintContext::DrawImage(): Image with zero width||height supressed\n"));
    return NS_OK;
  }
      
  return DrawImageBits(xgc, alphaBits, alphaRowBytes, 
                       image_bits, row_bytes, 
                       aX, aY, aWidth, aHeight);
}

                           
// Draw the bitmap, this draw just has destination coordinates
nsresult
nsXPrintContext::DrawImageBits(xGC *xgc,
                               PRUint8 *alphaBits, PRInt32  alphaRowBytes,
                               PRUint8 *image_bits, PRInt32  row_bytes,
                               PRInt32 aX, PRInt32 aY,
                               PRInt32 aWidth, PRInt32 aHeight)
{ 
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::DrawImageBits(%d/%d/%d/%d)\n",
         (int)aX, (int)aY, (int)aWidth, (int)aHeight));

  Pixmap alpha_pixmap  = None;
  GC     image_gc;

/* this does not work yet... */
#ifdef XPRINT_NOT_NOW
  // Create gc clip-mask on demand
  if( alphaBits != nsnull )
  {
    XImage    *x_image = nsnull;
    GC         gc;
    XGCValues  gcv;
    
    alpha_pixmap = XCreatePixmap(mPDisplay, 
                                 mDrawable,
                                 aWidth, aHeight, 1); /* ToDo: Check for error */
  
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
     * because the image library ensures that the bytes in
     * bitmask data are arranged left to right on the screen,
     * low to high address in memory. */
    x_image->byte_order = MSBFirst;

    // Write into the pixemap that is underneath gdk's alpha_pixmap
    // the image we just created.
    memset(&gcv, 0, sizeof(XGCValues)); /* this may be unneccesary */
    XGetGCValues(mPDisplay, *xgc, GCForeground|GCBackground, &gcv);
    gcv.function = GXcopy;
    gc = XCreateGC(mPDisplay, alpha_pixmap, GCForeground|GCBackground|GCFunction, &gcv);

    XPutImage(mPDisplay, alpha_pixmap, gc, x_image, 0, 0, 0, 0,
              aWidth, aHeight);
    XFreeGC(mPDisplay, gc);

    // Now we are done with the temporary image
    x_image->data = nsnull; /* Don't free the IL_Pixmap's bits. */
    XDestroyImage(x_image);
  }
#endif /* XPRINT_NOT_NOW */  
  
  if( alpha_pixmap != None )
  {
    /* create copy of GC before start to playing with it... */
    XGCValues gcv;  
    memset(&gcv, 0, sizeof(XGCValues)); /* this may be unneccesary */
    XGetGCValues(mPDisplay, *xgc, GCForeground|GCBackground, &gcv);
    gcv.function      = GXcopy;
    gcv.clip_mask     = alpha_pixmap;
    gcv.clip_x_origin = aX;
    gcv.clip_y_origin = aY;

    image_gc = XCreateGC(mPDisplay, mDrawable, 
                         (GCForeground|GCBackground|GCFunction|
                          GCClipXOrigin|GCClipYOrigin|GCClipMask),
                         &gcv);
  }
  else
  {
    /* this assumes that xlib_draw_rgb_image()/xlib_draw_gray_image()
     * does not change the GC... */
    image_gc = *xgc;
  }

  if( mIsGrayscale )
  {
    xlib_draw_gray_image(mDrawable,
                         image_gc,
                         aX, aY, aWidth, aHeight,
                         NS_XPRINT_RGB_DITHER,
                         image_bits, row_bytes);  
  }
  else
  {
    xlib_draw_rgb_image(mDrawable,
                        image_gc,
                        aX, aY, aWidth, aHeight,
                        NS_XPRINT_RGB_DITHER,
                        image_bits, row_bytes);
  }
  
  if( alpha_pixmap != None ) 
  {   
    XFreeGC(mPDisplay, image_gc);
    XFreePixmap(mPDisplay, alpha_pixmap);
  }
    
  return NS_OK;
}

NS_IMETHODIMP nsXPrintContext::GetPrintResolution(int &aPrintResolution) const
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::GetPrintResolution() res=%d\n",
         (int)mPrintResolution));
  
  aPrintResolution = mPrintResolution;
  return NS_OK;
}

