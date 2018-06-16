/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/ProcessMessageManager.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsXULAppAPI.h"
#include "nsIObserverService.h"
#include "base/task.h"

using namespace mozilla::ipc;
using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ContentBridgeParent,
                  nsIContentParent,
                  nsIObserver)

ContentBridgeParent::ContentBridgeParent()
  : mIsForBrowser(false)
  , mIsForJSPlugin(false)
{}

ContentBridgeParent::~ContentBridgeParent()
{
}

void
ContentBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->RemoveObserver(this, "content-child-shutdown");
  }
  MessageLoop::current()->PostTask(
    NewRunnableMethod("dom::ContentBridgeParent::DeferredDestroy",
                      this,
                      &ContentBridgeParent::DeferredDestroy));
}

/*static*/ ContentBridgeParent*
ContentBridgeParent::Create(Endpoint<PContentBridgeParent>&& aEndpoint)
{
  RefPtr<ContentBridgeParent> bridge = new ContentBridgeParent();
  bridge->mSelfRef = bridge;

  DebugOnly<bool> ok = aEndpoint.Bind(bridge);
  MOZ_ASSERT(ok);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(bridge, "content-child-shutdown", false);
  }

  // Initialize the message manager (and load delayed scripts) now that we
  // have established communications with the child.
  bridge->mMessageManager->InitWithCallback(bridge);

  return bridge.get();
}

void
ContentBridgeParent::DeferredDestroy()
{
  mSelfRef = nullptr;
  // |this| was just destroyed, hands off
}

mozilla::ipc::IPCResult
ContentBridgeParent::RecvSyncMessage(const nsString& aMsg,
                                     const ClonedMessageData& aData,
                                     InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                     const IPC::Principal& aPrincipal,
                                     nsTArray<StructuredCloneData>* aRetvals)
{
  return nsIContentParent::RecvSyncMessage(aMsg, aData, std::move(aCpows),
                                           aPrincipal, aRetvals);
}

mozilla::ipc::IPCResult
ContentBridgeParent::RecvAsyncMessage(const nsString& aMsg,
                                      InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                      const IPC::Principal& aPrincipal,
                                      const ClonedMessageData& aData)
{
  return nsIContentParent::RecvAsyncMessage(aMsg, std::move(aCpows),
                                            aPrincipal, aData);
}

PBrowserParent*
ContentBridgeParent::SendPBrowserConstructor(PBrowserParent* aActor,
                                             const TabId& aTabId,
                                             const TabId& aSameTabGroupAs,
                                             const IPCTabContext& aContext,
                                             const uint32_t& aChromeFlags,
                                             const ContentParentId& aCpID,
                                             const bool& aIsForBrowser)
{
  return PContentBridgeParent::SendPBrowserConstructor(aActor,
                                                       aTabId,
                                                       aSameTabGroupAs,
                                                       aContext,
                                                       aChromeFlags,
                                                       aCpID,
                                                       aIsForBrowser);
}

PParentToChildStreamParent*
ContentBridgeParent::SendPParentToChildStreamConstructor(PParentToChildStreamParent* aActor)
{
  return PContentBridgeParent::SendPParentToChildStreamConstructor(aActor);
}

PIPCBlobInputStreamParent*
ContentBridgeParent::SendPIPCBlobInputStreamConstructor(PIPCBlobInputStreamParent* aActor,
                                                        const nsID& aID,
                                                        const uint64_t& aSize)
{
  return
    PContentBridgeParent::SendPIPCBlobInputStreamConstructor(aActor, aID, aSize);
}

PIPCBlobInputStreamParent*
ContentBridgeParent::AllocPIPCBlobInputStreamParent(const nsID& aID,
                                                    const uint64_t& aSize)
{
  return nsIContentParent::AllocPIPCBlobInputStreamParent(aID, aSize);
}

bool
ContentBridgeParent::DeallocPIPCBlobInputStreamParent(PIPCBlobInputStreamParent* aActor)
{
  return nsIContentParent::DeallocPIPCBlobInputStreamParent(aActor);
}

