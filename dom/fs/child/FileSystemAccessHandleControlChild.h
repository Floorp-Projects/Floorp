/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMACCESSHANDLECONTROLCHILD_H_
#define DOM_FS_CHILD_FILESYSTEMACCESSHANDLECONTROLCHILD_H_

#include "mozilla/dom/PFileSystemAccessHandleControlChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class FileSystemSyncAccessHandle;

class FileSystemAccessHandleControlChild
    : public PFileSystemAccessHandleControlChild {
 public:
  NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(FileSystemAccessHandleControlChild,
                                          Destroy(), override)

  void SetAccessHandle(FileSystemSyncAccessHandle* aAccessHandle);

  void Shutdown();

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~FileSystemAccessHandleControlChild() = default;

  // This is called when the object's refcount drops to zero. We use this custom
  // callback to close the top level actor if it hasn't been explicitly closed
  // yet. For example when `FileSystemSyncAccessHandle::Create` fails after
  // creating and binding the top level actor.
  virtual void Destroy() {
    Shutdown();
    delete this;
  }

  // The weak reference is cleared in ActorDestroy.
  FileSystemSyncAccessHandle* MOZ_NON_OWNING_REF mAccessHandle;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_FILESYSTEMACCESSHANDLECONTROLCHILD_H_
