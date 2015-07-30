/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * We would like to expose PushManager and PushSubscription on window and
 * workers. Parts of the Push API is implemented in JS out of necessity due to:
 * 1) Using frame message managers, in which
 *    nsIMessageListener::receiveMessage() must be in JS.
 * 2) It is easier to use certain APIs like the permission prompt and Promises
 *    from JS.
 *
 * Unfortunately, JS-implemented WebIDL is not supported off the main thread. To
 * aid in fixing this, the nsIPushClient is introduced which deals with part (1)
 * above. Part (2) is handled by PushManagerImpl on the main thread. PushManager
 * wraps this in C++ since our bindings code cannot accomodate "JS-implemented
 * on the main thread, C++ on the worker" bindings. PushManager simply forwards
 * the calls to the JS component.
 *
 * On the worker threads, we don't have to deal with permission prompts, instead
 * we just reject calls if the principal does not have permission. On workers
 * WorkerPushManager dispatches runnables to the main thread which directly call
 * nsIPushClient.
 *
 * PushSubscription is in C++ on both threads since it isn't particularly
 * verbose to implement in C++ compared to JS.
 */

#ifndef mozilla_dom_PushManager_h
#define mozilla_dom_PushManager_h

#include "nsWrapperCache.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"

#include "nsCOMPtr.h"
#include "mozilla/nsRefPtr.h"
#include "jsapi.h"

class nsIGlobalObject;
class nsIPrincipal;

namespace mozilla {
namespace dom {

namespace workers {
class WorkerPrivate;
}

class Promise;
class PushManagerImpl;

class PushSubscription final : public nsISupports
                             , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PushSubscription)

  explicit PushSubscription(nsIGlobalObject* aGlobal,
                            const nsAString& aEndpoint,
                            const nsAString& aScope);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject*
  GetParentObject() const
  {
    return mGlobal;
  }

  void
  GetEndpoint(nsAString& aEndpoint) const
  {
    aEndpoint = mEndpoint;
  }

  static already_AddRefed<PushSubscription>
  Constructor(GlobalObject& aGlobal, const nsAString& aEndpoint, const nsAString& aScope, ErrorResult& aRv);

  void
  SetPrincipal(nsIPrincipal* aPrincipal);

  already_AddRefed<Promise>
  Unsubscribe(ErrorResult& aRv);

protected:
  ~PushSubscription();

private:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsString mEndpoint;
  nsString mScope;
};

class PushManager final : public nsISupports
                        , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PushManager)

  static bool
  Enabled(JSContext* aCx, JSObject* aObj);

  explicit PushManager(nsIGlobalObject* aGlobal, const nsAString& aScope);

  nsIGlobalObject*
  GetParentObject() const
  {
    return mGlobal;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  Subscribe(ErrorResult& aRv);

  already_AddRefed<Promise>
  GetSubscription(ErrorResult& aRv);

  already_AddRefed<Promise>
  PermissionState(ErrorResult& aRv);

  void
  SetPushManagerImpl(PushManagerImpl& foo, ErrorResult& aRv);

protected:
  ~PushManager();

private:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsRefPtr<PushManagerImpl> mImpl;
  nsString mScope;
};

class WorkerPushSubscription final : public nsISupports
                                   , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WorkerPushSubscription)

  explicit WorkerPushSubscription(const nsAString& aEndpoint,
                                  const nsAString& aScope);

  nsIGlobalObject*
  GetParentObject() const
  {
    return nullptr;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<WorkerPushSubscription>
  Constructor(GlobalObject& aGlobal, const nsAString& aEndpoint, const nsAString& aScope, ErrorResult& aRv);

  void
  GetEndpoint(nsAString& aEndpoint) const
  {
    aEndpoint = mEndpoint;
  }

  already_AddRefed<Promise>
  Unsubscribe(ErrorResult& aRv);

protected:
  ~WorkerPushSubscription();

private:
  nsString mEndpoint;
  nsString mScope;
};

class WorkerPushManager final : public nsISupports
                              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WorkerPushManager)

  enum SubscriptionAction {
    SubscribeAction,
    GetSubscriptionAction,
  };

  explicit WorkerPushManager(const nsAString& aScope);

  nsIGlobalObject*
  GetParentObject() const
  {
    return nullptr;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  PerformSubscriptionAction(SubscriptionAction aAction, ErrorResult& aRv);

  already_AddRefed<Promise>
  Subscribe(ErrorResult& aRv);

  already_AddRefed<Promise>
  GetSubscription(ErrorResult& aRv);

  already_AddRefed<Promise>
  PermissionState(ErrorResult& aRv);

protected:
  ~WorkerPushManager();

private:
  nsString mScope;
};
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PushManager_h
