/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Future_h
#define mozilla_dom_Future_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/FutureBinding.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"

struct JSContext;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class FutureInit;
class FutureCallback;
class AnyCallback;
class FutureResolver;

class Future MOZ_FINAL : public nsISupports,
                         public nsWrapperCache
{
  friend class FutureTask;
  friend class FutureResolver;
  friend class FutureResolverTask;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Future)

  Future(nsPIDOMWindow* aWindow);
  ~Future();

  static bool PrefEnabled();

  // WebIDL

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  static already_AddRefed<Future>
  Constructor(const GlobalObject& aGlobal, JSContext* aCx, FutureInit& aInit,
              ErrorResult& aRv);

  static already_AddRefed<Future>
  Resolve(const GlobalObject& aGlobal, JSContext* aCx,
          JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Future>
  Reject(const GlobalObject& aGlobal, JSContext* aCx,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  already_AddRefed<Future>
  Then(AnyCallback* aResolveCallback, AnyCallback* aRejectCallback);

  already_AddRefed<Future>
  Catch(AnyCallback* aRejectCallback);

  void Done(AnyCallback* aResolveCallback, AnyCallback* aRejectCallback);

private:
  enum FutureState {
    Pending,
    Resolved,
    Rejected
  };

  void SetState(FutureState aState)
  {
    MOZ_ASSERT(mState == Pending);
    MOZ_ASSERT(aState != Pending);
    mState = aState;
  }

  void SetResult(JS::Handle<JS::Value> aValue)
  {
    mResult = aValue;
  }

  // This method processes future's resolve/reject callbacks with future's
  // result. It's executed when the resolver.resolve() or resolver.reject() is
  // called or when the future already has a result and new callbacks are
  // appended by then(), catch() or done().
  void RunTask();

  void AppendCallbacks(FutureCallback* aResolveCallback,
                       FutureCallback* aRejectCallback);

  nsRefPtr<nsPIDOMWindow> mWindow;

  nsRefPtr<FutureResolver> mResolver;

  nsTArray<nsRefPtr<FutureCallback> > mResolveCallbacks;
  nsTArray<nsRefPtr<FutureCallback> > mRejectCallbacks;

  JS::Heap<JS::Value> mResult;
  FutureState mState;
  bool mTaskPending;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Future_h
