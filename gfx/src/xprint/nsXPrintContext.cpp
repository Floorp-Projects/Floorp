/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@nrubsig.org>
 *   Leon Sha <leon.sha@sun.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1 /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "imgScaler.h"
#include "nsXPrintContext.h"
#include "nsDeviceContextXP.h"
#include "xprintutil.h"
#include "prenv.h" /* for PR_GetEnv */
#include "prprf.h"
#include "plstr.h"
#include "nsPrintfCString.h"

/* NS_XPRINT_RGB_DITHER: Macro to check whether we should dither or not.
 * In theory we only have to look at the visual and depth ("TrueColor" with 
 * enougth bits for the colors or GrayScale/StaticGray with enougth bits for
 * the grayscale shades).
 * In real life some Xprt DDX do not have the GrayScale/StaticGray visuals and
 * we emulate grayscale support with a PseudoColor visual+grayscale palette
 * (that's why we test for |mIsGrayscale| explicitly)...
 */
#define NS_XPRINT_RGB_DITHER \
    (((mDepth >  12 && mVisual->c_class==TrueColor)  || \
      (mDepth >=  7 && mVisual->c_class==GrayScale)  || \
      (mDepth >=  7 && mVisual->c_class==StaticGray) || \
      (mIsGrayscale == PR_TRUE)) ?(XLIB_RGB_DITHER_NONE):(XLIB_RGB_DITHER_MAX))

#ifdef PR_LOGGING 
/* DEBUG: use 
 * % export NSPR_LOG_MODULES=nsXPrintContext:5 
 * to see these debug messages... 
 */
static PRLogModuleInfo *nsXPrintContextLM = PR_NewLogModule("nsXPrintContext");
#endif /* PR_LOGGING */

PR_BEGIN_EXTERN_C
static 
int xerror_handler( Display *display, XErrorEvent *ev )
{
    /* this should _never_ be happen... but if this happens - debug mode or not - scream !!! */
    char errmsg[80];
    XGetErrorText(display, ev->error_code, errmsg, sizeof(errmsg));
    fprintf(stderr, "nsGfxXprintModule: Warning (X Error) -  %s\n", errmsg);
    return 0;
}
PR_END_EXTERN_C

/** ---------------------------------------------------
 *  Default Constructor
 */
nsXPrintContext::nsXPrintContext() :
  mXlibRgbHandle(nsnull),
  mPDisplay(nsnull),
  mScreen(nsnull),
  mVisual(nsnull),
  mDrawable(None),
  mGC(nsnull),
  mDepth(0),
  mPContext(None),
  mJobStarted(PR_FALSE),
  mIsGrayscale(PR_FALSE), /* default is color output */
  mIsAPrinter(PR_TRUE),   /* default destination is printer */
  mPrintFile(nsnull),
  mXpuPrintToFileHandle(nsnull),
  mPrintResolution(0L),
  mContext(nsnull)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::nsXPrintContext()\n"));
}

/** ---------------------------------------------------
 *  Destructor
 */
nsXPrintContext::~nsXPrintContext()
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::~nsXPrintContext()\n"));
 
  if (mPDisplay)
  {
    if (mJobStarted)
    {
      /* Clean-up if noone else did it yet... */
      AbortDocument();
    }

    if (mGC)
    {
      mGC->Release();
      mGC = nsnull;
    }
       
    if (mXlibRgbHandle)
    {
      xxlib_rgb_destroy_handle(mXlibRgbHandle);
      mXlibRgbHandle = nsnull;
    }
    
    XPU_TRACE(XpuClosePrinterDisplay(mPDisplay, mPContext));           
    mPDisplay = nsnull;
    mPContext = None;
  }
  
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::~nsXPrintContext() done.\n"));
}

NS_IMPL_ISUPPORTS1(nsXPrintContext, nsIDrawingSurfaceXlib)


static nsresult
AlertBrokenXprt(Display *pdpy)
{
  /* Check for broken Xprt
   * Xfree86 Xprt is the only known broken version of Xprt (see bug 120211
   * for a list of tested&working Xprt servers) ...
   * XXX: We should look at XVendorRelease(), too - but there is no feedback
   * from XFree86.org when this issue will be fixed... ;-(
   */
  if (!(strstr(XServerVendor(pdpy), "XFree86") /*&& XVendorRelease(pdpy) < 45000000L*/))
    return NS_OK;

  /* Bad version found... log the issue and show the dialog and warn the user... */
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG,
         ("nsXPrintContext::AlertBrokenXprt: vendor: '%s', release=%ld\n",
          XServerVendor(pdpy), (long)XVendorRelease(pdpy)));

  /* Dialog disabled ? */
  if (PR_GetEnv("MOZILLA_XPRINT_DISABLE_BROKEN_XFREE86_WARNING") != nsnull)
    return NS_OK;
   
  return NS_ERROR_GFX_PRINTER_XPRINT_BROKEN_XPRT;
}

