/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RemoteLazyInputStreamStorage_h
#define mozilla_RemoteLazyInputStreamStorage_h

#include "mozilla/RefPtr.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"

class nsIInputStream;
struct nsID;

namespace mozilla {

class RemoteLazyInputStreamParentCallback;

class RemoteLazyInputStreamStorage final : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // This initializes the singleton and it must be called on the main-thread.
  static void Initialize();

  static Result<RefPtr<RemoteLazyInputStreamStorage>, nsresult> Get();

  void AddStream(nsIInputStream* aInputStream, const nsID& aID, uint64_t aSize,
                 uint64_t aChildID);

  // Removes and returns the stream corresponding to the nsID. May return a
  // nullptr if there's no stream stored for the nsID.
  nsCOMPtr<nsIInputStream> ForgetStream(const nsID& aID);

  bool HasStream(const nsID& aID);

  void GetStream(const nsID& aID, uint64_t aStart, uint64_t aLength,
                 nsIInputStream** aInputStream);

  void StoreCallback(const nsID& aID,
                     RemoteLazyInputStreamParentCallback* aCallback);

  already_AddRefed<RemoteLazyInputStreamParentCallback> TakeCallback(
      const nsID& aID);

 private:
  RemoteLazyInputStreamStorage() = default;
  ~RemoteLazyInputStreamStorage() = default;

  struct StreamData {
    nsCOMPtr<nsIInputStream> mInputStream;
    RefPtr<RemoteLazyInputStreamParentCallback> mCallback;

    // This is the Process ID connected with this inputStream. We need to store
    // this information in order to delete it if the child crashes/shutdowns.
    uint64_t mChildID;

    uint64_t mSize;
  };

  nsClassHashtable<nsIDHashKey, StreamData> mStorage;
};

}  // namespace mozilla

#endif  // mozilla_RemoteLazyInputStreamStorage_h
