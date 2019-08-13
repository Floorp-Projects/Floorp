/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistration.h"

#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PushManager.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "RemoteServiceWorkerRegistrationImpl.h"
#include "ServiceWorkerRegistrationImpl.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistration,
                                   DOMEventTargetHelper, mInstallingWorker,
                                   mWaitingWorker, mActiveWorker, mPushManager);

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerRegistration)
  NS_INTERFACE_MAP_ENTRY(ServiceWorkerRegistration)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

namespace {
const uint64_t kInvalidUpdateFoundId = 0;
}  // anonymous namespace

ServiceWorkerRegistration::ServiceWorkerRegistration(
    nsIGlobalObject* aGlobal,
    const ServiceWorkerRegistrationDescriptor& aDescriptor,
    ServiceWorkerRegistration::Inner* aInner)
    : DOMEventTargetHelper(aGlobal),
      mDescriptor(aDescriptor),
      mInner(aInner),
      mScheduledUpdateFoundId(kInvalidUpdateFoundId),
      mDispatchedUpdateFoundId(kInvalidUpdateFoundId) {
  MOZ_DIAGNOSTIC_ASSERT(mInner);

  KeepAliveIfHasListenersFor(NS_LITERAL_STRING("updatefound"));

  UpdateState(mDescriptor);
  mInner->SetServiceWorkerRegistration(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() {
  mInner->ClearServiceWorkerRegistration(this);
}

JSObject* ServiceWorkerRegistration::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ServiceWorkerRegistration_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerRegistration::CreateForMainThread(
    nsPIDOMWindowInner* aWindow,
    const ServiceWorkerRegistrationDescriptor& aDescriptor) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Inner> inner;
  if (ServiceWorkerParentInterceptEnabled()) {
    inner = new RemoteServiceWorkerRegistrationImpl(aDescriptor);
  } else {
    inner = new ServiceWorkerRegistrationMainThread(aDescriptor);
  }
  NS_ENSURE_TRUE(inner, nullptr);

  RefPtr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(aWindow->AsGlobal(), aDescriptor, inner);

  return registration.forget();
}

/* static */
already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerRegistration::CreateForWorker(
    WorkerPrivate* aWorkerPrivate, nsIGlobalObject* aGlobal,
    const ServiceWorkerRegistrationDescriptor& aDescriptor) {
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<Inner> inner;
  if (ServiceWorkerParentInterceptEnabled()) {
    inner = new RemoteServiceWorkerRegistrationImpl(aDescriptor);
  } else {
    inner = new ServiceWorkerRegistrationWorkerThread(aDescriptor);
  }
  NS_ENSURE_TRUE(inner, nullptr);

  RefPtr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(aGlobal, aDescriptor, inner);

  return registration.forget();
}

void ServiceWorkerRegistration::DisconnectFromOwner() {
  DOMEventTargetHelper::DisconnectFromOwner();
}

void ServiceWorkerRegistration::RegistrationCleared() {
  // Its possible that the registration will fail to install and be
  // immediately removed.  In that case we may never receive the
  // UpdateState() call if the actor was too slow to connect, etc.
  // Ensure that we force all our known actors to redundant so that
  // the appropriate statechange events are fired.  If we got the
  // UpdateState() already then this will be a no-op.
  UpdateStateInternal(Maybe<ServiceWorkerDescriptor>(),
                      Maybe<ServiceWorkerDescriptor>(),
                      Maybe<ServiceWorkerDescriptor>());

  // Our underlying registration was removed from SWM, so we
  // will never get an updatefound event again.  We can let
  // the object GC if content is not holding it alive.
  IgnoreKeepAliveIfHasListenersFor(NS_LITERAL_STRING("updatefound"));
}

already_AddRefed<ServiceWorker> ServiceWorkerRegistration::GetInstalling()
    const {
  RefPtr<ServiceWorker> ref = mInstallingWorker;
  return ref.forget();
}

already_AddRefed<ServiceWorker> ServiceWorkerRegistration::GetWaiting() const {
  RefPtr<ServiceWorker> ref = mWaitingWorker;
  return ref.forget();
}

already_AddRefed<ServiceWorker> ServiceWorkerRegistration::GetActive() const {
  RefPtr<ServiceWorker> ref = mActiveWorker;
  return ref.forget();
}

void ServiceWorkerRegistration::UpdateState(
    const ServiceWorkerRegistrationDescriptor& aDescriptor) {
  MOZ_DIAGNOSTIC_ASSERT(MatchesDescriptor(aDescriptor));

  mDescriptor = aDescriptor;

  UpdateStateInternal(aDescriptor.GetInstalling(), aDescriptor.GetWaiting(),
                      aDescriptor.GetActive());

  nsTArray<UniquePtr<VersionCallback>> callbackList;
  mVersionCallbackList.SwapElements(callbackList);
  for (auto& cb : callbackList) {
    if (cb->mVersion > mDescriptor.Version()) {
      mVersionCallbackList.AppendElement(std::move(cb));
      continue;
    }

    cb->mFunc(cb->mVersion == mDescriptor.Version());
  }
}

bool ServiceWorkerRegistration::MatchesDescriptor(
    const ServiceWorkerRegistrationDescriptor& aDescriptor) const {
  return aDescriptor.Id() == mDescriptor.Id() &&
         aDescriptor.PrincipalInfo() == mDescriptor.PrincipalInfo() &&
         aDescriptor.Scope() == mDescriptor.Scope();
}

void ServiceWorkerRegistration::GetScope(nsAString& aScope) const {
  CopyUTF8toUTF16(mDescriptor.Scope(), aScope);
}

ServiceWorkerUpdateViaCache ServiceWorkerRegistration::GetUpdateViaCache(
    ErrorResult& aRv) const {
  return mDescriptor.UpdateViaCache();
}

already_AddRefed<Promise> ServiceWorkerRegistration::Update(ErrorResult& aRv) {
  if (!mInner) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsIGlobalObject* global = GetParentObject();
  if (!global) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<Promise> outer = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  /**
   * `ServiceWorker` objects are not exposed on worker threads yet, so calling
   * `ServiceWorkerRegistration::Get{Installing,Waiting,Active}` won't work.
   */
  const bool hasNewestWorker = mDescriptor.GetInstalling() ||
                               mDescriptor.GetWaiting() ||
                               mDescriptor.GetActive();

  /**
   * If newestWorker is null, return a promise rejected with an
   * "InvalidStateError" DOMException and abort these steps.
   */
  if (!hasNewestWorker) {
    outer->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return outer.forget();
  }

  /**
   * If the context object’s relevant settings object’s global object
   * globalObject is a ServiceWorkerGlobalScope object, and globalObject’s
   * associated service worker's state is "installing", return a promise
   * rejected with an "InvalidStateError" DOMException and abort these steps.
   */
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    if (workerPrivate->IsServiceWorker() &&
        (workerPrivate->GetServiceWorkerDescriptor().State() ==
         ServiceWorkerState::Installing)) {
      outer->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
      return outer.forget();
    }
  }

  RefPtr<ServiceWorkerRegistration> self = this;

  mInner->Update(
      [outer, self](const ServiceWorkerRegistrationDescriptor& aDesc) {
        nsIGlobalObject* global = self->GetParentObject();
        MOZ_DIAGNOSTIC_ASSERT(global);
        RefPtr<ServiceWorkerRegistration> ref =
            global->GetOrCreateServiceWorkerRegistration(aDesc);
        if (!ref) {
          outer->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
          return;
        }
        outer->MaybeResolve(ref);
      },
      [outer, self](ErrorResult& aRv) { outer->MaybeReject(aRv); });

  return outer.forget();
}

