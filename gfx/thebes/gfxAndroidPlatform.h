/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_ANDROID_H
#define GFX_PLATFORM_ANDROID_H

#include "gfxPlatform.h"
#include "gfxUserFontSet.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class FontListEntry;
};
};  // namespace mozilla
using mozilla::dom::FontListEntry;

class gfxAndroidPlatform final : public gfxPlatform {
 public:
  gfxAndroidPlatform();
  virtual ~gfxAndroidPlatform();

  static gfxAndroidPlatform* GetPlatform() {
    return (gfxAndroidPlatform*)gfxPlatform::GetPlatform();
  }

  already_AddRefed<gfxASurface> CreateOffscreenSurface(
      const IntSize& aSize, gfxImageFormat aFormat) override;

  gfxImageFormat GetOffscreenFormat() override { return mOffscreenFormat; }

  // platform implementations of font functions
  gfxPlatformFontList* CreatePlatformFontList() override;

  void ReadSystemFontList(
      nsTArray<mozilla::dom::SystemFontListEntry>* aFontList) override;

  void GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh, Script aRunScript,
                              nsTArray<const char*>& aFontList) override;

  bool FontHintingEnabled() override;
  bool RequiresLinearZoom() override;

  already_AddRefed<mozilla::gfx::VsyncSource> CreateHardwareVsyncSource()
      override;

 protected:
  void InitAcceleration() override;

  bool AccelerateLayersByDefault() override { return true; }

  bool CheckVariationFontSupport() override {
    // We build with in-tree FreeType, so we know it is a new enough
    // version to support variations.
    return true;
  }

 private:
  gfxImageFormat mOffscreenFormat;
};

#endif /* GFX_PLATFORM_ANDROID_H */
