/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerprivate_h
#define mozilla_dom_serviceworkerprivate_h

#include <functional>
#include <type_traits>

#include "mozilla/Attributes.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/FetchService.h"
#include "mozilla/dom/RemoteWorkerController.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

#define NOTIFICATION_CLICK_EVENT_NAME u"notificationclick"
#define NOTIFICATION_CLOSE_EVENT_NAME u"notificationclose"

class nsIInterceptedChannel;
class nsIWorkerDebugger;

namespace mozilla {

template <typename T>
class Maybe;

class JSObjectHolder;

namespace dom {

class ClientInfoAndState;
class RemoteWorkerControllerChild;
class ServiceWorkerCloneData;
class ServiceWorkerInfo;
class ServiceWorkerPrivate;
class ServiceWorkerRegistrationInfo;

namespace ipc {
class StructuredCloneData;
}  // namespace ipc

class LifeCycleEventCallback : public Runnable {
 public:
  LifeCycleEventCallback() : Runnable("dom::LifeCycleEventCallback") {}

  // Called on the worker thread.
  virtual void SetResult(bool aResult) = 0;
};

// Used to keep track of pending waitUntil as well as in-flight extendable
// events. When the last token is released, we attempt to terminate the worker.
class KeepAliveToken final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  explicit KeepAliveToken(ServiceWorkerPrivate* aPrivate);

 private:
  ~KeepAliveToken();

  RefPtr<ServiceWorkerPrivate> mPrivate;
};

class ServiceWorkerPrivate final : public RemoteWorkerObserver {
  friend class KeepAliveToken;

 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerPrivate, override);

  using PromiseExtensionWorkerHasListener = MozPromise<bool, nsresult, false>;

 public:
  explicit ServiceWorkerPrivate(ServiceWorkerInfo* aInfo);

  nsresult SendMessageEvent(RefPtr<ServiceWorkerCloneData>&& aData,
                            const ClientInfoAndState& aClientInfoAndState);

  // This is used to validate the worker script and continue the installation
  // process.
  nsresult CheckScriptEvaluation(RefPtr<LifeCycleEventCallback> aCallback);

  nsresult SendLifeCycleEvent(const nsAString& aEventType,
                              RefPtr<LifeCycleEventCallback> aCallback);

  nsresult SendPushEvent(const nsAString& aMessageId,
                         const Maybe<nsTArray<uint8_t>>& aData,
                         RefPtr<ServiceWorkerRegistrationInfo> aRegistration);

  nsresult SendPushSubscriptionChangeEvent();

  nsresult SendNotificationEvent(const nsAString& aEventName,
                                 const nsAString& aID, const nsAString& aTitle,
                                 const nsAString& aDir, const nsAString& aLang,
                                 const nsAString& aBody, const nsAString& aTag,
                                 const nsAString& aIcon, const nsAString& aData,
                                 const nsAString& aBehavior,
                                 const nsAString& aScope);

  nsresult SendFetchEvent(nsCOMPtr<nsIInterceptedChannel> aChannel,
                          nsILoadGroup* aLoadGroup, const nsAString& aClientId,
                          const nsAString& aResultingClientId);

  Result<RefPtr<PromiseExtensionWorkerHasListener>, nsresult>
  WakeForExtensionAPIEvent(const nsAString& aExtensionAPINamespace,
                           const nsAString& aEXtensionAPIEventName);

  // This will terminate the current running worker thread and drop the
  // workerPrivate reference.
  // Called by ServiceWorkerInfo when [[Clear Registration]] is invoked
  // or whenever the spec mandates that we terminate the worker.
  // This is a no-op if the worker has already been stopped.
  void TerminateWorker();

  void NoteDeadServiceWorkerInfo();

  void NoteStoppedControllingDocuments();

  void UpdateState(ServiceWorkerState aState);

  nsresult GetDebugger(nsIWorkerDebugger** aResult);

  nsresult AttachDebugger();

  nsresult DetachDebugger();

  bool IsIdle() const;

  // This promise is used schedule clearing of the owning registrations and its
  // associated Service Workers if that registration becomes "unreachable" by
  // the ServiceWorkerManager. This occurs under two conditions, which are the
  // preconditions to calling this method:
  // - The owning registration must be unregistered.
  // - The associated Service Worker must *not* be controlling clients.
  //
  // Additionally, perhaps stating the obvious, the associated Service Worker
  // must *not* be idle (whatever must be done "when idle" can just be done
  // immediately).
  RefPtr<GenericPromise> GetIdlePromise();