NS_IMETHODIMP 
nsXPrintContext::Init(nsDeviceContextXp *dc, nsIDeviceContextSpecXp *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::Init()\n"));
  nsresult rv = NS_ERROR_FAILURE;

  int   prefDepth = 24;  /* 24 or 8 for PS DDX, 24, 8, 1 for PCL DDX... 
                          * I wish current Xprt would have a 1bit/8bit StaticGray 
                          * visual for the PS DDX... ;-( 
                          */
  /* Safeguard for production use (see bug 80562),
   * Make sure we can switch back to the
   * extensively tested 8bit Pseudocolor visual on demand... */
  if( PR_GetEnv("MOZILLA_XPRINT_EXPERIMENTAL_DISABLE_24BIT_VISUAL") != nsnull )
  {
    prefDepth = 8;
  }
  
  unsigned short width, height;
  XRectangle rect;

  if (NS_FAILED(XPU_TRACE(rv = SetupPrintContext(aSpec))))
    return rv;

  mScreen = XpGetScreenOfContext(mPDisplay, mPContext);
  mScreenNumber = XScreenNumberOfScreen(mScreen);

  XlibRgbArgs xargs;
  memset(&xargs, 0, sizeof(xargs));
  xargs.handle_name           = nsnull;
  xargs.disallow_image_tiling = True; /* XlibRGB's "Image tiling"-hack is 
                                       * incompatible to Xprint API */
  
  /* What about B/W-only printers ?! */
  if (mIsGrayscale)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("printing grayscale\n"));
    /* 1st attempt using StaticGray */
    xargs.xtemplate.c_class = StaticGray;
    xargs.xtemplate.depth   = 8;
    xargs.xtemplate_mask    = VisualClassMask|VisualDepthMask;
    mXlibRgbHandle = xxlib_rgb_create_handle(mPDisplay, mScreen, &xargs);
    if (!mXlibRgbHandle)
    { 
      /* 2nd attempt using GrayScale */
      xargs.xtemplate.c_class = GrayScale;
      xargs.xtemplate.depth   = 8;
      xargs.xtemplate_mask    = VisualClassMask|VisualDepthMask;
      mXlibRgbHandle = xxlib_rgb_create_handle(mPDisplay, mScreen, &xargs);             
      if (!mXlibRgbHandle)
      {
        /* Last attempt: Emulate StaticGray via Pseudocolor colormap */
        xargs.xtemplate_mask  = 0L;
        xargs.xtemplate.depth = 0;
        xargs.pseudogray      = True;
        mXlibRgbHandle = xxlib_rgb_create_handle(mPDisplay, mScreen, &xargs);
      }

      if (!mXlibRgbHandle)
      {
        PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("trying black/white\n"));

        /* Last attempt using StaticGray 1bit (b/w printer) */
        xargs.xtemplate.c_class = StaticGray;
        xargs.xtemplate.depth   = 1;
        xargs.xtemplate_mask    = VisualClassMask|VisualDepthMask;
        xargs.pseudogray        = False;
        mXlibRgbHandle = xxlib_rgb_create_handle(mPDisplay, mScreen, &xargs);
      }
    }
  }
  else
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("printing color\n"));
    if (prefDepth > 12)
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("trying TrueColor %d bit\n", prefDepth));
      xargs.xtemplate.depth   = prefDepth;
      xargs.xtemplate.c_class = TrueColor;
      xargs.xtemplate_mask    = VisualDepthMask|VisualClassMask;
      mXlibRgbHandle = xxlib_rgb_create_handle(mPDisplay, mScreen, &xargs);
    }  

    if (!mXlibRgbHandle)
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("trying PseudoColor 8 bit\n"));
      xargs.xtemplate.depth   = 8;
      xargs.xtemplate.c_class = PseudoColor;
      xargs.xtemplate_mask    = VisualDepthMask|VisualClassMask;
      mXlibRgbHandle = xxlib_rgb_create_handle(mPDisplay, mScreen, &xargs);
    }  
  }
  
  /* No XlibRgb handle ? Either we do not have a matching visual or no memory... */
  if (!mXlibRgbHandle)
    return NS_ERROR_GFX_PRINTER_COLORSPACE_NOT_SUPPORTED;

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

  background = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, NS_TO_XXLIB_RGB(NS_RGB(0xFF, 0xFF, 0xFF))); /* white */
  foreground = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, NS_TO_XXLIB_RGB(NS_RGB(0x00, 0x00, 0x00))); /* black */
  parent_win = XRootWindow(mPDisplay, mScreenNumber);                                         
                                             
  xattributes.background_pixel = background;
  xattributes.border_pixel     = foreground;
  xattributes.colormap         = xxlib_rgb_get_cmap(mXlibRgbHandle);
  xattributes_mask             = CWBorderPixel | CWBackPixel;
  if( xattributes.colormap != None )
  {
    xattributes_mask |= CWColormap;

    /* XXX: Why do we need this ? See bug 80562 ("Xprint does not support any
     * other visuals than Xprt's default one..").*/
    if (mDepth > 12)
    {
      XInstallColormap(mPDisplay, xattributes.colormap);
    }  
  }

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


nsresult nsXPrintContext::SetMediumSize(const char *aPaperName)
{
  nsresult                rv = NS_ERROR_GFX_PRINTER_PAPER_SIZE_NOT_SUPPORTED;
  XpuMediumSourceSizeList mlist;
  int                     mlist_count;
  int                     i;
  char                   *paper_name,
                         *alloc_paper_name; /* for free() ... */
  paper_name = alloc_paper_name = strdup(aPaperName);
  if (!paper_name)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("SetMediumSize: Requested page '%s'\n", paper_name));

  mlist = XpuGetMediumSourceSizeList(mPDisplay, mPContext, &mlist_count);
  if( !mlist )
  {
    return NS_ERROR_GFX_PRINTER_PAPER_SIZE_NOT_SUPPORTED;
  }

  XpuMediumSourceSizeRec *match = nsnull;

