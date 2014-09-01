/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBridgeChild_h
#define mozilla_dom_ContentBridgeChild_h

#include "mozilla/dom/PContentBridgeChild.h"
#include "mozilla/dom/nsIContentChild.h"

namespace mozilla {
namespace dom {

class ContentBridgeChild MOZ_FINAL : public PContentBridgeChild
                                   , public nsIContentChild
{
public:
  explicit ContentBridgeChild(Transport* aTransport);

  NS_DECL_ISUPPORTS

  static ContentBridgeChild*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  void DeferredDestroy();

  virtual bool RecvAsyncMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                const InfallibleTArray<jsipc::CpowEntry>& aCpows,
                                const IPC::Principal& aPrincipal) MOZ_OVERRIDE;

  virtual PBlobChild*
  SendPBlobConstructor(PBlobChild* actor,
                       const BlobConstructorParams& params);

  jsipc::JavaScriptChild* GetCPOWManager();

  virtual bool SendPBrowserConstructor(PBrowserChild* aActor,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const uint64_t& aID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser) MOZ_OVERRIDE;

protected:
  virtual ~ContentBridgeChild();

  virtual PBrowserChild* AllocPBrowserChild(const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const uint64_t& aID,
                                            const bool& aIsForApp,
                                            const bool& aIsForBrowser) MOZ_OVERRIDE;
  virtual bool DeallocPBrowserChild(PBrowserChild*) MOZ_OVERRIDE;
  virtual bool RecvPBrowserConstructor(PBrowserChild* aCctor,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const uint64_t& aID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser) MOZ_OVERRIDE;

  virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() MOZ_OVERRIDE;
  virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) MOZ_OVERRIDE;

  virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams) MOZ_OVERRIDE;
  virtual bool DeallocPBlobChild(PBlobChild*) MOZ_OVERRIDE;

  DISALLOW_EVIL_CONSTRUCTORS(ContentBridgeChild);

protected: // members
  nsRefPtr<ContentBridgeChild> mSelfRef;
  Transport* mTransport; // owned
};

} // dom
} // mozilla

#endif // mozilla_dom_ContentBridgeChild_h