  void SetHandlesFetch(bool aValue);

  RefPtr<GenericPromise> SetSkipWaitingFlag();

  static void RunningShutdown() {
    // Force a final update of the number of running ServiceWorkers
    UpdateRunning(0, 0);
    MOZ_ASSERT(sRunningServiceWorkers == 0);
    MOZ_ASSERT(sRunningServiceWorkersFetch == 0);
  }

  /**
   * Update Telemetry for # of running ServiceWorkers
   */
  static void UpdateRunning(int32_t aDelta, int32_t aFetchDelta);

 private:
  // Timer callbacks
  void NoteIdleWorkerCallback(nsITimer* aTimer);

  void TerminateWorkerCallback(nsITimer* aTimer);

  void RenewKeepAliveToken();

  void ResetIdleTimeout();

  void AddToken();

  void ReleaseToken();

  already_AddRefed<KeepAliveToken> CreateEventKeepAliveToken();

  nsresult SpawnWorkerIfNeeded();

  ~ServiceWorkerPrivate();

  nsresult Initialize();

  /**
   * RemoteWorkerObserver
   */
  void CreationFailed() override;

  void CreationSucceeded() override;

  void ErrorReceived(const ErrorValue& aError) override;

  void LockNotified(bool aCreated) final {
    // no-op for service workers
  }

  void WebTransportNotified(bool aCreated) final {
    // no-op for service workers
  }

  void Terminated() override;

  // Refreshes only the parts of mRemoteWorkerData that may change over time.
  void RefreshRemoteWorkerData(
      const RefPtr<ServiceWorkerRegistrationInfo>& aRegistration);

  nsresult SendPushEventInternal(
      RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
      ServiceWorkerPushEventOpArgs&& aArgs);

  // Setup the navigation preload by the intercepted channel and the
  // RegistrationInfo.
  RefPtr<FetchServicePromises> SetupNavigationPreload(
      nsCOMPtr<nsIInterceptedChannel>& aChannel,
      const RefPtr<ServiceWorkerRegistrationInfo>& aRegistration);

  nsresult SendFetchEventInternal(
      RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
      ParentToParentServiceWorkerFetchEventOpArgs&& aArgs,
      nsCOMPtr<nsIInterceptedChannel>&& aChannel,
      RefPtr<FetchServicePromises>&& aPreloadResponseReadyPromises);

  void Shutdown();

  RefPtr<GenericNonExclusivePromise> ShutdownInternal(
      uint32_t aShutdownStateId);

  nsresult ExecServiceWorkerOp(
      ServiceWorkerOpArgs&& aArgs,
      std::function<void(ServiceWorkerOpResult&&)>&& aSuccessCallback,
      std::function<void()>&& aFailureCallback = [] {});

  class PendingFunctionalEvent {
   public:
    PendingFunctionalEvent(
        ServiceWorkerPrivate* aOwner,
        RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration);

    virtual ~PendingFunctionalEvent();

    virtual nsresult Send() = 0;

   protected:
    ServiceWorkerPrivate* const MOZ_NON_OWNING_REF mOwner;
    RefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  };

  class PendingPushEvent final : public PendingFunctionalEvent {
   public:
    PendingPushEvent(ServiceWorkerPrivate* aOwner,
                     RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
                     ServiceWorkerPushEventOpArgs&& aArgs);

    nsresult Send() override;

   private:
    ServiceWorkerPushEventOpArgs mArgs;
  };

  class PendingFetchEvent final : public PendingFunctionalEvent {
   public:
    PendingFetchEvent(
        ServiceWorkerPrivate* aOwner,
        RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
        ParentToParentServiceWorkerFetchEventOpArgs&& aArgs,
        nsCOMPtr<nsIInterceptedChannel>&& aChannel,
        RefPtr<FetchServicePromises>&& aPreloadResponseReadyPromises);

    nsresult Send() override;

    ~PendingFetchEvent();

