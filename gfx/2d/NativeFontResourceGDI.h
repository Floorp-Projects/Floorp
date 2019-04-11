/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_NativeFontResourceGDI_h
#define mozilla_gfx_NativeFontResourceGDI_h

#include <windows.h>

#include "2D.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Vector.h"

namespace mozilla {
namespace gfx {

class NativeFontResourceGDI final : public NativeFontResource {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(NativeFontResourceGDI, override)

  /**
   * Creates a NativeFontResourceGDI if data is valid. Note aFontData will be
   * copied if required and so can be released after calling.
   *
   * @param aFontData the SFNT data.
   * @param aDataLength length of data.
   * @return Referenced NativeFontResourceGDI or nullptr if invalid.
   */
  static already_AddRefed<NativeFontResourceGDI> Create(uint8_t* aFontData,
                                                        uint32_t aDataLength);

  virtual ~NativeFontResourceGDI();

  already_AddRefed<UnscaledFont> CreateUnscaledFont(
      uint32_t aIndex, const uint8_t* aInstanceData,
      uint32_t aInstanceDataLength) final;

 private:
  explicit NativeFontResourceGDI(HANDLE aFontResourceHandle)
      : mFontResourceHandle(aFontResourceHandle) {}

  HANDLE mFontResourceHandle;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_NativeFontResourceGDI_h
