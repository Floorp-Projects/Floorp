/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RefMessageBodyService_h
#define mozilla_dom_RefMessageBodyService_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/StaticMutex.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace dom {

class RefMessageBody final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefMessageBody)

  RefMessageBody(nsID& aPortID,
                 UniquePtr<ipc::StructuredCloneData>&& aCloneData)
      : mPortID(aPortID), mCloneData(std::move(aCloneData)) {}

  const nsID& PortID() const { return mPortID; }

  ipc::StructuredCloneData* CloneData() { return mCloneData.get(); }

 private:
  ~RefMessageBody() = default;

  nsID mPortID;
  UniquePtr<ipc::StructuredCloneData> mCloneData;
};

class RefMessageBodyService final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefMessageBodyService)

  static already_AddRefed<RefMessageBodyService> GetOrCreate();

  void ForgetPort(nsID& aPortID);

  nsID Register(already_AddRefed<RefMessageBody> aBody, ErrorResult& aRv);

  already_AddRefed<RefMessageBody> Steal(nsID& aID);

 private:
  RefMessageBodyService();
  ~RefMessageBodyService();

  static RefMessageBodyService* GetOrCreateInternal(
      const StaticMutexAutoLock& aProofOfLock);

  nsRefPtrHashtable<nsIDHashKey, RefMessageBody> mMessages;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RefMessageBodyService_h
