/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_SHARED_FILESYSTEMHELPERS_H_
#define DOM_FS_SHARED_FILESYSTEMHELPERS_H_

#include "FileSystemTypes.h"
#include "mozilla/RefPtr.h"

namespace mozilla::dom::fs {

// XXX Consider moving this class template to MFBT.

// A wrapper class template on top of the RefPtr. The RefPtr provides us the
// automatic reference counting of objects with AddRef() and Release() methods.
// `Registered` provides automatic registration counting of objects with
// Register() and Unregister() methods. Registration counting works similarly
// as reference counting, but objects are not deleted when the number of
// registrations drops to zero (that's managed by reference counting). Instead,
// an object can trigger an asynchronous close operation which still needs to
// hold and use the referenced object. Example:
//
// using BoolPromise = MozPromise<bool, nsresult, false>;
//
// class MyObject {
//  public:
//   NS_INLINE_DECL_REFCOUNTING(MyObject)
//
//   void Register() {
//     mRegCnt++;
//   }
//
//   void Unregister() {
//     mRegCnt--;
//     if (mRegCnt == 0) {
//       BeginClose();
//     }
//   }
//
//  private:
//   RefPtr<BoolPromise> BeginClose() {
//     return InvokeAsync(mIOTaskQueue, __func__,
//                []() {
//                  return BoolPromise::CreateAndResolve(true, __func__);
//                })
//         ->Then(GetCurrentSerialEventTarget(), __func__,
//                [self = RefPtr<MyObject>(this)](
//                    const BoolPromise::ResolveOrRejectValue&) {
//                  return self->mIOTaskQueue->BeginShutdown();
//                })
//         ->Then(GetCurrentSerialEventTarget(), __func__,
//                [self = RefPtr<MyObject>(this)](
//                    const ShutdownPromise::ResolveOrRejectValue&) {
//                  return BoolPromise::CreateAndResolve(true, __func__);
//                });
//   }
//
//   RefPtr<TaskQueue> mIOTaskQueue;
//   uint32_t mRegCnt = 0;
// };

template <class T>
class Registered {
 private:
  RefPtr<T> mObject;

 public:
  ~Registered() {
    if (mObject) {
      mObject->Unregister();
    }
  }

  Registered() = default;

  Registered(const Registered& aOther) : mObject(aOther.mObject) {
    mObject->Register();
  }

  Registered(Registered&& aOther) noexcept = default;

  MOZ_IMPLICIT Registered(RefPtr<T> aObject) : mObject(std::move(aObject)) {
    if (mObject) {
      mObject->Register();
    }
  }

  Registered<T>& operator=(decltype(nullptr)) {
    RefPtr<T> oldObject = std::move(mObject);
    mObject = nullptr;
    if (oldObject) {
      oldObject->Unregister();
    }
    return *this;
  }

  Registered<T>& operator=(const Registered<T>& aRhs) {
    if (aRhs.mObject) {
      aRhs.mObject->Register();
    }
    RefPtr<T> oldObject = std::move(mObject);
    mObject = aRhs.mObject;
    if (oldObject) {
      oldObject->Unregister();
    }
    return *this;
  }

  Registered<T>& operator=(Registered<T>&& aRhs) noexcept {
    RefPtr<T> oldObject = std::move(mObject);
    mObject = std::move(aRhs.mObject);
    aRhs.mObject = nullptr;
    if (oldObject) {
      oldObject->Unregister();
    }
    return *this;
  }

  const RefPtr<T>& inspect() const { return mObject; }

  RefPtr<T> unwrap() {
    RefPtr<T> oldObject = std::move(mObject);
    mObject = nullptr;
    if (oldObject) {
      oldObject->Unregister();
    }
    return oldObject;
  }

  T* get() const { return mObject; }

  operator T*() const& { return get(); }

  T* operator->() const { return get(); }
};

// Spec says valid names don't include (os-dependent) path separators,
// and is not equal to a dot . or two dots ..
// We want to use the same validator from both child and parent.
bool IsValidName(const fs::Name& aName);

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_SHARED_FILESYSTEMHELPERS_H_
