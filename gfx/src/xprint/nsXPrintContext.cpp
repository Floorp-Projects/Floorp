/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Note: Sun Workshop 6 may have problems when mixing <string,h> 
 * and X includes. This was fixed by recent Xsun patches which 
 * are part of the "recommended patches" for Solaris. 
 * See bug 88703...
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> /* for strerror & memset */
#include "imgScaler.h"
#include "nsXPrintContext.h"
#include "nsDeviceContextXP.h"
#include "xprintutil.h"
#include "prenv.h" /* for PR_GetEnv */

/* misc defines */
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
  NS_INIT_REFCNT();
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::nsXPrintContext()\n"));
  
  mXlibRgbHandle = (XlibRgbHandle *)nsnull;
  mPDisplay    = (Display *)nsnull;
  mPContext    = (XPContext)None;
  mScreen      = (Screen *)nsnull;
  mVisual      = (Visual *)nsnull;
  mDrawable    = (Drawable)None;
  mDepth       = 0;
  mIsGrayscale = PR_FALSE; /* default is color output */
  mIsAPrinter  = PR_TRUE;  /* default destination is printer */
  mPrintFile   = nsnull;
  mXpuPrintToFileHandle = nsnull;
  mPrintResolution = 0L;
  mContext     = nsnull;
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
    if (mGC)
    {
      mGC->Release();
      mGC = nsnull;
    }
    
    XPU_TRACE(XpDestroyContext(mPDisplay, mPContext));

    // Cleanup things allocated along the way
    xxlib_rgb_destroy_handle(mXlibRgbHandle);
    mXlibRgbHandle = nsnull;
    
    XPU_TRACE(XCloseDisplay(mPDisplay));
    
    mPContext = nsnull;
    mPDisplay = nsnull;
  }
  
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::~nsXPrintContext() done.\n"));
}

NS_IMPL_ISUPPORTS1(nsXPrintContext, nsIDrawingSurfaceXlib)

NS_IMETHODIMP 
nsXPrintContext::Init(nsDeviceContextXp *dc, nsIDeviceContextSpecXp *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::Init()\n"));
  nsresult rv = NS_ERROR_FAILURE;

  int   prefDepth = 8;  /* 24 or 8 for PS DDX, 24, 8, 1 for PCL DDX... 
                         * I wish current Xprt would have a 1bit/8bit StaticGray 
                         * visual for the PS DDX... ;-( 
                         */
/* I can't get any other visual than the 8bit peudocolor one working... BAD...
 * This env var allows others to test this without hacking their own binaries...
 */
#ifdef XPRINT_MAKE_24BIT_VISUAL_AVAILABLE_FOR_TESTING  
  if( PR_GetEnv("MOZILLA_XPRINT_EXPERIMENTAL_USE_24BIT_VISUAL") != nsnull )
  {
    prefDepth = 24;
  }
#endif /* XPRINT_MAKE_24BIT_VISUAL_AVAILABLE_FOR_TESTING */  
  
  unsigned short width, height;
  XRectangle rect;

  if (NS_FAILED(XPU_TRACE(rv = SetupPrintContext(aSpec))))
    return rv;
  
  mScreen = XpGetScreenOfContext(mPDisplay, mPContext);
  mScreenNumber = XScreenNumberOfScreen(mScreen);
  mXlibRgbHandle = xxlib_rgb_create_handle_with_depth("xprint", mPDisplay, mScreen, prefDepth);
  if (!mXlibRgbHandle)
    return NS_ERROR_GFX_PRINTER_INVALID_ATTRIBUTE;
  xxlib_disallow_image_tiling(mXlibRgbHandle, TRUE);

  XpGetPageDimensions(mPDisplay, mPContext, &width, &height, &rect);
  rv = SetupWindow(rect.x, rect.y, rect.width, rect.height);
  if (NS_FAILED(rv))
    return rv;

  XMapWindow(mPDisplay, mDrawable); 
  
  mContext = dc;
    
  /* Set error handler and sync
   * ToDo: unset handler after all is done - what about handler "stacking" ?
   */
  (void)XSetErrorHandler(xerror_handler);

  /* only run unbuffered X11 connection on demand (for debugging/testing) */
  if( PR_GetEnv("MOZILLA_XPRINT_EXPERIMENTAL_SYNCHRONIZE") != nsnull )
  {
    XSynchronize(mPDisplay, True);
  }  
  
  return NS_OK;
}

