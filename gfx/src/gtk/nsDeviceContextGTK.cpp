/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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

#include <math.h>

#include "nspr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"

#include "nsDeviceContextGTK.h"
#include "nsFontMetricsGTK.h"
#include "nsGfxCIID.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"
#ifdef USE_XPRINT
#include "nsGfxXPrintCID.h"
#include "nsIDeviceContextXPrint.h"
#endif /* USE_XPRINT */

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>

#include "nsDeviceContextSpecG.h"

#define NS_TO_GDK_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor) NS_RGB(c.red, c.green, c.blue))

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define GDK_DEFAULT_FONT1 "-*-helvetica-medium-r-*--*-120-*-*-*-*-iso8859-1"
#define GDK_DEFAULT_FONT2 "-*-fixed-medium-r-*-*-*-120-*-*-*-*-*-*"
extern GdkFont *default_font;

/**
 * A singleton instance of nsSystemFontsGTK is created by the first
 * device context and destroyed by the module destructor.
 */
class nsSystemFontsGTK {

  public:
    nsSystemFontsGTK(float aPixelsToTwips);

    const nsFont& GetDefaultFont() { return mDefaultFont; }
    const nsFont& GetMenuFont() { return mMenuFont; }
    const nsFont& GetFieldFont() { return mFieldFont; }
    const nsFont& GetButtonFont() { return mButtonFont; }

  private:
    nsresult GetSystemFontInfo(GdkFont* iFont, nsFont* aFont,
                               float aPixelsToTwips) const;

    /*
     * The following system font constants exist:
     *
     * css2: http://www.w3.org/TR/REC-CSS2/fonts.html#x27
     * eSystemAttr_Font_Caption, eSystemAttr_Font_Icon, eSystemAttr_Font_Menu,
     * eSystemAttr_Font_MessageBox, eSystemAttr_Font_SmallCaption,
     * eSystemAttr_Font_StatusBar,
     * // css3
     * eSystemAttr_Font_Window, eSystemAttr_Font_Document,
     * eSystemAttr_Font_Workspace, eSystemAttr_Font_Desktop,
     * eSystemAttr_Font_Info, eSystemAttr_Font_Dialog,
     * eSystemAttr_Font_Button, eSystemAttr_Font_PullDownMenu,
     * eSystemAttr_Font_List, eSystemAttr_Font_Field,
     * // moz
     * eSystemAttr_Font_Tooltips, eSystemAttr_Font_Widget
     */
    nsFont mDefaultFont;
    nsFont mButtonFont;
    nsFont mFieldFont;
    nsFont mMenuFont;
};


nscoord nsDeviceContextGTK::mDpi = 96;
static nsSystemFontsGTK *gSystemFonts = nsnull;

NS_IMPL_ISUPPORTS1(nsDeviceContextGTK, nsIDeviceContext)