#ifdef PR_LOGGING 
  /* Print page sizes for the log... */
  for( i = 0 ; i < mlist_count ; i++ )
  {
    XpuMediumSourceSizeRec *curr = &mlist[i];
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("got '%s'/'%s'\t%d %f %f %f %f\n", 
           XPU_NULLXSTR(curr->tray_name), curr->medium_name, curr->mbool, 
           curr->ma1, curr->ma2, curr->ma3, curr->ma4));
  }
#endif /* PR_LOGGING */

  char *s;
  
  /* Did we get a tray name and paper name (e.g. "manual/din-a4") ? */
  if ((s = strchr(paper_name, '/')) != nsnull)
  {
    const char *tray_name;
    *s = '\0';
    tray_name  = paper_name;
    paper_name = s+1;
    
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("SetMediumSize: searching for '%s'/'%s'\n", tray_name, paper_name));
    match = XpuFindMediumSourceSizeByName(mlist, mlist_count, tray_name, paper_name);
  }
  else
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("SetMediumSize: searching for '%s'\n", paper_name));
    match = XpuFindMediumSourceSizeByName(mlist, mlist_count, nsnull, paper_name);
  }
  
  /* Found a match ? */
  if (match)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG,
           ("match '%s'/'%s' !\n", XPU_NULLXSTR(match->tray_name), match->medium_name));
           
    /* Set document's paper size */
    if( XpuSetDocMediumSourceSize(mPDisplay, mPContext, match) == 1 )
      rv = NS_OK;  
  }
  
  XpuFreeMediumSourceSizeList(mlist);
  free(alloc_paper_name);
  
  return rv;
}

nsresult nsXPrintContext::SetOrientation(int landscape)
{
  const char         *orientation;
  XpuOrientationList  list;
  int                 list_count;
  XpuOrientationRec  *match;

  /* which orientation ? */
  switch( landscape )
  {
    case 1 /* NS_LANDSCAPE */: orientation = "landscape"; break;
    case 0 /* NS_PORTRAIT */:  orientation = "portrait";  break;
    default:  
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
             ("Unsupported orientation %d.\n", landscape));  
      return NS_ERROR_GFX_PRINTER_ORIENTATION_NOT_SUPPORTED;
  }
  
  /* Print the used orientation to the log... */
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("orientation=%s\n", orientation));    

  /* Get list of supported orientations */
  list = XpuGetOrientationList(mPDisplay, mPContext, &list_count);
  if( !list )
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuGetOrientationList() failure.\n"));  
    return NS_ERROR_GFX_PRINTER_ORIENTATION_NOT_SUPPORTED;
  }

#ifdef PR_LOGGING 
  int i;
  /* Print orientations for the log... */
  for( i = 0 ; i < list_count ; i++ )
  {
    XpuOrientationRec *curr = &list[i];
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("got orientation='%s'\n", curr->orientation));
  }
#endif /* PR_LOGGING */

  /* Find requested orientation */
  match = XpuFindOrientationByName(list, list_count, orientation);
  if (!match)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuFindOrientationByName() failure.\n"));  
    XpuFreeOrientationList(list);
    return NS_ERROR_GFX_PRINTER_ORIENTATION_NOT_SUPPORTED;
  }

  /* Set orientation */
  if (XpuSetDocOrientation(mPDisplay, mPContext, match) != 1)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuSetDocOrientation() failure.\n"));  
    
    /* We have a "match" in the list of supported orientations but we are not
     * allowed to set it... Well - if this happens and we only have ONE entry
     * in this list then we can safely assume that this is the only choice...
     * (please correct me if I am wrong) 
     */
    if (list_count != 1)
    {
      /* ... otherwise we have a problem... */
      XpuFreeOrientationList(list);
      return NS_ERROR_GFX_PRINTER_ORIENTATION_NOT_SUPPORTED;
    }
  }
  
  XpuFreeOrientationList(list);

  return NS_OK;
}

nsresult nsXPrintContext::SetPlexMode(const char *plexname)
{
  XpuPlexList  list;
  int          list_count;
  XpuPlexRec  *match;

  /* Print the used plex name to the log... */
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("plexname=%s\n", plexname));    

  /* Get list of supported plex modes */
  list = XpuGetPlexList(mPDisplay, mPContext, &list_count);
  if( !list )
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuGetPlexList() failure.\n"));  
    return NS_ERROR_GFX_PRINTER_PLEX_NOT_SUPPORTED;
  }

#ifdef PR_LOGGING 
  int i;
  /* Print plex modes for the log... */
  for( i = 0 ; i < list_count ; i++ )
  {
    XpuPlexRec *curr = &list[i];
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("got plex='%s'\n", curr->plex));
  }
#endif /* PR_LOGGING */

  /* Find requested plex mode */
  match = XpuFindPlexByName(list, list_count, plexname);
  if (!match)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuFindPlexByName() failure.\n"));  
    XpuFreePlexList(list);
    return NS_ERROR_GFX_PRINTER_PLEX_NOT_SUPPORTED;
  }

  /* Set plex */
  if (XpuSetDocPlex(mPDisplay, mPContext, match) != 1)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuSetDocPlex() failure.\n"));  
    
    /* We have a "match" in the list of supported plex modes but we are not
     * allowed to set it... Well - if this happens and we only have ONE entry
     * in this list then we can safely assume that this is the only choice...
     * (please correct me if I am wrong) 
     */
    if (list_count != 1)
    {
      /* ... otherwise we have a problem... */
      XpuFreePlexList(list);
      return NS_ERROR_GFX_PRINTER_PLEX_NOT_SUPPORTED;
    }
  }
  
  XpuFreePlexList(list);

  return NS_OK;
}


