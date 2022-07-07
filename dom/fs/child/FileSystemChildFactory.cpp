/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fs/FileSystemChildFactory.h"
#include "mozilla/dom/POriginPrivateFileSystemChild.h"
#include "mozilla/dom/OriginPrivateFileSystemChild.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::fs {

namespace {

using ResolveGetHandle =
    mozilla::ipc::ResolveCallback<fs::FileSystemGetHandleResponse>;
using ResolveGetFile =
    mozilla::ipc::ResolveCallback<fs::FileSystemGetFileResponse>;
using ResolveResolve =
    mozilla::ipc::ResolveCallback<fs::FileSystemResolveResponse>;
using ResolveGetEntries =
    mozilla::ipc::ResolveCallback<fs::FileSystemGetEntriesResponse>;
using ResolveRemoveEntry =
    mozilla::ipc::ResolveCallback<fs::FileSystemRemoveEntryResponse>;
using DoReject = mozilla::ipc::RejectCallback;

class OriginPrivateFileSystemChildImpl final
    : public OriginPrivateFileSystemChild {
 public:
  class TOriginPrivateFileSystem final : public POriginPrivateFileSystemChild {
    NS_INLINE_DECL_REFCOUNTING(TOriginPrivateFileSystem);

   protected:
    ~TOriginPrivateFileSystem() = default;
  };

  OriginPrivateFileSystemChildImpl() : mImpl(new TOriginPrivateFileSystem()) {}

  NS_INLINE_DECL_REFCOUNTING(OriginPrivateFileSystemChildImpl, override)

  void SendGetDirectoryHandle(const fs::FileSystemGetHandleRequest& aRequest,
                              ResolveGetHandle&& aResolve,
                              DoReject&& aReject) override {
    mImpl->SendGetDirectoryHandle(aRequest,
                                  std::forward<ResolveGetHandle>(aResolve),
                                  std::forward<DoReject>(aReject));
  }

  void SendGetFileHandle(const fs::FileSystemGetHandleRequest& aRequest,
                         ResolveGetHandle&& aResolve,
                         DoReject&& aReject) override {
    mImpl->SendGetFileHandle(aRequest, std::forward<ResolveGetHandle>(aResolve),
                             std::forward<DoReject>(aReject));
  }

  void SendGetFile(const fs::FileSystemGetFileRequest& aRequest,
                   ResolveGetFile&& aResolve, DoReject&& aReject) override {
    mImpl->SendGetFile(aRequest, std::forward<ResolveGetFile>(aResolve),
                       std::forward<DoReject>(aReject));
  }

  void SendResolve(const fs::FileSystemResolveRequest& aRequest,
                   ResolveResolve&& aResolve, DoReject&& aReject) override {
    mImpl->SendResolve(aRequest, std::forward<ResolveResolve>(aResolve),
                       std::forward<DoReject>(aReject));
  }

  void SendGetEntries(const fs::FileSystemGetEntriesRequest& aRequest,
                      ResolveGetEntries&& aResolve,
                      DoReject&& aReject) override {
    mImpl->SendGetEntries(aRequest, std::forward<ResolveGetEntries>(aResolve),
                          std::forward<DoReject>(aReject));
  }

  void SendRemoveEntry(const fs::FileSystemRemoveEntryRequest& aRequest,
                       ResolveRemoveEntry&& aResolve,
                       DoReject&& aReject) override {
    mImpl->SendRemoveEntry(aRequest, std::forward<ResolveRemoveEntry>(aResolve),
                           std::forward<DoReject>(aReject));
  }

  void Close() override { mImpl->Close(); }

  POriginPrivateFileSystemChild* AsBindable() override { return mImpl.get(); };

 protected:
  ~OriginPrivateFileSystemChildImpl() = default;

  const RefPtr<TOriginPrivateFileSystem> mImpl;
};  // class OriginPrivateFileSystemChildImpl

}  // namespace

already_AddRefed<OriginPrivateFileSystemChild> FileSystemChildFactory::Create()
    const {
  return MakeAndAddRef<OriginPrivateFileSystemChildImpl>();
}

}  // namespace mozilla::dom::fs
