/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGTRANSFORMLIST_H__
#define MOZILLA_DOMSVGTRANSFORMLIST_H__

#include "DOMSVGAnimatedTransformList.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsTArray.h"
#include "SVGTransformList.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"

class nsIDOMSVGTransform;
class nsSVGElement;

namespace mozilla {

namespace dom {
class SVGMatrix;
}

class DOMSVGTransform;

/**
 * Class DOMSVGTransformList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGTransformList objects.
 *
 * See the architecture comment in DOMSVGAnimatedTransformList.h.
 */
class DOMSVGTransformList MOZ_FINAL : public nsISupports,
                                      public nsWrapperCache
{
  friend class DOMSVGTransform;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGTransformList)

  DOMSVGTransformList(DOMSVGAnimatedTransformList *aAList,
                      const SVGTransformList &aInternalList)
    : mAList(aAList)
  {
    SetIsDOMBinding();

    // aInternalList must be passed in explicitly because we can't use
    // InternalList() here. (Because it depends on IsAnimValList, which depends
    // on this object having been assigned to aAList's mBaseVal or mAnimVal,
    // which hasn't happend yet.)

    InternalListLengthWillChange(aInternalList.Length()); // Sync mItems
  }

  ~DOMSVGTransformList() {
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
    NS_ABORT_IF_FALSE(mItems.IsEmpty() ||
      mItems.Length() == InternalList().Length(),
      "DOM wrapper's list length is out of sync");
    return mItems.Length();
  }

  /// Called to notify us to synchronize our length and detach excess items.
  void InternalListLengthWillChange(uint32_t aNewLength);

  uint32_t NumberOfItems() const
  {
    if (IsAnimValList()) {
      Element()->FlushAnimations();
    }
    return LengthNoFlush();
  }
  void Clear(ErrorResult& error);
  already_AddRefed<DOMSVGTransform> Initialize(DOMSVGTransform& newItem,
                                               ErrorResult& error);
  DOMSVGTransform* GetItem(uint32_t index, ErrorResult& error)
  {
    bool found;
    DOMSVGTransform* item = IndexedGetter(index, found, error);
    if (!found) {
      error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    }
    return item;
  }
  DOMSVGTransform* IndexedGetter(uint32_t index, bool& found,
                                 ErrorResult& error);
  already_AddRefed<DOMSVGTransform> InsertItemBefore(DOMSVGTransform& newItem,
                                                     uint32_t index,
                                                     ErrorResult& error);
  already_AddRefed<DOMSVGTransform> ReplaceItem(DOMSVGTransform& newItem,
                                                uint32_t index,
                                                ErrorResult& error);
  already_AddRefed<DOMSVGTransform> RemoveItem(uint32_t index,
                                               ErrorResult& error);
  already_AddRefed<DOMSVGTransform> AppendItem(DOMSVGTransform& newItem,
                                               ErrorResult& error)
  {
    return InsertItemBefore(newItem, LengthNoFlush(), error);
  }
  already_AddRefed<DOMSVGTransform> CreateSVGTransformFromMatrix(dom::SVGMatrix& matrix);
  already_AddRefed<DOMSVGTransform> Consolidate(ErrorResult& error);
  uint32_t Length() const
  {
    return NumberOfItems();
  }

private:

  nsSVGElement* Element() const {
    return mAList->mElement;
  }

  /// Used to determine if this list is the baseVal or animVal list.
  bool IsAnimValList() const {
    NS_ABORT_IF_FALSE(this == mAList->mBaseVal || this == mAList->mAnimVal,
                      "Calling IsAnimValList() too early?!");
    return this == mAList->mAnimVal;
  }

  /**
   * Get a reference to this object's corresponding internal SVGTransformList.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal lists. This means that animVal lists don't
   * get const protection, but our setter methods guard against changing
   * animVal lists.
   */
  SVGTransformList& InternalList() const;

  /// Creates a DOMSVGTransform for aIndex, if it doesn't already exist.
  void EnsureItemAt(uint32_t aIndex);

  void MaybeInsertNullInAnimValListAt(uint32_t aIndex);
  void MaybeRemoveItemFromAnimValListAt(uint32_t aIndex);

  // Weak refs to our DOMSVGTransform items. The items are friends and take care
  // of clearing our pointer to them when they die.
  FallibleTArray<DOMSVGTransform*> mItems;

  nsRefPtr<DOMSVGAnimatedTransformList> mAList;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGTRANSFORMLIST_H__
