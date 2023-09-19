/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseNativeHandler_h
#define mozilla_dom_PromiseNativeHandler_h

#include <functional>
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "nsISupports.h"

namespace mozilla::dom {

/*
 * PromiseNativeHandler allows C++ to react to a Promise being
 * rejected/resolved. A PromiseNativeHandler can be appended to a Promise using
 * Promise::AppendNativeHandler().
 */
class PromiseNativeHandler : public nsISupports {
 protected:
  virtual ~PromiseNativeHandler() = default;

 public:
  MOZ_CAN_RUN_SCRIPT
  virtual void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) = 0;

  MOZ_CAN_RUN_SCRIPT
  virtual void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) = 0;
};

// This base class exists solely to use NS_IMPL_ISUPPORTS because it doesn't
// support template classes.
class MozPromiseRejectOnDestructionBase : public PromiseNativeHandler {
  NS_DECL_ISUPPORTS

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {}
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {}

 protected:
  ~MozPromiseRejectOnDestructionBase() override = default;
};

// Use this when you subscribe to a JS promise to settle a MozPromise that is
// not guaranteed to be settled by anyone else.
template <typename T>
class MozPromiseRejectOnDestruction final
    : public MozPromiseRejectOnDestructionBase {
 public:
  // (Accepting RefPtr<T> instead of T* because compiler fails to implicitly
  // convert it at call sites)
  MozPromiseRejectOnDestruction(const RefPtr<T>& aMozPromise,
                                const char* aCallSite)
      : mMozPromise(aMozPromise), mCallSite(aCallSite) {
    MOZ_ASSERT(aMozPromise);
  }

 protected:
  ~MozPromiseRejectOnDestruction() override {
    // Rejecting will be no-op if the promise is already settled
    mMozPromise->Reject(NS_BINDING_ABORTED, mCallSite);
  }

  RefPtr<T> mMozPromise;
  const char* mCallSite;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_PromiseNativeHandler_h
