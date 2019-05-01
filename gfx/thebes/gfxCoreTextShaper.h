/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CORETEXTSHAPER_H
#define GFX_CORETEXTSHAPER_H

#include "gfxFont.h"

#include <ApplicationServices/ApplicationServices.h>

class gfxMacFont;

class gfxCoreTextShaper : public gfxFontShaper {
 public:
  explicit gfxCoreTextShaper(gfxMacFont* aFont);

  virtual ~gfxCoreTextShaper();

  bool ShapeText(DrawTarget* aDrawTarget, const char16_t* aText, uint32_t aOffset, uint32_t aLength,
                 Script aScript, bool aVertical, RoundingFlags aRounding,
                 gfxShapedText* aShapedText) override;

  // clean up static objects that may have been cached
  static void Shutdown();

  // Flags used to track what AAT features should be enabled on the Core Text
  // font instance. (Internal; only public so that we can use the
  // MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS macro below.)
  enum FeatureFlags : uint8_t {
    kDefaultFeatures = 0x00,
    // bit flags for non-default feature settings we might need
    // to use, which will require separate font instances
    kDisableLigatures = 0x01,
    kAddSmallCaps = 0x02,
    kIndicFeatures = 0x04,

    // number of font instances, indexed by OR-ing the flags above
    kMaxFontInstances = 8
  };

 protected:
  CTFontRef mCTFont[kMaxFontInstances];

  // attributes for shaping text with LTR or RTL directionality
  CFDictionaryRef mAttributesDictLTR;
  CFDictionaryRef mAttributesDictRTL;

  nsresult SetGlyphsFromRun(gfxShapedText* aShapedText, uint32_t aOffset, uint32_t aLength,
                            CTRunRef aCTRun);

  CTFontRef CreateCTFontWithFeatures(CGFloat aSize, CTFontDescriptorRef aDescriptor);

  CFDictionaryRef CreateAttrDict(bool aRightToLeft);
  CFDictionaryRef CreateAttrDictWithoutDirection();

  static CTFontDescriptorRef CreateFontFeaturesDescriptor(
      const std::pair<SInt16, SInt16>* aFeatures, size_t aCount);

  static CTFontDescriptorRef GetFeaturesDescriptor(FeatureFlags aFeatureFlags);

  // cached font descriptors, created the first time they're needed
  static CTFontDescriptorRef sFeaturesDescriptor[kMaxFontInstances];
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(gfxCoreTextShaper::FeatureFlags)

#endif /* GFX_CORETEXTSHAPER_H */
