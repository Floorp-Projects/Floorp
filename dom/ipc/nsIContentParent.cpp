/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentParent.h"

#include "mozilla/AppProcessChecker.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/PTabContext.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/StructuredCloneIPCHelper.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/unused.h"

#include "nsFrameMessageManager.h"
#include "nsPrintfCString.h"
#include "xpcpublic.h"

using namespace mozilla::jsipc;

// XXX need another bug to move this to a common header.
#ifdef DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

namespace mozilla {
namespace dom {

nsIContentParent::nsIContentParent()
{
  mMessageManager = nsFrameMessageManager::NewProcessMessageManager(true);
}

ContentParent*
nsIContentParent::AsContentParent()
{
  MOZ_ASSERT(IsContentParent());
  return static_cast<ContentParent*>(this);
}

ContentBridgeParent*
nsIContentParent::AsContentBridgeParent()
{
  MOZ_ASSERT(IsContentBridgeParent());
  return static_cast<ContentBridgeParent*>(this);
}

PJavaScriptParent*
nsIContentParent::AllocPJavaScriptParent()
{
  return NewJavaScriptParent(xpc::GetJSRuntime());
}

bool
nsIContentParent::DeallocPJavaScriptParent(PJavaScriptParent* aParent)
{
  ReleaseJavaScriptParent(aParent);
  return true;
}

bool
nsIContentParent::CanOpenBrowser(const IPCTabContext& aContext)
{
  const IPCTabAppBrowserContext& appBrowser = aContext.appBrowserContext();

  // We don't trust the IPCTabContext we receive from the child, so we'll bail
  // if we receive an IPCTabContext that's not a PopupIPCTabContext.
  // (PopupIPCTabContext lets the child process prove that it has access to
  // the app it's trying to open.)
  if (appBrowser.type() != IPCTabAppBrowserContext::TPopupIPCTabContext) {
    ASSERT_UNLESS_FUZZING("Unexpected IPCTabContext type.  Aborting AllocPBrowserParent.");
    return false;
  }

  const PopupIPCTabContext& popupContext = appBrowser.get_PopupIPCTabContext();
  if (popupContext.opener().type() != PBrowserOrId::TPBrowserParent) {
    ASSERT_UNLESS_FUZZING("Unexpected PopupIPCTabContext type.  Aborting AllocPBrowserParent.");
    return false;
  }

  auto opener = TabParent::GetFrom(popupContext.opener().get_PBrowserParent());
  if (!opener) {
    ASSERT_UNLESS_FUZZING("Got null opener from child; aborting AllocPBrowserParent.");
    return false;
  }

  // Popup windows of isBrowser frames must be isBrowser if the parent
  // isBrowser.  Allocating a !isBrowser frame with same app ID would allow
  // the content to access data it's not supposed to.
  if (!popupContext.isBrowserElement() && opener->IsBrowserElement()) {
    ASSERT_UNLESS_FUZZING("Child trying to escalate privileges!  Aborting AllocPBrowserParent.");
    return false;
  }

  MaybeInvalidTabContext tc(aContext);
  if (!tc.IsValid()) {
    NS_ERROR(nsPrintfCString("Child passed us an invalid TabContext.  (%s)  "
                             "Aborting AllocPBrowserParent.",
                             tc.GetInvalidReason()).get());
    return false;
  }

  return true;
}

PBrowserParent*
nsIContentParent::AllocPBrowserParent(const TabId& aTabId,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const ContentParentId& aCpId,
                                      const bool& aIsForApp,
                                      const bool& aIsForBrowser)
{
  unused << aCpId;
  unused << aIsForApp;
  unused << aIsForBrowser;

  if (!CanOpenBrowser(aContext)) {
    return nullptr;
  }

  MaybeInvalidTabContext tc(aContext);
  MOZ_ASSERT(tc.IsValid());
  TabParent* parent = new TabParent(this, aTabId, tc.GetTabContext(), aChromeFlags);

  // We release this ref in DeallocPBrowserParent()
  NS_ADDREF(parent);
  return parent;
}

bool
nsIContentParent::DeallocPBrowserParent(PBrowserParent* aFrame)
{
  TabParent* parent = TabParent::GetFrom(aFrame);
  NS_RELEASE(parent);
  return true;
}

PBlobParent*
nsIContentParent::AllocPBlobParent(const BlobConstructorParams& aParams)
{
  return BlobParent::Create(this, aParams);
}

bool
nsIContentParent::DeallocPBlobParent(PBlobParent* aActor)
{
  BlobParent::Destroy(aActor);
  return true;
}

BlobParent*
nsIContentParent::GetOrCreateActorForBlob(Blob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);

