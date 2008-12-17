/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Stuart Parmenter <pavlov@pavlov.net>
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
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsCRT.h"

#include "nsThebesDeviceContext.h"
#include "nsThebesRenderingContext.h"

#include "nsIView.h"
#include "nsILookAndFeel.h"

#ifdef MOZ_ENABLE_GTK2
// for getenv
#include <cstdlib>
// for round
#include <cmath>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "nsFont.h"

#include <pango/pango.h>
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include <pango/pangox.h>
#endif /* MOZ_X11 */
#include <pango/pango-fontmap.h>
#endif /* GTK2 */

#include "gfxImageSurface.h"

#ifdef MOZ_ENABLE_GTK2
#include "nsSystemFontsGTK2.h"
#include "gfxPDFSurface.h"
#include "gfxPSSurface.h"
static nsSystemFontsGTK2 *gSystemFonts = nsnull;
#elif XP_WIN
#include "nsSystemFontsWin.h"
#include "gfxWindowsSurface.h"
#include "gfxPDFSurface.h"
static nsSystemFontsWin *gSystemFonts = nsnull;
#ifndef WINCE
#include <usp10.h>
#endif
#elif defined(XP_OS2)
#include "nsSystemFontsOS2.h"
#include "gfxPDFSurface.h"
static nsSystemFontsOS2 *gSystemFonts = nsnull;
#elif defined(XP_BEOS)
#include "nsSystemFontsBeOS.h"
static nsSystemFontsBeOS *gSystemFonts = nsnull;
#elif XP_MACOSX
#include "nsSystemFontsMac.h"
#include "gfxQuartzSurface.h"
#include "gfxImageSurface.h"
static nsSystemFontsMac *gSystemFonts = nsnull;
#elif defined(MOZ_WIDGET_QT)
#include "nsSystemFontsQt.h"
static nsSystemFontsQt *gSystemFonts = nsnull;
#else
#error Need to declare gSystemFonts!
#endif

#if defined(MOZ_ENABLE_GTK2) && defined(MOZ_X11)
extern "C" {
static int x11_error_handler (Display *dpy, XErrorEvent *err) {
    NS_ASSERTION(PR_FALSE, "X Error");
    return 0;
}
}
#endif

#ifdef PR_LOGGING
PRLogModuleInfo* gThebesGFXLog = nsnull;
#endif

NS_IMPL_ISUPPORTS_INHERITED0(nsThebesDeviceContext, DeviceContextImpl)

nsThebesDeviceContext::nsThebesDeviceContext()
{
#ifdef PR_LOGGING
    if (!gThebesGFXLog)
        gThebesGFXLog = PR_NewLogModule("thebesGfx");
#endif

    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("#### Creating DeviceContext %p\n", this));

    mDepth = 0;
    mWidth = 0;
    mHeight = 0;
    mPrintingScale = 1.0f;

    mWidgetSurfaceCache.Init();

#if defined(XP_WIN) && !defined(WINCE)
    SCRIPT_DIGITSUBSTITUTE sds;
    ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &sds);
#endif
}

nsThebesDeviceContext::~nsThebesDeviceContext()
{
}

/* static */ void
nsThebesDeviceContext::Shutdown()
{
    delete gSystemFonts;
    gSystemFonts = nsnull;
}

nsresult
nsThebesDeviceContext::SetDPI()
{
    PRInt32 dpi = -1;
    PRBool dotsArePixels = PR_TRUE;

    // PostScript, PDF and Mac (when printing) all use 72 dpi
    if (mPrintingSurface &&
        (mPrintingSurface->GetType() == gfxASurface::SurfaceTypePDF ||
         mPrintingSurface->GetType() == gfxASurface::SurfaceTypePS ||
         mPrintingSurface->GetType() == gfxASurface::SurfaceTypeQuartz)) {
        dpi = 72;
        dotsArePixels = PR_FALSE;
    } else {
        // Get prefVal the value of the preference
        // "layout.css.dpi"
        // or -1 if we can't get it.
        // If it's negative, use the default DPI setting
        // If it's 0, force the use of the OS's set resolution.  Set this if your
        //      X server has the correct DPI and it's less than 96dpi.
        // If it's positive, we use it as the logical resolution
        nsresult rv;
        PRInt32 prefDPI;
        nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv) && prefs) {
            rv = prefs->GetIntPref("layout.css.dpi", &prefDPI);
            if (NS_FAILED(rv)) {
                prefDPI = -1;
            }
        }

