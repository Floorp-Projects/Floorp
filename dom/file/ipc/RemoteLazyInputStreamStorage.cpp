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
#include "RemoteLazyInputStreamStorage.h"

namespace mozilla {

using namespace hal;

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
RemoteLazyInputStreamStorage* RemoteLazyInputStreamStorage::Get() {
  return gStorage;
}

/* static */
void RemoteLazyInputStreamStorage::Initialize() {
  MOZ_ASSERT(!gStorage);

  gStorage = new RemoteLazyInputStreamStorage();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(gStorage, "xpcom-shutdown", false);
    obs->AddObserver(gStorage, "ipc:content-shutdown", false);
  }
}

NS_IMETHODIMP
RemoteLazyInputStreamStorage::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "xpcom-shutdown");
      obs->RemoveObserver(this, "ipc:content-shutdown");
    }

    gStorage = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(!strcmp(aTopic, "ipc:content-shutdown"));

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  if (NS_WARN_IF(!props)) {
    return NS_ERROR_FAILURE;
  }

  uint64_t childID = CONTENT_PROCESS_ID_UNKNOWN;
  props->GetPropertyAsUint64(u"childID"_ns, &childID);
  if (NS_WARN_IF(childID == CONTENT_PROCESS_ID_UNKNOWN)) {
    return NS_ERROR_FAILURE;
  }

  mozilla::StaticMutexAutoLock lock(gMutex);

  for (auto iter = mStorage.Iter(); !iter.Done(); iter.Next()) {
    if (iter.Data()->mChildID == childID) {
      iter.Remove();
    }
  }

  return NS_OK;
}

void RemoteLazyInputStreamStorage::AddStream(nsIInputStream* aInputStream,
                                             const nsID& aID, uint64_t aSize,
                                             uint64_t aChildID) {
  MOZ_ASSERT(aInputStream);

  StreamData* data = new StreamData();
  data->mInputStream = aInputStream;
  data->mChildID = aChildID;
  data->mSize = aSize;

  mozilla::StaticMutexAutoLock lock(gMutex);
  mStorage.Put(aID, data);
}

nsCOMPtr<nsIInputStream> RemoteLazyInputStreamStorage::ForgetStream(
    const nsID& aID) {
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

  nsCOMPtr<nsIInputStream> inputStream;
  uint64_t size;

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
    size = data->mSize;
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
  if (aStart > 0 || aLength < size) {
    clonedStream =
        new SlicedInputStream(clonedStream.forget(), aStart, aLength);
  }

  clonedStream.forget(aInputStream);
}

void RemoteLazyInputStreamStorage::StoreCallback(
    const nsID& aID, RemoteLazyInputStreamParentCallback* aCallback) {
  MOZ_ASSERT(aCallback);

  mozilla::StaticMutexAutoLock lock(gMutex);
  StreamData* data = mStorage.Get(aID);
  if (data) {
    MOZ_ASSERT(!data->mCallback);
    data->mCallback = aCallback;
  }
}

already_AddRefed<RemoteLazyInputStreamParentCallback>
RemoteLazyInputStreamStorage::TakeCallback(const nsID& aID) {
  mozilla::StaticMutexAutoLock lock(gMutex);
  StreamData* data = mStorage.Get(aID);
  if (!data) {
    return nullptr;
  }

  RefPtr<RemoteLazyInputStreamParentCallback> callback;
  data->mCallback.swap(callback);
  return callback.forget();
}

}  // namespace mozilla
