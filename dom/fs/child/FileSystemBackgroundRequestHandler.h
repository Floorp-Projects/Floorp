/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMBACKGROUNDREQUESTHANDLER_H_
#define DOM_FS_CHILD_FILESYSTEMBACKGROUNDREQUESTHANDLER_H_

#include "mozilla/MozPromise.h"
#include "mozilla/UniquePtr.h"

class nsIGlobalObject;

template <class T>
class RefPtr;

namespace mozilla::dom {

class OriginPrivateFileSystemChild;

namespace fs {
class FileSystemChildFactory;
}

class FileSystemBackgroundRequestHandler {
 public:
  explicit FileSystemBackgroundRequestHandler(
      fs::FileSystemChildFactory* aChildFactory);

  FileSystemBackgroundRequestHandler();

  using CreateFileSystemManagerChildPromise =
      MozPromise<RefPtr<OriginPrivateFileSystemChild>, nsresult, false>;

  virtual RefPtr<CreateFileSystemManagerChildPromise>
  CreateFileSystemManagerChild(nsIGlobalObject* aGlobal);

  virtual ~FileSystemBackgroundRequestHandler();

 protected:
  const UniquePtr<fs::FileSystemChildFactory> mChildFactory;
};  // class FileSystemBackgroundRequestHandler

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_FILESYSTEMBACKGROUNDREQUESTHANDLER_H_
