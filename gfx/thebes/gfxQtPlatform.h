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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#ifndef GFX_PLATFORM_QT_H
#define GFX_PLATFORM_QT_H

#include "gfxPlatform.h"
#include "nsAutoRef.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

class gfxFontconfigUtils;
#ifndef MOZ_PANGO
typedef struct FT_LibraryRec_ *FT_Library;

class FontFamily;
class FontEntry;
#endif

class THEBES_API gfxQtPlatform : public gfxPlatform {
public:

    enum RenderMode {
        /* Use QPainter surfaces */
        RENDER_QPAINTER = 0,
        /* Use offscreen buffer for rendering with image or xlib gfx backend */
        RENDER_BUFFERED,
        /* Direct rendering to Widget surface */
        RENDER_DIRECT,
        /* max */
        RENDER_MODE_MAX
    };

    gfxQtPlatform();
    virtual ~gfxQtPlatform();

    static gfxQtPlatform *GetPlatform() {
        return (gfxQtPlatform*) gfxPlatform::GetPlatform();
    }

    already_AddRefed<gfxASurface> CreateOffscreenSurface(const gfxIntSize& size,
                                                         gfxASurface::gfxContentType contentType);

    nsresult GetFontList(nsIAtom *aLangGroup,
                         const nsACString& aGenericFamily,
                         nsTArray<nsString>& aListOfFonts);

    nsresult UpdateFontList();

    nsresult ResolveFontName(const nsAString& aFontName,
                             FontResolverCallback aCallback,
                             void *aClosure, bool& aAborted);

    nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    gfxFontGroup *CreateFontGroup(const nsAString &aFamilies,
                                  const gfxFontStyle *aStyle,
                                  gfxUserFontSet* aUserFontSet);

#ifdef MOZ_PANGO
    /**
     * Look up a local platform font using the full font face name (needed to
     * support @font-face src local() )
     */
    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName);

    /**
     * Activate a platform font (needed to support @font-face src url() )
     *
     */
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData,
                                           PRUint32 aLength);

    /**
     * Check whether format is supported on a platform or not (if unclear,
     * returns true).
     */
    virtual bool IsFontFormatSupported(nsIURI *aFontURI,
                                         PRUint32 aFormatFlags);
#endif

#ifndef MOZ_PANGO
    FontFamily *FindFontFamily(const nsAString& aName);
    FontEntry *FindFontEntry(const nsAString& aFamilyName, const gfxFontStyle& aFontStyle);
    already_AddRefed<gfxFont> FindFontForChar(PRUint32 aCh, gfxFont *aFont);
    bool GetPrefFontEntries(const nsCString& aLangGroup, nsTArray<nsRefPtr<gfxFontEntry> > *aFontEntryList);
    void SetPrefFontEntries(const nsCString& aLangGroup, nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList);
#endif

    void ClearPrefFonts() { mPrefFonts.Clear(); }

#ifndef MOZ_PANGO
    FT_Library GetFTLibrary();
#endif

    RenderMode GetRenderMode() { return mRenderMode; }
    void SetRenderMode(RenderMode rmode) { mRenderMode = rmode; }

    static PRInt32 GetDPI();

    virtual gfxImageFormat GetOffscreenFormat();

protected:
    static gfxFontconfigUtils *sFontconfigUtils;

private:
    virtual qcms_profile *GetPlatformCMSOutputProfile();

    // TODO: unify this with mPrefFonts (NB: holds families, not fonts) in gfxPlatformFontList
    nsDataHashtable<nsCStringHashKey, nsTArray<nsRefPtr<gfxFontEntry> > > mPrefFonts;

    RenderMode mRenderMode;
};

#endif /* GFX_PLATFORM_QT_H */

