/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistration.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "ServiceWorkerRegistrationImpl.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerRegistration)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

ServiceWorkerRegistration::ServiceWorkerRegistration(nsPIDOMWindowInner* aWindow,
                                                     const ServiceWorkerRegistrationDescriptor& aDescriptor)
  : DOMEventTargetHelper(aWindow)
  , mDescriptor(aDescriptor)
{
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

  RefPtr<ServiceWorkerRegistration> registration =
    new ServiceWorkerRegistrationMainThread(aWindow, aDescriptor);

  return registration.forget();
}

/* static */ already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerRegistration::CreateForWorker(WorkerPrivate* aWorkerPrivate,
                                           const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  NS_ConvertUTF8toUTF16 scope(aDescriptor.Scope());

  RefPtr<ServiceWorkerRegistration> registration =
    new ServiceWorkerRegistrationWorkerThread(aWorkerPrivate, aDescriptor);

  return registration.forget();
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
  if (!global) {
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
  return aDescriptor.PrincipalInfo() == mDescriptor.PrincipalInfo() &&
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

} // dom namespace
} // mozilla namespace
