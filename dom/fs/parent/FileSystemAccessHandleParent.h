/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMACCESSHANDLEPARENT_H_
#define DOM_FS_PARENT_FILESYSTEMACCESSHANDLEPARENT_H_

#include "mozilla/dom/PFileSystemAccessHandleParent.h"
#include "mozilla/dom/quota/DebugOnlyMacro.h"

namespace mozilla::dom {

class FileSystemAccessHandle;

class FileSystemAccessHandleParent : public PFileSystemAccessHandleParent {
 public:
  explicit FileSystemAccessHandleParent(
      RefPtr<FileSystemAccessHandle> aAccessHandle);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemAccessHandleParent, override)

  mozilla::ipc::IPCResult RecvClose();

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~FileSystemAccessHandleParent();

  RefPtr<FileSystemAccessHandle> mAccessHandle;

  DEBUGONLY(bool mActorDestroyed = false);
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMACCESSHANDLEPARENT_H_
