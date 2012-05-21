/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGPOINT_H__
#define MOZILLA_DOMSVGPOINT_H__

#include "DOMSVGPointList.h"
#include "gfxPoint.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsIDOMSVGPoint.h"
#include "nsTArray.h"
#include "SVGPoint.h"

class nsSVGElement;

// We make DOMSVGPoint a pseudo-interface to allow us to QI to it in order to
// check that the objects that scripts pass to DOMSVGPointList methods are
// our *native* point objects.
//
// {d6b6c440-af8d-40ee-856b-02a317cab275}
#define MOZILLA_DOMSVGPOINT_IID \
  { 0xd6b6c440, 0xaf8d, 0x40ee, \
    { 0x85, 0x6b, 0x02, 0xa3, 0x17, 0xca, 0xb2, 0x75 } }

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 30

namespace mozilla {

/**
 * Class DOMSVGPoint
 *
 * This class creates the DOM objects that wrap internal SVGPoint objects that
 * are in an SVGPointList. It is also used to create the objects returned by
 * SVGSVGElement.createSVGPoint() and other functions that return DOM SVGPoint
 * objects.
 *
 * See the architecture comment in DOMSVGPointList.h for an overview of the
 * important points regarding these DOM wrapper structures.
 *
 * See the architecture comment in DOMSVGLength.h (yes, LENGTH) for an overview
 * of the important points regarding how this specific class works.
 */
class DOMSVGPoint : public nsIDOMSVGPoint
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGPOINT_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGPoint)
  NS_DECL_NSIDOMSVGPOINT

  /**
   * Generic ctor for DOMSVGPoint objects that are created for an attribute.
   */
  DOMSVGPoint(DOMSVGPointList *aList,
              PRUint32 aListIndex,
              bool aIsAnimValItem)
    : mList(aList)
    , mListIndex(aListIndex)
    , mIsReadonly(false)
    , mIsAnimValItem(aIsAnimValItem)
  {
    // These shifts are in sync with the members.
    NS_ABORT_IF_FALSE(aList &&
                      aListIndex <= MaxListIndex(), "bad arg");

    NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGPoint!");
  }

  DOMSVGPoint(const DOMSVGPoint *aPt = nsnull)
    : mList(nsnull)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
  {
    if (aPt) {
      mPt = aPt->ToSVGPoint();
    }
  }

  DOMSVGPoint(float aX, float aY)
    : mList(nsnull)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
  {
    mPt.mX = aX;
    mPt.mY = aY;
  }

  DOMSVGPoint(const gfxPoint &aPt)
    : mList(nsnull)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
  {
    mPt.mX = float(aPt.x);
    mPt.mY = float(aPt.y);
    NS_ASSERTION(NS_finite(mPt.mX) && NS_finite(mPt.mX),
                 "DOMSVGPoint coords are not finite");
  }


  ~DOMSVGPoint() {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->mItems[mListIndex] = nsnull;
    }
  }

  /**
   * Create an unowned copy of this object. The caller is responsible for the
   * first AddRef()!
   */
  DOMSVGPoint* Clone() {
    return new DOMSVGPoint(this);
  }

  bool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list points, this will be
   * different to IsInList(). "Owner" here means that the instance has an
   * internal counterpart from which it gets its values. (A better name may
   * be HasWrappee().)
   */
  bool HasOwner() const {
    return !!mList;
  }

  /**
   * This method is called to notify this DOM object that it is being inserted
   * into a list, and give it the information it needs as a result.
   *
   * This object MUST NOT already belong to a list when this method is called.
   * That's not to say that script can't move these DOM objects between
   * lists - it can - it's just that the logic to handle that (and send out
   * the necessary notifications) is located elsewhere (in DOMSVGPointList).)
   */
  void InsertingIntoList(DOMSVGPointList *aList,
                         PRUint32 aListIndex,
                         bool aIsAnimValItem);

  static PRUint32 MaxListIndex() {
    return (1U << MOZ_SVG_LIST_INDEX_BIT_COUNT) - 1;
  }

  /// This method is called to notify this object that its list index changed.
  void UpdateListIndex(PRUint32 aListIndex) {
    mListIndex = aListIndex;
  }

  /**
   * This method is called to notify this DOM object that it is about to be
   * removed from its current DOM list so that it can first make a copy of its
   * internal counterpart's values. (If it didn't do this, then it would
   * "lose" its value on being removed.)
   */
  void RemovingFromList();

  SVGPoint ToSVGPoint() const {
    return HasOwner() ? const_cast<DOMSVGPoint*>(this)->InternalItem() : mPt;
  }

  bool IsReadonly() const {
    return mIsReadonly;
  }
  void SetReadonly(bool aReadonly) {
    mIsReadonly = aReadonly;
  }

protected:

  nsSVGElement* Element() {
    return mList->Element();
  }

  /**
   * Get a reference to the internal SVGPoint list item that this DOM wrapper
   * object currently wraps.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal items. This means that animVal items don't
   * get const protection, but then our setter methods guard against changing
   * animVal items.
   */
  SVGPoint& InternalItem();

#ifdef DEBUG
  bool IndexIsValid();
#endif

  nsRefPtr<DOMSVGPointList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  PRUint32 mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  PRUint32 mIsReadonly:1;    // PRUint32 because MSVC won't pack otherwise
  PRUint32 mIsAnimValItem:1; // PRUint32 because MSVC won't pack otherwise

  // The following member is only used when we're not in a list:
  SVGPoint mPt;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGPoint, MOZILLA_DOMSVGPOINT_IID)

} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // MOZILLA_DOMSVGPOINT_H__
