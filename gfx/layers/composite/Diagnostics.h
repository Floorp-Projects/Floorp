/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_composite_Diagnostics_h
#define mozilla_gfx_layers_composite_Diagnostics_h

#include "mozilla/Maybe.h"
#include <cstdint>

namespace mozilla {
namespace layers {

// These statistics are collected by layers backends, preferably by the GPU
struct GPUStats {
  GPUStats() : mInvalidPixels(0), mScreenPixels(0), mPixelsFilled(0) {}

  uint32_t mInvalidPixels;
  uint32_t mScreenPixels;
  uint32_t mPixelsFilled;
  Maybe<float> mDrawTime;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_gfx_layers_composite_Diagnostics_h
