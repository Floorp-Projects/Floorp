/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentParent.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/PTabContext.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/ProcessMessageManager.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/ipc/IPCBlobInputStreamParent.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"
#include "mozilla/Unused.h"

#include "nsIWebBrowserChrome.h"
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
  return NewJavaScriptParent();
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
  // (PopupIPCTabContext lets the child process prove that it has access to
  // the app it's trying to open.)
  // On e10s we also allow UnsafeTabContext to allow service workers to open
  // windows. This is enforced in MaybeInvalidTabContext.
  if (aContext.type() != IPCTabContext::TPopupIPCTabContext &&
      aContext.type() != IPCTabContext::TUnsafeIPCTabContext) {
    ASSERT_UNLESS_FUZZING("Unexpected IPCTabContext type.  Aborting AllocPBrowserParent.");
    return false;
  }

  if (aContext.type() == IPCTabContext::TPopupIPCTabContext) {
    const PopupIPCTabContext& popupContext = aContext.get_PopupIPCTabContext();
    if (popupContext.opener().type() != PBrowserOrId::TPBrowserParent) {
      ASSERT_UNLESS_FUZZING("Unexpected PopupIPCTabContext type.  Aborting AllocPBrowserParent.");
      return false;
    }

    auto opener = TabParent::GetFrom(popupContext.opener().get_PBrowserParent());
    if (!opener) {
      ASSERT_UNLESS_FUZZING("Got null opener from child; aborting AllocPBrowserParent.");
      return false;
    }

    // Popup windows of isMozBrowserElement frames must be isMozBrowserElement if
    // the parent isMozBrowserElement.  Allocating a !isMozBrowserElement frame with
    // same app ID would allow the content to access data it's not supposed to.
    if (!popupContext.isMozBrowserElement() && opener->IsMozBrowserElement()) {
      ASSERT_UNLESS_FUZZING("Child trying to escalate privileges!  Aborting AllocPBrowserParent.");
      return false;
    }
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
                                      const TabId& aSameTabGroupAs,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const ContentParentId& aCpId,
                                      const bool& aIsForBrowser)
{
  MOZ_ASSERT(!aSameTabGroupAs);

  Unused << aCpId;
  Unused << aIsForBrowser;

  if (!CanOpenBrowser(aContext)) {
    return nullptr;
  }

  uint32_t chromeFlags = aChromeFlags;
  TabId openerTabId(0);
  ContentParentId openerCpId(0);
  if (aContext.type() == IPCTabContext::TPopupIPCTabContext) {
    // CanOpenBrowser has ensured that the IPCTabContext is of
    // type PopupIPCTabContext, and that the opener TabParent is
    // reachable.
    const PopupIPCTabContext& popupContext = aContext.get_PopupIPCTabContext();
    auto opener = TabParent::GetFrom(popupContext.opener().get_PBrowserParent());
    openerTabId = opener->GetTabId();
    openerCpId = opener->Manager()->ChildID();

    // We must ensure that the private browsing and remoteness flags
    // match those of the opener.
    nsCOMPtr<nsILoadContext> loadContext = opener->GetLoadContext();
    if (!loadContext) {
      return nullptr;
    }

    bool isPrivate;
    loadContext->GetUsePrivateBrowsing(&isPrivate);
    if (isPrivate) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
    }
  }

  if (openerTabId > 0 ||
      aContext.type() == IPCTabContext::TUnsafeIPCTabContext) {
    // Creation of PBrowser triggered from grandchild process is currently
    // broken and not supported (i.e. this code path doesn't work in
    // ContentBridgeParent).
    //
    // If you're working on fixing the code path for ContentBridgeParent,
    // remember to handle the remote frame registration below carefully as it
    // has to be registered in parent process.
    MOZ_ASSERT(XRE_IsParentProcess());
    if (!XRE_IsParentProcess()) {
      return nullptr;
    }

    // The creation of PBrowser was triggered from content process through
    // either window.open() or service worker's openWindow().
    // We need to register remote frame with the child generated tab id.
    ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
    if (!cpm->RegisterRemoteFrame(aTabId, openerCpId, openerTabId, aContext, aCpId)) {
      return nullptr;
    }
  }

  // And because we're allocating a remote browser, of course the
  // window is remote.
  chromeFlags |= nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;

  MaybeInvalidTabContext tc(aContext);
  MOZ_ASSERT(tc.IsValid());
  TabParent* parent = new TabParent(this, aTabId, tc.GetTabContext(), chromeFlags);

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

