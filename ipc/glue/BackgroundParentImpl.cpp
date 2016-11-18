/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundParentImpl.h"

#include "BroadcastChannelParent.h"
#include "FileDescriptorSetParent.h"
#ifdef MOZ_WEBRTC
#include "CamerasParent.h"
#endif
#include "mozilla/media/MediaParent.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadEventChannelParent.h"
#include "mozilla/dom/GamepadTestChannelParent.h"
#endif
#include "mozilla/dom/PBlobParent.h"
#include "mozilla/dom/PGamepadEventChannelParent.h"
#include "mozilla/dom/PGamepadTestChannelParent.h"
#include "mozilla/dom/MessagePortParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/dom/quota/ActorsParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/PBackgroundTestParent.h"
#include "mozilla/ipc/PSendStreamParent.h"
#include "mozilla/ipc/SendStreamAlloc.h"
#include "mozilla/layout/VsyncParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "mozilla/Preferences.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"
#include "nsProxyRelease.h"
#include "mozilla/RefPtr.h"
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
using mozilla::dom::asmjscache::PAsmJSCacheEntryParent;
using mozilla::dom::cache::PCacheParent;
using mozilla::dom::cache::PCacheStorageParent;
using mozilla::dom::cache::PCacheStreamControlParent;
using mozilla::dom::FileSystemBase;
using mozilla::dom::FileSystemRequestParent;
using mozilla::dom::MessagePortParent;
using mozilla::dom::PMessagePortParent;
using mozilla::dom::UDPSocketParent;

namespace {

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

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundTestConstructor(
                                                  PBackgroundTestParent* aActor,
                                                  const nsCString& aTestArg)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!PBackgroundTestParent::Send__delete__(aActor, aTestArg)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
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

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundIDBFactoryConstructor(
                                            PBackgroundIDBFactoryParent* aActor,
                                            const LoggingInfo& aLoggingInfo)
{
  using mozilla::dom::indexedDB::RecvPBackgroundIDBFactoryConstructor;

  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!RecvPBackgroundIDBFactoryConstructor(aActor, aLoggingInfo)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
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
BackgroundParentImpl::AllocPBackgroundIndexedDBUtilsParent()
  -> PBackgroundIndexedDBUtilsParent*
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::indexedDB::AllocPBackgroundIndexedDBUtilsParent();
}

bool
BackgroundParentImpl::DeallocPBackgroundIndexedDBUtilsParent(
                                        PBackgroundIndexedDBUtilsParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return
    mozilla::dom::indexedDB::DeallocPBackgroundIndexedDBUtilsParent(aActor);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvFlushPendingFileDeletions()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!mozilla::dom::indexedDB::RecvFlushPendingFileDeletions()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
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

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBlobConstructor(PBlobParent* aActor,
                                           const BlobConstructorParams& aParams)
{
  const ParentBlobConstructorParams& params = aParams;
  if (params.blobParams().type() == AnyBlobConstructorParams::TKnownBlobConstructorParams) {
    if (!aActor->SendCreatedFromKnownBlob()) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  return IPC_OK();
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

PSendStreamParent*
BackgroundParentImpl::AllocPSendStreamParent()
{
  return mozilla::ipc::AllocPSendStreamParent();
}

bool
BackgroundParentImpl::DeallocPSendStreamParent(PSendStreamParent* aActor)
{
  delete aActor;
  return true;
}

BackgroundParentImpl::PVsyncParent*
BackgroundParentImpl::AllocPVsyncParent()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<mozilla::layout::VsyncParent> actor =
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
  RefPtr<mozilla::layout::VsyncParent> actor =
      dont_AddRef(static_cast<mozilla::layout::VsyncParent*>(aActor));
  return true;
}

camera::PCamerasParent*
BackgroundParentImpl::AllocPCamerasParent()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

#ifdef MOZ_WEBRTC
  RefPtr<mozilla::camera::CamerasParent> actor =
      mozilla::camera::CamerasParent::Create();
  return actor.forget().take();
#else
  return nullptr;
#endif
}

bool
BackgroundParentImpl::DeallocPCamerasParent(camera::PCamerasParent *aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

#ifdef MOZ_WEBRTC
  RefPtr<mozilla::camera::CamerasParent> actor =
      dont_AddRef(static_cast<mozilla::camera::CamerasParent*>(aActor));
#endif
  return true;
}

namespace {

class InitUDPSocketParentCallback final : public Runnable
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

  NS_IMETHOD
  Run() override
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

  RefPtr<UDPSocketParent> mActor;
  nsCString mFilter;
};

} // namespace

auto
BackgroundParentImpl::AllocPUDPSocketParent(const OptionalPrincipalInfo& /* unused */,
                                            const nsCString& /* unused */)
  -> PUDPSocketParent*
{
  RefPtr<UDPSocketParent> p = new UDPSocketParent(this);

  return p.forget().take();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPUDPSocketConstructor(PUDPSocketParent* aActor,
                                                const OptionalPrincipalInfo& aOptionalPrincipal,
                                                const nsCString& aFilter)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (aOptionalPrincipal.type() == OptionalPrincipalInfo::TPrincipalInfo) {
    // Support for checking principals (for non-mtransport use) will be handled in
    // bug 1167039
    return IPC_FAIL_NO_REASON(this);
  }
  // No principal - This must be from mtransport (WebRTC/ICE) - We'd want
  // to DispatchToMainThread() here, but if we do we must block RecvBind()
  // until Init() gets run.  Since we don't have a principal, and we verify
  // we have a filter, we can safely skip the Dispatch and just invoke Init()
  // to install the filter.

  // For mtransport, this will always be "stun", which doesn't allow outbound
  // packets if they aren't STUN packets until a STUN response is seen.
  if (!aFilter.EqualsASCII(NS_NETWORK_SOCKET_FILTER_HANDLER_STUN_SUFFIX)) {
    return IPC_FAIL_NO_REASON(this);
  }

  IPC::Principal principal;
  if (!static_cast<UDPSocketParent*>(aActor)->Init(principal, aFilter)) {
    MOZ_CRASH("UDPSocketCallback - failed init");
  }

  return IPC_OK();
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
                                            const nsString& aChannel)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  nsString originChannelKey;

