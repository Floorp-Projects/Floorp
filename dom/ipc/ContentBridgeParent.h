/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBridgeParent_h
#define mozilla_dom_ContentBridgeParent_h

#include "mozilla/dom/PContentBridgeParent.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {

class ContentBridgeParent : public PContentBridgeParent
                          , public nsIContentParent
                          , public nsIObserver
{
public:
  explicit ContentBridgeParent();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeferredDestroy();
  virtual bool IsContentBridgeParent() const override { return true; }
  void NotifyTabDestroyed();

  static ContentBridgeParent*
  Create(Endpoint<PContentBridgeParent>&& aEndpoint);

  virtual PBlobParent*
  SendPBlobConstructor(PBlobParent* actor,
                       const BlobConstructorParams& params) override;

  virtual PBrowserParent*
  SendPBrowserConstructor(PBrowserParent* aActor,
                          const TabId& aTabId,
                          const TabId& aSameTabGroupAs,
                          const IPCTabContext& aContext,
                          const uint32_t& aChromeFlags,
                          const ContentParentId& aCpID,
                          const bool& aIsForBrowser) override;

  virtual PFileDescriptorSetParent*
  SendPFileDescriptorSetConstructor(const FileDescriptor&) override;

  FORWARD_SHMEM_ALLOCATOR_TO(PContentBridgeParent)

  jsipc::CPOWManager* GetCPOWManager() override;

  virtual ContentParentId ChildID() const override
  {
    return mChildID;
  }
  virtual bool IsForBrowser() const override
  {
    return mIsForBrowser;
  }
  virtual int32_t Pid() const override
  {
    // XXX: do we need this for ContentBridgeParent?
    return -1;
  }

  virtual mozilla::ipc::PParentToChildStreamParent*
  SendPParentToChildStreamConstructor(mozilla::ipc::PParentToChildStreamParent*) override;

  virtual bool SendActivate(PBrowserParent* aTab) override
  {
    return PContentBridgeParent::SendActivate(aTab);
  }

  virtual bool SendDeactivate(PBrowserParent* aTab) override
  {
    return PContentBridgeParent::SendDeactivate(aTab);
  }

  virtual bool SendParentActivated(PBrowserParent* aTab,
                                   const bool& aActivated) override
  {
    return PContentBridgeParent::SendParentActivated(aTab, aActivated);
  }

protected:
  virtual ~ContentBridgeParent();

  void SetChildID(ContentParentId aId)
  {
    mChildID = aId;
  }

  void SetIsForBrowser(bool aIsForBrowser)
  {
    mIsForBrowser = aIsForBrowser;
  }

  void Close()
  {
    // Trick NewRunnableMethod
    PContentBridgeParent::Close();
  }

protected:
  virtual mozilla::ipc::IPCResult
  RecvSyncMessage(const nsString& aMsg,
                  const ClonedMessageData& aData,
                  InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                  const IPC::Principal& aPrincipal,
                  nsTArray<StructuredCloneData>* aRetvals) override;

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                                   InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData) override;

  virtual jsipc::PJavaScriptParent* AllocPJavaScriptParent() override;

  virtual bool
  DeallocPJavaScriptParent(jsipc::PJavaScriptParent*) override;

  virtual PBrowserParent*
  AllocPBrowserParent(const TabId& aTabId,
                      const TabId& aSameTabGroupAs,
                      const IPCTabContext &aContext,
                      const uint32_t& aChromeFlags,
                      const ContentParentId& aCpID,
                      const bool& aIsForBrowser) override;

  virtual bool DeallocPBrowserParent(PBrowserParent*) override;

  virtual PBlobParent*
  AllocPBlobParent(const BlobConstructorParams& aParams) override;

  virtual bool DeallocPBlobParent(PBlobParent*) override;

  virtual PMemoryStreamParent*
  AllocPMemoryStreamParent(const uint64_t& aSize) override;

  virtual bool DeallocPMemoryStreamParent(PMemoryStreamParent*) override;

  virtual PIPCBlobInputStreamParent*
  SendPIPCBlobInputStreamConstructor(PIPCBlobInputStreamParent* aActor,
                                     const nsID& aID,
                                     const uint64_t& aSize) override;

  virtual PIPCBlobInputStreamParent*
  AllocPIPCBlobInputStreamParent(const nsID& aID,
                                 const uint64_t& aSize) override;

  virtual bool
  DeallocPIPCBlobInputStreamParent(PIPCBlobInputStreamParent*) override;

  virtual PChildToParentStreamParent* AllocPChildToParentStreamParent() override;

  virtual bool
  DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor) override;

  virtual mozilla::ipc::PParentToChildStreamParent*
  AllocPParentToChildStreamParent() override;

  virtual bool
  DeallocPParentToChildStreamParent(mozilla::ipc::PParentToChildStreamParent* aActor) override;

  virtual PFileDescriptorSetParent*
  AllocPFileDescriptorSetParent(const mozilla::ipc::FileDescriptor&) override;

  virtual bool
  DeallocPFileDescriptorSetParent(PFileDescriptorSetParent*) override;

  DISALLOW_EVIL_CONSTRUCTORS(ContentBridgeParent);

protected: // members
  RefPtr<ContentBridgeParent> mSelfRef;
  ContentParentId mChildID;
  bool mIsForBrowser;

private:
  friend class ContentParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ContentBridgeParent_h
