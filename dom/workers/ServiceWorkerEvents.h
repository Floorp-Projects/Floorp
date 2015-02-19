/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerevents_h__
#define mozilla_dom_workers_serviceworkerevents_h__

#include "mozilla/dom/Event.h"
#include "mozilla/dom/ExtendableEventBinding.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/InstallEventBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsProxyRelease.h"

class nsIInterceptedChannel;

namespace mozilla {
namespace dom {
  class Request;
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class ServiceWorker;
class ServiceWorkerClient;

class FetchEvent MOZ_FINAL : public Event
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  nsRefPtr<ServiceWorkerClient> mClient;
  nsRefPtr<Request> mRequest;
  uint64_t mWindowId;
  bool mIsReload;
  bool mWaitToRespond;
protected:
  explicit FetchEvent(EventTarget* aOwner);
  ~FetchEvent();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FetchEvent, Event)
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return FetchEventBinding::Wrap(aCx, this);
  }

  void PostInit(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                uint64_t aWindowId);

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
  Client();

  bool
  IsReload() const
  {
    return mIsReload;
  }

  void
  RespondWith(Promise& aPromise, ErrorResult& aRv);

  already_AddRefed<Promise>
  ForwardTo(const nsAString& aUrl);

  already_AddRefed<Promise>
  Default();
};

class ExtendableEvent : public Event
{
  nsRefPtr<Promise> mPromise;

protected:
  explicit ExtendableEvent(mozilla::dom::EventTarget* aOwner);
  ~ExtendableEvent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ExtendableEvent, Event)
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return mozilla::dom::ExtendableEventBinding::Wrap(aCx, this);
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
  WaitUntil(Promise& aPromise);

  already_AddRefed<Promise>
  GetPromise() const
  {
    nsRefPtr<Promise> p = mPromise;
    return p.forget();
  }

  virtual ExtendableEvent* AsExtendableEvent() MOZ_OVERRIDE
  {
    return this;
  }
};

class InstallEvent MOZ_FINAL : public ExtendableEvent
{
  // FIXME(nsm): Bug 982787 will allow actually populating this.
  nsRefPtr<ServiceWorker> mActiveWorker;
  bool mActivateImmediately;

protected:
  explicit InstallEvent(mozilla::dom::EventTarget* aOwner);
  ~InstallEvent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InstallEvent, ExtendableEvent)
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return mozilla::dom::InstallEventBinding::Wrap(aCx, this);
  }

  static already_AddRefed<InstallEvent>
  Constructor(mozilla::dom::EventTarget* aOwner,
              const nsAString& aType,
              const InstallEventInit& aOptions)
  {
    nsRefPtr<InstallEvent> e = new InstallEvent(aOwner);
    bool trusted = e->Init(aOwner);
    e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
    e->SetTrusted(trusted);
    e->mActiveWorker = aOptions.mActiveWorker;
    return e.forget();
  }

  static already_AddRefed<InstallEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const InstallEventInit& aOptions,
              ErrorResult& aRv)
  {
    nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
    return Constructor(owner, aType, aOptions);
  }

  already_AddRefed<ServiceWorker>
  GetActiveWorker() const
  {
    nsRefPtr<ServiceWorker> sw = mActiveWorker;
    return sw.forget();
  }

  void
  Replace()
  {
    mActivateImmediately = true;
  };

  bool
  ActivateImmediately() const
  {
    return mActivateImmediately;
  }

  InstallEvent* AsInstallEvent() MOZ_OVERRIDE
  {
    return this;
  }
};

END_WORKERS_NAMESPACE
#endif /* mozilla_dom_workers_serviceworkerevents_h__ */