  // The format of originChannelKey is:
  //  <channelName>|<origin+OriginAttributes>

  originChannelKey.Assign(aChannel);

  originChannelKey.AppendLiteral("|");

  originChannelKey.Append(NS_ConvertUTF8toUTF16(aOrigin));

  return new BroadcastChannelParent(originChannelKey);
}

namespace {

struct MOZ_STACK_CLASS NullifyContentParentRAII
{
  explicit NullifyContentParentRAII(RefPtr<ContentParent>& aContentParent)
    : mContentParent(aContentParent)
  {}

  ~NullifyContentParentRAII()
  {
    mContentParent = nullptr;
  }

  RefPtr<ContentParent>& mContentParent;
};

class CheckPrincipalRunnable final : public Runnable
{
public:
  CheckPrincipalRunnable(already_AddRefed<ContentParent> aParent,
                         const PrincipalInfo& aPrincipalInfo,
                         const nsCString& aOrigin)
    : mContentParent(aParent)
    , mPrincipalInfo(aPrincipalInfo)
    , mOrigin(aOrigin)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mContentParent);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    NullifyContentParentRAII raii(mContentParent);

    nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(mPrincipalInfo);

    if (principal->GetIsNullPrincipal()) {
      mContentParent->KillHard("BroadcastChannel killed: no null principal.");
      return NS_OK;
    }

    nsAutoCString origin;
    nsresult rv = principal->GetOrigin(origin);
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
  RefPtr<ContentParent> mContentParent;
  PrincipalInfo mPrincipalInfo;
  nsCString mOrigin;
};

