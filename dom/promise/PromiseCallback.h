/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseCallback_h
#define mozilla_dom_PromiseCallback_h

#include "mozilla/dom/Promise.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

#ifndef SPIDERMONKEY_PROMISE
// This is the base class for any PromiseCallback.
// It's a logical step in the promise chain of callbacks.
class PromiseCallback : public nsISupports
{
protected:
  virtual ~PromiseCallback();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseCallback)

  PromiseCallback();

  virtual nsresult Call(JSContext* aCx,
                        JS::Handle<JS::Value> aValue) = 0;

  // Return the Promise that this callback will end up resolving or
  // rejecting, if any.
  virtual Promise* GetDependentPromise() = 0;

  enum Task {
    Resolve,
    Reject
  };

  // This factory returns a PromiseCallback object with refcount of 0.
  static PromiseCallback*
  Factory(Promise* aNextPromise, JS::Handle<JSObject*> aObject,
          AnyCallback* aCallback, Task aTask);
};

// WrapperPromiseCallback execs a JS Callback with a value, and then the return
// value is sent to either:
// a) If aNextPromise is non-null, the aNextPromise->ResolveFunction() or to
//    aNextPromise->RejectFunction() if the JS Callback throws.
// or
// b) If aNextPromise is null, in which case aResolveFunc and aRejectFunc must
//    be non-null, then to aResolveFunc, unless aCallback threw, in which case
//    aRejectFunc.
class WrapperPromiseCallback final : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WrapperPromiseCallback,
                                                         PromiseCallback)

  nsresult Call(JSContext* aCx,
                JS::Handle<JS::Value> aValue) override;

  Promise* GetDependentPromise() override;

  // Constructor for when we know we have a vanilla Promise.
  WrapperPromiseCallback(Promise* aNextPromise, JS::Handle<JSObject*> aGlobal,
                         AnyCallback* aCallback);

  // Constructor for when all we have to work with are resolve/reject functions.
  WrapperPromiseCallback(JS::Handle<JSObject*> aGlobal,
                         AnyCallback* aCallback,
                         JS::Handle<JSObject*> mNextPromiseObj,
                         AnyCallback* aResolveFunc,
                         AnyCallback* aRejectFunc);

private:
  ~WrapperPromiseCallback();

  // Either mNextPromise is non-null or all three of mNextPromiseObj,
  // mResolveFund and mRejectFunc must are non-null.
  RefPtr<Promise> mNextPromise;
  // mNextPromiseObj is the reflector itself; it may not be in the
  // same compartment as anything else we have.
  JS::Heap<JSObject*> mNextPromiseObj;
  RefPtr<AnyCallback> mResolveFunc;
  RefPtr<AnyCallback> mRejectFunc;
  JS::Heap<JSObject*> mGlobal;
  RefPtr<AnyCallback> mCallback;
};

// ResolvePromiseCallback calls aPromise->ResolveFunction() with the value
// received by Call().
class ResolvePromiseCallback final : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ResolvePromiseCallback,
                                                         PromiseCallback)

  nsresult Call(JSContext* aCx,
                JS::Handle<JS::Value> aValue) override;

  Promise* GetDependentPromise() override
  {
    return mPromise;
  }

  ResolvePromiseCallback(Promise* aPromise, JS::Handle<JSObject*> aGlobal);

private:
  ~ResolvePromiseCallback();

  RefPtr<Promise> mPromise;
  JS::Heap<JSObject*> mGlobal;
};

// RejectPromiseCallback calls aPromise->RejectFunction() with the value
// received by Call().
class RejectPromiseCallback final : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(RejectPromiseCallback,
                                                         PromiseCallback)

  nsresult Call(JSContext* aCx,
                JS::Handle<JS::Value> aValue) override;

  Promise* GetDependentPromise() override
  {
    return mPromise;
  }

  RejectPromiseCallback(Promise* aPromise, JS::Handle<JSObject*> aGlobal);

private:
  ~RejectPromiseCallback();

  RefPtr<Promise> mPromise;
  JS::Heap<JSObject*> mGlobal;
};

// InvokePromiseFuncCallback calls the given function with the value
// received by Call().
class InvokePromiseFuncCallback final : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(InvokePromiseFuncCallback,
                                                         PromiseCallback)

  nsresult Call(JSContext* aCx,
                JS::Handle<JS::Value> aValue) override;

  Promise* GetDependentPromise() override;

  InvokePromiseFuncCallback(JS::Handle<JSObject*> aGlobal,
                            JS::Handle<JSObject*> aNextPromiseObj,
                            AnyCallback* aPromiseFunc);

private:
  ~InvokePromiseFuncCallback();

  JS::Heap<JSObject*> mGlobal;
  JS::Heap<JSObject*> mNextPromiseObj;
  RefPtr<AnyCallback> mPromiseFunc;
};

// NativePromiseCallback wraps a PromiseNativeHandler.
class NativePromiseCallback final : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NativePromiseCallback,
                                           PromiseCallback)

  nsresult Call(JSContext* aCx,
                JS::Handle<JS::Value> aValue) override;

  Promise* GetDependentPromise() override
  {
    return nullptr;
  }

  NativePromiseCallback(PromiseNativeHandler* aHandler,
                        Promise::PromiseState aState);

private:
  ~NativePromiseCallback();

  RefPtr<PromiseNativeHandler> mHandler;
  Promise::PromiseState mState;
};

#endif // SPIDERMONKEY_PROMISE

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseCallback_h