already_AddRefed<Promise> ServiceWorkerRegistration::Unregister(
    ErrorResult& aRv) {
  nsIGlobalObject* global = GetParentObject();
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<Promise> outer = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!mInner) {
    outer->MaybeResolve(false);
    return outer.forget();
  }

  mInner->Unregister([outer](bool aSuccess) { outer->MaybeResolve(aSuccess); },
                     [outer](ErrorResult& aRv) {
                       // register() should be resilient and resolve false
                       // instead of rejecting in most cases.
                       outer->MaybeResolve(false);
                     });

  return outer.forget();
}

already_AddRefed<PushManager> ServiceWorkerRegistration::GetPushManager(
    JSContext* aCx, ErrorResult& aRv) {
  if (!mPushManager) {
    nsCOMPtr<nsIGlobalObject> globalObject = GetParentObject();

    if (!globalObject) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }

    GlobalObject global(aCx, globalObject->GetGlobalJSObject());
    mPushManager = PushManager::Constructor(
        global, NS_ConvertUTF8toUTF16(mDescriptor.Scope()), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  RefPtr<PushManager> ret = mPushManager;
  return ret.forget();
}

already_AddRefed<Promise> ServiceWorkerRegistration::ShowNotification(
    JSContext* aCx, const nsAString& aTitle,
    const NotificationOptions& aOptions, ErrorResult& aRv) {
  nsIGlobalObject* global = GetParentObject();
  if (!global) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  NS_ConvertUTF8toUTF16 scope(mDescriptor.Scope());

  // Until we ship ServiceWorker objects on worker threads the active
  // worker will always be nullptr.  So limit this check to main
  // thread for now.
  if (mDescriptor.GetActive().isNothing() && NS_IsMainThread()) {
    aRv.ThrowTypeError<MSG_NO_ACTIVE_WORKER>(scope);
    return nullptr;
  }

  RefPtr<Promise> p = Notification::ShowPersistentNotification(
      aCx, global, scope, aTitle, aOptions, mDescriptor, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return p.forget();
}

already_AddRefed<Promise> ServiceWorkerRegistration::GetNotifications(
    const GetNotificationOptions& aOptions, ErrorResult& aRv) {
  nsIGlobalObject* global = GetParentObject();
  if (!global) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  NS_ConvertUTF8toUTF16 scope(mDescriptor.Scope());

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
    if (NS_WARN_IF(!window)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }
    return Notification::Get(window, aOptions, scope, aRv);
  }

  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  worker->AssertIsOnWorkerThread();
  return Notification::WorkerGet(worker, aOptions, scope, aRv);
}

const ServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistration::Descriptor() const {
  return mDescriptor;
}

void ServiceWorkerRegistration::WhenVersionReached(
    uint64_t aVersion, ServiceWorkerBoolCallback&& aCallback) {
  if (aVersion <= mDescriptor.Version()) {
    aCallback(aVersion == mDescriptor.Version());
    return;
  }

  mVersionCallbackList.AppendElement(
      MakeUnique<VersionCallback>(aVersion, std::move(aCallback)));
}

void ServiceWorkerRegistration::MaybeScheduleUpdateFound(
    const Maybe<ServiceWorkerDescriptor>& aInstallingDescriptor) {
  // This function sets mScheduledUpdateFoundId to note when we were told about
  // a new installing worker. We rely on a call to
  // MaybeDispatchUpdateFoundRunnable (called indirectly from UpdateJobCallback)
  // to actually fire the event.
  uint64_t newId = aInstallingDescriptor.isSome()
                       ? aInstallingDescriptor.ref().Id()
                       : kInvalidUpdateFoundId;

  if (mScheduledUpdateFoundId != kInvalidUpdateFoundId) {
    if (mScheduledUpdateFoundId == newId) {
      return;
    }
    MaybeDispatchUpdateFound();
    MOZ_DIAGNOSTIC_ASSERT(mScheduledUpdateFoundId == kInvalidUpdateFoundId);
  }

  bool updateFound =
      newId != kInvalidUpdateFoundId && mDispatchedUpdateFoundId != newId;

  if (!updateFound) {
    return;
  }

  mScheduledUpdateFoundId = newId;
}

void ServiceWorkerRegistration::MaybeDispatchUpdateFoundRunnable() {
  if (mScheduledUpdateFoundId == kInvalidUpdateFoundId) {
    return;
  }

  nsIGlobalObject* global = GetParentObject();
  NS_ENSURE_TRUE_VOID(global);

  nsCOMPtr<nsIRunnable> r = NewCancelableRunnableMethod(
      "ServiceWorkerRegistration::MaybeDispatchUpdateFound", this,
      &ServiceWorkerRegistration::MaybeDispatchUpdateFound);

  Unused << global->EventTargetFor(TaskCategory::Other)
                ->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void ServiceWorkerRegistration::MaybeDispatchUpdateFound() {
  uint64_t scheduledId = mScheduledUpdateFoundId;
  mScheduledUpdateFoundId = kInvalidUpdateFoundId;

  if (scheduledId == kInvalidUpdateFoundId ||
      scheduledId == mDispatchedUpdateFoundId) {
    return;
  }

  mDispatchedUpdateFoundId = scheduledId;
  DispatchTrustedEvent(NS_LITERAL_STRING("updatefound"));
}

void ServiceWorkerRegistration::UpdateStateInternal(
    const Maybe<ServiceWorkerDescriptor>& aInstalling,
    const Maybe<ServiceWorkerDescriptor>& aWaiting,
    const Maybe<ServiceWorkerDescriptor>& aActive) {
  // Do this immediately as it may flush an already pending updatefound
  // event.  In that case we want to fire the pending event before
  // modifying any of the registration properties.
  MaybeScheduleUpdateFound(aInstalling);

  // Move the currently exposed workers into a separate list
  // of "old" workers.  We will then potentially add them
  // back to the registration properties below based on the
  // given descriptor.  Any that are not restored will need
  // to be moved to the redundant state.
  AutoTArray<RefPtr<ServiceWorker>, 3> oldWorkerList({
      mInstallingWorker.forget(),
      mWaitingWorker.forget(),
      mActiveWorker.forget(),
  });

  // Its important that all state changes are actually applied before
  // dispatching any statechange events.  Each ServiceWorker object
  // should be in the correct state and the ServiceWorkerRegistration
  // properties need to be set correctly as well.  To accomplish this
  // we use a ScopeExit to dispatch any statechange events.
  auto scopeExit = MakeScopeExit([&] {
    // Check to see if any of the "old" workers was completely discarded.
    // Set these workers to the redundant state.
    for (auto& oldWorker : oldWorkerList) {
      if (!oldWorker || oldWorker == mInstallingWorker ||
          oldWorker == mWaitingWorker || oldWorker == mActiveWorker) {
        continue;
      }

      oldWorker->SetState(ServiceWorkerState::Redundant);
    }

    // Check each worker to see if it needs a statechange event dispatched.
    if (mInstallingWorker) {
      mInstallingWorker->MaybeDispatchStateChangeEvent();
    }
    if (mWaitingWorker) {
      mWaitingWorker->MaybeDispatchStateChangeEvent();
    }
    if (mActiveWorker) {
      mActiveWorker->MaybeDispatchStateChangeEvent();
    }

    // We also check the "old" workers to see if they need a statechange
    // event as well.  Note, these may overlap with the known worker properties
    // above, but MaybeDispatchStateChangeEvent() will ignore duplicated calls.
    for (auto& oldWorker : oldWorkerList) {
      if (!oldWorker) {
        continue;
      }

      oldWorker->MaybeDispatchStateChangeEvent();
    }
  });

  // Clear all workers if the registration has been detached from the global.
  // Also, we cannot expose ServiceWorker objects on worker threads yet, so
  // do the same on when off-main-thread.  This main thread check should be
  // removed as part of bug 1113522.
  nsCOMPtr<nsIGlobalObject> global = GetParentObject();
  if (!global || !NS_IsMainThread()) {
    return;
  }

  if (aActive.isSome()) {
    if ((mActiveWorker = global->GetOrCreateServiceWorker(aActive.ref()))) {
      mActiveWorker->SetState(aActive.ref().State());
    }
  } else {
    mActiveWorker = nullptr;
  }

  if (aWaiting.isSome()) {
    if ((mWaitingWorker = global->GetOrCreateServiceWorker(aWaiting.ref()))) {
      mWaitingWorker->SetState(aWaiting.ref().State());
    }
  } else {
    mWaitingWorker = nullptr;
  }

  if (aInstalling.isSome()) {
    if ((mInstallingWorker =
             global->GetOrCreateServiceWorker(aInstalling.ref()))) {
      mInstallingWorker->SetState(aInstalling.ref().State());
    }
  } else {
    mInstallingWorker = nullptr;
  }
}

}  // namespace dom
}  // namespace mozilla
