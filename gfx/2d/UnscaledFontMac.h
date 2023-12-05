/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_UNSCALEDFONTMAC_H_
#define MOZILLA_GFX_UNSCALEDFONTMAC_H_

#ifdef MOZ_WIDGET_COCOA
#  include <ApplicationServices/ApplicationServices.h>
#else
#  include <CoreGraphics/CoreGraphics.h>
#  include <CoreText/CoreText.h>
#endif

#include "2D.h"

namespace mozilla {
namespace gfx {

class UnscaledFontMac final : public UnscaledFont {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontMac, override)
  explicit UnscaledFontMac(CGFontRef aFont, bool aIsDataFont = false)
      : mFont(aFont), mIsDataFont(aIsDataFont) {
    CFRetain(mFont);
  }
  explicit UnscaledFontMac(CTFontDescriptorRef aFontDesc, CGFontRef aFont,
                           bool aIsDataFont = false)
      : mFontDesc(aFontDesc), mFont(aFont), mIsDataFont(aIsDataFont) {
    CFRetain(mFontDesc);
    CFRetain(mFont);
  }

  virtual ~UnscaledFontMac() {
    if (mCTAxesCache) {
      CFRelease(mCTAxesCache);
    }
    if (mCGAxesCache) {
      CFRelease(mCGAxesCache);
    }
    if (mFontDesc) {
      CFRelease(mFontDesc);
    }
    if (mFont) {
      CFRelease(mFont);
    }
  }

  FontType GetType() const override { return FontType::MAC; }

  CGFontRef GetFont() const { return mFont; }

  bool GetFontFileData(FontFileDataOutput aDataCallback, void* aBaton) override;

  bool IsDataFont() const { return mIsDataFont; }

  already_AddRefed<ScaledFont> CreateScaledFont(
      Float aGlyphSize, const uint8_t* aInstanceData,
      uint32_t aInstanceDataLength, const FontVariation* aVariations,
      uint32_t aNumVariations) override;

  already_AddRefed<ScaledFont> CreateScaledFontFromWRFont(
      Float aGlyphSize, const wr::FontInstanceOptions* aOptions,
      const wr::FontInstancePlatformOptions* aPlatformOptions,
      const FontVariation* aVariations, uint32_t aNumVariations) override;

  static CGFontRef CreateCGFontWithVariations(CGFontRef aFont,
                                              CFArrayRef& aCGAxesCache,
                                              CFArrayRef& aCTAxesCache,
                                              uint32_t aVariationCount,
                                              const FontVariation* aVariations);

  // Generate a font descriptor to send to WebRender. The descriptor consists
  // of a string that concatenates the PostScript name of the font and the path
  // to the font file, and an "index" that indicates the length of the psname
  // part of the string (= starting offset of the path).
  bool GetFontDescriptor(FontDescriptorOutput aCb, void* aBaton) override;

  CFArrayRef& CGAxesCache() { return mCGAxesCache; }
  CFArrayRef& CTAxesCache() { return mCTAxesCache; }

  static already_AddRefed<UnscaledFont> CreateFromFontDescriptor(
      const uint8_t* aData, uint32_t aDataLength, uint32_t aIndex);

 private:
  CTFontDescriptorRef mFontDesc = nullptr;
  CGFontRef mFont = nullptr;
  CFArrayRef mCGAxesCache = nullptr;  // Cached arrays of variation axis details
  CFArrayRef mCTAxesCache = nullptr;
  bool mIsDataFont;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_UNSCALEDFONTMAC_H_ */
