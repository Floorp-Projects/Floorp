/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_GTK_H
#define GFX_PLATFORM_GTK_H

#include "gfxPlatform.h"
#include "nsAutoRef.h"
#include "nsTArray.h"
#include "mozilla/gfx/gfxVars.h"

#ifdef MOZ_X11
struct _XDisplay;
typedef struct _XDisplay Display;
#endif // MOZ_X11

namespace mozilla {
    namespace dom {
        class SystemFontListEntry;
    };
};

class gfxPlatformGtk : public gfxPlatform {
public:
    gfxPlatformGtk();
    virtual ~gfxPlatformGtk();

    static gfxPlatformGtk *GetPlatform() {
        return (gfxPlatformGtk*) gfxPlatform::GetPlatform();
    }

    void ReadSystemFontList(
        InfallibleTArray<mozilla::dom::SystemFontListEntry>* retValue) override;

    virtual already_AddRefed<gfxASurface>
      CreateOffscreenSurface(const IntSize& aSize,
                             gfxImageFormat aFormat) override;

    virtual nsresult GetFontList(nsAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts) override;

    virtual nsresult UpdateFontList() override;

    virtual void
    GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                           Script aRunScript,
                           nsTArray<const char*>& aFontList) override;

    virtual gfxPlatformFontList* CreatePlatformFontList() override;

    virtual nsresult GetStandardFamilyName(const nsAString& aFontName,
                                           nsAString& aFamilyName) override;

    gfxFontGroup*
    CreateFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                    const gfxFontStyle *aStyle,
                    gfxTextPerfMetrics* aTextPerf,
                    gfxUserFontSet *aUserFontSet,
                    gfxFloat aDevToCssSize) override;

    /**
     * Calls XFlush if xrender is enabled.
     */
    virtual void FlushContentDrawing() override;

    FT_Library GetFTLibrary() override;

    static int32_t GetFontScaleDPI();
    static double  GetFontScaleFactor();

#ifdef MOZ_X11
    virtual void GetAzureBackendInfo(mozilla::widget::InfoObject &aObj) override {
      gfxPlatform::GetAzureBackendInfo(aObj);
      aObj.DefineProperty("CairoUseXRender", mozilla::gfx::gfxVars::UseXRender());
    }
#endif

    bool UseImageOffscreenSurfaces();

    virtual gfxImageFormat GetOffscreenFormat() override;

    bool SupportsApzWheelInput() const override {
      return true;
    }

    void FontsPrefsChanged(const char *aPref) override;

    // maximum number of fonts to substitute for a generic
    uint32_t MaxGenericSubstitions();

    bool SupportsPluginDirectBitmapDrawing() override {
      return true;
    }

    bool AccelerateLayersByDefault() override;

#ifdef GL_PROVIDER_GLX
    already_AddRefed<mozilla::gfx::VsyncSource> CreateHardwareVsyncSource() override;
#endif

#ifdef MOZ_X11
    Display* GetCompositorDisplay() {
      return mCompositorDisplay;
    }
#endif // MOZ_X11

protected:
    bool CheckVariationFontSupport() override;

    int8_t mMaxGenericSubstitutions;

private:
    virtual void GetPlatformCMSOutputProfile(void *&mem,
                                             size_t &size) override;

#ifdef MOZ_X11
    Display* mCompositorDisplay;
#endif
};

#endif /* GFX_PLATFORM_GTK_H */
