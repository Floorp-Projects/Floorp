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
 */

#include <math.h>

#include "nspr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "il_util.h"
#include "nsCRT.h"

#include "nsDeviceContextGTK.h"
#include "nsFontMetricsGTK.h"
#include "nsGfxCIID.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"

#ifdef USE_XPRINT
#include "nsGfxXPrintCID.h"
#include "nsIDeviceContextXPrint.h"
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>

#include "nsIScreenManager.h"

#ifdef USE_XPRINT
#include "nsDeviceContextSpecG.h"
#endif

#define NS_TO_GDK_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor) NS_RGB(c.red, c.green, c.blue))

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define GDK_DEFAULT_FONT1 "-*-helvetica-medium-r-*--*-120-*-*-*-*-iso8859-1"
#define GDK_DEFAULT_FONT2 "-*-fixed-medium-r-*-*-*-120-*-*-*-*-*-*"
extern GdkFont *default_font;
nsresult GetSystemFontInfo(GdkFont* iFont, nsSystemAttrID anID, nsFont* aFont);

nscoord nsDeviceContextGTK::mDpi = 96;

NS_IMPL_ISUPPORTS1(nsDeviceContextGTK, nsIDeviceContext)

nsDeviceContextGTK::nsDeviceContextGTK()
{
  NS_INIT_REFCNT();
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mDepth = 0 ;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mNumCells = 0;

  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
}

nsDeviceContextGTK::~nsDeviceContextGTK()
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(kPrefCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    prefs->UnregisterCallback("browser.display.screen_resolution",
                              prefChanged, (void *)this);
  }
}


NS_IMETHODIMP nsDeviceContextGTK::Init(nsNativeWidget aNativeWidget)
{
  GtkRequisition req;
  GtkWidget *sb;

  // get the screen object and its width/height
  // XXXRight now this will only get the primary monitor.

  nsresult ignore;
  nsCOMPtr<nsIScreenManager> sm ( do_GetService("@mozilla.org/gfx/screenmanager;1", &ignore) );
  if ( sm ) {
    nsCOMPtr<nsIScreen> screen;
    sm->GetPrimaryScreen ( getter_AddRefs(screen) );
    if ( screen ) {
      PRInt32 x, y, width, height, depth;
      screen->GetAvailRect ( &x, &y, &width, &height );
      screen->GetPixelDepth ( &depth );
      mWidthFloat = float(width);
      mHeightFloat = float(height);
      mDepth = NS_STATIC_CAST ( PRUint32, depth );
    }
  }
    
  static int initialized = 0;
  if (!initialized) {
    initialized = 1;

    // Set prefVal the value of the preference "browser.display.screen_resolution"
    // or -1 if we can't get it.
    // If it's negative, we pretend it's not set.
    // If it's 0, it means force use of the operating system's logical resolution.
    // If it's positive, we use it as the logical resolution
    PRInt32 prefVal = -1;
    nsresult res;

    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
    if (NS_SUCCEEDED(res) && prefs) {
      res = prefs->GetIntPref("browser.display.screen_resolution", &prefVal);
      if (! NS_SUCCEEDED(res)) {
        prefVal = -1;
      }
      prefs->RegisterCallback("browser.display.screen_resolution", prefChanged,
                              (void *)this);
    }

    // Set OSVal to what the operating system thinks the logical resolution is.
    float screenWidthIn = float(::gdk_screen_width_mm()) / 25.4f;
    PRInt32 OSVal = nscoord(mWidthFloat / screenWidthIn);

    if (prefVal > 0) {
      // If there's a valid pref value for the logical resolution,
      // use it.
      mDpi = prefVal;
    } else if ((prefVal == 0) || (OSVal > 96)) {
      // Either if the pref is 0 (force use of OS value) or the OS
      // value is bigger than 96, use the OS value.
      mDpi = OSVal;
    } else {
      // if we couldn't get the pref or it's negative, and the OS
      // value is under 96ppi, then use 96.
      mDpi = 96;
    }
  }

  SetDPI(mDpi);
  
  sb = gtk_vscrollbar_new(NULL);
  gtk_widget_ref(sb);
  gtk_object_sink(GTK_OBJECT(sb));
  gtk_widget_size_request(sb,&req);
  mScrollbarWidth = req.width;
  gtk_widget_destroy(sb);
  gtk_widget_unref(sb);
  
  sb = gtk_hscrollbar_new(NULL);
  gtk_widget_ref(sb);
  gtk_object_sink(GTK_OBJECT(sb));
  gtk_widget_size_request(sb,&req);
  mScrollbarHeight = req.height;
  gtk_widget_destroy(sb);
  gtk_widget_unref(sb);

#ifdef DEBUG
  static PRBool once = PR_TRUE;
  if (once) {
    printf("GFX: dpi=%d t2p=%g p2t=%g depth=%d\n", mDpi, mTwipsToPixels, mPixelsToTwips,mDepth);
    once = PR_FALSE;
  }
#endif

  DeviceContextImpl::CommonInit();

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  nsIRenderingContext *pContext;
  nsresult             rv;
  nsDrawingSurfaceGTK  *surf;
  GtkWidget *w;

  w = (GtkWidget*)mWidget;

  // to call init for this, we need to have a valid nsDrawingSurfaceGTK created
  pContext = new nsRenderingContextGTK();

  if (nsnull != pContext)
  {
    NS_ADDREF(pContext);

    // create the nsDrawingSurfaceGTK
    surf = new nsDrawingSurfaceGTK();

    if (surf && w)
      {
        GdkDrawable *gwin = nsnull;
        GdkDrawable *win = nsnull;
        // FIXME
        if (GTK_IS_LAYOUT(w))
          gwin = (GdkDrawable*)GTK_LAYOUT(w)->bin_window;
        else
          gwin = (GdkDrawable*)(w)->window;

        // window might not be realized... ugh
        if (gwin)
          gdk_window_ref(gwin);
        else
          win = gdk_pixmap_new(nsnull,
                               w->allocation.width,
                               w->allocation.height,
                               gdk_rgb_get_visual()->depth);

        GdkGC *gc = gdk_gc_new(win);

        // init the nsDrawingSurfaceGTK
        rv = surf->Init(win,gc);

        if (NS_OK == rv)
          // Init the nsRenderingContextGTK
          rv = pContext->Init(this, surf);
      }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  if (NS_OK != rv)
  {
    NS_IF_RELEASE(pContext);
  }

  aContext = pContext;

  return rv;
}

NS_IMETHODIMP nsDeviceContextGTK::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  //XXX it is very critical that this not lie!! MMP
  // read the comments in the mac code for this
  aSupportsWidgets = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  aWidth = mScrollbarWidth * mPixelsToTwips;
  aHeight = mScrollbarHeight * mPixelsToTwips;
 
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;
  GtkStyle *style = gtk_style_new();  // get the default styles

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_SELECTED]);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_SELECTED]);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_SELECTED]);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->text[GTK_STATE_SELECTED]);
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight:
        aInfo->mSize = mScrollbarHeight;
        break;
    case eSystemAttr_Size_ScrollbarWidth: 
        aInfo->mSize = mScrollbarWidth;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = style->klass->xthickness;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = style->klass->ythickness;
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = 4;
        break;
    //---------
    // Fonts
    //---------
    case eSystemAttr_Font_Caption: 		// css2
    case eSystemAttr_Font_Icon: 
    case eSystemAttr_Font_Menu: 
    case eSystemAttr_Font_MessageBox: 
    case eSystemAttr_Font_SmallCaption: 
    case eSystemAttr_Font_StatusBar: 
    case eSystemAttr_Font_Window:			// css3
    case eSystemAttr_Font_Document:
    case eSystemAttr_Font_Workspace:
    case eSystemAttr_Font_Desktop:
    case eSystemAttr_Font_Info:
    case eSystemAttr_Font_Dialog:
    case eSystemAttr_Font_Button:
    case eSystemAttr_Font_PullDownMenu:
    case eSystemAttr_Font_List:
    case eSystemAttr_Font_Field:
    case eSystemAttr_Font_Tooltips:		// moz
    case eSystemAttr_Font_Widget:
        status = GetSystemFontInfo(style->font, anID, aInfo->mFont);
      break;
  } // switch

  gtk_style_unref(style);

  return status;
}