nsresult
nsXPrintContext::SetupPrintContext(nsIDeviceContextSpecXp *aSpec)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::SetupPrintContext()\n"));
  
  float        top, bottom, left, right;
  int          landscape;
  int          num_copies;
  const char  *printername;
  nsresult     rv;

  // Get the Attributes
  aSpec->GetToPrinter(mIsAPrinter);
  aSpec->GetGrayscale(mIsGrayscale);
  aSpec->GetTopMargin(top);
  aSpec->GetBottomMargin(bottom);
  aSpec->GetLeftMargin(left);
  aSpec->GetRightMargin(right);
  aSpec->GetLandscape(landscape);
  aSpec->GetCopies(num_copies);
  
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::SetupPrintContext: borders top=%f, bottom=%f, left=%f, right=%f\n", 
         top, bottom, left, right));

  /* get destination printer (we need this when printing to file as
   * the printer DDX in Xprt generates the data...) 
   */
  aSpec->GetPrinterName(&printername);
  
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
    
    if(!mPrintFile || !*mPrintFile)
      return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
  }

  /* workaround for crash in XCloseDisplay() on Solaris 2.7 Xserver - when 
   * shared memory transport is used XCloseDisplay() tries to free() the 
   * shared memory segment - causing heap corruption and/or SEGV.
   */
  PR_SetEnv("XSUNTRANSPORT=xxx");
     
  /* Get printer, either by "name" (foobar) or "name@display" (foobar@gaja:5) */
  if( XpuGetPrinter(printername, &mPDisplay, &mPContext) != 1 )
    return NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::SetupPrintContext: name='%s', display='%s', vendor='%s', release=%ld\n",
          printername,
          XDisplayString(mPDisplay), 
          XServerVendor(mPDisplay),
          (long)XVendorRelease(mPDisplay)));
          
  if (NS_FAILED(rv = AlertBrokenXprt(mPDisplay)))
    return rv;
    
  if( XpQueryExtension(mPDisplay, &mXpEventBase, &mXpErrorBase) == False )
    return NS_ERROR_UNEXPECTED;
    
#ifdef XPRINT_DEBUG_SOMETIMES_USEFULL
  dumpXpAttributes(mPDisplay, mPContext);
#endif /* XPRINT_DEBUG_SOMETIMES_USEFULL */
  
  const char *paper_name = nsnull,
             *plex_name  = nsnull;
  aSpec->GetPaperName(&paper_name);
  aSpec->GetPlexName(&plex_name);
  
  if (NS_FAILED(XPU_TRACE(rv = SetMediumSize(paper_name))))
    return rv;
  
  if (NS_FAILED(XPU_TRACE(rv = SetOrientation(landscape))))
    return rv;

  if (NS_FAILED(XPU_TRACE(rv = SetPlexMode(plex_name))))
    return rv;
    
  if (NS_FAILED(XPU_TRACE(rv = SetResolution())))
    return rv;
   
  if (XPU_TRACE(XpuSetDocumentCopies(mPDisplay, mPContext, num_copies)) != 1)
    return NS_ERROR_GFX_PRINTER_TOO_MANY_COPIES;
        
  /* set printer context
   * WARNING: after this point it is no longer allows to change job attributes
   * only after the XpSetContext() call the alllication is allowed to make 
   * assumptions about available fonts - otherwise you may get the wrong 
   * ones !!
   */
  XPU_TRACE(XpSetContext(mPDisplay, mPContext));

#ifdef XPRINT_DEBUG_SOMETIMES_USEFULL
  dumpXpAttributes(mPDisplay, mPContext);
#endif /* XPRINT_DEBUG_SOMETIMES_USEFULL */

  /* Get default printer resolution. May fail if Xprt is misconfigured.*/
  if( XpuGetResolution(mPDisplay, mPContext, &mPrintResolution) != 1 )
    return NS_ERROR_GFX_PRINTER_DRIVER_CONFIGURATION_ERROR;

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("print resolution %ld\n", (long)mPrintResolution));
  
  /* We want to get events when Xp(Start|End)(Job|Page) requests are processed... */  
  XpSelectInput(mPDisplay, mPContext, XPPrintMask);  

  return NS_OK;
}

NS_IMETHODIMP
nsXPrintContext::SetResolution( void )
{
  XpuResolutionList list;
  int               list_count;
  XpuResolutionRec *match;
  int               i;
  long              default_resolution;
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::SetResolution().\n"));  

  list = XpuGetResolutionList(mPDisplay, mPContext, &list_count);
  if( !list )
    return NS_ERROR_GFX_PRINTER_DRIVER_CONFIGURATION_ERROR;

#ifdef PR_LOGGING 
  /* Print resolutions for the log... */
  for( i = 0 ; i < list_count ; i++ )
  {
    XpuResolutionRec *curr = &list[i];
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("got resolution=%ld\n", (long)curr->dpi));
  }
