/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMQuad.h"

#include "mozilla/dom/DOMQuadBinding.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMRect.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMQuad, mParent, mBounds, mPoints[0],
                                      mPoints[1], mPoints[2], mPoints[3])

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMQuad, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMQuad, Release)

DOMQuad::DOMQuad(nsISupports* aParent, CSSPoint aPoints[4])
  : mParent(aParent)
{
  SetIsDOMBinding();
  for (uint32_t i = 0; i < 4; ++i) {
    mPoints[i] = new DOMPoint(aParent, aPoints[i].x, aPoints[i].y);
  }
}

DOMQuad::DOMQuad(nsISupports* aParent)
  : mParent(aParent)
{
  SetIsDOMBinding();
}

DOMQuad::~DOMQuad()
{
}

JSObject*
DOMQuad::WrapObject(JSContext* aCx)
{
  return DOMQuadBinding::Wrap(aCx, this);
}

already_AddRefed<DOMQuad>
DOMQuad::Constructor(const GlobalObject& aGlobal,
                     const DOMPointInit& aP1,
                     const DOMPointInit& aP2,
                     const DOMPointInit& aP3,
                     const DOMPointInit& aP4,
                     ErrorResult& aRV)
{
  nsRefPtr<DOMQuad> obj = new DOMQuad(aGlobal.GetAsSupports());
  obj->mPoints[0] = DOMPoint::Constructor(aGlobal, aP1, aRV);
  obj->mPoints[1] = DOMPoint::Constructor(aGlobal, aP2, aRV);
  obj->mPoints[2] = DOMPoint::Constructor(aGlobal, aP3, aRV);
  obj->mPoints[3] = DOMPoint::Constructor(aGlobal, aP4, aRV);
  return obj.forget();
}

already_AddRefed<DOMQuad>
DOMQuad::Constructor(const GlobalObject& aGlobal, const DOMRectReadOnly& aRect,
                     ErrorResult& aRV)
{
  CSSPoint points[4];
  Float x = aRect.X(), y = aRect.Y(), w = aRect.Width(), h = aRect.Height();
  points[0] = CSSPoint(x, y);
  points[1] = CSSPoint(x + w, y);
  points[2] = CSSPoint(x + w, y + h);
  points[3] = CSSPoint(x, y + h);
  nsRefPtr<DOMQuad> obj = new DOMQuad(aGlobal.GetAsSupports(), points);
  return obj.forget();
}

class DOMQuad::QuadBounds MOZ_FINAL : public DOMRectReadOnly
{
public:
  QuadBounds(DOMQuad* aQuad)
    : DOMRectReadOnly(aQuad->GetParentObject())
    , mQuad(aQuad)
  {}

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(QuadBounds, DOMRectReadOnly)
  NS_DECL_ISUPPORTS_INHERITED

  virtual double X() const
  {
    double x1, x2;
    GetHorizontalMinMax(&x1, &x2);
    return x1;
  }
  virtual double Y() const
  {
    double y1, y2;
    GetVerticalMinMax(&y1, &y2);
    return y1;
  }
  virtual double Width() const
  {
    double x1, x2;
    GetHorizontalMinMax(&x1, &x2);
    return x2 - x1;
  }
  virtual double Height() const
  {
    double y1, y2;
    GetVerticalMinMax(&y1, &y2);
    return y2 - y1;
  }

  void GetHorizontalMinMax(double* aX1, double* aX2) const
  {
    double x1, x2;
    x1 = x2 = mQuad->Point(0)->X();
    for (uint32_t i = 1; i < 4; ++i) {
      double x = mQuad->Point(i)->X();
      x1 = std::min(x1, x);
      x2 = std::max(x2, x);
    }
    *aX1 = x1;
    *aX2 = x2;
  }

  void GetVerticalMinMax(double* aY1, double* aY2) const
  {
    double y1, y2;
    y1 = y2 = mQuad->Point(0)->Y();
    for (uint32_t i = 1; i < 4; ++i) {
      double y = mQuad->Point(i)->Y();
      y1 = std::min(y1, y);
      y2 = std::max(y2, y);
    }
    *aY1 = y1;
    *aY2 = y2;
  }

protected:
  nsRefPtr<DOMQuad> mQuad;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(DOMQuad::QuadBounds, DOMRectReadOnly, mQuad)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMQuad::QuadBounds)
NS_INTERFACE_MAP_END_INHERITING(DOMRectReadOnly)

NS_IMPL_ADDREF_INHERITED(DOMQuad::QuadBounds, DOMRectReadOnly)
NS_IMPL_RELEASE_INHERITED(DOMQuad::QuadBounds, DOMRectReadOnly)

DOMRectReadOnly*
DOMQuad::Bounds() const
{
  if (!mBounds) {
    mBounds = new QuadBounds(const_cast<DOMQuad*>(this));
  }
  return mBounds;
}
