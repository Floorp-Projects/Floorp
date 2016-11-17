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
#include "mozilla/ipc/SendStream.h"

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
    TabChild::Create(this, aTabId, tc.GetTabContext(), aChromeFlags);

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

PSendStreamChild*
nsIContentChild::AllocPSendStreamChild()
{
  MOZ_CRASH("PSendStreamChild actors should be manually constructed!");
}

bool
nsIContentChild::DeallocPSendStreamChild(PSendStreamChild* aActor)
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
  RefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::GetChildProcessManager();
  if (cpm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForChild(aData, data);

    CrossProcessCpowHolder cpows(this, aCpows);
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()), nullptr,
                        aMsg, false, &data, &cpows, aPrincipal, nullptr);
  }
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
