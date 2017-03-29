/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeFontResourceGDI.h"

#include "Logging.h"
#include "mozilla/RefPtr.h"
#include "ScaledFontWin.h"

namespace mozilla {
namespace gfx {

/* static */
already_AddRefed<NativeFontResourceGDI>
NativeFontResourceGDI::Create(uint8_t *aFontData, uint32_t aDataLength,
                              bool aNeedsCairo)
{
  DWORD numberOfFontsAdded;
  HANDLE fontResourceHandle = ::AddFontMemResourceEx(aFontData, aDataLength,
                                                     0, &numberOfFontsAdded);
  if (!fontResourceHandle) {
    gfxWarning() << "Failed to add memory font resource.";
    return nullptr;
  }

  RefPtr<NativeFontResourceGDI> fontResouce =
    new NativeFontResourceGDI(fontResourceHandle, aNeedsCairo);

  return fontResouce.forget();
}

NativeFontResourceGDI::~NativeFontResourceGDI()
{
  ::RemoveFontMemResourceEx(mFontResourceHandle);
}

already_AddRefed<ScaledFont>
NativeFontResourceGDI::CreateScaledFont(uint32_t aIndex, Float aGlyphSize,
                                        const uint8_t* aInstanceData, uint32_t aInstanceDataLength)
{
  if (aInstanceDataLength < sizeof(LOGFONT)) {
    gfxWarning() << "GDI scaled font instance data is truncated.";
    return nullptr;
  }

  const LOGFONT* logFont = reinterpret_cast<const LOGFONT*>(aInstanceData);

  // Constructor for ScaledFontWin dereferences and copies the LOGFONT, so we
  // are safe to pass this reference.
  RefPtr<ScaledFontBase> scaledFont = new ScaledFontWin(logFont, aGlyphSize);
  if (mNeedsCairo && !scaledFont->PopulateCairoScaledFont()) {
    gfxWarning() << "Unable to create cairo scaled font GDI font.";
    return nullptr;
  }

  return scaledFont.forget();
}

} // gfx
} // mozilla
