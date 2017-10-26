/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

class FileSystemEntry;
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

  DataTransferItem(DataTransfer* aDataTransfer, const nsAString& aType,
                   eKind aKind = KIND_OTHER)
    : mIndex(0)
    , mChromeOnly(false)
    , mKind(aKind)
    , mType(aType)
    , mDataTransfer(aDataTransfer)
  {
    MOZ_ASSERT(mDataTransfer, "Must be associated with a DataTransfer");
  }

  already_AddRefed<DataTransferItem> Clone(DataTransfer* aDataTransfer) const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetAsString(FunctionStringCallback* aCallback,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aRv);

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

  void GetInternalType(nsAString& aType) const
  {
    aType = mType;
  }

  void GetType(nsAString& aType);

  eKind Kind() const
  {
    return mKind;
  }

  already_AddRefed<File>
  GetAsFile(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv);

  already_AddRefed<FileSystemEntry>
  GetAsEntry(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv);

  DataTransfer* GetParentObject() const
  {
    return mDataTransfer;
  }

  nsIPrincipal* Principal() const
  {
    return mPrincipal;
  }
  void SetPrincipal(nsIPrincipal* aPrincipal)
  {
    mPrincipal = aPrincipal;
  }

  already_AddRefed<nsIVariant> DataNoSecurityCheck();
  already_AddRefed<nsIVariant> Data(nsIPrincipal* aPrincipal, ErrorResult& aRv);

  // Note: This can modify the mKind.  Callers of this method must let the
  // relevant DataTransfer know, because its types list can change as a result.
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

  static eKind KindFromData(nsIVariant* aData);

private:
  ~DataTransferItem() {}
  already_AddRefed<File> CreateFileFromInputStream(nsIInputStream* aStream);

  // The index in the 2d mIndexedItems array
  uint32_t mIndex;

  bool mChromeOnly;
  eKind mKind;
  const nsString mType;
  nsCOMPtr<nsIVariant> mData;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  RefPtr<DataTransfer> mDataTransfer;

  // File cache for nsIFile application/x-moz-file entries.
  RefPtr<File> mCachedFile;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DataTransferItem_h */
