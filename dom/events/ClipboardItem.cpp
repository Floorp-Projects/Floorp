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

NS_IMPL_CYCLE_COLLECTION(ClipboardItem::ItemEntry, mGlobal, mData,
                         mPendingGetTypeRequests)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ClipboardItem::ItemEntry)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncClipboardRequestCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, PromiseNativeHandler)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ClipboardItem::ItemEntry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ClipboardItem::ItemEntry)

void ClipboardItem::ItemEntry::ResolvedCallback(JSContext* aCx,
                                                JS::Handle<JS::Value> aValue,
                                                ErrorResult& aRv) {
  MOZ_ASSERT(!mTransferable);
  mIsLoadingData = false;
  OwningStringOrBlob clipboardData;
  if (!clipboardData.Init(aCx, aValue)) {
    JS_ClearPendingException(aCx);
    RejectPendingPromises(NS_ERROR_DOM_DATA_ERR);
    return;
  }

  MaybeResolvePendingPromises(std::move(clipboardData));
}

void ClipboardItem::ItemEntry::RejectedCallback(JSContext* aCx,
                                                JS::Handle<JS::Value> aValue,
                                                ErrorResult& aRv) {
  MOZ_ASSERT(!mTransferable);
  mIsLoadingData = false;
  RejectPendingPromises(NS_ERROR_DOM_DATA_ERR);
}

RefPtr<ClipboardItem::ItemEntry::GetDataPromise>
ClipboardItem::ItemEntry::GetData() {
  // Data is still being loaded, either from the system clipboard or the data
  // promise provided to the ClipboardItem constructor.
  if (mIsLoadingData) {
    MOZ_ASSERT(!mData.IsString() && !mData.IsBlob(),
               "Data should be uninitialized");
    MOZ_ASSERT(mLoadResult.isNothing(), "Should have no load result");

    MozPromiseHolder<GetDataPromise> holder;
    RefPtr<GetDataPromise> promise = holder.Ensure(__func__);
    mPendingGetDataRequests.AppendElement(std::move(holder));
    return promise.forget();
  }

  if (NS_FAILED(mLoadResult.value())) {
    MOZ_ASSERT(!mData.IsString() && !mData.IsBlob(),
               "Data should be uninitialized");
    // We are not able to load data, so reject the promise directly.
    return GetDataPromise::CreateAndReject(mLoadResult.value(), __func__);
  }

  MOZ_ASSERT(mData.IsString() || mData.IsBlob(), "Data should be initialized");
  OwningStringOrBlob data(mData);
  return GetDataPromise::CreateAndResolve(std::move(data), __func__);
}

NS_IMETHODIMP ClipboardItem::ItemEntry::OnComplete(nsresult aResult) {
  MOZ_ASSERT(mIsLoadingData);

  mIsLoadingData = false;
  nsCOMPtr<nsITransferable> trans = std::move(mTransferable);

  if (NS_FAILED(aResult)) {
    RejectPendingPromises(aResult);
    return NS_OK;
  }

  MOZ_ASSERT(trans);
  nsCOMPtr<nsISupports> data;
  nsresult rv = trans->GetTransferData(NS_ConvertUTF16toUTF8(mType).get(),
                                       getter_AddRefs(data));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectPendingPromises(rv);
    return NS_OK;
  }

  RefPtr<Blob> blob;
  if (nsCOMPtr<nsISupportsString> supportsstr = do_QueryInterface(data)) {
    nsAutoString str;
    supportsstr->GetData(str);

    blob = Blob::CreateStringBlob(mGlobal, NS_ConvertUTF16toUTF8(str), mType);
  } else if (nsCOMPtr<nsIInputStream> istream = do_QueryInterface(data)) {
    uint64_t available;
    void* data = nullptr;
    rv = NS_ReadInputStreamToBuffer(istream, &data, -1, &available);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RejectPendingPromises(rv);
      return NS_OK;
    }

    blob = Blob::CreateMemoryBlob(mGlobal, data, available, mType);
  } else if (nsCOMPtr<nsISupportsCString> supportscstr =
                 do_QueryInterface(data)) {
    nsAutoCString str;
    supportscstr->GetData(str);

    blob = Blob::CreateStringBlob(mGlobal, str, mType);
  }

  if (!blob) {
    RejectPendingPromises(NS_ERROR_DOM_DATA_ERR);
    return NS_OK;
  }

  OwningStringOrBlob clipboardData;
  clipboardData.SetAsBlob() = std::move(blob);
  MaybeResolvePendingPromises(std::move(clipboardData));
  return NS_OK;
}

void ClipboardItem::ItemEntry::LoadDataFromSystemClipboard(
    nsIAsyncGetClipboardData* aDataGetter) {
  MOZ_ASSERT(aDataGetter);
  // XXX maybe we could consider adding a method to check whether the union
  // object is uninitialized or initialized.
  MOZ_DIAGNOSTIC_ASSERT(!mData.IsString() && !mData.IsBlob(),
                        "Data should be uninitialized.");
  MOZ_DIAGNOSTIC_ASSERT(mLoadResult.isNothing(), "Should have no load result");
  MOZ_DIAGNOSTIC_ASSERT(!mIsLoadingData && !mTransferable,
                        "Should not be in the process of loading data");

  mIsLoadingData = true;

  mTransferable = do_CreateInstance("@mozilla.org/widget/transferable;1");
  if (NS_WARN_IF(!mTransferable)) {
    OnComplete(NS_ERROR_FAILURE);
    return;
  }

  mTransferable->Init(nullptr);
  mTransferable->AddDataFlavor(NS_ConvertUTF16toUTF8(mType).get());

  nsresult rv = aDataGetter->GetData(mTransferable, this);
  if (NS_FAILED(rv)) {
    OnComplete(rv);
    return;
  }
}

