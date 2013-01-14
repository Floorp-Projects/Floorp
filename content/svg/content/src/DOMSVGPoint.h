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
#include "nsISVGPoint.h"
#include "nsTArray.h"
#include "SVGPoint.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"

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

namespace dom {
class SVGMatrix;
}

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
class DOMSVGPoint MOZ_FINAL : public nsISVGPoint
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGPOINT_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGPoint)

  /**
   * Generic ctor for DOMSVGPoint objects that are created for an attribute.
   */
  DOMSVGPoint(DOMSVGPointList *aList,
              uint32_t aListIndex,
              bool aIsAnimValItem)
    : nsISVGPoint()
    , mList(aList)
    , mListIndex(aListIndex)
    , mIsReadonly(false)
    , mIsAnimValItem(aIsAnimValItem)
  {
    // These shifts are in sync with the members.
    NS_ABORT_IF_FALSE(aList &&
                      aListIndex <= MaxListIndex(), "bad arg");

    NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGPoint!");
  }

  explicit DOMSVGPoint(const DOMSVGPoint *aPt = nullptr)
    : nsISVGPoint()
    , mList(nullptr)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
  {
    if (aPt) {
      mPt = aPt->ToSVGPoint();
    }
  }

  DOMSVGPoint(float aX, float aY)
    : nsISVGPoint()
    , mList(nullptr)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
  {
    mPt.mX = aX;
    mPt.mY = aY;
  }

  explicit DOMSVGPoint(const gfxPoint &aPt)
    : nsISVGPoint()
    , mList(nullptr)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
  {
    mPt.mX = float(aPt.x);
    mPt.mY = float(aPt.y);
    NS_ASSERTION(NS_finite(mPt.mX) && NS_finite(mPt.mX),
                 "DOMSVGPoint coords are not finite");
  }


  virtual ~DOMSVGPoint() {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->mItems[mListIndex] = nullptr;
    }
  }

  // WebIDL
  virtual float X();
  virtual void SetX(float aX, ErrorResult& rv);
  virtual float Y();
  virtual void SetY(float aY, ErrorResult& rv);
  virtual already_AddRefed<nsISVGPoint> MatrixTransform(dom::SVGMatrix& matrix);
  nsISupports* GetParentObject() MOZ_OVERRIDE {
    return mList;
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
                         uint32_t aListIndex,
                         bool aIsAnimValItem);

  static uint32_t MaxListIndex() {
    return (1U << MOZ_SVG_LIST_INDEX_BIT_COUNT) - 1;
  }

  /// This method is called to notify this object that its list index changed.
  void UpdateListIndex(uint32_t aListIndex) {
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

  uint32_t mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  uint32_t mIsReadonly:1;    // uint32_t because MSVC won't pack otherwise
  uint32_t mIsAnimValItem:1; // uint32_t because MSVC won't pack otherwise

  // The following member is only used when we're not in a list:
  SVGPoint mPt;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGPoint, MOZILLA_DOMSVGPOINT_IID)

} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // MOZILLA_DOMSVGPOINT_H__
