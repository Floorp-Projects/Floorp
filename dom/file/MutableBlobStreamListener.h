/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutableBlobStreamListener_h
#define mozilla_dom_MutableBlobStreamListener_h

#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsTString.h"
#include "mozilla/dom/MutableBlobStorage.h"

class nsIEventTarget;

namespace mozilla::dom {

class MutableBlobStreamListener final
    : public nsIThreadRetargetableStreamListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  MutableBlobStreamListener(
      MutableBlobStorage::MutableBlobStorageType aStorageType,
      const nsACString& aContentType, MutableBlobStorageCallback* aCallback,
      nsIEventTarget* aEventTarget = nullptr);

 private:
  ~MutableBlobStreamListener();

  static nsresult WriteSegmentFun(nsIInputStream* aWriter, void* aClosure,
                                  const char* aFromSegment, uint32_t aOffset,
                                  uint32_t aCount, uint32_t* aWriteCount);

  RefPtr<MutableBlobStorage> mStorage;
  RefPtr<MutableBlobStorageCallback> mCallback;

  MutableBlobStorage::MutableBlobStorageType mStorageType;
  nsCString mContentType;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MutableBlobStreamListener_h
