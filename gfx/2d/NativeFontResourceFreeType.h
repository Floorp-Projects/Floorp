/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_NativeFontResourceFreeType_h
#define mozilla_gfx_NativeFontResourceFreeType_h

#include "2D.h"

#include <cairo-ft.h>
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gfx {

class NativeFontResourceFreeType : public NativeFontResource
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(NativeFontResourceFreeType, override)

#ifdef MOZ_WIDGET_ANDROID
  static already_AddRefed<NativeFontResourceFreeType>
    Create(uint8_t *aFontData, uint32_t aDataLength, FT_Library aFTLibrary = nullptr);

  already_AddRefed<UnscaledFont>
    CreateUnscaledFont(uint32_t aIndex,
                       const uint8_t* aInstanceData, uint32_t aInstanceDataLength) override;
#endif

  ~NativeFontResourceFreeType();

  FT_Face CloneFace();

protected:
  NativeFontResourceFreeType(UniquePtr<uint8_t[]>&& aFontData,
                             uint32_t aDataLength,
                             FT_Face aFace);

  template<class T>
  static already_AddRefed<T>
    CreateInternal(uint8_t *aFontData, uint32_t aDataLength, FT_Library aFTLibrary);

  UniquePtr<uint8_t[]> mFontData;
  uint32_t mDataLength;
  FT_Face mFace;
};

#ifdef MOZ_WIDGET_GTK
class NativeFontResourceFontconfig final : public NativeFontResourceFreeType
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(NativeFontResourceFontconfig, override)

  static already_AddRefed<NativeFontResourceFontconfig>
    Create(uint8_t *aFontData, uint32_t aDataLength, FT_Library aFTLibrary = nullptr);

  already_AddRefed<UnscaledFont>
    CreateUnscaledFont(uint32_t aIndex,
                       const uint8_t* aInstanceData, uint32_t aInstanceDataLength) final;

private:
  friend class NativeFontResourceFreeType;

  NativeFontResourceFontconfig(UniquePtr<uint8_t[]>&& aFontData,
                               uint32_t aDataLength,
                               FT_Face aFace);
};
#endif

} // gfx
} // mozilla

#endif // mozilla_gfx_NativeFontResourceFreeType_h
