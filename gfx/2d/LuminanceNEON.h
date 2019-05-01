/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __LUMINANCENEON_H__
#define __LUMINANCENEON_H__

#include "mozilla/gfx/Point.h"

void ComputesRGBLuminanceMask_NEON(const uint8_t* aSourceData,
                                   int32_t aSourceStride, uint8_t* aDestData,
                                   int32_t aDestStride,
                                   const mozilla::gfx::IntSize& aSize,
                                   float aOpacity);

#endif /* __LUMINANCENEON_H__ */
