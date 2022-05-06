/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StreamBlobImpl_h
#define mozilla_dom_StreamBlobImpl_h

#include "BaseBlobImpl.h"
#include "nsCOMPtr.h"
#include "nsIMemoryReporter.h"
#include "nsICloneableInputStream.h"

namespace mozilla::dom {

class StreamBlobImpl final : public BaseBlobImpl, public nsIMemoryReporter {
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMEMORYREPORTER

  // Blob constructor.
  static already_AddRefed<StreamBlobImpl> Create(
      already_AddRefed<nsIInputStream> aInputStream,
      const nsAString& aContentType, uint64_t aLength,
      const nsAString& aBlobImplType);

  // File constructor.
  static already_AddRefed<StreamBlobImpl> Create(
      already_AddRefed<nsIInputStream> aInputStream, const nsAString& aName,
      const nsAString& aContentType, int64_t aLastModifiedDate,
      uint64_t aLength, const nsAString& aBlobImplType);

  void CreateInputStream(nsIInputStream** aStream,
                         ErrorResult& aRv) const override;

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) const override;

  bool IsMemoryFile() const override { return true; }

  int64_t GetFileId() const override { return mFileId; }

  void SetFileId(int64_t aFileId) { mFileId = aFileId; }

  void SetFullPath(const nsAString& aFullPath) { mFullPath = aFullPath; }

  void GetMozFullPathInternal(nsAString& aFullPath, ErrorResult& aRv) override {
    aFullPath = mFullPath;
  }

  void SetIsDirectory(bool aIsDirectory) { mIsDirectory = aIsDirectory; }

  bool IsDirectory() const override { return mIsDirectory; }

  size_t GetAllocationSize() const override;

  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override {
    return GetAllocationSize();
  }

  void GetBlobImplType(nsAString& aBlobImplType) const override;

 private:
  // Blob constructor.
  StreamBlobImpl(already_AddRefed<nsICloneableInputStream> aInputStream,
                 const nsAString& aContentType, uint64_t aLength,
                 const nsAString& aBlobImplType);

  // File constructor.
  StreamBlobImpl(already_AddRefed<nsICloneableInputStream> aInputStream,
                 const nsAString& aName, const nsAString& aContentType,
                 int64_t aLastModifiedDate, uint64_t aLength,
                 const nsAString& aBlobImplType);

  ~StreamBlobImpl() override;

  void MaybeRegisterMemoryReporter();

  nsCOMPtr<nsICloneableInputStream> mInputStream;

  nsString mBlobImplType;

  nsString mFullPath;
  bool mIsDirectory;
  int64_t mFileId;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_StreamBlobImpl_h
