/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _GFXALPHARECOVERY_GENERIC_H_
#define _GFXALPHARECOVERY_GENERIC_H_

#include "gfxAlphaRecovery.h"
#include "gfxImageSurface.h"
#include "nsDebug.h"
#include <xsimd/xsimd.hpp>

template <typename Arch>
bool gfxAlphaRecovery::RecoverAlphaGeneric(gfxImageSurface* blackSurf,
                                           const gfxImageSurface* whiteSurf) {
  mozilla::gfx::IntSize size = blackSurf->GetSize();

  if (size != whiteSurf->GetSize() ||
      (blackSurf->Format() != mozilla::gfx::SurfaceFormat::A8R8G8B8_UINT32 &&
       blackSurf->Format() != mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32) ||
      (whiteSurf->Format() != mozilla::gfx::SurfaceFormat::A8R8G8B8_UINT32 &&
       whiteSurf->Format() != mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32))
    return false;

  blackSurf->Flush();
  whiteSurf->Flush();

  unsigned char* blackData = blackSurf->Data();
  unsigned char* whiteData = whiteSurf->Data();

  if ((NS_PTR_TO_UINT32(blackData) & 0xf) !=
          (NS_PTR_TO_UINT32(whiteData) & 0xf) ||
      (blackSurf->Stride() - whiteSurf->Stride()) & 0xf) {
    // Cannot keep these in alignment.
    return false;
  }

  alignas(Arch::alignment()) static const uint8_t greenMaski[] = {
      0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
      0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
  };
  alignas(Arch::alignment()) static const uint8_t alphaMaski[] = {
      0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
      0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
  };

  using batch_type = xsimd::batch<uint8_t, Arch>;
  constexpr size_t batch_size = batch_type::size;
  static_assert(batch_size == 16);

  batch_type greenMask = batch_type::load_aligned(greenMaski);
  batch_type alphaMask = batch_type::load_aligned(alphaMaski);

  for (int32_t i = 0; i < size.height; ++i) {
    int32_t j = 0;
    // Loop single pixels until at 4 byte alignment.
    while (NS_PTR_TO_UINT32(blackData) & 0xf && j < size.width) {
      *((uint32_t*)blackData) =
          RecoverPixel(*reinterpret_cast<uint32_t*>(blackData),
                       *reinterpret_cast<uint32_t*>(whiteData));
      blackData += 4;
      whiteData += 4;
      j++;
    }
    // This extra loop allows the compiler to do some more clever registry
    // management and makes it about 5% faster than with only the 4 pixel
    // at a time loop.
    for (; j < size.width - 8; j += 8) {
      auto black1 = batch_type::load_aligned(blackData);
      auto white1 = batch_type::load_aligned(whiteData);
      auto black2 = batch_type::load_aligned(blackData + batch_size);
      auto white2 = batch_type::load_aligned(whiteData + batch_size);

      // Execute the same instructions as described in RecoverPixel, only
      // using an SSE2 packed saturated subtract.
      white1 = xsimd::ssub(white1, black1);
      white2 = xsimd::ssub(white2, black2);
      white1 = xsimd::ssub(greenMask, white1);
      white2 = xsimd::ssub(greenMask, white2);
      // Producing the final black pixel in an XMM register and storing
      // that is actually faster than doing a masked store since that
      // does an unaligned storage. We have the black pixel in a register
      // anyway.
      black1 = xsimd::bitwise_andnot(black1, alphaMask);
      black2 = xsimd::bitwise_andnot(black2, alphaMask);
      white1 = xsimd::slide_left<2>(white1);
      white2 = xsimd::slide_left<2>(white2);
      white1 &= alphaMask;
      white2 &= alphaMask;
      black1 |= white1;
      black2 |= white2;

      black1.store_aligned(blackData);
      black2.store_aligned(blackData + batch_size);
      blackData += 2 * batch_size;
      whiteData += 2 * batch_size;
    }
    for (; j < size.width - 4; j += 4) {
      auto black = batch_type::load_aligned(blackData);
      auto white = batch_type::load_aligned(whiteData);

      white = xsimd::ssub(white, black);
      white = xsimd::ssub(greenMask, white);
      black = xsimd::bitwise_andnot(black, alphaMask);
      white = xsimd::slide_left<2>(white);
      white &= alphaMask;
      black |= white;
      black.store_aligned(blackData);
      blackData += batch_size;
      whiteData += batch_size;
    }
    // Loop single pixels until we're done.
    while (j < size.width) {
      *((uint32_t*)blackData) =
          RecoverPixel(*reinterpret_cast<uint32_t*>(blackData),
                       *reinterpret_cast<uint32_t*>(whiteData));
      blackData += 4;
      whiteData += 4;
      j++;
    }
    blackData += blackSurf->Stride() - j * 4;
    whiteData += whiteSurf->Stride() - j * 4;
  }

  blackSurf->MarkDirty();

  return true;
}
#endif
