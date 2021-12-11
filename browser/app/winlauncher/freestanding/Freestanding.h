/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_Freestanding_h
#define mozilla_freestanding_Freestanding_h

/**
 * This header is automatically included in all source code residing in the
 * /browser/app/winlauncher/freestanding directory.
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1
#  error "This header should only be included by freestanding code"
#endif  // defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1

#define MOZ_USE_LAUNCHER_ERROR
#include "mozilla/NativeNt.h"

namespace mozilla {
namespace freestanding {

/**
 * Since this library is the only part of firefox.exe that needs special
 * treatment with respect to the heap, we implement |RtlNew| and |RtlDelete|
 * to be used instead of |new| and |delete| for any heap allocations inside
 * the freestanding library.
 */
template <typename T, typename... Args>
inline static T* RtlNew(Args&&... aArgs) {
  HANDLE processHeap = nt::RtlGetProcessHeap();
  if (!processHeap) {
    // Handle the case where the process heap is not initialized because
    // passing nullptr to RtlAllocateHeap crashes the process.
    return nullptr;
  }

  void* ptr = ::RtlAllocateHeap(processHeap, 0, sizeof(T));
  if (!ptr) {
    return nullptr;
  }

  return new (ptr) T(std::forward<Args>(aArgs)...);
}

template <typename T>
inline static void RtlDelete(T* aPtr) {
  if (!aPtr) {
    return;
  }

  aPtr->~T();
  ::RtlFreeHeap(nt::RtlGetProcessHeap(), 0, aPtr);
}

}  // namespace freestanding
}  // namespace mozilla

// Initialization code for all statically-allocated data in freestanding is
// placed into a separate section. This allows us to initialize any
// freestanding statics without needing to initialize everything else in this
// binary.
#pragma init_seg(".freestd$g")

#endif  // mozilla_freestanding_Freestanding_h
