/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataTransferItemList.h"

#include "nsContentUtils.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsQueryObject.h"
#include "nsVariant.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventForwards.h"
#include "mozilla/storage/Variant.h"
#include "mozilla/dom/DataTransferItemListBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DataTransferItemList, mDataTransfer,
                                      mItems, mIndexedItems, mFiles)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DataTransferItemList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DataTransferItemList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DataTransferItemList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* DataTransferItemList::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return DataTransferItemList_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DataTransferItemList> DataTransferItemList::Clone(
    DataTransfer* aDataTransfer) const {
  RefPtr<DataTransferItemList> list = new DataTransferItemList(aDataTransfer);

  // We need to clone the mItems and mIndexedItems lists while keeping the same
  // correspondences between the mIndexedItems and mItems lists (namely, if an
  // item is in mIndexedItems, and mItems it must have the same new identity)

  // First, we copy over indexedItems, and clone every entry. Then, we go over
  // mItems. For every entry, we use its mIndex property to locate it in
  // mIndexedItems on the original DataTransferItemList, and then copy over the
  // reference from the same index pair on the new DataTransferItemList

  list->mIndexedItems.SetLength(mIndexedItems.Length());
  list->mItems.SetLength(mItems.Length());

  // Copy over mIndexedItems, cloning every entry
  for (uint32_t i = 0; i < mIndexedItems.Length(); i++) {
    const nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[i];
    nsTArray<RefPtr<DataTransferItem>>& newItems = list->mIndexedItems[i];
    newItems.SetLength(items.Length());
    for (uint32_t j = 0; j < items.Length(); j++) {
      newItems[j] = items[j]->Clone(aDataTransfer);
    }
  }

  // Copy over mItems, getting the actual entries from mIndexedItems
  for (uint32_t i = 0; i < mItems.Length(); i++) {
    uint32_t index = mItems[i]->Index();
    MOZ_ASSERT(index < mIndexedItems.Length());
    uint32_t subIndex = mIndexedItems[index].IndexOf(mItems[i]);

    // Copy over the reference
    list->mItems[i] = list->mIndexedItems[index][subIndex];
  }

  return list.forget();
}

void DataTransferItemList::Remove(uint32_t aIndex,
                                  nsIPrincipal& aSubjectPrincipal,
                                  ErrorResult& aRv) {
  if (mDataTransfer->IsReadOnly()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aIndex >= Length()) {
    return;
  }

  ClearDataHelper(mItems[aIndex], aIndex, -1, aSubjectPrincipal, aRv);
}

DataTransferItem* DataTransferItemList::IndexedGetter(uint32_t aIndex,
                                                      bool& aFound) const {
  if (aIndex >= mItems.Length()) {
    aFound = false;
    return nullptr;
  }

  MOZ_ASSERT(mItems[aIndex]);
  aFound = true;
  return mItems[aIndex];
}

uint32_t DataTransferItemList::MozItemCount() const {
  uint32_t length = mIndexedItems.Length();
  // XXX: Compat hack - Index 0 always exists due to changes in internals, but
  // if it is empty, scripts using the moz* APIs should see it as not existing.
  if (length == 1 && mIndexedItems[0].IsEmpty()) {
    return 0;
  }
  return length;
}

void DataTransferItemList::Clear(nsIPrincipal& aSubjectPrincipal,
                                 ErrorResult& aRv) {
  if (NS_WARN_IF(mDataTransfer->IsReadOnly())) {
    return;
  }

  uint32_t count = Length();
  for (uint32_t i = 0; i < count; i++) {
    // We always remove the last item first, to avoid moving items around in
    // memory as much
    Remove(Length() - 1, aSubjectPrincipal, aRv);
    ENSURE_SUCCESS_VOID(aRv);
  }

  MOZ_ASSERT(Length() == 0);
}

