/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMMANAGERCHILD_H_
#define DOM_FS_CHILD_FILESYSTEMMANAGERCHILD_H_

#include "mozilla/dom/FileSystemWritableFileStreamChild.h"
#include "mozilla/dom/PFileSystemManagerChild.h"
#include "mozilla/dom/quota/ForwardDecls.h"

namespace mozilla::dom {

class FileSystemBackgroundRequestHandler;

class FileSystemManagerChild : public PFileSystemManagerChild {
 public:
  NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(FileSystemManagerChild, Destroy(),
                                          override)

  void SetBackgroundRequestHandler(
      FileSystemBackgroundRequestHandler* aBackgroundRequestHandler);

  void CloseAllWritables(std::function<void()>&& aCallback);

#ifdef DEBUG
  virtual bool AllSyncAccessHandlesClosed() const;

  virtual bool AllWritableFileStreamsClosed() const;
#endif

  virtual void Shutdown();

  already_AddRefed<PFileSystemWritableFileStreamChild>
  AllocPFileSystemWritableFileStreamChild();

  ::mozilla::ipc::IPCResult RecvCloseAll(CloseAllResolver&& aResolver);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~FileSystemManagerChild() = default;

  virtual void Destroy() {
    Shutdown();
    delete this;
  }

  // The weak reference is cleared in ActorDestroy.
  FileSystemBackgroundRequestHandler* MOZ_NON_OWNING_REF
      mBackgroundRequestHandler;

 private:
  template <class T>
  void CloseAllWritablesImpl(T& aPromises);
};

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_FILESYSTEMMANAGERCHILD_H_
