/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_
#define DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_

#include "mozilla/dom/POriginPrivateFileSystemChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

// XXX This can be greatly simplified if bug 1786484 gets fixed.
// See: https://phabricator.services.mozilla.com/D155351
//      https://phabricator.services.mozilla.com/D155352

class OriginPrivateFileSystemChild : public POriginPrivateFileSystemChild {
 public:
  NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(OriginPrivateFileSystemChild,
                                          Destroy())

  virtual void SendGetRootHandle(
      mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) {
    POriginPrivateFileSystemChild::SendGetRootHandleMsg(
        std::forward<
            mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>>(
            aResolve),
        std::forward<mozilla::ipc::RejectCallback>(aReject));
  }

  virtual void SendGetDirectoryHandle(
      const FileSystemGetHandleRequest& aRequest,
      mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) {
    POriginPrivateFileSystemChild::SendGetDirectoryHandleMsg(
        aRequest,
        std::forward<
            mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>>(
            aResolve),
        std::forward<mozilla::ipc::RejectCallback>(aReject));
  }

  virtual void SendGetFileHandle(
      const FileSystemGetHandleRequest& aRequest,
      mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) {
    POriginPrivateFileSystemChild::SendGetFileHandleMsg(
        aRequest,
        std::forward<
            mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>>(
            aResolve),
        std::forward<mozilla::ipc::RejectCallback>(aReject));
  }

  virtual void SendGetFile(
      const FileSystemGetFileRequest& aRequest,
      mozilla::ipc::ResolveCallback<FileSystemGetFileResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) {
    POriginPrivateFileSystemChild::SendGetFileMsg(
        aRequest,
        std::forward<mozilla::ipc::ResolveCallback<FileSystemGetFileResponse>>(
            aResolve),
        std::forward<mozilla::ipc::RejectCallback>(aReject));
  }

  virtual void SendResolve(
      const FileSystemResolveRequest& aRequest,
      mozilla::ipc::ResolveCallback<FileSystemResolveResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) {
    POriginPrivateFileSystemChild::SendResolveMsg(
        aRequest,
        std::forward<mozilla::ipc::ResolveCallback<FileSystemResolveResponse>>(
            aResolve),
        std::forward<mozilla::ipc::RejectCallback>(aReject));
  }

  virtual void SendGetEntries(
      const FileSystemGetEntriesRequest& aRequest,
      mozilla::ipc::ResolveCallback<FileSystemGetEntriesResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) {
    POriginPrivateFileSystemChild::SendGetEntriesMsg(
        aRequest,
        std::forward<
            mozilla::ipc::ResolveCallback<FileSystemGetEntriesResponse>>(
            aResolve),
        std::forward<mozilla::ipc::RejectCallback>(aReject));
  }

  virtual void SendRemoveEntry(
      const FileSystemRemoveEntryRequest& aRequest,
      mozilla::ipc::ResolveCallback<FileSystemRemoveEntryResponse>&& aResolve,
      mozilla::ipc::RejectCallback&& aReject) {
    POriginPrivateFileSystemChild::SendRemoveEntryMsg(
        aRequest,
        std::forward<
            mozilla::ipc::ResolveCallback<FileSystemRemoveEntryResponse>>(
            aResolve),
        std::forward<mozilla::ipc::RejectCallback>(aReject));
  }

  virtual void Shutdown() {
    if (CanSend()) {
      Close();
    }
  }

 protected:
  virtual ~OriginPrivateFileSystemChild() = default;

  virtual void Destroy() {
    Shutdown();
    delete this;
  }

 private:
  using POriginPrivateFileSystemChild::SendGetRootHandleMsg;

  using POriginPrivateFileSystemChild::SendGetDirectoryHandleMsg;

  using POriginPrivateFileSystemChild::SendGetFileHandleMsg;

  using POriginPrivateFileSystemChild::SendGetFileMsg;

  using POriginPrivateFileSystemChild::SendResolveMsg;

  using POriginPrivateFileSystemChild::SendGetEntriesMsg;

  using POriginPrivateFileSystemChild::SendRemoveEntryMsg;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_
