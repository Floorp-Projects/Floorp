/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_
#define DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_

#include "mozilla/dom/POriginPrivateFileSystemChild.h"
#include "mozilla/UniquePtr.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class OriginPrivateFileSystemChild {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void SendGetDirectoryHandle(
      const fs::FileSystemGetHandleRequest& request,
      mozilla::ipc::ResolveCallback<fs::FileSystemGetHandleResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) = 0;

  virtual void SendGetFileHandle(
      const fs::FileSystemGetHandleRequest& request,
      mozilla::ipc::ResolveCallback<fs::FileSystemGetHandleResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) = 0;

  virtual void SendGetFile(
      const fs::FileSystemGetFileRequest& request,
      mozilla::ipc::ResolveCallback<fs::FileSystemGetFileResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) = 0;

  virtual void SendResolve(
      const fs::FileSystemResolveRequest& request,
      mozilla::ipc::ResolveCallback<fs::FileSystemResolveResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) = 0;

  virtual void SendGetEntries(
      const fs::FileSystemGetEntriesRequest& request,
      mozilla::ipc::ResolveCallback<fs::FileSystemGetEntriesResponse>&&
          aResolve,
      mozilla::ipc::RejectCallback&& aReject) = 0;

  virtual void SendRemoveEntry(
      const fs::FileSystemRemoveEntryRequest& request,
      mozilla::ipc::ResolveCallback<fs::FileSystemRemoveEntryResponse>&&
          aResolve,
      mozilla::ipc::RejectCallback&& aReject) = 0;

  virtual void Close() = 0;

  virtual POriginPrivateFileSystemChild* AsBindable() = 0;

 protected:
  virtual ~OriginPrivateFileSystemChild() = default;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_
