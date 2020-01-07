/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileBlobImpl_h
#define mozilla_dom_FileBlobImpl_h

#include "mozilla/dom/BaseBlobImpl.h"

class nsIFile;

namespace mozilla {
namespace dom {

class FileBlobImpl : public BaseBlobImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileBlobImpl, BaseBlobImpl)

  // Create as a file
  explicit FileBlobImpl(nsIFile* aFile);

  // Create as a file
  FileBlobImpl(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, nsIFile* aFile);

  FileBlobImpl(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, nsIFile* aFile, int64_t aLastModificationDate);

  // Create as a file with custom name
  FileBlobImpl(
      nsIFile* aFile, const nsAString& aName, const nsAString& aContentType,
      const nsAString& aBlobImplType = NS_LITERAL_STRING("FileBlobImpl"));

  // Overrides
  virtual uint64_t GetSize(ErrorResult& aRv) override;
  virtual void GetType(nsAString& aType) override;
  virtual int64_t GetLastModified(ErrorResult& aRv) override;
  virtual void GetMozFullPathInternal(nsAString& aFullPath,
                                      ErrorResult& aRv) override;
  virtual void CreateInputStream(nsIInputStream** aInputStream,
                                 ErrorResult& aRv) override;

  virtual bool IsDirectory() const override;

  // We always have size and date for this kind of blob.
  virtual bool IsSizeUnknown() const override { return false; }
  virtual bool IsDateUnknown() const override { return false; }

  void SetName(const nsAString& aName) { mName = aName; }

  void SetType(const nsAString& aType) { mContentType = aType; }

  int64_t GetFileId() override { return mFileId; }

  void SetFileId(int64_t aFileId) { mFileId = aFileId; }

  void SetEmptySize() { mLength = 0; }

  void SetMozFullPath(const nsAString& aPath) { mMozFullPath = aPath; }

 protected:
  virtual ~FileBlobImpl() = default;

  // Create slice
  FileBlobImpl(const FileBlobImpl* aOther, uint64_t aStart, uint64_t aLength,
               const nsAString& aContentType);

  virtual already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart,
                                                 uint64_t aLength,
                                                 const nsAString& aContentType,
                                                 ErrorResult& aRv) override;

  nsCOMPtr<nsIFile> mFile;
  nsString mMozFullPath;
  int64_t mFileId;
  bool mWholeFile;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FileBlobImpl_h
