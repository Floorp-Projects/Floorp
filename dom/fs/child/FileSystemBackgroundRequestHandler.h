/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMBACKGROUNDREQUESTHANDLER_H_
#define DOM_FS_CHILD_FILESYSTEMBACKGROUNDREQUESTHANDLER_H_

#include "mozilla/MozPromise.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/quota/ForwardDecls.h"

template <class T>
class RefPtr;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

namespace dom {

class FileSystemManagerChild;

namespace fs {
class FileSystemChildFactory;
}

class FileSystemBackgroundRequestHandler {
 public:
  explicit FileSystemBackgroundRequestHandler(
      fs::FileSystemChildFactory* aChildFactory);

  explicit FileSystemBackgroundRequestHandler(
      RefPtr<FileSystemManagerChild> aFileSystemManagerChild);

  FileSystemBackgroundRequestHandler();

  NS_INLINE_DECL_REFCOUNTING(FileSystemBackgroundRequestHandler)

  void Shutdown();

  FileSystemManagerChild* GetFileSystemManagerChild() const;

  virtual RefPtr<mozilla::BoolPromise> CreateFileSystemManagerChild(
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

 protected:
  virtual ~FileSystemBackgroundRequestHandler();

  const UniquePtr<fs::FileSystemChildFactory> mChildFactory;

  MozPromiseHolder<BoolPromise> mCreateFileSystemManagerChildPromiseHolder;

  RefPtr<FileSystemManagerChild> mFileSystemManagerChild;

  bool mCreatingFileSystemManagerChild;
};  // class FileSystemBackgroundRequestHandler

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_CHILD_FILESYSTEMBACKGROUNDREQUESTHANDLER_H_
