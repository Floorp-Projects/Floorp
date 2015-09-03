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
#include "mozilla/dom/StructuredCloneIPCHelper.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/ipc/InputStreamUtils.h"

#include "nsPrintfCString.h"
#include "xpcpublic.h"

using namespace mozilla::ipc;
using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

PJavaScriptChild*
nsIContentChild::AllocPJavaScriptChild()
{
  return NewJavaScriptChild(xpc::GetJSRuntime());
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
                                    const bool& aIsForApp,
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

  nsRefPtr<TabChild> child =
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

  nsRefPtr<BlobImpl> blobImpl = aBlob->Impl();
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

bool
nsIContentChild::RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal)
{
  nsRefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::GetChildProcessManager();
  if (cpm) {
    StructuredCloneIPCHelper helper;
    ipc::UnpackClonedMessageDataForChild(aData, helper);

    CrossProcessCpowHolder cpows(this, aCpows);
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()), nullptr,
                        aMsg, false, &helper, &cpows, aPrincipal, nullptr);
  }
  return true;
}

} // namespace dom
} // namespace mozilla
