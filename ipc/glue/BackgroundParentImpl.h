/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundparentimpl_h__
#define mozilla_ipc_backgroundparentimpl_h__

#include "mozilla/Attributes.h"
#include "mozilla/ipc/PBackgroundParent.h"

namespace mozilla {

namespace layout {
class VsyncParent;
}  // namespace layout

namespace ipc {

// Instances of this class should never be created directly. This class is meant
// to be inherited in BackgroundImpl.
class BackgroundParentImpl : public PBackgroundParent {
 protected:
  BackgroundParentImpl();
  virtual ~BackgroundParentImpl();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PBackgroundTestParent* AllocPBackgroundTestParent(
      const nsCString& aTestArg) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundTestConstructor(
      PBackgroundTestParent* aActor, const nsCString& aTestArg) override;

  virtual bool DeallocPBackgroundTestParent(
      PBackgroundTestParent* aActor) override;

  virtual PBackgroundIDBFactoryParent* AllocPBackgroundIDBFactoryParent(
      const LoggingInfo& aLoggingInfo) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundIDBFactoryConstructor(
      PBackgroundIDBFactoryParent* aActor,
      const LoggingInfo& aLoggingInfo) override;

  virtual bool DeallocPBackgroundIDBFactoryParent(
      PBackgroundIDBFactoryParent* aActor) override;

  virtual PBackgroundIndexedDBUtilsParent*
  AllocPBackgroundIndexedDBUtilsParent() override;