#endif /* PR_LOGGING */

  /* We rely on printer default resolution if we have one... */
  if( XpuGetResolution(mPDisplay, mPContext, &default_resolution) == 1 )
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("using printers default resolution=%ld.\n", default_resolution));  
    XpuFreeResolutionList(list);
    return NS_OK;
  }

  /* XXX: Hardcoded resolution values... */
  match = XpuFindResolution(list, list_count, 300, 300); 
  if (!match)
  {
    /* Find a match between 300-600, lower resolution is better */
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("searching between 300-600, lower resolution is better...\n"));
    match = XpuFindResolution(list, list_count, 300, 600); 

    if (!match)
    {
      /* Find a match between 150-300, higher resolution is better */
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("searching between 150-300, higher resolution is better...\n"));
      match = XpuFindResolution(list, list_count, 300, 150); 

      if (!match)
      {
        /* If there is still no match then use the first one from the matches we
         * obtained...*/
         match = &list[0];
      }
    }  
  }

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("setting resolution to %ld DPI.\n", match->dpi));  
    
  if (XpuSetDocResolution(mPDisplay, mPContext, match) != 1)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuSetDocResolution() failure.\n"));
 
    /* We have a "match" in the list of supported resolutions but we are not
     * allowed to set it... Well - if this happens and we only have ONE entry
     * in this list then we can safely assume that this is the only choice...
     * (please correct me if I am wrong) 
     */
    if (list_count != 1)
    {
      /* ... otherwise we have a problem... */
      XpuFreeResolutionList(list);
      return NS_ERROR_GFX_PRINTER_DRIVER_CONFIGURATION_ERROR;
    }
  }
  
  XpuFreeResolutionList(list);
  
  return NS_OK;
}
  
NS_IMETHODIMP
nsXPrintContext::BeginDocument( PRUnichar * aTitle, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage )
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::BeginDocument(aTitle='%s')\n", ((aTitle)?(NS_ConvertUCS2toUTF8(aTitle).get()):("<NULL>"))));
  
  nsXPIDLCString job_title;
       
  if (aTitle)
  {
    /* Note that this is _only_ a _hack_ until bug 73446 
     * ("RFE: Need NS_ConvertUCS2ToLocalEncoding() and 
     * NS_ConvertLocalEncodingToUCS2()") is implemented...
     * Below we need COMPOUNT_TEXT which usually is the same as the Xserver's 
     * local encoding - this hack should at least work for C/POSIX 
     * and *.UTF-8 locales...
     */
    job_title.Assign(NS_ConvertUCS2toUTF8(aTitle));
  }
  else
  {
    job_title.AssignLiteral("Mozilla document without title");
  }
 
  /* Set the Job Attributes */
  XpuSetJobTitle(mPDisplay, mPContext, job_title);
  
  // Check the output type
  if(mIsAPrinter) 
  {
    XPU_TRACE(XpuStartJobToSpooler(mPDisplay));
  } 
  else 
  {   
    if( XPU_TRACE(mXpuPrintToFileHandle = XpuStartJobToFile(mPDisplay, mPContext, mPrintFile)) == nsnull )
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
             ("nsXPrintContext::BeginDocument(): XpuPrintToFile failure %s/(%d)\n", 
             strerror(errno), errno));

      return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
    }    
  } 

  XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, mXpEventBase, XPStartJobNotify));

  mJobStarted = PR_TRUE;
  
  return NS_OK;
}  

NS_IMETHODIMP
nsXPrintContext::BeginPage()
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::BeginPage()\n"));
   
  XPU_TRACE(XpStartPage(mPDisplay, mDrawable));
  XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, mXpEventBase, XPStartPageNotify));

  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::EndPage()
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::EndPage()\n"));

  XPU_TRACE(XpEndPage(mPDisplay));
  XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, mXpEventBase, XPEndPageNotify));
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::EndDocument()
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::EndDocument()\n"));

  XPU_TRACE(XpEndJob(mPDisplay));
  XPU_TRACE(XpuWaitForPrintNotify(mPDisplay, mXpEventBase, XPEndJobNotify));

  /* Are we printing to a file ? */
  if( !mIsAPrinter )
  {
    NS_ASSERTION(nsnull != mXpuPrintToFileHandle, "mXpuPrintToFileHandle is null.");
    
    if( XPU_TRACE(XpuWaitForPrintFileChild(mXpuPrintToFileHandle)) == XPGetDocFinished )
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuWaitForPrintFileChild returned success.\n"));
    }
    else
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuWaitForPrintFileChild returned failure.\n"));
    }
    
    mXpuPrintToFileHandle = nsnull;
  }

  mJobStarted = PR_FALSE;
    
  return NS_OK;
}

