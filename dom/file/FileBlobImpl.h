/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileBlobImpl_h
#define mozilla_dom_FileBlobImpl_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"

class nsIFile;

namespace mozilla::dom {

class FileBlobImpl : public BlobImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileBlobImpl, BlobImpl)

  // Create as a file
  explicit FileBlobImpl(nsIFile* aFile);

  // Create as a file
  FileBlobImpl(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, nsIFile* aFile);

  FileBlobImpl(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, nsIFile* aFile, int64_t aLastModificationDate);

  // Create as a file with custom name
  FileBlobImpl(nsIFile* aFile, const nsAString& aName,
               const nsAString& aContentType);

  void GetName(nsAString& aName) const override {
    MOZ_ASSERT(mIsFile, "Should only be called on files");
    aName = mName;
  }

  void SetName(const nsAString& aName) { mName = aName; }

  void GetDOMPath(nsAString& aPath) const override {
    MOZ_ASSERT(mIsFile, "Should only be called on files");
    aPath = mPath;
  }

  void SetDOMPath(const nsAString& aPath) override {
    MOZ_ASSERT(mIsFile, "Should only be called on files");
    mPath = aPath;
  }

  int64_t GetLastModified(ErrorResult& aRv) override;

  void GetMozFullPath(nsAString& aFileName, SystemCallerGuarantee /* unused */,
                      ErrorResult& aRv) override {
    MOZ_ASSERT(mIsFile, "Should only be called on files");

    GetMozFullPathInternal(aFileName, aRv);
  }

  void GetMozFullPathInternal(nsAString& aFilename, ErrorResult& aRv) override;

  uint64_t GetSize(ErrorResult& aRv) override;

  void GetType(nsAString& aType) override;

  void GetBlobImplType(nsAString& aBlobImplType) const override;

  size_t GetAllocationSize() const override { return 0; }

  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override {
    return GetAllocationSize();
  }

  uint64_t GetSerialNumber() const override { return mSerialNumber; }

  const nsTArray<RefPtr<BlobImpl>>* GetSubBlobImpls() const override {
    return nullptr;
  }

  void CreateInputStream(nsIInputStream** aStream,
                         ErrorResult& aRv) const override;

  int64_t GetFileId() const override { return mFileId; }

  void SetLazyData(const nsAString& aName, const nsAString& aContentType,
                   uint64_t aLength, int64_t aLastModifiedDate) override {
    mName = aName;
    mContentType = aContentType;
    mIsFile = !aName.IsVoid();
    mLength.emplace(aLength);
    mLastModified.emplace(aLastModifiedDate);
  }

  bool IsMemoryFile() const override { return false; }

  bool IsFile() const override { return mIsFile; }

  bool IsDirectory() const override;

  void SetType(const nsAString& aType) { mContentType = aType; }

  void SetFileId(int64_t aFileId) { mFileId = aFileId; }

  void SetEmptySize() { mLength.emplace(0); }

  void SetMozFullPath(const nsAString& aPath) { mMozFullPath = aPath; }

  void SetLastModified(int64_t aLastModified) {
    mLastModified.emplace(aLastModified);
  }

 protected:
  ~FileBlobImpl() override = default;

  // Create slice
  FileBlobImpl(const FileBlobImpl* aOther, uint64_t aStart, uint64_t aLength,
               const nsAString& aContentType);

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) const override;

  class GetTypeRunnable;
  void GetTypeInternal(nsAString& aType, const MutexAutoLock& aProofOfLock);

  // FileBlobImpl has getter methods with lazy initialization. Because any
  // BlobImpl must work thread-safe, we use a mutex.
  Mutex mMutex MOZ_UNANNOTATED;

  nsCOMPtr<nsIFile> mFile;

  nsString mContentType;
  nsString mName;
  nsString mPath;  // The path relative to a directory chosen by the user
  nsString mMozFullPath;

  const uint64_t mSerialNumber;
  uint64_t mStart;

  int64_t mFileId;

  Maybe<uint64_t> mLength;

  Maybe<int64_t> mLastModified;

  bool mIsFile;
  bool mWholeFile;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_FileBlobImpl_h
