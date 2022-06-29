/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_BACKGROUNDFILESYSTEMPARENT_H_
#define DOM_FS_PARENT_BACKGROUNDFILESYSTEMPARENT_H_

#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/dom/PBackgroundFileSystemParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsISupports.h"

namespace mozilla::dom {

class BackgroundFileSystemParent : public PBackgroundFileSystemParent {
 public:
  explicit BackgroundFileSystemParent(
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo)
      : mPrincipalInfo(aPrincipalInfo) {}

  mozilla::ipc::IPCResult RecvGetRoot(GetRootResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetDirectoryHandle(
      FileSystemGetHandleRequest&& aRequest,
      GetDirectoryHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFileHandle(
      FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFile(FileSystemGetFileRequest&& aRequest,
                                      GetFileResolver&& aResolver);

  mozilla::ipc::IPCResult RecvResolve(FileSystemResolveRequest&& aRequest,
                                      ResolveResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetEntries(FileSystemGetEntriesRequest&& aRequest,
                                         GetEntriesResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRemoveEntry(
      FileSystemRemoveEntryRequest&& aRequest, RemoveEntryResolver&& aResolver);

  NS_INLINE_DECL_REFCOUNTING(BackgroundFileSystemParent)

 protected:
  virtual ~BackgroundFileSystemParent() = default;

 private:
  mozilla::ipc::PrincipalInfo mPrincipalInfo;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_BACKGROUNDFILESYSTEMPARENT_H_
