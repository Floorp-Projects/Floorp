/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontWin.h"

#include "AutoHelpersWin.h"
#include "Logging.h"
#include "nsString.h"
#include "SFNTData.h"

#ifdef USE_SKIA
#include "skia/include/ports/SkTypeface_win.h"
#endif

#ifdef USE_CAIRO_SCALED_FONT
#include "cairo-win32.h"
#endif

#include "HelpersWinFonts.h"

namespace mozilla {
namespace gfx {

ScaledFontWin::ScaledFontWin(LOGFONT* aFont, Float aSize)
  : ScaledFontBase(aSize)
  , mLogFont(*aFont)
{
}

bool
ScaledFontWin::GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton)
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

  // If it's a font collection then attempt to get the index.
  uint32_t index = 0;
  if (table != 0) {
    UniquePtr<SFNTData> sfntData = SFNTData::Create(fontData.get(),
                                                    tableSize);
    if (!sfntData) {
      gfxWarning() << "Failed to create SFNTData for GetFontFileData.";
      return false;
    }

    // We cast here because for VS2015 char16_t != wchar_t, even though they are
    // both 16 bit.
    if (!sfntData->GetIndexForU16Name(
          reinterpret_cast<char16_t*>(mLogFont.lfFaceName), &index, LF_FACESIZE - 1)) {
      gfxWarning() << "Failed to get index for face name.";
      gfxDevCrash(LogReason::GetFontFileDataFailed) <<
        "Failed to get index for face name |" <<
        NS_ConvertUTF16toUTF8(mLogFont.lfFaceName).get() << "|.";
      return false;
    }
  }

  aDataCallback(fontData.get(), tableSize, index, mSize, aBaton);
  return true;
}

bool
ScaledFontWin::GetFontDescriptor(FontDescriptorOutput aCb, void* aBaton)
{
  aCb(reinterpret_cast<uint8_t*>(&mLogFont), sizeof(mLogFont), mSize, aBaton);
  return true;
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