#if defined(MOZ_ENABLE_GTK2)
        GdkScreen *screen = gdk_screen_get_default();
        gtk_settings_get_for_screen(screen); // Make sure init is run so we have a resolution
        PRInt32 OSVal = PRInt32(round(gdk_screen_get_resolution(screen)));

        if (prefDPI == 0) // Force the use of the OS dpi
            dpi = OSVal;
        else  // Otherwise, the minimum dpi is 96dpi
            dpi = PR_MAX(OSVal, 96);

#elif defined(XP_WIN)
        // XXX we should really look at the widget if !dc but it is currently always null
        HDC dc = GetPrintHDC();
        if (dc) {
            PRInt32 OSVal = GetDeviceCaps(dc, LOGPIXELSY);

            dpi = 144;
            mPrintingScale = float(OSVal)/dpi;
            dotsArePixels = PR_FALSE;
        } else {
            dc = GetDC((HWND)nsnull);

            PRInt32 OSVal = GetDeviceCaps(dc, LOGPIXELSY);

            ReleaseDC((HWND)nsnull, dc);

            if (OSVal != 0)
                dpi = OSVal;
        }

#elif defined(XP_OS2)
        // get a printer DC if available, otherwise create a new (memory) DC
        HDC dc = GetPrintHDC();
        PRBool doCloseDC = PR_FALSE;
        if (dc <= 0) { // test for NULLHANDLE/DEV_ERROR or HDC_ERROR
            // create DC compatible with the screen
            dc = DevOpenDC((HAB)1, OD_MEMORY,"*",0L, NULL, NULLHANDLE);
            doCloseDC = PR_TRUE;
        }
        if (dc > 0) {
            // we do have a DC and we can query the DPI setting from it
            LONG lDPI;
            if (DevQueryCaps(dc, CAPS_VERTICAL_FONT_RES, 1, &lDPI))
                dpi = lDPI;
            if (doCloseDC)
                DevCloseDC(dc);
        }
        if (dpi < 0) // something didn't work before, fall back to hardcoded DPI value
            dpi = 96;
#elif defined(XP_MACOSX)

        // we probably want to actually get a real DPI here?
        dpi = 96;

#elif defined(MOZ_WIDGET_QT)
		// TODO: get real DPI here with Qt methods
        dpi = 96;
