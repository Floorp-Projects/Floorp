/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions to help with Apple API calls.

#ifndef mozilla_AppleUtils_h
#define mozilla_AppleUtils_h

#include "mozilla/Attributes.h"
#include <CoreFoundation/CFBase.h>  // For CFRelease()
#include <CoreVideo/CVBuffer.h>     // For CVBufferRelease()

namespace mozilla {

// Wrapper class to call CFRelease/CVBufferRelease on reference types
// when they go out of scope.
template <class T, class F, F relFunc>
class AutoObjRefRelease {
 public:
  MOZ_IMPLICIT AutoObjRefRelease(T aRef) : mRef(aRef) {}
  ~AutoObjRefRelease() {
    if (mRef) {
      relFunc(mRef);
    }
  }
  // Return the wrapped ref so it can be used as an in parameter.
  operator T() { return mRef; }
  // Return a pointer to the wrapped ref for use as an out parameter.
  T* receive() { return &mRef; }

 private:
  // Copy operator isn't supported and is not implemented.
  AutoObjRefRelease<T, F, relFunc>& operator=(
      const AutoObjRefRelease<T, F, relFunc>&);
  T mRef;
};

template <typename T>
using AutoCFRelease = AutoObjRefRelease<T, decltype(&CFRelease), &CFRelease>;
template <typename T>
using AutoCVBufferRelease =
    AutoObjRefRelease<T, decltype(&CVBufferRelease), &CVBufferRelease>;

// CFRefPtr: A CoreFoundation smart pointer.
template <class T>
class CFRefPtr {
 public:
  explicit CFRefPtr(T aRef) : mRef(aRef) {
    if (mRef) {
      CFRetain(mRef);
    }
  }
  // Copy constructor.
  CFRefPtr(const CFRefPtr<T>& aCFRefPtr) : mRef(aCFRefPtr.mRef) {
    if (mRef) {
      CFRetain(mRef);
    }
  }
  // Copy operator
  CFRefPtr<T>& operator=(const CFRefPtr<T>& aCFRefPtr) {
    if (mRef == aCFRefPtr.mRef) {
      return;
    }
    if (mRef) {
      CFRelease(mRef);
    }
    mRef = aCFRefPtr.mRef;
    if (mRef) {
      CFRetain(mRef);
    }
    return *this;
  }
  ~CFRefPtr() {
    if (mRef) {
      CFRelease(mRef);
    }
  }
  // Return the wrapped ref so it can be used as an in parameter.
  operator T() { return mRef; }

 private:
  T mRef;
};

}  // namespace mozilla

#endif  // mozilla_AppleUtils_h
