/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MultipartFileImpl_h
#define mozilla_dom_MultipartFileImpl_h

#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/FileBinding.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

class MultipartFileImpl final : public FileImplBase
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
                            const ChromeFilePropertyBag& aBag,
                            ErrorResult& aRv);

  void InitializeChromeFile(nsPIDOMWindow* aWindow,
                            const nsAString& aData,
                            const ChromeFilePropertyBag& aBag,
                            ErrorResult& aRv);

  void InitializeChromeFile(nsPIDOMWindow* aWindow,
                            nsIFile* aData,
                            const ChromeFilePropertyBag& aBag,
                            bool aIsFromNsIFile,
                            ErrorResult& aRv);

  virtual already_AddRefed<FileImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) override;

  virtual uint64_t GetSize(ErrorResult& aRv) override
  {
    return mLength;
  }

  virtual nsresult GetInternalStream(nsIInputStream** aInputStream) override;

  virtual const nsTArray<nsRefPtr<FileImpl>>* GetSubBlobImpls() const override
  {
    return &mBlobImpls;
  }

  virtual void GetMozFullPathInternal(nsAString& aFullPath,
                                      ErrorResult& aRv) override;

  virtual nsresult
  SetMutable(bool aMutable) override;

  void SetName(const nsAString& aName)
  {
    mName = aName;
  }

  void SetFromNsIFile(bool aValue)
  {
    mIsFromNsIFile = aValue;
  }

  virtual bool MayBeClonedToOtherThreads() const override;

protected:
  virtual ~MultipartFileImpl() {}

  void SetLengthAndModifiedDate();

  nsTArray<nsRefPtr<FileImpl>> mBlobImpls;
  bool mIsFromNsIFile;
};

#endif // mozilla_dom_MultipartFileImpl_h
