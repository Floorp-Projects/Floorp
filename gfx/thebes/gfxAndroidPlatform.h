/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_ANDROID_H
#define GFX_PLATFORM_ANDROID_H

#include "gfxFT2Fonts.h"
#include "gfxPlatform.h"
#include "gfxUserFontSet.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

namespace mozilla {
    namespace dom {
        class FontListEntry;
    };
};
using mozilla::dom::FontListEntry;

typedef struct FT_LibraryRec_ *FT_Library;

class gfxAndroidPlatform : public gfxPlatform {
public:
    gfxAndroidPlatform();
    virtual ~gfxAndroidPlatform();

    static gfxAndroidPlatform *GetPlatform() {
        return (gfxAndroidPlatform*) gfxPlatform::GetPlatform();
    }

    virtual already_AddRefed<gfxASurface>
    CreateOffscreenSurface(const IntSize& aSize,
                           gfxImageFormat aFormat);
    
    virtual gfxImageFormat GetOffscreenFormat() { return mOffscreenFormat; }
    
    already_AddRefed<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(mozilla::gfx::DrawTarget* aTarget, gfxFont *aFont);

    // to support IPC font list (sharing between chrome and content)
    void GetSystemFontList(InfallibleTArray<FontListEntry>* retValue);

    // platform implementations of font functions
    virtual bool IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags);
    virtual gfxPlatformFontList* CreatePlatformFontList();
    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          uint16_t aWeight,
                                          int16_t aStretch,
                                          bool aItalic);
    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           uint16_t aWeight,
                                           int16_t aStretch,
                                           bool aItalic,
                                           const uint8_t* aFontData,
                                           uint32_t aLength);

    virtual void GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                        int32_t aRunScript,
                                        nsTArray<const char*>& aFontList);

    virtual nsresult GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts);

    virtual nsresult UpdateFontList();

    virtual nsresult GetStandardFamilyName(const nsAString& aFontName,
                                           nsAString& aFamilyName);

    virtual gfxFontGroup*
    CreateFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                    const gfxFontStyle *aStyle,
                    gfxUserFontSet* aUserFontSet);

    virtual bool FontHintingEnabled() override;
    virtual bool RequiresLinearZoom() override;

    FT_Library GetFTLibrary();

    virtual int GetScreenDepth() const;

    virtual bool CanRenderContentToDataSurface() const override {
      return true;
    }

    virtual bool HaveChoiceOfHWAndSWCanvas() override;
    virtual bool UseAcceleratedSkiaCanvas() override;
    virtual already_AddRefed<mozilla::gfx::VsyncSource> CreateHardwareVsyncSource() override;

#ifdef MOZ_WIDGET_GONK
    virtual bool IsInGonkEmulator() const { return mIsInGonkEmulator; }
#endif

    virtual bool SupportsApzTouchInput() const override {
      return true;
    }

protected:
    bool AccelerateLayersByDefault() override {
      return true;
    }

private:
    int mScreenDepth;
    gfxImageFormat mOffscreenFormat;

#ifdef MOZ_WIDGET_GONK
    bool mIsInGonkEmulator;
#endif
};

#endif /* GFX_PLATFORM_ANDROID_H */

