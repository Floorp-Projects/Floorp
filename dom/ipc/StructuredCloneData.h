/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_StructuredCloneData_h
#define mozilla_dom_ipc_StructuredCloneData_h

#include <algorithm>
#include "mozilla/RefPtr.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsISupportsImpl.h"

namespace IPC {
class Message;
}
class PickleIterator;

namespace mozilla {
namespace dom {
namespace ipc {

class SharedJSAllocatedData final
{
public:
  SharedJSAllocatedData(uint64_t* aData, size_t aDataLength)
  : mData(aData), mDataLength(aDataLength)
  {
    MOZ_ASSERT(mData);
  }

  static already_AddRefed<SharedJSAllocatedData>
  AllocateForExternalData(size_t aDataLength)
  {
    uint64_t* data = Allocate64bitSafely(aDataLength);
    if (!data) {
      return nullptr;
    }

    RefPtr<SharedJSAllocatedData> sharedData =
      new SharedJSAllocatedData(data, aDataLength);
    return sharedData.forget();
  }

  static already_AddRefed<SharedJSAllocatedData>
  CreateFromExternalData(const void* aData, size_t aDataLength)
  {
    RefPtr<SharedJSAllocatedData> sharedData =
      AllocateForExternalData(aDataLength);
    memcpy(sharedData->Data(), aData, aDataLength);
    return sharedData.forget();
  }

  NS_INLINE_DECL_REFCOUNTING(SharedJSAllocatedData)

  uint64_t* Data() const { return mData; }
  size_t DataLength() const { return mDataLength; }

private:
  ~SharedJSAllocatedData()
  {
    js_free(mData);
  }

  static uint64_t*
  Allocate64bitSafely(size_t aSize)
  {
    // Structured cloning requires 64-bit aligment.
    return static_cast<uint64_t*>(js_malloc(std::max(sizeof(uint64_t), aSize)));
  }

  uint64_t* mData;
  size_t mDataLength;
};

class StructuredCloneData : public StructuredCloneHolder
{
public:
  StructuredCloneData()
    : StructuredCloneHolder(StructuredCloneHolder::CloningSupported,
                            StructuredCloneHolder::TransferringSupported,
                            StructuredCloneHolder::DifferentProcess)
    , mExternalData(nullptr)
    , mExternalDataLength(0)
  {}

  StructuredCloneData(const StructuredCloneData&) = delete;

  ~StructuredCloneData()
  {
    MOZ_ASSERT(!(mExternalData && mSharedData));
  }

  StructuredCloneData&
  operator=(const StructuredCloneData& aOther) = delete;

  const nsTArray<RefPtr<BlobImpl>>& BlobImpls() const
  {
    return mBlobImplArray;
  }

  nsTArray<RefPtr<BlobImpl>>& BlobImpls()
  {
    return mBlobImplArray;
  }

  bool Copy(const StructuredCloneData& aData);

  void Read(JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue,
            ErrorResult &aRv);

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             ErrorResult &aRv);

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfers,
             ErrorResult &aRv);

  void UseExternalData(uint64_t* aData, size_t aDataLength)
  {
    MOZ_ASSERT(!Data());
    mExternalData = aData;
    mExternalDataLength = aDataLength;
  }

  bool CopyExternalData(const void* aData, size_t aDataLength);

  uint64_t* Data() const
  {
    return mSharedData ? mSharedData->Data() : mExternalData;
  }

  size_t DataLength() const
  {
    return mSharedData ? mSharedData->DataLength() : mExternalDataLength;
  }

  SharedJSAllocatedData* SharedData() const
  {
    return mSharedData;
  }

  // For IPC serialization
  void WriteIPCParams(IPC::Message* aMessage) const;
  bool ReadIPCParams(const IPC::Message* aMessage, PickleIterator* aIter);

private:
  uint64_t* MOZ_NON_OWNING_REF mExternalData;
  size_t mExternalDataLength;

  RefPtr<SharedJSAllocatedData> mSharedData;
};

} // namespace ipc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_StructuredCloneData_h