NS_IMETHODIMP nsDeviceContextGTK::GetDrawingSurface(nsIRenderingContext &aContext, 
                                                    nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;  
}

NS_IMETHODIMP nsDeviceContextGTK::ConvertPixel(nscolor aColor, 
                                               PRUint32 & aPixel)
{
  aPixel = ::gdk_rgb_xpixel_from_rgb (NS_TO_GDK_RGB(aColor));

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextGTK::CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsGTK::FamilyExists(aFontName);
}

NS_IMETHODIMP nsDeviceContextGTK::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  if (mWidth == -1)
    mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

  if (mHeight == -1)
    mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

  aWidth = mWidth;
  aHeight = mHeight;

  return NS_OK;
}



NS_IMETHODIMP nsDeviceContextGTK::GetRect(nsRect &aRect)
{
  PRInt32 width, height;
  nsresult rv;
  rv = GetDeviceSurfaceDimensions(width, height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}


NS_IMETHODIMP nsDeviceContextGTK::GetClientRect(nsRect &aRect)
{
//XXX do we know if the client rect should ever differ from the screen rect?
  return GetRect ( aRect );
}

NS_IMETHODIMP nsDeviceContextGTK::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                      nsIDeviceContext *&aContext)
{
#ifdef USE_XPRINT
  int method=-1;
  nsDeviceContextSpecGTK* spec=(nsDeviceContextSpecGTK*)aDevice;
  spec->GetPrintMethod(method);

  if (method == 1) { // XPRINT
    static NS_DEFINE_CID(kCDeviceContextXP, NS_DEVICECONTEXTXP_CID);
    nsresult rv;
    nsIDeviceContextXP *dcxp;
  
    rv = nsComponentManager::CreateInstance(kCDeviceContextXP,
                                            nsnull,
                                            NS_GET_IID(nsIDeviceContextXP),
                                            (void **)&dcxp);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create XP Device context");
  
    dcxp->SetSpec(aDevice);
    dcxp->InitDeviceContextXP((nsIDeviceContext*)aContext,
                              (nsIDeviceContext*)this);

    rv = dcxp->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);

    NS_RELEASE(dcxp);
  
    return rv;
  }
  // default/PS
