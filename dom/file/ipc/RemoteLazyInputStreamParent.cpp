/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStreamParent.h"
#include "RemoteLazyInputStreamStorage.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsStreamUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"

namespace mozilla {

extern mozilla::LazyLogModule gRemoteLazyStreamLog;

RemoteLazyInputStreamParent::RemoteLazyInputStreamParent(const nsID& aID)
    : mID(aID) {
  auto storage = RemoteLazyInputStreamStorage::Get().unwrapOr(nullptr);
  if (storage) {
    storage->ActorCreated(mID);
  }
}

void RemoteLazyInputStreamParent::ActorDestroy(
    IProtocol::ActorDestroyReason aReason) {
  auto storage = RemoteLazyInputStreamStorage::Get().unwrapOr(nullptr);
  if (storage) {
    storage->ActorDestroyed(mID);
  }
}

mozilla::ipc::IPCResult RemoteLazyInputStreamParent::RecvClone(
    mozilla::ipc::Endpoint<PRemoteLazyInputStreamParent>&& aCloneEndpoint) {
  if (!aCloneEndpoint.IsValid()) {
    return IPC_FAIL(this, "Unexpected invalid endpoint in RecvClone");
  }

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
          ("Parent::RecvClone %s", nsIDToCString(mID).get()));

  auto* newActor = new RemoteLazyInputStreamParent(mID);
  aCloneEndpoint.Bind(newActor);

  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteLazyInputStreamParent::RecvStreamNeeded(
    uint64_t aStart, uint64_t aLength, StreamNeededResolver&& aResolver) {
  nsCOMPtr<nsIInputStream> stream;
  auto storage = RemoteLazyInputStreamStorage::Get().unwrapOr(nullptr);
  if (storage) {
    storage->GetStream(mID, aStart, aLength, getter_AddRefs(stream));
  }

  if (!stream) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Warning,
            ("Parent::RecvStreamNeeded not available! %s",
             nsIDToCString(mID).get()));
    aResolver(Nothing());
    return IPC_OK();
  }

  Maybe<IPCStream> ipcStream;
  if (NS_WARN_IF(!SerializeIPCStream(stream.forget(), ipcStream,
                                     /* aAllowLazy */ false))) {
    return IPC_FAIL(this, "IPCStream serialization failed!");
  }

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Parent::RecvStreamNeeded resolve %s", nsIDToCString(mID).get()));
  aResolver(ipcStream);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteLazyInputStreamParent::RecvLengthNeeded(
    LengthNeededResolver&& aResolver) {
  nsCOMPtr<nsIInputStream> stream;
  auto storage = RemoteLazyInputStreamStorage::Get().unwrapOr(nullptr);
  if (storage) {
    storage->GetStream(mID, 0, UINT64_MAX, getter_AddRefs(stream));
  }

  if (!stream) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Warning,
            ("Parent::RecvLengthNeeded not available! %s",
             nsIDToCString(mID).get()));
    aResolver(-1);
    return IPC_OK();
  }

  int64_t length = -1;
  if (InputStreamLengthHelper::GetSyncLength(stream, &length)) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Parent::RecvLengthNeeded sync resolve %" PRId64 "! %s", length,
             nsIDToCString(mID).get()));
    aResolver(length);
    return IPC_OK();
  }

  InputStreamLengthHelper::GetAsyncLength(
      stream, [aResolver, id = mID](int64_t aLength) {
        MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
                ("Parent::RecvLengthNeeded async resolve %" PRId64 "! %s",
                 aLength, nsIDToCString(id).get()));
        aResolver(aLength);
      });
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteLazyInputStreamParent::RecvGoodbye() {
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Parent::RecvGoodbye! %s", nsIDToCString(mID).get()));
  Close();
  return IPC_OK();
}

}  // namespace mozilla
