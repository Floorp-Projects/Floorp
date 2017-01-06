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

namespace mozilla {
namespace gfx {

/* static */
already_AddRefed<NativeFontResourceMac>
NativeFontResourceMac::Create(uint8_t *aFontData, uint32_t aDataLength)
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
