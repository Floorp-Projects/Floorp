/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentChild.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ChildProcessMessageManager.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"
#include "mozilla/dom/ipc/IPCBlobInputStreamChild.h"

#include "nsPrintfCString.h"
#include "xpcpublic.h"

using namespace mozilla::ipc;
using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

PJavaScriptChild*
nsIContentChild::AllocPJavaScriptChild()
{
  return NewJavaScriptChild();
}

bool
nsIContentChild::DeallocPJavaScriptChild(PJavaScriptChild* aChild)
{
  ReleaseJavaScriptChild(aChild);
  return true;
}

PBrowserChild*
nsIContentChild::AllocPBrowserChild(const TabId& aTabId,
                                    const TabId& aSameTabGroupAs,
                                    const IPCTabContext& aContext,
                                    const uint32_t& aChromeFlags,
                                    const ContentParentId& aCpID,
                                    const bool& aIsForBrowser)
{
  // We'll happily accept any kind of IPCTabContext here; we don't need to
  // check that it's of a certain type for security purposes, because we
  // believe whatever the parent process tells us.

  MaybeInvalidTabContext tc(aContext);
  if (!tc.IsValid()) {
    NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                             "the parent process. (%s)  Crashing...",
                             tc.GetInvalidReason()).get());
    MOZ_CRASH("Invalid TabContext received from the parent process.");
  }

  RefPtr<TabChild> child =
    TabChild::Create(this, aTabId, aSameTabGroupAs,
                     tc.GetTabContext(), aChromeFlags);

  // The ref here is released in DeallocPBrowserChild.
  return child.forget().take();
}

bool
nsIContentChild::DeallocPBrowserChild(PBrowserChild* aIframe)
{
  TabChild* child = static_cast<TabChild*>(aIframe);
  NS_RELEASE(child);
  return true;
}

mozilla::ipc::IPCResult
nsIContentChild::RecvPBrowserConstructor(PBrowserChild* aActor,
                                         const TabId& aTabId,
                                         const TabId& aSameTabGroupAs,
                                         const IPCTabContext& aContext,
                                         const uint32_t& aChromeFlags,
                                         const ContentParentId& aCpID,
                                         const bool& aIsForBrowser)
{
  // This runs after AllocPBrowserChild() returns and the IPC machinery for this
  // PBrowserChild has been set up.

  auto tabChild = static_cast<TabChild*>(static_cast<TabChild*>(aActor));

  if (NS_WARN_IF(NS_FAILED(tabChild->Init()))) {
    return IPC_FAIL(tabChild, "TabChild::Init failed");
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(static_cast<nsITabChild*>(tabChild), "tab-child-created", nullptr);
  }
  // Notify parent that we are ready to handle input events.
  tabChild->SendRemoteIsReadyToHandleInputEvents();
  return IPC_OK();
}

PIPCBlobInputStreamChild*
nsIContentChild::AllocPIPCBlobInputStreamChild(const nsID& aID,
                                               const uint64_t& aSize)
{
  // IPCBlobInputStreamChild is refcounted. Here it's created and in
  // DeallocPIPCBlobInputStreamChild is released.

  RefPtr<IPCBlobInputStreamChild> actor =
    new IPCBlobInputStreamChild(aID, aSize);
  return actor.forget().take();
}

bool
nsIContentChild::DeallocPIPCBlobInputStreamChild(PIPCBlobInputStreamChild* aActor)
{
  RefPtr<IPCBlobInputStreamChild> actor =
    dont_AddRef(static_cast<IPCBlobInputStreamChild*>(aActor));
  return true;
}

PChildToParentStreamChild*
nsIContentChild::AllocPChildToParentStreamChild()
{
  MOZ_CRASH("PChildToParentStreamChild actors should be manually constructed!");
}

bool
nsIContentChild::DeallocPChildToParentStreamChild(PChildToParentStreamChild* aActor)
{
  delete aActor;
  return true;
}

PParentToChildStreamChild*
nsIContentChild::AllocPParentToChildStreamChild()
{
  return mozilla::ipc::AllocPParentToChildStreamChild();
}

bool
nsIContentChild::DeallocPParentToChildStreamChild(PParentToChildStreamChild* aActor)
{
  delete aActor;
  return true;
}

PFileDescriptorSetChild*
nsIContentChild::AllocPFileDescriptorSetChild(const FileDescriptor& aFD)
{
  return new FileDescriptorSetChild(aFD);
}

bool
nsIContentChild::DeallocPFileDescriptorSetChild(PFileDescriptorSetChild* aActor)
{
  delete static_cast<FileDescriptorSetChild*>(aActor);
  return true;
}

mozilla::ipc::IPCResult
nsIContentChild::RecvAsyncMessage(const nsString& aMsg,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal,
                                  const ClonedMessageData& aData)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "nsIContentChild::RecvAsyncMessage", OTHER, aMsg);

  CrossProcessCpowHolder cpows(this, aCpows);
  RefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::GetChildProcessManager();
  if (cpm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForChild(aData, data);

    cpm->ReceiveMessage(cpm, nullptr, aMsg, false, &data, &cpows, aPrincipal, nullptr,
                        IgnoreErrors());
  }
  return IPC_OK();
}

/* static */
already_AddRefed<nsIEventTarget>
nsIContentChild::GetConstructedEventTarget(const IPC::Message& aMsg)
{
  ActorHandle handle;
  TabId tabId, sameTabGroupAs;
  PickleIterator iter(aMsg);
  if (!IPC::ReadParam(&aMsg, &iter, &handle)) {
    return nullptr;
  }
  aMsg.IgnoreSentinel(&iter);
  if (!IPC::ReadParam(&aMsg, &iter, &tabId)) {
    return nullptr;
  }
  aMsg.IgnoreSentinel(&iter);
  if (!IPC::ReadParam(&aMsg, &iter, &sameTabGroupAs)) {
    return nullptr;
  }

  // If sameTabGroupAs is non-zero, then the new tab will be in the same
  // TabGroup as a previously created tab. Rather than try to find the
  // previously created tab (whose constructor message may not even have been
  // processed yet, in theory) and look up its event target, we just use the
  // default event target. This means that runnables for this tab will not be
  // labeled. However, this path is only taken for print preview and view
  // source, which are not performance-sensitive.
  if (sameTabGroupAs) {
    return nullptr;
  }

  // If the request for a new TabChild is coming from the parent process, then
  // there is no opener. Therefore, we create a fresh TabGroup.
  RefPtr<TabGroup> tabGroup = new TabGroup();
  nsCOMPtr<nsIEventTarget> target = tabGroup->EventTargetFor(TaskCategory::Other);
  return target.forget();
}


} // namespace dom
} // namespace mozilla
