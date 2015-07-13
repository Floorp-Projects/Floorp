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
#include "mozilla/dom/MessagePortParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/PBackgroundTestParent.h"
#include "mozilla/layout/VsyncParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "nsIAppsService.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"
#include "nsRefPtr.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "nsXULAppAPI.h"
#include "ServiceWorkerManagerParent.h"

#ifdef DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false)
#endif

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::dom::cache::PCacheParent;
using mozilla::dom::cache::PCacheStorageParent;
using mozilla::dom::cache::PCacheStreamControlParent;
using mozilla::dom::MessagePortParent;
using mozilla::dom::PMessagePortParent;
using mozilla::dom::UDPSocketParent;

namespace {

void
AssertIsInMainProcess()
{
  MOZ_ASSERT(XRE_IsParentProcess());
}

void
AssertIsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
}

class TestParent final : public mozilla::ipc::PBackgroundTestParent
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
  ActorDestroy(ActorDestroyReason aWhy) override;
};

} // namespace

namespace mozilla {
namespace ipc {

using mozilla::dom::ContentParent;
using mozilla::dom::BroadcastChannelParent;
using mozilla::dom::ServiceWorkerRegistrationData;
using mozilla::dom::workers::ServiceWorkerManagerParent;

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

namespace {

class InitUDPSocketParentCallback final : public nsRunnable
{
public:
  InitUDPSocketParentCallback(UDPSocketParent* aActor,
                              const nsACString& aFilter)
    : mActor(aActor)
    , mFilter(aFilter)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsInMainProcess();

    IPC::Principal principal;
    if (!mActor->Init(principal, mFilter)) {
      MOZ_CRASH("UDPSocketCallback - failed init");
    }
    return NS_OK;
  }

private:
  ~InitUDPSocketParentCallback() {};

  nsRefPtr<UDPSocketParent> mActor;
  nsCString mFilter;
};

} // namespace

auto
BackgroundParentImpl::AllocPUDPSocketParent(const OptionalPrincipalInfo& /* unused */,
                                            const nsCString& /* unused */)
  -> PUDPSocketParent*
{
  nsRefPtr<UDPSocketParent> p = new UDPSocketParent(this);

  return p.forget().take();
}

bool
BackgroundParentImpl::RecvPUDPSocketConstructor(PUDPSocketParent* aActor,
                                                const OptionalPrincipalInfo& aOptionalPrincipal,
                                                const nsCString& aFilter)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (aOptionalPrincipal.type() == OptionalPrincipalInfo::TPrincipalInfo) {
    // Support for checking principals (for non-mtransport use) will be handled in
    // bug 1167039
    return false;
  }
  // No principal - This must be from mtransport (WebRTC/ICE) - We'd want
  // to DispatchToMainThread() here, but if we do we must block RecvBind()
  // until Init() gets run.  Since we don't have a principal, and we verify
  // we have a filter, we can safely skip the Dispatch and just invoke Init()
  // to install the filter.

  // For mtransport, this will always be "stun", which doesn't allow outbound packets if
  // they aren't STUN packets until a STUN response is seen.
  if (!aFilter.EqualsASCII("stun")) {
    return false;
  }

  IPC::Principal principal;
  if (!static_cast<UDPSocketParent*>(aActor)->Init(principal, aFilter)) {
    MOZ_CRASH("UDPSocketCallback - failed init");
  }

  return true;
}

bool
BackgroundParentImpl::DeallocPUDPSocketParent(PUDPSocketParent* actor)
{
  UDPSocketParent* p = static_cast<UDPSocketParent*>(actor);
  p->Release();
  return true;
}

mozilla::dom::PBroadcastChannelParent*
BackgroundParentImpl::AllocPBroadcastChannelParent(
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsCString& aOrigin,
                                            const nsString& aChannel,
                                            const bool& aPrivateBrowsing)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new BroadcastChannelParent(aPrincipalInfo, aOrigin, aChannel,
                                    aPrivateBrowsing);
}

namespace {

class CheckPrincipalRunnable final : public nsRunnable
{
public:
  CheckPrincipalRunnable(already_AddRefed<ContentParent> aParent,
                         const PrincipalInfo& aPrincipalInfo,
                         const nsCString& aOrigin)
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

