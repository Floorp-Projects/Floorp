/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundParentImpl.h"

#include "BroadcastChannelParent.h"
#include "FileDescriptorSetParent.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/PBlobParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/PBackgroundTestParent.h"
#include "mozilla/layout/VsyncParent.h"
#include "nsNetUtil.h"
#include "nsRefPtr.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "nsXULAppAPI.h"

#ifdef DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false)
#endif

using mozilla::ipc::AssertIsOnBackgroundThread;

namespace {

void
AssertIsInMainProcess()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
}

void
AssertIsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
}

class TestParent MOZ_FINAL : public mozilla::ipc::PBackgroundTestParent
{
  friend class mozilla::ipc::BackgroundParentImpl;

  TestParent()
  {
    MOZ_COUNT_CTOR(TestParent);
  }

protected:
  ~TestParent()
  {
    MOZ_COUNT_DTOR(TestParent);
  }

public:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
};

} // anonymous namespace

namespace mozilla {
namespace ipc {

using mozilla::dom::ContentParent;
using mozilla::dom::BroadcastChannelParent;
using mozilla::dom::ServiceWorkerRegistrationData;

BackgroundParentImpl::BackgroundParentImpl()
{
  AssertIsInMainProcess();
  AssertIsOnMainThread();

  MOZ_COUNT_CTOR(mozilla::ipc::BackgroundParentImpl);
}

BackgroundParentImpl::~BackgroundParentImpl()
{
  AssertIsInMainProcess();
  AssertIsOnMainThread();

  MOZ_COUNT_DTOR(mozilla::ipc::BackgroundParentImpl);
}

void
BackgroundParentImpl::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
}

BackgroundParentImpl::PBackgroundTestParent*
BackgroundParentImpl::AllocPBackgroundTestParent(const nsCString& aTestArg)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new TestParent();
}

bool
BackgroundParentImpl::RecvPBackgroundTestConstructor(
                                                  PBackgroundTestParent* aActor,
                                                  const nsCString& aTestArg)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return PBackgroundTestParent::Send__delete__(aActor, aTestArg);
}

bool
BackgroundParentImpl::DeallocPBackgroundTestParent(
                                                  PBackgroundTestParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<TestParent*>(aActor);
  return true;
}

auto
BackgroundParentImpl::AllocPBackgroundIDBFactoryParent(
                                                const LoggingInfo& aLoggingInfo)
  -> PBackgroundIDBFactoryParent*
{
  using mozilla::dom::indexedDB::AllocPBackgroundIDBFactoryParent;

  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return AllocPBackgroundIDBFactoryParent(aLoggingInfo);
}

bool
BackgroundParentImpl::RecvPBackgroundIDBFactoryConstructor(
                                            PBackgroundIDBFactoryParent* aActor,
                                            const LoggingInfo& aLoggingInfo)
{
  using mozilla::dom::indexedDB::RecvPBackgroundIDBFactoryConstructor;

  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return RecvPBackgroundIDBFactoryConstructor(aActor, aLoggingInfo);
}

bool
BackgroundParentImpl::DeallocPBackgroundIDBFactoryParent(
                                            PBackgroundIDBFactoryParent* aActor)
{
  using mozilla::dom::indexedDB::DeallocPBackgroundIDBFactoryParent;

  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocPBackgroundIDBFactoryParent(aActor);
}

auto
BackgroundParentImpl::AllocPBlobParent(const BlobConstructorParams& aParams)
  -> PBlobParent*
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(aParams.type() !=
                   BlobConstructorParams::TParentBlobConstructorParams)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  return mozilla::dom::BlobParent::Create(this, aParams);
}

bool
BackgroundParentImpl::DeallocPBlobParent(PBlobParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  mozilla::dom::BlobParent::Destroy(aActor);
  return true;
}

PFileDescriptorSetParent*
BackgroundParentImpl::AllocPFileDescriptorSetParent(
                                          const FileDescriptor& aFileDescriptor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new FileDescriptorSetParent(aFileDescriptor);
}

bool
BackgroundParentImpl::DeallocPFileDescriptorSetParent(
                                               PFileDescriptorSetParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<FileDescriptorSetParent*>(aActor);
  return true;
}

BackgroundParentImpl::PVsyncParent*
BackgroundParentImpl::AllocPVsyncParent()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  nsRefPtr<mozilla::layout::VsyncParent> actor =
      mozilla::layout::VsyncParent::Create();
  // There still has one ref-count after return, and it will be released in
  // DeallocPVsyncParent().
  return actor.forget().take();
}

bool
BackgroundParentImpl::DeallocPVsyncParent(PVsyncParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // This actor already has one ref-count. Please check AllocPVsyncParent().
  nsRefPtr<mozilla::layout::VsyncParent> actor =
      dont_AddRef(static_cast<mozilla::layout::VsyncParent*>(aActor));
  return true;
}

mozilla::dom::PBroadcastChannelParent*
BackgroundParentImpl::AllocPBroadcastChannelParent(
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsString& aOrigin,
                                            const nsString& aChannel)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new BroadcastChannelParent(aOrigin, aChannel);
}