nsresult
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

  visual_info = xxlib_rgb_get_visual_info(mXlibRgbHandle);
  mVisual     = xxlib_rgb_get_visual(mXlibRgbHandle);
  mDepth      = xxlib_rgb_get_depth(mXlibRgbHandle);

  background = XWhitePixel(mPDisplay, mScreenNumber);
  foreground = XBlackPixel(mPDisplay, mScreenNumber);  
  parent_win = XRootWindow(mPDisplay, mScreenNumber);                                         
                                             
  xattributes.background_pixel = background;
  xattributes.border_pixel     = foreground;
  xattributes.colormap         = xxlib_rgb_get_cmap(mXlibRgbHandle);
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


nsresult
nsXPrintContext::SetupPrintContext(nsIDeviceContextSpecXp *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::SetupPrintContext()\n"));
  
  int    printSize;
  float  top, bottom, left, right;
  int    landscape;
  char  *buf;

  // Get the Attributes
  aSpec->GetToPrinter(mIsAPrinter);
  aSpec->GetGrayscale(mIsGrayscale);
  aSpec->GetSize(printSize);
  aSpec->GetTopMargin(top);
  aSpec->GetBottomMargin(bottom);
  aSpec->GetLeftMargin(left);
  aSpec->GetRightMargin(right);
  aSpec->GetLandscape(landscape);
  
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
      return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
  }

  /* workaround for crash in XCloseDisplay() on Solaris 2.7 Xserver - when 
   * shared memory transport is used XCloseDisplay() tries to free() the 
   * shared memory segment - causing heap corruption and/or SEGV.
   */
  putenv((char *)"XSUNTRANSPORT=xxx");
     
  /* get printer, either by "name" (foobar) or "name@display" (foobar@gaja:5)
   * ToDo: report error to user (dialog)
   */
  if( XpuGetPrinter(buf, &mPDisplay, &mPContext) != 1 )
    return NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;
    
#ifdef XPRINT_DEBUG_SOMETIMES_USEFULL
  dumpXpAttributes(mPDisplay, mPContext);
#endif /* DEBUG */

  /* which orientation ? */
  switch (landscape)
  {
    case 1 /* NS_LANDSCAPE */:
      if (XpuSetContentOrientation(mPDisplay,mPContext, XPDocAttr, "landscape") != 1)
      {
        NS_WARNING("orientation 'landscape' not supported on this printer");
        return NS_ERROR_FAILURE;
      }
      break;
    case 0 /* NS_PORTRAIT */:
      if (XpuSetContentOrientation(mPDisplay,mPContext, XPDocAttr, "portrait") != 1)
      {
        NS_WARNING("orientation 'portrait' not supported on this printer");
        return NS_ERROR_FAILURE;
      }
      break;
    default:  
      NS_WARNING("unsupported orientation");
      return NS_ERROR_FAILURE;
  }    
    
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
    return NS_ERROR_GFX_PRINTER_INVALID_ATTRIBUTE;

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
char *MyConvertUCS2ToLocalEncoding( PRUnichar *str )
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
    job_title = s = MyConvertUCS2ToLocalEncoding(aTitle); 
  else
    job_title = (char *)"Mozilla document without title";  

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

      return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
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
    
  return NS_OK;
}