class CheckPermissionRunnable final : public Runnable
{
public:
  CheckPermissionRunnable(already_AddRefed<ContentParent> aParent,
                          FileSystemRequestParent* aActor,
                          FileSystemBase::ePermissionCheckType aPermissionCheckType,
                          const nsCString& aPermissionName)
    : mContentParent(aParent)
    , mActor(aActor)
    , mPermissionCheckType(aPermissionCheckType)
    , mPermissionName(aPermissionName)
    , mBackgroundEventTarget(NS_GetCurrentThread())
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mContentParent);
    MOZ_ASSERT(mBackgroundEventTarget);
    MOZ_ASSERT(mPermissionCheckType == FileSystemBase::ePermissionCheckRequired ||
               mPermissionCheckType == FileSystemBase::ePermissionCheckByTestingPref);
  }

  NS_IMETHOD
  Run() override
  {
    if (NS_IsMainThread()) {
      NullifyContentParentRAII raii(mContentParent);

      // If the permission is granted, we go back to the background thread to
      // dispatch this task.
      if (CheckPermission()) {
        return mBackgroundEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
      }

      return NS_OK;
    }

    AssertIsOnBackgroundThread();

    // It can happen that this actor has been destroyed in the meantime we were
    // on the main-thread.
    if (!mActor->Destroyed()) {
      mActor->Start();
    }

    return NS_OK;
  }

private:
  ~CheckPermissionRunnable()
  {
     NS_ProxyRelease(mBackgroundEventTarget, mActor.forget());
  }

  bool
  CheckPermission()
  {
    if (mPermissionCheckType == FileSystemBase::ePermissionCheckByTestingPref &&
        mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
      return true;
    }

    return true;
  }

  RefPtr<ContentParent> mContentParent;

  RefPtr<FileSystemRequestParent> mActor;

  FileSystemBase::ePermissionCheckType mPermissionCheckType;
  nsCString mPermissionName;

  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;
};

} // namespace

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBroadcastChannelConstructor(
                                            PBroadcastChannelParent* actor,
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsCString& aOrigin,
                                            const nsString& aChannel)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    MOZ_ASSERT(aPrincipalInfo.type() != PrincipalInfo::TNullPrincipalInfo);
    return IPC_OK();
  }

  RefPtr<CheckPrincipalRunnable> runnable =
    new CheckPrincipalRunnable(parent.forget(), aPrincipalInfo, aOrigin);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return IPC_OK();
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

  RefPtr<dom::workers::ServiceWorkerManagerParent> agent =
    new dom::workers::ServiceWorkerManagerParent();
  return agent.forget().take();
}

bool
BackgroundParentImpl::DeallocPServiceWorkerManagerParent(
                                            PServiceWorkerManagerParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<dom::workers::ServiceWorkerManagerParent> parent =
    dont_AddRef(static_cast<dom::workers::ServiceWorkerManagerParent*>(aActor));
  MOZ_ASSERT(parent);
  return true;
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvShutdownServiceWorkerRegistrar()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<dom::ServiceWorkerRegistrar> service =
    dom::ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(service);

  service->Shutdown();
  return IPC_OK();
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

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPMessagePortConstructor(PMessagePortParent* aActor,
                                                  const nsID& aUUID,
                                                  const nsID& aDestinationUUID,
                                                  const uint32_t& aSequenceID)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  MessagePortParent* mp = static_cast<MessagePortParent*>(aActor);
  if (!mp->Entangle(aDestinationUUID, aSequenceID)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
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

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvMessagePortForceClose(const nsID& aUUID,
                                                const nsID& aDestinationUUID,
                                                const uint32_t& aSequenceID)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!MessagePortParent::ForceClose(aUUID, aDestinationUUID, aSequenceID)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PAsmJSCacheEntryParent*
BackgroundParentImpl::AllocPAsmJSCacheEntryParent(
                               const dom::asmjscache::OpenMode& aOpenMode,
                               const dom::asmjscache::WriteParams& aWriteParams,
                               const PrincipalInfo& aPrincipalInfo)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return
    dom::asmjscache::AllocEntryParent(aOpenMode, aWriteParams, aPrincipalInfo);
}

bool
BackgroundParentImpl::DeallocPAsmJSCacheEntryParent(
                                                 PAsmJSCacheEntryParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  dom::asmjscache::DeallocEntryParent(aActor);
  return true;
}

BackgroundParentImpl::PQuotaParent*
BackgroundParentImpl::AllocPQuotaParent()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::quota::AllocPQuotaParent();
}

