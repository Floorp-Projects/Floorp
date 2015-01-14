/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGMASKFRAMENEON_H__
#define __NS_SVGMASKFRAMENEON_H__

#include "mozilla/gfx/2D.h"

using namespace mozilla::gfx;

void
ComputesRGBLuminanceMask_NEON(const uint8_t *aSourceData,
                              int32_t aSourceStride,
                              uint8_t *aDestData,
                              int32_t aDestStride,
                              const IntSize &aSize,
                              float aOpacity);

#endif /* __NS_SVGMASKFRAMENEON_H__ */
