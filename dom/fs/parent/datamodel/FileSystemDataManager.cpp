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
#include "nsProxyRelease.h"
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

RefPtr<FileSystemDataManager::CreatePromise>
FileSystemDataManager::GetOrCreateFileSystemDataManager(const Origin& aOrigin) {
  if (RefPtr<FileSystemDataManager> dataManager =
          GetFileSystemDataManager(aOrigin)) {
    // XXX Handle the case when the manager is asynchronouslly opening!

    // XXX Handle the case when the manager is asynchronouslly closing!

    return CreatePromise::CreateAndResolve(
        Registered<FileSystemDataManager>(std::move(dataManager)), __func__);
  }

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID),
                CreatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__));

  nsCString taskQueueName("OPFS "_ns + aOrigin);

  RefPtr<TaskQueue> ioTaskQueue =
      TaskQueue::Create(streamTransportService.forget(), taskQueueName.get());

  auto dataManager = MakeRefPtr<FileSystemDataManager>(
      aOrigin, WrapMovingNotNull(ioTaskQueue));

  AddFileSystemDataManager(aOrigin, dataManager);

  return dataManager->BeginOpen()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [dataManager = Registered<FileSystemDataManager>(dataManager)](
          const BoolPromise::ResolveOrRejectValue&) {
        return CreatePromise::CreateAndResolve(dataManager, __func__);
      });
}

void FileSystemDataManager::Register() { mRegCount++; }

void FileSystemDataManager::Unregister() {
  MOZ_ASSERT(mRegCount > 0);

  mRegCount--;

  if (IsInactive()) {
    BeginClose();
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
    BeginClose();
  }
}

RefPtr<BoolPromise> FileSystemDataManager::OnOpen() {
  MOZ_ASSERT(mState == State::Opening);

  return mOpenPromiseHolder.Ensure(__func__);
}

RefPtr<BoolPromise> FileSystemDataManager::OnClose() {
  MOZ_ASSERT(mState == State::Closing);

  return mClosePromiseHolder.Ensure(__func__);
}

bool FileSystemDataManager::IsInactive() const {
  return !mRegCount && !mActors.Count();
}

RefPtr<BoolPromise> FileSystemDataManager::BeginOpen() {
  MOZ_ASSERT(mState == State::Initial);

  mState = State::Opening;

  InvokeAsync(MutableIOTargetPtr(), __func__,
              [self = RefPtr<FileSystemDataManager>(this)]() mutable {
                nsCOMPtr<nsISerialEventTarget> target =
                    self->MutableBackgroundTargetPtr();

                NS_ProxyRelease("ReleaseFileSystemDataManager", target,
                                self.forget());

                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const BoolPromise::ResolveOrRejectValue& value) {
               if (value.IsReject()) {
                 self->mState = State::Initial;

                 self->mOpenPromiseHolder.RejectIfExists(value.RejectValue(),
                                                         __func__);

               } else {
                 self->mState = State::Open;

                 self->mOpenPromiseHolder.ResolveIfExists(true, __func__);
               }
             });

  return OnOpen();
}

RefPtr<BoolPromise> FileSystemDataManager::BeginClose() {
  MOZ_ASSERT(mState == State::Open);
  MOZ_ASSERT(IsInactive());

  mState = State::Closing;

  InvokeAsync(MutableIOTargetPtr(), __func__,
              []() { return BoolPromise::CreateAndResolve(true, __func__); })
      ->Then(MutableBackgroundTargetPtr(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const BoolPromise::ResolveOrRejectValue&) {
               return self->mIOTaskQueue->BeginShutdown();
             })
      ->Then(MutableBackgroundTargetPtr(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const ShutdownPromise::ResolveOrRejectValue&) {
               RemoveFileSystemDataManager(self->mOrigin);

               self->mState = State::Closed;

               self->mClosePromiseHolder.ResolveIfExists(true, __func__);
             });

  return OnClose();
}

}  // namespace mozilla::dom::fs::data
