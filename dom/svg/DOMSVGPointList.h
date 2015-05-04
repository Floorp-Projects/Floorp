/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGPOINTLIST_H__
#define MOZILLA_DOMSVGPOINTLIST_H__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsSVGElement.h"
#include "nsTArray.h"
#include "SVGPointList.h" // IWYU pragma: keep
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {

class DOMSVGPoint;
class nsISVGPoint;
class SVGAnimatedPointList;

/**
 * Class DOMSVGPointList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGPointList objects.
 *
 * See the architecture comment in DOMSVGAnimatedLengthList.h first (that's
 * LENGTH list), then continue reading the remainder of this comment.
 *
 * The architecture of this class is very similar to that of DOMSVGLengthList
 * except that, since there is no nsIDOMSVGAnimatedPointList interface
 * in SVG, we have no parent DOMSVGAnimatedPointList (unlike DOMSVGLengthList
 * which has a parent DOMSVGAnimatedLengthList class). (There is an
 * SVGAnimatedPoints interface, but that is quite different to
 * DOMSVGAnimatedLengthList, since it is inherited by elements rather than
 * elements having members of that type.) As a consequence, much of the logic
 * that would otherwise be in DOMSVGAnimatedPointList (and is in
 * DOMSVGAnimatedLengthList) is contained in this class.
 *
 * This class is strongly intertwined with DOMSVGPoint. Our DOMSVGPoint
 * items are friends of us and responsible for nulling out our pointers to
 * them when they die.
 *
 * Our DOM items are created lazily on demand as and when script requests them.
 */
class DOMSVGPointList final : public nsISupports,
                              public nsWrapperCache
{
  friend class AutoChangePointListNotifier;
  friend class nsISVGPoint;
  friend class mozilla::DOMSVGPoint;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGPointList)

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject()
  {
    return static_cast<nsIContent*>(mElement);
  }

  /**
   * Factory method to create and return a DOMSVGPointList wrapper
   * for a given internal SVGPointList object. The factory takes care
   * of caching the object that it returns so that the same object can be
   * returned for the given SVGPointList each time it is requested.
   * The cached object is only removed from the cache when it is destroyed due
   * to there being no more references to it or to any of its descendant
   * objects. If that happens, any subsequent call requesting the DOM wrapper
   * for the SVGPointList will naturally result in a new
   * DOMSVGPointList being returned.
   *
   * It's unfortunate that aList is a void* instead of a typed argument. This
   * is because the mBaseVal and mAnimVal members of SVGAnimatedPointList are
   * of different types - a plain SVGPointList, and a SVGPointList*. We
   * use the addresses of these members as the key for the hash table, and
   * clearly SVGPointList* and a SVGPointList** are not the same type.
   */
  static already_AddRefed<DOMSVGPointList>
  GetDOMWrapper(void *aList,
                nsSVGElement *aElement,
                bool aIsAnimValList);

  /**
   * This method returns the DOMSVGPointList wrapper for an internal
   * SVGPointList object if it currently has a wrapper. If it does
   * not, then nullptr is returned.
   */
  static DOMSVGPointList*
  GetDOMWrapperIfExists(void *aList);

  /**
   * This will normally be the same as InternalList().Length(), except if
   * we've hit OOM, in which case our length will be zero.
   */
  uint32_t LengthNoFlush() const {
    MOZ_ASSERT(mItems.Length() == 0 ||
               mItems.Length() == InternalList().Length(),
               "DOM wrapper's list length is out of sync");
    return mItems.Length();
  }

  /**
   * WATCH OUT! If you add code to call this on a baseVal wrapper, then you
   * must also call it on the animVal wrapper too if necessary!! See other
   * callers!
   *
   * Called by internal code to notify us when we need to sync the length of
   * this DOM list with its internal list. This is called immediately prior to
   * the length of the internal list being changed so that any DOM list items
   * that need to be removed from the DOM list can first copy their values from
   * their internal counterpart.
   *
   * The only time this method could fail is on OOM when trying to increase the
   * length of the DOM list. If that happens then this method simply clears the
   * list and returns. Callers just proceed as normal, and we simply accept
   * that the DOM list will be empty (until successfully set to a new value).
   */
  void InternalListWillChangeTo(const SVGPointList& aNewValue);

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool AttrIsAnimating() const;

  uint32_t NumberOfItems() const
  {
    if (IsAnimValList()) {
      Element()->FlushAnimations();
    }
    return LengthNoFlush();
  }
  void Clear(ErrorResult& aError);
  already_AddRefed<nsISVGPoint> Initialize(nsISVGPoint& aNewItem,
                                           ErrorResult& aError);
  already_AddRefed<nsISVGPoint> GetItem(uint32_t index,
                                        ErrorResult& error);
  already_AddRefed<nsISVGPoint> IndexedGetter(uint32_t index, bool& found,
                                              ErrorResult& error);
  already_AddRefed<nsISVGPoint> InsertItemBefore(nsISVGPoint& aNewItem,
                                                 uint32_t aIndex,
                                                 ErrorResult& aError);
  already_AddRefed<nsISVGPoint> ReplaceItem(nsISVGPoint& aNewItem,
                                            uint32_t aIndex,
                                            ErrorResult& aError);
  already_AddRefed<nsISVGPoint> RemoveItem(uint32_t aIndex,
                                           ErrorResult& aError);
  already_AddRefed<nsISVGPoint> AppendItem(nsISVGPoint& aNewItem,
                                           ErrorResult& aError)
  {
    return InsertItemBefore(aNewItem, LengthNoFlush(), aError);
  }
  uint32_t Length() const
  {
    return NumberOfItems();
  }

private:

  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  DOMSVGPointList(nsSVGElement *aElement, bool aIsAnimValList)
    : mElement(aElement)
    , mIsAnimValList(aIsAnimValList)
  {
    InternalListWillChangeTo(InternalList()); // Sync mItems
  }

  ~DOMSVGPointList();

  nsSVGElement* Element() const {
    return mElement.get();
  }

  /// Used to determine if this list is the baseVal or animVal list.
  bool IsAnimValList() const {
    return mIsAnimValList;
  }

  /**
   * Get a reference to this object's corresponding internal SVGPointList.
   *
   * To simplify the code we just have this one method for obtaining both
   * base val and anim val internal lists. This means that anim val lists don't
   * get const protection, but our setter methods guard against changing
   * anim val lists.
   */
  SVGPointList& InternalList() const;

  SVGAnimatedPointList& InternalAList() const;

  /// Returns the nsISVGPoint at aIndex, creating it if necessary.
  already_AddRefed<nsISVGPoint> GetItemAt(uint32_t aIndex);

  void MaybeInsertNullInAnimValListAt(uint32_t aIndex);
  void MaybeRemoveItemFromAnimValListAt(uint32_t aIndex);

  // Weak refs to our nsISVGPoint items. The items are friends and take care
  // of clearing our pointer to them when they die.
  FallibleTArray<nsISVGPoint*> mItems;

  // Strong ref to our element to keep it alive. We hold this not only for
  // ourself, but also for our nsISVGPoint items too.
  nsRefPtr<nsSVGElement> mElement;

  bool mIsAnimValList;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGPOINTLIST_H__
