/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SlicedInputStream.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsIPropertyBag2.h"
#include "nsStreamUtils.h"
#include "RemoteLazyInputStreamParent.h"
#include "RemoteLazyInputStreamStorage.h"

namespace mozilla {

using namespace hal;

extern mozilla::LazyLogModule gRemoteLazyStreamLog;

namespace {
StaticMutex gMutex;
StaticRefPtr<RemoteLazyInputStreamStorage> gStorage;
}  // namespace

NS_INTERFACE_MAP_BEGIN(RemoteLazyInputStreamStorage)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(RemoteLazyInputStreamStorage)
NS_IMPL_RELEASE(RemoteLazyInputStreamStorage)

/* static */
Result<RefPtr<RemoteLazyInputStreamStorage>, nsresult>
RemoteLazyInputStreamStorage::Get() {
  mozilla::StaticMutexAutoLock lock(gMutex);
  if (gStorage) {
    RefPtr<RemoteLazyInputStreamStorage> storage = gStorage;
    return storage;
  }

  return Err(NS_ERROR_NOT_INITIALIZED);
}

/* static */
void RemoteLazyInputStreamStorage::Initialize() {
  mozilla::StaticMutexAutoLock lock(gMutex);
  MOZ_ASSERT(!gStorage);

  gStorage = new RemoteLazyInputStreamStorage();

  MOZ_ALWAYS_SUCCEEDS(NS_CreateBackgroundTaskQueue(
      "RemoteLazyInputStreamStorage", getter_AddRefs(gStorage->mTaskQueue)));

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(gStorage, "xpcom-shutdown", false);
  }
}

NS_IMETHODIMP
RemoteLazyInputStreamStorage::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "xpcom-shutdown");
  }

  mozilla::StaticMutexAutoLock lock(gMutex);
  gStorage = nullptr;
  return NS_OK;
}

void RemoteLazyInputStreamStorage::AddStream(nsIInputStream* aInputStream,
                                             const nsID& aID) {
  MOZ_ASSERT(aInputStream);

  MOZ_LOG(
      gRemoteLazyStreamLog, LogLevel::Verbose,
      ("Storage::AddStream(%s) = %p", nsIDToCString(aID).get(), aInputStream));

  UniquePtr<StreamData> data = MakeUnique<StreamData>();
  data->mInputStream = aInputStream;

  mozilla::StaticMutexAutoLock lock(gMutex);
  mStorage.InsertOrUpdate(aID, std::move(data));
}

nsCOMPtr<nsIInputStream> RemoteLazyInputStreamStorage::ForgetStream(
    const nsID& aID) {
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Storage::ForgetStream(%s)", nsIDToCString(aID).get()));

  UniquePtr<StreamData> entry;

  mozilla::StaticMutexAutoLock lock(gMutex);
  mStorage.Remove(aID, &entry);

  if (!entry) {
    return nullptr;
  }

  return std::move(entry->mInputStream);
}

bool RemoteLazyInputStreamStorage::HasStream(const nsID& aID) {
  mozilla::StaticMutexAutoLock lock(gMutex);
  StreamData* data = mStorage.Get(aID);
  return !!data;
}

void RemoteLazyInputStreamStorage::GetStream(const nsID& aID, uint64_t aStart,
                                             uint64_t aLength,
                                             nsIInputStream** aInputStream) {
  *aInputStream = nullptr;

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Storage::GetStream(%s, %" PRIu64 " %" PRIu64 ")",
           nsIDToCString(aID).get(), aStart, aLength));

  nsCOMPtr<nsIInputStream> inputStream;

  // NS_CloneInputStream cannot be called when the mutex is locked because it
  // can, recursively call GetStream() in case the child actor lives on the
  // parent process.
  {
    mozilla::StaticMutexAutoLock lock(gMutex);
    StreamData* data = mStorage.Get(aID);
    if (!data) {
      return;
    }

    inputStream = data->mInputStream;
  }

  MOZ_ASSERT(inputStream);

  // We cannot return always the same inputStream because not all of them are
  // able to be reused. Better to clone them.

  nsCOMPtr<nsIInputStream> clonedStream;
  nsCOMPtr<nsIInputStream> replacementStream;

  nsresult rv = NS_CloneInputStream(inputStream, getter_AddRefs(clonedStream),
                                    getter_AddRefs(replacementStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (replacementStream) {
    mozilla::StaticMutexAutoLock lock(gMutex);
    StreamData* data = mStorage.Get(aID);
    // data can be gone in the meantime.
    if (!data) {
      return;
    }

    data->mInputStream = replacementStream;
  }

  // Now it's the right time to apply a slice if needed.
  if (aStart > 0 || aLength < UINT64_MAX) {
    clonedStream =
        new SlicedInputStream(clonedStream.forget(), aStart, aLength);
  }

  clonedStream.forget(aInputStream);
}

void RemoteLazyInputStreamStorage::StoreCallback(
    const nsID& aID, RemoteLazyInputStreamParentCallback* aCallback) {
  MOZ_ASSERT(aCallback);

  MOZ_LOG(
      gRemoteLazyStreamLog, LogLevel::Verbose,
      ("Storage::StoreCallback(%s, %p)", nsIDToCString(aID).get(), aCallback));

  mozilla::StaticMutexAutoLock lock(gMutex);
  StreamData* data = mStorage.Get(aID);
  if (data) {
    MOZ_ASSERT(!data->mCallback);
    data->mCallback = aCallback;
  }
}

already_AddRefed<RemoteLazyInputStreamParentCallback>
RemoteLazyInputStreamStorage::TakeCallback(const nsID& aID) {
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Storage::TakeCallback(%s)", nsIDToCString(aID).get()));

  mozilla::StaticMutexAutoLock lock(gMutex);
  StreamData* data = mStorage.Get(aID);
  if (!data) {
    return nullptr;
  }

  RefPtr<RemoteLazyInputStreamParentCallback> callback;
  data->mCallback.swap(callback);
  return callback.forget();
}

void RemoteLazyInputStreamStorage::ActorCreated(const nsID& aID) {
  mozilla::StaticMutexAutoLock lock(gMutex);
  StreamData* data = mStorage.Get(aID);
  if (!data) {
    return;
  }

  size_t count = ++data->mActorCount;

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Storage::ActorCreated(%s) = %zu", nsIDToCString(aID).get(), count));
}

void RemoteLazyInputStreamStorage::ActorDestroyed(const nsID& aID) {
  UniquePtr<StreamData> entry;
  {
    mozilla::StaticMutexAutoLock lock(gMutex);
    StreamData* data = mStorage.Get(aID);
    if (!data) {
      return;
    }

    auto newCount = --data->mActorCount;
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Storage::ActorDestroyed(%s) = %zu (cb=%p)",
             nsIDToCString(aID).get(), newCount, data->mCallback.get()));

    if (newCount == 0) {
      mStorage.Remove(aID, &entry);
    }
  }

  if (entry && entry->mCallback) {
    entry->mCallback->ActorDestroyed(aID);
  }
}

}  // namespace mozilla