void ClipboardItem::ItemEntry::LoadDataFromDataPromise(Promise& aDataPromise) {
  MOZ_DIAGNOSTIC_ASSERT(!mData.IsString() && !mData.IsBlob(),
                        "Data should be uninitialized");
  MOZ_DIAGNOSTIC_ASSERT(mLoadResult.isNothing(), "Should have no load result");
  MOZ_DIAGNOSTIC_ASSERT(!mIsLoadingData && !mTransferable,
                        "Should not be in the process of loading data");

  mIsLoadingData = true;
  aDataPromise.AppendNativeHandler(this);
}

void ClipboardItem::ItemEntry::ReactGetTypePromise(Promise& aPromise) {
  // Data is still being loaded, either from the system clipboard or the data
  // promise provided to the ClipboardItem constructor.
  if (mIsLoadingData) {
    MOZ_ASSERT(!mData.IsString() && !mData.IsBlob(),
               "Data should be uninitialized");
    MOZ_ASSERT(mLoadResult.isNothing(), "Should have no load result.");
    mPendingGetTypeRequests.AppendElement(&aPromise);
    return;
  }

  if (NS_FAILED(mLoadResult.value())) {
    MOZ_ASSERT(!mData.IsString() && !mData.IsBlob(),
               "Data should be uninitialized");
    aPromise.MaybeRejectWithDataError("The data for type '"_ns +
                                      NS_ConvertUTF16toUTF8(mType) +
                                      "' was not found"_ns);
    return;
  }

  MaybeResolveGetTypePromise(mData, aPromise);
}

void ClipboardItem::ItemEntry::MaybeResolveGetTypePromise(
    const OwningStringOrBlob& aData, Promise& aPromise) {
  if (aData.IsBlob()) {
    aPromise.MaybeResolve(aData);
    return;
  }

  // XXX This is for the case that data is from ClipboardItem constructor,
  // maybe we should also load that into a Blob earlier. But Safari returns
  // different `Blob` instances for each `getTypes` call if the string is from
  // ClipboardItem constructor, which is more like our current setup.
  if (RefPtr<Blob> blob = Blob::CreateStringBlob(
          mGlobal, NS_ConvertUTF16toUTF8(aData.GetAsString()), mType)) {
    aPromise.MaybeResolve(blob);
    return;
  }

  aPromise.MaybeRejectWithDataError("The data for type '"_ns +
                                    NS_ConvertUTF16toUTF8(mType) +
                                    "' was not found"_ns);
}

void ClipboardItem::ItemEntry::RejectPendingPromises(nsresult aRv) {
  MOZ_ASSERT(NS_FAILED(aRv), "Should have a failure code here");
  MOZ_DIAGNOSTIC_ASSERT(!mData.IsString() && !mData.IsBlob(),
                        "Data should be uninitialized");
  MOZ_DIAGNOSTIC_ASSERT(mLoadResult.isNothing(), "Should not have load result");
  MOZ_DIAGNOSTIC_ASSERT(!mIsLoadingData && !mTransferable,
                        "Should not be in the process of loading data");
  mLoadResult.emplace(aRv);
  auto promiseHolders = std::move(mPendingGetDataRequests);
  for (auto& promiseHolder : promiseHolders) {
    promiseHolder.Reject(aRv, __func__);
  }
  auto getTypePromises = std::move(mPendingGetTypeRequests);
  for (auto& promise : getTypePromises) {
    promise->MaybeReject(aRv);
  }
}

void ClipboardItem::ItemEntry::MaybeResolvePendingPromises(
    OwningStringOrBlob&& aData) {
  MOZ_DIAGNOSTIC_ASSERT(!mData.IsString() && !mData.IsBlob(),
                        "Data should be uninitialized");
  MOZ_DIAGNOSTIC_ASSERT(mLoadResult.isNothing(), "Should not have load result");
  MOZ_DIAGNOSTIC_ASSERT(!mIsLoadingData && !mTransferable,
                        "Should not be in the process of loading data");
  mLoadResult.emplace(NS_OK);
  mData = std::move(aData);
  auto getDataPromiseHolders = std::move(mPendingGetDataRequests);
  for (auto& promiseHolder : getDataPromiseHolders) {
    OwningStringOrBlob data(mData);
    promiseHolder.Resolve(std::move(data), __func__);
  }
  auto promises = std::move(mPendingGetTypeRequests);
  for (auto& promise : promises) {
    MaybeResolveGetTypePromise(mData, *promise);
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ClipboardItem, mOwner, mItems)

ClipboardItem::ClipboardItem(nsISupports* aOwner,
                             const dom::PresentationStyle aPresentationStyle,
                             nsTArray<RefPtr<ItemEntry>>&& aItems)
    : mOwner(aOwner),
      mPresentationStyle(aPresentationStyle),
      mItems(std::move(aItems)) {}

// static
already_AddRefed<ClipboardItem> ClipboardItem::Constructor(
    const GlobalObject& aGlobal,
    const Record<nsString, OwningNonNull<Promise>>& aItems,
    const ClipboardItemOptions& aOptions, ErrorResult& aRv) {
  if (aItems.Entries().IsEmpty()) {
    aRv.ThrowTypeError("At least one entry required");
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  nsTArray<RefPtr<ItemEntry>> items;
  for (const auto& entry : aItems.Entries()) {
    RefPtr<ItemEntry> item = MakeRefPtr<ItemEntry>(global, entry.mKey);
    item->LoadDataFromDataPromise(*entry.mValue);
    items.AppendElement(std::move(item));
  }

  RefPtr<ClipboardItem> item = MakeRefPtr<ClipboardItem>(
      global, aOptions.mPresentationStyle, std::move(items));
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

      item->ReactGetTypePromise(*p);
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
