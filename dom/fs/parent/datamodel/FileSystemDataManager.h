/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_
#define DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_

#include "mozilla/NotNull.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/FileSystemHelpers.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
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

namespace fs::data {

class FileSystemDataManager
    : public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
 public:
  enum struct State : uint8_t { Initial = 0, Closing, Closed };

  FileSystemDataManager(const Origin& aOrigin,
                        MovingNotNull<RefPtr<TaskQueue>> aIOTaskQueue);

  using CreatePromise = MozPromise<Registered<FileSystemDataManager>, nsresult,
                                   /* IsExclusive */ true>;
  static RefPtr<CreatePromise> GetOrCreateFileSystemDataManager(
      const Origin& aOrigin);

  NS_INLINE_DECL_REFCOUNTING(FileSystemDataManager)

  nsISerialEventTarget* MutableBackgroundTargetPtr() const {
    return mBackgroundTarget.get();
  }

  nsISerialEventTarget* MutableIOTargetPtr() const {
    return mIOTaskQueue.get();
  }

  void Register();

  void Unregister();

  void RegisterActor(NotNull<FileSystemManagerParent*> aActor);

  void UnregisterActor(NotNull<FileSystemManagerParent*> aActor);

  RefPtr<BoolPromise> OnClose();

 protected:
  ~FileSystemDataManager();

  bool IsInactive() const;

  RefPtr<BoolPromise> BeginClose();

  nsTHashSet<FileSystemManagerParent*> mActors;
  const Origin mOrigin;
  const NotNull<nsCOMPtr<nsISerialEventTarget>> mBackgroundTarget;
  const NotNull<RefPtr<TaskQueue>> mIOTaskQueue;
  MozPromiseHolder<BoolPromise> mClosePromiseHolder;
  uint32_t mRegCount;
  State mState;
};

}  // namespace fs::data
}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_
