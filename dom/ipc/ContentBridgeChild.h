/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBridgeChild_h
#define mozilla_dom_ContentBridgeChild_h

#include "mozilla/dom/PContentBridgeChild.h"
#include "mozilla/dom/nsIContentChild.h"

namespace mozilla {
namespace dom {

class ContentBridgeChild final : public PContentBridgeChild
                               , public nsIContentChild
{
public:
  explicit ContentBridgeChild();

  NS_DECL_ISUPPORTS

  static void
  Create(Endpoint<PContentBridgeChild>&& aEndpoint);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeferredDestroy();

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                                   InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData) override;

  jsipc::CPOWManager* GetCPOWManager() override;

  virtual bool SendPBrowserConstructor(PBrowserChild* aActor,
                                       const TabId& aTabId,
                                       const TabId& aSameTabGroupAs,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const ContentParentId& aCpID,
                                       const bool& aIsForBrowser) override;

  virtual mozilla::ipc::PFileDescriptorSetChild*
  SendPFileDescriptorSetConstructor(const mozilla::ipc::FileDescriptor&) override;

  virtual mozilla::ipc::PChildToParentStreamChild*
  SendPChildToParentStreamConstructor(mozilla::ipc::PChildToParentStreamChild*) override;

  virtual mozilla::ipc::IPCResult RecvActivate(PBrowserChild* aTab) override;

  virtual mozilla::ipc::IPCResult RecvDeactivate(PBrowserChild* aTab) override;

  virtual mozilla::ipc::IPCResult RecvParentActivated(PBrowserChild* aTab,
                                                      const bool& aActivated) override;

  virtual already_AddRefed<nsIEventTarget> GetEventTargetFor(TabChild* aTabChild) override;

  FORWARD_SHMEM_ALLOCATOR_TO(PContentBridgeChild)

protected:
  virtual ~ContentBridgeChild();

  virtual PBrowserChild* AllocPBrowserChild(const TabId& aTabId,
                                            const TabId& aSameTabGroupAs,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const ContentParentId& aCpID,
                                            const bool& aIsForBrowser) override;
  virtual bool DeallocPBrowserChild(PBrowserChild*) override;
  virtual mozilla::ipc::IPCResult RecvPBrowserConstructor(PBrowserChild* aCctor,
                                                          const TabId& aTabId,
                                                          const TabId& aSameTabGroupAs,
                                                          const IPCTabContext& aContext,
                                                          const uint32_t& aChromeFlags,
                                                          const ContentParentId& aCpID,
                                                          const bool& aIsForBrowser) override;

  virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() override;
  virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) override;

  virtual PIPCBlobInputStreamChild*
  AllocPIPCBlobInputStreamChild(const nsID& aID,
                                const uint64_t& aSize) override;

  virtual bool
  DeallocPIPCBlobInputStreamChild(PIPCBlobInputStreamChild*) override;

  virtual mozilla::ipc::PChildToParentStreamChild*
  AllocPChildToParentStreamChild() override;

  virtual bool
  DeallocPChildToParentStreamChild(mozilla::ipc::PChildToParentStreamChild* aActor) override;

  virtual PParentToChildStreamChild* AllocPParentToChildStreamChild() override;

  virtual bool
  DeallocPParentToChildStreamChild(PParentToChildStreamChild* aActor) override;

  virtual PFileDescriptorSetChild*
  AllocPFileDescriptorSetChild(const mozilla::ipc::FileDescriptor& aFD) override;

  virtual bool
  DeallocPFileDescriptorSetChild(mozilla::ipc::PFileDescriptorSetChild* aActor) override;

  DISALLOW_EVIL_CONSTRUCTORS(ContentBridgeChild);

private:
  virtual already_AddRefed<nsIEventTarget>
  GetConstructedEventTarget(const Message& aMsg) override;

protected: // members
  RefPtr<ContentBridgeChild> mSelfRef;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ContentBridgeChild_h
