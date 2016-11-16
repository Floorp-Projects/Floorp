/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_filehandle_ActorsParent_h
#define mozilla_dom_filehandle_ActorsParent_h

#include "mozilla/dom/FileHandleStorage.h"
#include "mozilla/dom/PBackgroundMutableFileParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTHashtable.h"

template <class> struct already_AddRefed;
class nsIFile;
class nsIRunnable;
class nsIThreadPool;

namespace mozilla {

namespace ipc {

class PBackgroundParent;

} // namespace ipc

namespace dom {

class BlobImpl;
class FileHandle;
class FileHandleOp;

class FileHandleThreadPool final
{
  class FileHandleQueue;
  struct DelayedEnqueueInfo;
  class DirectoryInfo;
  struct StoragesCompleteCallback;

  nsCOMPtr<nsIThreadPool> mThreadPool;
  nsCOMPtr<nsIEventTarget> mOwningThread;

  nsClassHashtable<nsCStringHashKey, DirectoryInfo> mDirectoryInfos;

  nsTArray<nsAutoPtr<StoragesCompleteCallback>> mCompleteCallbacks;

  bool mShutdownRequested;
  bool mShutdownComplete;

public:
  static already_AddRefed<FileHandleThreadPool>
  Create();

#ifdef DEBUG
  void
  AssertIsOnOwningThread() const;

  nsIEventTarget*
  GetThreadPoolEventTarget() const;
#else
  void
  AssertIsOnOwningThread() const
  { }
#endif

  void
  Enqueue(FileHandle* aFileHandle,
          FileHandleOp* aFileHandleOp,
          bool aFinish);

  NS_INLINE_DECL_REFCOUNTING(FileHandleThreadPool)

  void
  WaitForDirectoriesToComplete(nsTArray<nsCString>&& aDirectoryIds,
                               nsIRunnable* aCallback);

  void
  Shutdown();

private:
  FileHandleThreadPool();

  // Reference counted.
  ~FileHandleThreadPool();

  nsresult
  Init();

  void
  Cleanup();

  void
  FinishFileHandle(FileHandle* aFileHandle);

  bool
  MaybeFireCallback(StoragesCompleteCallback* aCallback);
};

class BackgroundMutableFileParentBase
  : public PBackgroundMutableFileParent
{
  nsTHashtable<nsPtrHashKey<FileHandle>> mFileHandles;
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

  void
  Invalidate();

  FileHandleStorage
  Storage() const
  {
    return mStorage;
  }

  const nsCString&
  DirectoryId() const
  {
    return mDirectoryId;
  }

  const nsString&
  FileName() const
  {
    return mFileName;
  }

  bool
  RegisterFileHandle(FileHandle* aFileHandle);

  void
  UnregisterFileHandle(FileHandle* aFileHandle);

  void
  SetActorAlive();

  bool
  IsActorDestroyed() const
  {
    mozilla::ipc::AssertIsOnBackgroundThread();

    return mActorWasAlive && mActorDestroyed;
  }

  bool
  IsInvalidated() const
  {
    mozilla::ipc::AssertIsOnBackgroundThread();

    return mInvalidated;
  }

  virtual void
  NoteActiveState()
  { }

  virtual void
  NoteInactiveState()
  { }

  virtual mozilla::ipc::PBackgroundParent*
  GetBackgroundParent() const = 0;

  virtual already_AddRefed<nsISupports>
  CreateStream(bool aReadOnly);

  virtual already_AddRefed<BlobImpl>
  CreateBlobImpl()
  {
    return nullptr;
  }

protected:
  BackgroundMutableFileParentBase(FileHandleStorage aStorage,
                                  const nsACString& aDirectoryId,
                                  const nsAString& aFileName,
                                  nsIFile* aFile);

  // Reference counted.
  ~BackgroundMutableFileParentBase();

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PBackgroundFileHandleParent*
  AllocPBackgroundFileHandleParent(const FileMode& aMode) override;

  virtual mozilla::ipc::IPCResult
  RecvPBackgroundFileHandleConstructor(PBackgroundFileHandleParent* aActor,
                                       const FileMode& aMode) override;

  virtual bool
  DeallocPBackgroundFileHandleParent(PBackgroundFileHandleParent* aActor)
                                     override;

  virtual mozilla::ipc::IPCResult
  RecvDeleteMe() override;

  virtual mozilla::ipc::IPCResult
  RecvGetFileId(int64_t* aFileId) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_filehandle_ActorsParent_h
