/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerprivate_h
#define mozilla_dom_workers_serviceworkerprivate_h

#include "nsCOMPtr.h"

#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerInfo;

class LifeCycleEventCallback : public nsRunnable
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
// To spin up the worker thread we own a |WorkerPrivate| object which can be
// cancelled if no events are received for a certain amount of time. Note
// that, at the moment, we never close the worker due to inactivity.
//
// Adding an API function for a new event requires calling |SpawnWorkerIfNeeded|
// before any runnable is dispatched to the worker.
class ServiceWorkerPrivate final
{
public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerPrivate)

  explicit ServiceWorkerPrivate(ServiceWorkerInfo* aInfo);

  nsresult
  SendMessageEvent(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Optional<Sequence<JS::Value>>& aTransferable,
                   UniquePtr<ServiceWorkerClientInfo>&& aClientInfo);

  // This is used to validate the worker script and continue the installation
  // process. Note that the callback is dispatched to the main thread
  // ONLY if the evaluation was successful. Failure is handled by the JS
  // exception handler which will call ServiceWorkerManager::HandleError.
  nsresult
  ContinueOnSuccessfulScriptEvaluation(nsRunnable* aCallback);

  nsresult
  SendLifeCycleEvent(const nsAString& aEventType,
                     LifeCycleEventCallback* aCallback,
                     nsIRunnable* aLoadFailure);

  nsresult
  SendPushEvent(const Maybe<nsTArray<uint8_t>>& aData);

  nsresult
  SendPushSubscriptionChangeEvent();

  nsresult
  SendNotificationClickEvent(const nsAString& aID,
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
                 UniquePtr<ServiceWorkerClientInfo>&& aClientInfo,
                 bool aIsReload);

  // This will terminate the current running worker thread and drop the
  // workerPrivate reference.
  // Called by ServiceWorkerInfo when [[Clear Registration]] is invoked
  // or whenever the spec mandates that we terminate the worker.
  // This is a no-op if the worker has already been stopped.
  void
  TerminateWorker();

private:
  enum WakeUpReason {
    FetchEvent = 0,
    PushEvent,
    PushSubscriptionChangeEvent,
    MessageEvent,
    NotificationClickEvent,
    LifeCycleEvent
  };

  // |aLoadFailedRunnable| is a runnable dispatched to the main thread
  // if the script loader failed for some reason, but can be null.
  nsresult
  SpawnWorkerIfNeeded(WakeUpReason aWhy,
                      nsIRunnable* aLoadFailedRunnable,
                      nsILoadGroup* aLoadGroup = nullptr);

  ~ServiceWorkerPrivate()
  {
    MOZ_ASSERT(!mWorkerPrivate);
  }

  // The info object owns us - this should never be null.
  ServiceWorkerInfo* MOZ_NON_OWNING_REF mInfo;

  // The WorkerPrivate object can only be closed by this class or by the
  // RuntimeService class if gecko is shutting down. Closing the worker
  // multiple times is OK, since the second attempt will be a no-op.
  nsRefPtr<WorkerPrivate> mWorkerPrivate;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerprivate_h
