/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_QT_H
#define GFX_PLATFORM_QT_H

#include "gfxPlatform.h"
#include "nsAutoRef.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"
#ifdef MOZ_X11
#include "X11/Xlib.h"
#endif

class gfxFontconfigUtils;
class QWidget;
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

    virtual bool SupportsAzure(mozilla::gfx::BackendType& aBackend);

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
#ifdef MOZ_X11
    static Display* GetXDisplay(QWidget* aWindow = 0);
    static Screen* GetXScreen(QWidget* aWindow = 0);
#endif

protected:
    static gfxFontconfigUtils *sFontconfigUtils;

private:
    virtual qcms_profile *GetPlatformCMSOutputProfile();

    // TODO: unify this with mPrefFonts (NB: holds families, not fonts) in gfxPlatformFontList
    nsDataHashtable<nsCStringHashKey, nsTArray<nsRefPtr<gfxFontEntry> > > mPrefFonts;

    RenderMode mRenderMode;
};

#endif /* GFX_PLATFORM_QT_H */

