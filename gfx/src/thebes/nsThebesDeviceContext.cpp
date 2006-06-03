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
#include "nsThebesDrawingSurface.h"

#include "nsIView.h"

#ifdef MOZ_ENABLE_GTK2
// for getenv
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "nsFont.h"

#include <pango/pango.h>
#include <pango/pangox.h>
#include <pango/pango-fontmap.h>
#endif /* GTK2 */

#ifdef MOZ_ENABLE_GTK2
#include "nsSystemFontsGTK2.h"
#include "gfxPDFSurface.h"
#include "gfxPSSurface.h"
static nsSystemFontsGTK2 *gSystemFonts = nsnull;
#elif XP_WIN
#include <cairo-win32.h>
#include "nsSystemFontsWin.h"
#include "gfxWindowsSurface.h"
static nsSystemFontsWin *gSystemFonts = nsnull;
#include <usp10.h>
#elif defined(XP_BEOS)
#include "nsSystemFontsBeOS.h"
static nsSystemFontsBeOS *gSystemFonts = nsnull;
#elif XP_MACOSX
#include "nsSystemFontsMac.h"
static nsSystemFontsMac *gSystemFonts = nsnull;
#else
#error Need to declare gSystemFonts!
#endif

#ifdef MOZ_ENABLE_GTK2
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

    mDevUnitsToAppUnits = 1.0f;
    mAppUnitsToDevUnits = 1.0f;
    mCPixelScale = 1.0f;
    mZoom = 1.0f;
    mDepth = 0;
    mDpi = -1;

    mWidget = nsnull;
    mPrinter = PR_FALSE;

    mWidgetSurfaceCache.Init();

#ifdef XP_WIN
    SCRIPT_DIGITSUBSTITUTE sds;
    ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &sds);
#endif
}

nsThebesDeviceContext::~nsThebesDeviceContext()
{
    nsresult rv;
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        prefs->UnregisterCallback("layout.css.dpi",
                                  prefChanged, (void *)this);
    }
}

nsresult
nsThebesDeviceContext::SetDPI(PRInt32 aPrefDPI)
{
    PRInt32 OSVal;
    PRBool do_round = PR_TRUE;

    mDpi = 96;

#if defined(MOZ_ENABLE_GTK2)
    float screenWidthIn = float(::gdk_screen_width_mm()) / 25.4f;
    OSVal = NSToCoordRound(float(::gdk_screen_width()) / screenWidthIn);

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

    if (mPrinter) {
        // cairo printing doesn't really have the
        // notion of DPI so we have to use 72...
        // XXX is this an issue? we force everything else to be 96+
        mDpi = 72;
        do_round = PR_FALSE;
    }

#elif defined(XP_WIN)
    // XXX we should really look at the widget for printing and such, but this widget is currently always null...
    HDC dc = GetHDC() ? GetHDC() : GetDC((HWND)nsnull);

    OSVal = GetDeviceCaps(dc, LOGPIXELSY);
    if (GetDeviceCaps(dc, TECHNOLOGY) != DT_RASDISPLAY)
        do_round = PR_FALSE;

    if (dc != GetHDC())
        ReleaseDC((HWND)nsnull, dc);

    if (OSVal != 0)
        mDpi = OSVal;
#endif

    int in2pt = 72;

    // make p2t a nice round number - this prevents rounding problems
    mPixelsToTwips = float(NSIntPointsToTwips(in2pt)) / float(mDpi);
    if (do_round)
        mPixelsToTwips = float(NSToIntRound(mPixelsToTwips));
    mTwipsToPixels = 1.0f / mPixelsToTwips;

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::Init(nsNativeWidget aWidget)
{
    mWidget = aWidget;

    PRInt32 prefVal = -1;

    // Set prefVal the value of the preference
    // "layout.css.dpi"
    // or -1 if we can't get it.
    // If it's negative, we pretend it's not set.
    // If it's 0, it means force use of the operating system's logical
    // resolution.
    // If it's positive, we use it as the logical resolution
    nsresult res;

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &res));
    if (NS_SUCCEEDED(res) && prefs) {
        res = prefs->GetIntPref("layout.css.dpi", &prefVal);
        if (NS_FAILED(res)) {
            prefVal = -1;
        }
        prefs->RegisterCallback("layout.css.dpi", prefChanged,
                                (void *)this);
    }

    SetDPI(prefVal);

