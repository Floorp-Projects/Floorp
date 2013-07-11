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

class PromiseResolver;

// This is the base class for any PromiseCallback.
// It's a logical step in the promise chain of callbacks.
class PromiseCallback : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseCallback)

  PromiseCallback();
  virtual ~PromiseCallback();

  virtual void Call(const Optional<JS::Handle<JS::Value> >& aValue) = 0;

  enum Task {
    Resolve,
    Reject
  };

  // This factory returns a PromiseCallback object with refcount of 0.
  static PromiseCallback*
  Factory(PromiseResolver* aNextResolver, AnyCallback* aCallback,
          Task aTask);
};

// WrapperPromiseCallback execs a JS Callback with a value, and then the return
// value is sent to the aNextResolver->resolve() or to aNextResolver->Reject()
// if the JS Callback throws.
class WrapperPromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WrapperPromiseCallback,
                                           PromiseCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  WrapperPromiseCallback(PromiseResolver* aNextResolver,
                         AnyCallback* aCallback);
  ~WrapperPromiseCallback();

private:
  nsRefPtr<PromiseResolver> mNextResolver;
  nsRefPtr<AnyCallback> mCallback;
};

// SimpleWrapperPromiseCallback execs a JS Callback with a value.
class SimpleWrapperPromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SimpleWrapperPromiseCallback,
                                           PromiseCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  SimpleWrapperPromiseCallback(Promise* aPromise,
                               AnyCallback* aCallback);
  ~SimpleWrapperPromiseCallback();

private:
  nsRefPtr<Promise> mPromise;
  nsRefPtr<AnyCallback> mCallback;
};

// ResolvePromiseCallback calls aResolver->Resolve() with the value received by
// Call().
class ResolvePromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ResolvePromiseCallback,
                                           PromiseCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  ResolvePromiseCallback(PromiseResolver* aResolver);
  ~ResolvePromiseCallback();

private:
  nsRefPtr<PromiseResolver> mResolver;
};

// RejectPromiseCallback calls aResolver->Reject() with the value received by
// Call().
class RejectPromiseCallback MOZ_FINAL : public PromiseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RejectPromiseCallback,
                                           PromiseCallback)

  void Call(const Optional<JS::Handle<JS::Value> >& aValue) MOZ_OVERRIDE;

  RejectPromiseCallback(PromiseResolver* aResolver);
  ~RejectPromiseCallback();

private:
  nsRefPtr<PromiseResolver> mResolver;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseCallback_h
