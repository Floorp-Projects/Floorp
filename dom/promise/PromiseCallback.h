/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseCallback_h
#define mozilla_dom_PromiseCallback_h

#include "mozilla/dom/Promise.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

// This is the base class for any PromiseCallback.
// It's a logical step in the promise chain of callbacks.
class PromiseCallback : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseCallback)

  PromiseCallback();
  virtual ~PromiseCallback();

  virtual void Call(JS::Handle<JS::Value> aValue) = 0;

  enum Task {
    Resolve,
    Reject
  };

  // This factory returns a PromiseCallback object with refcount of 0.
  static PromiseCallback*
  Factory(Promise* aNextPromise, AnyCallback* aCallback, Task aTask);
};

// WrapperPromiseCallback execs a JS Callback with a value, and then the return
// value is sent to the aNextPromise->ResolveFunction() or to
// aNextPromise->RejectFunction() if the JS Callback throws.
class WrapperPromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WrapperPromiseCallback,
                                           PromiseCallback)

  void Call(JS::Handle<JS::Value> aValue) MOZ_OVERRIDE;

  WrapperPromiseCallback(Promise* aNextPromise, AnyCallback* aCallback);
  ~WrapperPromiseCallback();

private:
  nsRefPtr<Promise> mNextPromise;
  nsRefPtr<AnyCallback> mCallback;
};

// ResolvePromiseCallback calls aPromise->ResolveFunction() with the value
// received by Call().
class ResolvePromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ResolvePromiseCallback,
                                           PromiseCallback)

  void Call(JS::Handle<JS::Value> aValue) MOZ_OVERRIDE;

  ResolvePromiseCallback(Promise* aPromise);
  ~ResolvePromiseCallback();

private:
  nsRefPtr<Promise> mPromise;
};

// RejectPromiseCallback calls aPromise->RejectFunction() with the value
// received by Call().
class RejectPromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RejectPromiseCallback,
                                           PromiseCallback)

  void Call(JS::Handle<JS::Value> aValue) MOZ_OVERRIDE;

  RejectPromiseCallback(Promise* aPromise);
  ~RejectPromiseCallback();

private:
  nsRefPtr<Promise> mPromise;
};

// NativePromiseCallback wraps a NativePromiseHandler.
class NativePromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NativePromiseCallback,
                                           PromiseCallback)

  void Call(JS::Handle<JS::Value> aValue) MOZ_OVERRIDE;

  NativePromiseCallback(PromiseNativeHandler* aHandler,
                        Promise::PromiseState aState);
  ~NativePromiseCallback();

private:
  nsRefPtr<PromiseNativeHandler> mHandler;
  Promise::PromiseState mState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseCallback_h
