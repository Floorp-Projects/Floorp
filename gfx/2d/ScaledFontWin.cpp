/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontWin.h"
#include "ScaledFontBase.h"

#include "Logging.h"
#include "SFNTData.h"

#ifdef USE_SKIA
#include "skia/include/ports/SkTypeface_win.h"
#endif

#ifdef USE_CAIRO_SCALED_FONT
#include "cairo-win32.h"
#endif

namespace mozilla {
namespace gfx {

ScaledFontWin::ScaledFontWin(LOGFONT* aFont, Float aSize)
  : ScaledFontBase(aSize)
  , mLogFont(*aFont)
{
}

ScaledFontWin::ScaledFontWin(uint8_t* aFontData, uint32_t aFontDataLength,
                             uint32_t aIndex, Float aGlyphSize)
  : ScaledFontBase(aGlyphSize)
{
  mLogFont.lfHeight = 0;
  mLogFont.lfWidth = 0;
  mLogFont.lfEscapement = 0;
  mLogFont.lfOrientation = 0;
  mLogFont.lfWeight = FW_DONTCARE;
  mLogFont.lfItalic = FALSE;
  mLogFont.lfUnderline = FALSE;
  mLogFont.lfStrikeOut = FALSE;
  mLogFont.lfCharSet = DEFAULT_CHARSET;
  mLogFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
  mLogFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  mLogFont.lfQuality = DEFAULT_QUALITY;
  mLogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  mLogFont.lfFaceName[0] = 0;

  UniquePtr<SFNTData> sfntData = SFNTData::Create(aFontData, aFontDataLength);
  if (!sfntData) {
    gfxWarning() << "Failed to create SFNTData for ScaledFontWin.";
    return;
  }

  mozilla::u16string fontName;
  if (!sfntData->GetU16FullName(aIndex, fontName)) {
    gfxWarning() << "Failed to get font name from font.";
    return;
  }

  // Copy name to mLogFont and add null to end.
  // lfFaceName has a maximum length including null.
  if (fontName.size() > LF_FACESIZE - 1) {
    fontName.resize(LF_FACESIZE - 1);
  }
  // We cast here because for VS2015 char16_t != wchar_t, even though they are
  // both 16 bit.
  fontName.copy(reinterpret_cast<char16_t*>(mLogFont.lfFaceName),
                fontName.length());
  mLogFont.lfFaceName[fontName.length()] = 0;

  DWORD numberOfFontsAdded;
  HANDLE fontHandle = ::AddFontMemResourceEx(aFontData, aFontDataLength, 0,
                                             &numberOfFontsAdded);
  if (fontHandle) {
    mMemoryFontRemover.reset(new MemoryFontRemover(fontHandle));
  }

}

#ifdef USE_SKIA
SkTypeface* ScaledFontWin::GetSkTypeface()
{
  if (!mTypeface) {
    mTypeface = SkCreateTypefaceFromLOGFONT(mLogFont);
  }
  return mTypeface;
}
#endif

#ifdef USE_CAIRO_SCALED_FONT
cairo_font_face_t*
ScaledFontWin::GetCairoFontFace()
{
  if (mLogFont.lfFaceName[0] == 0) {
    return nullptr;
  }
  return cairo_win32_font_face_create_for_logfontw(&mLogFont);
}
#endif

}
}
