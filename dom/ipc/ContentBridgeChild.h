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
  explicit ContentBridgeChild(Transport* aTransport);

  NS_DECL_ISUPPORTS

  static ContentBridgeChild*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeferredDestroy();

  virtual bool RecvAsyncMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                const IPC::Principal& aPrincipal) override;

  virtual PBlobChild*
  SendPBlobConstructor(PBlobChild* actor,
                       const BlobConstructorParams& aParams) override;

  jsipc::CPOWManager* GetCPOWManager() override;

  virtual bool SendPBrowserConstructor(PBrowserChild* aActor,
                                       const TabId& aTabId,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const ContentParentId& aCpID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser) override;

protected:
  virtual ~ContentBridgeChild();

  virtual PBrowserChild* AllocPBrowserChild(const TabId& aTabId,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const ContentParentId& aCpID,
                                            const bool& aIsForApp,
                                            const bool& aIsForBrowser) override;
  virtual bool DeallocPBrowserChild(PBrowserChild*) override;
  virtual bool RecvPBrowserConstructor(PBrowserChild* aCctor,
                                       const TabId& aTabId,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const ContentParentId& aCpID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser) override;

  virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() override;
  virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) override;

  virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams) override;
  virtual bool DeallocPBlobChild(PBlobChild*) override;

  DISALLOW_EVIL_CONSTRUCTORS(ContentBridgeChild);

protected: // members
  nsRefPtr<ContentBridgeChild> mSelfRef;
  Transport* mTransport; // owned
};

} // dom
} // mozilla

#endif // mozilla_dom_ContentBridgeChild_h
