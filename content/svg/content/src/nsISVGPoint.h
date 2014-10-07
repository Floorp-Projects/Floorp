/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/SVGPointBinding.h"
#include "DOMSVGPointList.h"

class nsSVGElement;

// {d6b6c440-af8d-40ee-856b-02a317cab275}
#define MOZILLA_NSISVGPOINT_IID \
  { 0xd6b6c440, 0xaf8d, 0x40ee, \
    { 0x85, 0x6b, 0x02, 0xa3, 0x17, 0xca, 0xb2, 0x75 } }

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 29

namespace mozilla {

namespace dom {
class SVGMatrix;
}

/**
 * Class nsISVGPoint
 *
 * This class creates the DOM objects that wrap internal SVGPoint objects.
 * An nsISVGPoint can be either a DOMSVGPoint or a DOMSVGTranslatePoint
 */
class nsISVGPoint : public nsISupports,
                    public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_NSISVGPOINT_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsISVGPoint)

  /**
   * Generic ctor for DOMSVGPoint objects that are created for an attribute.
   */
  explicit nsISVGPoint()
    : mList(nullptr)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
    , mIsTranslatePoint(false)
  {
  }

  explicit nsISVGPoint(SVGPoint* aPt, bool aIsTranslatePoint)
    : mList(nullptr)
    , mListIndex(0)
    , mIsReadonly(false)
    , mIsAnimValItem(false)
    , mIsTranslatePoint(aIsTranslatePoint)
  {
    mPt.mX = aPt->GetX();
    mPt.mY = aPt->GetY();
  }

protected:
  virtual ~nsISVGPoint()
  {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->mItems[mListIndex] = nullptr;
    }
  }

public:
  /**
   * Creates an unowned copy of this object's point as a DOMSVGPoint.
   */
  virtual DOMSVGPoint* Copy() = 0;

  SVGPoint ToSVGPoint() const {
    return HasOwner() ? const_cast<nsISVGPoint*>(this)->InternalItem() : mPt;
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

  bool IsTranslatePoint() const {
    return mIsTranslatePoint;
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

  bool IsReadonly() const {
    return mIsReadonly;
  }
  void SetReadonly(bool aReadonly) {
    mIsReadonly = aReadonly;
  }

  // WebIDL
  virtual float X() = 0;
  virtual void SetX(float aX, ErrorResult& rv) = 0;
  virtual float Y() = 0;
  virtual void SetY(float aY, ErrorResult& rv) = 0;
  virtual already_AddRefed<nsISVGPoint> MatrixTransform(dom::SVGMatrix& matrix) = 0;
  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE
    { return dom::SVGPointBinding::Wrap(cx, this); }

  virtual nsISupports* GetParentObject() = 0;

protected:
#ifdef DEBUG
  bool IndexIsValid();
#endif

  nsRefPtr<DOMSVGPointList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  uint32_t mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  uint32_t mIsReadonly:1;       // These flags are uint32_t because MSVC won't
  uint32_t mIsAnimValItem:1;    // pack otherwise.
  uint32_t mIsTranslatePoint:1;

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

  // The following member is only used when we're not in a list:
  SVGPoint mPt;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISVGPoint, MOZILLA_NSISVGPOINT_IID)

} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT


