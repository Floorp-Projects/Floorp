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

#if (MOZ_WIDGET_GTK == 2)
extern "C" {
    typedef struct _GdkDrawable GdkDrawable;
}
#endif

#ifdef MOZ_X11
struct _XDisplay;
typedef struct _XDisplay Display;
#endif // MOZ_X11

class gfxFontconfigUtils;

class gfxPlatformGtk : public gfxPlatform {
public:
    gfxPlatformGtk();
    virtual ~gfxPlatformGtk();

    static gfxPlatformGtk *GetPlatform() {
        return (gfxPlatformGtk*) gfxPlatform::GetPlatform();
    }

    virtual already_AddRefed<gfxASurface>
      CreateOffscreenSurface(const IntSize& aSize,
                             gfxImageFormat aFormat) override;

    virtual already_AddRefed<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(mozilla::gfx::DrawTarget* aTarget, gfxFont *aFont) override;

    virtual nsresult GetFontList(nsIAtom *aLangGroup,
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
     * Look up a local platform font using the full font face name (needed to
     * support @font-face src local() )
     */
    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          uint16_t aWeight,
                                          int16_t aStretch,
                                          uint8_t aStyle) override;

    /**
     * Activate a platform font (needed to support @font-face src url() )
     *
     */
    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           uint16_t aWeight,
                                           int16_t aStretch,
                                           uint8_t aStyle,
                                           const uint8_t* aFontData,
                                           uint32_t aLength) override;

    /**
     * Check whether format is supported on a platform or not (if unclear,
     * returns true).
     */
    virtual bool IsFontFormatSupported(nsIURI *aFontURI,
                                         uint32_t aFormatFlags) override;

    /**
     * Calls XFlush if xrender is enabled.
     */
    virtual void FlushContentDrawing() override;

    FT_Library GetFTLibrary() override;

#if (MOZ_WIDGET_GTK == 2)
    static void SetGdkDrawable(cairo_surface_t *target,
                               GdkDrawable *drawable);
    static GdkDrawable *GetGdkDrawable(cairo_surface_t *target);
#endif

    static int32_t GetDPI();
    static double  GetDPIScale();

#ifdef MOZ_X11
    virtual void GetAzureBackendInfo(mozilla::widget::InfoObject &aObj) override {
      gfxPlatform::GetAzureBackendInfo(aObj);
      aObj.DefineProperty("CairoUseXRender", mozilla::gfx::gfxVars::UseXRender());
    }
#endif

    static bool UseFcFontList() { return sUseFcFontList; }

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

    bool AccelerateLayersByDefault() override {
      return false;
    }

#ifdef GL_PROVIDER_GLX
    already_AddRefed<mozilla::gfx::VsyncSource> CreateHardwareVsyncSource() override;
#endif

#ifdef MOZ_X11
    Display* GetCompositorDisplay() {
      return mCompositorDisplay;
    }
#endif // MOZ_X11

protected:
    static gfxFontconfigUtils *sFontconfigUtils;

    int8_t mMaxGenericSubstitutions;

private:
    virtual void GetPlatformCMSOutputProfile(void *&mem,
                                             size_t &size) override;

#ifdef MOZ_X11
    Display* mCompositorDisplay;
#endif

    // xxx - this will be removed once the new fontconfig platform font list
    // replaces gfxPangoFontGroup
    static bool sUseFcFontList;
};

#endif /* GFX_PLATFORM_GTK_H */