namespace {

class CheckPrincipalRunnable MOZ_FINAL : public nsRunnable
{
public:
  CheckPrincipalRunnable(already_AddRefed<ContentParent> aParent,
                         const PrincipalInfo& aPrincipalInfo,
                         const nsString& aOrigin)
    : mContentParent(aParent)
    , mPrincipalInfo(aPrincipalInfo)
    , mOrigin(aOrigin)
    , mBackgroundThread(NS_GetCurrentThread())
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mContentParent);
    MOZ_ASSERT(mBackgroundThread);
  }

  NS_IMETHODIMP Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(mPrincipalInfo);
    AssertAppPrincipal(mContentParent, principal);

    bool isNullPrincipal;
    nsresult rv = principal->GetIsNullPrincipal(&isNullPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv)) || isNullPrincipal) {
      mContentParent->KillHard("PBackground CheckPrincipal 1");
      return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), mOrigin);
    if (NS_FAILED(rv) || !uri) {
      mContentParent->KillHard("PBackground CheckPrincipal 2");
      return NS_OK;
    }

    rv = principal->CheckMayLoad(uri, false, false);
    if (NS_FAILED(rv)) {
      mContentParent->KillHard("PBackground CheckPrincipal 3");
      return NS_OK;
    }

    mContentParent = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<ContentParent> mContentParent;
  PrincipalInfo mPrincipalInfo;
  nsString mOrigin;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

} // anonymous namespace

bool
BackgroundParentImpl::RecvPBroadcastChannelConstructor(
                                            PBroadcastChannelParent* actor,
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsString& aOrigin,
                                            const nsString& aChannel)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  nsRefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    MOZ_ASSERT(aPrincipalInfo.type() != PrincipalInfo::TNullPrincipalInfo);
    return true;
  }

  nsRefPtr<CheckPrincipalRunnable> runnable =
    new CheckPrincipalRunnable(parent.forget(), aPrincipalInfo, aOrigin);
  nsresult rv = NS_DispatchToMainThread(runnable);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  return true;
}

bool
BackgroundParentImpl::DeallocPBroadcastChannelParent(
                                                PBroadcastChannelParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<BroadcastChannelParent*>(aActor);
  return true;
}

namespace {

class RegisterServiceWorkerCallback MOZ_FINAL : public nsRunnable
{
public:
  explicit RegisterServiceWorkerCallback(
                                     const ServiceWorkerRegistrationData& aData)
    : mData(aData)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    nsRefPtr<dom::ServiceWorkerRegistrar> service =
      dom::ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->RegisterServiceWorker(mData);
    return NS_OK;
  }

private:
  ServiceWorkerRegistrationData mData;
};

class UnregisterServiceWorkerCallback MOZ_FINAL : public nsRunnable
{
public:
  explicit UnregisterServiceWorkerCallback(const nsString& aScope)
    : mScope(aScope)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    nsRefPtr<dom::ServiceWorkerRegistrar> service =
      dom::ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->UnregisterServiceWorker(NS_ConvertUTF16toUTF8(mScope));
    return NS_OK;
  }

private:
  nsString mScope;
};

class CheckPrincipalWithCallbackRunnable MOZ_FINAL : public nsRunnable
{
public:
  CheckPrincipalWithCallbackRunnable(already_AddRefed<ContentParent> aParent,
                                     const PrincipalInfo& aPrincipalInfo,
                                     nsRunnable* aCallback)
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

  NS_IMETHODIMP Run()
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
  nsRefPtr<ContentParent> mContentParent;
  PrincipalInfo mPrincipalInfo;
  nsRefPtr<nsRunnable> mCallback;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

} // anonymous namespace

bool
BackgroundParentImpl::RecvRegisterServiceWorker(
                                     const ServiceWorkerRegistrationData& aData)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // Basic validation.
  if (aData.scope().IsEmpty() ||
      aData.scriptSpec().IsEmpty() ||
      aData.principal().type() == PrincipalInfo::TNullPrincipalInfo) {
    return false;
  }

  nsRefPtr<RegisterServiceWorkerCallback> callback =
    new RegisterServiceWorkerCallback(aData);

  nsRefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    callback->Run();
    return true;
  }

  nsRefPtr<CheckPrincipalWithCallbackRunnable> runnable =
    new CheckPrincipalWithCallbackRunnable(parent.forget(), aData.principal(),
                                           callback);
  nsresult rv = NS_DispatchToMainThread(runnable);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  return true;
}

bool
BackgroundParentImpl::RecvUnregisterServiceWorker(
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsString& aScope)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // Basic validation.
  if (aScope.IsEmpty() ||
      aPrincipalInfo.type() == PrincipalInfo::TNullPrincipalInfo) {
    return false;
  }

  nsRefPtr<UnregisterServiceWorkerCallback> callback =
    new UnregisterServiceWorkerCallback(aScope);

  nsRefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    callback->Run();
    return true;
  }

  nsRefPtr<CheckPrincipalWithCallbackRunnable> runnable =
    new CheckPrincipalWithCallbackRunnable(parent.forget(), aPrincipalInfo,
                                           callback);
  nsresult rv = NS_DispatchToMainThread(runnable);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  return true;
}

bool
BackgroundParentImpl::RecvShutdownServiceWorkerRegistrar()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return false;
  }

  nsRefPtr<dom::ServiceWorkerRegistrar> service =
    dom::ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(service);

  service->Shutdown();
  return true;
}

} // namespace ipc
} // namespace mozilla

void
TestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
}
