/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are
 * Copyright (C) 2004 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Stuart Parmenter <pavlov@pavlov.net>
 *    Vladimir Vukicevic <vladimir@pobox.com>
 *    Joe Hewitt <hewitt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 */

#include "nsIServiceManager.h"

#include "nsCairoDeviceContext.h"
#include "nsCairoRenderingContext.h"

#include "nsCOMPtr.h"
#include "nsIView.h"

#ifdef MOZ_ENABLE_GTK2
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "nsFontMetricsUtils.h"
#include "nsFont.h"

#include <pango/pango.h>
#include <pango/pangox.h>
#include <pango/pango-fontmap.h>
#endif

class nsSystemFontsGTK {

  public:
    nsSystemFontsGTK(float aPixelsToTwips);

    const nsFont& GetDefaultFont() { return mDefaultFont; }
    const nsFont& GetMenuFont() { return mMenuFont; }
    const nsFont& GetFieldFont() { return mFieldFont; }
    const nsFont& GetButtonFont() { return mButtonFont; }

  private:
    nsresult GetSystemFontInfo(GtkWidget *aWidget, nsFont* aFont,
                               float aPixelsToTwips) const;

    /*
     * The following system font constants exist:
     *
     * css2: http://www.w3.org/TR/REC-CSS2/fonts.html#x27
     * eSystemFont_Caption, eSystemFont_Icon, eSystemFont_Menu,
     * eSystemFont_MessageBox, eSystemFont_SmallCaption,
     * eSystemFont_StatusBar,
     * // css3
     * eSystemFont_Window, eSystemFont_Document,
     * eSystemFont_Workspace, eSystemFont_Desktop,
     * eSystemFont_Info, eSystemFont_Dialog,
     * eSystemFont_Button, eSystemFont_PullDownMenu,
     * eSystemFont_List, eSystemFont_Field,
     * // moz
     * eSystemFont_Tooltips, eSystemFont_Widget
     */
    nsFont mDefaultFont;
    nsFont mButtonFont;
    nsFont mFieldFont;
    nsFont mMenuFont;
};

static nsSystemFontsGTK *gSystemFonts = nsnull;


NS_IMPL_ISUPPORTS_INHERITED0(nsCairoDeviceContext, DeviceContextImpl)

nsCairoDeviceContext::nsCairoDeviceContext()
{
    NS_INIT_ISUPPORTS();

    mDevUnitsToAppUnits = 1.0f;
    mAppUnitsToDevUnits = 1.0f;
    mCPixelScale = 1.0f;
    mZoom = 1.0f;
    mTextZoom = 1.0f;

#ifdef MOZ_ENABLE_XLIB
    mXlibRgbHandle = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE);
#endif
}

nsCairoDeviceContext::~nsCairoDeviceContext()
{
}

int
helpful_error_handler (Display *dpy, XErrorEvent *error)
{
    fprintf (stderr, "++++++++ X Error! serial %d code: %d request: %d minor: %d\n",
             error->serial, error->error_code, error->request_code, error->minor_code);
    return 0;
}

NS_IMETHODIMP
nsCairoDeviceContext::Init(nsNativeWidget aWidget)
{
    //DeviceContextImpl::CommonInit();

    mTwipsToPixels = 96 / (float)NSIntPointsToTwips(72);
    mPixelsToTwips = 1.0f / mTwipsToPixels;
    //mTwipsToPixels = 1.0f;
    //mPixelsToTwips = 1.0f;

    mWidget = aWidget;

    if (!mScreenManager)
        mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");
    if (!mScreenManager)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIScreen> screen;
    mScreenManager->GetPrimaryScreen (getter_AddRefs(screen));
    if (screen) {
        PRInt32 x, y, width, height;
        screen->GetRect (&x, &y, &width, &height );
        mWidthFloat = float(width);
        mHeightFloat = float(height);
    }

    if (getenv ("MOZ_X_SYNC")) {
        fprintf (stderr, "+++ Enabling XSynchronize\n");
        XSynchronize (gdk_x11_get_default_xdisplay(), True);
        XSetErrorHandler (helpful_error_handler);
    }

    mWidth = -1;
    mHeight = -1;

    return NS_OK;
}

NS_IMETHODIMP
nsCairoDeviceContext::CreateRenderingContext(nsIView *aView,
                                             nsIRenderingContext *&aContext)
{
    NS_ENSURE_ARG_POINTER(aView);
    NS_PRECONDITION(aView->HasWidget(), "View has no widget!");

    nsCOMPtr<nsIWidget> widget;
    widget = aView->GetWidget();

    return CreateRenderingContext(widget, aContext);
}