#endif
  static NS_DEFINE_CID(kCDeviceContextPS, NS_DEVICECONTEXTPS_CID);
  
  // Create a Postscript device context 
  nsresult rv;
  nsIDeviceContextPS *dcps;
  
  rv = nsComponentManager::CreateInstance(kCDeviceContextPS,
                                          nsnull,
                                          NS_GET_IID(nsIDeviceContextPS),
                                          (void **)&dcps);

  NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context");
  
  dcps->SetSpec(aDevice);
  dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                            (nsIDeviceContext*)this);

  rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                            (void **)&aContext);

  NS_RELEASE(dcps);
  
  return rv;
}

NS_IMETHODIMP nsDeviceContextGTK::BeginDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::EndDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::BeginPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::EndPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth;
  return NS_OK;
}

nsresult
nsDeviceContextGTK::SetDPI(PRInt32 aDpi)
{
  mDpi = aDpi;
  
  int pt2t = 72;

  // make p2t a nice round number - this prevents rounding problems
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(aDpi)));
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  // XXX need to reflow all documents
  return NS_OK;
}

int nsDeviceContextGTK::prefChanged(const char *aPref, void *aClosure)
{
  nsDeviceContextGTK *context = (nsDeviceContextGTK*)aClosure;
  nsresult rv;
  
  if (nsCRT::strcmp(aPref, "browser.display.screen_resolution")==0) {
    PRInt32 dpi;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    rv = prefs->GetIntPref(aPref, &dpi);
    if (NS_SUCCEEDED(rv))
      context->SetDPI(dpi);
  }
  
  return 0;
}

nsresult GetSystemFontInfo( GdkFont* iFont, nsSystemAttrID anID, nsFont* aFont)
{
  nsresult status = NS_OK;
  GdkFont *theFont = iFont;

  aFont->style       = NS_FONT_STYLE_NORMAL;
  aFont->weight      = NS_FONT_WEIGHT_NORMAL;
  aFont->decorations = NS_FONT_DECORATION_NONE;
  
  // do we have the default_font defined by GTK/GDK then
  // we use it, if not then we load helvetica, if not then
  // we load fixed font else we error out.
  if( !theFont )
    theFont = default_font; // GTK default font
  
  if( !theFont )
    theFont = ::gdk_font_load( GDK_DEFAULT_FONT1 );
  
  if( !theFont )
    theFont = ::gdk_font_load( GDK_DEFAULT_FONT2 );
  
  if( !theFont )
  {
    status = NS_ERROR_FAILURE;
  }
  else
  {
    char *fontName = (char *)NULL;
    GdkFontPrivate *fontPrivate;
    XFontStruct *fontInfo;
    unsigned long pr = 0;

    // XXX I am not sure if I can use this as it is supposed to
    // be private.
    fontPrivate = (GdkFontPrivate*) theFont;
    fontInfo = (XFontStruct *)GDK_FONT_XFONT( theFont );

    ::XGetFontProperty( fontInfo, XA_FULL_NAME, &pr );
    if( pr )
    {
      fontName = XGetAtomName( fontPrivate->xdisplay, pr );
      aFont->name.AssignWithConversion( fontName );
      ::XFree( fontName );
    }
  
    pr = 0;
    ::XGetFontProperty( fontInfo, XA_WEIGHT, &pr );
    if ( pr > 10 )
      aFont->weight = NS_FONT_WEIGHT_BOLD;
    
    pr = 0;
    ::XGetFontProperty( fontInfo, XA_POINT_SIZE, &pr );
    if( pr )
    {
      int fontSize = pr/10;
      // Per rods@netscape.com, the default size of 12 is too large
      // we need to bump it down two points.
      if( anID == eSystemAttr_Font_Button ||
          anID == eSystemAttr_Font_Field ||
          anID == eSystemAttr_Font_List ||
          anID == eSystemAttr_Font_Widget ||
          anID == eSystemAttr_Font_Caption )
        fontSize -= 2;
      
      // if we are below 1 then we have a problem we just make it 1
      if( fontSize < 1 )
        fontSize = 1;
      
      aFont->size = NSIntPointsToTwips( fontSize );
    }

    pr = 0;
    ::XGetFontProperty( fontInfo, XA_ITALIC_ANGLE, &pr );
    if( pr )
      aFont->style = NS_FONT_STYLE_ITALIC;
    
    pr = 0;
    ::XGetFontProperty( fontInfo, XA_UNDERLINE_THICKNESS, &pr );
    if( pr )
      aFont->decorations = NS_FONT_DECORATION_UNDERLINE;
    
    status = NS_OK;
  }
  return (status);
}