#ifdef MOZ_ENABLE_GTK2
    if (getenv ("MOZ_X_SYNC")) {
        PR_LOG (gThebesGFXLog, PR_LOG_DEBUG, ("+++ Enabling XSynchronize\n"));
        XSynchronize (gdk_x11_get_default_xdisplay(), True);
        XSetErrorHandler(x11_error_handler);
    }

    // XXX
    if (mPrinter) {
        // XXX this should use a gfxAPrintingSurface or somesuch cast.
        // currently PDF and PS are identical here
        gfxSize size = ((gfxPSSurface*)(mPrintingSurface.get()))->GetSize();
        mWidth = NSFloatPointsToTwips(size.width);
        mHeight = NSFloatPointsToTwips(size.height);
        printf("%f %f\n", size.width, size.height);
        printf("%d %d\n", (PRInt32)mWidth, (PRInt32)mHeight);
    }
    // XXX
    mDepth = 24;
#endif

#ifdef XP_WIN
    HDC dc =  GetHDC() ? GetHDC() : GetDC((HWND)mWidget);
    mWidth = ::GetDeviceCaps(dc, HORZRES);
    mHeight = ::GetDeviceCaps(dc, VERTRES);
    mDepth = (PRUint32)::GetDeviceCaps(dc, BITSPIXEL);
    if (dc != (HDC)GetHDC())
        ReleaseDC((HWND)mWidget, dc);
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
nsThebesDeviceContext::CreateRenderingContext(nsIDrawingSurface *aSurface,
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
            aContext = pContext;
            NS_ADDREF(aContext);
        }
    }

    return rv;


    return NS_ERROR_NOT_IMPLEMENTED;
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
nsThebesDeviceContext::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
#ifdef XP_WIN
    float scale;
    GetCanonicalPixelScale(scale);

    aWidth  = ::GetSystemMetrics(SM_CXVSCROLL) * mDevUnitsToAppUnits * scale;
    aHeight = ::GetSystemMetrics(SM_CXHSCROLL) * mDevUnitsToAppUnits * scale;

#else

    aWidth = 10.0f * mPixelsToTwips;
    aHeight = 10.0f * mPixelsToTwips;
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
    nsresult status = NS_OK;

    if (!gSystemFonts) {
#ifdef MOZ_ENABLE_GTK2
        gSystemFonts = new nsSystemFontsGTK2(mPixelsToTwips);
#elif XP_WIN
        gSystemFonts = new nsSystemFontsWin(mPixelsToTwips);
#elif defined(XP_BEOS)
        gSystemFonts = new nsSystemFontsBeOS(mPixelsToTwips);
#elif XP_MACOSX
        gSystemFonts = new nsSystemFontsMac(mPixelsToTwips);
#else
#error Need to know how to create gSystemFonts, fix me!
#endif
    }

    return gSystemFonts->GetSystemFont(aID, aFont);
}

