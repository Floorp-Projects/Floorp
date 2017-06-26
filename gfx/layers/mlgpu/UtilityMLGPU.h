/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_UtilityMLGPU_h
#define mozilla_gfx_layers_mlgpu_UtilityMLGPU_h

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"

namespace mozilla {
namespace layers {

template <size_t T> struct AlignUp
{
  static inline size_t calc(size_t aAmount) {
    MOZ_ASSERT(IsPowerOfTwo(T), "alignment must be a power of two");
    return aAmount + ((T - (aAmount % T)) % T);
  }
};

template <> struct AlignUp<0>
{
  static inline size_t calc(size_t aAmount) {
    return aAmount;
  }
};

} // namespace layers
} // namespace mozilla

#ifdef ENABLE_AL_LOGGING
#  define AL_LOG(...) printf_stderr("AL: " __VA_ARGS__)
#else
#  define AL_LOG(...)
#endif

#endif // mozilla_gfx_layers_mlgpu_UtilityMLGPU_h
