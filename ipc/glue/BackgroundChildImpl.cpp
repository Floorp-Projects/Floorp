/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundChildImpl.h"

#include "ActorsChild.h"  // IndexedDB
#include "BroadcastChannelChild.h"
#include "FileDescriptorSetChild.h"
#ifdef MOZ_WEBRTC
#  include "CamerasChild.h"
#endif
#include "mozilla/media/MediaChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/dom/ClientManagerActors.h"
#include "mozilla/dom/FileCreatorChild.h"
#include "mozilla/dom/PBackgroundLSDatabaseChild.h"
#include "mozilla/dom/PBackgroundLSObserverChild.h"
#include "mozilla/dom/PBackgroundLSRequestChild.h"
#include "mozilla/dom/PBackgroundLSSimpleRequestChild.h"
#include "mozilla/dom/PBackgroundSDBConnectionChild.h"
#include "mozilla/dom/PFileSystemRequestChild.h"
#include "mozilla/dom/EndpointForReportChild.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/IPCBlobInputStreamChild.h"
#include "mozilla/dom/PMediaTransportChild.h"
#include "mozilla/dom/PendingIPCBlobChild.h"
#include "mozilla/dom/TemporaryIPCBlobChild.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIndexedDBUtilsChild.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/quota/PQuotaChild.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/RemoteWorkerServiceChild.h"
#include "mozilla/dom/SharedWorkerChild.h"
#include "mozilla/dom/StorageIPC.h"
#include "mozilla/dom/GamepadEventChannelChild.h"
#include "mozilla/dom/GamepadTestChannelChild.h"
#include "mozilla/dom/LocalStorage.h"
#include "mozilla/dom/MessagePortChild.h"
#include "mozilla/dom/ServiceWorkerActors.h"
#include "mozilla/dom/ServiceWorkerManagerChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/PBackgroundTestChild.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"
#include "mozilla/layout/VsyncChild.h"
#include "mozilla/net/HttpBackgroundChannelChild.h"
#include "mozilla/net/PUDPSocketChild.h"
#include "mozilla/dom/network/UDPSocketChild.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/MIDIPortChild.h"
#include "mozilla/dom/MIDIManagerChild.h"
#include "nsID.h"
#include "nsTraceRefcnt.h"

namespace {

class TestChild final : public mozilla::ipc::PBackgroundTestChild {
  friend class mozilla::ipc::BackgroundChildImpl;

  nsCString mTestArg;

  explicit TestChild(const nsCString& aTestArg) : mTestArg(aTestArg) {
    MOZ_COUNT_CTOR(TestChild);
  }

 protected:
  ~TestChild() override { MOZ_COUNT_DTOR(TestChild); }

 public:
  mozilla::ipc::IPCResult Recv__delete__(const nsCString& aTestArg) override;
};

}  // namespace

