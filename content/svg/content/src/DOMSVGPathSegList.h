/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_DOMSVGPATHSEGLIST_H__
#define MOZILLA_DOMSVGPATHSEGLIST_H__

#include "nsIDOMSVGPathSegList.h"
#include "SVGPathData.h"
#include "SVGPathSegUtils.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"

class nsSVGElement;

namespace mozilla {

class DOMSVGPathSeg;
class SVGAnimatedPathSegList;

/**
 * Class DOMSVGPathSegList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGPathData objects.
 *
 * See the architecture comment in DOMSVGAnimatedLengthList.h first (that's
 * LENGTH list), then continue reading the remainder of this comment.
 *
 * The architecture of this class is very similar to that of DOMSVGLengthList
 * except that, since there is no nsIDOMSVGAnimatedPathSegList interface
 * in SVG, we have no parent DOMSVGAnimatedPathSegList (unlike DOMSVGLengthList
 * which has a parent DOMSVGAnimatedLengthList class). (There is an
 * SVGAnimatedPathData interface, but that is quite different to
 * DOMSVGAnimatedLengthList, since it is inherited by elements rather than
 * elements having members of that type.) As a consequence, much of the logic
 * that would otherwise be in DOMSVGAnimatedPathSegList (and is in
 * DOMSVGAnimatedLengthList) is contained in this class.
 *
 * This class is strongly intertwined with DOMSVGPathSeg. Our DOMSVGPathSeg
 * items are friends of us and responsible for nulling out our pointers to
 * them when they die.
 *
 * Our DOM items are created lazily on demand as and when script requests them.
 */
class DOMSVGPathSegList : public nsIDOMSVGPathSegList
{
  friend class DOMSVGPathSeg;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGPathSegList)
  NS_DECL_NSIDOMSVGPATHSEGLIST

  /**
   * Factory method to create and return a DOMSVGPathSegList wrapper
   * for a given internal SVGPathData object. The factory takes care
   * of caching the object that it returns so that the same object can be
   * returned for the given SVGPathData each time it is requested.
   * The cached object is only removed from the cache when it is destroyed due
   * to there being no more references to it or to any of its descendant
   * objects. If that happens, any subsequent call requesting the DOM wrapper
   * for the SVGPathData will naturally result in a new
   * DOMSVGPathSegList being returned.
   *
   * It's unfortunate that aList is a void* instead of a typed argument. This
   * is because the mBaseVal and mAnimVal members of SVGAnimatedPathSegList are
   * of different types - a plain SVGPathData, and a SVGPathData*. We
   * use the addresses of these members as the key for the hash table, and
   * clearly SVGPathData* and a SVGPathData** are not the same type.
   */
  static already_AddRefed<DOMSVGPathSegList>
  GetDOMWrapper(void *aList,
                nsSVGElement *aElement,
                bool aIsAnimValList);

  /**
   * This method returns the DOMSVGPathSegList wrapper for an internal
   * SVGPathData object if it currently has a wrapper. If it does
   * not, then nsnull is returned.
   */
  static DOMSVGPathSegList*
  GetDOMWrapperIfExists(void *aList);

  /**
   * This will normally be the same as InternalList().CountItems(), except if
   * we've hit OOM, in which case our length will be zero.
   */
  PRUint32 Length() const {
    NS_ABORT_IF_FALSE(mItems.Length() == 0 ||
                      mItems.Length() == InternalList().CountItems(),
                      "DOM wrapper's list length is out of sync");
    return mItems.Length();
  }

  nsIDOMSVGPathSeg* GetItemWithoutAddRef(PRUint32 aIndex);

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
  void InternalListWillChangeTo(const SVGPathData& aNewValue);

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool AttrIsAnimating() const;

private:

  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  DOMSVGPathSegList(nsSVGElement *aElement, bool aIsAnimValList)
    : mElement(aElement)
    , mIsAnimValList(aIsAnimValList)
  {
    InternalListWillChangeTo(InternalList()); // Sync mItems
  }

  ~DOMSVGPathSegList();

  nsSVGElement* Element() {
    return mElement.get();
  }

  /// Used to determine if this list is the baseVal or animVal list.
  bool IsAnimValList() const {
    return mIsAnimValList;
  }

  /**
   * Get a reference to this object's corresponding internal SVGPathData.
   *
   * To simplify the code we just have this one method for obtaining both
   * base val and anim val internal lists. This means that anim val lists don't
   * get const protection, but our setter methods guard against changing
   * anim val lists.
   */
  SVGPathData& InternalList() const;

  SVGAnimatedPathSegList& InternalAList() const;

  /// Creates an instance of the appropriate DOMSVGPathSeg sub-class for
  // aIndex, if it doesn't already exist.
  void EnsureItemAt(PRUint32 aIndex);

  void MaybeInsertNullInAnimValListAt(PRUint32 aIndex,
                                      PRUint32 aInternalIndex,
                                      PRUint32 aArgCountForItem);
  void MaybeRemoveItemFromAnimValListAt(PRUint32 aIndex,
                                        PRUint32 aArgCountForItem);

  // Calls UpdateListIndex on all elements in |mItems| that satisfy ItemAt(),
  // from |aStartingIndex| to the end of |mItems|.  Also adjusts
  // |mItems.mInternalDataIndex| by the requested amount.
  void UpdateListIndicesFromIndex(PRUint32 aStartingIndex,
                                  PRInt32  aInternalDataIndexDelta);

  DOMSVGPathSeg*& ItemAt(PRUint32 aIndex) {
    return mItems[aIndex].mItem;
  }

  /**
   * This struct is used in our array of mItems to provide us with somewhere to
   * store the indexes into the internal SVGPathData of the internal seg data
   * that our DOMSVGPathSeg items wrap (the internal segment data is or varying
   * length, so we can't just use the index of our DOMSVGPathSeg items
   * themselves). The reason that we have this separate struct rather than
   * just storing the internal indexes in the DOMSVGPathSeg items is because we
   * want to create the DOMSVGPathSeg items lazily on demand.
   */
  struct ItemProxy {
    ItemProxy(){}
    ItemProxy(DOMSVGPathSeg *aItem, PRUint32 aInternalDataIndex)
      : mItem(aItem)
      , mInternalDataIndex(aInternalDataIndex)
    {}

    DOMSVGPathSeg *mItem;
    PRUint32 mInternalDataIndex;
  };

  // Weak refs to our DOMSVGPathSeg items. The items are friends and take care
  // of clearing our pointer to them when they die.
  nsTArray<ItemProxy> mItems;

  // Strong ref to our element to keep it alive. We hold this not only for
  // ourself, but also for our DOMSVGPathSeg items too.
  nsRefPtr<nsSVGElement> mElement;

  bool mIsAnimValList;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGPATHSEGLIST_H__
