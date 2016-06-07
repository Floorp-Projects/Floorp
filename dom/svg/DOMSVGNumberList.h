/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGNUMBERLIST_H__
#define MOZILLA_DOMSVGNUMBERLIST_H__

#include "DOMSVGAnimatedNumberList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsTArray.h"
#include "SVGNumberList.h"
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
class DOMSVGNumberList final : public nsISupports,
                               public nsWrapperCache
{
  friend class AutoChangeNumberListNotifier;
  friend class DOMSVGNumber;

  ~DOMSVGNumberList() {
    // Our mAList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mAList is null.
    if (mAList) {
      ( IsAnimValList() ? mAList->mAnimVal : mAList->mBaseVal ) = nullptr;
    }
  }

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGNumberList)

  DOMSVGNumberList(DOMSVGAnimatedNumberList *aAList,
                   const SVGNumberList &aInternalList)
    : mAList(aAList)
  {
    // aInternalList must be passed in explicitly because we can't use
    // InternalList() here. (Because it depends on IsAnimValList, which depends
    // on this object having been assigned to aAList's mBaseVal or mAnimVal,
    // which hasn't happend yet.)

    InternalListLengthWillChange(aInternalList.Length()); // Sync mItems
  }

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject()
  {
    return static_cast<nsIContent*>(Element());
  }

  /**
   * This will normally be the same as InternalList().Length(), except if we've
   * hit OOM in which case our length will be zero.
   */
  uint32_t LengthNoFlush() const {
    MOZ_ASSERT(mItems.Length() == 0 ||
               mItems.Length() == InternalList().Length(),
               "DOM wrapper's list length is out of sync");
    return mItems.Length();
  }

  /// Called to notify us to syncronize our length and detach excess items.
  void InternalListLengthWillChange(uint32_t aNewLength);

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool IsAnimating() const {
    return mAList->IsAnimating();
  }
  /**
   * Returns true if there is an animated list mirroring the base list.
   */
  bool AnimListMirrorsBaseList() const {
    return mAList->mAnimVal && !mAList->IsAnimating();
  }

  uint32_t NumberOfItems() const
  {
    if (IsAnimValList()) {
      Element()->FlushAnimations();
    }
    return LengthNoFlush();
  }
  void Clear(ErrorResult& error);
  already_AddRefed<DOMSVGNumber> Initialize(DOMSVGNumber& newItem,
                                            ErrorResult& error);
  already_AddRefed<DOMSVGNumber> GetItem(uint32_t index, ErrorResult& error);
  already_AddRefed<DOMSVGNumber> IndexedGetter(uint32_t index, bool& found,
                                               ErrorResult& error);
  already_AddRefed<DOMSVGNumber> InsertItemBefore(DOMSVGNumber& newItem,
                                                  uint32_t index, ErrorResult& error);
  already_AddRefed<DOMSVGNumber> ReplaceItem(DOMSVGNumber& newItem, uint32_t index,
                                             ErrorResult& error);
  already_AddRefed<DOMSVGNumber> RemoveItem(uint32_t index,
                                            ErrorResult& error);
  already_AddRefed<DOMSVGNumber> AppendItem(DOMSVGNumber& newItem,
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
    MOZ_ASSERT(this == mAList->mBaseVal || this == mAList->mAnimVal,
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

  /// Returns the DOMSVGNumber at aIndex, creating it if necessary.
  already_AddRefed<DOMSVGNumber> GetItemAt(uint32_t aIndex);

  void MaybeInsertNullInAnimValListAt(uint32_t aIndex);
  void MaybeRemoveItemFromAnimValListAt(uint32_t aIndex);

  // Weak refs to our DOMSVGNumber items. The items are friends and take care
  // of clearing our pointer to them when they die.
  FallibleTArray<DOMSVGNumber*> mItems;

  RefPtr<DOMSVGAnimatedNumberList> mAList;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGNUMBERLIST_H__
