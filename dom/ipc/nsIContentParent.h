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

#include "nsFrameMessageManager.h"
#include "nsISupports.h"
#include "mozilla/dom/CPOWManagerGetter.h"

#define NS_ICONTENTPARENT_IID                                   \
  { 0xeeec9ebf, 0x8ecf, 0x4e38,                                 \
    { 0x81, 0xda, 0xb7, 0x34, 0x13, 0x7e, 0xac, 0xf3 } }

class nsFrameMessageManager;

namespace IPC {
class Principal;
} // namespace IPC

namespace mozilla {

namespace jsipc {
class PJavaScriptParent;
class CpowEntry;
} // namespace jsipc

namespace dom {

class Blob;
class BlobConstructorParams;
class BlobImpl;
class BlobParent;
class ContentParent;
class ContentBridgeParent;
class IPCTabContext;
class PBlobParent;
class PBrowserParent;

class nsIContentParent : public nsISupports
                       , public mozilla::dom::ipc::MessageManagerCallback
                       , public CPOWManagerGetter
                       , public mozilla::ipc::IShmemAllocator
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTPARENT_IID)

  nsIContentParent();

  BlobParent* GetOrCreateActorForBlob(Blob* aBlob);
  BlobParent* GetOrCreateActorForBlobImpl(BlobImpl* aImpl);

  virtual ContentParentId ChildID() const = 0;
  virtual bool IsForApp() const = 0;
  virtual bool IsForBrowser() const = 0;

  MOZ_MUST_USE virtual PBlobParent*
  SendPBlobConstructor(PBlobParent* aActor,
                       const BlobConstructorParams& aParams) = 0;

  MOZ_MUST_USE virtual PBrowserParent*
  SendPBrowserConstructor(PBrowserParent* actor,
                          const TabId& aTabId,
                          const IPCTabContext& context,
                          const uint32_t& chromeFlags,
                          const ContentParentId& aCpId,
                          const bool& aIsForApp,
                          const bool& aIsForBrowser) = 0;

  virtual bool IsContentParent() const { return false; }

  ContentParent* AsContentParent();

  virtual bool IsContentBridgeParent() const { return false; }

  ContentBridgeParent* AsContentBridgeParent();

  virtual int32_t Pid() const = 0;

protected: // methods
  bool CanOpenBrowser(const IPCTabContext& aContext);

protected: // IPDL methods
  virtual mozilla::jsipc::PJavaScriptParent* AllocPJavaScriptParent();
  virtual bool DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*);

  virtual PBrowserParent* AllocPBrowserParent(const TabId& aTabId,
                                              const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const ContentParentId& aCpId,
                                              const bool& aIsForApp,
                                              const bool& aIsForBrowser);
  virtual bool DeallocPBrowserParent(PBrowserParent* frame);

  virtual PBlobParent* AllocPBlobParent(const BlobConstructorParams& aParams);

  virtual bool DeallocPBlobParent(PBlobParent* aActor);

  virtual bool RecvSyncMessage(const nsString& aMsg,
                               const ClonedMessageData& aData,
                               InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                               const IPC::Principal& aPrincipal,
                               nsTArray<ipc::StructuredCloneData>* aRetvals);
  virtual bool RecvRpcMessage(const nsString& aMsg,
                              const ClonedMessageData& aData,
                              InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                              const IPC::Principal& aPrincipal,
                              nsTArray<ipc::StructuredCloneData>* aRetvals);
  virtual bool RecvAsyncMessage(const nsString& aMsg,
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