nsDeviceContextGTK::nsDeviceContextGTK()
{
  NS_INIT_REFCNT();

  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mDepth = 0 ;
  mNumCells = 0;

  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
  mDeviceWindow = nsnull;
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

/* static */ void nsDeviceContextGTK::Shutdown()
{
  if (gSystemFonts) {
    delete gSystemFonts;
    gSystemFonts = nsnull;
  }
}

NS_IMETHODIMP nsDeviceContextGTK::Init(nsNativeWidget aNativeWidget)
{
  GtkRequisition req;
  GtkWidget *sb;
  
  // get the screen object and its width/height
  // XXXRight now this will only get the primary monitor.

  if (!mScreenManager)
    mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (!mScreenManager) {
    return NS_ERROR_FAILURE;
  }

  if (aNativeWidget) {
    // superwin?
    if (GDK_IS_SUPERWIN(aNativeWidget)) {
      mDeviceWindow = GDK_SUPERWIN(aNativeWidget)->shell_window;
    }
    // gtk widget?
    else if (GTK_IS_WIDGET(aNativeWidget)) {
      mDeviceWindow = GTK_WIDGET(aNativeWidget)->window;
    }
    // must be a bin_window
    else {
      mDeviceWindow = NS_STATIC_CAST(GdkWindow *, aNativeWidget);
    }
  }

  nsCOMPtr<nsIScreen> screen;
  mScreenManager->GetPrimaryScreen ( getter_AddRefs(screen) );
  if ( screen ) {
    PRInt32 x, y, width, height, depth;
    screen->GetAvailRect ( &x, &y, &width, &height );
    screen->GetPixelDepth ( &depth );
    mWidthFloat = float(width);
    mHeightFloat = float(height);
    mDepth = NS_STATIC_CAST ( PRUint32, depth );
  }
    
  static int initialized = 0;
  PRInt32 prefVal = -1;
  if (!initialized) {
    initialized = 1;

    // Set prefVal the value of the preference
    // "browser.display.screen_resolution"
    // or -1 if we can't get it.
    // If it's negative, we pretend it's not set.
    // If it's 0, it means force use of the operating system's logical
    // resolution.
    // If it's positive, we use it as the logical resolution
    nsresult res;

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
    if (NS_SUCCEEDED(res) && prefs) {
      res = prefs->GetIntPref("browser.display.screen_resolution", &prefVal);
      if (NS_FAILED(res)) {
        prefVal = -1;
      }
      prefs->RegisterCallback("browser.display.screen_resolution", prefChanged,
                              (void *)this);
    }

    SetDPI(prefVal);
  } else {
    SetDPI(mDpi); // to setup p2t and t2p
  }

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

  if (!gSystemFonts) {
    gSystemFonts = new nsSystemFontsGTK(mPixelsToTwips);
  }

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
    case eSystemAttr_Font_Menu:         // css2
    case eSystemAttr_Font_PullDownMenu: // css3
        *aInfo->mFont = gSystemFonts->GetMenuFont();
        break;

    case eSystemAttr_Font_Field:        // css3
    case eSystemAttr_Font_List:         // css3
        *aInfo->mFont = gSystemFonts->GetFieldFont();
        break;

    case eSystemAttr_Font_Button:       // css3
        *aInfo->mFont = gSystemFonts->GetButtonFont();
        break;

    case eSystemAttr_Font_Caption:      // css2
    case eSystemAttr_Font_Icon:         // css2
    case eSystemAttr_Font_MessageBox:   // css2
    case eSystemAttr_Font_SmallCaption: // css2
    case eSystemAttr_Font_StatusBar:    // css2
    case eSystemAttr_Font_Window:       // css3
    case eSystemAttr_Font_Document:     // css3
    case eSystemAttr_Font_Workspace:    // css3
    case eSystemAttr_Font_Desktop:      // css3
    case eSystemAttr_Font_Info:         // css3
    case eSystemAttr_Font_Dialog:       // css3
    case eSystemAttr_Font_Tooltips:     // moz
    case eSystemAttr_Font_Widget:       // moz
        *aInfo->mFont = gSystemFonts->GetDefaultFont();
        break;
  }

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
  // if we have an initialized widget for this device context, use it
  // to try and get real screen coordinates.
  if (mDeviceWindow) {
    gint x, y, width, height, depth;
    x = y = width = height = 0;

    gdk_window_get_geometry(mDeviceWindow, &x, &y, &width, &height,
                            &depth);
    gdk_window_get_origin(mDeviceWindow, &x, &y);

    nsCOMPtr<nsIScreen> screen;
    mScreenManager->ScreenForRect(x, y, width, height, getter_AddRefs(screen));
    screen->GetRect(&aRect.x, &aRect.y, &aRect.width, &aRect.height);
    aRect.x = NSToIntRound(mDevUnitsToAppUnits * aRect.x);
    aRect.y = NSToIntRound(mDevUnitsToAppUnits * aRect.y);
    aRect.width = NSToIntRound(mDevUnitsToAppUnits * aRect.width);
    aRect.height = NSToIntRound(mDevUnitsToAppUnits * aRect.height);
  }
  else {
    PRInt32 width, height;
    GetDeviceSurfaceDimensions(width, height);
    aRect.x = 0;
    aRect.y = 0;
    aRect.width = width;
    aRect.height = height;
  }
  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextGTK::GetClientRect(nsRect &aRect)
{
  // The client rect is never different from the standard rect on
  // linux because we don't have the concept of the title bar.
  return GetRect ( aRect );
}

NS_IMETHODIMP nsDeviceContextGTK::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                       nsIDeviceContext *&aContext)
{
  nsresult                 rv;
  PrintMethod              method;
  nsDeviceContextSpecGTK  *spec = NS_STATIC_CAST(nsDeviceContextSpecGTK *, aDevice);
  
  rv = spec->GetPrintMethod(method);
  if (NS_FAILED(rv)) 
    return rv;

#ifdef USE_XPRINT
  if (method == pmXprint) { // XPRINT
    static NS_DEFINE_CID(kCDeviceContextXp, NS_DEVICECONTEXTXP_CID);
    nsCOMPtr<nsIDeviceContextXp> dcxp(do_CreateInstance(kCDeviceContextXp, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create Xp Device context.");    
    if (NS_FAILED(rv)) 
      return rv;
    
    rv = dcxp->SetSpec(aDevice);
    if (NS_FAILED(rv)) 
      return rv;
    
    rv = dcxp->InitDeviceContextXP((nsIDeviceContext*)aContext,
                                   (nsIDeviceContext*)this);
    if (NS_FAILED(rv)) 
      return rv;
      
    rv = dcxp->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);
    return rv;
  }
  else
#endif /* USE_XPRINT */
  if (method == pmPostScript) { // PostScript
    // default/PS
    static NS_DEFINE_CID(kCDeviceContextPS, NS_DEVICECONTEXTPS_CID);
  
    // Create a Postscript device context 
    nsCOMPtr<nsIDeviceContextPS> dcps(do_CreateInstance(kCDeviceContextPS, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context.");
    if (NS_FAILED(rv)) 
      return rv;
  
    rv = dcps->SetSpec(aDevice);
    if (NS_FAILED(rv)) 
      return rv;
      
    rv = dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                                   (nsIDeviceContext*)this);
    if (NS_FAILED(rv)) 
      return rv;

    rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);
    return rv;
  }
  
  NS_WARNING("no print module created.");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsDeviceContextGTK::BeginDocument(PRUnichar * aTitle)
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
nsDeviceContextGTK::SetDPI(PRInt32 aPrefDPI)
{
  // Set OSVal to what the operating system thinks the logical resolution is.
  float screenWidthIn = float(::gdk_screen_width_mm()) / 25.4f;
  PRInt32 OSVal = nscoord(mWidthFloat / screenWidthIn);

  if (aPrefDPI > 0) {
    // If there's a valid pref value for the logical resolution,
    // use it.
    mDpi = aPrefDPI;
  } else if ((aPrefDPI == 0) || (OSVal > 96)) {
    // Either if the pref is 0 (force use of OS value) or the OS
    // value is bigger than 96, use the OS value.
    mDpi = OSVal;
  } else {
    // if we couldn't get the pref or it's negative, and the OS
    // value is under 96ppi, then use 96.
    mDpi = 96;
  }
  
  int pt2t = 72;

  // make p2t a nice round number - this prevents rounding problems
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(mDpi)));
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
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    rv = prefs->GetIntPref(aPref, &dpi);
    if (NS_SUCCEEDED(rv))
      context->SetDPI(dpi);

    // If this pref changes, we have to clear our cache of stored system
    // fonts.
    if (gSystemFonts) {
      delete gSystemFonts;
      gSystemFonts = nsnull;
    }
  }
  
  return 0;
}

