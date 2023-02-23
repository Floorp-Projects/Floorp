/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_filehandle_ActorsParent_h
#define mozilla_dom_filehandle_ActorsParent_h

#include "mozilla/dom/FileHandleStorage.h"
#include "mozilla/dom/PBackgroundMutableFileParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTHashSet.h"

template <class>
struct already_AddRefed;
class nsIFile;
class nsIRunnable;
class nsIThreadPool;

namespace mozilla {

namespace ipc {

class PBackgroundParent;

}  // namespace ipc

namespace dom {

class BlobImpl;
class FileHandle;

class BackgroundMutableFileParentBase : public PBackgroundMutableFileParent {
  friend PBackgroundMutableFileParent;

  nsCString mDirectoryId;
  nsString mFileName;
  FileHandleStorage mStorage;
  bool mInvalidated;
  bool mActorWasAlive;
  bool mActorDestroyed;

 protected:
  nsCOMPtr<nsIFile> mFile;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BackgroundMutableFileParentBase)

  void Invalidate();

  FileHandleStorage Storage() const { return mStorage; }

  const nsCString& DirectoryId() const { return mDirectoryId; }

  const nsString& FileName() const { return mFileName; }

  void SetActorAlive();

  bool IsActorDestroyed() const {
    mozilla::ipc::AssertIsOnBackgroundThread();

    return mActorWasAlive && mActorDestroyed;
  }

  bool IsInvalidated() const {
    mozilla::ipc::AssertIsOnBackgroundThread();

    return mInvalidated;
  }

  virtual void NoteActiveState() {}

  virtual void NoteInactiveState() {}

  virtual mozilla::ipc::PBackgroundParent* GetBackgroundParent() const = 0;

  virtual already_AddRefed<nsISupports> CreateStream(bool aReadOnly);

  virtual already_AddRefed<BlobImpl> CreateBlobImpl() { return nullptr; }

 protected:
  BackgroundMutableFileParentBase(FileHandleStorage aStorage,
                                  const nsACString& aDirectoryId,
                                  const nsAString& aFileName, nsIFile* aFile);

  // Reference counted.
  ~BackgroundMutableFileParentBase();

  // IPDL methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe();

  virtual mozilla::ipc::IPCResult RecvGetFileId(int64_t* aFileId);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_filehandle_ActorsParent_h