NS_IMETHODIMP 
nsXPrintContext::AbortDocument()
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::AbortDocument()\n"));

  if( mJobStarted )
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("canceling...\n"));
    XPU_TRACE(XpCancelJob(mPDisplay, True));
  }  

  /* Are we printing to a file ? */
  if( !mIsAPrinter && mXpuPrintToFileHandle )
  {   
    if( XPU_TRACE(XpuWaitForPrintFileChild(mXpuPrintToFileHandle)) == XPGetDocFinished )
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuWaitForPrintFileChild returned success.\n"));
    }
    else
    {
      PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("XpuWaitForPrintFileChild returned failure.\n"));
    }

    mXpuPrintToFileHandle = nsnull;
  }

  mJobStarted = PR_FALSE;

  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("print job aborted.\n"));

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
  
  double   scalingFactor;
  int      prev_res = 0,
           dummy;
  long     imageResolution;
  PRInt32  aDWidth_scaled,
           aDHeight_scaled;
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

  /* Get the general scaling factor for this device */
  float pixelscale = 1.0;
  mContext->GetCanonicalPixelScale(pixelscale);
  scalingFactor = 1.0 / pixelscale;

  /* Calculate image scaling factors */
  double scale_x = double(aSWidth)  / (double(aDWidth)  * scalingFactor);
  double scale_y = double(aSHeight) / (double(aDHeight) * scalingFactor);
  
  /* Which scaling factor is the "bigger" one ? 
   * (XpSetImageResolution() sets one image resolution for both X- and
   * Y-axis - therefore we want to get the "best"(=smallest job size) out
   * of the scaling factor...).
   */
  scalingFactor *= PR_MIN(scale_x, scale_y);
  
  /* Adjust destination size to the match the scaling factor */
  imageResolution = long(   double(mPrintResolution) * scalingFactor);
  aDWidth_scaled  = PRInt32(double(aDWidth)          * scalingFactor);
  aDHeight_scaled = PRInt32(double(aDHeight)         * scalingFactor);

  /* Check if we scaled the image to zero width/height - if "yes" then use the
   * smaller scaling factor and try again ... */
  if( (aDWidth_scaled <= 0) || (aDHeight_scaled <= 0) )
  {
    /* Chose smaller scaling factor (e.g. the reverse of what we're doing above...) */
    scalingFactor = 1.0 / pixelscale; /* Revert to original value */
    scalingFactor *= PR_MAX(scale_x, scale_y);

    /* Adjust destination size to the match the (new) scaling factor */
    imageResolution = long(   double(mPrintResolution) * scalingFactor);
    aDWidth_scaled  = PRInt32(double(aDWidth)          * scalingFactor);
    aDHeight_scaled = PRInt32(double(aDHeight)         * scalingFactor);
  }

  /* Image scaled to 0 width/height ? */
  NS_ASSERTION(!((aDWidth_scaled <= 0) || (aDHeight_scaled <= 0)),
               "Image scaled to zero width/height");
  if( (aDWidth_scaled <= 0) || (aDHeight_scaled <= 0) )
    return NS_OK;

  /* Image scaled to zero-width/height ? */
  NS_ASSERTION(imageResolution != 0, "Image resolution must not be 0");
  NS_ASSERTION(imageResolution >= 0, "Image resolution must not be negative");
  if( imageResolution <= 0 )
    return NS_OK;

  /* Set image resolution */
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
  
  aImage->LockImagePixels(PR_FALSE);
  
  PRUint8 *image_bits    = aImage->GetBits();
  PRInt32  row_bytes     = aImage->GetLineStride();
  PRUint8  imageDepth    = 24; /* R8G8B8 packed. Thanks to "tor" for that hint... */
  PRUint8 *alphaBits     = aImage->GetAlphaBits();
  PRInt32  alphaRowBytes = aImage->GetAlphaLineStride();
  int      alphaDepth    = aImage->GetAlphaDepth();
  PRInt32  aSrcWidth     = aImage->GetWidth();
  PRInt32  aSrcHeight    = aImage->GetHeight();
  PRUint8 *composed_bits = nsnull;

 /* image data might not be available (ex: spacer image) */
  if (!image_bits)
  {
    aImage->UnlockImagePixels(PR_FALSE);
    return NS_OK;
  }
 
  // Use client-side alpha image composing - plain X11 can only do 1bit alpha 
  // stuff - this method adds 8bit alpha support, too...
  if( alphaBits != nsnull )
  {
    composed_bits = ComposeAlphaImage(alphaBits, alphaRowBytes, alphaDepth,
                                      image_bits, row_bytes,
                                      aSrcWidth, aSrcHeight);
    if (!composed_bits)
    {
      aImage->UnlockImagePixels(PR_FALSE);
      return NS_ERROR_FAILURE;
    }

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
  {
    aImage->UnlockImagePixels(PR_FALSE);
    return NS_ERROR_FAILURE;
  }

  RectStretch(aSWidth, aSHeight,
              aDWidth, aDHeight,
              0, 0, aDWidth-1, aDHeight-1,
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
  
  aImage->UnlockImagePixels(PR_FALSE);

  return rv;
}


NS_IMETHODIMP
nsXPrintContext::DrawImage(xGC *xgc, nsIImage *aImage,
                           PRInt32 aX, PRInt32 aY,
                           PRInt32 dummy1, PRInt32 dummy2)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::DrawImage(%d/%d/%d(=dummy)/%d(=dummy))\n",
         (int)aX, (int)aY, (int)dummy1, (int)dummy2));

  aImage->LockImagePixels(PR_FALSE);

  nsresult rv;
  PRInt32  width         = aImage->GetWidth();
  PRInt32  height        = aImage->GetHeight();
  PRUint8 *alphaBits     = aImage->GetAlphaBits();
  PRInt32  alphaRowBytes = aImage->GetAlphaLineStride();
  PRInt32  alphaDepth    = aImage->GetAlphaDepth();
  PRUint8 *image_bits    = aImage->GetBits();
  PRUint8 *composed_bits = nsnull;
  PRInt32  row_bytes     = aImage->GetLineStride();

  /* image data might not be available (ex: spacer image) */
  if (!image_bits)
  {
    aImage->UnlockImagePixels(PR_FALSE);
    return NS_OK;
  }
  
  // Use client-side alpha image composing - plain X11 can only do 1bit alpha
  // stuff - this method adds 8bit alpha support, too...
  if( alphaBits != nsnull )
  {
    composed_bits = ComposeAlphaImage(alphaBits, alphaRowBytes, alphaDepth,
                                      image_bits, row_bytes,
                                      width, height);
    if (!composed_bits)
    {
      aImage->UnlockImagePixels(PR_FALSE);
      return NS_ERROR_FAILURE;
    }

    image_bits = composed_bits;
    alphaBits = nsnull;
  }
               
  rv = DrawImageBits(xgc, alphaBits, alphaRowBytes, alphaDepth,
                     image_bits, row_bytes,
                     aX, aY, width, height);

  if (composed_bits)
    PR_Free(composed_bits);

  aImage->UnlockImagePixels(PR_FALSE);

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


  xxlib_draw_xprint_scaled_rgb_image(mXlibRgbHandle,
                                     mDrawable,
                                     mPrintResolution, XpGetImageResolution(mPDisplay, mPContext),
                                     image_gc,
                                     aX, aY, aWidth, aHeight,
                                     NS_XPRINT_RGB_DITHER,
                                     image_bits, row_bytes);
  
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

