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
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "gfxContext.h"

#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gThebesGFXLog;
#endif

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#elif defined(XP_OS2)
#include "gfxOS2Surface.h"
#endif

class nsHashtable;
class nsFontCache;

class nsThebesDeviceContext : public nsIDeviceContext,
                              public nsIObserver,
                              public nsSupportsWeakReference
{
public:
    nsThebesDeviceContext();
    virtual ~nsThebesDeviceContext();

    static void Shutdown();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    NS_IMETHOD Init(nsIWidget *aWidget);
    NS_IMETHOD InitForPrinting(nsIDeviceContextSpec *aDevSpec);
    NS_IMETHOD CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContext(nsIRenderingContext *&aContext);
    NS_IMETHOD CreateRenderingContextInstance(nsIRenderingContext *&aContext);

    NS_IMETHOD GetMetricsFor(const nsFont& aFont, nsIAtom* aLanguage,
                             gfxUserFontSet* aUserFontSet,
                             nsIFontMetrics*& aMetrics);
    NS_IMETHOD GetMetricsFor(const nsFont& aFont,
                             gfxUserFontSet* aUserFontSet,
                             nsIFontMetrics*& aMetrics);

    NS_IMETHOD FirstExistingFont(const nsFont& aFont, nsString& aFaceName);

    NS_IMETHOD GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                                PRBool& aAliased);

    NS_IMETHOD CreateFontCache();
    NS_IMETHOD FontMetricsDeleted(const nsIFontMetrics* aFontMetrics);
    NS_IMETHOD FlushFontCache(void);

    NS_IMETHOD PrepareNativeWidget(nsIWidget *aWidget, void **aOut);

    NS_IMETHOD GetSystemFont(nsSystemFontID aID, nsFont *aFont) const;
    NS_IMETHOD ClearCachedSystemFonts();

    NS_IMETHOD CheckFontExistence(const nsString& aFaceName);

    NS_IMETHOD GetDepth(PRUint32& aDepth);

    NS_IMETHOD GetDeviceSurfaceDimensions(nscoord& aWidth, nscoord& aHeight);
    NS_IMETHOD GetRect(nsRect& aRect);
    NS_IMETHOD GetClientRect(nsRect& aRect);

    /* printing goop */
    NS_IMETHOD PrepareDocument(PRUnichar *aTitle, 
                               PRUnichar *aPrintToFileName);

    NS_IMETHOD BeginDocument(PRUnichar  *aTitle, 
                             PRUnichar  *aPrintToFileName,
                             PRInt32     aStartPage, 
                             PRInt32     aEndPage);

    NS_IMETHOD EndDocument(void);
    NS_IMETHOD AbortDocument(void);
    NS_IMETHOD BeginPage(void);
    NS_IMETHOD EndPage(void);
    /* end printing goop */

    virtual PRBool CheckDPIChange();

    virtual PRBool SetPixelScale(float aScale);

    PRBool IsPrinterSurface(void);

#if defined(XP_WIN) || defined(XP_OS2)
    HDC GetPrintHDC();
#endif

protected:
    virtual nsresult CreateFontAliasTable();
    nsresult AliasFont(const nsString& aFont, 
                       const nsString& aAlias, const nsString& aAltAlias,
                       PRBool aForceAlias);
    void GetLocaleLanguage(void);
    nsresult SetDPI();
    void ComputeClientRectUsingScreen(nsRect *outRect);
    void ComputeFullAreaUsingScreen(nsRect *outRect);
    void FindScreen(nsIScreen **outScreen);
    void CalcPrintingSize();
    void UpdateScaledAppUnits();

    PRUint32          mDepth;
    nsFontCache*      mFontCache;
    nsCOMPtr<nsIAtom> mLocaleLanguage; // XXX temp fix for performance bug
    nsHashtable*      mFontAliasTable;
    nsIWidget*        mWidget;

private:
    nsCOMPtr<nsIScreenManager> mScreenManager;

    nscoord mWidth;
    nscoord mHeight;

    nsRefPtr<gfxASurface> mPrintingSurface;
    float mPrintingScale;
    nsCOMPtr<nsIDeviceContextSpec> mDeviceContextSpec;
};

#endif /* _NS_CAIRODEVICECONTEXT_H_ */