mozilla::ipc::IPCResult
nsIContentParent::RecvPBrowserConstructor(PBrowserParent* actor,
                                          const TabId& tabId,
                                          const TabId& sameTabGroupAs,
                                          const IPCTabContext& context,
                                          const uint32_t& chromeFlags,
                                          const ContentParentId& cpId,
                                          const bool& isForBrowser)
{
  TabParent* parent = TabParent::GetFrom(actor);
  // When enabling input event prioritization, input events may preempt other
  // normal priority IPC messages. To prevent the input events preempt
  // PBrowserConstructor, we use an IPC 'RemoteIsReadyToHandleInputEvents' to
  // notify parent that TabChild is created. In this case, PBrowser is initiated
  // from content so that we can set TabParent as ready to handle input events.
  parent->SetReadyToHandleInputEvents();
  return IPC_OK();
}

PIPCBlobInputStreamParent*
nsIContentParent::AllocPIPCBlobInputStreamParent(const nsID& aID,
                                                 const uint64_t& aSize)
{
  MOZ_CRASH("PIPCBlobInputStreamParent actors should be manually constructed!");
  return nullptr;
}

bool
nsIContentParent::DeallocPIPCBlobInputStreamParent(PIPCBlobInputStreamParent* aActor)
{
  RefPtr<IPCBlobInputStreamParent> actor =
    dont_AddRef(static_cast<IPCBlobInputStreamParent*>(aActor));
  return true;
}

mozilla::ipc::IPCResult
nsIContentParent::RecvSyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal,
                                  nsTArray<ipc::StructuredCloneData>* aRetvals)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "nsIContentParent::RecvSyncMessage", OTHER, aMsg);

  CrossProcessCpowHolder cpows(this, aCpows);
  RefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForParent(aData, data);

    ppm->ReceiveMessage(ppm, nullptr, aMsg, true, &data, &cpows, aPrincipal, aRetvals,
                        IgnoreErrors());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
nsIContentParent::RecvRpcMessage(const nsString& aMsg,
                                 const ClonedMessageData& aData,
                                 InfallibleTArray<CpowEntry>&& aCpows,
                                 const IPC::Principal& aPrincipal,
                                 nsTArray<ipc::StructuredCloneData>* aRetvals)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "nsIContentParent::RecvRpcMessage", OTHER, aMsg);

  CrossProcessCpowHolder cpows(this, aCpows);
  RefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForParent(aData, data);

    ppm->ReceiveMessage(ppm, nullptr, aMsg, true, &data, &cpows, aPrincipal, aRetvals,
                        IgnoreErrors());
  }
  return IPC_OK();
}

PFileDescriptorSetParent*
nsIContentParent::AllocPFileDescriptorSetParent(const FileDescriptor& aFD)
{
  return new FileDescriptorSetParent(aFD);
}

bool
nsIContentParent::DeallocPFileDescriptorSetParent(PFileDescriptorSetParent* aActor)
{
  delete static_cast<FileDescriptorSetParent*>(aActor);
  return true;
}

PChildToParentStreamParent*
nsIContentParent::AllocPChildToParentStreamParent()
{
  return mozilla::ipc::AllocPChildToParentStreamParent();
}

bool
nsIContentParent::DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor)
{
  delete aActor;
  return true;
}

PParentToChildStreamParent*
nsIContentParent::AllocPParentToChildStreamParent()
{
  MOZ_CRASH("PParentToChildStreamChild actors should be manually constructed!");
}

bool
nsIContentParent::DeallocPParentToChildStreamParent(PParentToChildStreamParent* aActor)
{
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult
nsIContentParent::RecvAsyncMessage(const nsString& aMsg,
                                   InfallibleTArray<CpowEntry>&& aCpows,
                                   const IPC::Principal& aPrincipal,
                                   const ClonedMessageData& aData)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "nsIContentParent::RecvAsyncMessage", OTHER, aMsg);

  CrossProcessCpowHolder cpows(this, aCpows);
  RefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForParent(aData, data);

    ppm->ReceiveMessage(ppm, nullptr, aMsg, false, &data, &cpows, aPrincipal, nullptr,
                        IgnoreErrors());
  }
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