   private:
    ParentToParentServiceWorkerFetchEventOpArgs mArgs;
    nsCOMPtr<nsIInterceptedChannel> mChannel;
    // The promises from FetchService. It indicates if the preload response is
    // ready or not. The promise's resolve/reject value should be handled in
    // FetchEventOpChild, such that the preload result can be propagated to the
    // ServiceWorker through IPC. However, FetchEventOpChild creation could be
    // pending here, so this member is needed. And it will be forwarded to
    // FetchEventOpChild when crearting the FetchEventOpChild.
    RefPtr<FetchServicePromises> mPreloadResponseReadyPromises;
  };

  nsTArray<UniquePtr<PendingFunctionalEvent>> mPendingFunctionalEvents;

  /**
   * It's possible that there are still in-progress operations when a
   * a termination operation is issued. In this case, it's important to keep
   * the RemoteWorkerControllerChild actor alive until all pending operations
   * have completed before destroying it with Send__delete__().
   *
   * RAIIActorPtrHolder holds a singular, owning reference to a
   * RemoteWorkerControllerChild actor and is responsible for destroying the
   * actor in its (i.e. the holder's) destructor. This implies that all
   * in-progress operations must maintain a strong reference to their
   * corresponding holders and release the reference once completed/canceled.
   *
   * Additionally a RAIIActorPtrHolder must be initialized with a non-null actor
   * and cannot be moved or copied. Therefore, the identities of two held
   * actors can be compared by simply comparing their holders' addresses.
   */
  class RAIIActorPtrHolder final {
   public:
    NS_INLINE_DECL_REFCOUNTING(RAIIActorPtrHolder)

    explicit RAIIActorPtrHolder(
        already_AddRefed<RemoteWorkerControllerChild> aActor);

    RAIIActorPtrHolder(const RAIIActorPtrHolder& aOther) = delete;
    RAIIActorPtrHolder& operator=(const RAIIActorPtrHolder& aOther) = delete;

    RAIIActorPtrHolder(RAIIActorPtrHolder&& aOther) = delete;
    RAIIActorPtrHolder& operator=(RAIIActorPtrHolder&& aOther) = delete;

    RemoteWorkerControllerChild* operator->() const
        MOZ_NO_ADDREF_RELEASE_ON_RETURN;

    RemoteWorkerControllerChild* get() const;

    RefPtr<GenericPromise> OnDestructor();

   private:
    ~RAIIActorPtrHolder();

    MozPromiseHolder<GenericPromise> mDestructorPromiseHolder;

    const RefPtr<RemoteWorkerControllerChild> mActor;
  };

  RefPtr<RAIIActorPtrHolder> mControllerChild;

  RemoteWorkerData mRemoteWorkerData;

  TimeStamp mServiceWorkerLaunchTimeStart;

  // Counters for Telemetry - totals running simultaneously, and those that
  // handle Fetch, plus Max values for each
  static uint32_t sRunningServiceWorkers;
  static uint32_t sRunningServiceWorkersFetch;
  static uint32_t sRunningServiceWorkersMax;
  static uint32_t sRunningServiceWorkersFetchMax;

  // We know the state after we've evaluated the worker, and we then store
  // it in the registration.  The only valid state transition should be
  // from Unknown to Enabled or Disabled.
  enum { Unknown, Enabled, Disabled } mHandlesFetch{Unknown};

  // The info object owns us. It is possible to outlive it for a brief period
  // of time if there are pending waitUntil promises, in which case it
  // will be null and |SpawnWorkerIfNeeded| will always fail.
  ServiceWorkerInfo* MOZ_NON_OWNING_REF mInfo;

  nsCOMPtr<nsITimer> mIdleWorkerTimer;

  // We keep a token for |dom.serviceWorkers.idle_timeout| seconds to give the
  // worker a grace period after each event.
  RefPtr<KeepAliveToken> mIdleKeepAliveToken;

  uint64_t mDebuggerCount;

  uint64_t mTokenCount;

  // Used by the owning `ServiceWorkerRegistrationInfo` when it wants to call
  // `Clear` after being unregistered and isn't controlling any clients but this
  // worker (i.e. the registration's active worker) isn't idle yet. Note that
  // such an event should happen at most once in a
  // `ServiceWorkerRegistrationInfo`s lifetime, so this promise should also only
  // be obtained at most once.
  MozPromiseHolder<GenericPromise> mIdlePromiseHolder;

#ifdef DEBUG
  bool mIdlePromiseObtained = false;
#endif
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_serviceworkerprivate_h
