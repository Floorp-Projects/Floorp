/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundparentimpl_h__
#define mozilla_ipc_backgroundparentimpl_h__

#include "mozilla/Attributes.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PBackgroundParent.h"

namespace mozilla::ipc {

// Instances of this class should never be created directly. This class is meant
// to be inherited in BackgroundImpl.
class BackgroundParentImpl : public PBackgroundParent {
 protected:
  BackgroundParentImpl();
  virtual ~BackgroundParentImpl();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundTestParent* AllocPBackgroundTestParent(
      const nsCString& aTestArg) override;

  mozilla::ipc::IPCResult RecvPBackgroundTestConstructor(
      PBackgroundTestParent* aActor, const nsCString& aTestArg) override;

  bool DeallocPBackgroundTestParent(PBackgroundTestParent* aActor) override;

  already_AddRefed<PBackgroundFileSystemParent>
  AllocPBackgroundFileSystemParent() override;

  already_AddRefed<PBackgroundIDBFactoryParent>
  AllocPBackgroundIDBFactoryParent(const LoggingInfo& aLoggingInfo) override;

  already_AddRefed<net::PBackgroundDataBridgeParent>
  AllocPBackgroundDataBridgeParent(const uint64_t& aChannelID) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBFactoryConstructor(
      PBackgroundIDBFactoryParent* aActor,
      const LoggingInfo& aLoggingInfo) override;

  PBackgroundIndexedDBUtilsParent* AllocPBackgroundIndexedDBUtilsParent()
      override;

  bool DeallocPBackgroundIndexedDBUtilsParent(
      PBackgroundIndexedDBUtilsParent* aActor) override;

  mozilla::ipc::IPCResult RecvFlushPendingFileDeletions() override;

  PBackgroundSDBConnectionParent* AllocPBackgroundSDBConnectionParent(
      const PersistenceType& aPersistenceType,
      const PrincipalInfo& aPrincipalInfo) override;

  mozilla::ipc::IPCResult RecvPBackgroundSDBConnectionConstructor(
      PBackgroundSDBConnectionParent* aActor,
      const PersistenceType& aPersistenceType,
      const PrincipalInfo& aPrincipalInfo) override;

  bool DeallocPBackgroundSDBConnectionParent(
      PBackgroundSDBConnectionParent* aActor) override;

  PBackgroundLSDatabaseParent* AllocPBackgroundLSDatabaseParent(
      const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
      const uint64_t& aDatastoreId) override;

  mozilla::ipc::IPCResult RecvPBackgroundLSDatabaseConstructor(
      PBackgroundLSDatabaseParent* aActor, const PrincipalInfo& aPrincipalInfo,
      const uint32_t& aPrivateBrowsingId,
      const uint64_t& aDatastoreId) override;

  bool DeallocPBackgroundLSDatabaseParent(
      PBackgroundLSDatabaseParent* aActor) override;

  PBackgroundLSObserverParent* AllocPBackgroundLSObserverParent(
      const uint64_t& aObserverId) override;

  mozilla::ipc::IPCResult RecvPBackgroundLSObserverConstructor(
      PBackgroundLSObserverParent* aActor,
      const uint64_t& aObserverId) override;

  bool DeallocPBackgroundLSObserverParent(
      PBackgroundLSObserverParent* aActor) override;