  nsRefPtr<BlobImpl> blobImpl = aBlob->Impl();
  MOZ_ASSERT(blobImpl);

  return GetOrCreateActorForBlobImpl(blobImpl);
}

BlobParent*
nsIContentParent::GetOrCreateActorForBlobImpl(BlobImpl* aImpl)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aImpl);

  BlobParent* actor = BlobParent::GetOrCreate(this, aImpl);
  NS_ENSURE_TRUE(actor, nullptr);

  return actor;
}

bool
nsIContentParent::RecvSyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal,
                                  nsTArray<OwningSerializedStructuredCloneBuffer>* aRetvals)
{
  // FIXME Permission check in Content process
  nsIPrincipal* principal = aPrincipal;
  if (IsContentParent()) {
    ContentParent* parent = AsContentParent();
    if (!ContentParent::IgnoreIPCPrincipal() &&
        parent && principal && !AssertAppPrincipal(parent, principal)) {
      return false;
    }
  }

  nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    StructuredCloneIPCHelper helper;
    ipc::UnpackClonedMessageDataForParent(aData, helper);

    CrossProcessCpowHolder cpows(this, aCpows);
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), nullptr,
                        aMsg, true, &helper, &cpows, aPrincipal, aRetvals);
  }
  return true;
}

bool
nsIContentParent::RecvRpcMessage(const nsString& aMsg,
                                 const ClonedMessageData& aData,
                                 InfallibleTArray<CpowEntry>&& aCpows,
                                 const IPC::Principal& aPrincipal,
                                 nsTArray<OwningSerializedStructuredCloneBuffer>* aRetvals)
{
  // FIXME Permission check in Content process
  nsIPrincipal* principal = aPrincipal;
  if (IsContentParent()) {
    ContentParent* parent = AsContentParent();
    if (!ContentParent::IgnoreIPCPrincipal() &&
        parent && principal && !AssertAppPrincipal(parent, principal)) {
      return false;
    }
  }

  nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    StructuredCloneIPCHelper helper;
    ipc::UnpackClonedMessageDataForParent(aData, helper);

    CrossProcessCpowHolder cpows(this, aCpows);
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), nullptr,
                        aMsg, true, &helper, &cpows, aPrincipal, aRetvals);
  }
  return true;
}

bool
nsIContentParent::RecvAsyncMessage(const nsString& aMsg,
                                   const ClonedMessageData& aData,
                                   InfallibleTArray<CpowEntry>&& aCpows,
                                   const IPC::Principal& aPrincipal)
{
  // FIXME Permission check in Content process
  nsIPrincipal* principal = aPrincipal;
  if (IsContentParent()) {
    ContentParent* parent = AsContentParent();
    if (!ContentParent::IgnoreIPCPrincipal() &&
        parent && principal && !AssertAppPrincipal(parent, principal)) {
      return false;
    }
  }

  nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    StructuredCloneIPCHelper helper;
    ipc::UnpackClonedMessageDataForParent(aData, helper);

    CrossProcessCpowHolder cpows(this, aCpows);
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), nullptr,
                        aMsg, false, &helper, &cpows, aPrincipal, nullptr);
  }
  return true;
}

} // namespace dom
} // namespace mozilla
