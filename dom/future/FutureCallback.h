/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FutureCallback_h
#define mozilla_dom_FutureCallback_h

#include "mozilla/dom/Future.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class FutureResolver;

// This is the base class for any FutureCallback.
// It's a logical step in the future chain of callbacks.
class FutureCallback : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(FutureCallback)

  FutureCallback();
  virtual ~FutureCallback();

  virtual void Call(const Optional<JS::Handle<JS::Value> >& aValue) = 0;

  enum Task {
    Resolve,
    Reject
  };

  // This factory returns a FutureCallback object with refcount of 0.
  static FutureCallback*
  Factory(FutureResolver* aNextResolver, AnyCallback* aCallback,
          Task aTask);
};

// WrapperFutureCallback execs a JS Callback with a value, and then the return
// value is sent to the aNextResolver->resolve() or to aNextResolver->Reject()
// if the JS Callback throws.
class WrapperFutureCallback MOZ_FINAL : public FutureCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WrapperFutureCallback,
                                           FutureCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  WrapperFutureCallback(FutureResolver* aNextResolver,
                        AnyCallback* aCallback);
  ~WrapperFutureCallback();

private:
  nsRefPtr<FutureResolver> mNextResolver;
  nsRefPtr<AnyCallback> mCallback;
};

// SimpleWrapperFutureCallback execs a JS Callback with a value.
class SimpleWrapperFutureCallback MOZ_FINAL : public FutureCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SimpleWrapperFutureCallback,
                                           FutureCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  SimpleWrapperFutureCallback(Future* aFuture,
                              AnyCallback* aCallback);
  ~SimpleWrapperFutureCallback();

private:
  nsRefPtr<Future> mFuture;
  nsRefPtr<AnyCallback> mCallback;
};

// ResolveFutureCallback calls aResolver->Resolve() with the value received by
// Call().
class ResolveFutureCallback MOZ_FINAL : public FutureCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ResolveFutureCallback,
                                           FutureCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  ResolveFutureCallback(FutureResolver* aResolver);
  ~ResolveFutureCallback();

private:
  nsRefPtr<FutureResolver> mResolver;
};

// RejectFutureCallback calls aResolver->Reject() with the value received by
// Call().
class RejectFutureCallback MOZ_FINAL : public FutureCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RejectFutureCallback,
                                           FutureCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  RejectFutureCallback(FutureResolver* aResolver);
  ~RejectFutureCallback();

private:
  nsRefPtr<FutureResolver> mResolver;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FutureCallback_h
