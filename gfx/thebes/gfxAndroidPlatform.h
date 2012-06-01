/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_ANDROID_H
#define GFX_PLATFORM_ANDROID_H

#include "gfxFT2Fonts.h"
#include "gfxPlatform.h"
#include "gfxUserFontSet.h"
#include "nsTArray.h"

namespace mozilla {
    namespace dom {
        class FontListEntry;
    };
};
using mozilla::dom::FontListEntry;

typedef struct FT_LibraryRec_ *FT_Library;

class THEBES_API gfxAndroidPlatform : public gfxPlatform {
public:
    gfxAndroidPlatform();
    virtual ~gfxAndroidPlatform();

    static gfxAndroidPlatform *GetPlatform() {
        return (gfxAndroidPlatform*) gfxPlatform::GetPlatform();
    }

    virtual already_AddRefed<gfxASurface>
    CreateOffscreenSurface(const gfxIntSize& size,
                           gfxASurface::gfxContentType contentType);
    
    virtual bool SupportsAzure(mozilla::gfx::BackendType& aBackend) { aBackend = mozilla::gfx::BACKEND_SKIA; return true; }

    virtual mozilla::gfx::SurfaceFormat Optimal2DFormatForContent(gfxASurface::gfxContentType aContent);

    virtual gfxImageFormat GetOffscreenFormat() { return mOffscreenFormat; }
    
    mozilla::RefPtr<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(gfxFont *aFont);

    // to support IPC font list (sharing between chrome and content)
    void GetFontList(InfallibleTArray<FontListEntry>* retValue);

    // platform implementations of font functions
    virtual bool IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags);
    virtual gfxPlatformFontList* CreatePlatformFontList();
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData, PRUint32 aLength);

    virtual nsresult GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts);

    virtual nsresult UpdateFontList();

    virtual nsresult ResolveFontName(const nsAString& aFontName,
                                     FontResolverCallback aCallback,
                                     void *aClosure, bool& aAborted);

    virtual nsresult GetStandardFamilyName(const nsAString& aFontName,
                                           nsAString& aFamilyName);

    virtual gfxFontGroup *CreateFontGroup(const nsAString &aFamilies,
                                          const gfxFontStyle *aStyle,
                                          gfxUserFontSet* aUserFontSet);

    virtual bool FontHintingEnabled() MOZ_OVERRIDE;

    FT_Library GetFTLibrary();

    virtual int GetScreenDepth() const;

private:
    int mScreenDepth;
    gfxImageFormat mOffscreenFormat;
};

#endif /* GFX_PLATFORM_ANDROID_H */