    struct MOZ_STACK_CLASS RunRAII
    {
      explicit RunRAII(nsRefPtr<ContentParent>& aContentParent)
        : mContentParent(aContentParent)
      {}

      ~RunRAII()
      {
        mContentParent = nullptr;
      }

      nsRefPtr<ContentParent>& mContentParent;
    };

    RunRAII raii(mContentParent);

    nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(mPrincipalInfo);
    AssertAppPrincipal(mContentParent, principal);

    bool isNullPrincipal;
    nsresult rv = principal->GetIsNullPrincipal(&isNullPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv)) || isNullPrincipal) {
      mContentParent->KillHard("BroadcastChannel killed: no null principal.");
      return NS_OK;
    }

    nsAutoCString origin;
    rv = principal->GetOrigin(origin);
    if (NS_FAILED(rv)) {
      mContentParent->KillHard("BroadcastChannel killed: principal::GetOrigin failed.");
      return NS_OK;
    }

    if (NS_WARN_IF(!mOrigin.Equals(origin))) {
      mContentParent->KillHard("BroadcastChannel killed: origins do not match.");
      return NS_OK;
    }

    return NS_OK;
  }

private:
  nsRefPtr<ContentParent> mContentParent;
  PrincipalInfo mPrincipalInfo;
  nsCString mOrigin;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

} // namespace

bool
BackgroundParentImpl::RecvPBroadcastChannelConstructor(
                                            PBroadcastChannelParent* actor,
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsCString& aOrigin,
                                            const nsString& aChannel,
                                            const bool& aPrivateBrowsing)
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

mozilla::dom::PServiceWorkerManagerParent*
BackgroundParentImpl::AllocPServiceWorkerManagerParent()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new ServiceWorkerManagerParent();
}

bool
BackgroundParentImpl::DeallocPServiceWorkerManagerParent(
                                            PServiceWorkerManagerParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<ServiceWorkerManagerParent*>(aActor);
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

PCacheStorageParent*
BackgroundParentImpl::AllocPCacheStorageParent(const Namespace& aNamespace,
                                               const PrincipalInfo& aPrincipalInfo)
{
  return dom::cache::AllocPCacheStorageParent(this, aNamespace, aPrincipalInfo);
}

bool
BackgroundParentImpl::DeallocPCacheStorageParent(PCacheStorageParent* aActor)
{
  dom::cache::DeallocPCacheStorageParent(aActor);
  return true;
}

PCacheParent*
BackgroundParentImpl::AllocPCacheParent()
{
  MOZ_CRASH("CacheParent actor must be provided to PBackground manager");
  return nullptr;
}

bool
BackgroundParentImpl::DeallocPCacheParent(PCacheParent* aActor)
{
  dom::cache::DeallocPCacheParent(aActor);
  return true;
}

PCacheStreamControlParent*
BackgroundParentImpl::AllocPCacheStreamControlParent()
{
  MOZ_CRASH("CacheStreamControlParent actor must be provided to PBackground manager");
  return nullptr;
}

bool
BackgroundParentImpl::DeallocPCacheStreamControlParent(PCacheStreamControlParent* aActor)
{
  dom::cache::DeallocPCacheStreamControlParent(aActor);
  return true;
}

PMessagePortParent*
BackgroundParentImpl::AllocPMessagePortParent(const nsID& aUUID,
                                              const nsID& aDestinationUUID,
                                              const uint32_t& aSequenceID)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new MessagePortParent(aUUID);
}

bool
BackgroundParentImpl::RecvPMessagePortConstructor(PMessagePortParent* aActor,
                                                  const nsID& aUUID,
                                                  const nsID& aDestinationUUID,
                                                  const uint32_t& aSequenceID)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  MessagePortParent* mp = static_cast<MessagePortParent*>(aActor);
  return mp->Entangle(aDestinationUUID, aSequenceID);
}

bool
BackgroundParentImpl::DeallocPMessagePortParent(PMessagePortParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<MessagePortParent*>(aActor);
  return true;
}

bool
BackgroundParentImpl::RecvMessagePortForceClose(const nsID& aUUID,
                                                const nsID& aDestinationUUID,
                                                const uint32_t& aSequenceID)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return MessagePortParent::ForceClose(aUUID, aDestinationUUID, aSequenceID);
}

} // namespace ipc
} // namespace mozilla

void
TestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
}
