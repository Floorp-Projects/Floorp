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

class nsDOMMultipartFile : public nsDOMFile,
                           public nsIJSNativeInitializer
{
public:
  // Create as a file
  nsDOMMultipartFile(nsTArray<nsCOMPtr<nsIDOMBlob> > aBlobs,
                     const nsAString& aName,
                     const nsAString& aContentType)
    : nsDOMFile(aName, aContentType, UINT64_MAX),
      mBlobs(aBlobs),
      mIsFromNsiFile(false)
  {
    SetLengthAndModifiedDate();
  }

  // Create as a blob
  nsDOMMultipartFile(nsTArray<nsCOMPtr<nsIDOMBlob> >& aBlobs,
                     const nsAString& aContentType)
    : nsDOMFile(aContentType, UINT64_MAX),
      mBlobs(aBlobs),
      mIsFromNsiFile(false)
  {
    SetLengthAndModifiedDate();
  }

  // Create as a file to be later initialized
  nsDOMMultipartFile(const nsAString& aName)
    : nsDOMFile(aName, EmptyString(), UINT64_MAX),
      mIsFromNsiFile(false)
  {
  }

  // Create as a blob to be later initialized
  nsDOMMultipartFile()
    : nsDOMFile(EmptyString(), UINT64_MAX),
      mIsFromNsiFile(false)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner,
                        JSContext* aCx,
                        JSObject* aObj,
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

  already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType) MOZ_OVERRIDE;

  NS_IMETHOD GetSize(uint64_t*) MOZ_OVERRIDE;
  NS_IMETHOD GetInternalStream(nsIInputStream**) MOZ_OVERRIDE;

  static nsresult
  NewFile(const nsAString& aName, nsISupports* *aNewObject);

  // DOMClassInfo constructor (for Blob([b1, "foo"], { type: "image/png" }))
  static nsresult
  NewBlob(nsISupports* *aNewObject);

  // DOMClassInfo constructor (for File([b1, "foo"], { type: "image/png",
  //                                                   name: "foo.png" }))
  inline static nsresult
  NewFile(nsISupports* *aNewObject)
  {
    // Initialization will set the filename, so we can pass in an empty string
    // for now.
    return NewFile(EmptyString(), aNewObject);
  }

  virtual const nsTArray<nsCOMPtr<nsIDOMBlob> >*
  GetSubBlobs() const MOZ_OVERRIDE { return &mBlobs; }

  NS_IMETHOD GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;

protected:
  nsresult ParseBlobArrayArgument(JSContext* aCx, JS::Value& aValue,
                                  bool aNativeEOL, UnwrapFuncPtr aUnwrapFunc);

  void SetLengthAndModifiedDate();

  nsTArray<nsCOMPtr<nsIDOMBlob> > mBlobs;
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
  nsresult AppendBlob(nsIDOMBlob* aBlob);
  nsresult AppendArrayBuffer(JSObject* aBuffer);
  nsresult AppendBlobs(const nsTArray<nsCOMPtr<nsIDOMBlob> >& aBlob);

  nsTArray<nsCOMPtr<nsIDOMBlob> >& GetBlobs() { Flush(); return mBlobs; }

  already_AddRefed<nsIDOMBlob>
  GetBlobInternal(const nsACString& aContentType)
  {
    nsCOMPtr<nsIDOMBlob> blob =
      new nsDOMMultipartFile(GetBlobs(), NS_ConvertASCIItoUTF16(aContentType));
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

      nsCOMPtr<nsIDOMBlob> blob =
        new nsDOMMemoryFile(mData, mDataLen, EmptyString());
      mBlobs.AppendElement(blob);
      mData = nullptr; // The nsDOMMemoryFile takes ownership of the buffer
      mDataLen = 0;
      mDataBufferLen = 0;
    }
  }

  nsTArray<nsCOMPtr<nsIDOMBlob> > mBlobs;
  void* mData;
  uint64_t mDataLen;
  uint64_t mDataBufferLen;
};

#endif
