/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataTransferItem_h
#define mozilla_dom_DataTransferItem_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {

class FunctionStringCallback;

class DataTransferItem final : public nsISupports
                             , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DataTransferItem);

public:
  // The spec only talks about the "file" and "string" kinds. Due to the Moz*
  // APIs, it is possible to attach any type to a DataTransferItem, meaning that
  // we can have other kinds then just FILE and STRING. These others are simply
  // marked as "other" and can only be produced throug the Moz* APIs.
  enum eKind {
    KIND_FILE,
    KIND_STRING,
    KIND_OTHER,
  };

  DataTransferItem(DataTransferItemList* aParent, const nsAString& aType)
    : mIndex(0), mChromeOnly(false), mKind(KIND_OTHER), mType(aType), mParent(aParent)
  {}

  already_AddRefed<DataTransferItem> Clone(DataTransferItemList* aParent) const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  void GetAsString(FunctionStringCallback* aCallback, ErrorResult& aRv);
  void GetKind(nsAString& aKind) const
  {
    switch (mKind) {
    case KIND_FILE:
      aKind = NS_LITERAL_STRING("file");
      return;
    case KIND_STRING:
      aKind = NS_LITERAL_STRING("string");
      return;
    default:
      aKind = NS_LITERAL_STRING("other");
      return;
    }
  }

  void GetType(nsAString& aType) const
  {
    aType = mType;
  }
  void SetType(const nsAString& aType);

  eKind Kind() const
  {
    return mKind;
  }
  void SetKind(eKind aKind)
  {
    mKind = aKind;
  }

  already_AddRefed<File> GetAsFile(ErrorResult& aRv);

  DataTransferItemList* GetParentObject() const
  {
    return mParent;
  }

  nsIPrincipal* Principal() const
  {
    return mPrincipal;
  }
  void SetPrincipal(nsIPrincipal* aPrincipal)
  {
    mPrincipal = aPrincipal;
  }

  nsIVariant* Data()
  {
    if (!mData) {
      FillInExternalData();
    }
    return mData;
  }
  void SetData(nsIVariant* aData);

  uint32_t Index() const
  {
    return mIndex;
  }
  void SetIndex(uint32_t aIndex)
  {
    mIndex = aIndex;
  }
  void FillInExternalData();

  bool ChromeOnly() const
  {
    return mChromeOnly;
  }
  void SetChromeOnly(bool aChromeOnly)
  {
    mChromeOnly = aChromeOnly;
  }

private:
  ~DataTransferItem() {}
  already_AddRefed<File> FileFromISupports(nsISupports* aParent);
  already_AddRefed<File> CreateFileFromInputStream(nsISupports* aParent,
                                                   nsIInputStream* aStream,
                                                   ErrorResult& aRv);

  // The index in the 2d mIndexedItems array
  uint32_t mIndex;

  bool mChromeOnly;
  eKind mKind;
  nsString mType;
  nsCOMPtr<nsIVariant> mData;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  RefPtr<DataTransferItemList> mParent;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DataTransferItem_h */