#define DEFAULT_TWIP_FONT_SIZE 240

nsSystemFontsGTK::nsSystemFontsGTK(float aPixelsToTwips)
  : mDefaultFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                 NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
                 DEFAULT_TWIP_FONT_SIZE),
    mButtonFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
                DEFAULT_TWIP_FONT_SIZE),
    mFieldFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
               NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
               DEFAULT_TWIP_FONT_SIZE),
    mMenuFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
               NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
               DEFAULT_TWIP_FONT_SIZE)
{
  /*
   * Much of the widget creation code here is similar to the code in
   * nsLookAndFeel::InitColors().
   */

  // mDefaultFont
  GtkWidget *label = gtk_label_new("M");
  GtkWidget *parent = gtk_fixed_new();
  GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);

  gtk_container_add(GTK_CONTAINER(parent), label);
  gtk_container_add(GTK_CONTAINER(window), parent);

  gtk_widget_set_rc_style(parent);
  gtk_widget_set_rc_style(label);
  gtk_widget_realize(parent);
  gtk_widget_realize(label);

  GtkStyle *style = gtk_widget_get_style(label);
  GetSystemFontInfo(style->font, &mDefaultFont, aPixelsToTwips);

  gtk_widget_destroy(window);  // no unref, windows are different

  // mFieldFont
  GtkWidget *entry = gtk_entry_new();
  parent = gtk_fixed_new();
  window = gtk_window_new(GTK_WINDOW_POPUP);

  gtk_container_add(GTK_CONTAINER(parent), entry);
  gtk_container_add(GTK_CONTAINER(window), parent);

  gtk_widget_set_rc_style(entry);
  gtk_widget_realize(entry);

  style = gtk_widget_get_style(entry);
  GetSystemFontInfo(style->font, &mFieldFont, aPixelsToTwips);

  gtk_widget_destroy(window);  // no unref, windows are different

  // mMenuFont
  GtkWidget *accel_label = gtk_accel_label_new("M");
  GtkWidget *menuitem = gtk_menu_item_new();
  GtkWidget *menu = gtk_menu_new();

  gtk_container_add(GTK_CONTAINER(menuitem), accel_label);
  gtk_menu_append(GTK_MENU(menu), menuitem);

  gtk_widget_set_rc_style(accel_label);
  gtk_widget_set_rc_style(menu);
  gtk_widget_realize(menu);
  gtk_widget_realize(accel_label);

  style = gtk_widget_get_style(accel_label);
  GetSystemFontInfo(style->font, &mMenuFont, aPixelsToTwips);

  gtk_widget_unref(menu);

  // mButtonFont
  parent = gtk_fixed_new();
  GtkWidget *button = gtk_button_new();
  label = gtk_label_new("M");
  window = gtk_window_new(GTK_WINDOW_POPUP);
          
  gtk_container_add(GTK_CONTAINER(button), label);
  gtk_container_add(GTK_CONTAINER(parent), button);
  gtk_container_add(GTK_CONTAINER(window), parent);

  gtk_widget_set_rc_style(button);
  gtk_widget_set_rc_style(label);

  gtk_widget_realize(button);
  gtk_widget_realize(label);

  style = gtk_widget_get_style(label);
  GetSystemFontInfo(style->font, &mButtonFont, aPixelsToTwips);

  gtk_widget_destroy(window);  // no unref, windows are different

}

