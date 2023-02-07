/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMACCESSHANDLECONTROLPARENT_H_
#define DOM_FS_PARENT_FILESYSTEMACCESSHANDLECONTROLPARENT_H_

#include "mozilla/dom/PFileSystemAccessHandleControlParent.h"
#include "nsISupportsUtils.h"

namespace mozilla::dom {

class FileSystemAccessHandle;

class FileSystemAccessHandleControlParent
    : public PFileSystemAccessHandleControlParent {
 public:
  explicit FileSystemAccessHandleControlParent(
      RefPtr<FileSystemAccessHandle> aAccessHandle);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemAccessHandleControlParent,
                                        override)

  mozilla::ipc::IPCResult RecvClose(CloseResolver&& aResolver);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~FileSystemAccessHandleControlParent();

 private:
  RefPtr<FileSystemAccessHandle> mAccessHandle;

#ifdef DEBUG
  bool mActorDestroyed = false;
#endif
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMACCESSHANDLECONTROLPARENT_H_
