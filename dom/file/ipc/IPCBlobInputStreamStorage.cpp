/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamStorage.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace dom {

namespace {
StaticMutex gMutex;
StaticRefPtr<IPCBlobInputStreamStorage> gStorage;
}

IPCBlobInputStreamStorage::~IPCBlobInputStreamStorage()
{}

/* static */ IPCBlobInputStreamStorage*
IPCBlobInputStreamStorage::Get()
{
  return gStorage;
}

/* static */ void
IPCBlobInputStreamStorage::Initialize()
{
  MOZ_ASSERT(!gStorage);

  gStorage = new IPCBlobInputStreamStorage();
  ClearOnShutdown(&gStorage);
}

void
IPCBlobInputStreamStorage::AddStream(nsIInputStream* aInputStream,
                                     const nsID& aID)
{
  MOZ_ASSERT(aInputStream);

  mozilla::StaticMutexAutoLock lock(gMutex);
  mStorage.Put(aID, aInputStream);
}

void
IPCBlobInputStreamStorage::ForgetStream(const nsID& aID)
{
  mozilla::StaticMutexAutoLock lock(gMutex);
  mStorage.Remove(aID);
}

void
IPCBlobInputStreamStorage::GetStream(const nsID& aID,
                                     nsIInputStream** aInputStream)
{
  mozilla::StaticMutexAutoLock lock(gMutex);
  nsCOMPtr<nsIInputStream> stream = mStorage.Get(aID);
  if (!stream) {
    *aInputStream = nullptr;
    return;
  }

  // We cannot return always the same inputStream because not all of them are
  // able to be reused. Better to clone them.

  nsCOMPtr<nsIInputStream> clonedStream;
  nsCOMPtr<nsIInputStream> replacementStream;

  nsresult rv =
    NS_CloneInputStream(stream, getter_AddRefs(clonedStream),
                        getter_AddRefs(replacementStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (replacementStream) {
    mStorage.Put(aID, replacementStream);
  }

  clonedStream.forget(aInputStream);
}

} // namespace dom
} // namespace mozilla