DataTransferItem* DataTransferItemList::Add(const nsAString& aData,
                                            const nsAString& aType,
                                            nsIPrincipal& aSubjectPrincipal,
                                            ErrorResult& aRv) {
  if (NS_WARN_IF(mDataTransfer->IsReadOnly())) {
    return nullptr;
  }

  RefPtr<nsVariantCC> data(new nsVariantCC());
  data->SetAsAString(aData);

  nsAutoString format;
  mDataTransfer->GetRealFormat(aType, format);

  if (!DataTransfer::PrincipalMaySetData(format, data, &aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // We add the textual data to index 0. We set aInsertOnly to true, as we don't
  // want to update an existing entry if it is already present, as per the spec.
  RefPtr<DataTransferItem> item =
      SetDataWithPrincipal(format, data, 0, &aSubjectPrincipal,
                           /* aInsertOnly = */ true,
                           /* aHidden = */ false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(item->Kind() != DataTransferItem::KIND_FILE);

  return item;
}

DataTransferItem* DataTransferItemList::Add(File& aData,
                                            nsIPrincipal& aSubjectPrincipal,
                                            ErrorResult& aRv) {
  if (mDataTransfer->IsReadOnly()) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> supports = do_QueryObject(&aData);
  nsCOMPtr<nsIWritableVariant> data = new nsVariantCC();
  data->SetAsISupports(supports);

  nsAutoString type;
  aData.GetType(type);

  if (!DataTransfer::PrincipalMaySetData(type, data, &aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // We need to add this as a new item, as multiple files can't exist in the
  // same item in the Moz DataTransfer layout. It will be appended at the end of
  // the internal specced layout.
  uint32_t index = mIndexedItems.Length();
  RefPtr<DataTransferItem> item =
      SetDataWithPrincipal(type, data, index, &aSubjectPrincipal,
                           /* aInsertOnly = */ true,
                           /* aHidden = */ false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(item->Kind() == DataTransferItem::KIND_FILE);

  return item;
}

already_AddRefed<FileList> DataTransferItemList::Files(
    nsIPrincipal* aPrincipal) {
  // The DataTransfer can hold data with varying principals, coming from
  // different windows. This means that permissions checks need to be made when
  // accessing data from the DataTransfer. With the accessor methods, this is
  // checked by DataTransferItem::Data(), however with files, we keep a cached
  // live copy of the files list for spec compliance.
  //
  // A DataTransfer is only exposed to one webpage, chrome code and expanded
  // principals related to WebExtensions content scripts or user scripts.
  // The chrome code should be able to see all files on the DataTransfer, while
  // the webpage and WebExtensions content scripts and user scripts should only
  // be able to see the files they can see.
  //
  // As chrome code doesn't need as strict spec compliance as web visible code,
  // we generate a new FileList object every time you access the Files list from
  // chrome code, but re-use the cached one when accessing from content code.
  //
  // For WebExtensions content scripts (expanded principals subsuming both
  // the attached web page principal and the extension principal) and
  // WebExtensions user scripts (expanded principals subsuming the attached
  // web page principal but not the extension principal) we also don't cache
  // the FileList as for chrome code (because the webpage principal and other
  // extension content scripts/user scripts principals would not be able to
  // access the cached FileList when accessed by a different expanded principal
  // first, see Bug 1707214).
  //
  // It is not legal to expose an identical DataTransfer object is to multiple
  // different principals without using the `Clone` method or similar to copy it
  // first. If that happens, this method will assert, and return nullptr in
  // release builds. If this functionality is required in the future, a more
  // advanced caching mechanism for the FileList objects will be required.
  RefPtr<FileList> files;
  if (aPrincipal->IsSystemPrincipal() ||
      // WebExtensions content scripts and user scripts.
      nsContentUtils::IsExpandedPrincipal(aPrincipal)) {
    files = new FileList(mDataTransfer);
    GenerateFiles(files, aPrincipal);
    return files.forget();
  }

  if (!mFiles) {
    mFiles = new FileList(mDataTransfer);
    mFilesPrincipal = aPrincipal;
    RegenerateFiles();
  }

  if (!aPrincipal->Subsumes(mFilesPrincipal)) {
    MOZ_ASSERT(false,
               "This DataTransfer should only be accessed by the system "
               "and a single principal");
    return nullptr;
  }

  files = mFiles;
  return files.forget();
}

void DataTransferItemList::MozRemoveByTypeAt(const nsAString& aType,
                                             uint32_t aIndex,
                                             nsIPrincipal& aSubjectPrincipal,
                                             ErrorResult& aRv) {
  if (NS_WARN_IF(mDataTransfer->IsReadOnly() ||
                 aIndex >= mIndexedItems.Length())) {
    return;
  }

  bool removeAll = aType.IsEmpty();

  nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[aIndex];
  uint32_t count = items.Length();
  // We remove the last item of the list repeatedly - that way we don't
  // have to worry about modifying the loop iterator
  if (removeAll) {
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t index = items.Length() - 1;
      MOZ_ASSERT(index == count - i - 1);

      ClearDataHelper(items[index], -1, index, aSubjectPrincipal, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    }

    // items is no longer a valid reference, as removing the last element from
    // it via ClearDataHelper invalidated it. so we can't MOZ_ASSERT that the
    // length is now 0.
    return;
  }

  for (uint32_t i = 0; i < count; ++i) {
    // NOTE: As this is a moz-prefixed API, it works based on internal types.
    nsAutoString type;
    items[i]->GetInternalType(type);
    if (type == aType) {
      ClearDataHelper(items[i], -1, i, aSubjectPrincipal, aRv);
      return;
    }
  }
}

DataTransferItem* DataTransferItemList::MozItemByTypeAt(const nsAString& aType,
                                                        uint32_t aIndex) {
  if (NS_WARN_IF(aIndex >= mIndexedItems.Length())) {
    return nullptr;
  }

  uint32_t count = mIndexedItems[aIndex].Length();
  for (uint32_t i = 0; i < count; i++) {
    RefPtr<DataTransferItem> item = mIndexedItems[aIndex][i];
    // NOTE: As this is a moz-prefixed API it works on internal types
    nsString type;
    item->GetInternalType(type);
    if (type.Equals(aType)) {
      return item;
    }
  }

  return nullptr;
}

already_AddRefed<DataTransferItem> DataTransferItemList::SetDataWithPrincipal(
    const nsAString& aType, nsIVariant* aData, uint32_t aIndex,
    nsIPrincipal* aPrincipal, bool aInsertOnly, bool aHidden,
    ErrorResult& aRv) {
  if (aIndex < mIndexedItems.Length()) {
    nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[aIndex];
    uint32_t count = items.Length();
    for (uint32_t i = 0; i < count; i++) {
      RefPtr<DataTransferItem> item = items[i];
      nsString type;
      item->GetInternalType(type);
      if (type.Equals(aType)) {
        if (NS_WARN_IF(aInsertOnly)) {
          aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
          return nullptr;
        }

        // don't allow replacing data that has a stronger principal
        bool subsumes;
        if (NS_WARN_IF(item->Principal() && aPrincipal &&
                       (NS_FAILED(aPrincipal->Subsumes(item->Principal(),
                                                       &subsumes)) ||
                        !subsumes))) {
          aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
          return nullptr;
        }
        item->SetPrincipal(aPrincipal);

        DataTransferItem::eKind oldKind = item->Kind();
        item->SetData(aData);

        mDataTransfer->TypesListMayHaveChanged();

        if (aIndex != 0) {
          // If the item changes from being a file to not a file or vice-versa,
          // its presence in the mItems array may need to change.
          if (item->Kind() == DataTransferItem::KIND_FILE &&
              oldKind != DataTransferItem::KIND_FILE) {
            // not file => file
            mItems.AppendElement(item);
          } else if (item->Kind() != DataTransferItem::KIND_FILE &&
                     oldKind == DataTransferItem::KIND_FILE) {
            // file => not file
            mItems.RemoveElement(item);
          }
        }

        // Regenerate the Files array if we have modified a file's status
        if (item->Kind() == DataTransferItem::KIND_FILE ||
            oldKind == DataTransferItem::KIND_FILE) {
          RegenerateFiles();
        }

        return item.forget();
      }
    }
  } else {
    // Make sure that we aren't adding past the end of the mIndexedItems array.
    // XXX Should this be a MOZ_ASSERT instead?
    aIndex = mIndexedItems.Length();
  }

  // Add the new item
  RefPtr<DataTransferItem> item =
      AppendNewItem(aIndex, aType, aData, aPrincipal, aHidden);

  if (item->Kind() == DataTransferItem::KIND_FILE) {
    RegenerateFiles();
  }

  return item.forget();
}

DataTransferItem* DataTransferItemList::AppendNewItem(uint32_t aIndex,
                                                      const nsAString& aType,
                                                      nsIVariant* aData,
                                                      nsIPrincipal* aPrincipal,
                                                      bool aHidden) {
  if (mIndexedItems.Length() <= aIndex) {
    MOZ_ASSERT(mIndexedItems.Length() == aIndex);
    mIndexedItems.AppendElement();
  }
  RefPtr<DataTransferItem> item = new DataTransferItem(mDataTransfer, aType);
  item->SetIndex(aIndex);
  item->SetPrincipal(aPrincipal);
  item->SetData(aData);
  item->SetChromeOnly(aHidden);

  mIndexedItems[aIndex].AppendElement(item);

  // We only want to add the item to the main mItems list if the index we are
  // adding to is 0, or the item we are adding is a file. If we add an item
  // which is not a file to a non-zero index, invariants could be broken.
  // (namely the invariant that there are not 2 non-file entries in the items
  // array with the same type).
  //
  // We also want to update our DataTransfer's type list any time we're adding a
  // KIND_FILE item, or an item at index 0.
  if (item->Kind() == DataTransferItem::KIND_FILE || aIndex == 0) {
    if (!aHidden) {
      mItems.AppendElement(item);
    }
    mDataTransfer->TypesListMayHaveChanged();
  }

  return item;
}

void DataTransferItemList::GetTypes(nsTArray<nsString>& aTypes,
                                    CallerType aCallerType) const {
  MOZ_ASSERT(aTypes.IsEmpty());

  if (mIndexedItems.IsEmpty()) {
    return;
  }

  bool foundFile = false;
  for (const RefPtr<DataTransferItem>& item : mIndexedItems[0]) {
    MOZ_ASSERT(item);

    // XXX Why don't we check the caller type with item's permission only
    //     for "Files"?
    if (!foundFile) {
      foundFile = item->Kind() == DataTransferItem::KIND_FILE;
    }

    if (item->ChromeOnly() && aCallerType != CallerType::System) {
      continue;
    }

    // NOTE: The reason why we get the internal type here is because we want
    // kFileMime to appear in the types list for backwards compatibility
    // reasons.
    nsAutoString type;
    item->GetInternalType(type);
    if (item->Kind() != DataTransferItem::KIND_FILE ||
        type.EqualsASCII(kFileMime)) {
      aTypes.AppendElement(type);
    }
  }

  if (foundFile) {
    aTypes.AppendElement(u"Files"_ns);
  }
}

bool DataTransferItemList::HasType(const nsAString& aType) const {
  MOZ_ASSERT(!aType.EqualsASCII("Files"), "Use HasFile instead");
  if (mIndexedItems.IsEmpty()) {
    return false;
  }

  for (const RefPtr<DataTransferItem>& item : mIndexedItems[0]) {
    if (item->IsInternalType(aType)) {
      return true;
    }
  }
  return false;
}

bool DataTransferItemList::HasFile() const {
  if (mIndexedItems.IsEmpty()) {
    return false;
  }

  for (const RefPtr<DataTransferItem>& item : mIndexedItems[0]) {
    if (item->Kind() == DataTransferItem::KIND_FILE) {
      return true;
    }
  }
  return false;
}

const nsTArray<RefPtr<DataTransferItem>>* DataTransferItemList::MozItemsAt(
    uint32_t aIndex)  // -- INDEXED
{
  if (aIndex >= mIndexedItems.Length()) {
    return nullptr;
  }

  return &mIndexedItems[aIndex];
}

void DataTransferItemList::PopIndexZero() {
  MOZ_ASSERT(mIndexedItems.Length() > 1);
  MOZ_ASSERT(mIndexedItems[0].IsEmpty());

  mIndexedItems.RemoveElementAt(0);

  // Update the index of every element which has now been shifted
  for (uint32_t i = 0; i < mIndexedItems.Length(); i++) {
    nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[i];
    for (uint32_t j = 0; j < items.Length(); j++) {
      items[j]->SetIndex(i);
    }
  }
}

void DataTransferItemList::ClearAllItems() {
  // We always need to have index 0, so don't delete that one
  mItems.Clear();
  mIndexedItems.Clear();
  mIndexedItems.SetLength(1);
  mDataTransfer->TypesListMayHaveChanged();

  // Re-generate files (into an empty list)
  RegenerateFiles();
}

void DataTransferItemList::ClearDataHelper(DataTransferItem* aItem,
                                           uint32_t aIndexHint,
                                           uint32_t aMozOffsetHint,
                                           nsIPrincipal& aSubjectPrincipal,
                                           ErrorResult& aRv) {
  MOZ_ASSERT(aItem);
  if (NS_WARN_IF(mDataTransfer->IsReadOnly())) {
    return;
  }

  if (aItem->Principal() && !aSubjectPrincipal.Subsumes(aItem->Principal())) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  // Check if the aIndexHint is actually the index, and then remove the item
  // from aItems
  bool found;
  if (IndexedGetter(aIndexHint, found) == aItem) {
    mItems.RemoveElementAt(aIndexHint);
  } else {
    mItems.RemoveElement(aItem);
  }

  // Check if the aMozIndexHint and aMozOffsetHint are actually the index and
  // offset, and then remove them from mIndexedItems
  MOZ_ASSERT(aItem->Index() < mIndexedItems.Length());
  nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[aItem->Index()];
  if (aMozOffsetHint < items.Length() && aItem == items[aMozOffsetHint]) {
    items.RemoveElementAt(aMozOffsetHint);
  } else {
    items.RemoveElement(aItem);
  }

  mDataTransfer->TypesListMayHaveChanged();

  // Check if we should remove the index. We never remove index 0.
  if (items.Length() == 0 && aItem->Index() != 0) {
    mIndexedItems.RemoveElementAt(aItem->Index());

    // Update the index of every element which has now been shifted
    for (uint32_t i = aItem->Index(); i < mIndexedItems.Length(); i++) {
      nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[i];
      for (uint32_t j = 0; j < items.Length(); j++) {
        items[j]->SetIndex(i);
      }
    }
  }

  // Give the removed item the invalid index
  aItem->SetIndex(-1);

  if (aItem->Kind() == DataTransferItem::KIND_FILE) {
    RegenerateFiles();
  }
}

void DataTransferItemList::RegenerateFiles() {
  // We don't want to regenerate the files list unless we already have a files
  // list. That way we can avoid the unnecessary work if the user never touches
  // the files list.
  if (mFiles) {
    // We clear the list rather than performing smaller updates, because it
    // simplifies the logic greatly on this code path, which should be very
    // infrequently used.
    mFiles->Clear();

    DataTransferItemList::GenerateFiles(mFiles, mFilesPrincipal);
  }
}

void DataTransferItemList::GenerateFiles(FileList* aFiles,
                                         nsIPrincipal* aFilesPrincipal) {
  MOZ_ASSERT(aFiles);
  MOZ_ASSERT(aFilesPrincipal);

  // For non-system principals, the Files list should be empty if the
  // DataTransfer is protected.
  if (!aFilesPrincipal->IsSystemPrincipal() && mDataTransfer->IsProtected()) {
    return;
  }

  uint32_t count = Length();
  for (uint32_t i = 0; i < count; i++) {
    bool found;
    RefPtr<DataTransferItem> item = IndexedGetter(i, found);
    MOZ_ASSERT(found);

    if (item->Kind() == DataTransferItem::KIND_FILE) {
      RefPtr<File> file = item->GetAsFile(*aFilesPrincipal, IgnoreErrors());
      if (NS_WARN_IF(!file)) {
        continue;
      }
      aFiles->Append(file);
    }
  }
}

}  // namespace mozilla::dom
