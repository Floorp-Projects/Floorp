/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_SHARED_TARGETPTRHOLDER_H_
#define DOM_FS_SHARED_TARGETPTRHOLDER_H_

#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::fs {

// TODO: Remove this ad hoc class when bug 1805830 is fixed.
template <typename T>
class TargetPtrHolder {
 public:
  MOZ_IMPLICIT TargetPtrHolder(T* aRawPtr)
      : mTarget(GetCurrentSerialEventTarget()), mPtr(aRawPtr) {
    MOZ_ASSERT(mPtr);
  }

  TargetPtrHolder(const TargetPtrHolder&) = default;

  TargetPtrHolder& operator=(const TargetPtrHolder&) = default;

  TargetPtrHolder(TargetPtrHolder&&) = default;

  TargetPtrHolder& operator=(TargetPtrHolder&&) = default;

  ~TargetPtrHolder() {
    if (!mPtr) {
      return;
    }

    NS_ProxyRelease("TargetPtrHolder::mPtr", mTarget, mPtr.forget());
  }

  T* get() const {
    MOZ_ASSERT(mPtr);

    return mPtr.get();
  }

  T* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN { return get(); }

  bool operator!() { return !mPtr.get(); }

 private:
  nsCOMPtr<nsISerialEventTarget> mTarget;
  RefPtr<T> mPtr;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_SHARED_TARGETPTRHOLDER_H_
