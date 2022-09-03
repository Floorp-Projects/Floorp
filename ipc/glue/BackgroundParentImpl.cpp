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
#include "mozilla/dom/FileCreatorParent.h"
#include "mozilla/dom/FileSystemManagerParentFactory.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/GamepadEventChannelParent.h"
#include "mozilla/dom/GamepadTestChannelParent.h"
#include "mozilla/dom/MIDIManagerParent.h"
#include "mozilla/dom/MIDIPlatformService.h"
#include "mozilla/dom/MIDIPortParent.h"
#include "mozilla/dom/MediaTransportParent.h"
#include "mozilla/dom/MessagePortParent.h"
#include "mozilla/dom/PGamepadEventChannelParent.h"
#include "mozilla/dom/PGamepadTestChannelParent.h"
#include "mozilla/dom/RemoteWorkerControllerParent.h"
#include "mozilla/dom/RemoteWorkerParent.h"
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
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/locks/LockManagerParent.h"
#include "mozilla/dom/localstorage/ActorsParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "mozilla/dom/quota/ActorsParent.h"
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
#include "mozilla/net/HttpConnectionMgrParent.h"
#include "mozilla/net/WebSocketConnectionParent.h"
#include "mozilla/psm/IPCClientCertsParent.h"
#include "mozilla/psm/SelectTLSClientAuthCertParent.h"
#include "mozilla/psm/VerifySSLServerCertParent.h"
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

BackgroundParentImpl::BackgroundParentImpl() {
  AssertIsInMainOrSocketProcess();

  MOZ_COUNT_CTOR(mozilla::ipc::BackgroundParentImpl);
}

BackgroundParentImpl::~BackgroundParentImpl() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();

  MOZ_COUNT_DTOR(mozilla::ipc::BackgroundParentImpl);
}

void BackgroundParentImpl::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
}

already_AddRefed<net::PBackgroundDataBridgeParent>
BackgroundParentImpl::AllocPBackgroundDataBridgeParent(
    const uint64_t& aChannelID) {
  MOZ_ASSERT(XRE_IsSocketProcess(), "Should be in socket process");
  AssertIsOnBackgroundThread();

  RefPtr<net::BackgroundDataBridgeParent> actor =
      new net::BackgroundDataBridgeParent(aChannelID);
  return actor.forget();
}

BackgroundParentImpl::PBackgroundTestParent*
BackgroundParentImpl::AllocPBackgroundTestParent(const nsACString& aTestArg) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return new TestParent();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBackgroundTestConstructor(
    PBackgroundTestParent* aActor, const nsACString& aTestArg) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!PBackgroundTestParent::Send__delete__(aActor, aTestArg)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundTestParent(
    PBackgroundTestParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<TestParent*>(aActor);
  return true;
}

