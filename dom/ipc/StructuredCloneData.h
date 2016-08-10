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
  explicit SharedJSAllocatedData(JSStructuredCloneData&& aData)
    : mData(Move(aData))
  { }

  static already_AddRefed<SharedJSAllocatedData>
  CreateFromExternalData(const char* aData, size_t aDataLength)
  {
    JSStructuredCloneData buf;
    buf.WriteBytes(aData, aDataLength);
    RefPtr<SharedJSAllocatedData> sharedData =
      new SharedJSAllocatedData(Move(buf));
    return sharedData.forget();
  }

  static already_AddRefed<SharedJSAllocatedData>
  CreateFromExternalData(const JSStructuredCloneData& aData)
  {
    JSStructuredCloneData buf;
    auto iter = aData.Iter();
    while (!iter.Done()) {
      buf.WriteBytes(iter.Data(), iter.RemainingInSegment());
      iter.Advance(aData, iter.RemainingInSegment());
    }
    RefPtr<SharedJSAllocatedData> sharedData =
      new SharedJSAllocatedData(Move(buf));
    return sharedData.forget();
  }

  NS_INLINE_DECL_REFCOUNTING(SharedJSAllocatedData)

  JSStructuredCloneData& Data() { return mData; }
  size_t DataLength() const { return mData.Size(); }

private:
  ~SharedJSAllocatedData() { }

  JSStructuredCloneData mData;
};

class StructuredCloneData : public StructuredCloneHolder
{
public:
  StructuredCloneData()
    : StructuredCloneHolder(StructuredCloneHolder::CloningSupported,
                            StructuredCloneHolder::TransferringSupported,
                            StructuredCloneHolder::DifferentProcess)
    , mInitialized(false)
  {}

  StructuredCloneData(const StructuredCloneData&) = delete;

  StructuredCloneData(StructuredCloneData&& aOther) = default;

  ~StructuredCloneData()
  {}

  StructuredCloneData&
  operator=(const StructuredCloneData& aOther) = delete;

  StructuredCloneData&
  operator=(StructuredCloneData&& aOther) = default;

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

  bool UseExternalData(const JSStructuredCloneData& aData)
  {
    auto iter = aData.Iter();
    bool success = false;
    mExternalData =
      aData.Borrow<js::SystemAllocPolicy>(iter, aData.Size(), &success);
    mInitialized = true;
    return success;
  }

  bool CopyExternalData(const char* aData, size_t aDataLength);

  JSStructuredCloneData& Data()
  {
    return mSharedData ? mSharedData->Data() : mExternalData;
  }

  const JSStructuredCloneData& Data() const
  {
    return mSharedData ? mSharedData->Data() : mExternalData;
  }

  size_t DataLength() const
  {
    return mSharedData ? mSharedData->DataLength() : mExternalData.Size();
  }

  SharedJSAllocatedData* SharedData() const
  {
    return mSharedData;
  }

  // For IPC serialization
  void WriteIPCParams(IPC::Message* aMessage) const;
  bool ReadIPCParams(const IPC::Message* aMessage, PickleIterator* aIter);

private:
  JSStructuredCloneData mExternalData;
  RefPtr<SharedJSAllocatedData> mSharedData;
  bool mInitialized;
};

} // namespace ipc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_StructuredCloneData_h
