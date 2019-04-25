/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BaseBlobImpl_h
#define mozilla_dom_BaseBlobImpl_h

#include "mozilla/dom/BlobImpl.h"

namespace mozilla {
namespace dom {

class BaseBlobImpl : public BlobImpl {
 public:
  BaseBlobImpl(const nsAString& aBlobImplType, const nsAString& aName,
               const nsAString& aContentType, uint64_t aLength,
               int64_t aLastModifiedDate)
      : mBlobImplType(aBlobImplType),
        mIsFile(true),
        mImmutable(false),
        mContentType(aContentType),
        mName(aName),
        mStart(0),
        mLength(aLength),
        mLastModificationDate(aLastModifiedDate),
        mSerialNumber(NextSerialNumber()) {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  BaseBlobImpl(const nsAString& aBlobImplType, const nsAString& aName,
               const nsAString& aContentType, uint64_t aLength)
      : mBlobImplType(aBlobImplType),
        mIsFile(true),
        mImmutable(false),
        mContentType(aContentType),
        mName(aName),
        mStart(0),
        mLength(aLength),
        mLastModificationDate(INT64_MAX),
        mSerialNumber(NextSerialNumber()) {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  BaseBlobImpl(const nsAString& aBlobImplType, const nsAString& aContentType,
               uint64_t aLength)
      : mBlobImplType(aBlobImplType),
        mIsFile(false),
        mImmutable(false),
        mContentType(aContentType),
        mStart(0),
        mLength(aLength),
        mLastModificationDate(INT64_MAX),
        mSerialNumber(NextSerialNumber()) {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  BaseBlobImpl(const nsAString& aBlobImplType, const nsAString& aContentType,
               uint64_t aStart, uint64_t aLength)
      : mBlobImplType(aBlobImplType),
        mIsFile(false),
        mImmutable(false),
        mContentType(aContentType),
        mStart(aStart),
        mLength(aLength),
        mLastModificationDate(INT64_MAX),
        mSerialNumber(NextSerialNumber()) {
    MOZ_ASSERT(aLength != UINT64_MAX, "Must know length when creating slice");
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  virtual void GetName(nsAString& aName) const override;

  virtual void GetDOMPath(nsAString& aName) const override;

  virtual void SetDOMPath(const nsAString& aName) override;

  virtual int64_t GetLastModified(ErrorResult& aRv) override;

  virtual void SetLastModified(int64_t aLastModified) override;

  virtual void GetMozFullPath(nsAString& aName,
                              SystemCallerGuarantee /* unused */,
                              ErrorResult& aRv) override;

  virtual void GetMozFullPathInternal(nsAString& aFileName,
                                      ErrorResult& aRv) override;

  virtual uint64_t GetSize(ErrorResult& aRv) override { return mLength; }

  virtual void GetType(nsAString& aType) override;

  size_t GetAllocationSize() const override { return 0; }

  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override {
    return GetAllocationSize();
  }

  virtual uint64_t GetSerialNumber() const override { return mSerialNumber; }

  virtual already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart,
                                                 uint64_t aLength,
                                                 const nsAString& aContentType,
                                                 ErrorResult& aRv) override {
    return nullptr;
  }

  virtual const nsTArray<RefPtr<BlobImpl>>* GetSubBlobImpls() const override {
    return nullptr;
  }

  virtual void CreateInputStream(nsIInputStream** aStream,
                                 ErrorResult& aRv) override {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  virtual int64_t GetFileId() override;

  virtual nsresult GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                               nsACString& aContentType,
                               nsACString& aCharset) override;

  virtual nsresult GetMutable(bool* aMutable) const override;

  virtual nsresult SetMutable(bool aMutable) override;

  virtual void SetLazyData(const nsAString& aName,
                           const nsAString& aContentType, uint64_t aLength,
                           int64_t aLastModifiedDate) override {
    mName = aName;
    mContentType = aContentType;
    mLength = aLength;
    mLastModificationDate = aLastModifiedDate;
    mIsFile = !aName.IsVoid();
  }

  virtual bool IsMemoryFile() const override { return false; }

  virtual bool IsDateUnknown() const override {
    return mIsFile && mLastModificationDate == INT64_MAX;
  }

  virtual bool IsFile() const override { return mIsFile; }

  virtual bool IsSizeUnknown() const override { return mLength == UINT64_MAX; }

  virtual void GetBlobImplType(nsAString& aBlobImplType) const override {
    aBlobImplType = mBlobImplType;
  }

 protected:
  virtual ~BaseBlobImpl() {}

  /**
   * Returns a new, effectively-unique serial number. This should be used
   * by implementations to obtain a serial number for GetSerialNumber().
   * The implementation is thread safe.
   */
  static uint64_t NextSerialNumber();

  const nsString mBlobImplType;

  bool mIsFile;
  bool mImmutable;

  nsString mContentType;
  nsString mName;
  nsString mPath;  // The path relative to a directory chosen by the user

  uint64_t mStart;
  uint64_t mLength;

  int64_t mLastModificationDate;

  const uint64_t mSerialNumber;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BaseBlobImpl_h
