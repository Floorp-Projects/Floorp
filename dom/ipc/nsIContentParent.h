/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsIContentParent_h
#define mozilla_dom_nsIContentParent_h

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

class BlobConstructorParams;
class BlobParent;
class ContentParent;
class File;
class IPCTabContext;
class PBlobParent;
class PBrowserParent;

class nsIContentParent : public nsISupports
                       , public mozilla::dom::ipc::MessageManagerCallback
                       , public CPOWManagerGetter
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTPARENT_IID)

  nsIContentParent();

  BlobParent* GetOrCreateActorForBlob(File* aBlob);

  virtual uint64_t ChildID() = 0;
  virtual bool IsForApp() = 0;
  virtual bool IsForBrowser() = 0;

  virtual PBlobParent* SendPBlobConstructor(
    PBlobParent* aActor,
    const BlobConstructorParams& aParams) NS_WARN_UNUSED_RESULT = 0;

  virtual PBrowserParent* SendPBrowserConstructor(
    PBrowserParent* actor,
    const IPCTabContext& context,
    const uint32_t& chromeFlags,
    const uint64_t& aId,
    const bool& aIsForApp,
    const bool& aIsForBrowser) NS_WARN_UNUSED_RESULT = 0;

  virtual bool IsContentParent() { return false; }
  ContentParent* AsContentParent();

protected: // methods
  bool CanOpenBrowser(const IPCTabContext& aContext);

protected: // IPDL methods
  virtual mozilla::jsipc::PJavaScriptParent* AllocPJavaScriptParent();
  virtual bool DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*);

  virtual PBrowserParent* AllocPBrowserParent(const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const uint64_t& aId,
                                              const bool& aIsForApp,
                                              const bool& aIsForBrowser);
  virtual bool DeallocPBrowserParent(PBrowserParent* frame);

  virtual PBlobParent* AllocPBlobParent(const BlobConstructorParams& aParams);

  virtual bool DeallocPBlobParent(PBlobParent* aActor);

  virtual bool RecvSyncMessage(const nsString& aMsg,
                               const ClonedMessageData& aData,
                               const InfallibleTArray<jsipc::CpowEntry>& aCpows,
                               const IPC::Principal& aPrincipal,
                               InfallibleTArray<nsString>* aRetvals);
  virtual bool AnswerRpcMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                const InfallibleTArray<jsipc::CpowEntry>& aCpows,
                                const IPC::Principal& aPrincipal,
                                InfallibleTArray<nsString>* aRetvals);
  virtual bool RecvAsyncMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                const InfallibleTArray<jsipc::CpowEntry>& aCpows,
                                const IPC::Principal& aPrincipal);

protected: // members
  nsRefPtr<nsFrameMessageManager> mMessageManager;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentParent, NS_ICONTENTPARENT_IID)

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_nsIContentParent_h */
