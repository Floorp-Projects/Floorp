/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPUtils_h_
#define GMPUtils_h_

#include "mozilla/UniquePtr.h"

namespace mozilla {

template<typename T>
struct GMPUniqueDestroyPolicy
{
  void operator()(T* aGMPObject) const {
    aGMPObject->Destroy();
  }
};

// Ideally, this would be a template alias, but GCC 4.6 doesn't support them.  See bug 1124021.
template<typename T>
struct GMPUnique {
  typedef mozilla::UniquePtr<T, GMPUniqueDestroyPolicy<T>> Ptr;
};

bool GetEMEVoucherPath(nsIFile** aPath);

bool EMEVoucherFileExists();

} // namespace mozilla

#endif