#if 0 // debugging code to list the font properties
static void
ListFontProps(XFontStruct *aFont, Display *aDisplay)
{
  printf("\n\n");
  for (int i = 0, n = aFont->n_properties; i < n; ++i) {
    XFontProp *prop = aFont->properties + i;
    char *atomName = ::XGetAtomName(aDisplay, prop->name);
    // 500 is just a guess
    char *cardName = (prop->card32 > 0 && prop->card32 < 500)
                       ? ::XGetAtomName(aDisplay, prop->card32)
                       : 0;
    printf("%s : %ld (%s)\n", atomName, prop->card32, cardName?cardName:"");
    ::XFree(atomName);
    if (cardName)
      ::XFree(cardName);
  }
  printf("\n\n");
}
#endif

static void
AppendFontName(XFontStruct* aFontStruct, nsString& aString, Display *aDisplay)
{
  unsigned long pr = 0;
  ::XGetFontProperty(aFontStruct, XA_FAMILY_NAME, &pr);
  if (!pr)
    ::XGetFontProperty(aFontStruct, XA_FULL_NAME, &pr);
  if (pr) {
    char *fontName = ::XGetAtomName(aDisplay, pr);
    aString.AppendWithConversion(fontName);
    ::XFree(fontName);
  }
}

static PRUint16
GetFontWeight(XFontStruct* aFontStruct, Display *aDisplay)
{
  PRUint16 weight = NS_FONT_WEIGHT_NORMAL;

  // WEIGHT_NAME seems more reliable than WEIGHT, where 10 can mean
  // anything.  Check both, and make it bold if either says so.
  unsigned long pr = 0;
  Atom weightName = ::XInternAtom(aDisplay, "WEIGHT_NAME", True);
  if (weightName != None) {
    ::XGetFontProperty(aFontStruct, weightName, &pr);
    if (pr) {
      char *weightString = ::XGetAtomName(aDisplay, pr);
      if (nsCRT::strcasecmp(weightString, "bold") == 0)
        weight = NS_FONT_WEIGHT_BOLD;
      ::XFree(weightString);
    }
  }

  pr = 0;
  ::XGetFontProperty(aFontStruct, XA_WEIGHT, &pr);
  if ( pr > 10 )
    weight = NS_FONT_WEIGHT_BOLD;

  return weight;
}

