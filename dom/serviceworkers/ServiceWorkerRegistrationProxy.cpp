/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationProxy.h"

#include "mozilla/ipc/BackgroundParent.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerRegistrationParent.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;

ServiceWorkerRegistrationProxy::~ServiceWorkerRegistrationProxy()
{
  // Any thread
  MOZ_DIAGNOSTIC_ASSERT(!mActor);
  MOZ_DIAGNOSTIC_ASSERT(!mReg);
}

void
ServiceWorkerRegistrationProxy::MaybeShutdownOnBGThread()
{
  AssertIsOnBackgroundThread();
  if (!mActor) {
    return;
  }
  mActor->MaybeSendDelete();
}

void
ServiceWorkerRegistrationProxy::UpdateStateOnBGThread(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  AssertIsOnBackgroundThread();
  if (!mActor) {
    return;
  }
  Unused << mActor->SendUpdateState(aDescriptor.ToIPC());
}

void
ServiceWorkerRegistrationProxy::InitOnMainThread()
{
  AssertIsOnMainThread();

  auto scopeExit = MakeScopeExit([&] {
    MaybeShutdownOnMainThread();
  });

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  NS_ENSURE_TRUE_VOID(swm);

  RefPtr<ServiceWorkerRegistrationInfo> reg =
    swm->GetRegistration(mDescriptor.PrincipalInfo(), mDescriptor.Scope());
  NS_ENSURE_TRUE_VOID(reg);

  scopeExit.release();

  mReg = new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(
    "ServiceWorkerRegistrationProxy::mInfo", reg);

  mReg->AddInstance(this, mDescriptor);
}

void
ServiceWorkerRegistrationProxy::MaybeShutdownOnMainThread()
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod(__func__, this,
                      &ServiceWorkerRegistrationProxy::MaybeShutdownOnBGThread);

  MOZ_ALWAYS_SUCCEEDS(mEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
}

void
ServiceWorkerRegistrationProxy::StopListeningOnMainThread()
{
  AssertIsOnMainThread();

  if (!mReg) {
    return;
  }

  mReg->RemoveInstance(this);
  mReg = nullptr;
}

void
ServiceWorkerRegistrationProxy::UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  AssertIsOnMainThread();

  if (mDescriptor == aDescriptor) {
    return;
  }
  mDescriptor = aDescriptor;

  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<ServiceWorkerRegistrationDescriptor>(
    __func__, this, &ServiceWorkerRegistrationProxy::UpdateStateOnBGThread,
    aDescriptor);

  MOZ_ALWAYS_SUCCEEDS(mEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
}

void
ServiceWorkerRegistrationProxy::RegistrationRemoved()
{
  MaybeShutdownOnMainThread();
}

void
ServiceWorkerRegistrationProxy::GetScope(nsAString& aScope) const
{
  CopyUTF8toUTF16(mDescriptor.Scope(), aScope);
}

bool
ServiceWorkerRegistrationProxy::MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  AssertIsOnMainThread();
  return aDescriptor.Id() == mDescriptor.Id() &&
         aDescriptor.PrincipalInfo() == mDescriptor.PrincipalInfo() &&
         aDescriptor.Scope() == mDescriptor.Scope();
}

ServiceWorkerRegistrationProxy::ServiceWorkerRegistrationProxy(const ServiceWorkerRegistrationDescriptor& aDescriptor)
  : mActor(nullptr)
  , mEventTarget(GetCurrentThreadSerialEventTarget())
  , mDescriptor(aDescriptor)
{
}

void
ServiceWorkerRegistrationProxy::Init(ServiceWorkerRegistrationParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(aActor);
  MOZ_DIAGNOSTIC_ASSERT(!mActor);
  MOZ_DIAGNOSTIC_ASSERT(mEventTarget);

  mActor = aActor;

  // Note, this must be done from a separate Init() method and not in
  // the constructor.  If done from the constructor the runnable can
  // execute, complete, and release its reference before the constructor
  // returns.
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod(
    "ServiceWorkerRegistrationProxy::Init", this,
    &ServiceWorkerRegistrationProxy::InitOnMainThread);
  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
}

void
ServiceWorkerRegistrationProxy::RevokeActor(ServiceWorkerRegistrationParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mActor);
  MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
  mActor = nullptr;

  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod(__func__, this,
                      &ServiceWorkerRegistrationProxy::StopListeningOnMainThread);
  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
}

namespace {

class UnregisterCallback final : public nsIServiceWorkerUnregisterCallback
{
  RefPtr<GenericPromise::Private> mPromise;

  ~UnregisterCallback() = default;

public:
  explicit UnregisterCallback(GenericPromise::Private* aPromise)
    : mPromise(aPromise)
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  NS_IMETHOD
  UnregisterSucceeded(bool aState) override
  {
    mPromise->Resolve(aState, __func__);
    return NS_OK;
  }

  NS_IMETHOD
  UnregisterFailed() override
  {
    mPromise->Reject(NS_ERROR_DOM_SECURITY_ERR, __func__);
    return NS_OK;
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(UnregisterCallback, nsIServiceWorkerUnregisterCallback)

} // anonymous namespace

RefPtr<GenericPromise>
ServiceWorkerRegistrationProxy::Unregister()
{
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerRegistrationProxy> self = this;
  RefPtr<GenericPromise::Private> promise =
    new GenericPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__,
    [self, promise] () mutable {
      nsresult rv = NS_ERROR_DOM_INVALID_STATE_ERR;
      auto scopeExit = MakeScopeExit([&] {
        promise->Reject(rv, __func__);
      });

      NS_ENSURE_TRUE_VOID(self->mReg);

      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      NS_ENSURE_TRUE_VOID(swm);

      RefPtr<UnregisterCallback> cb = new UnregisterCallback(promise);

      rv = swm->Unregister(self->mReg->Principal(), cb,
                           NS_ConvertUTF8toUTF16(self->mReg->Scope()));
      NS_ENSURE_SUCCESS_VOID(rv);


      scopeExit.release();
    });

  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));

  return promise;
}

namespace {

class UpdateCallback final : public ServiceWorkerUpdateFinishCallback
{
  RefPtr<ServiceWorkerRegistrationPromise::Private> mPromise;

  ~UpdateCallback() = default;

public:
  explicit UpdateCallback(RefPtr<ServiceWorkerRegistrationPromise::Private>&& aPromise)
    : mPromise(std::move(aPromise))
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  void
  UpdateSucceeded(ServiceWorkerRegistrationInfo* aInfo) override
  {
    mPromise->Resolve(aInfo->Descriptor(), __func__);
  }

  void
  UpdateFailed(ErrorResult& aResult) override
  {
    mPromise->Reject(CopyableErrorResult(aResult), __func__);
  }
};

} // anonymous namespace

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerRegistrationProxy::Update()
{
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerRegistrationProxy> self = this;
  RefPtr<ServiceWorkerRegistrationPromise::Private> promise =
    new ServiceWorkerRegistrationPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__,
    [self, promise] () mutable {
      auto scopeExit = MakeScopeExit([&] {
        promise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      });

      NS_ENSURE_TRUE_VOID(self->mReg);

      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      NS_ENSURE_TRUE_VOID(swm);

      RefPtr<UpdateCallback> cb = new UpdateCallback(std::move(promise));
      swm->Update(self->mReg->Principal(), self->mReg->Scope(), cb);

      scopeExit.release();
    });

  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));

  return promise;
}

} // namespace dom
} // namespace mozilla
