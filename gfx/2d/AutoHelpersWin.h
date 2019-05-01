/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_AutoHelpersWin_h
#define mozilla_gfx_AutoHelpersWin_h

#include <windows.h>

namespace mozilla {
namespace gfx {

// Get the global device context, and auto-release it on destruction.
class AutoDC {
 public:
  AutoDC() { mDC = ::GetDC(nullptr); }

  ~AutoDC() { ::ReleaseDC(nullptr, mDC); }

  HDC GetDC() { return mDC; }

 private:
  HDC mDC;
};

// Select a font into the given DC, and auto-restore.
class AutoSelectFont {
 public:
  AutoSelectFont(HDC aDC, LOGFONTW* aLogFont) : mOwnsFont(false) {
    mFont = ::CreateFontIndirectW(aLogFont);
    if (mFont) {
      mOwnsFont = true;
      mDC = aDC;
      mOldFont = (HFONT)::SelectObject(aDC, mFont);
    } else {
      mOldFont = nullptr;
    }
  }

  AutoSelectFont(HDC aDC, HFONT aFont) : mOwnsFont(false) {
    mDC = aDC;
    mFont = aFont;
    mOldFont = (HFONT)::SelectObject(aDC, aFont);
  }

  ~AutoSelectFont() {
    if (mOldFont) {
      ::SelectObject(mDC, mOldFont);
      if (mOwnsFont) {
        ::DeleteObject(mFont);
      }
    }
  }

  bool IsValid() const { return mFont != nullptr; }

  HFONT GetFont() const { return mFont; }

 private:
  HDC mDC;
  HFONT mFont;
  HFONT mOldFont;
  bool mOwnsFont;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_AutoHelpersWin_h