namespace mozilla {
namespace ipc {

using mozilla::dom::UDPSocketChild;
using mozilla::net::PUDPSocketChild;

using mozilla::dom::LocalStorage;
using mozilla::dom::PServiceWorkerChild;
using mozilla::dom::PServiceWorkerContainerChild;
using mozilla::dom::PServiceWorkerRegistrationChild;
using mozilla::dom::StorageDBChild;
using mozilla::dom::cache::PCacheChild;
using mozilla::dom::cache::PCacheStorageChild;
using mozilla::dom::cache::PCacheStreamControlChild;

using mozilla::dom::WebAuthnTransactionChild;

using mozilla::dom::PMIDIManagerChild;
using mozilla::dom::PMIDIPortChild;

// -----------------------------------------------------------------------------
// BackgroundChildImpl::ThreadLocal
// -----------------------------------------------------------------------------

BackgroundChildImpl::ThreadLocal::ThreadLocal() : mCurrentFileHandle(nullptr) {
  // May happen on any thread!
  MOZ_COUNT_CTOR(mozilla::ipc::BackgroundChildImpl::ThreadLocal);
}

BackgroundChildImpl::ThreadLocal::~ThreadLocal() {
  // May happen on any thread!
  MOZ_COUNT_DTOR(mozilla::ipc::BackgroundChildImpl::ThreadLocal);
}

// -----------------------------------------------------------------------------
// BackgroundChildImpl
// -----------------------------------------------------------------------------

BackgroundChildImpl::BackgroundChildImpl() {
  // May happen on any thread!
  MOZ_COUNT_CTOR(mozilla::ipc::BackgroundChildImpl);
}

BackgroundChildImpl::~BackgroundChildImpl() {
  // May happen on any thread!
  MOZ_COUNT_DTOR(mozilla::ipc::BackgroundChildImpl);
}

void BackgroundChildImpl::ProcessingError(Result aCode, const char* aReason) {
  // May happen on any thread!

  nsAutoCString abortMessage;

  switch (aCode) {
#define HANDLE_CASE(_result)              \
  case _result:                           \
    abortMessage.AssignLiteral(#_result); \
    break

    HANDLE_CASE(MsgDropped);
    HANDLE_CASE(MsgNotKnown);
    HANDLE_CASE(MsgNotAllowed);
    HANDLE_CASE(MsgPayloadError);
    HANDLE_CASE(MsgProcessingError);
    HANDLE_CASE(MsgRouteError);
    HANDLE_CASE(MsgValueError);

#undef HANDLE_CASE

    default:
      MOZ_CRASH("Unknown error code!");
  }

  MOZ_CRASH_UNSAFE_PRINTF("%s: %s", abortMessage.get(), aReason);
}

void BackgroundChildImpl::ActorDestroy(ActorDestroyReason aWhy) {
  // May happen on any thread!
}

PBackgroundTestChild* BackgroundChildImpl::AllocPBackgroundTestChild(
    const nsCString& aTestArg) {
  return new TestChild(aTestArg);
}

bool BackgroundChildImpl::DeallocPBackgroundTestChild(
    PBackgroundTestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<TestChild*>(aActor);
  return true;
}

BackgroundChildImpl::PBackgroundIDBFactoryChild*
BackgroundChildImpl::AllocPBackgroundIDBFactoryChild(
    const LoggingInfo& aLoggingInfo) {
  MOZ_CRASH(
      "PBackgroundIDBFactoryChild actors should be manually "
      "constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundIDBFactoryChild(
    PBackgroundIDBFactoryChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundIndexedDBUtilsChild*
BackgroundChildImpl::AllocPBackgroundIndexedDBUtilsChild() {
  MOZ_CRASH(
      "PBackgroundIndexedDBUtilsChild actors should be manually "
      "constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundIndexedDBUtilsChild(
    PBackgroundIndexedDBUtilsChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundSDBConnectionChild*
BackgroundChildImpl::AllocPBackgroundSDBConnectionChild(
    const PrincipalInfo& aPrincipalInfo) {
  MOZ_CRASH(
      "PBackgroundSDBConnectionChild actor should be manually "
      "constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundSDBConnectionChild(
    PBackgroundSDBConnectionChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundLSDatabaseChild*
BackgroundChildImpl::AllocPBackgroundLSDatabaseChild(
    const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
    const uint64_t& aDatastoreId) {
  MOZ_CRASH("PBackgroundLSDatabaseChild actor should be manually constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundLSDatabaseChild(
    PBackgroundLSDatabaseChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundLSObserverChild*
BackgroundChildImpl::AllocPBackgroundLSObserverChild(
    const uint64_t& aObserverId) {
  MOZ_CRASH("PBackgroundLSObserverChild actor should be manually constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundLSObserverChild(
    PBackgroundLSObserverChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundLSRequestChild*
BackgroundChildImpl::AllocPBackgroundLSRequestChild(
    const LSRequestParams& aParams) {
  MOZ_CRASH("PBackgroundLSRequestChild actor should be manually constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundLSRequestChild(
    PBackgroundLSRequestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundLocalStorageCacheChild*
BackgroundChildImpl::AllocPBackgroundLocalStorageCacheChild(
    const PrincipalInfo& aPrincipalInfo, const nsCString& aOriginKey,
    const uint32_t& aPrivateBrowsingId) {
  MOZ_CRASH(
      "PBackgroundLocalStorageChild actors should be manually "
      "constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundLocalStorageCacheChild(
    PBackgroundLocalStorageCacheChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundLSSimpleRequestChild*
BackgroundChildImpl::AllocPBackgroundLSSimpleRequestChild(
    const LSSimpleRequestParams& aParams) {
  MOZ_CRASH(
      "PBackgroundLSSimpleRequestChild actor should be manually "
      "constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundLSSimpleRequestChild(
    PBackgroundLSSimpleRequestChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundStorageChild*
BackgroundChildImpl::AllocPBackgroundStorageChild(
    const nsString& aProfilePath) {
  MOZ_CRASH("PBackgroundStorageChild actors should be manually constructed!");
}

bool BackgroundChildImpl::DeallocPBackgroundStorageChild(
    PBackgroundStorageChild* aActor) {
  MOZ_ASSERT(aActor);

  StorageDBChild* child = static_cast<StorageDBChild*>(aActor);
  child->ReleaseIPDLReference();
  return true;
}

dom::PPendingIPCBlobChild* BackgroundChildImpl::AllocPPendingIPCBlobChild(
    const IPCBlob& aBlob) {
  return new dom::PendingIPCBlobChild(aBlob);
}

bool BackgroundChildImpl::DeallocPPendingIPCBlobChild(
    dom::PPendingIPCBlobChild* aActor) {
  delete aActor;
  return true;
}

dom::PRemoteWorkerChild* BackgroundChildImpl::AllocPRemoteWorkerChild(
    const RemoteWorkerData& aData) {
  RefPtr<dom::RemoteWorkerChild> agent = new dom::RemoteWorkerChild();
  return agent.forget().take();
}

IPCResult BackgroundChildImpl::RecvPRemoteWorkerConstructor(
    PRemoteWorkerChild* aActor, const RemoteWorkerData& aData) {
  dom::RemoteWorkerChild* actor = static_cast<dom::RemoteWorkerChild*>(aActor);
  actor->ExecWorker(aData);
  return IPC_OK();
}

bool BackgroundChildImpl::DeallocPRemoteWorkerChild(
    dom::PRemoteWorkerChild* aActor) {
  RefPtr<dom::RemoteWorkerChild> actor =
      dont_AddRef(static_cast<dom::RemoteWorkerChild*>(aActor));
  return true;
}

dom::PRemoteWorkerServiceChild*
BackgroundChildImpl::AllocPRemoteWorkerServiceChild() {
  RefPtr<dom::RemoteWorkerServiceChild> agent =
      new dom::RemoteWorkerServiceChild();
  return agent.forget().take();
}

bool BackgroundChildImpl::DeallocPRemoteWorkerServiceChild(
    dom::PRemoteWorkerServiceChild* aActor) {
  RefPtr<dom::RemoteWorkerServiceChild> actor =
      dont_AddRef(static_cast<dom::RemoteWorkerServiceChild*>(aActor));
  return true;
}

dom::PSharedWorkerChild* BackgroundChildImpl::AllocPSharedWorkerChild(
    const dom::RemoteWorkerData& aData, const uint64_t& aWindowID,
    const dom::MessagePortIdentifier& aPortIdentifier) {
  RefPtr<dom::SharedWorkerChild> agent = new dom::SharedWorkerChild();
  return agent.forget().take();
}

bool BackgroundChildImpl::DeallocPSharedWorkerChild(
    dom::PSharedWorkerChild* aActor) {
  RefPtr<dom::SharedWorkerChild> actor =
      dont_AddRef(static_cast<dom::SharedWorkerChild*>(aActor));
  return true;
}

dom::PTemporaryIPCBlobChild*
BackgroundChildImpl::AllocPTemporaryIPCBlobChild() {
  MOZ_CRASH("This is not supposed to be called.");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPTemporaryIPCBlobChild(
    dom::PTemporaryIPCBlobChild* aActor) {
  RefPtr<dom::TemporaryIPCBlobChild> actor =
      dont_AddRef(static_cast<dom::TemporaryIPCBlobChild*>(aActor));
  return true;
}

dom::PFileCreatorChild* BackgroundChildImpl::AllocPFileCreatorChild(
    const nsString& aFullPath, const nsString& aType, const nsString& aName,
    const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
    const bool& aIsFromNsIFile) {
  return new dom::FileCreatorChild();
}

bool BackgroundChildImpl::DeallocPFileCreatorChild(PFileCreatorChild* aActor) {
  delete static_cast<dom::FileCreatorChild*>(aActor);
  return true;
}

already_AddRefed<dom::PIPCBlobInputStreamChild>
BackgroundChildImpl::AllocPIPCBlobInputStreamChild(const nsID& aID,
                                                   const uint64_t& aSize) {
  RefPtr<dom::IPCBlobInputStreamChild> actor =
      new dom::IPCBlobInputStreamChild(aID, aSize);
  return actor.forget();
}

PFileDescriptorSetChild* BackgroundChildImpl::AllocPFileDescriptorSetChild(
    const FileDescriptor& aFileDescriptor) {
  return new FileDescriptorSetChild(aFileDescriptor);
}

bool BackgroundChildImpl::DeallocPFileDescriptorSetChild(
    PFileDescriptorSetChild* aActor) {
  MOZ_ASSERT(aActor);

  delete static_cast<FileDescriptorSetChild*>(aActor);
  return true;
}

BackgroundChildImpl::PVsyncChild* BackgroundChildImpl::AllocPVsyncChild() {
  RefPtr<mozilla::layout::VsyncChild> actor = new mozilla::layout::VsyncChild();
  // There still has one ref-count after return, and it will be released in
  // DeallocPVsyncChild().
  return actor.forget().take();
}

bool BackgroundChildImpl::DeallocPVsyncChild(PVsyncChild* aActor) {
  MOZ_ASSERT(aActor);

  // This actor already has one ref-count. Please check AllocPVsyncChild().
  RefPtr<mozilla::layout::VsyncChild> actor =
      dont_AddRef(static_cast<mozilla::layout::VsyncChild*>(aActor));
  return true;
}

PUDPSocketChild* BackgroundChildImpl::AllocPUDPSocketChild(
    const Maybe<PrincipalInfo>& aPrincipalInfo, const nsCString& aFilter) {
  MOZ_CRASH("AllocPUDPSocket should not be called");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPUDPSocketChild(PUDPSocketChild* child) {
  UDPSocketChild* p = static_cast<UDPSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

// -----------------------------------------------------------------------------
// BroadcastChannel API
// -----------------------------------------------------------------------------

dom::PBroadcastChannelChild* BackgroundChildImpl::AllocPBroadcastChannelChild(
    const PrincipalInfo& aPrincipalInfo, const nsCString& aOrigin,
    const nsString& aChannel) {
  RefPtr<dom::BroadcastChannelChild> agent =
      new dom::BroadcastChannelChild(aOrigin);
  return agent.forget().take();
}

bool BackgroundChildImpl::DeallocPBroadcastChannelChild(
    PBroadcastChannelChild* aActor) {
  RefPtr<dom::BroadcastChannelChild> child =
      dont_AddRef(static_cast<dom::BroadcastChannelChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

camera::PCamerasChild* BackgroundChildImpl::AllocPCamerasChild() {
#ifdef MOZ_WEBRTC
  RefPtr<camera::CamerasChild> agent = new camera::CamerasChild();
  return agent.forget().take();
#else
  return nullptr;
#endif
}

bool BackgroundChildImpl::DeallocPCamerasChild(camera::PCamerasChild* aActor) {
#ifdef MOZ_WEBRTC
  RefPtr<camera::CamerasChild> child =
      dont_AddRef(static_cast<camera::CamerasChild*>(aActor));
  MOZ_ASSERT(aActor);
  camera::Shutdown();
#endif
  return true;
}

// -----------------------------------------------------------------------------
// ServiceWorkerManager
// -----------------------------------------------------------------------------

dom::PServiceWorkerManagerChild*
BackgroundChildImpl::AllocPServiceWorkerManagerChild() {
  RefPtr<dom::ServiceWorkerManagerChild> agent =
      new dom::ServiceWorkerManagerChild();
  return agent.forget().take();
}

bool BackgroundChildImpl::DeallocPServiceWorkerManagerChild(
    PServiceWorkerManagerChild* aActor) {
  RefPtr<dom::ServiceWorkerManagerChild> child =
      dont_AddRef(static_cast<dom::ServiceWorkerManagerChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

// -----------------------------------------------------------------------------
// Cache API
// -----------------------------------------------------------------------------

PCacheStorageChild* BackgroundChildImpl::AllocPCacheStorageChild(
    const Namespace& aNamespace, const PrincipalInfo& aPrincipalInfo) {
  MOZ_CRASH("CacheStorageChild actor must be provided to PBackground manager");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPCacheStorageChild(
    PCacheStorageChild* aActor) {
  dom::cache::DeallocPCacheStorageChild(aActor);
  return true;
}

PCacheChild* BackgroundChildImpl::AllocPCacheChild() {
  return dom::cache::AllocPCacheChild();
}

bool BackgroundChildImpl::DeallocPCacheChild(PCacheChild* aActor) {
  dom::cache::DeallocPCacheChild(aActor);
  return true;
}

PCacheStreamControlChild* BackgroundChildImpl::AllocPCacheStreamControlChild() {
  return dom::cache::AllocPCacheStreamControlChild();
}

bool BackgroundChildImpl::DeallocPCacheStreamControlChild(
    PCacheStreamControlChild* aActor) {
  dom::cache::DeallocPCacheStreamControlChild(aActor);
  return true;
}

// -----------------------------------------------------------------------------
// MessageChannel/MessagePort API
// -----------------------------------------------------------------------------

dom::PMessagePortChild* BackgroundChildImpl::AllocPMessagePortChild(
    const nsID& aUUID, const nsID& aDestinationUUID,
    const uint32_t& aSequenceID) {
  RefPtr<dom::MessagePortChild> agent = new dom::MessagePortChild();
  return agent.forget().take();
}

bool BackgroundChildImpl::DeallocPMessagePortChild(PMessagePortChild* aActor) {
  RefPtr<dom::MessagePortChild> child =
      dont_AddRef(static_cast<dom::MessagePortChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

PChildToParentStreamChild*
BackgroundChildImpl::AllocPChildToParentStreamChild() {
  MOZ_CRASH("PChildToParentStreamChild actors should be manually constructed!");
}

bool BackgroundChildImpl::DeallocPChildToParentStreamChild(
    PChildToParentStreamChild* aActor) {
  delete aActor;
  return true;
}

PParentToChildStreamChild*
BackgroundChildImpl::AllocPParentToChildStreamChild() {
  return mozilla::ipc::AllocPParentToChildStreamChild();
}

bool BackgroundChildImpl::DeallocPParentToChildStreamChild(
    PParentToChildStreamChild* aActor) {
  delete aActor;
  return true;
}

BackgroundChildImpl::PQuotaChild* BackgroundChildImpl::AllocPQuotaChild() {
  MOZ_CRASH("PQuotaChild actor should be manually constructed!");
}

bool BackgroundChildImpl::DeallocPQuotaChild(PQuotaChild* aActor) {
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

// -----------------------------------------------------------------------------
// WebMIDI API
// -----------------------------------------------------------------------------

PMIDIPortChild* BackgroundChildImpl::AllocPMIDIPortChild(
    const MIDIPortInfo& aPortInfo, const bool& aSysexEnabled) {
  MOZ_CRASH("Should be created manually");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPMIDIPortChild(PMIDIPortChild* aActor) {
  MOZ_ASSERT(aActor);
  // The reference is increased in dom/midi/MIDIPort.cpp. We should
  // decrease it after IPC.
  RefPtr<dom::MIDIPortChild> child =
      dont_AddRef(static_cast<dom::MIDIPortChild*>(aActor));
  child->Teardown();
  return true;
}

PMIDIManagerChild* BackgroundChildImpl::AllocPMIDIManagerChild() {
  MOZ_CRASH("Should be created manually");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPMIDIManagerChild(PMIDIManagerChild* aActor) {
  MOZ_ASSERT(aActor);
  // The reference is increased in dom/midi/MIDIAccessManager.cpp. We should
  // decrease it after IPC.
  RefPtr<dom::MIDIManagerChild> child =
      dont_AddRef(static_cast<dom::MIDIManagerChild*>(aActor));
  return true;
}

// Gamepad API Background IPC
dom::PGamepadEventChannelChild*
BackgroundChildImpl::AllocPGamepadEventChannelChild() {
  MOZ_CRASH("PGamepadEventChannelChild actor should be manually constructed!");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPGamepadEventChannelChild(
    PGamepadEventChannelChild* aActor) {
  MOZ_ASSERT(aActor);
  delete static_cast<dom::GamepadEventChannelChild*>(aActor);
  return true;
}

dom::PGamepadTestChannelChild*
BackgroundChildImpl::AllocPGamepadTestChannelChild() {
  MOZ_CRASH("PGamepadTestChannelChild actor should be manually constructed!");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPGamepadTestChannelChild(
    PGamepadTestChannelChild* aActor) {
  MOZ_ASSERT(aActor);
  delete static_cast<dom::GamepadTestChannelChild*>(aActor);
  return true;
}

mozilla::dom::PClientManagerChild*
BackgroundChildImpl::AllocPClientManagerChild() {
  return mozilla::dom::AllocClientManagerChild();
}

bool BackgroundChildImpl::DeallocPClientManagerChild(
    mozilla::dom::PClientManagerChild* aActor) {
  return mozilla::dom::DeallocClientManagerChild(aActor);
}

#ifdef EARLY_BETA_OR_EARLIER
void BackgroundChildImpl::OnChannelReceivedMessage(const Message& aMsg) {
  if (aMsg.type() == layout::PVsync::MessageType::Msg_Notify__ID) {
    // Not really necessary to look at the message payload, it will be
    // <0.5ms away from TimeStamp::Now()
    SchedulerGroup::MarkVsyncReceived();
  }
}
#endif

dom::PWebAuthnTransactionChild*
BackgroundChildImpl::AllocPWebAuthnTransactionChild() {
  MOZ_CRASH("PWebAuthnTransaction actor should be manually constructed!");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPWebAuthnTransactionChild(
    PWebAuthnTransactionChild* aActor) {
  MOZ_ASSERT(aActor);
  RefPtr<dom::WebAuthnTransactionChild> child =
      dont_AddRef(static_cast<dom::WebAuthnTransactionChild*>(aActor));
  return true;
}

net::PHttpBackgroundChannelChild*
BackgroundChildImpl::AllocPHttpBackgroundChannelChild(
    const uint64_t& aChannelId) {
  MOZ_CRASH(
      "PHttpBackgroundChannelChild actor should be manually constructed!");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPHttpBackgroundChannelChild(
    PHttpBackgroundChannelChild* aActor) {
  // The reference is increased in BackgroundChannelCreateCallback::ActorCreated
  // of HttpBackgroundChannelChild.cpp. We should decrease it after IPC
  // destroyed.
  RefPtr<net::HttpBackgroundChannelChild> child =
      dont_AddRef(static_cast<net::HttpBackgroundChannelChild*>(aActor));
  return true;
}

PServiceWorkerChild* BackgroundChildImpl::AllocPServiceWorkerChild(
    const IPCServiceWorkerDescriptor&) {
  return dom::AllocServiceWorkerChild();
}

bool BackgroundChildImpl::DeallocPServiceWorkerChild(
    PServiceWorkerChild* aActor) {
  return dom::DeallocServiceWorkerChild(aActor);
}

PServiceWorkerContainerChild*
BackgroundChildImpl::AllocPServiceWorkerContainerChild() {
  return dom::AllocServiceWorkerContainerChild();
}

bool BackgroundChildImpl::DeallocPServiceWorkerContainerChild(
    PServiceWorkerContainerChild* aActor) {
  return dom::DeallocServiceWorkerContainerChild(aActor);
}

PServiceWorkerRegistrationChild*
BackgroundChildImpl::AllocPServiceWorkerRegistrationChild(
    const IPCServiceWorkerRegistrationDescriptor&) {
  return dom::AllocServiceWorkerRegistrationChild();
}

bool BackgroundChildImpl::DeallocPServiceWorkerRegistrationChild(
    PServiceWorkerRegistrationChild* aActor) {
  return dom::DeallocServiceWorkerRegistrationChild(aActor);
}

dom::PEndpointForReportChild* BackgroundChildImpl::AllocPEndpointForReportChild(
    const nsString& aGroupName, const PrincipalInfo& aPrincipalInfo) {
  return new dom::EndpointForReportChild();
}

bool BackgroundChildImpl::DeallocPEndpointForReportChild(
    PEndpointForReportChild* aActor) {
  MOZ_ASSERT(aActor);
  delete static_cast<dom::EndpointForReportChild*>(aActor);
  return true;
}

dom::PMediaTransportChild* BackgroundChildImpl::AllocPMediaTransportChild() {
  // We don't allocate here: MediaTransportHandlerIPC is in charge of that,
  // so we don't need to know the implementation particulars here.
  MOZ_ASSERT_UNREACHABLE(
      "The only thing that ought to be creating a PMediaTransportChild is "
      "MediaTransportHandlerIPC!");
  return nullptr;
}

bool BackgroundChildImpl::DeallocPMediaTransportChild(
    dom::PMediaTransportChild* aActor) {
  delete aActor;
  return true;
}

}  // namespace ipc
}  // namespace mozilla

mozilla::ipc::IPCResult TestChild::Recv__delete__(const nsCString& aTestArg) {
  MOZ_RELEASE_ASSERT(aTestArg == mTestArg,
                     "BackgroundTest message was corrupted!");

  return IPC_OK();
}
