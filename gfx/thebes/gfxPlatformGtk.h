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

namespace mozilla {
namespace dom {
class SystemFontListEntry;
};
};  // namespace mozilla

class gfxPlatformGtk : public gfxPlatform {
 public:
  gfxPlatformGtk();
  virtual ~gfxPlatformGtk();

  static gfxPlatformGtk* GetPlatform() {
    return (gfxPlatformGtk*)gfxPlatform::GetPlatform();
  }

  void ReadSystemFontList(
      InfallibleTArray<mozilla::dom::SystemFontListEntry>* retValue) override;

  already_AddRefed<gfxASurface> CreateOffscreenSurface(
      const IntSize& aSize, gfxImageFormat aFormat) override;

  nsresult GetFontList(nsAtom* aLangGroup, const nsACString& aGenericFamily,
                       nsTArray<nsString>& aListOfFonts) override;

  nsresult UpdateFontList() override;

  void GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh, Script aRunScript,
                              nsTArray<const char*>& aFontList) override;

  gfxPlatformFontList* CreatePlatformFontList() override;

  gfxFontGroup* CreateFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                                const gfxFontStyle* aStyle,
                                gfxTextPerfMetrics* aTextPerf,
                                gfxUserFontSet* aUserFontSet,
                                gfxFloat aDevToCssSize) override;

  /**
   * Calls XFlush if xrender is enabled.
   */
  void FlushContentDrawing() override;

  FT_Library GetFTLibrary() override;

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

#ifdef MOZ_X11
  Display* GetCompositorDisplay() { return mCompositorDisplay; }
#endif  // MOZ_X11

#ifdef MOZ_WAYLAND
  void SetWaylandLastVsync(uint32_t aVsyncTimestamp) {
    mWaylandLastVsyncTimestamp = aVsyncTimestamp;
  }
  int64_t GetWaylandLastVsync() { return mWaylandLastVsyncTimestamp; }
  void SetWaylandFrameDelay(int64_t aFrameDelay) {
    mWaylandFrameDelay = aFrameDelay;
  }
  int64_t GetWaylandFrameDelay() { return mWaylandFrameDelay; }
#endif

 protected:
  bool CheckVariationFontSupport() override;

  int8_t mMaxGenericSubstitutions;

 private:
  void GetPlatformCMSOutputProfile(void*& mem, size_t& size) override;

#ifdef MOZ_X11
  Display* mCompositorDisplay;
#endif
#ifdef MOZ_WAYLAND
  int64_t mWaylandLastVsyncTimestamp;
  int64_t mWaylandFrameDelay;
#endif
};

#endif /* GFX_PLATFORM_GTK_H */
