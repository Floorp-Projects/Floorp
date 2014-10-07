/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMBlobBuilder_h
#define nsDOMBlobBuilder_h

#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/FileBinding.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

class MultipartFileImpl MOZ_FINAL : public FileImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Create as a file
  MultipartFileImpl(const nsTArray<nsRefPtr<FileImpl>>& aBlobImpls,
                    const nsAString& aName,
                    const nsAString& aContentType)
    : FileImplBase(aName, aContentType, UINT64_MAX),
      mBlobImpls(aBlobImpls),
      mIsFromNsIFile(false)
  {
    SetLengthAndModifiedDate();
  }

  // Create as a blob
  MultipartFileImpl(const nsTArray<nsRefPtr<FileImpl>>& aBlobImpls,
                    const nsAString& aContentType)
    : FileImplBase(aContentType, UINT64_MAX),
      mBlobImpls(aBlobImpls),
      mIsFromNsIFile(false)
  {
    SetLengthAndModifiedDate();
  }

  // Create as a file to be later initialized
  explicit MultipartFileImpl(const nsAString& aName)
    : FileImplBase(aName, EmptyString(), UINT64_MAX),
      mIsFromNsIFile(false)
  {
  }

  // Create as a blob to be later initialized
  MultipartFileImpl()
    : FileImplBase(EmptyString(), UINT64_MAX),
      mIsFromNsIFile(false)
  {
  }

  void InitializeBlob();

  void InitializeBlob(
       JSContext* aCx,
       const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
       const nsAString& aContentType,
       bool aNativeEOL,
       ErrorResult& aRv);

  void InitializeChromeFile(File& aData,
                            const FilePropertyBag& aBag,
                            ErrorResult& aRv);

  void InitializeChromeFile(nsPIDOMWindow* aWindow,
                            const nsAString& aData,
                            const FilePropertyBag& aBag,
                            ErrorResult& aRv);

  void InitializeChromeFile(nsPIDOMWindow* aWindow,
                            nsIFile* aData,
                            const FilePropertyBag& aBag,
                            bool aIsFromNsIFile,
                            ErrorResult& aRv);

  virtual already_AddRefed<FileImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) MOZ_OVERRIDE;

  virtual uint64_t GetSize(ErrorResult& aRv) MOZ_OVERRIDE
  {
    return mLength;
  }

  virtual nsresult GetInternalStream(nsIInputStream** aInputStream) MOZ_OVERRIDE;

  virtual const nsTArray<nsRefPtr<FileImpl>>* GetSubBlobImpls() const MOZ_OVERRIDE
  {
    return &mBlobImpls;
  }

  virtual void GetMozFullPathInternal(nsAString& aFullPath,
                                      ErrorResult& aRv) MOZ_OVERRIDE;

  void SetName(const nsAString& aName)
  {
    mName = aName;
  }

  void SetFromNsIFile(bool aValue)
  {
    mIsFromNsIFile = aValue;
  }

protected:
  virtual ~MultipartFileImpl() {}

  void SetLengthAndModifiedDate();

  nsTArray<nsRefPtr<FileImpl>> mBlobImpls;
  bool mIsFromNsIFile;
};

class BlobSet {
public:
  BlobSet()
    : mData(nullptr), mDataLen(0), mDataBufferLen(0)
  {}

  ~BlobSet()
  {
    moz_free(mData);
  }

  nsresult AppendVoidPtr(const void* aData, uint32_t aLength);
  nsresult AppendString(const nsAString& aString, bool nativeEOL, JSContext* aCx);
  nsresult AppendBlobImpl(FileImpl* aBlobImpl);
  nsresult AppendBlobImpls(const nsTArray<nsRefPtr<FileImpl>>& aBlobImpls);

  nsTArray<nsRefPtr<FileImpl>>& GetBlobImpls() { Flush(); return mBlobImpls; }

  already_AddRefed<File>
  GetBlobInternal(nsISupports* aParent, const nsACString& aContentType)
  {
    nsRefPtr<File> blob = new File(aParent,
      new MultipartFileImpl(GetBlobImpls(), NS_ConvertASCIItoUTF16(aContentType)));
    return blob.forget();
  }

protected:
  bool ExpandBufferSize(uint64_t aSize)
  {
    using mozilla::CheckedUint32;

    if (mDataBufferLen >= mDataLen + aSize) {
      mDataLen += aSize;
      return true;
    }

    // Start at 1 or we'll loop forever.
    CheckedUint32 bufferLen =
      std::max<uint32_t>(static_cast<uint32_t>(mDataBufferLen), 1);
    while (bufferLen.isValid() && bufferLen.value() < mDataLen + aSize)
      bufferLen *= 2;

    if (!bufferLen.isValid())
      return false;

    void* data = moz_realloc(mData, bufferLen.value());
    if (!data)
      return false;

    mData = data;
    mDataBufferLen = bufferLen.value();
    mDataLen += aSize;
    return true;
  }

  void Flush() {
    if (mData) {
      // If we have some data, create a blob for it
      // and put it on the stack

      nsRefPtr<FileImpl> blobImpl =
        new FileImplMemory(mData, mDataLen, EmptyString());
      mBlobImpls.AppendElement(blobImpl);
      mData = nullptr; // The FileImplMemory takes ownership of the buffer
      mDataLen = 0;
      mDataBufferLen = 0;
    }
  }

  nsTArray<nsRefPtr<FileImpl>> mBlobImpls;
  void* mData;
  uint64_t mDataLen;
  uint64_t mDataBufferLen;
};

#endif
