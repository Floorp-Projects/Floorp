/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerevents_h__
#define mozilla_dom_workers_serviceworkerevents_h__

#include "mozilla/dom/Event.h"
#include "mozilla/dom/ExtendableEventBinding.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

#ifndef MOZ_SIMPLEPUSH
#include "mozilla/dom/PushEventBinding.h"
#include "mozilla/dom/PushMessageDataBinding.h"
#endif

#include "nsProxyRelease.h"
#include "nsContentUtils.h"

class nsIInterceptedChannel;

namespace mozilla {
namespace dom {
class Blob;
class Request;
class ResponseOrPromise;
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class ServiceWorkerClient;

class FetchEvent final : public Event
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  nsRefPtr<ServiceWorkerClient> mClient;
  nsRefPtr<Request> mRequest;
  nsAutoPtr<ServiceWorkerClientInfo> mClientInfo;
  bool mIsReload;
  bool mWaitToRespond;
protected:
  explicit FetchEvent(EventTarget* aOwner);
  ~FetchEvent();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FetchEvent, Event)
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return FetchEventBinding::Wrap(aCx, this, aGivenProto);
  }

  void PostInit(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                nsAutoPtr<ServiceWorkerClientInfo>& aClientInfo);

  static already_AddRefed<FetchEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const FetchEventInit& aOptions,
              ErrorResult& aRv);

  bool
  WaitToRespond() const
  {
    return mWaitToRespond;
  }

  Request*
  Request_() const
  {
    return mRequest;
  }

  already_AddRefed<ServiceWorkerClient>
  GetClient();

  bool
  IsReload() const
  {
    return mIsReload;
  }

  void
  RespondWith(Promise& aArg, ErrorResult& aRv);

  already_AddRefed<Promise>
  ForwardTo(const nsAString& aUrl);

  already_AddRefed<Promise>
  Default();
};

class ExtendableEvent : public Event
{
  nsTArray<nsRefPtr<Promise>> mPromises;

protected:
  explicit ExtendableEvent(mozilla::dom::EventTarget* aOwner);
  ~ExtendableEvent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ExtendableEvent, Event)
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::ExtendableEventBinding::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<ExtendableEvent>
  Constructor(mozilla::dom::EventTarget* aOwner,
              const nsAString& aType,
              const EventInit& aOptions)
  {
    nsRefPtr<ExtendableEvent> e = new ExtendableEvent(aOwner);
    bool trusted = e->Init(aOwner);
    e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
    e->SetTrusted(trusted);
    return e.forget();
  }

  static already_AddRefed<ExtendableEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const EventInit& aOptions,
              ErrorResult& aRv)
  {
    nsCOMPtr<EventTarget> target = do_QueryInterface(aGlobal.GetAsSupports());
    return Constructor(target, aType, aOptions);
  }

  void
  WaitUntil(Promise& aPromise, ErrorResult& aRv);

  already_AddRefed<Promise>
  GetPromise();

  virtual ExtendableEvent* AsExtendableEvent() override
  {
    return this;
  }
};

#ifndef MOZ_SIMPLEPUSH

class PushMessageData final : public nsISupports,
                              public nsWrapperCache
{
  nsString mData;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PushMessageData)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::PushMessageDataBinding_workers::Wrap(aCx, this, aGivenProto);
  }

  nsISupports* GetParentObject() const {
    return nullptr;
  }

  void Json(JSContext* cx, JS::MutableHandle<JSObject*> aRetval);
  void Text(nsAString& aData);
  void ArrayBuffer(JSContext* cx, JS::MutableHandle<JSObject*> aRetval);
  mozilla::dom::Blob* Blob();

  explicit PushMessageData(const nsAString& aData);
private:
  ~PushMessageData();

};

class PushEvent final : public ExtendableEvent
{
  // FIXME(nsm): Bug 1149195.
  // nsRefPtr<PushMessageData> mData;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;

protected:
  explicit PushEvent(mozilla::dom::EventTarget* aOwner);
  ~PushEvent() {}

public:
  // FIXME(nsm): Bug 1149195.
  // Add cycle collection macros once data is re-exposed.
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::PushEventBinding_workers::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<PushEvent>
  Constructor(mozilla::dom::EventTarget* aOwner,
              const nsAString& aType,
              const PushEventInit& aOptions)
  {
    nsRefPtr<PushEvent> e = new PushEvent(aOwner);
    bool trusted = e->Init(aOwner);
    e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
    e->SetTrusted(trusted);
    // FIXME(nsm): Bug 1149195.
    //if(aOptions.mData.WasPassed()){
    //  e->mData = new PushMessageData(aOptions.mData.Value());
    //}
    return e.forget();
  }

  static already_AddRefed<PushEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const PushEventInit& aOptions,
              ErrorResult& aRv)
  {
    nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
    return Constructor(owner, aType, aOptions);
  }

  void PostInit(nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker)
  {
    mServiceWorker = aServiceWorker;
  }

  PushMessageData* Data()
  {
    // FIXME(nsm): Bug 1149195.
    MOZ_CRASH("Should not be called!");
    return nullptr;
  }
};
#endif /* ! MOZ_SIMPLEPUSH */

END_WORKERS_NAMESPACE
#endif /* mozilla_dom_workers_serviceworkerevents_h__ */
