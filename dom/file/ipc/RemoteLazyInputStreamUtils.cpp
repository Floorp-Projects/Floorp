/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStreamUtils.h"
#include "RemoteLazyInputStream.h"
#include "RemoteLazyInputStreamChild.h"
#include "RemoteLazyInputStreamParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "RemoteLazyInputStreamStorage.h"
#include "StreamBlobImpl.h"

namespace mozilla {

namespace {

template <typename M>
nsresult SerializeInputStreamParent(nsIInputStream* aInputStream,
                                    uint64_t aSize, uint64_t aChildID,
                                    PRemoteLazyInputStreamParent*& aActorParent,
                                    M* aManager) {
  // Parent to Child we always send a RemoteLazyInputStream.
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIInputStream> stream = aInputStream;

  // In case this is a RemoteLazyInputStream, we don't want to create a loop:
  // RemoteLazyInputStreamParent -> RemoteLazyInputStream ->
  // RemoteLazyInputStreamParent. Let's use the underlying inputStream instead.
  nsCOMPtr<mozIRemoteLazyInputStream> remoteLazyInputStream =
      do_QueryInterface(aInputStream);
  if (remoteLazyInputStream) {
    stream = remoteLazyInputStream->GetInternalStream();
    // If we don't have an underlying stream, it's better to terminate here
    // instead of sending an 'empty' RemoteLazyInputStream actor on the other
    // side, unable to be used.
    if (NS_WARN_IF(!stream)) {
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv;
  RefPtr<RemoteLazyInputStreamParent> parentActor =
      RemoteLazyInputStreamParent::Create(stream, aSize, aChildID, &rv,
                                          aManager);
  if (!parentActor) {
    return rv;
  }

  if (!aManager->SendPRemoteLazyInputStreamConstructor(
          parentActor, parentActor->ID(), parentActor->Size())) {
    return NS_ERROR_FAILURE;
  }

  aActorParent = parentActor;
  return NS_OK;
}

}  // anonymous namespace

// static
nsresult RemoteLazyInputStreamUtils::SerializeInputStream(
    nsIInputStream* aInputStream, uint64_t aSize, RemoteLazyStream& aOutStream,
    dom::ContentParent* aManager) {
  PRemoteLazyInputStreamParent* actor = nullptr;
  nsresult rv = SerializeInputStreamParent(
      aInputStream, aSize, aManager->ChildID(), actor, aManager);
  NS_ENSURE_SUCCESS(rv, rv);

  aOutStream = actor;
  return NS_OK;
}

// static
nsresult RemoteLazyInputStreamUtils::SerializeInputStream(
    nsIInputStream* aInputStream, uint64_t aSize, RemoteLazyStream& aOutStream,
    mozilla::ipc::PBackgroundParent* aManager) {
  PRemoteLazyInputStreamParent* actor = nullptr;
  nsresult rv = SerializeInputStreamParent(
      aInputStream, aSize, mozilla::ipc::BackgroundParent::GetChildID(aManager),
      actor, aManager);
  NS_ENSURE_SUCCESS(rv, rv);

  aOutStream = actor;
  return NS_OK;
}

// static
nsresult RemoteLazyInputStreamUtils::SerializeInputStream(
    nsIInputStream* aInputStream, uint64_t aSize, RemoteLazyStream& aOutStream,
    dom::ContentChild* aManager) {
  mozilla::ipc::AutoIPCStream ipcStream(true /* delayed start */);
  if (!ipcStream.Serialize(aInputStream, aManager)) {
    return NS_ERROR_FAILURE;
  }

  aOutStream = ipcStream.TakeValue();
  return NS_OK;
}

// static
nsresult RemoteLazyInputStreamUtils::SerializeInputStream(
    nsIInputStream* aInputStream, uint64_t aSize, RemoteLazyStream& aOutStream,
    mozilla::ipc::PBackgroundChild* aManager) {
  mozilla::ipc::AutoIPCStream ipcStream(true /* delayed start */);
  if (!ipcStream.Serialize(aInputStream, aManager)) {
    return NS_ERROR_FAILURE;
  }

  aOutStream = ipcStream.TakeValue();
  return NS_OK;
}

}  // namespace mozilla
