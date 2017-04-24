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
} // namespace layout

namespace ipc {

// Instances of this class should never be created directly. This class is meant
// to be inherited in BackgroundImpl.
class BackgroundParentImpl : public PBackgroundParent
{
protected:
  BackgroundParentImpl();
  virtual ~BackgroundParentImpl();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PBackgroundTestParent*
  AllocPBackgroundTestParent(const nsCString& aTestArg) override;

  virtual mozilla::ipc::IPCResult
  RecvPBackgroundTestConstructor(PBackgroundTestParent* aActor,
                                 const nsCString& aTestArg) override;

  virtual bool
  DeallocPBackgroundTestParent(PBackgroundTestParent* aActor) override;

  virtual PBackgroundIDBFactoryParent*
  AllocPBackgroundIDBFactoryParent(const LoggingInfo& aLoggingInfo)
                                   override;

  virtual mozilla::ipc::IPCResult
  RecvPBackgroundIDBFactoryConstructor(PBackgroundIDBFactoryParent* aActor,
                                       const LoggingInfo& aLoggingInfo)
                                       override;

  virtual bool
  DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor)
                                     override;

  virtual PBackgroundIndexedDBUtilsParent*
  AllocPBackgroundIndexedDBUtilsParent() override;

  virtual bool
  DeallocPBackgroundIndexedDBUtilsParent(
                                        PBackgroundIndexedDBUtilsParent* aActor)
                                        override;

  virtual mozilla::ipc::IPCResult
  RecvFlushPendingFileDeletions() override;

  virtual PBlobParent*
  AllocPBlobParent(const BlobConstructorParams& aParams) override;

  virtual bool
  DeallocPBlobParent(PBlobParent* aActor) override;

  virtual PMemoryStreamParent*
  AllocPMemoryStreamParent(const uint64_t& aSize) override;

  virtual bool
  DeallocPMemoryStreamParent(PMemoryStreamParent* aActor) override;

  virtual PIPCBlobInputStreamParent*
  AllocPIPCBlobInputStreamParent(const nsID& aID,
                                 const uint64_t& aSize) override;

  virtual bool
  DeallocPIPCBlobInputStreamParent(PIPCBlobInputStreamParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvPBlobConstructor(PBlobParent* aActor,
                       const BlobConstructorParams& params) override;

  virtual PFileDescriptorSetParent*
  AllocPFileDescriptorSetParent(const FileDescriptor& aFileDescriptor)
                                override;

  virtual bool
  DeallocPFileDescriptorSetParent(PFileDescriptorSetParent* aActor)
                                  override;

  virtual PVsyncParent*
  AllocPVsyncParent() override;

  virtual bool
  DeallocPVsyncParent(PVsyncParent* aActor) override;

  virtual PBroadcastChannelParent*
  AllocPBroadcastChannelParent(const PrincipalInfo& aPrincipalInfo,
                               const nsCString& aOrigin,
                               const nsString& aChannel) override;

  virtual mozilla::ipc::IPCResult
  RecvPBroadcastChannelConstructor(PBroadcastChannelParent* actor,
                                   const PrincipalInfo& aPrincipalInfo,
                                   const nsCString& origin,
                                   const nsString& channel) override;

  virtual bool
  DeallocPBroadcastChannelParent(PBroadcastChannelParent* aActor) override;

  virtual PChildToParentStreamParent*
  AllocPChildToParentStreamParent() override;

  virtual bool
  DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor) override;

  virtual PParentToChildStreamParent*
  AllocPParentToChildStreamParent() override;

  virtual bool
  DeallocPParentToChildStreamParent(PParentToChildStreamParent* aActor) override;

  virtual PServiceWorkerManagerParent*
  AllocPServiceWorkerManagerParent() override;

  virtual bool
  DeallocPServiceWorkerManagerParent(PServiceWorkerManagerParent* aActor) override;

  virtual PCamerasParent*
  AllocPCamerasParent() override;

  virtual bool
  DeallocPCamerasParent(PCamerasParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvShutdownServiceWorkerRegistrar() override;

  virtual dom::cache::PCacheStorageParent*
  AllocPCacheStorageParent(const dom::cache::Namespace& aNamespace,
                           const PrincipalInfo& aPrincipalInfo) override;

  virtual bool
  DeallocPCacheStorageParent(dom::cache::PCacheStorageParent* aActor) override;

  virtual dom::cache::PCacheParent* AllocPCacheParent() override;

  virtual bool
  DeallocPCacheParent(dom::cache::PCacheParent* aActor) override;

  virtual dom::cache::PCacheStreamControlParent*
  AllocPCacheStreamControlParent() override;

  virtual bool
  DeallocPCacheStreamControlParent(dom::cache::PCacheStreamControlParent* aActor)
                                   override;

  virtual PUDPSocketParent*
  AllocPUDPSocketParent(const OptionalPrincipalInfo& pInfo,
                        const nsCString& aFilter) override;
  virtual mozilla::ipc::IPCResult
  RecvPUDPSocketConstructor(PUDPSocketParent*,
                            const OptionalPrincipalInfo& aPrincipalInfo,
                            const nsCString& aFilter) override;
  virtual bool
  DeallocPUDPSocketParent(PUDPSocketParent*) override;

  virtual PMessagePortParent*
  AllocPMessagePortParent(const nsID& aUUID,
                          const nsID& aDestinationUUID,
                          const uint32_t& aSequenceID) override;

  virtual mozilla::ipc::IPCResult
  RecvPMessagePortConstructor(PMessagePortParent* aActor,
                              const nsID& aUUID,
                              const nsID& aDestinationUUID,
                              const uint32_t& aSequenceID) override;

  virtual bool
  DeallocPMessagePortParent(PMessagePortParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvMessagePortForceClose(const nsID& aUUID,
                            const nsID& aDestinationUUID,
                            const uint32_t& aSequenceID) override;

  virtual PAsmJSCacheEntryParent*
  AllocPAsmJSCacheEntryParent(const dom::asmjscache::OpenMode& aOpenMode,
                              const dom::asmjscache::WriteParams& aWriteParams,
                              const PrincipalInfo& aPrincipalInfo) override;

  virtual bool
  DeallocPAsmJSCacheEntryParent(PAsmJSCacheEntryParent* aActor) override;

  virtual PQuotaParent*
  AllocPQuotaParent() override;

  virtual bool
  DeallocPQuotaParent(PQuotaParent* aActor) override;

  virtual PFileSystemRequestParent*
  AllocPFileSystemRequestParent(const FileSystemParams&) override;

  virtual mozilla::ipc::IPCResult
  RecvPFileSystemRequestConstructor(PFileSystemRequestParent* actor,
                                    const FileSystemParams& params) override;

  virtual bool
  DeallocPFileSystemRequestParent(PFileSystemRequestParent*) override;

  // Gamepad API Background IPC
  virtual PGamepadEventChannelParent*
  AllocPGamepadEventChannelParent() override;

  virtual bool
  DeallocPGamepadEventChannelParent(PGamepadEventChannelParent *aActor) override;

  virtual PGamepadTestChannelParent*
  AllocPGamepadTestChannelParent() override;

  virtual bool
  DeallocPGamepadTestChannelParent(PGamepadTestChannelParent* aActor) override;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundparentimpl_h__