NS_IMETHODIMP
nsThebesDeviceContext::CheckFontExistence(const nsString& aFaceName)
{
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetDepth(PRUint32& aDepth)
{
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
nsThebesDeviceContext::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
    if (mPrinter) {
        // we have a printer device
#ifdef MOZ_ENABLE_GTK2
        aWidth = mWidth;
        aHeight = mHeight;
#else
        aWidth = NSToIntRound(mWidth * mDevUnitsToAppUnits);
        aHeight = NSToIntRound(mHeight * mDevUnitsToAppUnits);
#endif
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
    if (mPrinter) {
        // we have a printer device
        aRect.x = 0;
        aRect.y = 0;
#ifdef MOZ_ENABLE_GTK2
        aRect.width = NSToIntRound(mWidth);
        aRect.height = NSToIntRound(mHeight);
#else
        aRect.width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
        aRect.height = NSToIntRound(mHeight * mDevUnitsToAppUnits);
#endif
    } else
        ComputeFullAreaUsingScreen ( &aRect );

    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::GetClientRect(nsRect &aRect)
{
    if (mPrinter) {
        // we have a printer device
        aRect.x = 0;
        aRect.y = 0;
#ifdef MOZ_ENABLE_GTK2
        aRect.width = NSToIntRound(mWidth);
        aRect.height = NSToIntRound(mHeight);
#else
        aRect.width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
        aRect.height = NSToIntRound(mHeight * mDevUnitsToAppUnits);
#endif
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
 * below methods are for printing and are not implemented
 */
NS_IMETHODIMP
nsThebesDeviceContext::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                           nsIDeviceContext *&aContext)
{
    nsThebesDeviceContext *newDevCon = new nsThebesDeviceContext();

    if (newDevCon) {
        // this will ref count it
        nsresult rv = newDevCon->QueryInterface(NS_GET_IID(nsIDeviceContext), (void**)&aContext);
        NS_ASSERTION(NS_SUCCEEDED(rv), "This has to support nsIDeviceContext");
    } else {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    NS_ADDREF(aDevice);

    newDevCon->mPrinter = PR_TRUE;

    aDevice->GetSurfaceForPrinter(getter_AddRefs(newDevCon->mPrintingSurface));

    newDevCon->Init(nsnull);

    float newscale = newDevCon->TwipsToDevUnits();
    float origscale = this->TwipsToDevUnits();

    newDevCon->SetCanonicalPixelScale(newscale / origscale);

    float t2d = this->TwipsToDevUnits();
    float a2d = this->AppUnitsToDevUnits();

    newDevCon->SetAppUnitsToDevUnits((a2d / t2d) * newDevCon->mTwipsToPixels);
    newDevCon->SetDevUnitsToAppUnits(1.0f / newDevCon->mAppUnitsToDevUnits);

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
    nsRefPtr<gfxContext> thebes = new gfxContext(mPrintingSurface);
    thebes->BeginPrinting(nsDependentString(aTitle ? aTitle : kEmpty),
                          nsDependentString(aPrintToFileName ? aPrintToFileName : kEmpty));
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::EndDocument(void)
{
    nsRefPtr<gfxContext> thebes = new gfxContext(mPrintingSurface);
    thebes->EndPrinting();
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::AbortDocument(void)
{
    nsRefPtr<gfxContext> thebes = new gfxContext(mPrintingSurface);
    thebes->AbortPrinting();
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::BeginPage(void)
{
    nsRefPtr<gfxContext> thebes = new gfxContext(mPrintingSurface);
    thebes->BeginPage();
    return NS_OK;
}

NS_IMETHODIMP
nsThebesDeviceContext::EndPage(void)
{
    nsRefPtr<gfxContext> thebes = new gfxContext(mPrintingSurface);
    thebes->EndPage();
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::SetAltDevice(nsIDeviceContext* aAltDC)
{
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::GetAltDevice(nsIDeviceContext** aAltDC)
{
    *aAltDC = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsThebesDeviceContext::SetUseAltDC(PRUint8 aValue, PRBool aOn)
{
    return NS_OK;
}

/** End printing methods **/

int
nsThebesDeviceContext::prefChanged(const char *aPref, void *aClosure)
{
    nsThebesDeviceContext *context = (nsThebesDeviceContext*)aClosure;
    nsresult rv;
  
    if (nsCRT::strcmp(aPref, "layout.css.dpi") == 0) {
        PRInt32 dpi;
        nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
        rv = prefs->GetIntPref(aPref, &dpi);
        if (NS_SUCCEEDED(rv))
            context->SetDPI(dpi);
    }

    return 0;
}


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
        outRect->y = NSToIntRound(y * mDevUnitsToAppUnits);
        outRect->x = NSToIntRound(x * mDevUnitsToAppUnits);
        outRect->width = NSToIntRound(width * mDevUnitsToAppUnits);
        outRect->height = NSToIntRound(height * mDevUnitsToAppUnits);
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
        outRect->y = NSToIntRound(y * mDevUnitsToAppUnits);
        outRect->x = NSToIntRound(x * mDevUnitsToAppUnits);
        outRect->width = NSToIntRound(width * mDevUnitsToAppUnits);
        outRect->height = NSToIntRound(height * mDevUnitsToAppUnits);
        
        mWidth = width;
        mHeight = height;
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
    // now then, if we have more than one screen, we need to find which screen this
    // window is on.
#ifdef XP_WIN
    HWND window = NS_REINTERPRET_CAST(HWND, mWidget);
    if (window) {
        RECT globalPosition;
        ::GetWindowRect(window, &globalPosition); 
        if (mScreenManager) {
            mScreenManager->ScreenForRect(globalPosition.left, globalPosition.top, 
                                          globalPosition.right - globalPosition.left,
                                          globalPosition.bottom - globalPosition.top,
                                          outScreen);
            return;
        }
    }
#endif

#ifdef MOZ_ENABLE_GTK2
    gint x, y, width, height, depth;
    x = y = width = height = 0;

    gdk_window_get_geometry(GDK_WINDOW(mWidget), &x, &y, &width, &height,
                            &depth);
    gdk_window_get_origin(GDK_WINDOW(mWidget), &x, &y);
    if (mScreenManager) {
        mScreenManager->ScreenForRect(x, y, width, height, outScreen);
        return;
    }

#endif

#ifdef XP_MACOSX
    // ???
#endif

    mScreenManager->GetPrimaryScreen(outScreen);
}
