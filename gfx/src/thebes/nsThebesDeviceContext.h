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

#ifndef _NS_THEBESDEVICECONTEXT_H_
#define _NS_THEBESDEVICECONTEXT_H_

#include "nsIScreenManager.h"

#include "nsDeviceContext.h"

#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "prlog.h"

#include "gfxContext.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gThebesGFXLog;
#endif

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#endif

class nsThebesDeviceContext : public DeviceContextImpl
{
public:
    nsThebesDeviceContext();
    virtual ~nsThebesDeviceContext();

    NS_DECL_ISUPPORTS_INHERITED

    NS_IMETHOD Init(nsNativeWidget aWidget);
    NS_IMETHOD InitForPrinting(nsIDeviceContextSpec* aDevSpec);
    NS_IMETHOD CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext);

    NS_IMETHOD CreateRenderingContext(nsIDrawingSurface *aSurface, nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContext(nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContextInstance(nsIRenderingContext *&aContext);

    NS_IMETHOD SupportsNativeWidgets(PRBool &aSupportsWidgets);
    NS_IMETHOD PrepareNativeWidget(nsIWidget* aWidget, void** aOut);

    NS_IMETHOD GetSystemFont(nsSystemFontID aID, nsFont *aFont) const;

    NS_IMETHOD CheckFontExistence(const nsString& aFaceName);

    NS_IMETHOD GetDepth(PRUint32& aDepth);

    NS_IMETHOD GetPaletteInfo(nsPaletteInfo& aPaletteInfo);

    NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel);

    NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
    NS_IMETHOD GetRect(nsRect &aRect);
    NS_IMETHOD GetClientRect(nsRect &aRect);

    /* printing goop */
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
    /* end printing goop */

    static void DebugShowCairoSurface (const char *aName, cairo_surface_t *aSurface);

    virtual PRBool CheckDPIChange();

    nsNativeWidget GetWidget() { return mWidget; }
#ifdef XP_WIN
    HDC GetHDC() {
        if (mPrintingSurface) {
            NS_ASSERTION(mPrintingSurface->GetType() == gfxASurface::SurfaceTypeWin32, "invalid surface type");
            return reinterpret_cast<gfxWindowsSurface*>(mPrintingSurface.get())->GetDC();
        }
        return nsnull;
    }
#endif

protected:
    nsresult SetDPI();
    void ComputeClientRectUsingScreen(nsRect* outRect);
    void ComputeFullAreaUsingScreen(nsRect* outRect);
    void FindScreen(nsIScreen** outScreen);
    void CalcPrintingSize();

    PRUint32 mDepth;

private:
    nsCOMPtr<nsIScreenManager> mScreenManager;

    nscoord mWidth;
    nscoord mHeight;
    PRBool mPrinter;

    nsRefPtrHashtable<nsISupportsHashKey, gfxASurface> mWidgetSurfaceCache;

    nsRefPtr<gfxASurface> mPrintingSurface;
    nsIDeviceContextSpec * mDeviceContextSpec;
};

#endif /* _NS_CAIRODEVICECONTEXT_H_ */