auto BackgroundParentImpl::AllocPBackgroundIDBFactoryParent(
    const LoggingInfo& aLoggingInfo)
    -> already_AddRefed<PBackgroundIDBFactoryParent> {
  using mozilla::dom::indexedDB::AllocPBackgroundIDBFactoryParent;

  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return AllocPBackgroundIDBFactoryParent(aLoggingInfo);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundIDBFactoryConstructor(
    PBackgroundIDBFactoryParent* aActor, const LoggingInfo& aLoggingInfo) {
  using mozilla::dom::indexedDB::RecvPBackgroundIDBFactoryConstructor;

  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!RecvPBackgroundIDBFactoryConstructor(aActor, aLoggingInfo)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

auto BackgroundParentImpl::AllocPBackgroundIndexedDBUtilsParent()
    -> PBackgroundIndexedDBUtilsParent* {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::indexedDB::AllocPBackgroundIndexedDBUtilsParent();
}

bool BackgroundParentImpl::DeallocPBackgroundIndexedDBUtilsParent(
    PBackgroundIndexedDBUtilsParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::indexedDB::DeallocPBackgroundIndexedDBUtilsParent(
      aActor);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvFlushPendingFileDeletions() {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundSDBConnectionParent(aPersistenceType,
                                                           aPrincipalInfo);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundSDBConnectionConstructor(
    PBackgroundSDBConnectionParent* aActor,
    const PersistenceType& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo) {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundSDBConnectionParent(aActor);
}

BackgroundParentImpl::PBackgroundLSDatabaseParent*
BackgroundParentImpl::AllocPBackgroundLSDatabaseParent(
    const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
    const uint64_t& aDatastoreId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSDatabaseParent(
      aPrincipalInfo, aPrivateBrowsingId, aDatastoreId);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSDatabaseConstructor(
    PBackgroundLSDatabaseParent* aActor, const PrincipalInfo& aPrincipalInfo,
    const uint32_t& aPrivateBrowsingId, const uint64_t& aDatastoreId) {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSDatabaseParent(aActor);
}

BackgroundParentImpl::PBackgroundLSObserverParent*
BackgroundParentImpl::AllocPBackgroundLSObserverParent(
    const uint64_t& aObserverId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSObserverParent(aObserverId);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSObserverConstructor(
    PBackgroundLSObserverParent* aActor, const uint64_t& aObserverId) {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSObserverParent(aActor);
}

BackgroundParentImpl::PBackgroundLSRequestParent*
BackgroundParentImpl::AllocPBackgroundLSRequestParent(
    const LSRequestParams& aParams) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSRequestParent(this, aParams);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSRequestConstructor(
    PBackgroundLSRequestParent* aActor, const LSRequestParams& aParams) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPBackgroundLSRequestConstructor(aActor, aParams)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBackgroundLSRequestParent(
    PBackgroundLSRequestParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSRequestParent(aActor);
}

BackgroundParentImpl::PBackgroundLSSimpleRequestParent*
BackgroundParentImpl::AllocPBackgroundLSSimpleRequestParent(
    const LSSimpleRequestParams& aParams) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLSSimpleRequestParent(this, aParams);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLSSimpleRequestConstructor(
    PBackgroundLSSimpleRequestParent* aActor,
    const LSSimpleRequestParams& aParams) {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLSSimpleRequestParent(aActor);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvLSClearPrivateBrowsing() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  if (BackgroundParent::IsOtherProcessActor(this)) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mozilla::dom::RecvLSClearPrivateBrowsing()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

BackgroundParentImpl::PBackgroundLocalStorageCacheParent*
BackgroundParentImpl::AllocPBackgroundLocalStorageCacheParent(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aOriginKey,
    const uint32_t& aPrivateBrowsingId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLocalStorageCacheParent(
      aPrincipalInfo, aOriginKey, aPrivateBrowsingId);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLocalStorageCacheConstructor(
    PBackgroundLocalStorageCacheParent* aActor,
    const PrincipalInfo& aPrincipalInfo, const nsACString& aOriginKey,
    const uint32_t& aPrivateBrowsingId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::RecvPBackgroundLocalStorageCacheConstructor(
      this, aActor, aPrincipalInfo, aOriginKey, aPrivateBrowsingId);
}

bool BackgroundParentImpl::DeallocPBackgroundLocalStorageCacheParent(
    PBackgroundLocalStorageCacheParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundLocalStorageCacheParent(aActor);
}

auto BackgroundParentImpl::AllocPBackgroundStorageParent(
    const nsAString& aProfilePath, const uint32_t& aPrivateBrowsingId)
    -> PBackgroundStorageParent* {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundStorageParent(aProfilePath,
                                                     aPrivateBrowsingId);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBackgroundStorageConstructor(
    PBackgroundStorageParent* aActor, const nsAString& aProfilePath,
    const uint32_t& aPrivateBrowsingId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::RecvPBackgroundStorageConstructor(aActor, aProfilePath,
                                                         aPrivateBrowsingId);
}

bool BackgroundParentImpl::DeallocPBackgroundStorageParent(
    PBackgroundStorageParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundStorageParent(aActor);
}

already_AddRefed<BackgroundParentImpl::PBackgroundSessionStorageManagerParent>
BackgroundParentImpl::AllocPBackgroundSessionStorageManagerParent(
    const uint64_t& aTopContextId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return dom::AllocPBackgroundSessionStorageManagerParent(aTopContextId);
}

already_AddRefed<mozilla::dom::PBackgroundSessionStorageServiceParent>
BackgroundParentImpl::AllocPBackgroundSessionStorageServiceParent() {
  AssertIsInMainOrSocketProcess();
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

already_AddRefed<PIdleSchedulerParent>
BackgroundParentImpl::AllocPIdleSchedulerParent() {
  AssertIsOnBackgroundThread();
  RefPtr<IdleSchedulerParent> actor = new IdleSchedulerParent();
  return actor.forget();
}

mozilla::dom::PRemoteWorkerParent*
BackgroundParentImpl::AllocPRemoteWorkerParent(const RemoteWorkerData& aData) {
  RefPtr<dom::RemoteWorkerParent> agent = new dom::RemoteWorkerParent();
  return agent.forget().take();
}

bool BackgroundParentImpl::DeallocPRemoteWorkerParent(
    mozilla::dom::PRemoteWorkerParent* aActor) {
  RefPtr<mozilla::dom::RemoteWorkerParent> actor =
      dont_AddRef(static_cast<mozilla::dom::RemoteWorkerParent*>(aActor));
  return true;
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

mozilla::dom::PRemoteWorkerServiceParent*
BackgroundParentImpl::AllocPRemoteWorkerServiceParent() {
  return new mozilla::dom::RemoteWorkerServiceParent();
}

IPCResult BackgroundParentImpl::RecvPRemoteWorkerServiceConstructor(
    PRemoteWorkerServiceParent* aActor) {
  mozilla::dom::RemoteWorkerServiceParent* actor =
      static_cast<mozilla::dom::RemoteWorkerServiceParent*>(aActor);

  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);
  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    actor->Initialize(NOT_REMOTE_TYPE);
  } else {
    actor->Initialize(parent->GetRemoteType());
    NS_ReleaseOnMainThread("ContentParent release", parent.forget());
  }
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPRemoteWorkerServiceParent(
    mozilla::dom::PRemoteWorkerServiceParent* aActor) {
  delete aActor;
  return true;
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

  // If the ContentParent is null we are dealing with a same-process actor.
  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);
  if (!parent) {
    isFileRemoteType = true;
  } else {
    isFileRemoteType = parent->GetRemoteType() == FILE_REMOTE_TYPE;
    NS_ReleaseOnMainThread("ContentParent release", parent.forget());
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<mozilla::dom::VsyncParent> actor = new mozilla::dom::VsyncParent();

  RefPtr<mozilla::VsyncDispatcher> vsyncDispatcher =
      gfxPlatform::GetPlatform()->GetGlobalVsyncDispatcher();
  actor->UpdateVsyncDispatcher(vsyncDispatcher);
  return actor.forget();
}

camera::PCamerasParent* BackgroundParentImpl::AllocPCamerasParent() {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  return static_cast<camera::CamerasParent*>(aActor)->RecvPCamerasConstructor();
}
#endif

bool BackgroundParentImpl::DeallocPCamerasParent(
    camera::PCamerasParent* aActor) {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
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

already_AddRefed<mozilla::psm::PVerifySSLServerCertParent>
BackgroundParentImpl::AllocPVerifySSLServerCertParent(
    const nsTArray<ByteArray>& aPeerCertChain, const nsACString& aHostName,
    const int32_t& aPort, const OriginAttributes& aOriginAttributes,
    const Maybe<ByteArray>& aStapledOCSPResponse,
    const Maybe<ByteArray>& aSctsFromTLSExtension,
    const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
    const uint32_t& aProviderFlags, const uint32_t& aCertVerifierFlags) {
  RefPtr<mozilla::psm::VerifySSLServerCertParent> parent =
      new mozilla::psm::VerifySSLServerCertParent();
  return parent.forget();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPVerifySSLServerCertConstructor(
    PVerifySSLServerCertParent* aActor, nsTArray<ByteArray>&& aPeerCertChain,
    const nsACString& aHostName, const int32_t& aPort,
    const OriginAttributes& aOriginAttributes,
    const Maybe<ByteArray>& aStapledOCSPResponse,
    const Maybe<ByteArray>& aSctsFromTLSExtension,
    const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
    const uint32_t& aProviderFlags, const uint32_t& aCertVerifierFlags) {
  mozilla::psm::VerifySSLServerCertParent* authCert =
      static_cast<mozilla::psm::VerifySSLServerCertParent*>(aActor);
  if (!authCert->Dispatch(std::move(aPeerCertChain), aHostName, aPort,
                          aOriginAttributes, aStapledOCSPResponse,
                          aSctsFromTLSExtension, aDcInfo, aProviderFlags,
                          aCertVerifierFlags)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

already_AddRefed<mozilla::psm::PSelectTLSClientAuthCertParent>
BackgroundParentImpl::AllocPSelectTLSClientAuthCertParent(
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const int32_t& aPort, const uint32_t& aProviderFlags,
    const uint32_t& aProviderTlsFlags, const ByteArray& aServerCertBytes,
    const nsTArray<ByteArray>& aCANames) {
  RefPtr<mozilla::psm::SelectTLSClientAuthCertParent> parent =
      new mozilla::psm::SelectTLSClientAuthCertParent();
  return parent.forget();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPSelectTLSClientAuthCertConstructor(
    PSelectTLSClientAuthCertParent* actor, const nsACString& aHostName,
    const OriginAttributes& aOriginAttributes, const int32_t& aPort,
    const uint32_t& aProviderFlags, const uint32_t& aProviderTlsFlags,
    const ByteArray& aServerCertBytes, nsTArray<ByteArray>&& aCANames) {
  mozilla::psm::SelectTLSClientAuthCertParent* selectTLSClientAuthCertParent =
      static_cast<mozilla::psm::SelectTLSClientAuthCertParent*>(actor);
  if (!selectTLSClientAuthCertParent->Dispatch(
          aHostName, aOriginAttributes, aPort, aProviderFlags,
          aProviderTlsFlags, aServerCertBytes, std::move(aCANames))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::dom::PBroadcastChannelParent*
BackgroundParentImpl::AllocPBroadcastChannelParent(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aOrigin,
    const nsAString& aChannel) {
  AssertIsInMainOrSocketProcess();
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

struct MOZ_STACK_CLASS NullifyContentParentRAII {
  explicit NullifyContentParentRAII(RefPtr<ContentParent>& aContentParent)
      : mContentParent(aContentParent) {}

  ~NullifyContentParentRAII() { mContentParent = nullptr; }

  RefPtr<ContentParent>& mContentParent;
};

class CheckPrincipalRunnable final : public Runnable {
 public:
  CheckPrincipalRunnable(already_AddRefed<ContentParent> aParent,
                         const PrincipalInfo& aPrincipalInfo,
                         const nsACString& aOrigin)
      : Runnable("ipc::CheckPrincipalRunnable"),
        mContentParent(aParent),
        mPrincipalInfo(aPrincipalInfo),
        mOrigin(aOrigin) {
    AssertIsInMainOrSocketProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mContentParent);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    NullifyContentParentRAII raii(mContentParent);

    auto principalOrErr = PrincipalInfoToPrincipal(mPrincipalInfo);
    if (NS_WARN_IF(principalOrErr.isErr())) {
      mContentParent->KillHard(
          "BroadcastChannel killed: PrincipalInfoToPrincipal failed.");
      return NS_OK;
    }

    nsAutoCString origin;
    nsresult rv = principalOrErr.unwrap()->GetOrigin(origin);
    if (NS_FAILED(rv)) {
      mContentParent->KillHard(
          "BroadcastChannel killed: principal::GetOrigin failed.");
      return NS_OK;
    }

    if (NS_WARN_IF(!mOrigin.Equals(origin))) {
      mContentParent->KillHard(
          "BroadcastChannel killed: origins do not match.");
      return NS_OK;
    }

    return NS_OK;
  }

 private:
  RefPtr<ContentParent> mContentParent;
  PrincipalInfo mPrincipalInfo;
  nsCString mOrigin;
};

}  // namespace

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBroadcastChannelConstructor(
    PBroadcastChannelParent* actor, const PrincipalInfo& aPrincipalInfo,
    const nsACString& aOrigin, const nsAString& aChannel) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    return IPC_OK();
  }

  RefPtr<CheckPrincipalRunnable> runnable =
      new CheckPrincipalRunnable(parent.forget(), aPrincipalInfo, aOrigin);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPBroadcastChannelParent(
    PBroadcastChannelParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<BroadcastChannelParent*>(aActor);
  return true;
}

mozilla::dom::PServiceWorkerManagerParent*
BackgroundParentImpl::AllocPServiceWorkerManagerParent() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<dom::ServiceWorkerManagerParent> agent =
      new dom::ServiceWorkerManagerParent();
  return agent.forget().take();
}

bool BackgroundParentImpl::DeallocPServiceWorkerManagerParent(
    PServiceWorkerManagerParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<dom::ServiceWorkerManagerParent> parent =
      dont_AddRef(static_cast<dom::ServiceWorkerManagerParent*>(aActor));
  MOZ_ASSERT(parent);
  return true;
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvShutdownServiceWorkerRegistrar() {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return new MessagePortParent(aUUID);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPMessagePortConstructor(
    PMessagePortParent* aActor, const nsID& aUUID, const nsID& aDestinationUUID,
    const uint32_t& aSequenceID) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  MessagePortParent* mp = static_cast<MessagePortParent*>(aActor);
  if (!mp->Entangle(aDestinationUUID, aSequenceID)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

already_AddRefed<psm::PIPCClientCertsParent>
BackgroundParentImpl::AllocPIPCClientCertsParent() {
  // This should only be called in the parent process with the socket process
  // as the child process, not any content processes, hence the check that the
  // child ID be 0.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mozilla::ipc::BackgroundParent::GetChildID(this) == 0);
  if (!XRE_IsParentProcess() ||
      mozilla::ipc::BackgroundParent::GetChildID(this) != 0) {
    return nullptr;
  }
  RefPtr<psm::IPCClientCertsParent> result = new psm::IPCClientCertsParent();
  return result.forget();
}

bool BackgroundParentImpl::DeallocPMessagePortParent(
    PMessagePortParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<MessagePortParent*>(aActor);
  return true;
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvMessagePortForceClose(
    const nsID& aUUID, const nsID& aDestinationUUID,
    const uint32_t& aSequenceID) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  if (!MessagePortParent::ForceClose(aUUID, aDestinationUUID, aSequenceID)) {
    return IPC_FAIL(this, "MessagePortParent::ForceClose failed.");
  }

  return IPC_OK();
}

BackgroundParentImpl::PQuotaParent* BackgroundParentImpl::AllocPQuotaParent() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::quota::AllocPQuotaParent();
}

bool BackgroundParentImpl::DeallocPQuotaParent(PQuotaParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::quota::DeallocPQuotaParent(aActor);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvShutdownQuotaManager() {
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
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
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<net::HttpBackgroundChannelParent> actor =
      new net::HttpBackgroundChannelParent();
  return actor.forget();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPHttpBackgroundChannelConstructor(
    net::PHttpBackgroundChannelParent* aActor, const uint64_t& aChannelId) {
  MOZ_ASSERT(aActor);
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  net::HttpBackgroundChannelParent* aParent =
      static_cast<net::HttpBackgroundChannelParent*>(aActor);

  if (NS_WARN_IF(NS_FAILED(aParent->Init(aChannelId)))) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

PMIDIPortParent* BackgroundParentImpl::AllocPMIDIPortParent(
    const MIDIPortInfo& aPortInfo, const bool& aSysexEnabled) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<MIDIPortParent> result = new MIDIPortParent(aPortInfo, aSysexEnabled);
  return result.forget().take();
}

bool BackgroundParentImpl::DeallocPMIDIPortParent(PMIDIPortParent* aActor) {
  MOZ_ASSERT(aActor);
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<MIDIPortParent> parent =
      dont_AddRef(static_cast<MIDIPortParent*>(aActor));
  parent->Teardown();
  return true;
}

PMIDIManagerParent* BackgroundParentImpl::AllocPMIDIManagerParent() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<MIDIManagerParent> result = new MIDIManagerParent();
  MIDIPlatformService::Get()->AddManager(result);
  return result.forget().take();
}

bool BackgroundParentImpl::DeallocPMIDIManagerParent(
    PMIDIManagerParent* aActor) {
  MOZ_ASSERT(aActor);
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<MIDIManagerParent> parent =
      dont_AddRef(static_cast<MIDIManagerParent*>(aActor));
  parent->Teardown();
  return true;
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
  RDDProcessManager* rdd = RDDProcessManager::Get();
  using Type =
      Tuple<const nsresult&, Endpoint<mozilla::PRemoteDecoderManagerChild>&&>;
  if (!rdd) {
    aResolver(
        Type(NS_ERROR_NOT_AVAILABLE, Endpoint<PRemoteDecoderManagerChild>()));
  } else {
    rdd->EnsureRDDProcessAndCreateBridge(OtherPid())
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
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvEnsureUtilityProcessAndCreateBridge(
    const RemoteDecodeIn& aLocation,
    EnsureUtilityProcessAndCreateBridgeResolver&& aResolver) {
  base::ProcessId otherPid = OtherPid();
  nsCOMPtr<nsISerialEventTarget> managerThread = GetCurrentSerialEventTarget();
  if (!managerThread) {
    return IPC_FAIL_NO_REASON(this);
  }
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "BackgroundParentImpl::RecvEnsureUtilityProcessAndCreateBridge()",
      [aResolver, managerThread, otherPid, aLocation]() {
        RefPtr<UtilityProcessManager> upm =
            UtilityProcessManager::GetSingleton();
        using Type = Tuple<const nsresult&,
                           Endpoint<mozilla::PRemoteDecoderManagerChild>&&>;
        if (!upm) {
          aResolver(Type(NS_ERROR_NOT_AVAILABLE,
                         Endpoint<PRemoteDecoderManagerChild>()));
        } else {
          SandboxingKind sbKind = GetSandboxingKindFromLocation(aLocation);
          upm->StartProcessForRemoteMediaDecoding(otherPid, sbKind)
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

dom::PMediaTransportParent* BackgroundParentImpl::AllocPMediaTransportParent() {
#ifdef MOZ_WEBRTC
  return new MediaTransportParent;
#else
  return nullptr;
#endif
}

bool BackgroundParentImpl::DeallocPMediaTransportParent(
    dom::PMediaTransportParent* aActor) {
#ifdef MOZ_WEBRTC
  delete aActor;
#endif
  return true;
}

already_AddRefed<dom::locks::PLockManagerParent>
BackgroundParentImpl::AllocPLockManagerParent(
    const ContentPrincipalInfo& aPrincipalInfo, const nsID& aClientId) {
  return MakeAndAddRef<mozilla::dom::locks::LockManagerParent>(aPrincipalInfo,
                                                               aClientId);
}

already_AddRefed<mozilla::net::PWebSocketConnectionParent>
BackgroundParentImpl::AllocPWebSocketConnectionParent(
    const uint32_t& aListenerId) {
  Maybe<nsCOMPtr<nsIHttpUpgradeListener>> listener =
      net::HttpConnectionMgrParent::GetAndRemoveHttpUpgradeListener(
          aListenerId);
  if (!listener) {
    return nullptr;
  }

  RefPtr<mozilla::net::WebSocketConnectionParent> actor =
      new mozilla::net::WebSocketConnectionParent(*listener);
  return actor.forget();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPWebSocketConnectionConstructor(
    PWebSocketConnectionParent* actor, const uint32_t& aListenerId) {
  return IPC_OK();
}

}  // namespace mozilla::ipc

void TestParent::ActorDestroy(ActorDestroyReason aWhy) {
  mozilla::ipc::AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
}
