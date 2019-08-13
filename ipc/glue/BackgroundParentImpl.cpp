/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundParentImpl.h"

#include "BroadcastChannelParent.h"
#include "FileDescriptorSetParent.h"
#ifdef MOZ_WEBRTC
#  include "CamerasParent.h"
#endif
#include "mozilla/media/MediaParent.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/ClientManagerActors.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/EndpointForReportParent.h"
#include "mozilla/dom/FileCreatorParent.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/GamepadEventChannelParent.h"
#include "mozilla/dom/GamepadTestChannelParent.h"
#include "mozilla/dom/PGamepadEventChannelParent.h"
#include "mozilla/dom/PGamepadTestChannelParent.h"
#include "mozilla/dom/MediaTransportParent.h"
#include "mozilla/dom/MessagePortParent.h"
#include "mozilla/dom/PendingIPCBlobParent.h"
#include "mozilla/dom/ServiceWorkerActors.h"
#include "mozilla/dom/ServiceWorkerManagerParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/StorageActivityService.h"
#include "mozilla/dom/TemporaryIPCBlobParent.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/IPCBlobInputStreamParent.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/localstorage/ActorsParent.h"
#include "mozilla/dom/quota/ActorsParent.h"
#include "mozilla/dom/simpledb/ActorsParent.h"
#include "mozilla/dom/RemoteWorkerParent.h"
#include "mozilla/dom/RemoteWorkerServiceParent.h"
#include "mozilla/dom/ReportingHeader.h"
#include "mozilla/dom/SharedWorkerParent.h"
#include "mozilla/dom/StorageIPC.h"
#include "mozilla/dom/MIDIManagerParent.h"
#include "mozilla/dom/MIDIPortParent.h"
#include "mozilla/dom/MIDIPlatformService.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/PBackgroundTestParent.h"
#include "mozilla/ipc/PChildToParentStreamParent.h"
#include "mozilla/ipc/PParentToChildStreamParent.h"
#include "mozilla/layout/VsyncParent.h"
#include "mozilla/net/HttpBackgroundChannelParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"
#include "nsProxyRelease.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "nsXULAppAPI.h"

#ifdef DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false)
#endif

using mozilla::AssertIsOnMainThread;
using mozilla::dom::FileSystemBase;
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
using mozilla::dom::UDPSocketParent;
using mozilla::dom::WebAuthnTransactionParent;
using mozilla::dom::cache::PCacheParent;
using mozilla::dom::cache::PCacheStorageParent;
using mozilla::dom::cache::PCacheStreamControlParent;
using mozilla::ipc::AssertIsOnBackgroundThread;

namespace {

class TestParent final : public mozilla::ipc::PBackgroundTestParent {
  friend class mozilla::ipc::BackgroundParentImpl;

  TestParent() { MOZ_COUNT_CTOR(TestParent); }

 protected:
  ~TestParent() override { MOZ_COUNT_DTOR(TestParent); }

