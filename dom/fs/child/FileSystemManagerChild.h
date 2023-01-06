/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMMANAGERCHILD_H_
#define DOM_FS_CHILD_FILESYSTEMMANAGERCHILD_H_

#include "mozilla/dom/PFileSystemManagerChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class FileSystemManagerChild : public PFileSystemManagerChild {
 public:
  NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(FileSystemManagerChild, Destroy())

#ifdef DEBUG
  virtual bool AllSyncAccessHandlesClosed() const;
#endif

  virtual void CloseAllWritableFileStreams();

  virtual void Shutdown();

  already_AddRefed<PFileSystemAccessHandleChild>
  AllocPFileSystemAccessHandleChild();

  already_AddRefed<PFileSystemWritableFileStreamChild>
  AllocPFileSystemWritableFileStreamChild();

  ::mozilla::ipc::IPCResult RecvCloseAll(CloseAllResolver&& aResolver);

 protected:
  virtual ~FileSystemManagerChild() = default;

  virtual void Destroy() {
    Shutdown();
    delete this;
  }
};

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_FILESYSTEMMANAGERCHILD_H_