mozilla::jsipc::PJavaScriptParent *
ContentBridgeParent::AllocPJavaScriptParent()
{
  return nsIContentParent::AllocPJavaScriptParent();
}

bool
ContentBridgeParent::DeallocPJavaScriptParent(PJavaScriptParent *parent)
{
  return nsIContentParent::DeallocPJavaScriptParent(parent);
}

PBrowserParent*
ContentBridgeParent::AllocPBrowserParent(const TabId& aTabId,
                                         const TabId& aSameTabGroupAs,
                                         const IPCTabContext &aContext,
                                         const uint32_t& aChromeFlags,
                                         const ContentParentId& aCpID,
                                         const bool& aIsForBrowser)
{
  return nsIContentParent::AllocPBrowserParent(aTabId,
                                               aSameTabGroupAs,
                                               aContext,
                                               aChromeFlags,
                                               aCpID,
                                               aIsForBrowser);
}

bool
ContentBridgeParent::DeallocPBrowserParent(PBrowserParent* aParent)
{
  return nsIContentParent::DeallocPBrowserParent(aParent);
}

mozilla::ipc::IPCResult
ContentBridgeParent::RecvPBrowserConstructor(PBrowserParent* actor,
                                             const TabId& tabId,
                                             const TabId& sameTabGroupAs,
                                             const IPCTabContext& context,
                                             const uint32_t& chromeFlags,
                                             const ContentParentId& cpId,
                                             const bool& isForBrowser)
{
  return nsIContentParent::RecvPBrowserConstructor(actor,
                                                   tabId,
                                                   sameTabGroupAs,
                                                   context,
                                                   chromeFlags,
                                                   cpId,
                                                   isForBrowser);
}

void
ContentBridgeParent::NotifyTabDestroyed()
{
  int32_t numLiveTabs = ManagedPBrowserParent().Count();
  if (numLiveTabs == 1) {
    MessageLoop::current()->PostTask(NewRunnableMethod(
      "dom::ContentBridgeParent::Close", this, &ContentBridgeParent::Close));
  }
}

// This implementation is identical to ContentParent::GetCPOWManager but we can't
// move it to nsIContentParent because it calls ManagedPJavaScriptParent() which
// only exists in PContentParent and PContentBridgeParent.
jsipc::CPOWManager*
ContentBridgeParent::GetCPOWManager()
{
  if (PJavaScriptParent* p = LoneManagedOrNullAsserts(ManagedPJavaScriptParent())) {
    return CPOWManagerFor(p);
  }
  return nullptr;
}

NS_IMETHODIMP
ContentBridgeParent::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  if (!strcmp(aTopic, "content-child-shutdown")) {
    Close();
  }
  return NS_OK;
}

PFileDescriptorSetParent*
ContentBridgeParent::SendPFileDescriptorSetConstructor(const FileDescriptor& aFD)
{
  return PContentBridgeParent::SendPFileDescriptorSetConstructor(aFD);
}

PFileDescriptorSetParent*
ContentBridgeParent::AllocPFileDescriptorSetParent(const FileDescriptor& aFD)
{
  return nsIContentParent::AllocPFileDescriptorSetParent(aFD);
}

bool
ContentBridgeParent::DeallocPFileDescriptorSetParent(PFileDescriptorSetParent* aActor)
{
  return nsIContentParent::DeallocPFileDescriptorSetParent(aActor);
}

PChildToParentStreamParent*
ContentBridgeParent::AllocPChildToParentStreamParent()
{
  return nsIContentParent::AllocPChildToParentStreamParent();
}

bool
ContentBridgeParent::DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor)
{
  return nsIContentParent::DeallocPChildToParentStreamParent(aActor);
}

PParentToChildStreamParent*
ContentBridgeParent::AllocPParentToChildStreamParent()
{
  return nsIContentParent::AllocPParentToChildStreamParent();
}

bool
ContentBridgeParent::DeallocPParentToChildStreamParent(PParentToChildStreamParent* aActor)
{
  return nsIContentParent::DeallocPParentToChildStreamParent(aActor);
}

} // namespace dom
} // namespace mozilla
