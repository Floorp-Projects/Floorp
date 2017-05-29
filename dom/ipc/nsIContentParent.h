/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsIContentParent_h
#define mozilla_dom_nsIContentParent_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/PChildToParentStreamParent.h"
#include "mozilla/ipc/PParentToChildStreamParent.h"

#include "nsFrameMessageManager.h"
#include "nsISupports.h"
#include "mozilla/dom/CPOWManagerGetter.h"

#define NS_ICONTENTPARENT_IID                                   \
  { 0xeeec9ebf, 0x8ecf, 0x4e38,                                 \
    { 0x81, 0xda, 0xb7, 0x34, 0x13, 0x7e, 0xac, 0xf3 } }

namespace IPC {
class Principal;
} // namespace IPC

namespace mozilla {

namespace jsipc {
class PJavaScriptParent;
class CpowEntry;
} // namespace jsipc

namespace ipc {
class PFileDescriptorSetParent;
class PChildToParentStreamParent;
class PParentToChildStreamParent;
class PIPCBlobInputStreamParent;
}

namespace dom {

class Blob;
class BlobConstructorParams;
class BlobImpl;
class ContentParent;
class ContentBridgeParent;
class IPCTabContext;
class PBrowserParent;

class nsIContentParent : public nsISupports
                       , public mozilla::dom::ipc::MessageManagerCallback
                       , public CPOWManagerGetter
                       , public mozilla::ipc::IShmemAllocator
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTPARENT_IID)

  nsIContentParent();

  virtual ContentParentId ChildID() const = 0;
  virtual bool IsForBrowser() const = 0;
  virtual bool IsForJSPlugin() const = 0;

  virtual mozilla::ipc::PIPCBlobInputStreamParent*
  SendPIPCBlobInputStreamConstructor(mozilla::ipc::PIPCBlobInputStreamParent* aActor,
                                     const nsID& aID,
                                     const uint64_t& aSize) = 0;

  MOZ_MUST_USE virtual PBrowserParent*
  SendPBrowserConstructor(PBrowserParent* actor,
                          const TabId& aTabId,
                          const TabId& aSameTabGroupAs,
                          const IPCTabContext& context,
                          const uint32_t& chromeFlags,
                          const ContentParentId& aCpId,
                          const bool& aIsForBrowser) = 0;

  virtual mozilla::ipc::PFileDescriptorSetParent*
  SendPFileDescriptorSetConstructor(const mozilla::ipc::FileDescriptor&) = 0;

  virtual bool IsContentParent() const { return false; }

  ContentParent* AsContentParent();

  virtual bool IsContentBridgeParent() const { return false; }

  ContentBridgeParent* AsContentBridgeParent();

  nsFrameMessageManager* GetMessageManager() const { return mMessageManager; }

  virtual bool SendActivate(PBrowserParent* aTab) = 0;

  virtual bool SendDeactivate(PBrowserParent* aTab) = 0;

  virtual bool SendParentActivated(PBrowserParent* aTab,
                                   const bool& aActivated) = 0;

  virtual int32_t Pid() const = 0;

  virtual mozilla::ipc::PParentToChildStreamParent*
  SendPParentToChildStreamConstructor(mozilla::ipc::PParentToChildStreamParent*) = 0;

protected: // methods
  bool CanOpenBrowser(const IPCTabContext& aContext);

protected: // IPDL methods
  virtual mozilla::jsipc::PJavaScriptParent* AllocPJavaScriptParent();
  virtual bool DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*);

  virtual PBrowserParent* AllocPBrowserParent(const TabId& aTabId,
                                              const TabId& aSameTabGroupsAs,
                                              const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const ContentParentId& aCpId,
                                              const bool& aIsForBrowser);
  virtual bool DeallocPBrowserParent(PBrowserParent* frame);

  virtual mozilla::ipc::PIPCBlobInputStreamParent*
  AllocPIPCBlobInputStreamParent(const nsID& aID, const uint64_t& aSize);

  virtual bool
  DeallocPIPCBlobInputStreamParent(mozilla::ipc::PIPCBlobInputStreamParent* aActor);

  virtual mozilla::ipc::PFileDescriptorSetParent*
  AllocPFileDescriptorSetParent(const mozilla::ipc::FileDescriptor& aFD);

  virtual bool
  DeallocPFileDescriptorSetParent(mozilla::ipc::PFileDescriptorSetParent* aActor);

  virtual mozilla::ipc::PChildToParentStreamParent* AllocPChildToParentStreamParent();

  virtual bool
  DeallocPChildToParentStreamParent(mozilla::ipc::PChildToParentStreamParent* aActor);

  virtual mozilla::ipc::PParentToChildStreamParent* AllocPParentToChildStreamParent();

  virtual bool
  DeallocPParentToChildStreamParent(mozilla::ipc::PParentToChildStreamParent* aActor);

  virtual mozilla::ipc::IPCResult RecvSyncMessage(const nsString& aMsg,
                                                  const ClonedMessageData& aData,
                                                  InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                                  const IPC::Principal& aPrincipal,
                                                  nsTArray<ipc::StructuredCloneData>* aRetvals);
  virtual mozilla::ipc::IPCResult RecvRpcMessage(const nsString& aMsg,
                                                 const ClonedMessageData& aData,
                                                 InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                                 const IPC::Principal& aPrincipal,
                                                 nsTArray<ipc::StructuredCloneData>* aRetvals);
  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                                   InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData);

protected: // members
  RefPtr<nsFrameMessageManager> mMessageManager;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentParent, NS_ICONTENTPARENT_IID)

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_nsIContentParent_h */
