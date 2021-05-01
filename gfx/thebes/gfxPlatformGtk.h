/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#endif  // MOZ_X11

class gfxPlatformGtk final : public gfxPlatform {
 public:
  gfxPlatformGtk();
  virtual ~gfxPlatformGtk();

  static gfxPlatformGtk* GetPlatform() {
    return (gfxPlatformGtk*)gfxPlatform::GetPlatform();
  }

  void ReadSystemFontList(mozilla::dom::SystemFontList* retValue) override;

  already_AddRefed<gfxASurface> CreateOffscreenSurface(
      const IntSize& aSize, gfxImageFormat aFormat) override;

  nsresult GetFontList(nsAtom* aLangGroup, const nsACString& aGenericFamily,
                       nsTArray<nsString>& aListOfFonts) override;

  void GetCommonFallbackFonts(uint32_t aCh, Script aRunScript,
                              eFontPresentation aPresentation,
                              nsTArray<const char*>& aFontList) override;

  gfxPlatformFontList* CreatePlatformFontList() override;

  /**
   * Calls XFlush if xrender is enabled.
   */
  void FlushContentDrawing() override;

  static int32_t GetFontScaleDPI();
  static double GetFontScaleFactor();

#ifdef MOZ_X11
  void GetAzureBackendInfo(mozilla::widget::InfoObject& aObj) override {
    gfxPlatform::GetAzureBackendInfo(aObj);
    aObj.DefineProperty("CairoUseXRender", mozilla::gfx::gfxVars::UseXRender());
  }
#endif

  bool UseImageOffscreenSurfaces();

  gfxImageFormat GetOffscreenFormat() override;

  bool SupportsApzWheelInput() const override { return true; }

  void FontsPrefsChanged(const char* aPref) override;

  // maximum number of fonts to substitute for a generic
  uint32_t MaxGenericSubstitions();

  bool SupportsPluginDirectBitmapDrawing() override { return true; }

  bool AccelerateLayersByDefault() override;

#ifdef MOZ_X11
  already_AddRefed<mozilla::gfx::VsyncSource> CreateHardwareVsyncSource()
      override;
#endif

#ifdef MOZ_WAYLAND
  bool UseDMABufWebGL() override { return mUseWebGLDmabufBackend; }
  void DisableDMABufWebGL() { mUseWebGLDmabufBackend = false; }
#endif

  bool IsX11Display() { return mIsX11Display; }
  bool IsWaylandDisplay() override {
    return !mIsX11Display && !gfxPlatform::IsHeadless();
  }

 protected:
  void InitX11EGLConfig();
  void InitDmabufConfig();
  void InitPlatformGPUProcessPrefs() override;
  bool CheckVariationFontSupport() override;

  int8_t mMaxGenericSubstitutions;

 private:
  nsTArray<uint8_t> GetPlatformCMSOutputProfileData() override;

  bool mIsX11Display;
#ifdef MOZ_WAYLAND
  bool mUseWebGLDmabufBackend;
#endif
};

#endif /* GFX_PLATFORM_GTK_H */
