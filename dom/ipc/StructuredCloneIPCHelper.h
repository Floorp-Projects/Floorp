/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneIPCHelper_h
#define mozilla_dom_StructuredCloneIPCHelper_h

#include "mozilla/dom/StructuredCloneHelper.h"

namespace IPC {
class Message;
}

namespace mozilla {
namespace dom {

class StructuredCloneIPCHelper final : public StructuredCloneHelper
{
public:
  StructuredCloneIPCHelper()
    : StructuredCloneHelper(StructuredCloneHelper::CloningSupported,
                            StructuredCloneHelper::TransferringNotSupported,
                            StructuredCloneHelper::DifferentProcess)
    , mData(nullptr)
    , mDataLength(0)
    , mDataOwned(eNone)
  {}

  StructuredCloneIPCHelper(const StructuredCloneIPCHelper&) = delete;

  ~StructuredCloneIPCHelper()
  {
    if (mDataOwned == eAllocated) {
      free(mData);
    } else if (mDataOwned == eJSAllocated) {
      js_free(mData);
    }
  }

  StructuredCloneIPCHelper&
  operator=(const StructuredCloneIPCHelper& aOther) = delete;

  const nsTArray<nsRefPtr<BlobImpl>>& BlobImpls() const
  {
    return mBlobImplArray;
  }

  nsTArray<nsRefPtr<BlobImpl>>& BlobImpls()
  {
    return mBlobImplArray;
  }

  bool Copy(const StructuredCloneIPCHelper& aHelper);

  void Read(JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue,
            ErrorResult &aRv);

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             ErrorResult &aRv);

  void UseExternalData(uint64_t* aData, size_t aDataLength)
  {
    MOZ_ASSERT(!mData);
    mData = aData;
    mDataLength = aDataLength;
    MOZ_ASSERT(mDataOwned == eNone);
  }

  uint64_t* Data() const
  {
    return mData;
  }

  size_t DataLength() const
  {
    return mDataLength;
  }

  // For IPC serialization
  void WriteIPCParams(IPC::Message* aMessage) const;
  bool ReadIPCParams(const IPC::Message* aMessage, void** aIter);

private:
  uint64_t* mData;
  size_t mDataLength;
  enum {
    eNone,
    eAllocated,
    eJSAllocated
  } mDataOwned;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StructuredCloneIPCHelper_h
