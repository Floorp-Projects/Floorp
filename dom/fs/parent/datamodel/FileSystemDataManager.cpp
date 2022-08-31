/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDataManager.h"

#include "mozilla/Result.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::fs::data {

namespace {

// nsCStringHashKey with disabled memmove
class nsCStringHashKeyDM : public nsCStringHashKey {
 public:
  explicit nsCStringHashKeyDM(const nsCStringHashKey::KeyTypePointer aKey)
      : nsCStringHashKey(aKey) {}
  enum { ALLOW_MEMMOVE = false };
};

// When CheckedUnsafePtr's checking is enabled, it's necessary to ensure that
// the hashtable uses the copy constructor instead of memmove for moving entries
// since memmove will break CheckedUnsafePtr in a memory-corrupting way.
using FileSystemDataManagerHashKey =
    std::conditional<DiagnosticAssertEnabled::value, nsCStringHashKeyDM,
                     nsCStringHashKey>::type;

// Raw (but checked when the diagnostic assert is enabled) references as we
// don't want to keep FileSystemDataManager objects alive forever. When a
// FileSystemDataManager is destroyed it calls RemoveFileSystemDataManager
// to clear itself.
using FileSystemDataManagerHashtable =
    nsBaseHashtable<FileSystemDataManagerHashKey,
                    NotNull<CheckedUnsafePtr<FileSystemDataManager>>,
                    MovingNotNull<CheckedUnsafePtr<FileSystemDataManager>>>;

StaticAutoPtr<FileSystemDataManagerHashtable> gDataManagers;

RefPtr<FileSystemDataManager> GetFileSystemDataManager(const Origin& aOrigin) {
  if (gDataManagers) {
    auto maybeDataManager = gDataManagers->MaybeGet(aOrigin);
    if (maybeDataManager) {
      RefPtr<FileSystemDataManager> result(
          std::move(*maybeDataManager).unwrapBasePtr());
      return result;
    }
  }

  return nullptr;
}

void AddFileSystemDataManager(
    const Origin& aOrigin, const RefPtr<FileSystemDataManager>& aDataManager) {
  if (!gDataManagers) {
    gDataManagers = new FileSystemDataManagerHashtable();
  }

  MOZ_ASSERT(!gDataManagers->Contains(aOrigin));
  gDataManagers->InsertOrUpdate(aOrigin,
                                WrapMovingNotNullUnchecked(aDataManager));
}

void RemoveFileSystemDataManager(const Origin& aOrigin) {
  MOZ_ASSERT(gDataManagers);
  const DebugOnly<bool> removed = gDataManagers->Remove(aOrigin);
  MOZ_ASSERT(removed);

  if (!gDataManagers->Count()) {
    gDataManagers = nullptr;
  }
}

}  // namespace

FileSystemDataManager::FileSystemDataManager(
    const Origin& aOrigin, MovingNotNull<RefPtr<TaskQueue>> aIOTaskQueue)
    : mOrigin(aOrigin),
      mBackgroundTarget(WrapNotNull(GetCurrentSerialEventTarget())),
      mIOTaskQueue(std::move(aIOTaskQueue)),
      mRegCount(0),
      mState(State::Initial) {}

FileSystemDataManager::~FileSystemDataManager() {
  MOZ_ASSERT(mState == State::Closed);
}

FileSystemDataManager::result_t
FileSystemDataManager::GetOrCreateFileSystemDataManager(const Origin& aOrigin) {
  if (RefPtr<FileSystemDataManager> dataManager =
          GetFileSystemDataManager(aOrigin)) {
    return Registered<FileSystemDataManager>(std::move(dataManager));
  }

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID));

  nsCString taskQueueName("OPFS "_ns + aOrigin);

  RefPtr<TaskQueue> ioTaskQueue =
      TaskQueue::Create(streamTransportService.forget(), taskQueueName.get());

  auto dataManager = MakeRefPtr<FileSystemDataManager>(
      aOrigin, WrapMovingNotNull(ioTaskQueue));

  AddFileSystemDataManager(aOrigin, dataManager);

  return Registered<FileSystemDataManager>(std::move(dataManager));
}

void FileSystemDataManager::Register() { mRegCount++; }

void FileSystemDataManager::Unregister() {
  MOZ_ASSERT(mRegCount > 0);

  mRegCount--;

  if (IsInactive()) {
    Close();
  }
}

void FileSystemDataManager::RegisterActor(
    NotNull<FileSystemManagerParent*> aActor) {
  MOZ_ASSERT(!mActors.Contains(aActor));

  mActors.Insert(aActor);
}

void FileSystemDataManager::UnregisterActor(
    NotNull<FileSystemManagerParent*> aActor) {
  MOZ_ASSERT(mActors.Contains(aActor));

  mActors.Remove(aActor);

  if (IsInactive()) {
    Close();
  }
}

bool FileSystemDataManager::IsInactive() const {
  return !mRegCount && !mActors.Count();
}

void FileSystemDataManager::Close() {
  MOZ_ASSERT(mState == State::Initial);
  MOZ_ASSERT(IsInactive());

  mState = State::Closed;

  mIOTaskQueue->BeginShutdown();

  RemoveFileSystemDataManager(mOrigin);
}

}  // namespace mozilla::dom::fs::data
