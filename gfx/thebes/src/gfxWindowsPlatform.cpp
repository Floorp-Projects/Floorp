/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Masatoshi Kimura <VYV03354@nifty.ne.jp>
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

#include "gfxWindowsPlatform.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"

#include "nsUnicharUtils.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsIWindowsRegKey.h"
#include "nsILocalFile.h"
#include "plbase64.h"

#ifdef MOZ_FT2_FONTS
#include "ft2build.h"
#include FT_FREETYPE_H
#include "gfxFT2Fonts.h"
#include "gfxFT2FontList.h"
#include "cairo-ft.h"
#include "nsAppDirectoryServiceDefs.h"
#else
#include "gfxGDIFontList.h"
#include "gfxGDIFont.h"
#ifdef CAIRO_HAS_DWRITE_FONT
#include "gfxDWriteFontList.h"
#include "gfxDWriteFonts.h"
#include "gfxDWriteCommon.h"
#include <dwrite.h>
#endif
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"
#endif

#ifdef WINCE
#include <shlwapi.h>

#ifdef CAIRO_HAS_DDRAW_SURFACE
#include "gfxDDrawSurface.h"
#endif
#endif

#include "gfxUserFontSet.h"

#include <string>

#ifdef MOZ_FT2_FONTS
static FT_Library gPlatformFTLibrary = NULL;
#endif

#ifdef CAIRO_HAS_DWRITE_FONT
// DirectWrite is not available on all platforms, we need to use the function
// pointer.
typedef HRESULT (WINAPI*DWriteCreateFactoryFunc)(
  __in   DWRITE_FACTORY_TYPE factoryType,
  __in   REFIID iid,
  __out  IUnknown **factory
);
#endif

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

gfxWindowsPlatform::gfxWindowsPlatform()
{
    mPrefFonts.Init(50);

#ifdef MOZ_FT2_FONTS
    FT_Init_FreeType(&gPlatformFTLibrary);
#endif

/* Pick the default render mode differently between
 * desktop, Windows Mobile, and Windows CE.
 */
#if defined(WINCE_WINDOWS_MOBILE)
    mRenderMode = RENDER_IMAGE_DDRAW16;
#elif defined(WINCE)
    mRenderMode = RENDER_DDRAW_GL;
#else
    mRenderMode = RENDER_GDI;
#endif

    nsCOMPtr<nsIPrefBranch2> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);

#ifdef CAIRO_HAS_DWRITE_FONT
    nsresult rv;
    PRBool useDirectWrite = PR_FALSE;

    rv = pref->GetBoolPref(
        "gfx.font_rendering.directwrite.enabled", &useDirectWrite);
    if (NS_FAILED(rv)) {
        useDirectWrite = PR_FALSE;
    }
            
    if (useDirectWrite) {
        DWriteCreateFactoryFunc createDWriteFactory = (DWriteCreateFactoryFunc)
            GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory");

        if (createDWriteFactory) {
            /**
             * I need a direct pointer to be able to cast to IUnknown**, I also
             * need to remember to release this because the nsRefPtr will
             * AddRef it.
             */
            IDWriteFactory *factory;
            HRESULT hr = createDWriteFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&factory));
            mDWriteFactory = factory;
            factory->Release();
        }
    }
#endif

    PRInt32 rmode;
    if (NS_SUCCEEDED(pref->GetIntPref("mozilla.widget.render-mode", &rmode))) {
        if (rmode >= 0 && rmode < RENDER_MODE_MAX) {
#ifndef CAIRO_HAS_DDRAW_SURFACE
            if (rmode == RENDER_DDRAW || rmode == RENDER_DDRAW_GL)
                rmode = RENDER_IMAGE_STRETCH24;
#endif
            if (rmode == RENDER_DIRECT2D) {
#ifndef CAIRO_HAS_D2D_SURFACE
                return;
#else
                if (!cairo_d2d_has_support()) {
                    return;
                }
#ifdef CAIRO_HAS_DWRITE_FONT
                if (!GetDWriteFactory()) {
#endif
                    // D2D doesn't work without DirectWrite.
                    return;
#ifdef CAIRO_HAS_DWRITE_FONT
                }
#endif
#endif
            }
            mRenderMode = (RenderMode) rmode;
        }
    }
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
    // not calling FT_Done_FreeType because cairo may still hold references to
    // these FT_Faces.  See bug 458169.
}

gfxPlatformFontList*
gfxWindowsPlatform::CreatePlatformFontList()
{
#ifdef MOZ_FT2_FONTS
    return new gfxFT2FontList();
#else
#ifdef CAIRO_HAS_DWRITE_FONT
    if (!GetDWriteFactory()) {
#endif
        return new gfxGDIFontList();
#ifdef CAIRO_HAS_DWRITE_FONT
    } else {
        return new gfxDWriteFontList();
    }
#endif
#endif
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                           gfxASurface::gfxImageFormat imageFormat)
{
    gfxASurface *surf = nsnull;

#ifdef CAIRO_HAS_DDRAW_SURFACE
    if (mRenderMode == RENDER_DDRAW || mRenderMode == RENDER_DDRAW_GL)
        surf = new gfxDDrawSurface(NULL, size, imageFormat);
#endif

#ifdef CAIRO_HAS_WIN32_SURFACE
    if (mRenderMode == RENDER_GDI)
        surf = new gfxWindowsSurface(size, imageFormat);
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
    if (mRenderMode == RENDER_DIRECT2D)
        surf = new gfxD2DSurface(size, imageFormat);
#endif

    if (surf == nsnull)
        surf = new gfxImageSurface(size, imageFormat);

    NS_IF_ADDREF(surf);

    return surf;
}

