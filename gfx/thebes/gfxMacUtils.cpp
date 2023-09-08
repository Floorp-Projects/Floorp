/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMacUtils.h"
#include <CoreVideo/CoreVideo.h>

/* static */ CFStringRef gfxMacUtils::CFStringForTransferFunction(
    mozilla::gfx::TransferFunction aTransferFunction) {
  switch (aTransferFunction) {
    case mozilla::gfx::TransferFunction::BT709:
      return kCVImageBufferTransferFunction_ITU_R_709_2;

    case mozilla::gfx::TransferFunction::SRGB:
      return kCVImageBufferTransferFunction_sRGB;

    case mozilla::gfx::TransferFunction::PQ:
      return kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ;

    case mozilla::gfx::TransferFunction::HLG:
      return kCVImageBufferTransferFunction_ITU_R_2100_HLG;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown TransferFunction.");
      return kCVImageBufferTransferFunction_ITU_R_709_2;
  }
}
