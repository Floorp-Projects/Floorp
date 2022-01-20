/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MemoryBlobImpl_h
#define mozilla_dom_MemoryBlobImpl_h

#include "mozilla/dom/BaseBlobImpl.h"
#include "mozilla/LinkedList.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsICloneableInputStream.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISeekableStream.h"

namespace mozilla {
namespace dom {

class MemoryBlobImpl final : public BaseBlobImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(MemoryBlobImpl, BaseBlobImpl)

  // File constructor.
  static already_AddRefed<MemoryBlobImpl> CreateWithLastModifiedNow(
      void* aMemoryBuffer, uint64_t aLength, const nsAString& aName,
      const nsAString& aContentType, bool aCrossOriginIsolated);

  // File constructor with custom lastModified attribue value. You should
  // probably use CreateWithLastModifiedNow() instead of this one.
  static already_AddRefed<MemoryBlobImpl> CreateWithCustomLastModified(
      void* aMemoryBuffer, uint64_t aLength, const nsAString& aName,
      const nsAString& aContentType, int64_t aLastModifiedDate);

  // Blob constructor.
  MemoryBlobImpl(void* aMemoryBuffer, uint64_t aLength,
                 const nsAString& aContentType)
      : BaseBlobImpl(aContentType, aLength),
        mDataOwner(new DataOwner(aMemoryBuffer, aLength)) {
    MOZ_ASSERT(mDataOwner && mDataOwner->mData, "must have data");
  }

  void CreateInputStream(nsIInputStream** aStream, ErrorResult& aRv) override;

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) override;

  bool IsMemoryFile() const override { return true; }

  size_t GetAllocationSize() const override { return mLength; }

  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override {
    return GetAllocationSize();
  }

  void GetBlobImplType(nsAString& aBlobImplType) const override {
    aBlobImplType = u"MemoryBlobImpl"_ns;
  }

  class DataOwner final : public mozilla::LinkedListElement<DataOwner> {
   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataOwner)
    DataOwner(void* aMemoryBuffer, uint64_t aLength)
        : mData(aMemoryBuffer), mLength(aLength) {
      mozilla::StaticMutexAutoLock lock(sDataOwnerMutex);

      if (!sDataOwners) {
        sDataOwners = new mozilla::LinkedList<DataOwner>();
        EnsureMemoryReporterRegistered();
      }
      sDataOwners->insertBack(this);
    }

   private:
    // Private destructor, to discourage deletion outside of Release():
    ~DataOwner() {
      mozilla::StaticMutexAutoLock lock(sDataOwnerMutex);

      remove();
      if (sDataOwners->isEmpty()) {
        // Free the linked list if it's empty.
        sDataOwners = nullptr;
      }

      free(mData);
    }

   public:
    static void EnsureMemoryReporterRegistered();

    // sDataOwners and sMemoryReporterRegistered may only be accessed while
    // holding sDataOwnerMutex!  You also must hold the mutex while touching
    // elements of the linked list that DataOwner inherits from.
    static mozilla::StaticMutex sDataOwnerMutex;
    static mozilla::StaticAutoPtr<mozilla::LinkedList<DataOwner> > sDataOwners;
    static bool sMemoryReporterRegistered;

    void* mData;
    uint64_t mLength;
  };

  class DataOwnerAdapter final : public nsIInputStream,
                                 public nsISeekableStream,
                                 public nsIIPCSerializableInputStream,
                                 public nsICloneableInputStream {
    using DataOwner = MemoryBlobImpl::DataOwner;

   public:
    static nsresult Create(DataOwner* aDataOwner, uint32_t aStart,
                           uint32_t aLength, nsIInputStream** _retval);

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

    // These are mandatory.
    NS_FORWARD_NSIINPUTSTREAM(mStream->)
    NS_FORWARD_NSISEEKABLESTREAM(mSeekableStream->)
    NS_FORWARD_NSITELLABLESTREAM(mSeekableStream->)
    NS_FORWARD_NSICLONEABLEINPUTSTREAM(mCloneableInputStream->)

   private:
    ~DataOwnerAdapter() = default;

    DataOwnerAdapter(DataOwner* aDataOwner, nsIInputStream* aStream,
                     uint32_t aLength)
        : mDataOwner(aDataOwner),
          mStream(aStream),
          mSeekableStream(do_QueryInterface(aStream)),
          mSerializableInputStream(do_QueryInterface(aStream)),
          mCloneableInputStream(do_QueryInterface(aStream)),
          mLength(aLength) {
      MOZ_ASSERT(
          mSeekableStream && mSerializableInputStream && mCloneableInputStream,
          "Somebody gave us the wrong stream!");
    }

    template <typename M>
    void SerializeInternal(mozilla::ipc::InputStreamParams& aParams,
                           FileDescriptorArray& aFileDescriptors,
                           bool aDelayedStart, uint32_t aMaxSize,
                           uint32_t* aSizeUsed, M* aManager);

    RefPtr<DataOwner> mDataOwner;
    nsCOMPtr<nsIInputStream> mStream;
    nsCOMPtr<nsISeekableStream> mSeekableStream;
    nsCOMPtr<nsIIPCSerializableInputStream> mSerializableInputStream;
    nsCOMPtr<nsICloneableInputStream> mCloneableInputStream;
    uint32_t mLength;
  };

 private:
  // File constructor.
  MemoryBlobImpl(void* aMemoryBuffer, uint64_t aLength, const nsAString& aName,
                 const nsAString& aContentType, int64_t aLastModifiedDate)
      : BaseBlobImpl(aName, aContentType, aLength, aLastModifiedDate),
        mDataOwner(new DataOwner(aMemoryBuffer, aLength)) {
    MOZ_ASSERT(mDataOwner && mDataOwner->mData, "must have data");
  }

  // Create slice
  MemoryBlobImpl(const MemoryBlobImpl* aOther, uint64_t aStart,
                 uint64_t aLength, const nsAString& aContentType)
      : BaseBlobImpl(aContentType, aOther->mStart + aStart, aLength),
        mDataOwner(aOther->mDataOwner) {
    MOZ_ASSERT(mDataOwner && mDataOwner->mData, "must have data");
  }

  ~MemoryBlobImpl() = default;

  // Used when backed by a memory store
  RefPtr<DataOwner> mDataOwner;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MemoryBlobImpl_h