/* nsEPSObjectXp - a EPSF helper class
 * For details on the EPSF spec, see Adobe specification #5002,
 * "Encapsulated PostScript File Format Specification". The document
 * structuring conventions are described in Specificion #5001, 
 * "PostScript Language Document Structuring Conventions Specification".
 */
class nsEPSObjectXp {
  public:
      /** ---------------------------------------------------
       * Constructor
       */
      nsEPSObjectXp(const unsigned char *aData, unsigned long aDataLength) :
        mStatus(NS_ERROR_INVALID_ARG),
        mData(nsnull),
        mDataLength(0UL),
        mCurrPos(nsnull),
        mBBllx(0.0),
        mBBlly(0.0),
        mBBurx(0.0),
        mBBury(0.0)
      {
        mData       = aData;
        mDataLength = aDataLength;

        NS_PRECONDITION(aData != nsnull,   "aData == nsnull");
        NS_PRECONDITION(aDataLength > 0UL, "No data");    

        Reset();
        Parse();
      }
      
      static inline
      PRBool IsEPSF(const unsigned char *aData, unsigned long aDataLength)
      {
        /* First line (assuming a single line of PostScript line is not longer
         * than 256 chars) should usually look like "%!PS-Adobe-3.0 EPSF-3.0"
         * (version numbers may be different) */
        return (PL_strnstr(aData, " EPSF-", PR_MIN(aDataLength, 256)) != nsnull);
      }
      

      /** ---------------------------------------------------
       * @return the result code from parsing the EPS data.
       * If the return value is not NS_OK, the EPS object is
       * invalid and should not be used further.
       */
      nsresult GetStatus() { return mStatus; };

      /** ---------------------------------------------------
       * Return Bounding box coordinates: lower left x,
       * lower left y, upper right x, upper right y.
       */
      inline void GetBoundingBox(PRFloat64 &aBBllx,
                          PRFloat64 &aBBlly,
                          PRFloat64 &aBBurx,
                          PRFloat64 &aBBury)
      {
        aBBllx = mBBllx;
        aBBlly = mBBlly;
        aBBurx = mBBurx;
        aBBury = mBBury;
      };

      /** ---------------------------------------------------
       * Append the EPS object to the provided string object.
       */
      void AppendTo(nsACString& aDestBuffer)
      {
        nsCAutoString line;
        PRBool        inPreview = PR_FALSE;

        Reset();
        while (EPSFFgets(line)) {
          if (inPreview) {
            /* filter out the print-preview section */
            if (StringBeginsWith(line, NS_LITERAL_CSTRING("%%EndPreview")))
                inPreview = PR_FALSE;
            continue;
          }
          else if (StringBeginsWith(line, NS_LITERAL_CSTRING("%%BeginPreview:"))){
            inPreview = PR_TRUE;
            continue;
          }

          /* Output the EPSF with this platform's line terminator */
          aDestBuffer.Append(line.get(), line.Length());
          aDestBuffer.Append(NS_LITERAL_CSTRING("\n"));
        }
      }
  private:
      nsresult             mStatus;
      const unsigned char *mData;
      unsigned long        mDataLength;
      const char          *mCurrPos;
      PRFloat64            mBBllx,
                           mBBlly,
                           mBBurx,
                           mBBury;

      void Parse()
      {
        nsCAutoString line;

        Reset();   
        while (EPSFFgets(line)) {
          if (PR_sscanf(line.get(), "%%%%BoundingBox: %lf %lf %lf %lf",
                        &mBBllx, &mBBlly, &mBBurx, &mBBury) == 4) {
            mStatus = NS_OK;
            return;
          }
        }
        mStatus = NS_ERROR_INVALID_ARG;
      }
      
      inline void Reset()
      {
        mCurrPos = mData;
      }

      PRBool EPSFFgets(nsACString& aBuffer)
      {
        aBuffer.Truncate();
        
        if (!mCurrPos)
          return PR_FALSE;
        
        while (1) {
          int ch = *mCurrPos++;
          if ('\n' == ch) {
            /* Eat any following carriage return */
            ch = *mCurrPos++;
            if ((mCurrPos < (mData + mDataLength)) && ('\r' != ch))
              mCurrPos--;
            return PR_TRUE;
          }
          else if ('\r' == ch) {
            /* Eat any following line feed */
            ch = *mCurrPos++;
            if ((mCurrPos < (mData + mDataLength)) && ('\n' != ch))
              mCurrPos--;
            return PR_TRUE;
          }
          else if (mCurrPos >= (mData + mDataLength)) {
            /* If we read any text before the EOF, return true. */
            return !aBuffer.IsEmpty();
          }

          /* Normal case */
          aBuffer.Append((char)ch);
        }
      }
};