 public:
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace

namespace mozilla {
namespace ipc {

using mozilla::dom::BroadcastChannelParent;
using mozilla::dom::ContentParent;
using mozilla::dom::ServiceWorkerRegistrationData;

BackgroundParentImpl::BackgroundParentImpl() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();

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

BackgroundParentImpl::PBackgroundTestParent*
BackgroundParentImpl::AllocPBackgroundTestParent(const nsCString& aTestArg) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return new TestParent();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBackgroundTestConstructor(
    PBackgroundTestParent* aActor, const nsCString& aTestArg) {
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
    const LoggingInfo& aLoggingInfo) -> PBackgroundIDBFactoryParent* {
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

bool BackgroundParentImpl::DeallocPBackgroundIDBFactoryParent(
    PBackgroundIDBFactoryParent* aActor) {
  using mozilla::dom::indexedDB::DeallocPBackgroundIDBFactoryParent;

  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocPBackgroundIDBFactoryParent(aActor);
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
    const PrincipalInfo& aPrincipalInfo) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundSDBConnectionParent(aPrincipalInfo);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundSDBConnectionConstructor(
    PBackgroundSDBConnectionParent* aActor,
    const PrincipalInfo& aPrincipalInfo) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPBackgroundSDBConnectionConstructor(aActor,
                                                             aPrincipalInfo)) {
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
    const PrincipalInfo& aPrincipalInfo, const nsCString& aOriginKey,
    const uint32_t& aPrivateBrowsingId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundLocalStorageCacheParent(
      aPrincipalInfo, aOriginKey, aPrivateBrowsingId);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPBackgroundLocalStorageCacheConstructor(
    PBackgroundLocalStorageCacheParent* aActor,
    const PrincipalInfo& aPrincipalInfo, const nsCString& aOriginKey,
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
    const nsString& aProfilePath) -> PBackgroundStorageParent* {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return mozilla::dom::AllocPBackgroundStorageParent(aProfilePath);
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPBackgroundStorageConstructor(
    PBackgroundStorageParent* aActor, const nsString& aProfilePath) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::RecvPBackgroundStorageConstructor(aActor, aProfilePath);
}

bool BackgroundParentImpl::DeallocPBackgroundStorageParent(
    PBackgroundStorageParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPBackgroundStorageParent(aActor);
}

mozilla::dom::PPendingIPCBlobParent*
BackgroundParentImpl::AllocPPendingIPCBlobParent(const IPCBlob& aBlob) {
  MOZ_CRASH("PPendingIPCBlobParent actors should be manually constructed!");
}

bool BackgroundParentImpl::DeallocPPendingIPCBlobParent(
    mozilla::dom::PPendingIPCBlobParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
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

mozilla::dom::PRemoteWorkerServiceParent*
BackgroundParentImpl::AllocPRemoteWorkerServiceParent() {
  return new mozilla::dom::RemoteWorkerServiceParent();
}

IPCResult BackgroundParentImpl::RecvPRemoteWorkerServiceConstructor(
    PRemoteWorkerServiceParent* aActor) {
  mozilla::dom::RemoteWorkerServiceParent* actor =
      static_cast<mozilla::dom::RemoteWorkerServiceParent*>(aActor);
  actor->Initialize();
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
    const nsString& aFullPath, const nsString& aType, const nsString& aName,
    const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
    const bool& aIsFromNsIFile) {
  RefPtr<dom::FileCreatorParent> actor = new dom::FileCreatorParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPFileCreatorConstructor(
    dom::PFileCreatorParent* aActor, const nsString& aFullPath,
    const nsString& aType, const nsString& aName,
    const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
    const bool& aIsFromNsIFile) {
  bool isFileRemoteType = false;

  // If the ContentParent is null we are dealing with a same-process actor.
  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(this);
  if (!parent) {
    isFileRemoteType = true;
  } else {
    isFileRemoteType = parent->GetRemoteType().EqualsLiteral(FILE_REMOTE_TYPE);
    NS_ReleaseOnMainThreadSystemGroup("ContentParent release", parent.forget());
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

already_AddRefed<dom::PIPCBlobInputStreamParent>
BackgroundParentImpl::AllocPIPCBlobInputStreamParent(const nsID& aID,
                                                     const uint64_t& aSize) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<dom::IPCBlobInputStreamParent> actor =
      dom::IPCBlobInputStreamParent::Create(aID, aSize, this);
  return actor.forget();
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPIPCBlobInputStreamConstructor(
    dom::PIPCBlobInputStreamParent* aActor, const nsID& aID,
    const uint64_t& aSize) {
  if (!static_cast<dom::IPCBlobInputStreamParent*>(aActor)->HasValidStream()) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

PFileDescriptorSetParent* BackgroundParentImpl::AllocPFileDescriptorSetParent(
    const FileDescriptor& aFileDescriptor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  return new FileDescriptorSetParent(aFileDescriptor);
}

bool BackgroundParentImpl::DeallocPFileDescriptorSetParent(
    PFileDescriptorSetParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete static_cast<FileDescriptorSetParent*>(aActor);
  return true;
}

PChildToParentStreamParent*
BackgroundParentImpl::AllocPChildToParentStreamParent() {
  return mozilla::ipc::AllocPChildToParentStreamParent();
}

bool BackgroundParentImpl::DeallocPChildToParentStreamParent(
    PChildToParentStreamParent* aActor) {
  delete aActor;
  return true;
}

PParentToChildStreamParent*
BackgroundParentImpl::AllocPParentToChildStreamParent() {
  MOZ_CRASH(
      "PParentToChildStreamParent actors should be manually constructed!");
}

bool BackgroundParentImpl::DeallocPParentToChildStreamParent(
    PParentToChildStreamParent* aActor) {
  delete aActor;
  return true;
}

BackgroundParentImpl::PVsyncParent* BackgroundParentImpl::AllocPVsyncParent() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<mozilla::layout::VsyncParent> actor =
      mozilla::layout::VsyncParent::Create();
  // There still has one ref-count after return, and it will be released in
  // DeallocPVsyncParent().
  return actor.forget().take();
}

bool BackgroundParentImpl::DeallocPVsyncParent(PVsyncParent* aActor) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // This actor already has one ref-count. Please check AllocPVsyncParent().
  RefPtr<mozilla::layout::VsyncParent> actor =
      dont_AddRef(static_cast<mozilla::layout::VsyncParent*>(aActor));
  return true;
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
    const Maybe<PrincipalInfo>& /* unused */, const nsCString & /* unused */)
    -> PUDPSocketParent* {
  RefPtr<UDPSocketParent> p = new UDPSocketParent(this);

  return p.forget().take();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPUDPSocketConstructor(
    PUDPSocketParent* aActor, const Maybe<PrincipalInfo>& aOptionalPrincipal,
    const nsCString& aFilter) {
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

  IPC::Principal principal;
  if (!static_cast<UDPSocketParent*>(aActor)->Init(principal, aFilter)) {
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
    const PrincipalInfo& aPrincipalInfo, const nsCString& aOrigin,
    const nsString& aChannel) {
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
                         const nsCString& aOrigin)
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

    nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(mPrincipalInfo);

    nsAutoCString origin;
    nsresult rv = principal->GetOrigin(origin);
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
    const nsCString& aOrigin, const nsString& aChannel) {
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

PCacheStorageParent* BackgroundParentImpl::AllocPCacheStorageParent(
    const Namespace& aNamespace, const PrincipalInfo& aPrincipalInfo) {
  return dom::cache::AllocPCacheStorageParent(this, aNamespace, aPrincipalInfo);
}

bool BackgroundParentImpl::DeallocPCacheStorageParent(
    PCacheStorageParent* aActor) {
  dom::cache::DeallocPCacheStorageParent(aActor);
  return true;
}

PCacheParent* BackgroundParentImpl::AllocPCacheParent() {
  MOZ_CRASH("CacheParent actor must be provided to PBackground manager");
  return nullptr;
}

bool BackgroundParentImpl::DeallocPCacheParent(PCacheParent* aActor) {
  dom::cache::DeallocPCacheParent(aActor);
  return true;
}

PCacheStreamControlParent*
BackgroundParentImpl::AllocPCacheStreamControlParent() {
  MOZ_CRASH(
      "CacheStreamControlParent actor must be provided to PBackground manager");
  return nullptr;
}

bool BackgroundParentImpl::DeallocPCacheStreamControlParent(
    PCacheStreamControlParent* aActor) {
  dom::cache::DeallocPCacheStreamControlParent(aActor);
  return true;
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
    return IPC_FAIL_NO_REASON(this);
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
dom::PGamepadEventChannelParent*
BackgroundParentImpl::AllocPGamepadEventChannelParent() {
  RefPtr<dom::GamepadEventChannelParent> parent =
      new dom::GamepadEventChannelParent();

  return parent.forget().take();
}

bool BackgroundParentImpl::DeallocPGamepadEventChannelParent(
    dom::PGamepadEventChannelParent* aActor) {
  MOZ_ASSERT(aActor);
  RefPtr<dom::GamepadEventChannelParent> parent =
      dont_AddRef(static_cast<dom::GamepadEventChannelParent*>(aActor));
  return true;
}

dom::PGamepadTestChannelParent*
BackgroundParentImpl::AllocPGamepadTestChannelParent() {
  RefPtr<dom::GamepadTestChannelParent> parent =
      new dom::GamepadTestChannelParent();

  return parent.forget().take();
}

bool BackgroundParentImpl::DeallocPGamepadTestChannelParent(
    dom::PGamepadTestChannelParent* aActor) {
  MOZ_ASSERT(aActor);
  RefPtr<dom::GamepadTestChannelParent> parent =
      dont_AddRef(static_cast<dom::GamepadTestChannelParent*>(aActor));
  return true;
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

net::PHttpBackgroundChannelParent*
BackgroundParentImpl::AllocPHttpBackgroundChannelParent(
    const uint64_t& aChannelId) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  RefPtr<net::HttpBackgroundChannelParent> actor =
      new net::HttpBackgroundChannelParent();

  // hold extra refcount for IPDL
  return actor.forget().take();
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

bool BackgroundParentImpl::DeallocPHttpBackgroundChannelParent(
    net::PHttpBackgroundChannelParent* aActor) {
  MOZ_ASSERT(aActor);
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  // release extra refcount hold by AllocPHttpBackgroundChannelParent
  RefPtr<net::HttpBackgroundChannelParent> actor =
      dont_AddRef(static_cast<net::HttpBackgroundChannelParent*>(aActor));

  return true;
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

PServiceWorkerParent* BackgroundParentImpl::AllocPServiceWorkerParent(
    const IPCServiceWorkerDescriptor&) {
  return dom::AllocServiceWorkerParent();
}

bool BackgroundParentImpl::DeallocPServiceWorkerParent(
    PServiceWorkerParent* aActor) {
  return dom::DeallocServiceWorkerParent(aActor);
}

IPCResult BackgroundParentImpl::RecvPServiceWorkerConstructor(
    PServiceWorkerParent* aActor,
    const IPCServiceWorkerDescriptor& aDescriptor) {
  dom::InitServiceWorkerParent(aActor, aDescriptor);
  return IPC_OK();
}

PServiceWorkerContainerParent*
BackgroundParentImpl::AllocPServiceWorkerContainerParent() {
  return dom::AllocServiceWorkerContainerParent();
}

bool BackgroundParentImpl::DeallocPServiceWorkerContainerParent(
    PServiceWorkerContainerParent* aActor) {
  return dom::DeallocServiceWorkerContainerParent(aActor);
}

mozilla::ipc::IPCResult
BackgroundParentImpl::RecvPServiceWorkerContainerConstructor(
    PServiceWorkerContainerParent* aActor) {
  dom::InitServiceWorkerContainerParent(aActor);
  return IPC_OK();
}

PServiceWorkerRegistrationParent*
BackgroundParentImpl::AllocPServiceWorkerRegistrationParent(
    const IPCServiceWorkerRegistrationDescriptor&) {
  return dom::AllocServiceWorkerRegistrationParent();
}

bool BackgroundParentImpl::DeallocPServiceWorkerRegistrationParent(
    PServiceWorkerRegistrationParent* aActor) {
  return dom::DeallocServiceWorkerRegistrationParent(aActor);
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
    const nsString& aGroupName, const PrincipalInfo& aPrincipalInfo) {
  RefPtr<dom::EndpointForReportParent> actor =
      new dom::EndpointForReportParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvPEndpointForReportConstructor(
    PEndpointForReportParent* aActor, const nsString& aGroupName,
    const PrincipalInfo& aPrincipalInfo) {
  static_cast<dom::EndpointForReportParent*>(aActor)->Run(aGroupName,
                                                          aPrincipalInfo);
  return IPC_OK();
}

bool BackgroundParentImpl::DeallocPEndpointForReportParent(
    PEndpointForReportParent* aActor) {
  RefPtr<dom::EndpointForReportParent> actor =
      dont_AddRef(static_cast<dom::EndpointForReportParent*>(aActor));
  return true;
}

mozilla::ipc::IPCResult BackgroundParentImpl::RecvRemoveEndpoint(
    const nsString& aGroupName, const nsCString& aEndpointURL,
    const PrincipalInfo& aPrincipalInfo) {
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("BackgroundParentImpl::RecvRemoveEndpoint(",
                             [aGroupName, aEndpointURL, aPrincipalInfo]() {
                               dom::ReportingHeader::RemoveEndpoint(
                                   aGroupName, aEndpointURL, aPrincipalInfo);
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

}  // namespace ipc
}  // namespace mozilla

void TestParent::ActorDestroy(ActorDestroyReason aWhy) {
  mozilla::ipc::AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
}
