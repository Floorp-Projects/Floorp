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

#ifndef _NS_CAIRODEVICECONTEXT_H_
#define _NS_CAIRODEVICECONTEXT_H_

#include "nsIScreenManager.h"

#include "nsDeviceContext.h"

#include <cairo.h>

#ifdef MOZ_ENABLE_XLIB
#include "xlibrgb.h"
#endif

class nsCairoDeviceContext : public DeviceContextImpl
{
public:
    nsCairoDeviceContext();
    virtual ~nsCairoDeviceContext();

    NS_DECL_ISUPPORTS_INHERITED

    NS_IMETHOD Init(nsNativeWidget aWidget);
    NS_IMETHOD CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext);

    NS_IMETHOD CreateRenderingContext(nsIDrawingSurface *aSurface, nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContext(nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContextInstance(nsIRenderingContext *&aContext);

    NS_IMETHOD SupportsNativeWidgets(PRBool &aSupportsWidgets);

    NS_IMETHOD GetScrollBarDimensions(float &aWidth, float &aHeight) const;

    NS_IMETHOD GetSystemFont(nsSystemFontID aID, nsFont *aFont) const;

    NS_IMETHOD CheckFontExistence(const nsString& aFaceName);

    NS_IMETHOD GetDepth(PRUint32& aDepth);

    NS_IMETHOD GetPaletteInfo(nsPaletteInfo& aPaletteInfo);

    NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel);

    NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
    NS_IMETHOD GetRect(nsRect &aRect);
    NS_IMETHOD GetClientRect(nsRect &aRect);

    NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                   nsIDeviceContext *&aContext);

    NS_IMETHOD PrepareDocument(PRUnichar * aTitle, 
                               PRUnichar*  aPrintToFileName);

    NS_IMETHOD BeginDocument(PRUnichar*  aTitle, 
                             PRUnichar*  aPrintToFileName,
                             PRInt32     aStartPage, 
                             PRInt32     aEndPage);

    NS_IMETHOD EndDocument(void);
    NS_IMETHOD AbortDocument(void);
    NS_IMETHOD BeginPage(void);
    NS_IMETHOD EndPage(void);
    NS_IMETHOD SetAltDevice(nsIDeviceContext* aAltDC);
    NS_IMETHOD GetAltDevice(nsIDeviceContext** aAltDC);
    NS_IMETHOD SetUseAltDC(PRUint8 aValue, PRBool aOn);

#if defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_XLIB)
    Display *GetXDisplay();
    Visual *GetXVisual();
    Colormap GetXColormap();
    Drawable GetXPixmapParentDrawable();
#endif

private:
    nsNativeWidget mWidget;

    nsCOMPtr<nsIScreenManager> mScreenManager;

    float mWidthFloat;
    float mHeightFloat;
    PRInt32 mWidth;
    PRInt32 mHeight;

#if defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_XLIB)
    Drawable mPixmapParentDrawable;
#endif

#ifdef MOZ_ENABLE_XLIB
    XlibRgbHandle *mXlibRgbHandle;
#endif

};

#endif /* _NS_CAIRODEVICECONTEXT_H_ */

