/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsIContentChild_h
#define mozilla_dom_nsIContentChild_h

#include "mozilla/dom/ipc/IdType.h"

#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"
#include "mozilla/dom/CPOWManagerGetter.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"

#define NS_ICONTENTCHILD_IID                                    \
  { 0x4eed2e73, 0x94ba, 0x48a8,                                 \
    { 0xa2, 0xd1, 0xa5, 0xed, 0x86, 0xd7, 0xbb, 0xe4 } }

class nsString;

namespace IPC {
class Principal;
} // namespace IPC

namespace mozilla {
namespace ipc {
class FileDescriptor;
class PFileDescriptorSetChild;
class PSendStreamChild;
class Shmem;
} // namespace ipc

namespace jsipc {
class PJavaScriptChild;
class CpowEntry;
} // namespace jsipc

namespace dom {

class Blob;
class BlobChild;
class BlobImpl;
class BlobConstructorParams;
class ClonedMessageData;
class IPCTabContext;
class PBlobChild;
class PBrowserChild;

class nsIContentChild : public nsISupports
                      , public CPOWManagerGetter
                      , public mozilla::ipc::IShmemAllocator
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTCHILD_IID)

  BlobChild* GetOrCreateActorForBlob(Blob* aBlob);
  BlobChild* GetOrCreateActorForBlobImpl(BlobImpl* aImpl);

  virtual PBlobChild*
  SendPBlobConstructor(PBlobChild* aActor,
                       const BlobConstructorParams& aParams) = 0;

  virtual bool
  SendPBrowserConstructor(PBrowserChild* aActor,
                          const TabId& aTabId,
                          const IPCTabContext& aContext,
                          const uint32_t& aChromeFlags,
                          const ContentParentId& aCpID,
                          const bool& aIsForBrowser) = 0;

  virtual mozilla::ipc::PFileDescriptorSetChild*
  SendPFileDescriptorSetConstructor(const mozilla::ipc::FileDescriptor&) = 0;

  virtual mozilla::ipc::PSendStreamChild*
  SendPSendStreamConstructor(mozilla::ipc::PSendStreamChild*) = 0;

protected:
  virtual jsipc::PJavaScriptChild* AllocPJavaScriptChild();
  virtual bool DeallocPJavaScriptChild(jsipc::PJavaScriptChild*);

  virtual PBrowserChild* AllocPBrowserChild(const TabId& aTabId,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const ContentParentId& aCpId,
                                            const bool& aIsForBrowser);
  virtual bool DeallocPBrowserChild(PBrowserChild*);

  virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams);

  virtual bool DeallocPBlobChild(PBlobChild* aActor);

  virtual mozilla::ipc::PSendStreamChild* AllocPSendStreamChild();

  virtual bool DeallocPSendStreamChild(mozilla::ipc::PSendStreamChild* aActor);

  virtual mozilla::ipc::PFileDescriptorSetChild*
  AllocPFileDescriptorSetChild(const mozilla::ipc::FileDescriptor& aFD);

  virtual bool
  DeallocPFileDescriptorSetChild(mozilla::ipc::PFileDescriptorSetChild* aActor);

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                                   InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentChild, NS_ICONTENTCHILD_IID)

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_nsIContentChild_h */
