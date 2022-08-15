/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ClipboardItem.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Record.h"
#include "nsComponentManagerUtils.h"
#include "nsIClipboard.h"
#include "nsIInputStream.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION(ClipboardItem::ItemEntry, mData,
                         mPendingGetTypeRequests)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ClipboardItem::ItemEntry, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ClipboardItem::ItemEntry, Release)

ClipboardItem::ItemEntry::ItemEntry(const nsAString& aType,
                                    const nsACString& aFormat)
    : mType(aType), mFormat(aFormat) {
  // XXX https://bugzilla.mozilla.org/show_bug.cgi?id=1776879.
  // In most of cases, the mType and mFormat are the same, execpt for plain
  // text. We expose it as "text/plain" to the web, but we use "text/unicode"
  // internally to retrieve from system clipboard.
  MOZ_ASSERT_IF(
      !mType.Equals(NS_ConvertUTF8toUTF16(mFormat)),
      mType.EqualsLiteral(kTextMime) && mFormat.EqualsLiteral(kUnicodeMime));
}

void ClipboardItem::ItemEntry::SetData(already_AddRefed<Blob>&& aBlob) {
  // XXX maybe we could consider adding a method to check whether the union
  // object is uninitialized or initialized.
  MOZ_DIAGNOSTIC_ASSERT(!mData.IsString() && !mData.IsBlob(),
                        "Data should be uninitialized.");
  MOZ_DIAGNOSTIC_ASSERT(
      !mLoadingPromise.Exists(),
      "Should not be in the process of loading data from clipboard.");
  mData.SetAsBlob() = std::move(aBlob);
}

void ClipboardItem::ItemEntry::LoadData(nsIGlobalObject& aGlobal,
                                        nsITransferable& aTransferable) {
  // XXX maybe we could consider adding a method to check whether the union
  // object is uninitialized or initialized.
  MOZ_DIAGNOSTIC_ASSERT(!mData.IsString() && !mData.IsBlob(),
                        "Data should be uninitialized.");
  MOZ_DIAGNOSTIC_ASSERT(
      !mLoadingPromise.Exists(),
      "Should not be in the process of loading data from clipboard.");

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsITransferable> trans(&aTransferable);
  clipboard->AsyncGetData(trans, nsIClipboard::kGlobalClipboard)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          /* resolved */
          [self = RefPtr{this}, global = RefPtr{&aGlobal}, trans]() {
            self->mLoadingPromise.Complete();

            nsCOMPtr<nsISupports> data;
            nsresult rv = trans->GetTransferData(self->Format().get(),
                                                 getter_AddRefs(data));
            if (NS_WARN_IF(NS_FAILED(rv))) {
              self->RejectPendingGetTypePromises(rv);
              return;
            }

            RefPtr<Blob> blob;
            if (nsCOMPtr<nsISupportsString> supportsstr =
                    do_QueryInterface(data)) {
              nsAutoString str;
              supportsstr->GetData(str);

              blob = Blob::CreateStringBlob(global, NS_ConvertUTF16toUTF8(str),
                                            self->Type());
            } else if (nsCOMPtr<nsIInputStream> istream =
                           do_QueryInterface(data)) {
              uint64_t available;
              void* data = nullptr;
              nsresult rv =
                  NS_ReadInputStreamToBuffer(istream, &data, -1, &available);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                self->RejectPendingGetTypePromises(rv);
                return;
              }

              blob =
                  Blob::CreateMemoryBlob(global, data, available, self->Type());
            } else if (nsCOMPtr<nsISupportsCString> supportscstr =
                           do_QueryInterface(data)) {
              nsAutoCString str;
              supportscstr->GetData(str);

              blob = Blob::CreateStringBlob(global, str, self->Type());
            }

            if (!blob) {
              self->RejectPendingGetTypePromises(NS_ERROR_DOM_DATA_ERR);
              return;
            }

            self->ResolvePendingGetTypePromises(*blob);
            self->SetData(blob.forget());
          },
          /* rejected */
          [self = RefPtr{this}](nsresult rv) {
            self->mLoadingPromise.Complete();
            self->RejectPendingGetTypePromises(rv);
          })
      ->Track(mLoadingPromise);
}

void ClipboardItem::ItemEntry::ReactPromise(nsIGlobalObject& aGlobal,
                                            Promise& aPromise) {
  // Still loading data from system clipboard.
  if (mLoadingPromise.Exists()) {
    mPendingGetTypeRequests.AppendElement(&aPromise);
    return;
  }

  if (mData.IsBlob()) {
    aPromise.MaybeResolve(mData);
    return;
  }

  // XXX This is for the case that data is from ClipboardItem constructor,
  // maybe we should also load that into a Blob earlier. But Safari returns
  // different `Blob` instances for each `getTypes` call if the string is from
  // ClipboardItem constructor, which is more like our current setup.
  if (mData.IsString()) {
    if (RefPtr<Blob> blob = Blob::CreateStringBlob(
            &aGlobal, NS_ConvertUTF16toUTF8(mData.GetAsString()), mType)) {
      aPromise.MaybeResolve(blob);
      return;
    }
  }

  aPromise.MaybeRejectWithDataError("The data for type '"_ns +
                                    NS_ConvertUTF16toUTF8(mType) +
                                    "' was not found"_ns);
}

void ClipboardItem::ItemEntry::RejectPendingGetTypePromises(nsresult rv) {
  MOZ_ASSERT(!mLoadingPromise.Exists(),
             "Should not be in the process of loading data from clipboard.");
  nsTArray<RefPtr<Promise>> promises = std::move(mPendingGetTypeRequests);
  for (auto& promise : promises) {
    promise->MaybeReject(rv);
  }
}

void ClipboardItem::ItemEntry::ResolvePendingGetTypePromises(Blob& aBlob) {
  MOZ_ASSERT(!mLoadingPromise.Exists(),
             "Should not be in the process of loading data from clipboard.");
  nsTArray<RefPtr<Promise>> promises = std::move(mPendingGetTypeRequests);
  for (auto& promise : promises) {
    promise->MaybeResolve(&aBlob);
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ClipboardItem, mOwner, mItems)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ClipboardItem, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ClipboardItem, Release)

ClipboardItem::ClipboardItem(nsISupports* aOwner,
                             const dom::PresentationStyle aPresentationStyle,
                             nsTArray<RefPtr<ItemEntry>>&& aItems)
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

  nsTArray<RefPtr<ItemEntry>> items;
  for (const auto& entry : aItems.Entries()) {
    nsAutoCString format = entry.mKey.EqualsLiteral(kTextMime)
                               ? nsAutoCString(kUnicodeMime)
                               : NS_ConvertUTF16toUTF8(entry.mKey);
    items.AppendElement(
        MakeRefPtr<ItemEntry>(entry.mKey, format, entry.mValue));
  }

  RefPtr<ClipboardItem> item = MakeRefPtr<ClipboardItem>(
      aGlobal.GetAsSupports(), aOptions.mPresentationStyle, std::move(items));
  return item.forget();
}

void ClipboardItem::GetTypes(nsTArray<nsString>& aTypes) const {
  for (const auto& item : mItems) {
    aTypes.AppendElement(item->Type());
  }
}

already_AddRefed<Promise> ClipboardItem::GetType(const nsAString& aType,
                                                 ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  for (auto& item : mItems) {
    MOZ_ASSERT(item);

    const nsAString& type = item->Type();
    if (type == aType) {
      nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
      if (NS_WARN_IF(!global)) {
        p->MaybeReject(NS_ERROR_UNEXPECTED);
        return p.forget();
      }

      item->ReactPromise(*global, *p);
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
