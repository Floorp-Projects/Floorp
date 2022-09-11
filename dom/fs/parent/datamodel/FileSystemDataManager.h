/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_
#define DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_

#include "mozilla/NotNull.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/ThreadBound.h"
#include "mozilla/dom/FileSystemHelpers.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "nsCOMPtr.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsTHashSet.h"

namespace mozilla {

template <typename V, typename E>
class Result;

namespace dom {

class FileSystemManagerParent;

namespace fs {
class FileSystemChildMetadata;
}  // namespace fs

namespace quota {
class DirectoryLock;
}  // namespace quota

namespace fs::data {

class FileSystemDatabaseManager;

Result<EntryId, QMResult> GetRootHandle(const Origin& origin);

Result<EntryId, QMResult> GetEntryHandle(
    const FileSystemChildMetadata& aHandle);

class FileSystemDataManager
    : public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
 public:
  enum struct State : uint8_t { Initial = 0, Opening, Open, Closing, Closed };

  FileSystemDataManager(const quota::OriginMetadata& aOriginMetadata,
                        MovingNotNull<nsCOMPtr<nsIEventTarget>> aIOTarget,
                        MovingNotNull<RefPtr<TaskQueue>> aIOTaskQueue);

  // IsExclusive is true because we want to allow the move operations. There's
  // always just one consumer anyway.
  using CreatePromise = MozPromise<Registered<FileSystemDataManager>, nsresult,
                                   /* IsExclusive */ true>;
  static RefPtr<CreatePromise> GetOrCreateFileSystemDataManager(
      const quota::OriginMetadata& aOriginMetadata);

  static void AbortOperationsForLocks(
      const quota::Client::DirectoryLockIdTable& aDirectoryLockIds);

  static void InitiateShutdown();

  static bool IsShutdownCompleted();

  NS_INLINE_DECL_REFCOUNTING(FileSystemDataManager)

  void AssertIsOnIOTarget() const;

  nsISerialEventTarget* MutableBackgroundTargetPtr() const {
    return mBackgroundTarget.get();
  }

  nsISerialEventTarget* MutableIOTargetPtr() const {
    return mIOTaskQueue.get();
  }

  Maybe<quota::DirectoryLock&> MaybeDirectoryLockRef() const {
    return ToMaybeRef(mDirectoryLock.get());
  }

  FileSystemDatabaseManager* MutableDatabaseManagerPtr() const {
    MOZ_ASSERT(mDatabaseManager);

    return mDatabaseManager.get();
  }

  void Register();

  void Unregister();

  void RegisterActor(NotNull<FileSystemManagerParent*> aActor);

  void UnregisterActor(NotNull<FileSystemManagerParent*> aActor);

  bool IsOpen() const { return mState == State::Open; }

  RefPtr<BoolPromise> OnOpen();

  RefPtr<BoolPromise> OnClose();

  bool LockExclusive(const EntryId& aEntryId);

  void UnlockExclusive(const EntryId& aEntryId);

 protected:
  ~FileSystemDataManager();

  bool IsInactive() const;

  bool IsOpening() const { return mState == State::Opening; }

  bool IsClosing() const { return mState == State::Closing; }

  void RequestAllowToClose();

  RefPtr<BoolPromise> BeginOpen();

  RefPtr<BoolPromise> BeginClose();

  // Things touched on background thread only.
  struct BackgroundThreadAccessible {
    nsTHashSet<FileSystemManagerParent*> mActors;
  };
  ThreadBound<BackgroundThreadAccessible> mBackgroundThreadAccessible;

  const quota::OriginMetadata mOriginMetadata;
  nsTHashSet<EntryId> mExclusiveLocks;
  const NotNull<nsCOMPtr<nsISerialEventTarget>> mBackgroundTarget;
  const NotNull<nsCOMPtr<nsIEventTarget>> mIOTarget;
  const NotNull<RefPtr<TaskQueue>> mIOTaskQueue;
  RefPtr<quota::DirectoryLock> mDirectoryLock;
  UniquePtr<FileSystemDatabaseManager> mDatabaseManager;
  MozPromiseHolder<BoolPromise> mOpenPromiseHolder;
  MozPromiseHolder<BoolPromise> mClosePromiseHolder;
  uint32_t mRegCount;
  State mState;
};

}  // namespace fs::data
}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_