static
PRUint8 *ComposeAlphaImage(
               PRUint8 *alphaBits, PRInt32  alphaRowBytes, PRUint8 alphaDepth,
               PRUint8 *image_bits, PRInt32  row_bytes,
               PRInt32 aWidth, PRInt32 aHeight)
{
  PRUint8 *composed_bits; /* composed image */
  if (!(composed_bits = (PRUint8 *)PR_Malloc(aHeight * row_bytes)))
    return nsnull;

/* unfortunately this code does not work for some strange reason ... */
#ifdef XPRINT_NOT_NOW
  XGCValues gcv;
  memset(&gcv, 0, sizeof(XGCValues)); /* this may be unneccesary */
  XGetGCValues(mPDisplay, *xgc, GCForeground, &gcv);
  
  /* should be replaced by xxlibrgb_query_color() */
  XColor color;
  color.pixel = gcv.foreground;
  XQueryColor(mPDisplay, xxlib_rgb_get_cmap(mXlibRgbHandle), &color);
  
  unsigned long background = NS_RGB(color.red>>8, color.green>>8, color.blue>>8);
#else
  unsigned long background = NS_RGB(0xFF,0xFF,0xFF); /* white! */
#endif /* XPRINT_NOT_NOW */
  long x, y;
    
  switch(alphaDepth)
  {
    case 1:
    {
#define NS_GET_BIT(rowptr, x) (rowptr[(x)>>3] &  (1<<(7-(x)&0x7)))
        unsigned short r = NS_GET_R(background),
                       g = NS_GET_R(background),
                       b = NS_GET_R(background);
                     
        for(y = 0 ; y < aHeight ; y++)
        {
          unsigned char *imageRow  = image_bits    + y * row_bytes;
          unsigned char *destRow   = composed_bits + y * row_bytes;
          unsigned char *alphaRow  = alphaBits     + y * alphaRowBytes;
          for(x = 0 ; x < aWidth ; x++)
          {
            if(NS_GET_BIT(alphaRow, x))
            {
              /* copy src color */
              destRow[3*x  ] = imageRow[3*x  ];
              destRow[3*x+1] = imageRow[3*x+1];
              destRow[3*x+2] = imageRow[3*x+2];
            }
            else
            {
              /* copy background color (e.g. pixel is "transparent") */
              destRow[3*x  ] = r;
              destRow[3*x+1] = g;
              destRow[3*x+2] = b;
            }
          }
        }        
    }  
        break;
    case 8:
    {
        unsigned short r = NS_GET_R(background),
                       g = NS_GET_R(background),
                       b = NS_GET_R(background);
        
        for(y = 0 ; y < aHeight ; y++)
        {
          unsigned char *imageRow  = image_bits    + y * row_bytes;
          unsigned char *destRow   = composed_bits + y * row_bytes;
          unsigned char *alphaRow  = alphaBits     + y * alphaRowBytes;
          for(x = 0 ; x < aWidth ; x++)
          {
            unsigned short alpha = alphaRow[x];
            MOZ_BLEND(destRow[3*x  ], r, imageRow[3*x  ], alpha);
            MOZ_BLEND(destRow[3*x+1], g, imageRow[3*x+1], alpha);
            MOZ_BLEND(destRow[3*x+2], b, imageRow[3*x+2], alpha);
          }
        }
    }  
        break;
    default:
    {
        NS_WARNING("alpha depth x not supported");
        PR_Free(composed_bits);
        return nsnull;
    }  
        break;
  }
  
  return composed_bits;      
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
    NS_ASSERTION((aSrcWidth != 0) && (aSrcHeight != 0) &&
      (aSWidth != 0)   && (aSHeight != 0) ||
      (aDWidth != 0)   && (aDHeight != 0), 
      "nsXPrintContext::DrawImage(): Image with zero source||dest width||height supressed\n");
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
  
  if (aDWidth==0 || aDHeight==0)
  {
    NS_ASSERTION((aDWidth==0 || aDHeight==0), 
                 "nsXPrintContext::DrawImageBitsScaled(): Image with zero dest width||height supressed\n");
    return NS_OK;
  }
  
  PRUint8 *image_bits    = aImage->GetBits();
  PRInt32  row_bytes     = aImage->GetLineStride();
  PRUint8  imageDepth    = 24; /* R8G8B8 packed. Thanks to "tor" for that hint... */
  PRUint8 *alphaBits     = aImage->GetAlphaBits();
  PRInt32  alphaRowBytes = aImage->GetAlphaLineStride();
  int      alphaDepth    = aImage->GetAlphaDepth();
  PRInt32  aSrcWidth     = aImage->GetWidth();
  PRInt32  aSrcHeight    = aImage->GetHeight();
  PRUint8 *composed_bits = nsnull;

  // Use client-side alpha image composing - plain X11 can only do 1bit alpha 
  // stuff - this method adds 8bit alpha support, too...
  if( alphaBits != nsnull )
  {
    composed_bits = ComposeAlphaImage(alphaBits, alphaRowBytes, alphaDepth,
                                      image_bits, row_bytes,
                                      aSrcWidth, aSrcHeight);
    if (!composed_bits)
      return NS_ERROR_FAILURE;
    image_bits = composed_bits;
    alphaBits = nsnull; /* ComposeAlphaImage() handled the alpha channel. 
                         * Once we enable XPRINT_SERVER_SIDE_ALPHA_COMPOSING 
                         * we have to send the alpha data to the server
                         * instead of processing them locally (e.g. on 
                         * the client-side).
                         */
  }

#define ROUNDUP(nbytes, pad) ((((nbytes) + ((pad)-1)) / (pad)) * ((pad)>>3))

  PRInt32  srcimg_bytes_per_line = row_bytes;
  PRInt32  dstimg_bytes_per_line = ROUNDUP((imageDepth * aDWidth), 32);
  PRUint8 *srcimg_data           = image_bits;
  PRUint8 *dstimg_data           = (PRUint8 *)PR_Malloc((aDHeight+1) * dstimg_bytes_per_line); 
  if (!dstimg_data)
    return NS_ERROR_FAILURE;

  RectStretch(aSX, aSY, aSX+aSWidth-1, aSY+aSHeight-1,
              0, 0, (aDWidth-1), (aDHeight-1),
              srcimg_data, srcimg_bytes_per_line,
              dstimg_data, dstimg_bytes_per_line,
              imageDepth);

#ifdef XPRINT_SERVER_SIDE_ALPHA_COMPOSING
/* ToDo: scale alpha image */
#endif /* XPRINT_SERVER_SIDE_ALPHA_COMPOSING */
    
  rv = DrawImageBits(xgc, alphaBits, alphaRowBytes, alphaDepth,
                     dstimg_data, dstimg_bytes_per_line, 
                     aDX, aDY, aDWidth, aDHeight);

  if (dstimg_data)   
    PR_Free(dstimg_data);
  if (composed_bits) 
    PR_Free(composed_bits);
  
  return rv;
}


