/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGPOINT_H__
#define MOZILLA_DOMSVGPOINT_H__

#include "DOMSVGPointList.h"
#include "mozilla/gfx/2D.h"
#include "nsAutoPtr.h"
#include "nsDebug.h"
#include "nsISVGPoint.h"
#include "SVGPoint.h"
#include "mozilla/Attributes.h"
#include "mozilla/FloatingPoint.h"

class nsSVGElement;

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
class DOMSVGPoint final : public nsISVGPoint
{
  friend class AutoChangePointNotifier;

  typedef mozilla::gfx::Point Point;

public:
  /**
   * Generic ctor for DOMSVGPoint objects that are created for an attribute.
   */
  DOMSVGPoint(DOMSVGPointList *aList,
              uint32_t aListIndex,
              bool aIsAnimValItem)
    : nsISVGPoint()
  {
    mList = aList;
    mListIndex = aListIndex;
    mIsAnimValItem = aIsAnimValItem;

    // These shifts are in sync with the members.
    MOZ_ASSERT(aList && aListIndex <= MaxListIndex(), "bad arg");

    MOZ_ASSERT(IndexIsValid(), "Bad index for DOMSVGPoint!");
  }

  explicit DOMSVGPoint(const DOMSVGPoint *aPt = nullptr)
    : nsISVGPoint()
  {
    if (aPt) {
      mPt = aPt->ToSVGPoint();
    }
  }

  DOMSVGPoint(float aX, float aY)
    : nsISVGPoint()
  {
    mPt.mX = aX;
    mPt.mY = aY;
  }

  explicit DOMSVGPoint(const Point& aPt)
    : nsISVGPoint()
  {
    mPt.mX = aPt.x;
    mPt.mY = aPt.y;
    NS_ASSERTION(IsFinite(mPt.mX) && IsFinite(mPt.mX),
                 "DOMSVGPoint coords are not finite");
  }


  // WebIDL
  virtual float X() override;
  virtual void SetX(float aX, ErrorResult& rv) override;
  virtual float Y() override;
  virtual void SetY(float aY, ErrorResult& rv) override;
  virtual already_AddRefed<nsISVGPoint> MatrixTransform(dom::SVGMatrix& matrix) override;
  nsISupports* GetParentObject() override {
    return mList;
  }

  virtual DOMSVGPoint* Copy() override {
    return new DOMSVGPoint(this);
  }

protected:

  nsSVGElement* Element() {
    return mList->Element();
  }
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGPOINT_H__
