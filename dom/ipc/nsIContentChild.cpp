/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentChild.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
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

  nsRefPtr<TabChild> child = TabChild::Create(this, tc.GetTabContext(), aChromeFlags);

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
  delete aActor;
  return true;
}

BlobChild*
nsIContentChild::GetOrCreateActorForBlob(nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);

  // If the blob represents a remote blob then we can simply pass its actor back
  // here.
  const auto* domFile = static_cast<DOMFile*>(aBlob);
  nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(domFile->Impl());
  if (remoteBlob) {
    BlobChild* actor =
      static_cast<BlobChild*>(
        static_cast<PBlobChild*>(remoteBlob->GetPBlob()));
    MOZ_ASSERT(actor);
    if (actor->Manager() == this) {
      return actor;
    }
  }

  // All blobs shared between processes must be immutable.
  nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(aBlob);
  if (!mutableBlob || NS_FAILED(mutableBlob->SetMutable(false))) {
    NS_WARNING("Failed to make blob immutable!");
    return nullptr;
  }

#ifdef DEBUG
  {
    // XXX This is only safe so long as all blob implementations in our tree
    //     inherit DOMFileImplBase. If that ever changes then this will need to
    //     grow a real interface or something.
    const auto* blob = static_cast<DOMFileImplBase*>(domFile->Impl());

    MOZ_ASSERT(!blob->IsSizeUnknown());
    MOZ_ASSERT(!blob->IsDateUnknown());
  }
#endif

  ParentBlobConstructorParams params;

  nsString contentType;
  nsresult rv = aBlob->GetType(contentType);
  NS_ENSURE_SUCCESS(rv, nullptr);

  uint64_t length;
  rv = aBlob->GetSize(&length);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIInputStream> stream;
  rv = aBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, nullptr);

  InputStreamParams inputStreamParams;
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  SerializeInputStream(stream, inputStreamParams, fds);

  MOZ_ASSERT(fds.IsEmpty());

  params.optionalInputStreamParams() = inputStreamParams;

  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
  if (file) {
    FileBlobConstructorParams fileParams;

    rv = file->GetName(fileParams.name());
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = file->GetMozLastModifiedDate(&fileParams.modDate());
    NS_ENSURE_SUCCESS(rv, nullptr);

    fileParams.contentType() = contentType;
    fileParams.length() = length;

    params.blobParams() = fileParams;
  } else {
    NormalBlobConstructorParams blobParams;
    blobParams.contentType() = contentType;
    blobParams.length() = length;
    params.blobParams() = blobParams;
  }

  BlobChild* actor = BlobChild::Create(this, aBlob);
  NS_ENSURE_TRUE(actor, nullptr);

  return SendPBlobConstructor(actor, params) ? actor : nullptr;
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
    CpowIdHolder cpows(GetCPOWManager(), aCpows);
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()),
                        aMsg, false, &cloneData, &cpows, aPrincipal, nullptr);
  }
  return true;
}

} // dom
} // mozilla