#else
#error undefined platform dpi
#endif

        if (prefDPI > 0 && !mPrintingSurface)
            dpi = prefDPI;
    }

    NS_ASSERTION(dpi != -1, "no dpi set");

    if (dotsArePixels) {
        // First figure out the closest multiple of 96, which is the number of
        // dev pixels per CSS pixel.  Then, divide that into AppUnitsPerCSSPixel()
        // to get the number of app units per dev pixel.  The PR_MAXes are to
        // make sure we don't end up dividing by zero.
        PRUint32 roundedDPIScaleFactor = (dpi + 48)/96;
#ifdef MOZ_WIDGET_GTK2
        // be more conservative about activating scaling on GTK2, since the dpi
        // information is more likely to be wrong
        roundedDPIScaleFactor = dpi/96;
#endif
        mAppUnitsPerDevNotScaledPixel =
          PR_MAX(1, AppUnitsPerCSSPixel() / PR_MAX(1, roundedDPIScaleFactor));
    } else {
        /* set mAppUnitsPerDevPixel so we're using exactly 72 dpi, even
         * though that means we have a non-integer number of device "pixels"
         * per CSS pixel
         */
        mAppUnitsPerDevNotScaledPixel = (AppUnitsPerCSSPixel() * 96) / dpi;
    }

    mAppUnitsPerInch = NSIntPixelsToAppUnits(dpi, mAppUnitsPerDevNotScaledPixel);

    UpdateScaledAppUnits();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::Init(nsNativeWidget aWidget)
{
    mWidget = aWidget;

    SetDPI();


#if defined(MOZ_ENABLE_GTK2) && defined(MOZ_X11)
    if (getenv ("MOZ_X_SYNC")) {
        PR_LOG (gThebesGFXLog, PR_LOG_DEBUG, ("+++ Enabling XSynchronize\n"));
        XSynchronize (gdk_x11_get_default_xdisplay(), True);
        XSetErrorHandler(x11_error_handler);
    }

#endif

    mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContext(nsIView *aView,
                                              nsIRenderingContext *&aContext)
{
    // This is currently only called by the caret code
    NS_ENSURE_ARG_POINTER(aView);
    NS_PRECONDITION(aView->HasWidget(), "View has no widget!");

    nsCOMPtr<nsIWidget> widget;
    widget = aView->GetWidget();

    return CreateRenderingContext(widget, aContext);
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContext(nsIWidget *aWidget,
                                              nsIRenderingContext *&aContext)
{
    nsresult rv;

    aContext = nsnull;
    nsCOMPtr<nsIRenderingContext> pContext;
    rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
    if (NS_SUCCEEDED(rv)) {
        nsRefPtr<gfxASurface> surface(aWidget->GetThebesSurface());
        if (surface)
            rv = pContext->Init(this, surface);
        else
            rv = NS_ERROR_FAILURE;

        if (NS_SUCCEEDED(rv)) {
            aContext = pContext;
            NS_ADDREF(aContext);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContext(nsIRenderingContext *&aContext)
{
    nsresult rv = NS_OK;

    aContext = nsnull;
    nsCOMPtr<nsIRenderingContext> pContext;
    rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
    if (NS_SUCCEEDED(rv)) {
        if (mPrintingSurface)
            rv = pContext->Init(this, mPrintingSurface);
        else
            rv = NS_ERROR_FAILURE;

        if (NS_SUCCEEDED(rv)) {
            pContext->Scale(mPrintingScale, mPrintingScale);
            aContext = pContext;
            NS_ADDREF(aContext);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::CreateRenderingContextInstance(nsIRenderingContext *&aContext)
{
    nsCOMPtr<nsIRenderingContext> renderingContext = new nsThebesRenderingContext();
    if (!renderingContext)
        return NS_ERROR_OUT_OF_MEMORY;

    aContext = renderingContext;
    NS_ADDREF(aContext);

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
    aSupportsWidgets = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::ClearCachedSystemFonts()
{
    //clear our cache of stored system fonts
    if (gSystemFonts) {
        delete gSystemFonts;
        gSystemFonts = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
    if (!gSystemFonts) {
#ifdef MOZ_ENABLE_GTK2
        gSystemFonts = new nsSystemFontsGTK2();
#elif XP_WIN
        gSystemFonts = new nsSystemFontsWin();
#elif XP_OS2
        gSystemFonts = new nsSystemFontsOS2();
#elif defined(XP_BEOS)
        gSystemFonts = new nsSystemFontsBeOS();
#elif XP_MACOSX
        gSystemFonts = new nsSystemFontsMac();
#elif defined(MOZ_WIDGET_QT)
        gSystemFonts = new nsSystemFontsQt();
#else
#error Need to know how to create gSystemFonts, fix me!
#endif
    }

    nsString fontName;
    gfxFontStyle fontStyle;
    nsresult rv = gSystemFonts->GetSystemFont(aID, &fontName, &fontStyle);
    NS_ENSURE_SUCCESS(rv, rv);

    aFont->name = fontName;
    aFont->style = fontStyle.style;
    aFont->systemFont = fontStyle.systemFont;
    aFont->variant = NS_FONT_VARIANT_NORMAL;
    aFont->familyNameQuirks = fontStyle.familyNameQuirks;
    aFont->weight = fontStyle.weight;
    aFont->decorations = NS_FONT_DECORATION_NONE;
    aFont->size = NSFloatPixelsToAppUnits(fontStyle.size, UnscaledAppUnitsPerDevPixel());
    //aFont->langGroup = fontStyle.langGroup;
    aFont->sizeAdjust = fontStyle.sizeAdjust;

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::CheckFontExistence(const nsString& aFaceName)
{
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetDepth(PRUint32& aDepth)
{
    nsCOMPtr<nsIScreen> primaryScreen;
    if (mDepth == 0) {
        mScreenManager->GetPrimaryScreen(getter_AddRefs(primaryScreen));
        primaryScreen->GetColorDepth(reinterpret_cast<PRInt32 *>(&mDepth));
    }

    aDepth = mDepth;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
    aPaletteInfo.isPaletteDevice = PR_FALSE;
    aPaletteInfo.sizePalette = 0;
    aPaletteInfo.numReserved = 0;
    aPaletteInfo.palette = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
    aPixel = aColor;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetDeviceSurfaceDimensions(nscoord &aWidth, nscoord &aHeight)
{
    if (mPrintingSurface) {
        // we have a printer device
        aWidth = mWidth;
        aHeight = mHeight;
    } else {
        nsRect area;
        ComputeFullAreaUsingScreen(&area);
        aWidth = area.width;
        aHeight = area.height;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetRect(nsRect &aRect)
{
    if (mPrintingSurface) {
        // we have a printer device
        aRect.x = 0;
        aRect.y = 0;
        aRect.width = mWidth;
        aRect.height = mHeight;
    } else
        ComputeFullAreaUsingScreen ( &aRect );

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetClientRect(nsRect &aRect)
{
    if (mPrintingSurface) {
        // we have a printer device
        aRect.x = 0;
        aRect.y = 0;
        aRect.width = mWidth;
        aRect.height = mHeight;
    }
    else
        ComputeClientRectUsingScreen(&aRect);

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::PrepareNativeWidget(nsIWidget* aWidget, void** aOut)
{
    *aOut = nsnull;
    return NS_OK;
}


/*
 * below methods are for printing
 */
NS_IMETHODIMP
nsThebesDeviceContext::InitForPrinting(nsIDeviceContextSpec *aDevice)
{
    NS_ENSURE_ARG_POINTER(aDevice);

    mDeviceContextSpec = aDevice;

    nsresult rv = aDevice->GetSurfaceForPrinter(getter_AddRefs(mPrintingSurface));
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    Init(nsnull);

    CalcPrintingSize();

    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::PrepareDocument(PRUnichar * aTitle, 
                                       PRUnichar*  aPrintToFileName)
{
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::BeginDocument(PRUnichar*  aTitle, 
                                     PRUnichar*  aPrintToFileName,
                                     PRInt32     aStartPage, 
                                     PRInt32     aEndPage)
{
    static const PRUnichar kEmpty[] = { '\0' };
    nsresult rv;

    rv = mPrintingSurface->BeginPrinting(nsDependentString(aTitle ? aTitle : kEmpty),
                                         nsDependentString(aPrintToFileName ? aPrintToFileName : kEmpty));

    if (NS_SUCCEEDED(rv) && mDeviceContextSpec)
        rv = mDeviceContextSpec->BeginDocument(aTitle, aPrintToFileName, aStartPage, aEndPage);

    return rv;
}


NS_IMETHODIMP
nsThebesDeviceContext::EndDocument(void)
{
    nsresult rv = NS_OK;

    if (mPrintingSurface) {
        rv = mPrintingSurface->EndPrinting();
        if (NS_SUCCEEDED(rv))
            mPrintingSurface->Finish();
    }

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndDocument();

    return rv;
}


NS_IMETHODIMP
nsThebesDeviceContext::AbortDocument(void)
{
    nsresult rv = mPrintingSurface->AbortPrinting();

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndDocument();

    return rv;
}


NS_IMETHODIMP
nsThebesDeviceContext::BeginPage(void)
{
    nsresult rv = NS_OK;

    if (mDeviceContextSpec)
        rv = mDeviceContextSpec->BeginPage();

    if (NS_FAILED(rv)) return rv;

   /* We need to get a new surface for each page on the Mac */
#ifdef XP_MACOSX
    mDeviceContextSpec->GetSurfaceForPrinter(getter_AddRefs(mPrintingSurface));
#endif
    rv = mPrintingSurface->BeginPage();

    return rv;
}

NS_IMETHODIMP
nsThebesDeviceContext::EndPage(void)
{
    nsresult rv = mPrintingSurface->EndPage();

    /* We need to release the CGContextRef in the surface here, plus it's
       not something you would want anyway, as these CGContextRefs are only good
       for one page. */
#ifdef XP_MACOSX
    mPrintingSurface = nsnull;
#endif

    if (mDeviceContextSpec)
        mDeviceContextSpec->EndPage();

    return rv;
}

/** End printing methods **/

void
nsThebesDeviceContext::ComputeClientRectUsingScreen(nsRect* outRect)
{
    // we always need to recompute the clientRect
    // because the window may have moved onto a different screen. In the single
    // monitor case, we only need to do the computation if we haven't done it
    // once already, and remember that we have because we're assured it won't change.
    nsCOMPtr<nsIScreen> screen;
    FindScreen (getter_AddRefs(screen));
    if (screen) {
        PRInt32 x, y, width, height;
        screen->GetAvailRect(&x, &y, &width, &height);
        
        // convert to device units
        outRect->y = NSIntPixelsToAppUnits(y, AppUnitsPerDevPixel());
        outRect->x = NSIntPixelsToAppUnits(x, AppUnitsPerDevPixel());
        outRect->width = NSIntPixelsToAppUnits(width, AppUnitsPerDevPixel());
        outRect->height = NSIntPixelsToAppUnits(height, AppUnitsPerDevPixel());
    }
}

void
nsThebesDeviceContext::ComputeFullAreaUsingScreen(nsRect* outRect)
{
    // if we have more than one screen, we always need to recompute the clientRect
    // because the window may have moved onto a different screen. In the single
    // monitor case, we only need to do the computation if we haven't done it
    // once already, and remember that we have because we're assured it won't change.
    nsCOMPtr<nsIScreen> screen;
    FindScreen ( getter_AddRefs(screen) );
    if ( screen ) {
        PRInt32 x, y, width, height;
        screen->GetRect ( &x, &y, &width, &height );
        
        // convert to device units
        outRect->y = NSIntPixelsToAppUnits(y, AppUnitsPerDevPixel());
        outRect->x = NSIntPixelsToAppUnits(x, AppUnitsPerDevPixel());
        outRect->width = NSIntPixelsToAppUnits(width, AppUnitsPerDevPixel());
        outRect->height = NSIntPixelsToAppUnits(height, AppUnitsPerDevPixel());
        
        mWidth = outRect->width;
        mHeight = outRect->height;
    }
    
}


//
// FindScreen
//
// Determines which screen intersects the largest area of the given surface.
//
void
nsThebesDeviceContext::FindScreen(nsIScreen** outScreen)
{
    if (mWidget)
        mScreenManager->ScreenForNativeWidget(mWidget, outScreen);
    else
        mScreenManager->GetPrimaryScreen(outScreen);
}

void
nsThebesDeviceContext::CalcPrintingSize()
{
    if (!mPrintingSurface)
        return;

    PRBool inPoints = PR_TRUE;

    gfxSize size;
    switch (mPrintingSurface->GetType()) {
    case gfxASurface::SurfaceTypeImage:
        inPoints = PR_FALSE;
        size = reinterpret_cast<gfxImageSurface*>(mPrintingSurface.get())->GetSize();
        break;

#if defined(MOZ_ENABLE_GTK2) || defined(XP_WIN) || defined(XP_OS2)
    case gfxASurface::SurfaceTypePDF:
        inPoints = PR_TRUE;
        size = reinterpret_cast<gfxPDFSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef MOZ_ENABLE_GTK2
    case gfxASurface::SurfaceTypePS:
        inPoints = PR_TRUE;
        size = reinterpret_cast<gfxPSSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef XP_MACOSX
    case gfxASurface::SurfaceTypeQuartz:
        inPoints = PR_TRUE; // this is really only true when we're printing
        size = reinterpret_cast<gfxQuartzSurface*>(mPrintingSurface.get())->GetSize();
        break;
#endif

#ifdef XP_WIN
    case gfxASurface::SurfaceTypeWin32:
    case gfxASurface::SurfaceTypeWin32Printing:
    {
        inPoints = PR_FALSE;
        HDC dc =  GetPrintHDC();
        if (!dc)
            dc = GetDC((HWND)mWidget);
        size.width = NSFloatPixelsToAppUnits(::GetDeviceCaps(dc, HORZRES)/mPrintingScale, AppUnitsPerDevPixel());
        size.height = NSFloatPixelsToAppUnits(::GetDeviceCaps(dc, VERTRES)/mPrintingScale, AppUnitsPerDevPixel());
        mDepth = (PRUint32)::GetDeviceCaps(dc, BITSPIXEL);
        if (dc != (HDC)GetPrintHDC())
            ReleaseDC((HWND)mWidget, dc);
        break;
    }
#endif

#ifdef XP_OS2
    case gfxASurface::SurfaceTypeOS2:
    {
        inPoints = PR_FALSE;
        // we already set the size in the surface constructor we set for
        // printing, so just get those values here
        size = reinterpret_cast<gfxOS2Surface*>(mPrintingSurface.get())->GetSize();
        // as they are in pixels we need to scale them to app units
        size.width = NSFloatPixelsToAppUnits(size.width, AppUnitsPerDevPixel());
        size.height = NSFloatPixelsToAppUnits(size.height, AppUnitsPerDevPixel());
        // still need to get the depth from the device context
        HDC dc = GetPrintHDC();
        LONG value;
        if (DevQueryCaps(dc, CAPS_COLOR_BITCOUNT, 1, &value))
            mDepth = value;
        else
            mDepth = 8; // default to 8bpp, should be enough for printers
        break;
    }
#endif
    default:
        NS_ASSERTION(0, "trying to print to unknown surface type");
    }

    if (inPoints) {
        mWidth = NSToCoordRound(float(size.width) * AppUnitsPerInch() / 72);
        mHeight = NSToCoordRound(float(size.height) * AppUnitsPerInch() / 72);
    } else {
        mWidth = NSToIntRound(size.width);
        mHeight = NSToIntRound(size.height);
    }
}

PRBool nsThebesDeviceContext::CheckDPIChange() {
    PRInt32 oldDevPixels = mAppUnitsPerDevNotScaledPixel;
    PRInt32 oldInches = mAppUnitsPerInch;

    SetDPI();

    return oldDevPixels != mAppUnitsPerDevNotScaledPixel ||
           oldInches != mAppUnitsPerInch;
}

PRBool
nsThebesDeviceContext::SetPixelScale(float aScale)
{
    if (aScale <= 0) {
        NS_NOTREACHED("Invalid pixel scale value");
        return PR_FALSE;
    }
    PRInt32 oldAppUnitsPerDevPixel = mAppUnitsPerDevPixel;
    mPixelScale = aScale;
    UpdateScaledAppUnits();
    return oldAppUnitsPerDevPixel != mAppUnitsPerDevPixel;
}

void
nsThebesDeviceContext::UpdateScaledAppUnits()
{
    mAppUnitsPerDevPixel = PR_MAX(1, PRInt32(float(mAppUnitsPerDevNotScaledPixel) / mPixelScale));
}

#if defined(XP_WIN) || defined(XP_OS2)
HDC
nsThebesDeviceContext::GetPrintHDC()
{
    if (mPrintingSurface) {
        switch (mPrintingSurface->GetType()) {
#ifdef XP_WIN
            case gfxASurface::SurfaceTypeWin32:
            case gfxASurface::SurfaceTypeWin32Printing:
                return reinterpret_cast<gfxWindowsSurface*>(mPrintingSurface.get())->GetDC();
#endif

#ifdef XP_OS2
            case gfxASurface::SurfaceTypeOS2:
                return GpiQueryDevice(reinterpret_cast<gfxOS2Surface*>(mPrintingSurface.get())->GetPS());
#endif

            default:
                NS_ASSERTION(0, "invalid surface type in GetPrintHDC");
                break;
        }
    }

    return nsnull;
}
#endif
