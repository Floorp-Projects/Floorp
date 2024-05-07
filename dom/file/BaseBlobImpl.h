/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BaseBlobImpl_h
#define mozilla_dom_BaseBlobImpl_h

#include "nsIGlobalObject.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/ErrorResult.h"

namespace mozilla::dom {

class FileBlobImpl;

class BaseBlobImpl : public BlobImpl {
  friend class FileBlobImpl;

 public:
  // File constructor.
  BaseBlobImpl(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, int64_t aLastModifiedDate)
      : mIsFile(true),
        mContentType(aContentType),
        mName(aName),
        mStart(0),
        mLength(aLength),
        mSerialNumber(NextSerialNumber()),
        mLastModificationDate(aLastModifiedDate) {
    dom::Blob::MakeValidBlobType(mContentType);
  }

  // Blob constructor without starting point.
  BaseBlobImpl(const nsAString& aContentType, uint64_t aLength)
      : mIsFile(false),
        mContentType(aContentType),
        mStart(0),
        mLength(aLength),
        mSerialNumber(NextSerialNumber()),
        mLastModificationDate(0) {
    dom::Blob::MakeValidBlobType(mContentType);
  }

  // Blob constructor with starting point.
  BaseBlobImpl(const nsAString& aContentType, uint64_t aStart, uint64_t aLength)
      : mIsFile(false),
        mContentType(aContentType),
        mStart(aStart),
        mLength(aLength),
        mSerialNumber(NextSerialNumber()),
        mLastModificationDate(0) {
    dom::Blob::MakeValidBlobType(mContentType);
  }

  void GetName(nsAString& aName) const override;

  void GetDOMPath(nsAString& aPath) const override;

  void SetDOMPath(const nsAString& aPath) override;

  int64_t GetLastModified(ErrorResult& aRv) override;

  void GetMozFullPath(nsAString& aFileName, SystemCallerGuarantee /* unused */,
                      ErrorResult& aRv) override;

  void GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) override;

  uint64_t GetSize(ErrorResult& aRv) override { return mLength; }

  void GetType(nsAString& aType) override;

  size_t GetAllocationSize() const override { return 0; }

  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override {
    return GetAllocationSize();
  }

  uint64_t GetSerialNumber() const override { return mSerialNumber; }

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) const override {
    return nullptr;
  }

  const nsTArray<RefPtr<BlobImpl>>* GetSubBlobImpls() const override {
    return nullptr;
  }

  void CreateInputStream(nsIInputStream** aStream,
                         ErrorResult& aRv) const override {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  int64_t GetFileId() const override;

  void SetLazyData(const nsAString& aName, const nsAString& aContentType,
                   uint64_t aLength, int64_t aLastModifiedDate) override {
    mName = aName;
    mContentType = aContentType;
    mLength = aLength;
    SetLastModificationDatePrecisely(aLastModifiedDate);
    mIsFile = !aName.IsVoid();
  }

  bool IsMemoryFile() const override { return false; }

  bool IsFile() const override { return mIsFile; }

  void GetBlobImplType(nsAString& aBlobImplType) const override {
    aBlobImplType = u"BaseBlobImpl"_ns;
  }

 protected:
  ~BaseBlobImpl() override = default;

  /**
   * Returns a new, effectively-unique serial number. This should be used
   * by implementations to obtain a serial number for GetSerialNumber().
   * The implementation is thread safe.
   */
  static uint64_t NextSerialNumber();

  void SetLastModificationDate(RTPCallerType aRTPCallerType, int64_t aDate);
  void SetLastModificationDatePrecisely(int64_t aDate);

#ifdef DEBUG
  bool IsLastModificationDateUnset() const {
    return mLastModificationDate == INT64_MAX;
  }
#endif

  const nsString mBlobImplType;

  bool mIsFile;

  nsString mContentType;
  nsString mName;
  nsString mPath;  // The path relative to a directory chosen by the user

  uint64_t mStart;
  uint64_t mLength;

  const uint64_t mSerialNumber;

 private:
  int64_t mLastModificationDate;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_BaseBlobImpl_h
