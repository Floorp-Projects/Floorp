/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerprivate_h
#define mozilla_dom_workers_serviceworkerprivate_h

#include "nsCOMPtr.h"

#include "WorkerPrivate.h"

#define NOTIFICATION_CLICK_EVENT_NAME "notificationclick"
#define NOTIFICATION_CLOSE_EVENT_NAME "notificationclose"

class nsIInterceptedChannel;

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerInfo;
class ServiceWorkerRegistrationInfo;
class KeepAliveToken;

class LifeCycleEventCallback : public Runnable
{
public:
  // Called on the worker thread.
  virtual void
  SetResult(bool aResult) = 0;
};

// ServiceWorkerPrivate is a wrapper for managing the on-demand aspect of
// service workers. It handles all event dispatching to the worker and ensures
// the worker thread is running when needed.
//
// Lifetime management: To spin up the worker thread we own a |WorkerPrivate|
// object which can be cancelled if no events are received for a certain
// amount of time. The worker is kept alive by holding a |KeepAliveToken|
// reference.
//
// Extendable events hold tokens for the duration of their handler execution
// and until their waitUntil promise is resolved, while ServiceWorkerPrivate
// will hold a token for |dom.serviceWorkers.idle_timeout| seconds after each
// new event.
//
// Note: All timer events must be handled on the main thread because the
// worker may block indefinitely the worker thread (e. g. infinite loop in the
// script).
//
// There are 3 cases where we may ignore keep alive tokens:
// 1. When ServiceWorkerPrivate's token expired, if there are still waitUntil
// handlers holding tokens, we wait another |dom.serviceWorkers.idle_extended_timeout|
// seconds before forcibly terminating the worker.
// 2. If the worker stopped controlling documents and it is not handling push
// events.
// 3. The content process is shutting down.
//
// Adding an API function for a new event requires calling |SpawnWorkerIfNeeded|
// with an appropriate reason before any runnable is dispatched to the worker.
// If the event is extendable then the runnable should inherit
// ExtendableEventWorkerRunnable.
class ServiceWorkerPrivate final : public nsISupports
{
  friend class KeepAliveToken;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ServiceWorkerPrivate)

  explicit ServiceWorkerPrivate(ServiceWorkerInfo* aInfo);

  nsresult
  SendMessageEvent(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Optional<Sequence<JS::Value>>& aTransferable,
                   UniquePtr<ServiceWorkerClientInfo>&& aClientInfo);

  // This is used to validate the worker script and continue the installation
  // process.
  nsresult
  CheckScriptEvaluation(LifeCycleEventCallback* aCallback);

  nsresult
  SendLifeCycleEvent(const nsAString& aEventType,
                     LifeCycleEventCallback* aCallback,
                     nsIRunnable* aLoadFailure);

  nsresult
  SendPushEvent(const nsAString& aMessageId,
                const Maybe<nsTArray<uint8_t>>& aData,
                ServiceWorkerRegistrationInfo* aRegistration);

  nsresult
  SendPushSubscriptionChangeEvent();

  nsresult
  SendNotificationEvent(const nsAString& aEventName,
                        const nsAString& aID,
                        const nsAString& aTitle,
                        const nsAString& aDir,
                        const nsAString& aLang,
                        const nsAString& aBody,
                        const nsAString& aTag,
                        const nsAString& aIcon,
                        const nsAString& aData,
                        const nsAString& aBehavior,
                        const nsAString& aScope);

  nsresult
  SendFetchEvent(nsIInterceptedChannel* aChannel,
                 nsILoadGroup* aLoadGroup,
                 const nsAString& aDocumentId,
                 bool aIsReload);

  void
  StoreISupports(nsISupports* aSupports);

  void
  RemoveISupports(nsISupports* aSupports);

  // This will terminate the current running worker thread and drop the
  // workerPrivate reference.
  // Called by ServiceWorkerInfo when [[Clear Registration]] is invoked
  // or whenever the spec mandates that we terminate the worker.
  // This is a no-op if the worker has already been stopped.
  void
  TerminateWorker();

  void
  NoteDeadServiceWorkerInfo();

  void
  NoteStoppedControllingDocuments();

  void
  Activated();

  nsresult
  GetDebugger(nsIWorkerDebugger** aResult);

  nsresult
  AttachDebugger();

  nsresult
  DetachDebugger();

private:
  enum WakeUpReason {
    FetchEvent = 0,
    PushEvent,
    PushSubscriptionChangeEvent,
    MessageEvent,
    NotificationClickEvent,
    NotificationCloseEvent,
    LifeCycleEvent,
    AttachEvent
  };

  // Timer callbacks
  static void
  NoteIdleWorkerCallback(nsITimer* aTimer, void* aPrivate);

  static void
  TerminateWorkerCallback(nsITimer* aTimer, void *aPrivate);

  void
  RenewKeepAliveToken(WakeUpReason aWhy);

  void
  ResetIdleTimeout();

  void
  AddToken();

  void
  ReleaseToken();

  // |aLoadFailedRunnable| is a runnable dispatched to the main thread
  // if the script loader failed for some reason, but can be null.
  nsresult
  SpawnWorkerIfNeeded(WakeUpReason aWhy,
                      nsIRunnable* aLoadFailedRunnable,
                      nsILoadGroup* aLoadGroup = nullptr);

  ~ServiceWorkerPrivate();

  already_AddRefed<KeepAliveToken>
  CreateEventKeepAliveToken();

  // The info object owns us. It is possible to outlive it for a brief period
  // of time if there are pending waitUntil promises, in which case it
  // will be null and |SpawnWorkerIfNeeded| will always fail.
  ServiceWorkerInfo* MOZ_NON_OWNING_REF mInfo;

  // The WorkerPrivate object can only be closed by this class or by the
  // RuntimeService class if gecko is shutting down. Closing the worker
  // multiple times is OK, since the second attempt will be a no-op.
  RefPtr<WorkerPrivate> mWorkerPrivate;

  nsCOMPtr<nsITimer> mIdleWorkerTimer;

  // We keep a token for |dom.serviceWorkers.idle_timeout| seconds to give the
  // worker a grace period after each event.
  RefPtr<KeepAliveToken> mIdleKeepAliveToken;

  uint64_t mDebuggerCount;

  uint64_t mTokenCount;

  // Meant for keeping objects alive while handling requests from the worker
  // on the main thread. Access to this array is provided through
  // |StoreISupports| and |RemoveISupports|. Note that the array is also
  // cleared whenever the worker is terminated.
  nsTArray<nsCOMPtr<nsISupports>> mSupportsArray;

  // Array of function event worker runnables that are pending due to
  // the worker activating.  Main thread only.
  nsTArray<RefPtr<WorkerRunnable>> mPendingFunctionalEvents;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerprivate_h
