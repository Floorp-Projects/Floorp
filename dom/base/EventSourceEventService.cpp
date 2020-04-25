/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventSourceEventService.h"
#include "mozilla/StaticPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"

namespace mozilla {
namespace dom {

namespace {

StaticRefPtr<EventSourceEventService> gEventSourceEventService;

}  // anonymous namespace

class EventSourceBaseRunnable : public Runnable {
 public:
  EventSourceBaseRunnable(uint64_t aHttpChannelId, uint64_t aInnerWindowID)
      : Runnable("dom::EventSourceBaseRunnable"),
        mHttpChannelId(aHttpChannelId),
        mInnerWindowID(aInnerWindowID) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<EventSourceEventService> service =
        EventSourceEventService::GetOrCreate();
    MOZ_ASSERT(service);

    EventSourceEventService::EventSourceListeners listeners;

    service->GetListeners(mInnerWindowID, listeners);

    for (uint32_t i = 0; i < listeners.Length(); ++i) {
      DoWork(listeners[i]);
    }

    return NS_OK;
  }

 protected:
  ~EventSourceBaseRunnable() = default;

  virtual void DoWork(nsIEventSourceEventListener* aListener) = 0;

  uint64_t mHttpChannelId;
  uint64_t mInnerWindowID;
};

class EventSourceConnectionOpenedRunnable final
    : public EventSourceBaseRunnable {
 public:
  EventSourceConnectionOpenedRunnable(uint64_t aHttpChannelId,
                                      uint64_t aInnerWindowID)
      : EventSourceBaseRunnable(aHttpChannelId, aInnerWindowID) {}

 private:
  virtual void DoWork(nsIEventSourceEventListener* aListener) override {
    DebugOnly<nsresult> rv =
        aListener->EventSourceConnectionOpened(mHttpChannelId);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EventSourceConnectionOpened failed");
  }
};

class EventSourceConnectionClosedRunnable final
    : public EventSourceBaseRunnable {
 public:
  EventSourceConnectionClosedRunnable(uint64_t aHttpChannelId,
                                      uint64_t aInnerWindowID)
      : EventSourceBaseRunnable(aHttpChannelId, aInnerWindowID) {}

 private:
  virtual void DoWork(nsIEventSourceEventListener* aListener) override {
    DebugOnly<nsresult> rv =
        aListener->EventSourceConnectionClosed(mHttpChannelId);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EventSourceConnectionClosed failed");
  }
};

class EventSourceEventRunnable final : public EventSourceBaseRunnable {
 public:
  EventSourceEventRunnable(uint64_t aHttpChannelId, uint64_t aInnerWindowID,
                           const nsAString& aEventName,
                           const nsAString& aLastEventID,
                           const nsAString& aData, uint32_t aRetry,
                           DOMHighResTimeStamp aTimeStamp)
      : EventSourceBaseRunnable(aHttpChannelId, aInnerWindowID),
        mEventName(aEventName),
        mLastEventID(aLastEventID),
        mData(aData),
        mRetry(aRetry),
        mTimeStamp(aTimeStamp) {}

 private:
  virtual void DoWork(nsIEventSourceEventListener* aListener) override {
    DebugOnly<nsresult> rv = aListener->EventReceived(
        mHttpChannelId, mEventName, mLastEventID, mData, mRetry, mTimeStamp);

    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Event op failed");
  }

  nsString mEventName;
  nsString mLastEventID;
  nsString mData;
  uint32_t mRetry;
  DOMHighResTimeStamp mTimeStamp;
};

/* static */
already_AddRefed<EventSourceEventService>
EventSourceEventService::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gEventSourceEventService) {
    gEventSourceEventService = new EventSourceEventService();
  }

  RefPtr<EventSourceEventService> service = gEventSourceEventService.get();
  return service.forget();
}

NS_INTERFACE_MAP_BEGIN(EventSourceEventService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEventSourceEventService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIEventSourceEventService)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(EventSourceEventService)
NS_IMPL_RELEASE(EventSourceEventService)

EventSourceEventService::EventSourceEventService() : mCountListeners(0) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "inner-window-destroyed", false);
  }
}

