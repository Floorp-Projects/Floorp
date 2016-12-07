/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutableBlobStreamListener_h
#define mozilla_dom_MutableBlobStreamListener_h

#include "nsIStreamListener.h"
#include "mozilla/dom/MutableBlobStorage.h"

namespace mozilla {
namespace dom {

// This class is main-thread only.
class MutableBlobStreamListener final : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  MutableBlobStreamListener(MutableBlobStorage::MutableBlobStorageType aType,
                            nsISupports* aParent,
                            const nsACString& aContentType,
                            MutableBlobStorageCallback* aCallback);

private:
  ~MutableBlobStreamListener();

  static nsresult
  WriteSegmentFun(nsIInputStream* aWriter, void* aClosure,
                  const char* aFromSegment, uint32_t aOffset,
                  uint32_t aCount, uint32_t* aWriteCount);

  RefPtr<MutableBlobStorage> mStorage;
  RefPtr<MutableBlobStorageCallback> mCallback;

  nsCOMPtr<nsISupports> mParent;
  MutableBlobStorage::MutableBlobStorageType mStorageType;
  nsCString mContentType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MutableBlobStreamListener_h
