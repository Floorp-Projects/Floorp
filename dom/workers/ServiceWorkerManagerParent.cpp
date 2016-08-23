/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerParent.h"
#include "ServiceWorkerManagerService.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

namespace {

uint64_t sServiceWorkerManagerParentID = 0;

class RegisterServiceWorkerCallback final : public Runnable
{
public:
  RegisterServiceWorkerCallback(const ServiceWorkerRegistrationData& aData,
                                uint64_t aParentID)
    : mData(aData)
    , mParentID(aParentID)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    RefPtr<dom::ServiceWorkerRegistrar> service =
      dom::ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->RegisterServiceWorker(mData);

    RefPtr<ServiceWorkerManagerService> managerService =
      ServiceWorkerManagerService::Get();
    if (managerService) {
      managerService->PropagateRegistration(mParentID, mData);
    }

    return NS_OK;
  }

private:
  ServiceWorkerRegistrationData mData;
  const uint64_t mParentID;
};

class UnregisterServiceWorkerCallback final : public Runnable
{
public:
  UnregisterServiceWorkerCallback(const PrincipalInfo& aPrincipalInfo,
                                  const nsString& aScope,
                                  uint64_t aParentID)
    : mPrincipalInfo(aPrincipalInfo)
    , mScope(aScope)
    , mParentID(aParentID)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    RefPtr<dom::ServiceWorkerRegistrar> service =
      dom::ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->UnregisterServiceWorker(mPrincipalInfo,
                                     NS_ConvertUTF16toUTF8(mScope));

    RefPtr<ServiceWorkerManagerService> managerService =
      ServiceWorkerManagerService::Get();
    if (managerService) {
      managerService->PropagateUnregister(mParentID, mPrincipalInfo,
                                          mScope);
    }

    return NS_OK;
  }

private:
  const PrincipalInfo mPrincipalInfo;
  nsString mScope;
  uint64_t mParentID;
};

class CheckPrincipalWithCallbackRunnable final : public Runnable
{
public:
  CheckPrincipalWithCallbackRunnable(already_AddRefed<ContentParent> aParent,
                                     const PrincipalInfo& aPrincipalInfo,
                                     Runnable* aCallback)
    : mContentParent(aParent)
    , mPrincipalInfo(aPrincipalInfo)
    , mCallback(aCallback)
    , mBackgroundThread(NS_GetCurrentThread())
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mContentParent);
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mBackgroundThread);
  }

  NS_IMETHOD Run() override
  {
    if (NS_IsMainThread()) {
      nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(mPrincipalInfo);
      AssertAppPrincipal(mContentParent, principal);
      mContentParent = nullptr;

      mBackgroundThread->Dispatch(this, NS_DISPATCH_NORMAL);
      return NS_OK;
    }

    AssertIsOnBackgroundThread();
    mCallback->Run();
    mCallback = nullptr;

    return NS_OK;
  }

private:
  RefPtr<ContentParent> mContentParent;
  PrincipalInfo mPrincipalInfo;
  RefPtr<Runnable> mCallback;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

} // namespace

ServiceWorkerManagerParent::ServiceWorkerManagerParent()
  : mService(ServiceWorkerManagerService::GetOrCreate())
  , mID(++sServiceWorkerManagerParentID)
  , mActorDestroyed(false)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this);
}

ServiceWorkerManagerParent::~ServiceWorkerManagerParent()
{
  AssertIsOnBackgroundThread();
}

already_AddRefed<ContentParent>
ServiceWorkerManagerParent::GetContentParent() const
{
  AssertIsOnBackgroundThread();

  // This object must be released on main-thread.
  RefPtr<ContentParent> parent =
    BackgroundParent::GetContentParent(Manager());
  return parent.forget();
}

bool
ServiceWorkerManagerParent::RecvRegister(
                                     const ServiceWorkerRegistrationData& aData)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // Basic validation.
  if (aData.scope().IsEmpty() ||
      aData.principal().type() == PrincipalInfo::TNullPrincipalInfo ||
      aData.principal().type() == PrincipalInfo::TSystemPrincipalInfo) {
    return false;
  }

  RefPtr<RegisterServiceWorkerCallback> callback =
    new RegisterServiceWorkerCallback(aData, mID);

  RefPtr<ContentParent> parent =
    BackgroundParent::GetContentParent(Manager());

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    callback->Run();
    return true;
  }

  RefPtr<CheckPrincipalWithCallbackRunnable> runnable =
    new CheckPrincipalWithCallbackRunnable(parent.forget(), aData.principal(),
                                           callback);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return true;
}

bool
ServiceWorkerManagerParent::RecvUnregister(const PrincipalInfo& aPrincipalInfo,
                                           const nsString& aScope)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // Basic validation.
  if (aScope.IsEmpty() ||
      aPrincipalInfo.type() == PrincipalInfo::TNullPrincipalInfo ||
      aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    return false;
  }

  RefPtr<UnregisterServiceWorkerCallback> callback =
    new UnregisterServiceWorkerCallback(aPrincipalInfo, aScope, mID);

  RefPtr<ContentParent> parent =
    BackgroundParent::GetContentParent(Manager());

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    callback->Run();
    return true;
  }

  RefPtr<CheckPrincipalWithCallbackRunnable> runnable =
    new CheckPrincipalWithCallbackRunnable(parent.forget(), aPrincipalInfo,
                                           callback);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateSoftUpdate(const PrincipalOriginAttributes& aOriginAttributes,
                                                    const nsString& aScope)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateSoftUpdate(mID, aOriginAttributes, aScope);
  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateUnregister(const PrincipalInfo& aPrincipalInfo,
                                                    const nsString& aScope)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateUnregister(mID, aPrincipalInfo, aScope);
  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateRemove(const nsCString& aHost)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateRemove(mID, aHost);
  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateRemoveAll()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateRemoveAll(mID);
  return true;
}

bool
ServiceWorkerManagerParent::RecvShutdown()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->UnregisterActor(this);
  mService = nullptr;

  Unused << Send__delete__(this);
  return true;
}

void
ServiceWorkerManagerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  mActorDestroyed = true;

  if (mService) {
    // This object is about to be released and with it, also mService will be
    // released too.
    mService->UnregisterActor(this);
  }
}

} // namespace workers
} // namespace dom
} // namespace mozilla
