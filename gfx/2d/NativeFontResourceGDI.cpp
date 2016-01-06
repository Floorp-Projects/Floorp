/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeFontResourceGDI.h"

#include "Logging.h"
#include "mozilla/RefPtr.h"
#include "ScaledFontWin.h"
#include "SFNTData.h"

namespace mozilla {
namespace gfx {

/* static */
already_AddRefed<NativeFontResourceGDI>
NativeFontResourceGDI::Create(uint8_t *aFontData, uint32_t aDataLength,
                              bool aNeedsCairo)
{
  UniquePtr<SFNTData> sfntData = SFNTData::Create(aFontData, aDataLength);
  if (!sfntData) {
    gfxWarning() << "Failed to create SFNTData for ScaledFontWin.";
    return nullptr;
  }

  Vector<mozilla::u16string> fontNames;
  if (!sfntData->GetU16FullNames(fontNames)) {
    gfxWarning() << "Failed to get font names from font.";
    return nullptr;
  }

  // lfFaceName has a maximum length including null.
  for (size_t i = 0; i < fontNames.length(); ++i) {
    if (fontNames[i].size() > LF_FACESIZE - 1) {
      fontNames[i].resize(LF_FACESIZE - 1);
    }
    // Add null to end for easy copying later.
    fontNames[i].append(1, '\0');
  }

  DWORD numberOfFontsAdded;
  HANDLE fontResourceHandle = ::AddFontMemResourceEx(aFontData, aDataLength,
                                                     0, &numberOfFontsAdded);
  if (!fontResourceHandle) {
    gfxWarning() << "Failed to add memory font resource.";
    return nullptr;
  }

  if (numberOfFontsAdded != fontNames.length()) {
    gfxWarning() <<
      "Number of fonts added doesn't match number of names extracted.";
  }

  RefPtr<NativeFontResourceGDI> fontResouce =
    new NativeFontResourceGDI(fontResourceHandle, Move(fontNames), aNeedsCairo);

  return fontResouce.forget();
}

NativeFontResourceGDI::~NativeFontResourceGDI()
{
  ::RemoveFontMemResourceEx(mFontResourceHandle);
}

already_AddRefed<ScaledFont>
NativeFontResourceGDI::CreateScaledFont(uint32_t aIndex, uint32_t aGlyphSize)
{
  if (aIndex >= mFontNames.length()) {
    gfxWarning() << "Font index is too high for font resource.";
    return nullptr;
  }

  if (mFontNames[aIndex].empty()) {
    gfxWarning() << "Font name for index is empty.";
    return nullptr;
  }

  LOGFONT logFont;
  logFont.lfHeight = 0;
  logFont.lfWidth = 0;
  logFont.lfEscapement = 0;
  logFont.lfOrientation = 0;
  logFont.lfWeight = FW_DONTCARE;
  logFont.lfItalic = FALSE;
  logFont.lfUnderline = FALSE;
  logFont.lfStrikeOut = FALSE;
  logFont.lfCharSet = DEFAULT_CHARSET;
  logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
  logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  logFont.lfQuality = DEFAULT_QUALITY;
  logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

  // Copy name to mLogFont (null already included in font name). We cast here
  // because for VS2015 char16_t != wchar_t, even though they are both 16 bit.
  mFontNames[aIndex].copy(reinterpret_cast<char16_t*>(logFont.lfFaceName),
                          mFontNames[aIndex].length());

  // Constructor for ScaledFontWin dereferences and copies the LOGFONT, so we
  // are safe to pass this reference.
  RefPtr<ScaledFontBase> scaledFont = new ScaledFontWin(&logFont, aGlyphSize);
  if (mNeedsCairo && !scaledFont->PopulateCairoScaledFont()) {
    gfxWarning() << "Unable to create cairo scaled font DWrite font.";
    return nullptr;
  }

  return scaledFont.forget();
}

} // gfx
} // mozilla
