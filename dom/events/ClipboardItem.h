/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ClipboardItem_h_
#define mozilla_dom_ClipboardItem_h_

#include "mozilla/dom/Blob.h"
#include "mozilla/dom/ClipboardBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/MozPromise.h"

#include "nsIClipboard.h"
#include "nsWrapperCache.h"

class nsITransferable;

namespace mozilla::dom {

struct ClipboardItemOptions;
template <typename KeyType, typename ValueType>
class Record;
class Promise;

class ClipboardItem final : public nsWrapperCache {
 public:
  class ItemEntry final : public PromiseNativeHandler,
                          public nsIAsyncClipboardRequestCallback {
   public:
    using GetDataPromise =
        MozPromise<OwningStringOrBlob, nsresult, /* IsExclusive = */ true>;

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIASYNCCLIPBOARDREQUESTCALLBACK
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ItemEntry, PromiseNativeHandler)

    explicit ItemEntry(nsIGlobalObject* aGlobal, const nsAString& aType)
        : mGlobal(aGlobal), mType(aType) {
      MOZ_ASSERT(mGlobal);
    }
    ItemEntry(nsIGlobalObject* aGlobal, const nsAString& aType,
              const nsAString& aData)
        : ItemEntry(aGlobal, aType) {
      mLoadResult.emplace(NS_OK);
      mData.SetAsString() = aData;
    }

    // PromiseNativeHandler
    void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                          ErrorResult& aRv) override;
    void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                          ErrorResult& aRv) override;

    const nsString& Type() const { return mType; }
    RefPtr<GetDataPromise> GetData();

    //  Load data from system clipboard.
    void LoadDataFromSystemClipboard(nsIAsyncGetClipboardData* aDataGetter);
    void LoadDataFromDataPromise(Promise& aDataPromise);

    // If clipboard data is in the process of loading from either system
    // clipboard or data promise, add `aPromise` to the pending list which will
    // be resolved/rejected later when process is finished. Otherwise,
    // resolve/reject `aPromise` based on cached result and data.
    void ReactGetTypePromise(Promise& aPromise);

   private:
    ~ItemEntry() {
      if (!mPendingGetDataRequests.IsEmpty()) {
        RejectPendingPromises(NS_ERROR_FAILURE);
      }
    };

    void MaybeResolveGetTypePromise(const OwningStringOrBlob& aData,
                                    Promise& aPromise);
    void MaybeResolvePendingPromises(OwningStringOrBlob&& aData);
    void RejectPendingPromises(nsresult rv);

    nsCOMPtr<nsIGlobalObject> mGlobal;

    // MIME type of this entry.
    nsString mType;

    // Cache the loading result.
    OwningStringOrBlob mData;
    Maybe<nsresult> mLoadResult;

    // Indicates if the data is still being loaded.
    bool mIsLoadingData = false;
    nsCOMPtr<nsITransferable> mTransferable;

    // Pending promises for data retrieval requests.
    nsTArray<MozPromiseHolder<GetDataPromise>> mPendingGetDataRequests;
    nsTArray<RefPtr<Promise>> mPendingGetTypeRequests;
  };

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ClipboardItem)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(ClipboardItem)

  ClipboardItem(nsISupports* aOwner, dom::PresentationStyle aPresentationStyle,
                nsTArray<RefPtr<ItemEntry>>&& aItems);

  static already_AddRefed<ClipboardItem> Constructor(
      const GlobalObject& aGlobal,
      const Record<nsString, OwningNonNull<Promise>>& aItems,
      const ClipboardItemOptions& aOptions, ErrorResult& aRv);

  dom::PresentationStyle PresentationStyle() const {
    return mPresentationStyle;
  };
  void GetTypes(nsTArray<nsString>& aTypes) const;

  already_AddRefed<Promise> GetType(const nsAString& aType, ErrorResult& aRv);

  nsISupports* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  const nsTArray<RefPtr<ItemEntry>>& Entries() const { return mItems; }

 private:
  ~ClipboardItem() = default;

  nsCOMPtr<nsISupports> mOwner;
  dom::PresentationStyle mPresentationStyle;
  nsTArray<RefPtr<ItemEntry>> mItems;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ClipboardItem_h_
