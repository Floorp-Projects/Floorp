/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataTransferItemList.h"

#include "nsContentUtils.h"
#include "nsIGlobalObject.h"
#include "nsIClipboard.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsISupportsPrimitives.h"
#include "nsQueryObject.h"
#include "nsVariant.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventForwards.h"
#include "mozilla/storage/Variant.h"
#include "mozilla/dom/DataTransferItemListBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DataTransferItemList, mParent, mItems,
                                      mIndexedItems, mFiles)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DataTransferItemList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DataTransferItemList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DataTransferItemList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
DataTransferItemList::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return DataTransferItemListBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DataTransferItemList>
DataTransferItemList::Clone(DataTransfer* aParent) const
{
  RefPtr<DataTransferItemList> list =
    new DataTransferItemList(aParent, mIsExternal, mIsCrossDomainSubFrameDrop);

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
      newItems[j] = items[j]->Clone(list);
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

void
DataTransferItemList::Remove(uint32_t aIndex, ErrorResult& aRv)
{
  if (IsReadOnly()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aIndex >= Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  ClearDataHelper(mItems[aIndex], aIndex, -1, aRv);
}

DataTransferItem*
DataTransferItemList::IndexedGetter(uint32_t aIndex, bool& aFound, ErrorResult& aRv) const
{
  if (aIndex >= mItems.Length()) {
    aFound = false;
    return nullptr;
  }

  RefPtr<DataTransferItem> item = mItems[aIndex];

  // Check if the caller is allowed to access the drag data. Callers with
  // chrome privileges can always read the data. During the
  // drop event, allow retrieving the data except in the case where the
  // source of the drag is in a child frame of the caller. In that case,
  // we only allow access to data of the same principal. During other events,
  // only allow access to the data with the same principal.
  nsIPrincipal* principal = nullptr;
  if (mIsCrossDomainSubFrameDrop) {
    principal = nsContentUtils::SubjectPrincipal();
  }

  if (item->Principal() && principal &&
      !principal->Subsumes(item->Principal())) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    aFound = false;
    return nullptr;
  }

  nsCOMPtr<nsIVariant> variantData = item->Data();
  nsCOMPtr<nsISupports> data;
  if (variantData &&
      NS_SUCCEEDED(variantData->GetAsISupports(getter_AddRefs(data))) &&
      data) {
    // Make sure the code that is calling us is same-origin with the data.
    nsCOMPtr<EventTarget> pt = do_QueryInterface(data);
    if (pt) {
      nsresult rv = NS_OK;
      nsIScriptContext* c = pt->GetContextForEventHandlers(&rv);
      if (NS_WARN_IF(NS_FAILED(rv) || !c)) {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return nullptr;
      }

      nsIGlobalObject* go = c->GetGlobalObject();
      if (NS_WARN_IF(!go)) {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return nullptr;
      }

      nsCOMPtr<nsIScriptObjectPrincipal> sp = do_QueryInterface(go);
      MOZ_ASSERT(sp, "This cannot fail on the main thread.");

      nsIPrincipal* dataPrincipal = sp->GetPrincipal();
      if (!principal) {
        principal = nsContentUtils::SubjectPrincipal();
      }
      if (NS_WARN_IF(!dataPrincipal || !principal->Equals(dataPrincipal))) {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return nullptr;
      }
    }
  }

  aFound = true;
  return item;
}

uint32_t
DataTransferItemList::MozItemCount() const
{
  uint32_t length = mIndexedItems.Length();
  // XXX: Compat hack - Index 0 always exists due to changes in internals, but
  // if it is empty, scripts using the moz* APIs should see it as not existing.
  if (length == 1 && mIndexedItems[0].IsEmpty()) {
    return 0;
  }
  return length;
}

void
DataTransferItemList::Clear(ErrorResult& aRv)
{
  if (NS_WARN_IF(IsReadOnly())) {
    return;
  }

  uint32_t count = Length();
  for (uint32_t i = 0; i < count; i++) {
    // We always remove the last item first, to avoid moving items around in
    // memory as much
    Remove(Length() - 1, aRv);
    ENSURE_SUCCESS_VOID(aRv);
  }

  MOZ_ASSERT(Length() == 0);
}

DataTransferItem*
DataTransferItemList::Add(const nsAString& aData,
                          const nsAString& aType,
                          ErrorResult& aRv)
{
  if (NS_WARN_IF(IsReadOnly())) {
    return nullptr;
  }

  nsCOMPtr<nsIVariant> data(new storage::TextVariant(aData));

  nsAutoString format;
  mParent->GetRealFormat(aType, format);

  // We add the textual data to index 0. We set aInsertOnly to true, as we don't
  // want to update an existing entry if it is already present, as per the spec.
  RefPtr<DataTransferItem> item =
    SetDataWithPrincipal(format, data, 0,
                         nsContentUtils::SubjectPrincipal(),
                         /* aInsertOnly = */ true,
                         /* aHidden = */ false,
                         aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(item->Kind() != DataTransferItem::KIND_FILE);

  return item;
}

DataTransferItem*
DataTransferItemList::Add(File& aData, ErrorResult& aRv)
{
  if (IsReadOnly()) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> supports = do_QueryObject(&aData);
  nsCOMPtr<nsIWritableVariant> data = new nsVariant();
  data->SetAsISupports(supports);

  nsAutoString type;
  aData.GetType(type);


  // We need to add this as a new item, as multiple files can't exist in the
  // same item in the Moz DataTransfer layout. It will be appended at the end of
  // the internal specced layout.
  uint32_t index = mIndexedItems.Length();
  RefPtr<DataTransferItem> item =
    SetDataWithPrincipal(type, data, index,
                         nsContentUtils::SubjectPrincipal(),
                         true, false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(item->Kind() == DataTransferItem::KIND_FILE);

  return item;
}

FileList*
DataTransferItemList::Files()
{
  if (!mFiles) {
    mFiles = new FileList(static_cast<nsIDOMDataTransfer*>(mParent));
    RegenerateFiles();
  }

  return mFiles;
}

void
DataTransferItemList::MozRemoveByTypeAt(const nsAString& aType,
                                        uint32_t aIndex,
                                        ErrorResult& aRv)
{
  if (NS_WARN_IF(IsReadOnly() ||
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

      ClearDataHelper(items[index], -1, index, aRv);
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
    nsAutoString type;
    items[i]->GetType(type);
    if (type == aType) {
      ClearDataHelper(items[i], -1, i, aRv);
      return;
    }
  }
}

DataTransferItem*
DataTransferItemList::MozItemByTypeAt(const nsAString& aType, uint32_t aIndex)
{
  if (NS_WARN_IF(aIndex >= mIndexedItems.Length())) {
    return nullptr;
  }

  uint32_t count = mIndexedItems[aIndex].Length();
  for (uint32_t i = 0; i < count; i++) {
    RefPtr<DataTransferItem> item = mIndexedItems[aIndex][i];
    nsString type;
    item->GetType(type);
    if (type.Equals(aType)) {
      return item;
    }
  }

  return nullptr;
}

already_AddRefed<DataTransferItem>
DataTransferItemList::SetDataWithPrincipal(const nsAString& aType,
                                           nsIVariant* aData,
                                           uint32_t aIndex,
                                           nsIPrincipal* aPrincipal,
                                           bool aInsertOnly,
                                           bool aHidden,
                                           ErrorResult& aRv)
{
  if (aIndex < mIndexedItems.Length()) {
    nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[aIndex];
    uint32_t count = items.Length();
    for (uint32_t i = 0; i < count; i++) {
      RefPtr<DataTransferItem> item = items[i];
      nsString type;
      item->GetType(type);
      if (type.Equals(aType)) {
        if (NS_WARN_IF(aInsertOnly)) {
          aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
          return nullptr;
        }

        // don't allow replacing data that has a stronger principal
        bool subsumes;
        if (NS_WARN_IF(item->Principal() && aPrincipal &&
                       (NS_FAILED(aPrincipal->Subsumes(item->Principal(),
                                                       &subsumes))
                        || !subsumes))) {
          aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
          return nullptr;
        }
        item->SetPrincipal(aPrincipal);

        DataTransferItem::eKind oldKind = item->Kind();
        item->SetData(aData);

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
  RefPtr<DataTransferItem> item = AppendNewItem(aIndex, aType, aData, aPrincipal, aHidden);

  if (item->Kind() == DataTransferItem::KIND_FILE) {
    RegenerateFiles();
  }

  return item.forget();
}

DataTransferItem*
DataTransferItemList::AppendNewItem(uint32_t aIndex,
                                    const nsAString& aType,
                                    nsIVariant* aData,
                                    nsIPrincipal* aPrincipal,
                                    bool aHidden)
{
  if (mIndexedItems.Length() <= aIndex) {
    MOZ_ASSERT(mIndexedItems.Length() == aIndex);
    mIndexedItems.AppendElement();
  }
  RefPtr<DataTransferItem> item = new DataTransferItem(this, aType);
  item->SetIndex(aIndex);
  item->SetPrincipal(aPrincipal);
  item->SetData(aData);
  item->SetChromeOnly(aHidden);

  mIndexedItems[aIndex].AppendElement(item);

  // We only want to add the item to the main mItems list if the index we are
  // adding to is 0, or the item we are adding is a file. If we add an item
  // which is not a file to a non-zero index, invariants could be broken.
  // (namely the invariant that there are not 2 non-file entries in the items
  // array with the same type)
  if (!aHidden && (item->Kind() == DataTransferItem::KIND_FILE || aIndex == 0)) {
    mItems.AppendElement(item);
  }

  return item;
}

const nsTArray<RefPtr<DataTransferItem>>*
DataTransferItemList::MozItemsAt(uint32_t aIndex) // -- INDEXED
{
  if (aIndex >= mIndexedItems.Length()) {
    return nullptr;
  }

  return &mIndexedItems[aIndex];
}

bool
DataTransferItemList::IsReadOnly() const
{
  MOZ_ASSERT(mParent);
  return mParent->IsReadOnly();
}

int32_t
DataTransferItemList::ClipboardType() const
{
  MOZ_ASSERT(mParent);
  return mParent->ClipboardType();
}

EventMessage
DataTransferItemList::GetEventMessage() const
{
  MOZ_ASSERT(mParent);
  return mParent->GetEventMessage();
}

void
DataTransferItemList::PopIndexZero()
{
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

void
DataTransferItemList::ClearAllItems()
{
  // We always need to have index 0, so don't delete that one
  mItems.Clear();
  mIndexedItems.Clear();
  mIndexedItems.SetLength(1);

  // Re-generate files (into an empty list)
  RegenerateFiles();
}

void
DataTransferItemList::ClearDataHelper(DataTransferItem* aItem,
                                      uint32_t aIndexHint,
                                      uint32_t aMozOffsetHint,
                                      ErrorResult& aRv)
{
  MOZ_ASSERT(aItem);
  if (NS_WARN_IF(IsReadOnly())) {
    return;
  }

  nsIPrincipal* principal = nsContentUtils::SubjectPrincipal();
  if (aItem->Principal() && principal &&
      !principal->Subsumes(aItem->Principal())) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  // Check if the aIndexHint is actually the index, and then remove the item
  // from aItems
  ErrorResult rv;
  bool found;
  if (IndexedGetter(aIndexHint, found, rv) == aItem) {
    mItems.RemoveElementAt(aIndexHint);
  } else {
    mItems.RemoveElement(aItem);
  }
  rv.SuppressException();

  // Check if the aMozIndexHint and aMozOffsetHint are actually the index and
  // offset, and then remove them from mIndexedItems
  MOZ_ASSERT(aItem->Index() < mIndexedItems.Length());
  nsTArray<RefPtr<DataTransferItem>>& items = mIndexedItems[aItem->Index()];
  if (aMozOffsetHint < items.Length() && aItem == items[aMozOffsetHint]) {
    items.RemoveElementAt(aMozOffsetHint);
  } else {
    items.RemoveElement(aItem);
  }

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

void
DataTransferItemList::RegenerateFiles()
{
  // We don't want to regenerate the files list unless we already have a files
  // list. That way we can avoid the unnecessary work if the user never touches
  // the files list.
  if (mFiles) {
    // We clear the list rather than performing smaller updates, because it
    // simplifies the logic greatly on this code path, which should be very
    // infrequently used.
    mFiles->Clear();

    uint32_t count = Length();
    for (uint32_t i = 0; i < count; i++) {
      ErrorResult rv;
      bool found;
      RefPtr<DataTransferItem> item = IndexedGetter(i, found, rv);
      if (NS_WARN_IF(!found || rv.Failed())) {
        continue;
      }

      if (item->Kind() == DataTransferItem::KIND_FILE) {
        RefPtr<File> file = item->GetAsFile(rv);
        if (NS_WARN_IF(rv.Failed() || !file)) {
          continue;
        }
        mFiles->Append(file);
      }
    }
  }
}

} // namespace mozilla
} // namespace dom
