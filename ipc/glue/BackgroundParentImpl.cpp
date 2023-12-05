/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundParentImpl.h"

#include "BroadcastChannelParent.h"
#ifdef MOZ_WEBRTC
#  include "CamerasParent.h"
#endif
#include "mozilla/Assertions.h"
#include "mozilla/RDDProcessManager.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/RemoteDecodeUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BackgroundSessionStorageServiceParent.h"
#include "mozilla/dom/ClientManagerActors.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/EndpointForReportParent.h"
#include "mozilla/dom/FetchParent.h"
#include "mozilla/dom/FileCreatorParent.h"
#include "mozilla/dom/FileSystemManagerParentFactory.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/GamepadEventChannelParent.h"
#include "mozilla/dom/GamepadTestChannelParent.h"
#include "mozilla/dom/MIDIManagerParent.h"
#include "mozilla/dom/MIDIPlatformService.h"
#include "mozilla/dom/MIDIPortParent.h"
#include "mozilla/dom/MessagePortParent.h"
#include "mozilla/dom/PGamepadEventChannelParent.h"
#include "mozilla/dom/PGamepadTestChannelParent.h"
#include "mozilla/dom/RemoteWorkerControllerParent.h"
#include "mozilla/dom/RemoteWorkerServiceParent.h"
#include "mozilla/dom/ReportingHeader.h"
#include "mozilla/dom/ServiceWorkerActors.h"
#include "mozilla/dom/ServiceWorkerContainerParent.h"
#include "mozilla/dom/ServiceWorkerManagerParent.h"
#include "mozilla/dom/ServiceWorkerParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrationParent.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/dom/SharedWorkerParent.h"
#include "mozilla/dom/StorageActivityService.h"
#include "mozilla/dom/StorageIPC.h"
#include "mozilla/dom/TemporaryIPCBlobParent.h"
#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/dom/WebTransportParent.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/locks/LockManagerParent.h"
#include "mozilla/dom/localstorage/ActorsParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "mozilla/dom/quota/ActorsParent.h"
#include "mozilla/dom/quota/QuotaParent.h"
#include "mozilla/dom/simpledb/ActorsParent.h"
#include "mozilla/dom/VsyncParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/IdleSchedulerParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/PBackgroundTestParent.h"
#include "mozilla/net/BackgroundDataBridgeParent.h"
#include "mozilla/net/HttpBackgroundChannelParent.h"
#include "nsIHttpChannelInternal.h"
#include "nsIPrincipal.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

using mozilla::AssertIsOnMainThread;
using mozilla::dom::FileSystemRequestParent;
using mozilla::dom::MessagePortParent;
using mozilla::dom::MIDIManagerParent;
using mozilla::dom::MIDIPlatformService;
using mozilla::dom::MIDIPortParent;
using mozilla::dom::PMessagePortParent;
using mozilla::dom::PMIDIManagerParent;
using mozilla::dom::PMIDIPortParent;
using mozilla::dom::PServiceWorkerContainerParent;
using mozilla::dom::PServiceWorkerParent;
using mozilla::dom::PServiceWorkerRegistrationParent;
using mozilla::dom::ServiceWorkerParent;
using mozilla::dom::UDPSocketParent;
using mozilla::dom::WebAuthnTransactionParent;
using mozilla::dom::cache::PCacheParent;
using mozilla::dom::cache::PCacheStorageParent;
using mozilla::dom::cache::PCacheStreamControlParent;
using mozilla::ipc::AssertIsOnBackgroundThread;

namespace {

class TestParent final : public mozilla::ipc::PBackgroundTestParent {
  friend class mozilla::ipc::BackgroundParentImpl;

  MOZ_COUNTED_DEFAULT_CTOR(TestParent)

 protected:
  ~TestParent() override { MOZ_COUNT_DTOR(TestParent); }