nsresult
gfxWindowsPlatform::GetFontList(nsIAtom *aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup, aGenericFamily, aListOfFonts);

    return NS_OK;
}

static void
RemoveCharsetFromFontSubstitute(nsAString &aName)
{
    PRInt32 comma = aName.FindChar(PRUnichar(','));
    if (comma >= 0)
        aName.Truncate(comma);
}

nsresult
gfxWindowsPlatform::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();

    return NS_OK;
}

struct ResolveData {
    ResolveData(gfxPlatform::FontResolverCallback aCallback,
                gfxWindowsPlatform *aCaller, const nsAString *aFontName,
                void *aClosure) :
        mFoundCount(0), mCallback(aCallback), mCaller(aCaller),
        mFontName(aFontName), mClosure(aClosure) {}
    PRUint32 mFoundCount;
    gfxPlatform::FontResolverCallback mCallback;
    gfxWindowsPlatform *mCaller;
    const nsAString *mFontName;
    void *mClosure;
};

nsresult
gfxWindowsPlatform::ResolveFontName(const nsAString& aFontName,
                                    FontResolverCallback aCallback,
                                    void *aClosure,
                                    PRBool& aAborted)
{
    nsAutoString resolvedName;
    if (!gfxPlatformFontList::PlatformFontList()->
             ResolveFontName(aFontName, resolvedName)) {
        aAborted = PR_FALSE;
        return NS_OK;
    }
    aAborted = !(*aCallback)(resolvedName, aClosure);
    return NS_OK;
}

nsresult
gfxWindowsPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxFontGroup *
gfxWindowsPlatform::CreateFontGroup(const nsAString &aFamilies,
                                    const gfxFontStyle *aStyle,
                                    gfxUserFontSet *aUserFontSet)
{
#ifdef MOZ_FT2_FONTS
    return new gfxFT2FontGroup(aFamilies, aStyle);
#else
    return new gfxFontGroup(aFamilies, aStyle, aUserFontSet);
#endif
}

gfxFontEntry* 
gfxWindowsPlatform::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                    const nsAString& aFontName)
{
    return gfxPlatformFontList::PlatformFontList()->LookupLocalFont(aProxyEntry, 
                                                                    aFontName);
}

gfxFontEntry* 
gfxWindowsPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                     const PRUint8 *aFontData, PRUint32 aLength)
{
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aProxyEntry,
                                                                     aFontData,
                                                                     aLength);
}

PRBool
gfxWindowsPlatform::IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_WOFF     |
                        gfxUserFontSet::FLAG_FORMAT_OPENTYPE | 
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE)) {
        return PR_TRUE;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return PR_FALSE;
    }

    // no format hint set, need to look at data
    return PR_TRUE;
}

gfxFontFamily *
gfxWindowsPlatform::FindFontFamily(const nsAString& aName)
{
    return gfxPlatformFontList::PlatformFontList()->FindFamily(aName);
}

gfxFontEntry *
gfxWindowsPlatform::FindFontEntry(const nsAString& aName, const gfxFontStyle& aFontStyle)
{
    nsRefPtr<gfxFontFamily> ff = FindFontFamily(aName);
    if (!ff)
        return nsnull;

    PRBool aNeedsBold;
    return ff->FindFontForStyle(aFontStyle, aNeedsBold);
}

qcms_profile*
gfxWindowsPlatform::GetPlatformCMSOutputProfile()
{
#ifndef MOZ_FT2_FONTS
    WCHAR str[MAX_PATH];
    DWORD size = MAX_PATH;
    BOOL res;

    HDC dc = GetDC(nsnull);
    if (!dc)
        return nsnull;

#if _MSC_VER
    __try {
        res = GetICMProfileW(dc, &size, (LPWSTR)&str);
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        res = FALSE;
    }
#else
    res = GetICMProfileW(dc, &size, (LPWSTR)&str);
#endif

    ReleaseDC(nsnull, dc);
    if (!res)
        return nsnull;

    qcms_profile* profile =
        qcms_profile_from_path(NS_ConvertUTF16toUTF8(str).get());
#ifdef DEBUG_tor
    if (profile)
        fprintf(stderr,
                "ICM profile read from %s successfully\n",
                NS_ConvertUTF16toUTF8(str).get());
#endif
    return profile;
#else
    return nsnull;
#endif
}

PRBool
gfxWindowsPlatform::GetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> > *array)
{
    return mPrefFonts.Get(aKey, array);
}

void
gfxWindowsPlatform::SetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> >& array)
{
    mPrefFonts.Put(aKey, array);
}

#ifdef MOZ_FT2_FONTS
FT_Library
gfxWindowsPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}
#endif

void
gfxWindowsPlatform::InitDisplayCaps()
{
    HDC dc = GetDC((HWND)nsnull);

    gfxPlatform::sDPI = GetDeviceCaps(dc, LOGPIXELSY);

    ReleaseDC((HWND)nsnull, dc);
}