  PBackgroundLSRequestParent* AllocPBackgroundLSRequestParent(
      const LSRequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundLSRequestConstructor(
      PBackgroundLSRequestParent* aActor,
      const LSRequestParams& aParams) override;

  bool DeallocPBackgroundLSRequestParent(
      PBackgroundLSRequestParent* aActor) override;

  PBackgroundLSSimpleRequestParent* AllocPBackgroundLSSimpleRequestParent(
      const LSSimpleRequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundLSSimpleRequestConstructor(
      PBackgroundLSSimpleRequestParent* aActor,
      const LSSimpleRequestParams& aParams) override;

  bool DeallocPBackgroundLSSimpleRequestParent(
      PBackgroundLSSimpleRequestParent* aActor) override;

  mozilla::ipc::IPCResult RecvLSClearPrivateBrowsing() override;

  PBackgroundLocalStorageCacheParent* AllocPBackgroundLocalStorageCacheParent(
      const PrincipalInfo& aPrincipalInfo, const nsCString& aOriginKey,
      const uint32_t& aPrivateBrowsingId) override;

  mozilla::ipc::IPCResult RecvPBackgroundLocalStorageCacheConstructor(
      PBackgroundLocalStorageCacheParent* aActor,
      const PrincipalInfo& aPrincipalInfo, const nsCString& aOriginKey,
      const uint32_t& aPrivateBrowsingId) override;

  bool DeallocPBackgroundLocalStorageCacheParent(
      PBackgroundLocalStorageCacheParent* aActor) override;

  PBackgroundStorageParent* AllocPBackgroundStorageParent(
      const nsString& aProfilePath,
      const uint32_t& aPrivateBrowsingId) override;

  mozilla::ipc::IPCResult RecvPBackgroundStorageConstructor(
      PBackgroundStorageParent* aActor, const nsString& aProfilePath,
      const uint32_t& aPrivateBrowsingId) override;

  bool DeallocPBackgroundStorageParent(
      PBackgroundStorageParent* aActor) override;

  already_AddRefed<PBackgroundSessionStorageManagerParent>
  AllocPBackgroundSessionStorageManagerParent(
      const uint64_t& aTopContextId) override;

  already_AddRefed<PBackgroundSessionStorageServiceParent>
  AllocPBackgroundSessionStorageServiceParent() override;

  already_AddRefed<PIdleSchedulerParent> AllocPIdleSchedulerParent() override;

  PTemporaryIPCBlobParent* AllocPTemporaryIPCBlobParent() override;

  mozilla::ipc::IPCResult RecvPTemporaryIPCBlobConstructor(
      PTemporaryIPCBlobParent* actor) override;

  bool DeallocPTemporaryIPCBlobParent(PTemporaryIPCBlobParent* aActor) override;

  PFileCreatorParent* AllocPFileCreatorParent(
      const nsString& aFullPath, const nsString& aType, const nsString& aName,
      const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
      const bool& aIsFromNsIFile) override;

  mozilla::ipc::IPCResult RecvPFileCreatorConstructor(
      PFileCreatorParent* actor, const nsString& aFullPath,
      const nsString& aType, const nsString& aName,
      const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
      const bool& aIsFromNsIFile) override;

  bool DeallocPFileCreatorParent(PFileCreatorParent* aActor) override;

  mozilla::dom::PRemoteWorkerParent* AllocPRemoteWorkerParent(
      const RemoteWorkerData& aData) override;

  bool DeallocPRemoteWorkerParent(PRemoteWorkerParent* aActor) override;

  mozilla::dom::PRemoteWorkerControllerParent*
  AllocPRemoteWorkerControllerParent(
      const mozilla::dom::RemoteWorkerData& aRemoteWorkerData) override;

  mozilla::ipc::IPCResult RecvPRemoteWorkerControllerConstructor(
      mozilla::dom::PRemoteWorkerControllerParent* aActor,
      const mozilla::dom::RemoteWorkerData& aRemoteWorkerData) override;

  bool DeallocPRemoteWorkerControllerParent(
      mozilla::dom::PRemoteWorkerControllerParent* aActor) override;

  mozilla::dom::PRemoteWorkerServiceParent* AllocPRemoteWorkerServiceParent()
      override;

  mozilla::ipc::IPCResult RecvPRemoteWorkerServiceConstructor(
      PRemoteWorkerServiceParent* aActor) override;

  bool DeallocPRemoteWorkerServiceParent(
      PRemoteWorkerServiceParent* aActor) override;

  mozilla::dom::PSharedWorkerParent* AllocPSharedWorkerParent(
      const mozilla::dom::RemoteWorkerData& aData, const uint64_t& aWindowID,
      const mozilla::dom::MessagePortIdentifier& aPortIdentifier) override;

  mozilla::ipc::IPCResult RecvPSharedWorkerConstructor(
      PSharedWorkerParent* aActor, const mozilla::dom::RemoteWorkerData& aData,
      const uint64_t& aWindowID,
      const mozilla::dom::MessagePortIdentifier& aPortIdentifier) override;

  bool DeallocPSharedWorkerParent(PSharedWorkerParent* aActor) override;

  already_AddRefed<PVsyncParent> AllocPVsyncParent() override;

  already_AddRefed<mozilla::psm::PVerifySSLServerCertParent>
  AllocPVerifySSLServerCertParent(
      const nsTArray<ByteArray>& aPeerCertChain, const nsCString& aHostName,
      const int32_t& aPort, const OriginAttributes& aOriginAttributes,
      const Maybe<ByteArray>& aStapledOCSPResponse,
      const Maybe<ByteArray>& aSctsFromTLSExtension,
      const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
      const uint32_t& aProviderFlags,
      const uint32_t& aCertVerifierFlags) override;

  mozilla::ipc::IPCResult RecvPVerifySSLServerCertConstructor(
      PVerifySSLServerCertParent* aActor, nsTArray<ByteArray>&& aPeerCertChain,
      const nsCString& aHostName, const int32_t& aPort,
      const OriginAttributes& aOriginAttributes,
      const Maybe<ByteArray>& aStapledOCSPResponse,
      const Maybe<ByteArray>& aSctsFromTLSExtension,
      const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
      const uint32_t& aProviderFlags,
      const uint32_t& aCertVerifierFlags) override;

  PBroadcastChannelParent* AllocPBroadcastChannelParent(
      const PrincipalInfo& aPrincipalInfo, const nsCString& aOrigin,
      const nsString& aChannel) override;

  mozilla::ipc::IPCResult RecvPBroadcastChannelConstructor(
      PBroadcastChannelParent* actor, const PrincipalInfo& aPrincipalInfo,
      const nsCString& origin, const nsString& channel) override;

  bool DeallocPBroadcastChannelParent(PBroadcastChannelParent* aActor) override;

  PServiceWorkerManagerParent* AllocPServiceWorkerManagerParent() override;

  bool DeallocPServiceWorkerManagerParent(
      PServiceWorkerManagerParent* aActor) override;

  PCamerasParent* AllocPCamerasParent() override;
#ifdef MOZ_WEBRTC
  mozilla::ipc::IPCResult RecvPCamerasConstructor(
      PCamerasParent* aActor) override;
#endif
  bool DeallocPCamerasParent(PCamerasParent* aActor) override;

  mozilla::ipc::IPCResult RecvShutdownServiceWorkerRegistrar() override;

  dom::cache::PCacheStorageParent* AllocPCacheStorageParent(
      const dom::cache::Namespace& aNamespace,
      const PrincipalInfo& aPrincipalInfo) override;

  bool DeallocPCacheStorageParent(
      dom::cache::PCacheStorageParent* aActor) override;

  PUDPSocketParent* AllocPUDPSocketParent(const Maybe<PrincipalInfo>& pInfo,
                                          const nsCString& aFilter) override;
  mozilla::ipc::IPCResult RecvPUDPSocketConstructor(
      PUDPSocketParent*, const Maybe<PrincipalInfo>& aPrincipalInfo,
      const nsCString& aFilter) override;
  bool DeallocPUDPSocketParent(PUDPSocketParent*) override;

  PMessagePortParent* AllocPMessagePortParent(
      const nsID& aUUID, const nsID& aDestinationUUID,
      const uint32_t& aSequenceID) override;

  mozilla::ipc::IPCResult RecvPMessagePortConstructor(
      PMessagePortParent* aActor, const nsID& aUUID,
      const nsID& aDestinationUUID, const uint32_t& aSequenceID) override;

  already_AddRefed<PIPCClientCertsParent> AllocPIPCClientCertsParent() override;

  bool DeallocPMessagePortParent(PMessagePortParent* aActor) override;

  mozilla::ipc::IPCResult RecvMessagePortForceClose(
      const nsID& aUUID, const nsID& aDestinationUUID,
      const uint32_t& aSequenceID) override;

  PQuotaParent* AllocPQuotaParent() override;

  bool DeallocPQuotaParent(PQuotaParent* aActor) override;

  mozilla::ipc::IPCResult RecvShutdownQuotaManager() override;

  mozilla::ipc::IPCResult RecvShutdownBackgroundSessionStorageManagers()
      override;

  mozilla::ipc::IPCResult RecvPropagateBackgroundSessionStorageManager(
      const uint64_t& aCurrentTopContextId,
      const uint64_t& aTargetTopContextId) override;

  mozilla::ipc::IPCResult RecvRemoveBackgroundSessionStorageManager(
      const uint64_t& aTopContextId) override;

  mozilla::ipc::IPCResult RecvLoadSessionStorageManagerData(
      const uint64_t& aTopContextId,
      nsTArray<mozilla::dom::SSCacheCopy>&& aOriginCacheCopy) override;

  mozilla::ipc::IPCResult RecvGetSessionStorageManagerData(
      const uint64_t& aTopContextId, const uint32_t& aSizeLimit,
      const bool& aCancelSessionStoreTimer,
      GetSessionStorageManagerDataResolver&& aResolver) override;

  already_AddRefed<PFileSystemRequestParent> AllocPFileSystemRequestParent(
      const FileSystemParams&) override;

  mozilla::ipc::IPCResult RecvPFileSystemRequestConstructor(
      PFileSystemRequestParent* actor, const FileSystemParams& params) override;

  // Gamepad API Background IPC
  already_AddRefed<PGamepadEventChannelParent> AllocPGamepadEventChannelParent()
      override;

  already_AddRefed<PGamepadTestChannelParent> AllocPGamepadTestChannelParent()
      override;

  PWebAuthnTransactionParent* AllocPWebAuthnTransactionParent() override;

  bool DeallocPWebAuthnTransactionParent(
      PWebAuthnTransactionParent* aActor) override;

  already_AddRefed<PHttpBackgroundChannelParent>
  AllocPHttpBackgroundChannelParent(const uint64_t& aChannelId) override;

  mozilla::ipc::IPCResult RecvPHttpBackgroundChannelConstructor(
      PHttpBackgroundChannelParent* aActor,
      const uint64_t& aChannelId) override;

  PClientManagerParent* AllocPClientManagerParent() override;

  bool DeallocPClientManagerParent(PClientManagerParent* aActor) override;

  mozilla::ipc::IPCResult RecvPClientManagerConstructor(
      PClientManagerParent* aActor) override;

  PMIDIPortParent* AllocPMIDIPortParent(const MIDIPortInfo& aPortInfo,
                                        const bool& aSysexEnabled) override;

  bool DeallocPMIDIPortParent(PMIDIPortParent* aActor) override;

  PMIDIManagerParent* AllocPMIDIManagerParent() override;

  bool DeallocPMIDIManagerParent(PMIDIManagerParent* aActor) override;

  mozilla::ipc::IPCResult RecvStorageActivity(
      const PrincipalInfo& aPrincipalInfo) override;

  already_AddRefed<PServiceWorkerParent> AllocPServiceWorkerParent(
      const IPCServiceWorkerDescriptor&) final;

  mozilla::ipc::IPCResult RecvPServiceWorkerManagerConstructor(
      PServiceWorkerManagerParent* aActor) override;

  mozilla::ipc::IPCResult RecvPServiceWorkerConstructor(
      PServiceWorkerParent* aActor,
      const IPCServiceWorkerDescriptor& aDescriptor) override;

  already_AddRefed<PServiceWorkerContainerParent>
  AllocPServiceWorkerContainerParent() final;

  mozilla::ipc::IPCResult RecvPServiceWorkerContainerConstructor(
      PServiceWorkerContainerParent* aActor) override;

  already_AddRefed<PServiceWorkerRegistrationParent>
  AllocPServiceWorkerRegistrationParent(
      const IPCServiceWorkerRegistrationDescriptor&) final;

  mozilla::ipc::IPCResult RecvPServiceWorkerRegistrationConstructor(
      PServiceWorkerRegistrationParent* aActor,
      const IPCServiceWorkerRegistrationDescriptor& aDescriptor) override;

  PEndpointForReportParent* AllocPEndpointForReportParent(
      const nsString& aGroupName, const PrincipalInfo& aPrincipalInfo) override;

  mozilla::ipc::IPCResult RecvPEndpointForReportConstructor(
      PEndpointForReportParent* actor, const nsString& aGroupName,
      const PrincipalInfo& aPrincipalInfo) override;

  mozilla::ipc::IPCResult RecvEnsureRDDProcessAndCreateBridge(
      EnsureRDDProcessAndCreateBridgeResolver&& aResolver) override;

  mozilla::ipc::IPCResult RecvEnsureUtilityProcessAndCreateBridge(
      EnsureUtilityProcessAndCreateBridgeResolver&& aResolver) override;

  bool DeallocPEndpointForReportParent(
      PEndpointForReportParent* aActor) override;

  mozilla::ipc::IPCResult RecvRemoveEndpoint(
      const nsString& aGroupName, const nsCString& aEndpointURL,
      const PrincipalInfo& aPrincipalInfo) override;

  dom::PMediaTransportParent* AllocPMediaTransportParent() override;
  bool DeallocPMediaTransportParent(
      dom::PMediaTransportParent* aActor) override;

  already_AddRefed<mozilla::net::PWebSocketConnectionParent>
  AllocPWebSocketConnectionParent(const uint32_t& aListenerId) override;
  mozilla::ipc::IPCResult RecvPWebSocketConnectionConstructor(
      PWebSocketConnectionParent* actor, const uint32_t& aListenerId) override;

  already_AddRefed<PLockManagerParent> AllocPLockManagerParent(
      const ContentPrincipalInfo& aPrincipalInfo, const nsID& aClientId) final;
};

}  // namespace mozilla::ipc

#endif  // mozilla_ipc_backgroundparentimpl_h__