bool
BackgroundParentImpl::DeallocPQuotaParent(PQuotaParent* aActor)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::quota::DeallocPQuotaParent(aActor);
}

dom::PFileSystemRequestParent*
BackgroundParentImpl::AllocPFileSystemRequestParent(
                                                const FileSystemParams& aParams)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<FileSystemRequestParent> result = new FileSystemRequestParent();

  if (NS_WARN_IF(!result->Initialize(aParams))) {
    return nullptr;
  }

  return result.forget().take();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPFileSystemRequestConstructor(
                                               PFileSystemRequestParent* aActor,
                                               const FileSystemParams& aParams)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<FileSystemRequestParent> actor = static_cast<FileSystemRequestParent*>(aActor);

  if (actor->PermissionCheckType() == FileSystemBase::ePermissionCheckNotRequired) {
    actor->Start();
    return IPC_OK();
  }

  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    actor->Start();
    return IPC_OK();
  }

  const nsCString& permissionName = actor->PermissionName();
  MOZ_ASSERT(!permissionName.IsEmpty());

  // At this point we should have the right permission but we do the last check
  // with this runnable. If the app doesn't have the permission, we kill the
  // child process.
  RefPtr<CheckPermissionRunnable> runnable =
    new CheckPermissionRunnable(parent.forget(), actor,
                                actor->PermissionCheckType(), permissionName);

  nsresult rv = NS_DispatchToMainThread(runnable);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  return IPC_OK();
}

bool
BackgroundParentImpl::DeallocPFileSystemRequestParent(
                                              PFileSystemRequestParent* aDoomed)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<FileSystemRequestParent> parent =
    dont_AddRef(static_cast<FileSystemRequestParent*>(aDoomed));
  return true;
}

// Gamepad API Background IPC
dom::PGamepadEventChannelParent*
BackgroundParentImpl::AllocPGamepadEventChannelParent()
{
#ifdef MOZ_GAMEPAD
  RefPtr<dom::GamepadEventChannelParent> parent =
    new dom::GamepadEventChannelParent();

  return parent.forget().take();
#else
  return nullptr;
#endif
}

bool
BackgroundParentImpl::DeallocPGamepadEventChannelParent(dom::PGamepadEventChannelParent *aActor)
{
#ifdef MOZ_GAMEPAD
  MOZ_ASSERT(aActor);
  RefPtr<dom::GamepadEventChannelParent> parent =
    dont_AddRef(static_cast<dom::GamepadEventChannelParent*>(aActor));
#endif
  return true;
}

dom::PGamepadTestChannelParent*
BackgroundParentImpl::AllocPGamepadTestChannelParent()
{
#ifdef MOZ_GAMEPAD
  RefPtr<dom::GamepadTestChannelParent> parent =
    new dom::GamepadTestChannelParent();

  return parent.forget().take();
#else
  return nullptr;
#endif
}

bool
BackgroundParentImpl::DeallocPGamepadTestChannelParent(dom::PGamepadTestChannelParent *aActor)
{
#ifdef MOZ_GAMEPAD
  MOZ_ASSERT(aActor);
  RefPtr<dom::GamepadTestChannelParent> parent =
    dont_AddRef(static_cast<dom::GamepadTestChannelParent*>(aActor));
#endif
  return true;
}

} // namespace ipc
} // namespace mozilla

void
TestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mozilla::ipc::AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
}