NS_IMETHODIMP nsXPrintContext::RenderEPS(const nsRect& aRect, const unsigned char *aData, unsigned long aDatalen)
{
  PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
         ("nsXPrintContext::EPS(aData, aDatalen=%d)\n", aDatalen));
  
  char xp_formats_supported[] = "xp-embedded-formats-supported";
  const char *embedded_formats_supported = XpGetOneAttribute(mPDisplay, mPContext, XPPrinterAttr, xp_formats_supported);

  /* Check whether "PostScript Level 2" is supported as embedding format
   * (The content of the "xp-embedded-formats-supported" attribute needs
   * to be searched in a case-insensitive way since the model-configs
   * may use the same word with multiple variants of case
   * (e.g. "PostScript" vs. "Postscript" or "PCL" vs. "Pcl" etc.")
   * To avoid problems we simply use |PL_strcasestr()| (case-insensitive
   * strstr()) instead of |strstr()| here...)
   */
  if (embedded_formats_supported == NULL)
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, ("nsXPrintContext::RenderPostScriptDataFragment(): Embedding data not supported for this DDX/Printer\n"));
    return NS_ERROR_FAILURE;    
  }

  if( PL_strcasestr(embedded_formats_supported, "PostScript 2") == NULL )
  {
    PR_LOG(nsXPrintContextLM, PR_LOG_DEBUG, 
           ("nsXPrintContext::RenderPostScriptDataFragment(): Embedding data not supported for this DDX/Printer "
            "(supported embedding formats are '%s')\n", embedded_formats_supported));
    XFree((void *)embedded_formats_supported);
    return NS_ERROR_FAILURE;    
  }

  /* Temp buffer for EPSF data - this has to live outside the if()/else()
   * block below to gurantee that the pointers we get from it are still
   * valid when we feed the data to |XpPutDocumentData| */
  nsXPIDLCString  aBuffer;
  const unsigned char *embedData;
  unsigned long        embedDataLength;
    
  /* If the input is EPSF then we need to do some EPSF-specific handling */
  if (nsEPSObjectXp::IsEPSF(aData, aDatalen))
  {
    PRFloat64 boxLLX, 
              boxLLY,
              boxURX,
              boxURY;

    nsEPSObjectXp epsfData(aData, aDatalen);
    /* Non-EPSF data are not supported yet */
    if (NS_FAILED(epsfData.GetStatus()))
      return NS_ERROR_INVALID_ARG;  

    epsfData.GetBoundingBox(boxLLX, boxLLY, boxURX, boxURY);

    /* Size buffer that all the data in |aData| and context fits into the string */
    aBuffer.SetCapacity(aDatalen + 1024); 
    aBuffer.Assign("%%BeginDocument: Mozilla EPSF plugin data\n"
                   "/b4_Inc_state save def\n"
                   "/dict_count countdictstack def\n"
                   "/op_count count 1 sub def\n"
                   "userdict begin\n"
                   "/showpage { } def\n"
                   "0 setgray 0 setlinecap 1 setlinewidth 0 setlinejoin\n"
                   "10 setmiterlimit [ ] 0 setdash newpath\n"
                   "/languagelevel where\n"
                   "{pop languagelevel\n"
                   "  1 ne\n"
                   "  {false setstrokeadjust false setoverprint\n"
                   "  } if\n"
                   "} if\n");

    /* translate to the lower left corner of the rectangle */
    aBuffer.Append(nsPrintfCString(64, "%f %f translate\n", 
                   double(aRect.x),
                   double(aRect.y + aRect.height)));

    /* Rescale */
    aBuffer.Append(nsPrintfCString(64, "%f %f scale\n", 
                   double(aRect.width / (boxURX - boxLLX)),
                   double(-(aRect.height / (boxURY - boxLLY)))));

    /* Translate to the EPSF origin. Can't use translate() here because
     * it takes integers.
     */
    aBuffer.Append(nsPrintfCString(64, "%f %f translate\n", double(-boxLLX), double(-boxLLY)));

    epsfData.AppendTo(aBuffer);
    aBuffer.Append("count op_count sub { pop } repeat\n"
                   "countdictstack dict_count sub { end } repeat\n"
                   "b4_Inc_state restore\n"
                   "%%EndDocument\n");
    embedData       = aBuffer.get();
    embedDataLength = aBuffer.Length();
  }
  else
  {
    /* Non-EPSF codepath - pass the data as-is... */
    embedData       = aData;
    embedDataLength = aDatalen;
  }
  
  /* Note that the embedded PostScript code uses the same resolution and
   * coordinate space as currently be used by the DDX (if you do not
   * want that simply reset it yourself :) */
  const char *type     = "PostScript 2"; /* Format of embedded data 
                                          * (older PS DDX may be picky, fixed via
                                          * http://xprint.mozdev.org/bugs/show_bug.cgi?id=4023)
                                          */
  const char *option   = "";             /* PostScript DDX does not support any options yet
                                          * (in general |BadValue| will be returned for not
                                          * supported options/option values) */

  /* XpPutDocumentData() takes |const| input for all string arguments, only the X11 prototypes do not allow |const| yet */
  XpPutDocumentData(mPDisplay, mDrawable, (unsigned char *)embedData, embedDataLength, (char *)type, (char *)option);

  XFree((void *)embedded_formats_supported);
  
  return NS_OK;
}