NS_IMETHODIMP
nsCairoDeviceContext::CreateRenderingContext(nsIDrawingSurface *aSurface,
                                             nsIRenderingContext *&aContext)
{
    nsresult rv;

    aContext = nsnull;
    nsCOMPtr<nsIRenderingContext> pContext;
    rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
    if (NS_SUCCEEDED(rv)) {
        rv = pContext->Init(this, aSurface);
        if (NS_SUCCEEDED(rv)) {
            aContext = pContext;
            NS_ADDREF(aContext);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsCairoDeviceContext::CreateRenderingContext(nsIWidget *aWidget,
                                             nsIRenderingContext *&aContext)
{
    nsresult rv;

    aContext = nsnull;
    nsCOMPtr<nsIRenderingContext> pContext;
    rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
    if (NS_SUCCEEDED(rv)) {
        rv = pContext->Init(this, aWidget);
        if (NS_SUCCEEDED(rv)) {
            aContext = pContext;
            NS_ADDREF(aContext);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsCairoDeviceContext::CreateRenderingContext(nsIRenderingContext *&aContext)
{
    NS_ERROR("CreateRenderingContext with other rendering context arg; fix this if this needs to be called");
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDeviceContext::CreateRenderingContextInstance(nsIRenderingContext *&aContext)
{
    nsCOMPtr<nsIRenderingContext> renderingContext = new nsCairoRenderingContext();
    if (!renderingContext)
        return NS_ERROR_OUT_OF_MEMORY;

    aContext = renderingContext;
    NS_ADDREF(aContext);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoDeviceContext::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
    aSupportsWidgets = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDeviceContext::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
    aWidth = 10.0f * mPixelsToTwips;
    aHeight = 10.0f * mPixelsToTwips;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDeviceContext::GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
    nsresult status = NS_OK;

    if (!gSystemFonts) {
        gSystemFonts = new nsSystemFontsGTK(mPixelsToTwips);
    }

    switch (aID) {
    case eSystemFont_Menu:         // css2
    case eSystemFont_PullDownMenu: // css3
        *aFont = gSystemFonts->GetMenuFont();
        break;

    case eSystemFont_Field:        // css3
    case eSystemFont_List:         // css3
        *aFont = gSystemFonts->GetFieldFont();
        break;

    case eSystemFont_Button:       // css3
        *aFont = gSystemFonts->GetButtonFont();
        break;

    case eSystemFont_Caption:      // css2
    case eSystemFont_Icon:         // css2
    case eSystemFont_MessageBox:   // css2
    case eSystemFont_SmallCaption: // css2
    case eSystemFont_StatusBar:    // css2
    case eSystemFont_Window:       // css3
    case eSystemFont_Document:     // css3
    case eSystemFont_Workspace:    // css3
    case eSystemFont_Desktop:      // css3
    case eSystemFont_Info:         // css3
    case eSystemFont_Dialog:       // css3
    case eSystemFont_Tooltips:     // moz
    case eSystemFont_Widget:       // moz
        *aFont = gSystemFonts->GetDefaultFont();
        break;
    }

    return status;
}

NS_IMETHODIMP
nsCairoDeviceContext::CheckFontExistence(const nsString& aFaceName)
{
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDeviceContext::GetDepth(PRUint32& aDepth)
{
    aDepth = 24;
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
    aPaletteInfo.isPaletteDevice = PR_FALSE;
    aPaletteInfo.sizePalette = 0;
    aPaletteInfo.numReserved = 0;
    aPaletteInfo.palette = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
    aPixel = aColor;
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
    if (mWidth == -1)
        mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

    if (mHeight == -1)
        mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

    aWidth = mWidth;
    aHeight = mHeight;

    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::GetRect(nsRect &aRect)
{
#if defined (MOZ_ENABLE_GTK2) || defined (MOZ_ENABLE_XLIB)
    if (mWidget) {
        Window root_ignore;
        int x, y;
        unsigned int bwidth_ignore, width, height, depth;

        XGetGeometry(GDK_WINDOW_XDISPLAY(GDK_DRAWABLE(mWidget)),
                     GDK_WINDOW_XWINDOW(GDK_DRAWABLE(mWidget)),
                     &root_ignore, &x, &y,
                     &width, &height,
                     &bwidth_ignore, &depth);

        nsCOMPtr<nsIScreen> screen;
        mScreenManager->ScreenForRect(x, y, width, height, getter_AddRefs(screen));
        screen->GetRect(&aRect.x, &aRect.y, &aRect.width, &aRect.height);

        aRect.x = NSToIntRound(mDevUnitsToAppUnits * aRect.x);
        aRect.y = NSToIntRound(mDevUnitsToAppUnits * aRect.y);
        aRect.width = NSToIntRound(mDevUnitsToAppUnits * aRect.width);
        aRect.height = NSToIntRound(mDevUnitsToAppUnits * aRect.height);
    } else {
        aRect.x = 0;
        aRect.y = 0;

        this->GetDeviceSurfaceDimensions(aRect.width, aRect.height);
    }
#else
#error write me
#endif

    fprintf (stderr, "+++ GetRect: %d %d %d %d\n", aRect.x, aRect.y, aRect.width, aRect.height);

    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::GetClientRect(nsRect &aRect)
{
    fprintf (stderr, "+++ GetClientRect: ");
    nsresult rv = this->GetRect(aRect);
    return rv;
}

/*
 * below methods are for printing and are not implemented
 */
NS_IMETHODIMP
nsCairoDeviceContext::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                          nsIDeviceContext *&aContext)
{
    /* we don't do printing */
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCairoDeviceContext::PrepareDocument(PRUnichar * aTitle, 
                                      PRUnichar*  aPrintToFileName)
{
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::BeginDocument(PRUnichar*  aTitle, 
                                            PRUnichar*  aPrintToFileName,
                                            PRInt32     aStartPage, 
                                            PRInt32     aEndPage)
{
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::EndDocument(void)
{
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::AbortDocument(void)
{
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::BeginPage(void)
{
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::EndPage(void)
{
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::SetAltDevice(nsIDeviceContext* aAltDC)
{
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::GetAltDevice(nsIDeviceContext** aAltDC)
{
    *aAltDC = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::SetUseAltDC(PRUint8 aValue, PRBool aOn)
{
    return NS_OK;
}

#if defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_XLIB)
Display *
nsCairoDeviceContext::GetXDisplay()
{
#ifdef MOZ_ENABLE_GTK2
    return gdk_x11_get_default_xdisplay();
#endif
}

Visual *
nsCairoDeviceContext::GetXVisual()
{
    return DefaultVisual(GetXDisplay(),DefaultScreen(GetXDisplay()));
}

Colormap
nsCairoDeviceContext::GetXColormap()
{
    return DefaultColormap(GetXDisplay(),DefaultScreen(GetXDisplay()));
}

Drawable
nsCairoDeviceContext::GetXPixmapParentDrawable()
{
    return RootWindow(GetXDisplay(),DefaultScreen(GetXDisplay()));
}

#endif



PRInt32 GetXftDPI(void);

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

  gtk_widget_ensure_style(label);

  GetSystemFontInfo(label, &mDefaultFont, aPixelsToTwips);

  gtk_widget_destroy(window);  // no unref, windows are different

  // mFieldFont
  GtkWidget *entry = gtk_entry_new();
  parent = gtk_fixed_new();
  window = gtk_window_new(GTK_WINDOW_POPUP);

  gtk_container_add(GTK_CONTAINER(parent), entry);
  gtk_container_add(GTK_CONTAINER(window), parent);
  gtk_widget_ensure_style(entry);

  GetSystemFontInfo(entry, &mFieldFont, aPixelsToTwips);

  gtk_widget_destroy(window);  // no unref, windows are different

  // mMenuFont
  GtkWidget *accel_label = gtk_accel_label_new("M");
  GtkWidget *menuitem = gtk_menu_item_new();
  GtkWidget *menu = gtk_menu_new();
  gtk_object_ref(GTK_OBJECT(menu));
  gtk_object_sink(GTK_OBJECT(menu));

  gtk_container_add(GTK_CONTAINER(menuitem), accel_label);
  gtk_menu_append(GTK_MENU(menu), menuitem);

  gtk_widget_ensure_style(accel_label);

  GetSystemFontInfo(accel_label, &mMenuFont, aPixelsToTwips);

  gtk_widget_unref(menu);

  // mButtonFont
  parent = gtk_fixed_new();
  GtkWidget *button = gtk_button_new();
  label = gtk_label_new("M");
  window = gtk_window_new(GTK_WINDOW_POPUP);
          
  gtk_container_add(GTK_CONTAINER(button), label);
  gtk_container_add(GTK_CONTAINER(parent), button);
  gtk_container_add(GTK_CONTAINER(window), parent);

  gtk_widget_ensure_style(label);

  GetSystemFontInfo(label, &mButtonFont, aPixelsToTwips);

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

#if defined(MOZ_ENABLE_COREXFONTS) || defined(MOZ_WIDGET_GTK)

#define LOCATE_MINUS(pos, str)  { \
   pos = str.FindChar('-'); \
   if (pos < 0) \
     return ; \
  }
#define NEXT_MINUS(pos, str) { \
   pos = str.FindChar('-', pos+1); \
   if (pos < 0) \
     return ; \
  }  

static void
AppendFontFFREName(nsString& aString, const char* aXLFDName)
{
  // convert fontname from XFLD to FFRE and append, ie. from
  // -adobe-courier-medium-o-normal--14-140-75-75-m-90-iso8859-15
  // to
  // adobe-courier-iso8859-15
  nsCAutoString nameStr(aXLFDName);
  PRInt32 pos1, pos2;
  // remove first '-' and everything before it. 
  LOCATE_MINUS(pos1, nameStr);
  nameStr.Cut(0, pos1+1);

  // skip foundry and family name
  LOCATE_MINUS(pos1, nameStr);
  NEXT_MINUS(pos1, nameStr);
  pos2 = pos1;

  // find '-' just before charset registry
  for (PRInt32 i=0; i < 10; i++) {
    NEXT_MINUS(pos2, nameStr);
  }

  // remove everything in between
  nameStr.Cut(pos1, pos2-pos1);

  aString.AppendWithConversion(nameStr.get());
}
#endif /* MOZ_ENABLE_COREXFONTS || MOZ_WIDGET_GTK*/

#ifdef MOZ_WIDGET_GTK
static void
AppendFontName(XFontStruct* aFontStruct, nsString& aString, Display *aDisplay)
{
  unsigned long pr = 0;
  // we first append the FFRE name to reconstruct font more faithfully
  unsigned long font_atom = gdk_atom_intern("FONT", FALSE);
  if (::XGetFontProperty(aFontStruct, font_atom, &pr) && pr) {
    char* xlfdName = ::XGetAtomName(aDisplay, pr);
    AppendFontFFREName(aString, xlfdName);
    ::XFree(xlfdName);
  }
 
  aString.Append(PRUnichar(','));

  // next, we need to append family name to cover more encodings.
  if ((::XGetFontProperty(aFontStruct, XA_FAMILY_NAME, &pr) ||
       ::XGetFontProperty(aFontStruct, XA_FULL_NAME, &pr)) &&
      pr) {
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
    if (::XGetFontProperty(aFontStruct, weightName, &pr) && pr) {
      char *weightString = ::XGetAtomName(aDisplay, pr);
      if (nsCRT::strcasecmp(weightString, "bold") == 0)
        weight = NS_FONT_WEIGHT_BOLD;
      ::XFree(weightString);
    }
  }

  pr = 0;
  if (::XGetFontProperty(aFontStruct, XA_WEIGHT, &pr) && pr > 10 )
    weight = NS_FONT_WEIGHT_BOLD;

  return weight;
}

static nscoord
GetFontSize(XFontStruct *aFontStruct, float aPixelsToTwips)
{
  unsigned long pr = 0;
  Atom pixelSizeAtom = ::XInternAtom(GDK_DISPLAY(), "PIXEL_SIZE", 0);
  if (!::XGetFontProperty(aFontStruct, pixelSizeAtom, &pr) || !pr)
    return DEFAULT_TWIP_FONT_SIZE;
  return NSIntPixelsToTwips(pr, aPixelsToTwips);
}

nsresult
nsSystemFontsGTK::GetSystemFontInfo(GtkWidget *aWidget, nsFont* aFont,
                                    float aPixelsToTwips) const
{
  GtkStyle *style = gtk_widget_get_style(aWidget);

  GdkFont *theFont = style->font;

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

    XFontSet fontSet = NS_REINTERPRET_CAST(XFontSet, GDK_FONT_XFONT(theFont));
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
      // we need to append FFRE name instead of family name in this case
      AppendFontFFREName(fontName, *fontNames);
      ++fontNames;
      --numFonts;
      if (numFonts == 0)
        break;
      fontName.Append(PRUnichar(','));
    }
  }
  return NS_OK;
}
#endif /* MOZ_WIDGET_GTK */

#ifdef MOZ_WIDGET_GTK2

#ifdef MOZ_ENABLE_COREXFONTS
static void xlfd_from_pango_font_description(GtkWidget *aWidget,
                                             const PangoFontDescription *aFontDesc,
                                             nsString& aFontName);
#endif /* MOZ_ENABLE_COREXFONTS */

nsresult
nsSystemFontsGTK::GetSystemFontInfo(GtkWidget *aWidget, nsFont* aFont,
                                    float aPixelsToTwips) const
{
  GtkSettings *settings = gtk_widget_get_settings(aWidget);

  aFont->style       = NS_FONT_STYLE_NORMAL;
  aFont->decorations = NS_FONT_DECORATION_NONE;

  gchar *fontname;
  g_object_get(settings, "gtk-font-name", &fontname, NULL);

  PangoFontDescription *desc;
  desc = pango_font_description_from_string(fontname);

  aFont->systemFont = PR_TRUE;

  g_free(fontname);

  aFont->name.Truncate();
#ifdef MOZ_ENABLE_XFT
  if (NS_IsXftEnabled()) {
    aFont->name.Assign(PRUnichar('"'));
    aFont->name.AppendWithConversion(pango_font_description_get_family(desc));
    aFont->name.Append(PRUnichar('"'));
  }
#endif /* MOZ_ENABLE_XFT */

#ifdef MOZ_ENABLE_COREXFONTS
  // if name already set by Xft, do nothing
  if (!aFont->name.Length()) {
    xlfd_from_pango_font_description(aWidget, desc, aFont->name);
  }
#endif /* MOZ_ENABLE_COREXFONTS */
  aFont->weight = pango_font_description_get_weight(desc);

  float size = float(pango_font_description_get_size(desc) / PANGO_SCALE);
#ifdef MOZ_ENABLE_XFT
  if (NS_IsXftEnabled()) {
    PRInt32 dpi = GetXftDPI();
    if (dpi != 0) {
      // pixels/inch * twips/pixel * inches/twip == 1, except it isn't, since
      // our idea of dpi may be different from Xft's.
      size *= float(dpi) * aPixelsToTwips * (1.0f/1440.0f);
    }
  }
#endif /* MOZ_ENABLE_XFT */
  aFont->size = NSFloatPointsToTwips(size);
  
  pango_font_description_free(desc);

  return NS_OK;
}
#endif /* MOZ_WIDGET_GTK2 */

#ifdef MOZ_WIDGET_GTK
/* static */
PRInt32
GetOSDPI(void)
{

#ifdef MOZ_ENABLE_XFT
  // try to get it from xft
  if (NS_IsXftEnabled()) {
    PRInt32 xftdpi = GetXftDPI();
    if (xftdpi)
      return xftdpi;
  }
#endif /* MOZ_ENABLE_XFT */

  // Set OSVal to what the operating system thinks the logical resolution is.
  float screenWidthIn = float(::gdk_screen_width_mm()) / 25.4f;
  return NSToCoordRound(float(::gdk_screen_width()) / screenWidthIn);
}
#endif /* MOZ_WIDGET_GTK */

#ifdef MOZ_WIDGET_GTK2
/* static */
PRInt32
GetOSDPI(void)
{
  GtkSettings *settings = gtk_settings_get_default();

  // first try to get the gtk2 dpi
  gint dpi = 0;

  // See if there's a gtk-xft-dpi object on the settings object - note
  // that we don't have to free the spec since it isn't addrefed
  // before being returned.  It's just part of an internal object.
  // The gtk-xft-dpi setting is included in rh8 and might be included
  // in later versions of gtk, so we conditionally check for it.
  GParamSpec *spec;
  spec = g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(settings)),
                                      "gtk-xft-dpi");
  if (spec) {
    g_object_get(G_OBJECT(settings),
                 "gtk-xft-dpi", &dpi,
                 NULL);
  }

  if (dpi > 0)
    return NSToCoordRound(dpi / 1024.0);

#ifdef MOZ_ENABLE_XFT
  // try to get it from xft
  PRInt32 xftdpi = GetXftDPI();

  if (xftdpi)
    return xftdpi;
#endif /* MOZ_ENABLE_XFT */
  
  // fall back to the physical resolution
  float screenWidthIn = float(::gdk_screen_width_mm()) / 25.4f;
  return NSToCoordRound(float(::gdk_screen_width()) / screenWidthIn);
}
#endif /* MOZ_WIDGET_GTK2 */

#ifdef MOZ_ENABLE_XFT
/* static */
PRInt32
GetXftDPI(void)
{
  char *val = XGetDefault(GDK_DISPLAY(), "Xft", "dpi");
  if (val) {
    char *e;
    double d = strtod(val, &e);

    if (e != val)
      return NSToCoordRound(d);
  }

  return 0;
}
#endif /* MOZ_ENABLE_XFT */

#if defined(MOZ_WIDGET_GTK2) && defined(MOZ_ENABLE_COREXFONTS)
// xlfd_from_pango_font_description copied from vte, which was
// written by nalin@redhat.com, and added some codes.
static void
xlfd_from_pango_font_description(GtkWidget *aWidget,
         const PangoFontDescription *aFontDesc,
                                 nsString& aFontName)
{
  char *spec;
  PangoContext *context;
  PangoFont *font;
  PangoXSubfont *subfont_ids;
  PangoFontMap *fontmap;
  int *subfont_charsets, i, count = 0;
  char *tmp, *subfont;
  char *encodings[] = {
    "ascii-0",
    "big5-0",
    "dos-437",
    "dos-737",
    "gb18030.2000-0",
    "gb18030.2000-1",
    "gb2312.1980-0",
    "iso8859-1",
    "iso8859-2",
    "iso8859-3",
    "iso8859-4",
    "iso8859-5",
    "iso8859-7",
    "iso8859-8",
    "iso8859-9",
    "iso8859-10",
    "iso8859-15",
    "iso10646-0",
    "iso10646-1",
    "jisx0201.1976-0",
    "jisx0208.1983-0",
    "jisx0208.1990-0",
    "jisx0208.1997-0",
    "jisx0212.1990-0",
    "jisx0213.2000-1",
    "jisx0213.2000-2",
    "koi8-r",
    "koi8-u",
    "koi8-ub",
    "ksc5601.1987-0",
    "ksc5601.1992-3",
    "tis620-0",
    "iso8859-13",
    "microsoft-cp1251"
    "misc-fontspecific",
  };
#if XlibSpecificationRelease >= 6
  XOM xom;
#endif
  if (!aFontDesc) {
    return;
  }

  context = gtk_widget_get_pango_context(GTK_WIDGET(aWidget));

  pango_context_set_language (context, gtk_get_default_language ());
  fontmap = pango_x_font_map_for_display(GDK_DISPLAY());

  if (!fontmap) {
    return;
  }

  font = pango_font_map_load_font(fontmap, context, aFontDesc);
  if (!font) {
    return;
  }

#if XlibSpecificationRelease >= 6
  xom = XOpenOM (GDK_DISPLAY(), NULL, NULL, NULL);
  if (xom) {
    XOMCharSetList cslist;
    int n_encodings = 0;
    cslist.charset_count = 0;
    XGetOMValues (xom,
      XNRequiredCharSet, &cslist,
      NULL);
    n_encodings = cslist.charset_count;
    if (n_encodings) {
      char **xom_encodings = (char**) g_malloc (sizeof(char*) * n_encodings);

      for (i = 0; i < n_encodings; i++) {
        xom_encodings[i] = g_ascii_strdown (cslist.charset_list[i], -1);
      }
      count = pango_x_list_subfonts(font, xom_encodings, n_encodings,
            &subfont_ids, &subfont_charsets);

      for(i = 0; i < n_encodings; i++) {
        g_free (xom_encodings[i]);
      }
      g_free (xom_encodings);
    }
    XCloseOM (xom);
  }
#endif
  if (count == 0) {
    count = pango_x_list_subfonts(font, encodings, G_N_ELEMENTS(encodings),
          &subfont_ids, &subfont_charsets);
  }

  for (i = 0; i < count; i++) {
    subfont = pango_x_font_subfont_xlfd(font, subfont_ids[i]);
    AppendFontFFREName(aFontName, subfont);
    g_free(subfont);
    aFontName.Append(PRUnichar(','));
  }

  spec = pango_font_description_to_string(aFontDesc);

  if (subfont_ids != NULL) {
    g_free(subfont_ids);
  }
  if (subfont_charsets != NULL) {
    g_free(subfont_charsets);
  }
  g_free(spec);
}
#endif /* MOZ_WIDGET_GTK2 && MOZ_ENABLE_COREXFONTS */
