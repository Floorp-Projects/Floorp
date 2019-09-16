/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_UNSCALEDFONTFREETYPE_H_
#define MOZILLA_GFX_UNSCALEDFONTFREETYPE_H_

#include <cairo-ft.h>

#include "2D.h"

namespace mozilla {
namespace gfx {

class ScaledFontFreeType;
class ScaledFontFontconfig;

class UnscaledFontFreeType : public UnscaledFont {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontFreeType, override)
  explicit UnscaledFontFreeType(FT_Face aFace, bool aOwnsFace = false)
      : mFace(aFace), mOwnsFace(aOwnsFace), mIndex(0) {}
  explicit UnscaledFontFreeType(const char* aFile, uint32_t aIndex = 0)
      : mFace(nullptr), mOwnsFace(false), mFile(aFile), mIndex(aIndex) {}
  explicit UnscaledFontFreeType(std::string&& aFile, uint32_t aIndex = 0)
      : mFace(nullptr),
        mOwnsFace(false),
        mFile(std::move(aFile)),
        mIndex(aIndex) {}
  UnscaledFontFreeType(FT_Face aFace, NativeFontResource* aNativeFontResource)
      : mFace(aFace),
        mOwnsFace(false),
        mIndex(0),
        mNativeFontResource(aNativeFontResource) {}
  virtual ~UnscaledFontFreeType() {
    if (mOwnsFace) {
      Factory::ReleaseFTFace(mFace);
    }
  }

  FontType GetType() const override { return FontType::FREETYPE; }

  FT_Face GetFace() const { return mFace; }
  const char* GetFile() const { return mFile.c_str(); }
  uint32_t GetIndex() const { return mIndex; }
  const RefPtr<NativeFontResource>& GetNativeFontResource() const {
    return mNativeFontResource;
  }

  bool GetFontFileData(FontFileDataOutput aDataCallback, void* aBaton) override;

  bool GetFontDescriptor(FontDescriptorOutput aCb, void* aBaton) override;

  bool GetWRFontDescriptor(WRFontDescriptorOutput aCb, void* aBaton) override;

#ifdef MOZ_WIDGET_ANDROID
  FT_Face InitFace();

  already_AddRefed<ScaledFont> CreateScaledFont(
      Float aGlyphSize, const uint8_t* aInstanceData,
      uint32_t aInstanceDataLength, const FontVariation* aVariations,
      uint32_t aNumVariations) override;
#endif

 protected:
  FT_Face mFace;
  bool mOwnsFace;
  std::string mFile;
  uint32_t mIndex;
  RefPtr<NativeFontResource> mNativeFontResource;

  friend class ScaledFontFreeType;
  friend class ScaledFontFontconfig;

  static void GetVariationSettingsFromFace(
      std::vector<FontVariation>* aVariations, FT_Face aFace);

  static void ApplyVariationsToFace(const FontVariation* aVariations,
                                    uint32_t aNumVariations, FT_Face aFace);
};

#ifdef MOZ_WIDGET_GTK
class UnscaledFontFontconfig : public UnscaledFontFreeType {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontFontconfig, override)
  explicit UnscaledFontFontconfig(FT_Face aFace, bool aOwnsFace = false)
      : UnscaledFontFreeType(aFace, aOwnsFace) {}
  explicit UnscaledFontFontconfig(const char* aFile, uint32_t aIndex = 0)
      : UnscaledFontFreeType(aFile, aIndex) {}
  explicit UnscaledFontFontconfig(std::string&& aFile, uint32_t aIndex = 0)
      : UnscaledFontFreeType(std::move(aFile), aIndex) {}
  UnscaledFontFontconfig(FT_Face aFace, NativeFontResource* aNativeFontResource)
      : UnscaledFontFreeType(aFace, aNativeFontResource) {}

  FontType GetType() const override { return FontType::FONTCONFIG; }

  static already_AddRefed<UnscaledFont> CreateFromFontDescriptor(
      const uint8_t* aData, uint32_t aDataLength, uint32_t aIndex);

  already_AddRefed<ScaledFont> CreateScaledFont(
      Float aGlyphSize, const uint8_t* aInstanceData,
      uint32_t aInstanceDataLength, const FontVariation* aVariations,
      uint32_t aNumVariations) override;

  already_AddRefed<ScaledFont> CreateScaledFontFromWRFont(
      Float aGlyphSize, const wr::FontInstanceOptions* aOptions,
      const wr::FontInstancePlatformOptions* aPlatformOptions,
      const FontVariation* aVariations, uint32_t aNumVariations) override;
};
#endif

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_UNSCALEDFONTFREETYPE_H_ */
