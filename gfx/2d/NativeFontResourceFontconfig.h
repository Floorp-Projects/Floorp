/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_NativeFontResourceFontconfig_h
#define mozilla_gfx_NativeFontResourceFontconfig_h

#include "2D.h"

#include <cairo-ft.h>
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gfx {

class NativeFontResourceFontconfig final : public NativeFontResource
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(NativeFontResourceFontconfig, override)

  static already_AddRefed<NativeFontResourceFontconfig>
    Create(uint8_t *aFontData, uint32_t aDataLength, FT_Library aFTLibrary = nullptr);

  already_AddRefed<UnscaledFont>
    CreateUnscaledFont(uint32_t aIndex,
                       const uint8_t* aInstanceData, uint32_t aInstanceDataLength) final;

  ~NativeFontResourceFontconfig();

  FT_Face CloneFace();

private:
  NativeFontResourceFontconfig(UniquePtr<uint8_t[]>&& aFontData,
                               uint32_t aDataLength,
                               FT_Face aFace);

  UniquePtr<uint8_t[]> mFontData;
  uint32_t mDataLength;
  FT_Face mFace;
};

} // gfx
} // mozilla

#endif // mozilla_gfx_NativeFontResourceFontconfig_h