EventSourceEventService::~EventSourceEventService() {
  MOZ_ASSERT(NS_IsMainThread());
}

void EventSourceEventService::EventSourceConnectionOpened(
    uint64_t aHttpChannelId, uint64_t aInnerWindowID) {
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<EventSourceConnectionOpenedRunnable> runnable =
      new EventSourceConnectionOpenedRunnable(aHttpChannelId, aInnerWindowID);
  DebugOnly<nsresult> rv = NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

void EventSourceEventService::EventSourceConnectionClosed(
    uint64_t aHttpChannelId, uint64_t aInnerWindowID) {
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }
  RefPtr<EventSourceConnectionClosedRunnable> runnable =
      new EventSourceConnectionClosedRunnable(aHttpChannelId, aInnerWindowID);
  DebugOnly<nsresult> rv = NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

void EventSourceEventService::EventReceived(
    uint64_t aHttpChannelId, uint64_t aInnerWindowID,
    const nsAString& aEventName, const nsAString& aLastEventID,
    const nsAString& aData, uint32_t aRetry, DOMHighResTimeStamp aTimeStamp) {
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<EventSourceEventRunnable> runnable =
      new EventSourceEventRunnable(aHttpChannelId, aInnerWindowID, aEventName,
                                   aLastEventID, aData, aRetry, aTimeStamp);
  DebugOnly<nsresult> rv = NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

NS_IMETHODIMP
EventSourceEventService::AddListener(uint64_t aInnerWindowID,
                                     nsIEventSourceEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!aListener) {
    return NS_ERROR_FAILURE;
  }
  ++mCountListeners;

  WindowListener* listener = mWindows.Get(aInnerWindowID);
  if (!listener) {
    listener = new WindowListener();
    mWindows.Put(aInnerWindowID, listener);
  }

  listener->mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
EventSourceEventService::RemoveListener(
    uint64_t aInnerWindowID, nsIEventSourceEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener) {
    return NS_ERROR_FAILURE;
  }

  WindowListener* listener = mWindows.Get(aInnerWindowID);
  if (!listener) {
    return NS_ERROR_FAILURE;
  }

  if (!listener->mListeners.RemoveElement(aListener)) {
    return NS_ERROR_FAILURE;
  }

  // The last listener for this window.
  if (listener->mListeners.IsEmpty()) {
    mWindows.Remove(aInnerWindowID);
  }

  MOZ_ASSERT(mCountListeners);
  --mCountListeners;

  return NS_OK;
}

NS_IMETHODIMP
EventSourceEventService::HasListenerFor(uint64_t aInnerWindowID,
                                        bool* aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  *aResult = mWindows.Get(aInnerWindowID);

  return NS_OK;
}

NS_IMETHODIMP
EventSourceEventService::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "xpcom-shutdown")) {
    Shutdown();
    return NS_OK;
  }

  if (!strcmp(aTopic, "inner-window-destroyed") && HasListeners()) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t innerID;
    nsresult rv = wrapper->GetData(&innerID);
    NS_ENSURE_SUCCESS(rv, rv);

    WindowListener* listener = mWindows.Get(innerID);
    if (!listener) {
      return NS_OK;
    }

    MOZ_ASSERT(mCountListeners >= listener->mListeners.Length());
    mCountListeners -= listener->mListeners.Length();
    mWindows.Remove(innerID);
  }

  // This should not happen.
  return NS_ERROR_FAILURE;
}

void EventSourceEventService::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (gEventSourceEventService) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(gEventSourceEventService, "xpcom-shutdown");
      obs->RemoveObserver(gEventSourceEventService, "inner-window-destroyed");
    }

    mWindows.Clear();
    gEventSourceEventService = nullptr;
  }
}

bool EventSourceEventService::HasListeners() const { return !!mCountListeners; }

void EventSourceEventService::GetListeners(
    uint64_t aInnerWindowID,
    EventSourceEventService::EventSourceListeners& aListeners) const {
  aListeners.Clear();

  WindowListener* listener = mWindows.Get(aInnerWindowID);
  if (!listener) {
    return;
  }

  aListeners.AppendElements(listener->mListeners);
}

}  // namespace dom
}  // namespace mozilla
