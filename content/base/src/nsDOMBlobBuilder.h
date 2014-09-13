/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMBlobBuilder_h
#define nsDOMBlobBuilder_h

#include "nsDOMFile.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/Attributes.h"
#include <algorithm>

#define NS_DOMMULTIPARTBLOB_CID { 0x47bf0b43, 0xf37e, 0x49ef, \
  { 0x81, 0xa0, 0x18, 0xba, 0xc0, 0x57, 0xb5, 0xcc } }
#define NS_DOMMULTIPARTBLOB_CONTRACTID "@mozilla.org/dom/multipart-blob;1"

#define NS_DOMMULTIPARTFILE_CID { 0xc3361f77, 0x60d1, 0x4ea9, \
  { 0x94, 0x96, 0xdf, 0x5d, 0x6f, 0xcd, 0xd7, 0x8f } }
#define NS_DOMMULTIPARTFILE_CONTRACTID "@mozilla.org/dom/multipart-file;1"

using namespace mozilla::dom;

class DOMMultipartFileImpl MOZ_FINAL : public DOMFileImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Create as a file
  DOMMultipartFileImpl(const nsTArray<nsRefPtr<DOMFileImpl>>& aBlobImpls,
                       const nsAString& aName,
                       const nsAString& aContentType)
    : DOMFileImplBase(aName, aContentType, UINT64_MAX),
      mBlobImpls(aBlobImpls),
      mIsFromNsiFile(false)
  {
    SetLengthAndModifiedDate();
  }

  // Create as a blob
  DOMMultipartFileImpl(const nsTArray<nsRefPtr<DOMFileImpl>>& aBlobImpls,
                       const nsAString& aContentType)
    : DOMFileImplBase(aContentType, UINT64_MAX),
      mBlobImpls(aBlobImpls),
      mIsFromNsiFile(false)
  {
    SetLengthAndModifiedDate();
  }

  // Create as a file to be later initialized
  explicit DOMMultipartFileImpl(const nsAString& aName)
    : DOMFileImplBase(aName, EmptyString(), UINT64_MAX),
      mIsFromNsiFile(false)
  {
  }

  // Create as a blob to be later initialized
  DOMMultipartFileImpl()
    : DOMFileImplBase(EmptyString(), UINT64_MAX),
      mIsFromNsiFile(false)
  {
  }

  virtual nsresult
  Initialize(nsISupports* aOwner, JSContext* aCx, JSObject* aObj,
             const JS::CallArgs& aArgs) MOZ_OVERRIDE;

  typedef nsIDOMBlob* (*UnwrapFuncPtr)(JSContext*, JSObject*);
  nsresult InitBlob(JSContext* aCx,
                    uint32_t aArgc,
                    JS::Value* aArgv,
                    UnwrapFuncPtr aUnwrapFunc);
  nsresult InitFile(JSContext* aCx,
                    uint32_t aArgc,
                    JS::Value* aArgv);
  nsresult InitChromeFile(JSContext* aCx,
                          uint32_t aArgc,
                          JS::Value* aArgv);

  virtual already_AddRefed<DOMFileImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  virtual nsresult GetSize(uint64_t* aSize) MOZ_OVERRIDE;

  virtual nsresult GetInternalStream(nsIInputStream** aInputStream) MOZ_OVERRIDE;

  static nsresult NewFile(const nsAString& aName, nsISupports** aNewObject);

  // DOMClassInfo constructor (for Blob([b1, "foo"], { type: "image/png" }))
  static nsresult NewBlob(nsISupports* *aNewObject);

  // DOMClassInfo constructor (for File([b1, "foo"], { type: "image/png",
  //                                                   name: "foo.png" }))
  inline static nsresult NewFile(nsISupports** aNewObject)
  {
    // Initialization will set the filename, so we can pass in an empty string
    // for now.
    return NewFile(EmptyString(), aNewObject);
  }

  virtual const nsTArray<nsRefPtr<DOMFileImpl>>* GetSubBlobImpls() const MOZ_OVERRIDE
  {
    return &mBlobImpls;
  }

  virtual nsresult GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;

protected:
  virtual ~DOMMultipartFileImpl() {}

  nsresult ParseBlobArrayArgument(JSContext* aCx, JS::Value& aValue,
                                  bool aNativeEOL, UnwrapFuncPtr aUnwrapFunc);

  void SetLengthAndModifiedDate();

  nsTArray<nsRefPtr<DOMFileImpl>> mBlobImpls;
  bool mIsFromNsiFile;
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
  nsresult AppendString(JSString* aString, bool nativeEOL, JSContext* aCx);
  nsresult AppendBlobImpl(DOMFileImpl* aBlobImpl);
  nsresult AppendArrayBuffer(JSObject* aBuffer);
  nsresult AppendBlobImpls(const nsTArray<nsRefPtr<DOMFileImpl>>& aBlobImpls);

  nsTArray<nsRefPtr<DOMFileImpl>>& GetBlobImpls() { Flush(); return mBlobImpls; }

  already_AddRefed<nsIDOMBlob>
  GetBlobInternal(const nsACString& aContentType)
  {
    nsCOMPtr<nsIDOMBlob> blob = new DOMFile(
      new DOMMultipartFileImpl(GetBlobImpls(), NS_ConvertASCIItoUTF16(aContentType)));
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

      nsRefPtr<DOMFileImpl> blobImpl =
        new DOMFileImplMemory(mData, mDataLen, EmptyString());
      mBlobImpls.AppendElement(blobImpl);
      mData = nullptr; // The nsDOMMemoryFile takes ownership of the buffer
      mDataLen = 0;
      mDataBufferLen = 0;
    }
  }

  nsTArray<nsRefPtr<DOMFileImpl>> mBlobImpls;
  void* mData;
  uint64_t mDataLen;
  uint64_t mDataBufferLen;
};

#endif