  virtual bool DeallocPBackgroundIndexedDBUtilsParent(
      PBackgroundIndexedDBUtilsParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvFlushPendingFileDeletions() override;

  virtual PBackgroundSDBConnectionParent* AllocPBackgroundSDBConnectionParent(
      const PrincipalInfo& aPrincipalInfo) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundSDBConnectionConstructor(
      PBackgroundSDBConnectionParent* aActor,
      const PrincipalInfo& aPrincipalInfo) override;

  virtual bool DeallocPBackgroundSDBConnectionParent(
      PBackgroundSDBConnectionParent* aActor) override;

  virtual PBackgroundLSDatabaseParent* AllocPBackgroundLSDatabaseParent(
      const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
      const uint64_t& aDatastoreId) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundLSDatabaseConstructor(
      PBackgroundLSDatabaseParent* aActor, const PrincipalInfo& aPrincipalInfo,
      const uint32_t& aPrivateBrowsingId,
      const uint64_t& aDatastoreId) override;

  virtual bool DeallocPBackgroundLSDatabaseParent(
      PBackgroundLSDatabaseParent* aActor) override;

  virtual PBackgroundLSObserverParent* AllocPBackgroundLSObserverParent(
      const uint64_t& aObserverId) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundLSObserverConstructor(
      PBackgroundLSObserverParent* aActor,
      const uint64_t& aObserverId) override;

  virtual bool DeallocPBackgroundLSObserverParent(
      PBackgroundLSObserverParent* aActor) override;

  virtual PBackgroundLSRequestParent* AllocPBackgroundLSRequestParent(
      const LSRequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundLSRequestConstructor(
      PBackgroundLSRequestParent* aActor,
      const LSRequestParams& aParams) override;

  virtual bool DeallocPBackgroundLSRequestParent(
      PBackgroundLSRequestParent* aActor) override;

  virtual PBackgroundLSSimpleRequestParent*
  AllocPBackgroundLSSimpleRequestParent(
      const LSSimpleRequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundLSSimpleRequestConstructor(
      PBackgroundLSSimpleRequestParent* aActor,
      const LSSimpleRequestParams& aParams) override;

  virtual bool DeallocPBackgroundLSSimpleRequestParent(
      PBackgroundLSSimpleRequestParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvLSClearPrivateBrowsing() override;

  virtual PBackgroundLocalStorageCacheParent*
  AllocPBackgroundLocalStorageCacheParent(
      const PrincipalInfo& aPrincipalInfo, const nsCString& aOriginKey,
      const uint32_t& aPrivateBrowsingId) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundLocalStorageCacheConstructor(
      PBackgroundLocalStorageCacheParent* aActor,
      const PrincipalInfo& aPrincipalInfo, const nsCString& aOriginKey,
      const uint32_t& aPrivateBrowsingId) override;

  virtual bool DeallocPBackgroundLocalStorageCacheParent(
      PBackgroundLocalStorageCacheParent* aActor) override;

  virtual PBackgroundStorageParent* AllocPBackgroundStorageParent(
      const nsString& aProfilePath) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundStorageConstructor(
      PBackgroundStorageParent* aActor, const nsString& aProfilePath) override;

  virtual bool DeallocPBackgroundStorageParent(
      PBackgroundStorageParent* aActor) override;

  virtual PPendingIPCBlobParent* AllocPPendingIPCBlobParent(
      const IPCBlob& aBlob) override;

  virtual bool DeallocPPendingIPCBlobParent(
      PPendingIPCBlobParent* aActor) override;

  virtual already_AddRefed<PIPCBlobInputStreamParent>
  AllocPIPCBlobInputStreamParent(const nsID& aID,
                                 const uint64_t& aSize) override;

  virtual mozilla::ipc::IPCResult RecvPIPCBlobInputStreamConstructor(
      PIPCBlobInputStreamParent* aActor, const nsID& aID,
      const uint64_t& aSize) override;

  virtual PTemporaryIPCBlobParent* AllocPTemporaryIPCBlobParent() override;

  virtual mozilla::ipc::IPCResult RecvPTemporaryIPCBlobConstructor(
      PTemporaryIPCBlobParent* actor) override;

  virtual bool DeallocPTemporaryIPCBlobParent(
      PTemporaryIPCBlobParent* aActor) override;

  virtual PFileCreatorParent* AllocPFileCreatorParent(
      const nsString& aFullPath, const nsString& aType, const nsString& aName,
      const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
      const bool& aIsFromNsIFile) override;

  virtual mozilla::ipc::IPCResult RecvPFileCreatorConstructor(
      PFileCreatorParent* actor, const nsString& aFullPath,
      const nsString& aType, const nsString& aName,
      const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
      const bool& aIsFromNsIFile) override;

  virtual bool DeallocPFileCreatorParent(PFileCreatorParent* aActor) override;

  virtual mozilla::dom::PRemoteWorkerParent* AllocPRemoteWorkerParent(
      const RemoteWorkerData& aData) override;

  virtual bool DeallocPRemoteWorkerParent(PRemoteWorkerParent* aActor) override;

  virtual mozilla::dom::PRemoteWorkerControllerParent*
  AllocPRemoteWorkerControllerParent(
      const mozilla::dom::RemoteWorkerData& aRemoteWorkerData) override;

  virtual mozilla::ipc::IPCResult RecvPRemoteWorkerControllerConstructor(
      mozilla::dom::PRemoteWorkerControllerParent* aActor,
      const mozilla::dom::RemoteWorkerData& aRemoteWorkerData) override;

  virtual bool DeallocPRemoteWorkerControllerParent(
      mozilla::dom::PRemoteWorkerControllerParent* aActor) override;

  virtual mozilla::dom::PRemoteWorkerServiceParent*
  AllocPRemoteWorkerServiceParent() override;

  virtual mozilla::ipc::IPCResult RecvPRemoteWorkerServiceConstructor(
      PRemoteWorkerServiceParent* aActor) override;

  virtual bool DeallocPRemoteWorkerServiceParent(
      PRemoteWorkerServiceParent* aActor) override;

  virtual mozilla::dom::PSharedWorkerParent* AllocPSharedWorkerParent(
      const mozilla::dom::RemoteWorkerData& aData, const uint64_t& aWindowID,
      const mozilla::dom::MessagePortIdentifier& aPortIdentifier) override;

  virtual mozilla::ipc::IPCResult RecvPSharedWorkerConstructor(
      PSharedWorkerParent* aActor, const mozilla::dom::RemoteWorkerData& aData,
      const uint64_t& aWindowID,
      const mozilla::dom::MessagePortIdentifier& aPortIdentifier) override;

  virtual bool DeallocPSharedWorkerParent(PSharedWorkerParent* aActor) override;

  virtual PFileDescriptorSetParent* AllocPFileDescriptorSetParent(
      const FileDescriptor& aFileDescriptor) override;

  virtual bool DeallocPFileDescriptorSetParent(
      PFileDescriptorSetParent* aActor) override;

  virtual PVsyncParent* AllocPVsyncParent() override;

  virtual bool DeallocPVsyncParent(PVsyncParent* aActor) override;

  virtual PBroadcastChannelParent* AllocPBroadcastChannelParent(
      const PrincipalInfo& aPrincipalInfo, const nsCString& aOrigin,
      const nsString& aChannel) override;

  virtual mozilla::ipc::IPCResult RecvPBroadcastChannelConstructor(
      PBroadcastChannelParent* actor, const PrincipalInfo& aPrincipalInfo,
      const nsCString& origin, const nsString& channel) override;

  virtual bool DeallocPBroadcastChannelParent(
      PBroadcastChannelParent* aActor) override;

  virtual PChildToParentStreamParent* AllocPChildToParentStreamParent()
      override;

  virtual bool DeallocPChildToParentStreamParent(
      PChildToParentStreamParent* aActor) override;

  virtual PParentToChildStreamParent* AllocPParentToChildStreamParent()
      override;

  virtual bool DeallocPParentToChildStreamParent(
      PParentToChildStreamParent* aActor) override;

  virtual PServiceWorkerManagerParent* AllocPServiceWorkerManagerParent()
      override;

  virtual bool DeallocPServiceWorkerManagerParent(
      PServiceWorkerManagerParent* aActor) override;

  virtual PCamerasParent* AllocPCamerasParent() override;

  virtual bool DeallocPCamerasParent(PCamerasParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvShutdownServiceWorkerRegistrar() override;

  virtual dom::cache::PCacheStorageParent* AllocPCacheStorageParent(
      const dom::cache::Namespace& aNamespace,
      const PrincipalInfo& aPrincipalInfo) override;

  virtual bool DeallocPCacheStorageParent(
      dom::cache::PCacheStorageParent* aActor) override;

  virtual dom::cache::PCacheParent* AllocPCacheParent() override;

  virtual bool DeallocPCacheParent(dom::cache::PCacheParent* aActor) override;

  virtual dom::cache::PCacheStreamControlParent*
  AllocPCacheStreamControlParent() override;

  virtual bool DeallocPCacheStreamControlParent(
      dom::cache::PCacheStreamControlParent* aActor) override;

  virtual PUDPSocketParent* AllocPUDPSocketParent(
      const Maybe<PrincipalInfo>& pInfo, const nsCString& aFilter) override;
  virtual mozilla::ipc::IPCResult RecvPUDPSocketConstructor(
      PUDPSocketParent*, const Maybe<PrincipalInfo>& aPrincipalInfo,
      const nsCString& aFilter) override;
  virtual bool DeallocPUDPSocketParent(PUDPSocketParent*) override;

  virtual PMessagePortParent* AllocPMessagePortParent(
      const nsID& aUUID, const nsID& aDestinationUUID,
      const uint32_t& aSequenceID) override;

  virtual mozilla::ipc::IPCResult RecvPMessagePortConstructor(
      PMessagePortParent* aActor, const nsID& aUUID,
      const nsID& aDestinationUUID, const uint32_t& aSequenceID) override;

  virtual bool DeallocPMessagePortParent(PMessagePortParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvMessagePortForceClose(
      const nsID& aUUID, const nsID& aDestinationUUID,
      const uint32_t& aSequenceID) override;

  virtual PQuotaParent* AllocPQuotaParent() override;

  virtual bool DeallocPQuotaParent(PQuotaParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvShutdownQuotaManager() override;

  virtual already_AddRefed<PFileSystemRequestParent>
  AllocPFileSystemRequestParent(const FileSystemParams&) override;

  virtual mozilla::ipc::IPCResult RecvPFileSystemRequestConstructor(
      PFileSystemRequestParent* actor, const FileSystemParams& params) override;

  // Gamepad API Background IPC
  virtual PGamepadEventChannelParent* AllocPGamepadEventChannelParent()
      override;

  virtual bool DeallocPGamepadEventChannelParent(
      PGamepadEventChannelParent* aActor) override;

  virtual PGamepadTestChannelParent* AllocPGamepadTestChannelParent() override;

  virtual bool DeallocPGamepadTestChannelParent(
      PGamepadTestChannelParent* aActor) override;

  virtual PWebAuthnTransactionParent* AllocPWebAuthnTransactionParent()
      override;

  virtual bool DeallocPWebAuthnTransactionParent(
      PWebAuthnTransactionParent* aActor) override;

  virtual already_AddRefed<PHttpBackgroundChannelParent>
  AllocPHttpBackgroundChannelParent(const uint64_t& aChannelId) override;

  virtual mozilla::ipc::IPCResult RecvPHttpBackgroundChannelConstructor(
      PHttpBackgroundChannelParent* aActor,
      const uint64_t& aChannelId) override;

  virtual PClientManagerParent* AllocPClientManagerParent() override;

  virtual bool DeallocPClientManagerParent(
      PClientManagerParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvPClientManagerConstructor(
      PClientManagerParent* aActor) override;

  virtual PMIDIPortParent* AllocPMIDIPortParent(
      const MIDIPortInfo& aPortInfo, const bool& aSysexEnabled) override;

  virtual bool DeallocPMIDIPortParent(PMIDIPortParent* aActor) override;

  virtual PMIDIManagerParent* AllocPMIDIManagerParent() override;

  virtual bool DeallocPMIDIManagerParent(PMIDIManagerParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvStorageActivity(
      const PrincipalInfo& aPrincipalInfo) override;

  virtual PServiceWorkerParent* AllocPServiceWorkerParent(
      const IPCServiceWorkerDescriptor&) override;

  virtual bool DeallocPServiceWorkerParent(PServiceWorkerParent*) override;

  virtual mozilla::ipc::IPCResult RecvPServiceWorkerConstructor(
      PServiceWorkerParent* aActor,
      const IPCServiceWorkerDescriptor& aDescriptor) override;

  virtual PServiceWorkerContainerParent* AllocPServiceWorkerContainerParent()
      override;

  virtual bool DeallocPServiceWorkerContainerParent(
      PServiceWorkerContainerParent*) override;

  virtual mozilla::ipc::IPCResult RecvPServiceWorkerContainerConstructor(
      PServiceWorkerContainerParent* aActor) override;

  virtual PServiceWorkerRegistrationParent*
  AllocPServiceWorkerRegistrationParent(
      const IPCServiceWorkerRegistrationDescriptor&) override;

  virtual bool DeallocPServiceWorkerRegistrationParent(
      PServiceWorkerRegistrationParent*) override;

  virtual mozilla::ipc::IPCResult RecvPServiceWorkerRegistrationConstructor(
      PServiceWorkerRegistrationParent* aActor,
      const IPCServiceWorkerRegistrationDescriptor& aDescriptor) override;

  virtual PEndpointForReportParent* AllocPEndpointForReportParent(
      const nsString& aGroupName, const PrincipalInfo& aPrincipalInfo) override;

  virtual mozilla::ipc::IPCResult RecvPEndpointForReportConstructor(
      PEndpointForReportParent* actor, const nsString& aGroupName,
      const PrincipalInfo& aPrincipalInfo) override;

  virtual bool DeallocPEndpointForReportParent(
      PEndpointForReportParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvRemoveEndpoint(
      const nsString& aGroupName, const nsCString& aEndpointURL,
      const PrincipalInfo& aPrincipalInfo) override;

  virtual dom::PMediaTransportParent* AllocPMediaTransportParent() override;
  virtual bool DeallocPMediaTransportParent(
      dom::PMediaTransportParent* aActor) override;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_backgroundparentimpl_h__
