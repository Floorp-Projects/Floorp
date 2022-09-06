/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ClipboardItem_h_
#define mozilla_dom_ClipboardItem_h_

#include "mozilla/dom/Blob.h"
#include "mozilla/dom/ClipboardBinding.h"
#include "mozilla/MozPromise.h"

#include "nsWrapperCache.h"

class nsITransferable;

namespace mozilla::dom {

struct ClipboardItemOptions;
template <typename KeyType, typename ValueType>
class Record;
class Promise;

class ClipboardItem final : public nsWrapperCache {
 public:
  class ItemEntry final {
   public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ItemEntry)
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ItemEntry)

    ItemEntry(const nsAString& aType, const nsACString& aFormat);
    ItemEntry(const nsAString& aType, const nsACString& aFormat,
              OwningStringOrBlob&& aData)
        : ItemEntry(aType, aFormat) {
      mData = std::move(aData);
    }
    ItemEntry(const nsAString& aType, const nsACString& aFormat,
              const OwningStringOrBlob& aData)
        : ItemEntry(aType, aFormat) {
      mData = aData;
    }

    const nsString& Type() const { return mType; }
    const nsCString& Format() const { return mFormat; }
    const OwningStringOrBlob& Data() const { return mData; }

    void SetData(already_AddRefed<Blob>&& aBlob);
    void LoadData(nsIGlobalObject& aGlobal, nsITransferable& aTransferable);
    // If clipboard data is in the process of loading from system clipboard, add
    // `aPromise` to the pending list which will be resolved/rejected later when
    // process is finished. Otherwise, resolve/reject `aPromise` based on
    // `mData`.
    void ReactPromise(nsIGlobalObject& aGlobal, Promise& aPromise);

   private:
    ~ItemEntry() { mLoadingPromise.DisconnectIfExists(); }

    void ResolvePendingGetTypePromises(Blob& aBlob);
    void RejectPendingGetTypePromises(nsresult rv);

    nsString mType;
    nsCString mFormat;
    OwningStringOrBlob mData;
    MozPromiseRequestHolder<GenericPromise> mLoadingPromise;
    nsTArray<RefPtr<Promise>> mPendingGetTypeRequests;
  };

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ClipboardItem)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(ClipboardItem)

  ClipboardItem(nsISupports* aOwner, dom::PresentationStyle aPresentationStyle,
                nsTArray<RefPtr<ItemEntry>>&& aItems);

  static already_AddRefed<ClipboardItem> Constructor(
      const GlobalObject& aGlobal,
      const Record<nsString, OwningStringOrBlob>& aItems,
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
