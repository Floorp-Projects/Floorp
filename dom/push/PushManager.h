/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * PushManager and PushSubscription are exposed on the main and worker threads.
 * The main thread version is implemented in Push.js. The JS implementation
 * makes it easier to use certain APIs like the permission prompt and Promises.
 *
 * Unfortunately, JS-implemented WebIDL is not supported off the main thread.
 * To work around this, we use a chain of runnables to query the JS-implemented
 * nsIPushService component for subscription information, and return the
 * results to the worker. We don't have to deal with permission prompts, since
 * we just reject calls if the principal does not have permission.
 *
 * On the main thread, PushManager wraps a JS-implemented PushManagerImpl
 * instance. The C++ wrapper is necessary because our bindings code cannot
 * accomodate "JS-implemented on the main thread, C++ on the worker" bindings.
 *
 * PushSubscription is in C++ on both threads since it isn't particularly
 * verbose to implement in C++ compared to JS.
 */

#ifndef mozilla_dom_PushManager_h
#define mozilla_dom_PushManager_h

#include "nsWrapperCache.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TypedArray.h"

#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

class nsIGlobalObject;
class nsIPrincipal;

namespace mozilla {
class ErrorResult;

namespace dom {

class OwningArrayBufferViewOrArrayBufferOrString;
class Promise;
class PushManagerImpl;
struct PushSubscriptionOptionsInit;
class WorkerPrivate;

class PushManager final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(PushManager)

  enum SubscriptionAction {
    SubscribeAction,
    GetSubscriptionAction,
  };

  // The main thread constructor.
  PushManager(nsIGlobalObject* aGlobal, PushManagerImpl* aImpl);

  // The worker thread constructor.
  explicit PushManager(const nsAString& aScope);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<PushManager> Constructor(GlobalObject& aGlobal,
                                                   const nsAString& aScope,
                                                   ErrorResult& aRv);

  already_AddRefed<Promise> PerformSubscriptionActionFromWorker(
      SubscriptionAction aAction, ErrorResult& aRv);

  already_AddRefed<Promise> PerformSubscriptionActionFromWorker(
      SubscriptionAction aAction, const PushSubscriptionOptionsInit& aOptions,
      ErrorResult& aRv);

  already_AddRefed<Promise> Subscribe(
      const PushSubscriptionOptionsInit& aOptions, ErrorResult& aRv);

  already_AddRefed<Promise> GetSubscription(ErrorResult& aRv);

  already_AddRefed<Promise> PermissionState(
      const PushSubscriptionOptionsInit& aOptions, ErrorResult& aRv);

 private:
  ~PushManager();

  nsresult NormalizeAppServerKey(
      const OwningArrayBufferViewOrArrayBufferOrString& aSource,
      nsTArray<uint8_t>& aAppServerKey);

  // The following are only set and accessed on the main thread.
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<PushManagerImpl> mImpl;

  // Only used on the worker thread.
  nsString mScope;
};
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PushManager_h
