/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistration.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PushManager.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "ServiceWorkerRegistrationImpl.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistration,
                                   DOMEventTargetHelper,
                                   mInstallingWorker,
                                   mWaitingWorker,
                                   mActiveWorker,
                                   mPushManager);

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerRegistration)
  NS_INTERFACE_MAP_ENTRY(ServiceWorkerRegistration)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

ServiceWorkerRegistration::ServiceWorkerRegistration(nsIGlobalObject* aGlobal,
                                                     const ServiceWorkerRegistrationDescriptor& aDescriptor,
                                                     ServiceWorkerRegistration::Inner* aInner)
  : DOMEventTargetHelper(aGlobal)
  , mDescriptor(aDescriptor)
  , mInner(aInner)
{
  MOZ_DIAGNOSTIC_ASSERT(mInner);
  UpdateState(mDescriptor);
  mInner->SetServiceWorkerRegistration(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration()
{
  if (mInner) {
    mInner->ClearServiceWorkerRegistration(this);
  }
}

JSObject*
ServiceWorkerRegistration::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto)
{
  return ServiceWorkerRegistrationBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerRegistration::CreateForMainThread(nsPIDOMWindowInner* aWindow,
                                               const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Inner> inner = new ServiceWorkerRegistrationMainThread(aDescriptor);

  RefPtr<ServiceWorkerRegistration> registration =
    new ServiceWorkerRegistration(aWindow->AsGlobal(), aDescriptor, inner);

  return registration.forget();
}

/* static */ already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerRegistration::CreateForWorker(WorkerPrivate* aWorkerPrivate,
                                           nsIGlobalObject* aGlobal,
                                           const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<Inner> inner =
    new ServiceWorkerRegistrationWorkerThread(aDescriptor);

  RefPtr<ServiceWorkerRegistration> registration =
    new ServiceWorkerRegistration(aGlobal, aDescriptor, inner);

  return registration.forget();
}

void
ServiceWorkerRegistration::DisconnectFromOwner()
{
  mInner->ClearServiceWorkerRegistration(this);
  mInner = nullptr;
  DOMEventTargetHelper::DisconnectFromOwner();
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistration::GetInstalling() const
{
  RefPtr<ServiceWorker> ref = mInstallingWorker;
  return ref.forget();
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistration::GetWaiting() const
{
  RefPtr<ServiceWorker> ref = mWaitingWorker;
  return ref.forget();
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistration::GetActive() const
{
  RefPtr<ServiceWorker> ref = mActiveWorker;
  return ref.forget();
}

void
ServiceWorkerRegistration::UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  MOZ_DIAGNOSTIC_ASSERT(MatchesDescriptor(aDescriptor));

  mDescriptor = aDescriptor;

  nsCOMPtr<nsIGlobalObject> global = GetParentObject();

  // Clear all workers if the registration has been detached from the global.
  // Also, we cannot expose ServiceWorker objects on worker threads yet, so
  // do the same on when off-main-thread.  This main thread check should be
  // removed as part of bug 1113522.
  if (!global || !NS_IsMainThread()) {
    mInstallingWorker = nullptr;
    mWaitingWorker = nullptr;
    mActiveWorker = nullptr;
    return;
  }

  Maybe<ServiceWorkerDescriptor> active = aDescriptor.GetActive();
  if (active.isSome()) {
    mActiveWorker = global->GetOrCreateServiceWorker(active.ref());
  } else {
    mActiveWorker = nullptr;
  }

  Maybe<ServiceWorkerDescriptor> waiting = aDescriptor.GetWaiting();
  if (waiting.isSome()) {
    mWaitingWorker = global->GetOrCreateServiceWorker(waiting.ref());
  } else {
    mWaitingWorker = nullptr;
  }

  Maybe<ServiceWorkerDescriptor> installing = aDescriptor.GetInstalling();
  if (installing.isSome()) {
    mInstallingWorker = global->GetOrCreateServiceWorker(installing.ref());
  } else {
    mInstallingWorker = nullptr;
  }
}

bool
ServiceWorkerRegistration::MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor) const
{
  return aDescriptor.Id() == mDescriptor.Id() &&
         aDescriptor.PrincipalInfo() == mDescriptor.PrincipalInfo() &&
         aDescriptor.Scope() == mDescriptor.Scope();
}

void
ServiceWorkerRegistration::GetScope(nsAString& aScope) const
{
  CopyUTF8toUTF16(mDescriptor.Scope(), aScope);
}

ServiceWorkerUpdateViaCache
ServiceWorkerRegistration::GetUpdateViaCache(ErrorResult& aRv) const
{
  return mDescriptor.UpdateViaCache();
}

already_AddRefed<Promise>
ServiceWorkerRegistration::Update(ErrorResult& aRv)
{
  if (!mInner) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  return mInner->Update(aRv);
}

already_AddRefed<Promise>
ServiceWorkerRegistration::Unregister(ErrorResult& aRv)
{
  if (!mInner) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  return mInner->Unregister(aRv);
}

already_AddRefed<PushManager>
ServiceWorkerRegistration::GetPushManager(JSContext* aCx, ErrorResult& aRv)
{
  if (!mInner) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  if (!mPushManager) {
    mPushManager = mInner->GetPushManager(aCx, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  RefPtr<PushManager> ret = mPushManager;
  return ret.forget();
}

already_AddRefed<Promise>
ServiceWorkerRegistration::ShowNotification(JSContext* aCx,
                                            const nsAString& aTitle,
                                            const NotificationOptions& aOptions,
                                            ErrorResult& aRv)
{
  if (!mInner) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  return mInner->ShowNotification(aCx, aTitle, aOptions, aRv);
}

already_AddRefed<Promise>
ServiceWorkerRegistration::GetNotifications(const GetNotificationOptions& aOptions,
                                            ErrorResult& aRv)
{
  if (!mInner) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  return mInner->GetNotifications(aOptions, aRv);
}

} // dom namespace
} // mozilla namespace
