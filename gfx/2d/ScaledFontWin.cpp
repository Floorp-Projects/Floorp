/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontWin.h"
#include "UnscaledFontGDI.h"

#include "AutoHelpersWin.h"
#include "Logging.h"
#include "nsString.h"

#ifdef USE_SKIA
#include "skia/include/ports/SkTypeface_win.h"
#endif

#ifdef USE_CAIRO_SCALED_FONT
#include "cairo-win32.h"
#endif

#include "HelpersWinFonts.h"

namespace mozilla {
namespace gfx {

ScaledFontWin::ScaledFontWin(const LOGFONT* aFont,
                             const RefPtr<UnscaledFont>& aUnscaledFont,
                             Float aSize)
  : ScaledFontBase(aUnscaledFont, aSize)
  , mLogFont(*aFont)
{
}

bool
UnscaledFontGDI::GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton)
{
  AutoDC dc;
  AutoSelectFont font(dc.GetDC(), &mLogFont);

  // Check for a font collection first.
  uint32_t table = 0x66637474; // 'ttcf'
  uint32_t tableSize = ::GetFontData(dc.GetDC(), table, 0, nullptr, 0);
  if (tableSize == GDI_ERROR) {
    // Try as if just a single font.
    table = 0;
    tableSize = ::GetFontData(dc.GetDC(), table, 0, nullptr, 0);
    if (tableSize == GDI_ERROR) {
      return false;
    }
  }

  UniquePtr<uint8_t[]> fontData(new uint8_t[tableSize]);

  uint32_t sizeGot =
    ::GetFontData(dc.GetDC(), table, 0, fontData.get(), tableSize);
  if (sizeGot != tableSize) {
    return false;
  }

  aDataCallback(fontData.get(), tableSize, 0, aBaton);
  return true;
}

bool
ScaledFontWin::GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton)
{
  aCb(reinterpret_cast<uint8_t*>(&mLogFont), sizeof(mLogFont), nullptr, 0, aBaton);
  return true;
}

bool
UnscaledFontGDI::GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton)
{
  aCb(reinterpret_cast<uint8_t*>(&mLogFont), sizeof(mLogFont), aBaton);
  return true;
}

bool
UnscaledFontGDI::GetFontDescriptor(FontDescriptorOutput aCb, void* aBaton)
{
  aCb(reinterpret_cast<uint8_t*>(&mLogFont), sizeof(mLogFont), 0, aBaton);
  return true;
}

already_AddRefed<UnscaledFont>
UnscaledFontGDI::CreateFromFontDescriptor(const uint8_t* aData, uint32_t aDataLength, uint32_t aIndex)
{
  if (aDataLength < sizeof(LOGFONT)) {
    gfxWarning() << "GDI font descriptor is truncated.";
    return nullptr;
  }

  const LOGFONT* logFont = reinterpret_cast<const LOGFONT*>(aData);
  RefPtr<UnscaledFont> unscaledFont = new UnscaledFontGDI(*logFont);
  return unscaledFont.forget();
}

already_AddRefed<ScaledFont>
UnscaledFontGDI::CreateScaledFont(Float aGlyphSize,
                                  const uint8_t* aInstanceData,
                                  uint32_t aInstanceDataLength,
                                  const FontVariation* aVariations,
                                  uint32_t aNumVariations)
{
  if (aInstanceDataLength < sizeof(LOGFONT)) {
    gfxWarning() << "GDI unscaled font instance data is truncated.";
    return nullptr;
  }

  NativeFont nativeFont;
  nativeFont.mType = NativeFontType::GDI_LOGFONT;
  nativeFont.mFont = (void*)aInstanceData;

  return Factory::CreateScaledFontForNativeFont(nativeFont, this, aGlyphSize);
}

AntialiasMode
ScaledFontWin::GetDefaultAAMode()
{
  return GetSystemDefaultAAMode();
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
