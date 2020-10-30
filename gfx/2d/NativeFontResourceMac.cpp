/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unordered_set>
#include "NativeFontResourceMac.h"
#include "UnscaledFontMac.h"
#include "Types.h"

#include "mozilla/RefPtr.h"
#include "mozilla/DataMutex.h"

#ifdef MOZ_WIDGET_UIKIT
#  include <CoreFoundation/CoreFoundation.h>
#endif

#include "nsIMemoryReporter.h"
#include "nsCocoaFeatures.h"

namespace mozilla {
namespace gfx {

static StaticDataMutex<std::unordered_set<void*>> sWeakFontDataSet("WeakFonts");

void FontDataDeallocate(void*, void* info) {
  auto set = sWeakFontDataSet.Lock();
  set->erase(info);
  free(info);
}

class NativeFontResourceMacReporter final : public nsIMemoryReporter {
  ~NativeFontResourceMacReporter() = default;

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    MOZ_COLLECT_REPORT("explicit/gfx/native-font-resource-mac", KIND_HEAP,
                       UNITS_BYTES, SizeOfData(MallocSizeOf),
                       "Total memory used by native font API resource data.");
    return NS_OK;
  }

  static size_t SizeOfData(mozilla::MallocSizeOf aMallocSizeOf) {
    auto fontData = sWeakFontDataSet.Lock();
    size_t total = 0;
    for (auto& i : *fontData) {
      total += aMallocSizeOf(i);
    }
    return total;
  }
};

NS_IMPL_ISUPPORTS(NativeFontResourceMacReporter, nsIMemoryReporter)

void NativeFontResourceMac::RegisterMemoryReporter() {
  RegisterStrongMemoryReporter(new NativeFontResourceMacReporter);
}

/* static */
already_AddRefed<NativeFontResourceMac> NativeFontResourceMac::Create(
    uint8_t* aFontData, uint32_t aDataLength) {
  uint8_t* fontData = (uint8_t*)malloc(aDataLength);
  memcpy(fontData, aFontData, aDataLength);
  CFAllocatorContext context = {0,       fontData, nullptr, nullptr,
                                nullptr, nullptr,  nullptr, FontDataDeallocate,
                                nullptr};
  CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorDefault, &context);
  CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, fontData,
                                               aDataLength, allocator);

  {
    auto set = sWeakFontDataSet.Lock();
    set->insert(fontData);
  }

  CTFontDescriptorRef ctFontDesc =
      CTFontManagerCreateFontDescriptorFromData(data);
  CFRelease(data);

  // creating the CGFontRef via the CTFont avoids the data being held alive
  // in a cache.
  CTFontRef ctFont = CTFontCreateWithFontDescriptor(ctFontDesc, 0, NULL);

  // Creating the CGFont from the CTFont prevents the font data from being
  // held in the TDescriptorSource cache. This appears to be true even
  // if we later create a CTFont from the CGFont.
  CGFontRef fontRef = CTFontCopyGraphicsFont(ctFont, NULL);
  CFRelease(ctFont);

  if (!fontRef) {
    return nullptr;
  }

  // passes ownership of fontRef to the NativeFontResourceMac instance
  RefPtr<NativeFontResourceMac> fontResource =
      new NativeFontResourceMac(ctFontDesc, fontRef, aDataLength);

  return fontResource.forget();
}

already_AddRefed<UnscaledFont> NativeFontResourceMac::CreateUnscaledFont(
    uint32_t aIndex, const uint8_t* aInstanceData,
    uint32_t aInstanceDataLength) {
  RefPtr<UnscaledFont> unscaledFont =
      new UnscaledFontMac(mFontDescRef, mFontRef, true);

  return unscaledFont.forget();
}

}  // namespace gfx
}  // namespace mozilla
