/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions to help with Apple API calls.

#ifndef mozilla_AppleUtils_h
#define mozilla_AppleUtils_h

#include "mozilla/Attributes.h"

namespace mozilla {

// Wrapper class to call CFRelease on reference types
// when they go out of scope.
template <class T>
class AutoCFRelease {
public:
  MOZ_IMPLICIT AutoCFRelease(T aRef)
    : mRef(aRef)
  {
  }
  ~AutoCFRelease()
  {
    if (mRef) {
      CFRelease(mRef);
    }
  }
  // Return the wrapped ref so it can be used as an in parameter.
  operator T()
  {
    return mRef;
  }
  // Return a pointer to the wrapped ref for use as an out parameter.
  T* receive()
  {
    return &mRef;
  }

private:
  // Copy operator isn't supported and is not implemented.
  AutoCFRelease<T>& operator=(const AutoCFRelease<T>&);
  T mRef;
};

} // namespace mozilla

#endif // mozilla_AppleUtils_h