static nscoord
GetFontSize(XFontStruct *aFontStruct, float aPixelsToTwips)
{
  unsigned long pr = 0;
  Atom pixelSizeAtom = ::XInternAtom(GDK_DISPLAY(), "PIXEL_SIZE", 0);
  ::XGetFontProperty(aFontStruct, pixelSizeAtom, &pr);
  if (!pr)
    return DEFAULT_TWIP_FONT_SIZE;
  return NSIntPixelsToTwips(pr, aPixelsToTwips);
}

nsresult
nsSystemFontsGTK::GetSystemFontInfo(GdkFont* iFont, nsFont* aFont, float aPixelsToTwips) const
{
  GdkFont *theFont = iFont;

  aFont->style       = NS_FONT_STYLE_NORMAL;
  aFont->weight      = NS_FONT_WEIGHT_NORMAL;
  aFont->decorations = NS_FONT_DECORATION_NONE;
  
  // do we have the default_font defined by GTK/GDK then
  // we use it, if not then we load helvetica, if not then
  // we load fixed font else we error out.
  if (!theFont)
    theFont = default_font; // GTK default font
  
  if (!theFont)
    theFont = ::gdk_font_load( GDK_DEFAULT_FONT1 );
  
  if (!theFont)
    theFont = ::gdk_font_load( GDK_DEFAULT_FONT2 );
  
  if (!theFont)
    return NS_ERROR_FAILURE;

  Display *fontDisplay = GDK_FONT_XDISPLAY(theFont);
  if (theFont->type == GDK_FONT_FONT) {
    XFontStruct *fontStruct =
        NS_STATIC_CAST(XFontStruct*, GDK_FONT_XFONT(theFont));

    aFont->name.Truncate();
    AppendFontName(fontStruct, aFont->name, fontDisplay);
    aFont->weight = GetFontWeight(fontStruct, fontDisplay);
    aFont->size = GetFontSize(fontStruct, aPixelsToTwips);
  } else {
    NS_ASSERTION(theFont->type == GDK_FONT_FONTSET,
                 "theFont->type can only have two values");

    XFontSet fontSet = NS_STATIC_CAST(XFontSet, GDK_FONT_XFONT(theFont));
    XFontStruct **fontStructs;
    char **fontNames;
    int numFonts = ::XFontsOfFontSet(fontSet, &fontStructs, &fontNames);
    if (numFonts == 0)
      return NS_ERROR_FAILURE;

    // Use the weight and size from the first font, but append all
    // the names.
    aFont->weight = GetFontWeight(*fontStructs, fontDisplay);
    aFont->size = GetFontSize(*fontStructs, aPixelsToTwips);
    nsString& fontName = aFont->name;
    fontName.Truncate();
    for (;;) {
      AppendFontName(*fontStructs, fontName, fontDisplay);
      ++fontStructs;
      --numFonts;
      if (numFonts == 0)
        break;
      fontName.AppendWithConversion(",");
    }
  }
  return NS_OK;
}
