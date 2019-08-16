/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerprivateimpl_h__
#define mozilla_dom_serviceworkerprivateimpl_h__

#include <functional>

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

#include "ServiceWorkerPrivate.h"
#include "mozilla/Attributes.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/RemoteWorkerController.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"

class nsIInterceptedChannel;

namespace mozilla {

template <typename T>
class Maybe;

namespace ipc {

class AutoIPCStream;

}  // namespace ipc

namespace dom {

class ClientInfoAndState;
class LifeCycleEventCallback;
class RemoteWorkerControllerChild;
class ServiceWorkerCloneData;
class ServiceWorkerRegistrationInfo;

class ServiceWorkerPrivateImpl final : public ServiceWorkerPrivate::Inner,
                                       public RemoteWorkerObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerPrivateImpl, override);

  explicit ServiceWorkerPrivateImpl(RefPtr<ServiceWorkerPrivate> aOuter);

  nsresult Initialize();

  RefPtr<GenericPromise> SetSkipWaitingFlag();

 private:
  class RAIIActorPtrHolder;

  ~ServiceWorkerPrivateImpl();

  /**
   * ServiceWorkerPrivate::Inner
   */
  nsresult SendMessageEvent(
      RefPtr<ServiceWorkerCloneData>&& aData,
      const ClientInfoAndState& aClientInfoAndState) override;

  nsresult CheckScriptEvaluation(
      RefPtr<LifeCycleEventCallback> aCallback) override;

  nsresult SendLifeCycleEvent(
      const nsAString& aEventName,
      RefPtr<LifeCycleEventCallback> aCallback) override;

  nsresult SendPushEvent(RefPtr<ServiceWorkerRegistrationInfo> aRegistration,
                         const nsAString& aMessageId,
                         const Maybe<nsTArray<uint8_t>>& aData) override;

  nsresult SendPushSubscriptionChangeEvent() override;

  nsresult SendNotificationEvent(const nsAString& aEventName,
                                 const nsAString& aID, const nsAString& aTitle,
                                 const nsAString& aDir, const nsAString& aLang,
                                 const nsAString& aBody, const nsAString& aTag,
                                 const nsAString& aIcon, const nsAString& aData,
                                 const nsAString& aBehavior,
                                 const nsAString& aScope,
                                 uint32_t aDisableOpenClickDelay) override;

  nsresult SendFetchEvent(RefPtr<ServiceWorkerRegistrationInfo> aRegistration,
                          nsCOMPtr<nsIInterceptedChannel> aChannel,
                          const nsAString& aClientId,
                          const nsAString& aResultingClientId,
                          bool aIsReload) override;

  void TerminateWorker() override;

  void UpdateState(ServiceWorkerState aState) override;

  void NoteDeadOuter() override;

  bool WorkerIsDead() const override;

  /**
   * RemoteWorkerObserver
   */
  void CreationFailed() override;

  void CreationSucceeded() override;

  void ErrorReceived(const ErrorValue& aError) override;

  void Terminated() override;

  // Refreshes only the parts of mRemoteWorkerData that may change over time.
  nsresult RefreshRemoteWorkerData(
      const RefPtr<ServiceWorkerRegistrationInfo>& aRegistration);

  nsresult SpawnWorkerIfNeeded();

  nsresult SendPushEventInternal(
      RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
      ServiceWorkerPushEventOpArgs&& aArgs);

  nsresult SendFetchEventInternal(
      RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
      ServiceWorkerFetchEventOpArgs&& aArgs,
      nsCOMPtr<nsIInterceptedChannel>&& aChannel,
      UniquePtr<mozilla::ipc::AutoIPCStream>&& aAutoStream);

  void Shutdown();

  RefPtr<GenericNonExclusivePromise> ShutdownInternal();

  nsresult ExecServiceWorkerOp(
      ServiceWorkerOpArgs&& aArgs,
      std::function<void(ServiceWorkerOpResult&&)>&& aSuccessCallback,
      std::function<void()>&& aFailureCallback = [] {});

  class PendingFunctionalEvent {
   public:
    PendingFunctionalEvent(
        ServiceWorkerPrivateImpl* aOwner,
        RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration);

    virtual ~PendingFunctionalEvent();

    virtual nsresult Send() = 0;

   protected:
    ServiceWorkerPrivateImpl* const MOZ_NON_OWNING_REF mOwner;
    RefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  };

  class PendingPushEvent final : public PendingFunctionalEvent {
   public:
    PendingPushEvent(ServiceWorkerPrivateImpl* aOwner,
                     RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
                     ServiceWorkerPushEventOpArgs&& aArgs);

    nsresult Send() override;

   private:
    ServiceWorkerPushEventOpArgs mArgs;
  };

  class PendingFetchEvent final : public PendingFunctionalEvent {
   public:
    PendingFetchEvent(ServiceWorkerPrivateImpl* aOwner,
                      RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
                      ServiceWorkerFetchEventOpArgs&& aArgs,
                      nsCOMPtr<nsIInterceptedChannel>&& aChannel,
                      UniquePtr<AutoIPCStream>&& aAutoStream);

    nsresult Send() override;

    ~PendingFetchEvent();

   private:
    ServiceWorkerFetchEventOpArgs mArgs;
    nsCOMPtr<nsIInterceptedChannel> mChannel;
    UniquePtr<mozilla::ipc::AutoIPCStream> mAutoStream;
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

  RefPtr<ServiceWorkerPrivate> mOuter;

  RemoteWorkerData mRemoteWorkerData;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_serviceworkerprivateimpl_h__
