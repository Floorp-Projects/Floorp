/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerevents_h__
#define mozilla_dom_workers_serviceworkerevents_h__

#include "mozilla/dom/Event.h"
#include "mozilla/dom/ExtendableEventBinding.h"
#include "mozilla/dom/ExtendableMessageEventBinding.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

#ifndef MOZ_SIMPLEPUSH
#include "mozilla/dom/File.h"
#endif

#include "nsProxyRelease.h"
#include "nsContentUtils.h"

class nsIInterceptedChannel;

namespace mozilla {
namespace dom {
class Blob;
class MessagePort;
class MessagePortList;
class Request;
class ResponseOrPromise;

struct PushEventInit;
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class ServiceWorkerRegistrationInfo;

class CancelChannelRunnable final : public Runnable
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  const nsresult mStatus;
public:
  CancelChannelRunnable(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                        nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
                        nsresult aStatus);

  NS_IMETHOD Run() override;
};

class ExtendableEvent : public Event
{
protected:
  nsTArray<RefPtr<Promise>> mPromises;

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
    RefPtr<ExtendableEvent> e = new ExtendableEvent(aOwner);
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
  WaitUntil(JSContext* aCx, Promise& aPromise, ErrorResult& aRv);

  already_AddRefed<Promise>
  GetPromise();

  virtual ExtendableEvent* AsExtendableEvent() override
  {
    return this;
  }
};

class FetchEvent final : public ExtendableEvent
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  RefPtr<Request> mRequest;
  nsCString mScriptSpec;
  nsCString mPreventDefaultScriptSpec;
  nsString mClientId;
  uint32_t mPreventDefaultLineNumber;
  uint32_t mPreventDefaultColumnNumber;
  bool mIsReload;
  bool mWaitToRespond;
protected:
  explicit FetchEvent(EventTarget* aOwner);
  ~FetchEvent();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FetchEvent, ExtendableEvent)

  // Note, we cannot use NS_FORWARD_TO_EVENT because we want a different
  // PreventDefault(JSContext*) override.
  NS_FORWARD_NSIDOMEVENT(Event::)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return FetchEventBinding::Wrap(aCx, this, aGivenProto);
  }

  void PostInit(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
                const nsACString& aScriptSpec);

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
    MOZ_ASSERT(mRequest);
    return mRequest;
  }

  void
  GetClientId(nsAString& aClientId) const
  {
    aClientId = mClientId;
  }

  bool
  IsReload() const
  {
    return mIsReload;
  }

  void
  RespondWith(JSContext* aCx, Promise& aArg, ErrorResult& aRv);

  already_AddRefed<Promise>
  ForwardTo(const nsAString& aUrl);

  already_AddRefed<Promise>
  Default();

  void
  PreventDefault(JSContext* aCx) override;

  void
  ReportCanceled();
};

#ifndef MOZ_SIMPLEPUSH

class PushMessageData final : public nsISupports,
                              public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PushMessageData)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const {
    return mOwner;
  }

  void Json(JSContext* cx, JS::MutableHandle<JS::Value> aRetval,
            ErrorResult& aRv);
  void Text(nsAString& aData);
  void ArrayBuffer(JSContext* cx, JS::MutableHandle<JSObject*> aRetval,
                   ErrorResult& aRv);
  already_AddRefed<mozilla::dom::Blob> Blob(ErrorResult& aRv);

  PushMessageData(nsISupports* aOwner, nsTArray<uint8_t>&& aBytes);
private:
  nsCOMPtr<nsISupports> mOwner;
  nsTArray<uint8_t> mBytes;
  nsString mDecodedText;
  ~PushMessageData();

  nsresult EnsureDecodedText();
  uint8_t* GetContentsCopy();
};

class PushEvent final : public ExtendableEvent
{
  RefPtr<PushMessageData> mData;

protected:
  explicit PushEvent(mozilla::dom::EventTarget* aOwner);
  ~PushEvent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PushEvent, ExtendableEvent)
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<PushEvent>
  Constructor(mozilla::dom::EventTarget* aOwner,
              const nsAString& aType,
              const PushEventInit& aOptions,
              ErrorResult& aRv);

  static already_AddRefed<PushEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const PushEventInit& aOptions,
              ErrorResult& aRv)
  {
    nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
    return Constructor(owner, aType, aOptions, aRv);
  }

  PushMessageData*
  GetData() const
  {
    return mData;
  }
};
#endif /* ! MOZ_SIMPLEPUSH */

class ExtendableMessageEvent final : public ExtendableEvent
{
  JS::Heap<JS::Value> mData;
  nsString mOrigin;
  nsString mLastEventId;
  RefPtr<ServiceWorkerClient> mClient;
  RefPtr<ServiceWorker> mServiceWorker;
  RefPtr<MessagePort> mMessagePort;
  RefPtr<MessagePortList> mPorts;

protected:
  explicit ExtendableMessageEvent(EventTarget* aOwner);
  ~ExtendableMessageEvent();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ExtendableMessageEvent,
                                                         ExtendableEvent)

  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::ExtendableMessageEventBinding::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<ExtendableMessageEvent>
  Constructor(mozilla::dom::EventTarget* aOwner,
              const nsAString& aType,
              const ExtendableMessageEventInit& aOptions,
              ErrorResult& aRv);

  static already_AddRefed<ExtendableMessageEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const ExtendableMessageEventInit& aOptions,
              ErrorResult& aRv);

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
               ErrorResult& aRv);

  void GetSource(Nullable<OwningClientOrServiceWorkerOrMessagePort>& aValue) const;

  NS_IMETHOD GetOrigin(nsAString& aOrigin)
  {
    aOrigin = mOrigin;
    return NS_OK;
  }

  NS_IMETHOD GetLastEventId(nsAString& aLastEventId)
  {
    aLastEventId = mLastEventId;
    return NS_OK;
  }

  MessagePortList* GetPorts() const;

  void SetPorts(MessagePortList* aPorts);

  void SetSource(ServiceWorkerClient* aClient);

  void SetSource(ServiceWorker* aServiceWorker);
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_serviceworkerevents_h__ */
