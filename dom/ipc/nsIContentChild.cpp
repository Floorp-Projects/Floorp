/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentChild.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"
#include "mozilla/dom/ipc/MemoryStreamChild.h"
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

  return IPC_OK();
}

PMemoryStreamChild*
nsIContentChild::AllocPMemoryStreamChild(const uint64_t& aSize)
{
  return new MemoryStreamChild();
}

bool
nsIContentChild::DeallocPMemoryStreamChild(PMemoryStreamChild* aActor)
{
  delete aActor;
  return true;
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

PBlobChild*
nsIContentChild::AllocPBlobChild(const BlobConstructorParams& aParams)
{
  return BlobChild::Create(this, aParams);
}

bool
nsIContentChild::DeallocPBlobChild(PBlobChild* aActor)
{
  BlobChild::Destroy(aActor);
  return true;
}

BlobChild*
nsIContentChild::GetOrCreateActorForBlob(Blob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);

  RefPtr<BlobImpl> blobImpl = aBlob->Impl();
  MOZ_ASSERT(blobImpl);

  return GetOrCreateActorForBlobImpl(blobImpl);
}

BlobChild*
nsIContentChild::GetOrCreateActorForBlobImpl(BlobImpl* aImpl)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aImpl);

  BlobChild* actor = BlobChild::GetOrCreate(this, aImpl);
  NS_ENSURE_TRUE(actor, nullptr);

  return actor;
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
  NS_LossyConvertUTF16toASCII messageNameCStr(aMsg);
  PROFILER_LABEL_DYNAMIC("nsIContentChild", "RecvAsyncMessage",
                         js::ProfileEntry::Category::EVENTS,
                         messageNameCStr.get());

  CrossProcessCpowHolder cpows(this, aCpows);
  RefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::GetChildProcessManager();
  if (cpm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForChild(aData, data);

    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()), nullptr,
                        aMsg, false, &data, &cpows, aPrincipal, nullptr);
  }
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
