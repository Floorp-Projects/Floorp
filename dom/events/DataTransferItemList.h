/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataTransferItemList_h
#define mozilla_dom_DataTransferItemList_h

#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DataTransferItem.h"
#include "mozilla/dom/FileList.h"

namespace mozilla {
namespace dom {

class DataTransferItem;

class DataTransferItemList final : public nsISupports
                                 , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DataTransferItemList);

  DataTransferItemList(DataTransfer* aParent, bool aIsExternal,
                       bool aIsCrossDomainSubFrameDrop)
    : mParent(aParent)
    , mIsCrossDomainSubFrameDrop(aIsCrossDomainSubFrameDrop)
    , mIsExternal(aIsExternal)
  {
    // We always allocate an index 0 in our DataTransferItemList. This is done
    // in order to maintain the invariants according to the spec. Mainly, within
    // the spec's list, there is intended to be a single copy of each mime type,
    // for string typed items. File typed items are allowed to have duplicates.
    // In the old moz* system, this was modeled by having multiple indexes, each
    // of which was independent. Files were fetched from all indexes, but
    // strings were only fetched from the first index. In order to maintain this
    // correlation and avoid breaking code with the new changes, index 0 is now
    // always present and used to store strings, and all file items are given
    // their own index starting at index 1.
    mIndexedItems.SetLength(1);
  }

  already_AddRefed<DataTransferItemList> Clone(DataTransfer* aParent) const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  uint32_t Length() const
  {
    return mItems.Length();
  };

  DataTransferItem* Add(const nsAString& aData, const nsAString& aType,
                        ErrorResult& rv);
  DataTransferItem* Add(File& aData, ErrorResult& aRv);

  void Remove(uint32_t aIndex, ErrorResult& aRv);

  DataTransferItem* IndexedGetter(uint32_t aIndex, bool& aFound,
                                  ErrorResult& aRv) const;

  void Clear(ErrorResult& aRv);

  DataTransfer* GetParentObject() const
  {
    return mParent;
  }

  // Accessors for data from ParentObject
  bool IsReadOnly() const;
  int32_t ClipboardType() const;
  EventMessage GetEventMessage() const;

  already_AddRefed<DataTransferItem>
  SetDataWithPrincipal(const nsAString& aType, nsIVariant* aData,
                       uint32_t aIndex, nsIPrincipal* aPrincipal,
                       bool aInsertOnly, ErrorResult& aRv);

  FileList* Files();

  // Moz-style helper methods for interacting with the stored data
  void MozRemoveByTypeAt(const nsAString& aType, uint32_t aIndex,
                         ErrorResult& aRv);
  DataTransferItem* MozItemByTypeAt(const nsAString& aType, uint32_t aIndex);
  const nsTArray<RefPtr<DataTransferItem>>* MozItemsAt(uint32_t aIndex);
  uint32_t MozItemCount() const;

  // Causes everything in indexes above 0 to shift down one index.
  void PopIndexZero();

  // Delete every item in the DataTransferItemList, without checking for
  // permissions or read-only status (for internal use only).
  void ClearAllItems();

private:
  void ClearDataHelper(DataTransferItem* aItem, uint32_t aIndexHint,
                       uint32_t aMozOffsetHint, ErrorResult& aRv);
  DataTransferItem* AppendNewItem(uint32_t aIndex, const nsAString& aType,
                                  nsIVariant* aData, nsIPrincipal* aPrincipal);
  void RegenerateFiles();

  ~DataTransferItemList() {}

  RefPtr<DataTransfer> mParent;
  bool mIsCrossDomainSubFrameDrop;
  bool mIsExternal;
  RefPtr<FileList> mFiles;
  nsTArray<RefPtr<DataTransferItem>> mItems;
  nsTArray<nsTArray<RefPtr<DataTransferItem>>> mIndexedItems;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DataTransferItemList_h
