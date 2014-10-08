/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentChild.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/ipc/InputStreamUtils.h"

#include "JavaScriptChild.h"
#include "nsDOMFile.h"
#include "nsIJSRuntimeService.h"
#include "nsPrintfCString.h"

using namespace mozilla::ipc;
using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

PJavaScriptChild*
nsIContentChild::AllocPJavaScriptChild()
{
  nsCOMPtr<nsIJSRuntimeService> svc = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
  NS_ENSURE_TRUE(svc, nullptr);

  JSRuntime *rt;
  svc->GetRuntime(&rt);
  NS_ENSURE_TRUE(svc, nullptr);

  nsAutoPtr<JavaScriptChild> child(new JavaScriptChild(rt));
  if (!child->init()) {
    return nullptr;
  }
  return child.forget();
}

bool
nsIContentChild::DeallocPJavaScriptChild(PJavaScriptChild* aChild)
{
  static_cast<JavaScriptChild*>(aChild)->decref();
  return true;
}

PBrowserChild*
nsIContentChild::AllocPBrowserChild(const IPCTabContext& aContext,
                                    const uint32_t& aChromeFlags,
                                    const uint64_t& aID,
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
    TabChild::Create(this, tc.GetTabContext(), aChromeFlags);

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
nsIContentChild::GetOrCreateActorForBlob(DOMFile* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);

  nsRefPtr<DOMFileImpl> blobImpl = aBlob->Impl();
  MOZ_ASSERT(blobImpl);

  BlobChild* actor = BlobChild::GetOrCreate(this, blobImpl);
  NS_ENSURE_TRUE(actor, nullptr);

  return actor;
}

bool
nsIContentChild::RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal)
{
  nsRefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::sChildProcessManager;
  if (cpm) {
    StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForChild(aData);
    CpowIdHolder cpows(this, aCpows);
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()),
                        aMsg, false, &cloneData, &cpows, aPrincipal, nullptr);
  }
  return true;
}

} // dom
} // mozilla
