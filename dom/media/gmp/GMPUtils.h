/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPUtils_h_
#define GMPUtils_h_

#include "mozilla/UniquePtr.h"

namespace mozilla {

template<typename T>
struct DestroyPolicy
{
  void operator()(T* aGMPObject) const {
    aGMPObject->Destroy();
  }
};

template<typename T>
using GMPUniquePtr = mozilla::UniquePtr<T, DestroyPolicy<T>>;

} // namespace mozilla

#endif
