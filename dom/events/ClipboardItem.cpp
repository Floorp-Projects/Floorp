/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ClipboardItem.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Record.h"

namespace mozilla::dom {

void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                                 ClipboardItem::ItemEntry& aField,
                                 const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mData, aName, aFlags);
}

void ImplCycleCollectionUnlink(ClipboardItem::ItemEntry& aField) {
  ImplCycleCollectionUnlink(aField.mData);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ClipboardItem, mOwner, mItems)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ClipboardItem, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ClipboardItem, Release)

ClipboardItem::ClipboardItem(nsISupports* aOwner,
                             const dom::PresentationStyle aPresentationStyle,
                             nsTArray<ItemEntry>&& aItems)
    : mOwner(aOwner),
      mPresentationStyle(aPresentationStyle),
      mItems(std::move(aItems)) {}

// static
already_AddRefed<ClipboardItem> ClipboardItem::Constructor(
    const GlobalObject& aGlobal,
    const Record<nsString, OwningStringOrBlob>& aItems,
    const ClipboardItemOptions& aOptions, ErrorResult& aRv) {
  if (aItems.Entries().IsEmpty()) {
    aRv.ThrowTypeError("At least one entry required");
    return nullptr;
  }

  nsTArray<ItemEntry> items;
  for (const auto& entry : aItems.Entries()) {
    ItemEntry* item = items.AppendElement();
    item->mType = entry.mKey;
    item->mData = entry.mValue;
  }

  RefPtr<ClipboardItem> item = new ClipboardItem(
      aGlobal.GetAsSupports(), aOptions.mPresentationStyle, std::move(items));
  return item.forget();
}

void ClipboardItem::GetTypes(nsTArray<nsString>& aTypes) const {
  for (const auto& item : mItems) {
    aTypes.AppendElement(item.mType);
  }
}

already_AddRefed<Promise> ClipboardItem::GetType(const nsAString& aType,
                                                 ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  for (const auto& item : mItems) {
    if (item.mType == aType) {
      if (item.mData.IsBlob()) {
        p->MaybeResolve(item.mData);
      } else {
        NS_ConvertUTF16toUTF8 string(item.mData.GetAsString());
        RefPtr<Blob> blob = Blob::CreateStringBlob(global, string, item.mType);
        p->MaybeResolve(blob);
      }
      return p.forget();
    }
  }

  p->MaybeRejectWithNotFoundError(
      "The type '"_ns + NS_ConvertUTF16toUTF8(aType) + "' was not found"_ns);
  return p.forget();
}

JSObject* ClipboardItem::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::ClipboardItem_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