 public:
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace

namespace mozilla::ipc {

using mozilla::dom::BroadcastChannelParent;
using mozilla::dom::ContentParent;
using mozilla::dom::ThreadsafeContentParentHandle;

BackgroundParentImpl::BackgroundParentImpl() {
  AssertIsInMainProcess();

  MOZ_COUNT_CTOR(mozilla::ipc::BackgroundParentImpl);
}

BackgroundParentImpl::~BackgroundParentImpl() {
  AssertIsInMainProcess();
  AssertIsOnMainThread();

  MOZ_COUNT_DTOR(mozilla::ipc::BackgroundParentImpl);
}

void BackgroundParentImpl::ProcessingError(Result aCode, const char* aReason) {
  if (MsgDropped == aCode) {
    return;
  }

  // XXX Remove this cut-out once bug 1858621 is fixed. Some parent actors
  // currently return nullptr in actor allocation methods for non fatal errors.
  // We don't want to crash the parent process or child processes in that case.
  if (MsgValueError == aCode) {
    return;
  }

  // Other errors are big deals.
  nsDependentCString reason(aReason);
  if (BackgroundParent::IsOtherProcessActor(this)) {
#ifndef FUZZING
    BackgroundParent::KillHardAsync(this, reason);
#endif
    if (CanSend()) {
      GetIPCChannel()->InduceConnectionError();
    }
  } else {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ipc_channel_error, reason);

    MOZ_CRASH("in-process BackgroundParent abort due to IPC error");
  }
}

void BackgroundParentImpl::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
}

BackgroundParentImpl::PBackgroundTestParent*
BackgroundParentImpl::AllocPBackgroundTestParent(const nsACString& aTestArg) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new TestParent();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBackgroundTestConstructor(
    PBackgroundTestParent* aActor, const nsACString& aTestArg) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!PBackgroundTestParent::Send__delete__(aActor, aTestArg)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundTestParent(
    PBackgroundTestParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<TestParent*>(aActor);
  return true;
}

auto BackgroundParentImpl::AllocPBackgroundIDBFactoryParent(
    const LoggingInfo& aLoggingInfo)
    -> already_AddRefed<PBackgroundIDBFactoryParent> {
  using mozilla::dom::indexedDB::AllocPBackgroundIDBFactoryParent;

  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return AllocPBackgroundIDBFactoryParent(aLoggingInfo);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundIDBFactoryConstructor(
    PBackgroundIDBFactoryParent* aActor, const LoggingInfo& aLoggingInfo) {
  using mozilla::dom::indexedDB::RecvPBackgroundIDBFactoryConstructor;

  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!RecvPBackgroundIDBFactoryConstructor(aActor, aLoggingInfo)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

auto BackgroundParentImpl::AllocPBackgroundIndexedDBUtilsParent()
    -> PBackgroundIndexedDBUtilsParent* {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::indexedDB::AllocPBackgroundIndexedDBUtilsParent();
}

bool BackgroundParentImpl::DeallocPBackgroundIndexedDBUtilsParent(
    PBackgroundIndexedDBUtilsParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::indexedDB::DeallocPBackgroundIndexedDBUtilsParent(
      aActor);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvFlushPendingFileDeletions() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!mozilla::dom::indexedDB::RecvFlushPendingFileDeletions()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

BackgroundParentImpl::PBackgroundSDBConnectionParent*
BackgroundParentImpl::AllocPBackgroundSDBConnectionParent(
    const PersistenceType& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundSDBConnectionParent(aPersistenceType,
                                                           aPrincipalInfo);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundSDBConnectionConstructor(
    PBackgroundSDBConnectionParent* aActor,
    const PersistenceType& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPBackgroundSDBConnectionConstructor(
          aActor, aPersistenceType, aPrincipalInfo)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundSDBConnectionParent(
    PBackgroundSDBConnectionParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundSDBConnectionParent(aActor);
}

BackgroundParentImpl::PBackgroundLSDatabaseParent*
BackgroundParentImpl::AllocPBackgroundLSDatabaseParent(
    const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
    const uint64_t& aDatastoreId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSDatabaseParent(
      aPrincipalInfo, aPrivateBrowsingId, aDatastoreId);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSDatabaseConstructor(
    PBackgroundLSDatabaseParent* aActor, const PrincipalInfo& aPrincipalInfo,
    const uint32_t& aPrivateBrowsingId, const uint64_t& aDatastoreId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPBackgroundLSDatabaseConstructor(
          aActor, aPrincipalInfo, aPrivateBrowsingId, aDatastoreId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundLSDatabaseParent(
    PBackgroundLSDatabaseParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSDatabaseParent(aActor);
}

BackgroundParentImpl::PBackgroundLSObserverParent*
BackgroundParentImpl::AllocPBackgroundLSObserverParent(
    const uint64_t& aObserverId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSObserverParent(aObserverId);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSObserverConstructor(
    PBackgroundLSObserverParent* aActor, const uint64_t& aObserverId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPBackgroundLSObserverConstructor(aActor,
                                                          aObserverId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundLSObserverParent(
    PBackgroundLSObserverParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSObserverParent(aActor);
}

BackgroundParentImpl::PBackgroundLSRequestParent*
BackgroundParentImpl::AllocPBackgroundLSRequestParent(
    const LSRequestParams& aParams) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSRequestParent(this, aParams);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSRequestConstructor(
    PBackgroundLSRequestParent* aActor, const LSRequestParams& aParams) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPBackgroundLSRequestConstructor(aActor, aParams)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundLSRequestParent(
    PBackgroundLSRequestParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSRequestParent(aActor);
}

BackgroundParentImpl::PBackgroundLSSimpleRequestParent*
BackgroundParentImpl::AllocPBackgroundLSSimpleRequestParent(
    const LSSimpleRequestParams& aParams) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSSimpleRequestParent(this, aParams);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSSimpleRequestConstructor(
    PBackgroundLSSimpleRequestParent* aActor,
    const LSSimpleRequestParams& aParams) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPBackgroundLSSimpleRequestConstructor(aActor,
                                                               aParams)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundLSSimpleRequestParent(
    PBackgroundLSSimpleRequestParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSSimpleRequestParent(aActor);
}

BackgroundParentImpl::PBackgroundLocalStorageCacheParent*
BackgroundParentImpl::AllocPBackgroundLocalStorageCacheParent(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aOriginKey,
    const uint32_t& aPrivateBrowsingId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLocalStorageCacheParent(
      aPrincipalInfo, aOriginKey, aPrivateBrowsingId);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLocalStorageCacheConstructor(
    PBackgroundLocalStorageCacheParent* aActor,
    const PrincipalInfo& aPrincipalInfo, const nsACString& aOriginKey,
    const uint32_t& aPrivateBrowsingId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::RecvPBackgroundLocalStorageCacheConstructor(
      this, aActor, aPrincipalInfo, aOriginKey, aPrivateBrowsingId);
}

bool BackgroundParentImpl::DeallocPBackgroundLocalStorageCacheParent(
    PBackgroundLocalStorageCacheParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLocalStorageCacheParent(aActor);
}

auto BackgroundParentImpl::AllocPBackgroundStorageParent(
    const nsAString& aProfilePath, const uint32_t& aPrivateBrowsingId)
    -> PBackgroundStorageParent* {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundStorageParent(aProfilePath,
                                                     aPrivateBrowsingId);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBackgroundStorageConstructor(
    PBackgroundStorageParent* aActor, const nsAString& aProfilePath,
    const uint32_t& aPrivateBrowsingId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::RecvPBackgroundStorageConstructor(aActor, aProfilePath,
                                                         aPrivateBrowsingId);
}

bool BackgroundParentImpl::DeallocPBackgroundStorageParent(
    PBackgroundStorageParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundStorageParent(aActor);
}

already_AddRefed<BackgroundParentImpl::PBackgroundSessionStorageManagerParent>
BackgroundParentImpl::AllocPBackgroundSessionStorageManagerParent(
    const uint64_t& aTopContextId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return dom::AllocPBackgroundSessionStorageManagerParent(aTopContextId);
}

already_AddRefed<mozilla::dom::PBackgroundSessionStorageServiceParent>
BackgroundParentImpl::AllocPBackgroundSessionStorageServiceParent() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return MakeAndAddRef<mozilla::dom::BackgroundSessionStorageServiceParent>();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvCreateFileSystemManagerParent(
    const PrincipalInfo& aPrincipalInfo,
    Endpoint<PFileSystemManagerParent>&& aParentEndpoint,
    CreateFileSystemManagerParentResolver&& aResolver) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::CreateFileSystemManagerParent(
      aPrincipalInfo, std::move(aParentEndpoint), std::move(aResolver));
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvCreateWebTransportParent(
    const nsAString& aURL, nsIPrincipal* aPrincipal,
    const mozilla::Maybe<IPCClientInfo>& aClientInfo, const bool& aDedicated,
    const bool& aRequireUnreliable, const uint32_t& aCongestionControl,
    // Sequence<WebTransportHash>* aServerCertHashes,
    Endpoint<PWebTransportParent>&& aParentEndpoint,
    CreateWebTransportParentResolver&& aResolver) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<mozilla::dom::WebTransportParent> webt =
      new mozilla::dom::WebTransportParent();
  webt->Create(aURL, aPrincipal, aClientInfo, aDedicated, aRequireUnreliable,
               aCongestionControl,
               /*aServerCertHashes, */ std::move(aParentEndpoint),
               std::move(aResolver));
  return IPC_OK();
}

already_AddRefed<PIdleSchedulerParent>
BackgroundParentImpl::AllocPIdleSchedulerParent() {
  AssertIsOnBackgroundThread();
  RefPtr<IdleSchedulerParent> actor = new IdleSchedulerParent();
  return actor.forget();
}

dom::PRemoteWorkerControllerParent*
BackgroundParentImpl::AllocPRemoteWorkerControllerParent(
    const dom::RemoteWorkerData& aRemoteWorkerData) {
  RefPtr<dom::RemoteWorkerControllerParent> actor =
      new dom::RemoteWorkerControllerParent(aRemoteWorkerData);
  return actor.forget().take();
}

IPCResult BackgroundParentImpl::RecvPRemoteWorkerControllerConstructor(
    dom::PRemoteWorkerControllerParent* aActor,
    const dom::RemoteWorkerData& aRemoteWorkerData) {
  MOZ_ASSERT(aActor);

  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPRemoteWorkerControllerParent(
    dom::PRemoteWorkerControllerParent* aActor) {
  RefPtr<dom::RemoteWorkerControllerParent> actor =
      dont_AddRef(static_cast<dom::RemoteWorkerControllerParent*>(aActor));
  return true;
}

already_AddRefed<dom::PRemoteWorkerServiceParent>
BackgroundParentImpl::AllocPRemoteWorkerServiceParent() {
  return MakeAndAddRef<dom::RemoteWorkerServiceParent>();
}

IPCResult BackgroundParentImpl::RecvPRemoteWorkerServiceConstructor(
    PRemoteWorkerServiceParent* aActor) {
  mozilla::dom::RemoteWorkerServiceParent* actor =
      static_cast<mozilla::dom::RemoteWorkerServiceParent*>(aActor);

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(this);
  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    actor->Initialize(NOT_REMOTE_TYPE);
  } else {
    actor->Initialize(parent->GetRemoteType());
  }
  return IPC_OK();
}

mozilla::dom::PSharedWorkerParent*
BackgroundParentImpl::AllocPSharedWorkerParent(
    const mozilla::dom::RemoteWorkerData& aData, const uint64_t& aWindowID,
    const mozilla::dom::MessagePortIdentifier& aPortIdentifier) {
  RefPtr<dom::SharedWorkerParent> agent =
      new mozilla::dom::SharedWorkerParent();
  return agent.forget().take();
}

IPCResult BackgroundParentImpl::RecvPSharedWorkerConstructor(
    PSharedWorkerParent* aActor, const mozilla::dom::RemoteWorkerData& aData,
    const uint64_t& aWindowID,
    const mozilla::dom::MessagePortIdentifier& aPortIdentifier) {
  mozilla::dom::SharedWorkerParent* actor =
      static_cast<mozilla::dom::SharedWorkerParent*>(aActor);
  actor->Initialize(aData, aWindowID, aPortIdentifier);
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPSharedWorkerParent(
    mozilla::dom::PSharedWorkerParent* aActor) {
  RefPtr<mozilla::dom::SharedWorkerParent> actor =
      dont_AddRef(static_cast<mozilla::dom::SharedWorkerParent*>(aActor));
  return true;
}

dom::PFileCreatorParent* BackgroundParentImpl::AllocPFileCreatorParent(
    const nsAString& aFullPath, const nsAString& aType, const nsAString& aName,
    const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
    const bool& aIsFromNsIFile) {
  RefPtr<dom::FileCreatorParent> actor = new dom::FileCreatorParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPFileCreatorConstructor(
    dom::PFileCreatorParent* aActor, const nsAString& aFullPath,
    const nsAString& aType, const nsAString& aName,
    const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
    const bool& aIsFromNsIFile) {
  bool isFileRemoteType = false;

  // If the ContentParentHandle is null we are dealing with a same-process
  // actor.
  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(this);
  if (!parent) {
    isFileRemoteType = true;
  } else {
    isFileRemoteType = parent->GetRemoteType() == FILE_REMOTE_TYPE;
  }

  dom::FileCreatorParent* actor = static_cast<dom::FileCreatorParent*>(aActor);

  // We allow the creation of File via this IPC call only for the 'file' process
  // or for testing.
  if (!isFileRemoteType && !StaticPrefs::dom_file_createInChild()) {
    Unused << dom::FileCreatorParent::Send__delete__(
        actor, dom::FileCreationErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return IPC_OK();
  }

  return actor->CreateAndShareFile(aFullPath, aType, aName, aLastModified,
                                   aExistenceCheck, aIsFromNsIFile);
}

bool BackgroundParentImpl::DeallocPFileCreatorParent(
    dom::PFileCreatorParent* aActor) {
  RefPtr<dom::FileCreatorParent> actor =
      dont_AddRef(static_cast<dom::FileCreatorParent*>(aActor));
  return true;
}

dom::PTemporaryIPCBlobParent*
BackgroundParentImpl::AllocPTemporaryIPCBlobParent() {
  return new dom::TemporaryIPCBlobParent();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPTemporaryIPCBlobConstructor(
    dom::PTemporaryIPCBlobParent* aActor) {
  dom::TemporaryIPCBlobParent* actor =
      static_cast<dom::TemporaryIPCBlobParent*>(aActor);
  return actor->CreateAndShareFile();
}

bool BackgroundParentImpl::DeallocPTemporaryIPCBlobParent(
    dom::PTemporaryIPCBlobParent* aActor) {
  delete aActor;
  return true;
}

already_AddRefed<BackgroundParentImpl::PVsyncParent>
BackgroundParentImpl::AllocPVsyncParent() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<mozilla::dom::VsyncParent> actor = new mozilla::dom::VsyncParent();

  RefPtr<mozilla::VsyncDispatcher> vsyncDispatcher =
      gfxPlatform::GetPlatform()->GetGlobalVsyncDispatcher();
  actor->UpdateVsyncDispatcher(vsyncDispatcher);
  return actor.forget();
}

camera::PCamerasParent* BackgroundParentImpl::AllocPCamerasParent() {
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

#ifdef MOZ_WEBRTC
mozilla::ipc::IPCResult BackgroundParentImpl::RecvPCamerasConstructor(
    camera::PCamerasParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  return static_cast<camera::CamerasParent*>(aActor)->RecvPCamerasConstructor();
}
#endif

bool BackgroundParentImpl::DeallocPCamerasParent(
    camera::PCamerasParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

#ifdef MOZ_WEBRTC
  RefPtr<mozilla::camera::CamerasParent> actor =
      dont_AddRef(static_cast<mozilla::camera::CamerasParent*>(aActor));
#endif
  return true;
}

auto BackgroundParentImpl::AllocPUDPSocketParent(
    const Maybe<PrincipalInfo>& /* unused */, const nsACString& /* unused */)
    -> PUDPSocketParent* {
  RefPtr<UDPSocketParent> p = new UDPSocketParent(this);

  return p.forget().take();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPUDPSocketConstructor(
    PUDPSocketParent* aActor, const Maybe<PrincipalInfo>& aOptionalPrincipal,
    const nsACString& aFilter) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (aOptionalPrincipal.isSome()) {
    // Support for checking principals (for non-mtransport use) will be handled
    // in bug 1167039
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

  if (!static_cast<UDPSocketParent*>(aActor)->Init(nullptr, aFilter)) {
    MOZ_CRASH("UDPSocketCallback - failed init");
  }

  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPUDPSocketParent(PUDPSocketParent* actor) {
  UDPSocketParent* p = static_cast<UDPSocketParent*>(actor);
  p->Release();
  return true;
}

mozilla::dom::PBroadcastChannelParent*
BackgroundParentImpl::AllocPBroadcastChannelParent(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aOrigin,
    const nsAString& aChannel) {
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

class CheckPrincipalRunnable final : public Runnable {
 public:
  CheckPrincipalRunnable(
      already_AddRefed<ThreadsafeContentParentHandle> aParent,
      const PrincipalInfo& aPrincipalInfo, const nsACString& aOrigin)
      : Runnable("ipc::CheckPrincipalRunnable"),
        mContentParent(aParent),
        mPrincipalInfo(aPrincipalInfo),
        mOrigin(aOrigin) {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mContentParent);
  }

  NS_IMETHOD Run() override {
    AssertIsOnMainThread();
    RefPtr<ContentParent> contentParent = mContentParent->GetContentParent();
    if (!contentParent) {
      return NS_OK;
    }

    auto principalOrErr = PrincipalInfoToPrincipal(mPrincipalInfo);
    if (NS_WARN_IF(principalOrErr.isErr())) {
      contentParent->KillHard(
          "BroadcastChannel killed: PrincipalInfoToPrincipal failed.");
      return NS_OK;
    }

    nsAutoCString origin;
    nsresult rv = principalOrErr.unwrap()->GetOrigin(origin);
    if (NS_FAILED(rv)) {
      contentParent->KillHard(
          "BroadcastChannel killed: principal::GetOrigin failed.");
      return NS_OK;
    }

    if (NS_WARN_IF(!mOrigin.Equals(origin))) {
      contentParent->KillHard("BroadcastChannel killed: origins do not match.");
      return NS_OK;
    }

    return NS_OK;
  }

 private:
  RefPtr<ThreadsafeContentParentHandle> mContentParent;
  PrincipalInfo mPrincipalInfo;
  nsCString mOrigin;
};

}  // namespace

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBroadcastChannelConstructor(
    PBroadcastChannelParent* actor, const PrincipalInfo& aPrincipalInfo,
    const nsACString& aOrigin, const nsAString& aChannel) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(this);

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    return IPC_OK();
  }

  // XXX The principal can be checked right here on the PBackground thread
  // since BackgroundParentImpl now overrides the ProcessingError method and
  // kills invalid child processes (IPC_FAIL triggers a processing error).

  RefPtr<CheckPrincipalRunnable> runnable =
      new CheckPrincipalRunnable(parent.forget(), aPrincipalInfo, aOrigin);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBroadcastChannelParent(
    PBroadcastChannelParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<BroadcastChannelParent*>(aActor);
  return true;
}

mozilla::dom::PServiceWorkerManagerParent*
BackgroundParentImpl::AllocPServiceWorkerManagerParent() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<dom::ServiceWorkerManagerParent> agent =
      new dom::ServiceWorkerManagerParent();
  return agent.forget().take();
}

bool BackgroundParentImpl::DeallocPServiceWorkerManagerParent(
    PServiceWorkerManagerParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<dom::ServiceWorkerManagerParent> parent =
      dont_AddRef(static_cast<dom::ServiceWorkerManagerParent*>(aActor));
  MOZ_ASSERT(parent);
  return true;
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvShutdownServiceWorkerRegistrar() {
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

already_AddRefed<PCacheStorageParent>
BackgroundParentImpl::AllocPCacheStorageParent(
    const Namespace& aNamespace, const PrincipalInfo& aPrincipalInfo) {
  return dom::cache::AllocPCacheStorageParent(this, aNamespace, aPrincipalInfo);
}

PMessagePortParent* BackgroundParentImpl::AllocPMessagePortParent(
    const nsID& aUUID, const nsID& aDestinationUUID,
    const uint32_t& aSequenceID) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return new MessagePortParent(aUUID);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPMessagePortConstructor(
    PMessagePortParent* aActor, const nsID& aUUID, const nsID& aDestinationUUID,
    const uint32_t& aSequenceID) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  MessagePortParent* mp = static_cast<MessagePortParent*>(aActor);
  if (!mp->Entangle(aDestinationUUID, aSequenceID)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPMessagePortParent(
    PMessagePortParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<MessagePortParent*>(aActor);
  return true;
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvMessagePortForceClose(
    const nsID& aUUID, const nsID& aDestinationUUID,
    const uint32_t& aSequenceID) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!MessagePortParent::ForceClose(aUUID, aDestinationUUID, aSequenceID)) {
    return IPC_FAIL(this, "MessagePortParent::ForceClose failed.");
  }

  return IPC_OK();
}

BackgroundParentImpl::PQuotaParent* BackgroundParentImpl::AllocPQuotaParent() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::quota::AllocPQuotaParent();
}

bool BackgroundParentImpl::DeallocPQuotaParent(PQuotaParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::quota::DeallocPQuotaParent(aActor);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvShutdownQuotaManager() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mozilla::dom::quota::RecvShutdownQuotaManager()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvShutdownBackgroundSessionStorageManagers() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mozilla::dom::RecvShutdownBackgroundSessionStorageManagers()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPropagateBackgroundSessionStorageManager(
    const uint64_t& aCurrentTopContextId, const uint64_t& aTargetTopContextId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL(this, "Wrong actor");
  }

  mozilla::dom::RecvPropagateBackgroundSessionStorageManager(
      aCurrentTopContextId, aTargetTopContextId);

  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvRemoveBackgroundSessionStorageManager(
    const uint64_t& aTopContextId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mozilla::dom::RecvRemoveBackgroundSessionStorageManager(aTopContextId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvGetSessionStorageManagerData(
    const uint64_t& aTopContextId, const uint32_t& aSizeLimit,
    const bool& aCancelSessionStoreTimer,
    GetSessionStorageManagerDataResolver&& aResolver) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL(this, "Wrong actor");
  }

  if (!mozilla::dom::RecvGetSessionStorageData(aTopContextId, aSizeLimit,
                                               aCancelSessionStoreTimer,
                                               std::move(aResolver))) {
    return IPC_FAIL(this, "Couldn't get session storage data");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvLoadSessionStorageManagerData(
    const uint64_t& aTopContextId,
    nsTArray<mozilla::dom::SSCacheCopy>&& aOriginCacheCopy) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL(this, "Wrong actor");
  }

  if (!mozilla::dom::RecvLoadSessionStorageData(aTopContextId,
                                                std::move(aOriginCacheCopy))) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

already_AddRefed<dom::PFileSystemRequestParent>
BackgroundParentImpl::AllocPFileSystemRequestParent(
    const FileSystemParams& aParams) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<FileSystemRequestParent> result = new FileSystemRequestParent();

  if (NS_WARN_IF(!result->Initialize(aParams))) {
    return nullptr;
  }

  return result.forget();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPFileSystemRequestConstructor(
    PFileSystemRequestParent* aActor, const FileSystemParams& params) {
  static_cast<FileSystemRequestParent*>(aActor)->Start();
  return IPC_OK();
}

// Gamepad API Background IPC
already_AddRefed<dom::PGamepadEventChannelParent>
BackgroundParentImpl::AllocPGamepadEventChannelParent() {
  return dom::GamepadEventChannelParent::Create();
}

already_AddRefed<dom::PGamepadTestChannelParent>
BackgroundParentImpl::AllocPGamepadTestChannelParent() {
  return dom::GamepadTestChannelParent::Create();
}

dom::PWebAuthnTransactionParent*
BackgroundParentImpl::AllocPWebAuthnTransactionParent() {
  return new dom::WebAuthnTransactionParent();
}

bool BackgroundParentImpl::DeallocPWebAuthnTransactionParent(
    dom::PWebAuthnTransactionParent* aActor) {
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

already_AddRefed<net::PHttpBackgroundChannelParent>
BackgroundParentImpl::AllocPHttpBackgroundChannelParent(
    const uint64_t& aChannelId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<net::HttpBackgroundChannelParent> actor =
      new net::HttpBackgroundChannelParent();
  return actor.forget();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPHttpBackgroundChannelConstructor(
    net::PHttpBackgroundChannelParent* aActor, const uint64_t& aChannelId) {
  MOZ_ASSERT(aActor);
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  net::HttpBackgroundChannelParent* aParent =
      static_cast<net::HttpBackgroundChannelParent*>(aActor);

  if (NS_WARN_IF(NS_FAILED(aParent->Init(aChannelId)))) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvCreateMIDIPort(
    Endpoint<PMIDIPortParent>&& aEndpoint, const MIDIPortInfo& aPortInfo,
    const bool& aSysexEnabled) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "invalid endpoint for MIDIPort");
  }

  MIDIPlatformService::OwnerThread()->Dispatch(NS_NewRunnableFunction(
      "CreateMIDIPortRunnable", [=, endpoint = std::move(aEndpoint)]() mutable {
        RefPtr<MIDIPortParent> result =
            new MIDIPortParent(aPortInfo, aSysexEnabled);
        endpoint.Bind(result);
      }));

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvCreateMIDIManager(
    Endpoint<PMIDIManagerParent>&& aEndpoint) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "invalid endpoint for MIDIManager");
  }

  MIDIPlatformService::OwnerThread()->Dispatch(NS_NewRunnableFunction(
      "CreateMIDIManagerRunnable",
      [=, endpoint = std::move(aEndpoint)]() mutable {
        RefPtr<MIDIManagerParent> result = new MIDIManagerParent();
        endpoint.Bind(result);
        MIDIPlatformService::Get()->AddManager(result);
      }));

  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvHasMIDIDevice(
    HasMIDIDeviceResolver&& aResolver) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  InvokeAsync(MIDIPlatformService::OwnerThread(), __func__,
              []() {
                bool hasDevice = MIDIPlatformService::Get()->HasDevice();
                return BoolPromise::CreateAndResolve(hasDevice, __func__);
              })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [resolver = std::move(aResolver)](
                 const BoolPromise::ResolveOrRejectValue& r) {
               resolver(r.IsResolve() && r.ResolveValue());
             });

  return IPC_OK();
}

mozilla::dom::PClientManagerParent*
BackgroundParentImpl::AllocPClientManagerParent() {
  return mozilla::dom::AllocClientManagerParent();
}

bool BackgroundParentImpl::DeallocPClientManagerParent(
    mozilla::dom::PClientManagerParent* aActor) {
  return mozilla::dom::DeallocClientManagerParent(aActor);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPClientManagerConstructor(
    mozilla::dom::PClientManagerParent* aActor) {
  mozilla::dom::InitClientManagerParent(aActor);
  return IPC_OK();
}

IPCResult BackgroundParentImpl::RecvStorageActivity(
    const PrincipalInfo& aPrincipalInfo) {
  dom::StorageActivityService::SendActivity(aPrincipalInfo);
  return IPC_OK();
}

IPCResult BackgroundParentImpl::RecvPServiceWorkerManagerConstructor(
    PServiceWorkerManagerParent* const aActor) {
  // Only the parent process is allowed to construct this actor.
  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL_NO_REASON(aActor);
  }
  return IPC_OK();
}

already_AddRefed<PServiceWorkerParent>
BackgroundParentImpl::AllocPServiceWorkerParent(
    const IPCServiceWorkerDescriptor&) {
  return MakeAndAddRef<ServiceWorkerParent>();
}

IPCResult BackgroundParentImpl::RecvPServiceWorkerConstructor(
    PServiceWorkerParent* aActor,
    const IPCServiceWorkerDescriptor& aDescriptor) {
  dom::InitServiceWorkerParent(aActor, aDescriptor);
  return IPC_OK();
}

already_AddRefed<PServiceWorkerContainerParent>
BackgroundParentImpl::AllocPServiceWorkerContainerParent() {
  return MakeAndAddRef<mozilla::dom::ServiceWorkerContainerParent>();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPServiceWorkerContainerConstructor(
    PServiceWorkerContainerParent* aActor) {
  dom::InitServiceWorkerContainerParent(aActor);
  return IPC_OK();
}

already_AddRefed<PServiceWorkerRegistrationParent>
BackgroundParentImpl::AllocPServiceWorkerRegistrationParent(
    const IPCServiceWorkerRegistrationDescriptor&) {
  return MakeAndAddRef<mozilla::dom::ServiceWorkerRegistrationParent>();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPServiceWorkerRegistrationConstructor(
    PServiceWorkerRegistrationParent* aActor,
    const IPCServiceWorkerRegistrationDescriptor& aDescriptor) {
  dom::InitServiceWorkerRegistrationParent(aActor, aDescriptor);
  return IPC_OK();
}

dom::PEndpointForReportParent*
BackgroundParentImpl::AllocPEndpointForReportParent(
    const nsAString& aGroupName, const PrincipalInfo& aPrincipalInfo) {
  RefPtr<dom::EndpointForReportParent> actor =
      new dom::EndpointForReportParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPEndpointForReportConstructor(
    PEndpointForReportParent* aActor, const nsAString& aGroupName,
    const PrincipalInfo& aPrincipalInfo) {
  static_cast<dom::EndpointForReportParent*>(aActor)->Run(aGroupName,
                                                          aPrincipalInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvEnsureRDDProcessAndCreateBridge(
    EnsureRDDProcessAndCreateBridgeResolver&& aResolver) {
  using Type = std::tuple<const nsresult&,
                          Endpoint<mozilla::PRemoteDecoderManagerChild>&&>;

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(this);
  if (NS_WARN_IF(!parent)) {
    aResolver(
        Type(NS_ERROR_NOT_AVAILABLE, Endpoint<PRemoteDecoderManagerChild>()));
    return IPC_OK();
  }

  RDDProcessManager* rdd = RDDProcessManager::Get();
  if (!rdd) {
    aResolver(
        Type(NS_ERROR_NOT_AVAILABLE, Endpoint<PRemoteDecoderManagerChild>()));
    return IPC_OK();
  }

  rdd->EnsureRDDProcessAndCreateBridge(OtherPid(), parent->ChildID())
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [resolver = std::move(aResolver)](
                 mozilla::RDDProcessManager::EnsureRDDPromise::
                     ResolveOrRejectValue&& aValue) mutable {
               if (aValue.IsReject()) {
                 resolver(Type(aValue.RejectValue(),
                               Endpoint<PRemoteDecoderManagerChild>()));
                 return;
               }
               resolver(Type(NS_OK, std::move(aValue.ResolveValue())));
             });
  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvEnsureUtilityProcessAndCreateBridge(
    const RemoteDecodeIn& aLocation,
    EnsureUtilityProcessAndCreateBridgeResolver&& aResolver) {
  base::ProcessId otherPid = OtherPid();
  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(this);
  if (NS_WARN_IF(!parent)) {
    return IPC_FAIL_NO_REASON(this);
  }
  dom::ContentParentId childId = parent->ChildID();
  nsCOMPtr<nsISerialEventTarget> managerThread = GetCurrentSerialEventTarget();
  if (!managerThread) {
    return IPC_FAIL_NO_REASON(this);
  }
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "BackgroundParentImpl::RecvEnsureUtilityProcessAndCreateBridge()",
      [aResolver, managerThread, otherPid, childId, aLocation]() {
        RefPtr<UtilityProcessManager> upm =
            UtilityProcessManager::GetSingleton();
        using Type =
            std::tuple<const nsresult&,
                       Endpoint<mozilla::PRemoteDecoderManagerChild>&&>;
        if (!upm) {
          managerThread->Dispatch(NS_NewRunnableFunction(
              "BackgroundParentImpl::RecvEnsureUtilityProcessAndCreateBridge::"
              "Failure",
              [aResolver]() {
                aResolver(Type(NS_ERROR_NOT_AVAILABLE,
                               Endpoint<PRemoteDecoderManagerChild>()));
              }));
        } else {
          SandboxingKind sbKind = GetSandboxingKindFromLocation(aLocation);
          upm->StartProcessForRemoteMediaDecoding(otherPid, childId, sbKind)
              ->Then(managerThread, __func__,
                     [resolver = aResolver](
                         mozilla::ipc::UtilityProcessManager::
                             StartRemoteDecodingUtilityPromise::
                                 ResolveOrRejectValue&& aValue) mutable {
                       if (aValue.IsReject()) {
                         resolver(Type(aValue.RejectValue(),
                                       Endpoint<PRemoteDecoderManagerChild>()));
                         return;
                       }
                       resolver(Type(NS_OK, std::move(aValue.ResolveValue())));
                     });
        }
      }));
  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvRequestCameraAccess(
    RequestCameraAccessResolver&& aResolver) {
#ifdef MOZ_WEBRTC
  mozilla::camera::CamerasParent::RequestCameraAccess()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [resolver = std::move(aResolver)](
          const mozilla::camera::CamerasParent::CameraAccessRequestPromise::
              ResolveOrRejectValue& aValue) {
        if (aValue.IsResolve()) {
          resolver(aValue.ResolveValue());
        } else {
          resolver(aValue.RejectValue());
        }
      });
#else
  aResolver(NS_ERROR_NOT_IMPLEMENTED);
#endif
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPEndpointForReportParent(
    PEndpointForReportParent* aActor) {
  RefPtr<dom::EndpointForReportParent> actor =
      dont_AddRef(static_cast<dom::EndpointForReportParent*>(aActor));
  return true;
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvRemoveEndpoint(
    const nsAString& aGroupName, const nsACString& aEndpointURL,
    const PrincipalInfo& aPrincipalInfo) {
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "BackgroundParentImpl::RecvRemoveEndpoint(",
      [aGroupName = nsString(aGroupName),
       aEndpointURL = nsCString(aEndpointURL), aPrincipalInfo]() {
        dom::ReportingHeader::RemoveEndpoint(aGroupName, aEndpointURL,
                                             aPrincipalInfo);
      }));

  return IPC_OK();
}

already_AddRefed<dom::locks::PLockManagerParent>
BackgroundParentImpl::AllocPLockManagerParent(NotNull<nsIPrincipal*> aPrincipal,
                                              const nsID& aClientId) {
  return MakeAndAddRef<mozilla::dom::locks::LockManagerParent>(aPrincipal,
                                                               aClientId);
}

already_AddRefed<dom::PFetchParent> BackgroundParentImpl::AllocPFetchParent() {
  return MakeAndAddRef<dom::FetchParent>();
}

}  // namespace mozilla::ipc

void TestParent::ActorDestroy(ActorDestroyReason aWhy) {
  mozilla::ipc::AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
}
