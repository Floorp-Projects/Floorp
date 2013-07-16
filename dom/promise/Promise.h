/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_h
#define mozilla_dom_Promise_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/PromiseBinding.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"

struct JSContext;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class PromiseInit;
class PromiseCallback;
class AnyCallback;
class PromiseResolver;

class Promise MOZ_FINAL : public nsISupports,
                          public nsWrapperCache
{
  friend class PromiseTask;
  friend class PromiseResolver;
  friend class PromiseResolverTask;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Promise)

  Promise(nsPIDOMWindow* aWindow);
  ~Promise();

  static bool PrefEnabled();

  // WebIDL

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  static already_AddRefed<Promise>
  Constructor(const GlobalObject& aGlobal, JSContext* aCx, PromiseInit& aInit,
              ErrorResult& aRv);

  static already_AddRefed<Promise>
  Resolve(const GlobalObject& aGlobal, JSContext* aCx,
          JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Reject(const GlobalObject& aGlobal, JSContext* aCx,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  already_AddRefed<Promise>
  Then(const Optional<OwningNonNull<AnyCallback> >& aResolveCallback,
       const Optional<OwningNonNull<AnyCallback> >& aRejectCallback);


  already_AddRefed<Promise>
  Catch(const Optional<OwningNonNull<AnyCallback> >& aRejectCallback);

private:
  enum PromiseState {
    Pending,
    Resolved,
    Rejected
  };

  void SetState(PromiseState aState)
  {
    MOZ_ASSERT(mState == Pending);
    MOZ_ASSERT(aState != Pending);
    mState = aState;
  }

  void SetResult(JS::Handle<JS::Value> aValue)
  {
    mResult = aValue;
  }

  // This method processes promise's resolve/reject callbacks with promise's
  // result. It's executed when the resolver.resolve() or resolver.reject() is
  // called or when the promise already has a result and new callbacks are
  // appended by then(), catch() or done().
  void RunTask();

  void AppendCallbacks(PromiseCallback* aResolveCallback,
                       PromiseCallback* aRejectCallback);

  nsRefPtr<nsPIDOMWindow> mWindow;

  nsRefPtr<PromiseResolver> mResolver;

  nsTArray<nsRefPtr<PromiseCallback> > mResolveCallbacks;
  nsTArray<nsRefPtr<PromiseCallback> > mRejectCallbacks;

  JS::Heap<JS::Value> mResult;
  PromiseState mState;
  bool mTaskPending;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Promise_h