NS_IMETHODIMP
nsXPrintContext::DrawImage(xGC *xgc, nsIImage *aImage,
                           PRInt32 aX, PRInt32 aY,
                           PRInt32 dummy1, PRInt32 dummy2)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::DrawImage(%d/%d/%d(=dummy)/%d(=dummy))\n",
         (int)aX, (int)aY, (int)dummy1, (int)dummy2));

  nsresult rv;
  PRInt32  width         = aImage->GetWidth();
  PRInt32  height        = aImage->GetHeight();
  PRUint8 *alphaBits     = aImage->GetAlphaBits();
  PRInt32  alphaRowBytes = aImage->GetAlphaLineStride();
  PRInt32  alphaDepth    = aImage->GetAlphaDepth();
  PRUint8 *image_bits    = aImage->GetBits();
  PRUint8 *composed_bits = nsnull;
  PRInt32  row_bytes     = aImage->GetLineStride();
  
  // Use client-side alpha image composing - plain X11 can only do 1bit alpha
  // stuff - this method adds 8bit alpha support, too...
  if( alphaBits != nsnull )
  {
    composed_bits = ComposeAlphaImage(alphaBits, alphaRowBytes, alphaDepth,
                                      image_bits, row_bytes,
                                      width, height);
    if (!composed_bits)
      return NS_ERROR_FAILURE;
    image_bits = composed_bits;
    alphaBits = nsnull;
  }
               
  rv = DrawImageBits(xgc, alphaBits, alphaRowBytes, alphaDepth,
                     image_bits, row_bytes,
                     aX, aY, width, height);

  if (composed_bits)
    PR_Free(composed_bits);
  return rv;                     
}

                          
// Draw the bitmap, this draw just has destination coordinates
nsresult
nsXPrintContext::DrawImageBits(xGC *xgc,
                               PRUint8 *alphaBits, PRInt32  alphaRowBytes, PRUint8 alphaDepth,
                               PRUint8 *image_bits, PRInt32  row_bytes,
                               PRInt32 aX, PRInt32 aY,
                               PRInt32 aWidth, PRInt32 aHeight)
{ 
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::DrawImageBits(%d/%d/%d/%d)\n",
         (int)aX, (int)aY, (int)aWidth, (int)aHeight));

  Pixmap alpha_pixmap  = None;
  GC     image_gc;

  if( (aWidth == 0) || (aHeight == 0) )
  {
    NS_ASSERTION((aWidth != 0) && (aHeight != 0), "Image with zero width||height supressed.");
    return NS_OK;
  }
  
/* server-side alpha image support (1bit) - this does not work yet... */
#ifdef XPRINT_SERVER_SIDE_ALPHA_COMPOSING
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
#endif /* XPRINT_SERVER_SIDE_ALPHA_COMPOSING */  
  
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
    xxlib_draw_gray_image(mXlibRgbHandle,
                          mDrawable,
                          image_gc,
                          aX, aY, aWidth, aHeight,
                          NS_XPRINT_RGB_DITHER,
                          image_bits, row_bytes);  
  }
  else
  {
    xxlib_draw_rgb_image(mXlibRgbHandle,
                         mDrawable,
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

NS_IMETHODIMP nsXPrintContext::GetPrintResolution(int &aPrintResolution)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::GetPrintResolution() res=%d, mPContext=%lx\n",
          (int)mPrintResolution, (long)mPContext));
  
  if(mPContext!=None)
  {
    aPrintResolution = mPrintResolution;
    return NS_OK;
  }
  
  aPrintResolution = 0;
  return NS_ERROR_FAILURE;    
}

