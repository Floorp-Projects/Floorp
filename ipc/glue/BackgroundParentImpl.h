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
}

namespace ipc {

// Instances of this class should never be created directly. This class is meant
// to be inherited in BackgroundImpl.
class BackgroundParentImpl : public PBackgroundParent
{
protected:
  BackgroundParentImpl();
  virtual ~BackgroundParentImpl();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PBackgroundTestParent*
  AllocPBackgroundTestParent(const nsCString& aTestArg) MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundTestConstructor(PBackgroundTestParent* aActor,
                                 const nsCString& aTestArg) MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundTestParent(PBackgroundTestParent* aActor) MOZ_OVERRIDE;

  virtual PBackgroundIDBFactoryParent*
  AllocPBackgroundIDBFactoryParent(const LoggingInfo& aLoggingInfo)
                                   MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundIDBFactoryConstructor(PBackgroundIDBFactoryParent* aActor,
                                       const LoggingInfo& aLoggingInfo)
                                       MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor)
                                     MOZ_OVERRIDE;

  virtual PBlobParent*
  AllocPBlobParent(const BlobConstructorParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobParent(PBlobParent* aActor) MOZ_OVERRIDE;

  virtual PFileDescriptorSetParent*
  AllocPFileDescriptorSetParent(const FileDescriptor& aFileDescriptor)
                                MOZ_OVERRIDE;

  virtual bool
  DeallocPFileDescriptorSetParent(PFileDescriptorSetParent* aActor)
                                  MOZ_OVERRIDE;

  virtual PVsyncParent*
  AllocPVsyncParent() MOZ_OVERRIDE;

  virtual bool
  DeallocPVsyncParent(PVsyncParent* aActor) MOZ_OVERRIDE;

  virtual PBroadcastChannelParent*
  AllocPBroadcastChannelParent(const PrincipalInfo& aPrincipalInfo,
                               const nsString& aOrigin,
                               const nsString& aChannel) MOZ_OVERRIDE;

  virtual bool
  RecvPBroadcastChannelConstructor(PBroadcastChannelParent* actor,
                                   const PrincipalInfo& aPrincipalInfo,
                                   const nsString& origin,
                                   const nsString& channel) MOZ_OVERRIDE;

  virtual bool
  DeallocPBroadcastChannelParent(PBroadcastChannelParent* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvRegisterServiceWorker(const ServiceWorkerRegistrationData& aData)
                            MOZ_OVERRIDE;

  virtual bool
  RecvUnregisterServiceWorker(const PrincipalInfo& aPrincipalInfo,
                              const nsString& aScope) MOZ_OVERRIDE;

  virtual bool
  RecvShutdownServiceWorkerRegistrar() MOZ_OVERRIDE;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundparentimpl_h__
