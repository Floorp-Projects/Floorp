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
#endif

#ifdef MOZ_ENABLE_XLIB
#include <X11/Xlib.h>
#endif

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

NS_IMETHODIMP
nsCairoDeviceContext::Init(nsNativeWidget aWidget)
{
    //DeviceContextImpl::CommonInit();

    // mTwipsToPixels = 96 / (float)NSIntPointsToTwips(72);
    // mPixelsToTwips = 1.0f / mTwipsToPixels;
    mTwipsToPixels = 1.0f;
    mPixelsToTwips = 1.0f;

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
    NS_WARNING("GetSystemFont!");
    return NS_OK;
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

    return NS_OK;
}


NS_IMETHODIMP
nsCairoDeviceContext::GetClientRect(nsRect &aRect)
{
    return this->GetRect(aRect);
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
