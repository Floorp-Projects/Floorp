/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeFontResourceMac.h"
#include "Types.h"

#include "mozilla/RefPtr.h"

#ifdef MOZ_WIDGET_UIKIT
#include <CoreFoundation/CoreFoundation.h>
#endif

// Simple helper class to automatically release a CFObject when it goes out
// of scope.
template<class T>
class AutoRelease
{
public:
  explicit AutoRelease(T aObject)
    : mObject(aObject)
  {
  }

  ~AutoRelease()
  {
    if (mObject) {
      CFRelease(mObject);
    }
  }

  operator T()
  {
    return mObject;
  }

  T forget()
  {
    T obj = mObject;
    mObject = nullptr;
    return obj;
  }

private:
  T mObject;
};

// This is essentially identical to the similarly-named helper function
// in gfx/thebes/gfxMacFont.cpp. Maybe we should put it somewhere that
// can be shared by both Moz2d and Thebes callers?
static CFDictionaryRef
CreateVariationDictionaryOrNull(CGFontRef aCGFont, uint32_t aVariationCount,
  const mozilla::gfx::ScaledFont::VariationSetting* aVariations)
{
  AutoRelease<CTFontRef>
    ctFont(CTFontCreateWithGraphicsFont(aCGFont, 0, nullptr, nullptr));
  AutoRelease<CFArrayRef> axes(CTFontCopyVariationAxes(ctFont));
  if (!axes) {
    return nullptr;
  }

  CFIndex axisCount = CFArrayGetCount(axes);
  AutoRelease<CFMutableDictionaryRef>
    dict(CFDictionaryCreateMutable(kCFAllocatorDefault, axisCount,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks));

  // Number of variation settings passed in the aVariations parameter.
  // This will typically be a very low value, so we just linear-search them.
  bool allDefaultValues = true;

  for (CFIndex i = 0; i < axisCount; ++i) {
    // We sanity-check the axis info found in the CTFont, and bail out
    // (returning null) if it doesn't have the expected types.
    CFTypeRef axisInfo = CFArrayGetValueAtIndex(axes, i);
    if (CFDictionaryGetTypeID() != CFGetTypeID(axisInfo)) {
      return nullptr;
    }
    CFDictionaryRef axis = static_cast<CFDictionaryRef>(axisInfo);

    CFTypeRef axisTag =
        CFDictionaryGetValue(axis, kCTFontVariationAxisIdentifierKey);
    if (!axisTag || CFGetTypeID(axisTag) != CFNumberGetTypeID()) {
      return nullptr;
    }
    int64_t tagLong;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(axisTag),
                          kCFNumberSInt64Type, &tagLong)) {
      return nullptr;
    }

    CFTypeRef axisName =
      CFDictionaryGetValue(axis, kCTFontVariationAxisNameKey);
    if (!axisName || CFGetTypeID(axisName) != CFStringGetTypeID()) {
      return nullptr;
    }

    // Clamp axis values to the supported range.
    CFTypeRef min = CFDictionaryGetValue(axis, kCTFontVariationAxisMinimumValueKey);
    CFTypeRef max = CFDictionaryGetValue(axis, kCTFontVariationAxisMaximumValueKey);
    CFTypeRef def = CFDictionaryGetValue(axis, kCTFontVariationAxisDefaultValueKey);
    if (!min || CFGetTypeID(min) != CFNumberGetTypeID() ||
        !max || CFGetTypeID(max) != CFNumberGetTypeID() ||
        !def || CFGetTypeID(def) != CFNumberGetTypeID()) {
      return nullptr;
    }
    double minDouble;
    double maxDouble;
    double defDouble;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(min), kCFNumberDoubleType,
                          &minDouble) ||
        !CFNumberGetValue(static_cast<CFNumberRef>(max), kCFNumberDoubleType,
                          &maxDouble) ||
        !CFNumberGetValue(static_cast<CFNumberRef>(def), kCFNumberDoubleType,
                          &defDouble)) {
      return nullptr;
    }

    double value = defDouble;
    for (uint32_t j = 0; j < aVariationCount; ++j) {
      if (aVariations[j].mTag == tagLong) {
        value = std::min(std::max<double>(aVariations[j].mValue,
                                          minDouble),
                         maxDouble);
        if (value != defDouble) {
          allDefaultValues = false;
        }
        break;
      }
    }
    AutoRelease<CFNumberRef> valueNumber(CFNumberCreate(kCFAllocatorDefault,
                                                        kCFNumberDoubleType,
                                                        &value));
    CFDictionaryAddValue(dict, axisName, valueNumber);
  }

  if (allDefaultValues) {
    // We didn't actually set any non-default values, so throw away the
    // variations dictionary and just use the default rendering.
    return nullptr;
  }

  return dict.forget();
}

namespace mozilla {
namespace gfx {

/* static */
already_AddRefed<NativeFontResourceMac>
NativeFontResourceMac::Create(uint8_t *aFontData, uint32_t aDataLength,
                              uint32_t aVariationCount,
                              const ScaledFont::VariationSetting* aVariations)
{
  // copy font data
  CFDataRef data = CFDataCreate(kCFAllocatorDefault, aFontData, aDataLength);
  if (!data) {
    return nullptr;
  }

  // create a provider
  CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);

  // release our reference to the CFData, provider keeps it alive
  CFRelease(data);

  // create the font object
  CGFontRef fontRef = CGFontCreateWithDataProvider(provider);

  // release our reference, font will keep it alive as long as needed
  CGDataProviderRelease(provider);

  if (!fontRef) {
    return nullptr;
  }

  if (aVariationCount > 0) {
    MOZ_ASSERT(aVariations);
    AutoRelease<CFDictionaryRef>
      varDict(CreateVariationDictionaryOrNull(fontRef, aVariationCount,
                                              aVariations));
    if (varDict) {
      CGFontRef varFont = CGFontCreateCopyWithVariations(fontRef, varDict);
      if (varFont) {
        CFRelease(fontRef);
        fontRef = varFont;
      }
    }
  }

  // passes ownership of fontRef to the NativeFontResourceMac instance
  RefPtr<NativeFontResourceMac> fontResource =
    new NativeFontResourceMac(fontRef);

  return fontResource.forget();
}

already_AddRefed<ScaledFont>
NativeFontResourceMac::CreateScaledFont(uint32_t aIndex, Float aGlyphSize,
                                        const uint8_t* aInstanceData, uint32_t aInstanceDataLength)
{
  RefPtr<ScaledFontBase> scaledFont = new ScaledFontMac(mFontRef, aGlyphSize);

  if (!scaledFont->PopulateCairoScaledFont()) {
    gfxWarning() << "Unable to create cairo scaled Mac font.";
    return nullptr;
  }

  return scaledFont.forget();
}

} // gfx
} // mozilla
