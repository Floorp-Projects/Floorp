/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeFontResourceGDI.h"

#include "Logging.h"
#include "mozilla/RefPtr.h"
#include "ScaledFontWin.h"
#include "UnscaledFontGDI.h"

namespace mozilla {
namespace gfx {

/* static */
already_AddRefed<NativeFontResourceGDI>
NativeFontResourceGDI::Create(uint8_t *aFontData, uint32_t aDataLength)
{
  DWORD numberOfFontsAdded;
  HANDLE fontResourceHandle = ::AddFontMemResourceEx(aFontData, aDataLength,
                                                     0, &numberOfFontsAdded);
  if (!fontResourceHandle) {
    gfxWarning() << "Failed to add memory font resource.";
    return nullptr;
  }

  RefPtr<NativeFontResourceGDI> fontResouce =
    new NativeFontResourceGDI(fontResourceHandle);

  return fontResouce.forget();
}

NativeFontResourceGDI::~NativeFontResourceGDI()
{
  ::RemoveFontMemResourceEx(mFontResourceHandle);
}

already_AddRefed<UnscaledFont>
NativeFontResourceGDI::CreateUnscaledFont(uint32_t aIndex,
                                          const uint8_t* aInstanceData,
                                          uint32_t aInstanceDataLength)
{
  if (aInstanceDataLength < sizeof(LOGFONT)) {
    gfxWarning() << "GDI unscaled font instance data is truncated.";
    return nullptr;
  }

  const LOGFONT* logFont = reinterpret_cast<const LOGFONT*>(aInstanceData);
  RefPtr<UnscaledFont> unscaledFont = new UnscaledFontGDI(*logFont);
  return unscaledFont.forget();
}

} // gfx
} // mozilla
