/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerevents_h__
#define mozilla_dom_serviceworkerevents_h__

#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ExtendableEventBinding.h"
#include "mozilla/dom/ExtendableMessageEventBinding.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerCommon.h"

#include "nsProxyRelease.h"
#include "nsContentUtils.h"

class nsIInterceptedChannel;

namespace mozilla {
namespace dom {

class Blob;
class Client;
class MessagePort;
struct PushEventInit;
class Request;
class ResponseOrPromise;
class ServiceWorker;
class ServiceWorkerRegistrationInfo;

// Defined in ServiceWorker.cpp
bool
ServiceWorkerVisible(JSContext* aCx, JSObject* aObj);

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
public:
  class ExtensionsHandler {
  public:
    virtual bool
    WaitOnPromise(Promise& aPromise) = 0;

    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
  };

private:
  RefPtr<ExtensionsHandler> mExtensionsHandler;

protected:
  bool
  WaitOnPromise(Promise& aPromise);

  explicit ExtendableEvent(mozilla::dom::EventTarget* aOwner);
  ~ExtendableEvent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  void
  SetKeepAliveHandler(ExtensionsHandler* aExtensionsHandler);

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::ExtendableEvent_Binding::Wrap(aCx, this, aGivenProto);
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
    e->SetComposed(aOptions.mComposed);
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

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return FetchEvent_Binding::Wrap(aCx, this, aGivenProto);
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

  // Pull in the Event version of PreventDefault so we don't get
  // shadowing warnings.
  using Event::PreventDefault;
  void
  PreventDefault(JSContext* aCx, CallerType aCallerType) override;

  void
  ReportCanceled();
};

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

class ExtendableMessageEvent final : public ExtendableEvent
{
  JS::Heap<JS::Value> mData;
  nsString mOrigin;
  nsString mLastEventId;
  RefPtr<Client> mClient;
  RefPtr<ServiceWorker> mServiceWorker;
  RefPtr<MessagePort> mMessagePort;
  nsTArray<RefPtr<MessagePort>> mPorts;

protected:
  explicit ExtendableMessageEvent(EventTarget* aOwner);
  ~ExtendableMessageEvent();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ExtendableMessageEvent,
                                                         ExtendableEvent)

  virtual JSObject* WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::ExtendableMessageEvent_Binding::Wrap(aCx, this, aGivenProto);
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

  void GetOrigin(nsAString& aOrigin) const
  {
    aOrigin = mOrigin;
  }

  void GetLastEventId(nsAString& aLastEventId) const
  {
    aLastEventId = mLastEventId;
  }

  void GetPorts(nsTArray<RefPtr<MessagePort>>& aPorts);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_serviceworkerevents_h__ */
