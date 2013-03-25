/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGNUMBERLIST_H__
#define MOZILLA_DOMSVGNUMBERLIST_H__

#include "DOMSVGAnimatedNumberList.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsTArray.h"
#include "SVGNumberList.h"
#include "nsIDOMSVGNumber.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"

class nsSVGElement;

namespace mozilla {

class DOMSVGNumber;

/**
 * Class DOMSVGNumberList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGNumberList objects.
 *
 * See the architecture comment in DOMSVGAnimatedNumberList.h.
 *
 * This class is strongly intertwined with DOMSVGAnimatedNumberList and
 * DOMSVGNumber. We are a friend of DOMSVGAnimatedNumberList, and are
 * responsible for nulling out our DOMSVGAnimatedNumberList's pointer to us
 * when we die, essentially making its pointer to us a weak pointer. Similarly,
 * our DOMSVGNumber items are friends of us and responsible for nulling out our
 * pointers to them.
 *
 * Our DOM items are created lazily on demand as and when script requests them.
 */
class DOMSVGNumberList MOZ_FINAL : public nsISupports,
                                   public nsWrapperCache
{
  friend class DOMSVGNumber;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGNumberList)

  DOMSVGNumberList(DOMSVGAnimatedNumberList *aAList,
                   const SVGNumberList &aInternalList)
    : mAList(aAList)
  {
    SetIsDOMBinding();

    // aInternalList must be passed in explicitly because we can't use
    // InternalList() here. (Because it depends on IsAnimValList, which depends
    // on this object having been assigned to aAList's mBaseVal or mAnimVal,
    // which hasn't happend yet.)

    InternalListLengthWillChange(aInternalList.Length()); // Sync mItems
  }

  ~DOMSVGNumberList() {
    // Our mAList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mAList is null.
    if (mAList) {
      ( IsAnimValList() ? mAList->mAnimVal : mAList->mBaseVal ) = nullptr;
    }
  }

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope) MOZ_OVERRIDE;

  nsISupports* GetParentObject()
  {
    return static_cast<nsIContent*>(Element());
  }

  /**
   * This will normally be the same as InternalList().Length(), except if we've
   * hit OOM in which case our length will be zero.
   */
  uint32_t LengthNoFlush() const {
    NS_ABORT_IF_FALSE(mItems.Length() == 0 ||
                      mItems.Length() == InternalList().Length(),
                      "DOM wrapper's list length is out of sync");
    return mItems.Length();
  }

  /// Called to notify us to syncronize our length and detach excess items.
  void InternalListLengthWillChange(uint32_t aNewLength);

  uint32_t NumberOfItems() const
  {
    if (IsAnimValList()) {
      Element()->FlushAnimations();
    }
    return LengthNoFlush();
  }
  void Clear(ErrorResult& error);
  already_AddRefed<nsIDOMSVGNumber> Initialize(nsIDOMSVGNumber *newItem,
                                               ErrorResult& error);
  nsIDOMSVGNumber* GetItem(uint32_t index, ErrorResult& error)
  {
    bool found;
    nsIDOMSVGNumber* item = IndexedGetter(index, found, error);
    if (!found) {
      error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    }
    return item;
  }
  nsIDOMSVGNumber* IndexedGetter(uint32_t index, bool& found,
                                 ErrorResult& error);
  already_AddRefed<nsIDOMSVGNumber> InsertItemBefore(nsIDOMSVGNumber *newItem,
                                                     uint32_t index,
                                                     ErrorResult& error);
  already_AddRefed<nsIDOMSVGNumber> ReplaceItem(nsIDOMSVGNumber *newItem,
                                                uint32_t index,
                                                ErrorResult& error);
  already_AddRefed<nsIDOMSVGNumber> RemoveItem(uint32_t index,
                                               ErrorResult& error);
  already_AddRefed<nsIDOMSVGNumber> AppendItem(nsIDOMSVGNumber *newItem,
                                               ErrorResult& error)
  {
    return InsertItemBefore(newItem, LengthNoFlush(), error);
  }
  uint32_t Length() const
  {
    return NumberOfItems();
  }

private:

  nsSVGElement* Element() const {
    return mAList->mElement;
  }

  uint8_t AttrEnum() const {
    return mAList->mAttrEnum;
  }

  /// Used to determine if this list is the baseVal or animVal list.
  bool IsAnimValList() const {
    NS_ABORT_IF_FALSE(this == mAList->mBaseVal || this == mAList->mAnimVal,
                      "Calling IsAnimValList() too early?!");
    return this == mAList->mAnimVal;
  }

  /**
   * Get a reference to this object's corresponding internal SVGNumberList.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal lists. This means that animVal lists don't
   * get const protection, but our setter methods guard against changing
   * animVal lists.
   */
  SVGNumberList& InternalList() const;

  /// Creates a DOMSVGNumber for aIndex, if it doesn't already exist.
  void EnsureItemAt(uint32_t aIndex);

  void MaybeInsertNullInAnimValListAt(uint32_t aIndex);
  void MaybeRemoveItemFromAnimValListAt(uint32_t aIndex);

  // Weak refs to our DOMSVGNumber items. The items are friends and take care
  // of clearing our pointer to them when they die.
  FallibleTArray<DOMSVGNumber*> mItems;

  nsRefPtr<DOMSVGAnimatedNumberList> mAList;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGNUMBERLIST_H__
