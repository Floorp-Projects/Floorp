/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundFileSystemParent.h"
#include "mozilla/dom/FileSystemTypes.h"

using IPCResult = mozilla::ipc::IPCResult;

namespace mozilla::dom {

IPCResult BackgroundFileSystemParent::RecvGetRoot(GetRootResolver&& aResolver) {
  FileSystemGetHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult BackgroundFileSystemParent::RecvGetDirectoryHandle(
    FileSystemGetHandleRequest&& /* aRequest */,
    GetDirectoryHandleResolver&& aResolver) {
  FileSystemGetHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult BackgroundFileSystemParent::RecvGetFileHandle(
    FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver) {
  FileSystemGetHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult BackgroundFileSystemParent::RecvGetFile(
    FileSystemGetFileRequest&& aRequest, GetFileResolver&& aResolver) {
  FileSystemGetFileResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult BackgroundFileSystemParent::RecvResolve(
    FileSystemResolveRequest&& aRequest, ResolveResolver&& aResolver) {
  FileSystemResolveResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult BackgroundFileSystemParent::RecvGetEntries(
    FileSystemGetEntriesRequest&& aRequest, GetEntriesResolver&& aResolver) {
  FileSystemGetEntriesResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult BackgroundFileSystemParent::RecvRemoveEntry(
    FileSystemRemoveEntryRequest&& aRequest, RemoveEntryResolver&& aResolver) {
  FileSystemRemoveEntryResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

}  // namespace mozilla::dom
