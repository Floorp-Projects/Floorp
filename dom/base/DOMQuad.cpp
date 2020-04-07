/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMQuad.h"

#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMQuadBinding.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/DOMRectBinding.h"
#include "mozilla/FloatingPoint.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMQuad, mParent, mPoints[0], mPoints[1],
                                      mPoints[2], mPoints[3])

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMQuad, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMQuad, Release)

DOMQuad::DOMQuad(nsISupports* aParent, CSSPoint aPoints[4]) : mParent(aParent) {
  for (uint32_t i = 0; i < 4; ++i) {
    mPoints[i] = new DOMPoint(aParent, aPoints[i].x, aPoints[i].y);
  }
}

DOMQuad::DOMQuad(nsISupports* aParent) : mParent(aParent) {}

DOMQuad::~DOMQuad() = default;

JSObject* DOMQuad::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return DOMQuad_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMQuad> DOMQuad::FromRect(const GlobalObject& aGlobal,
                                            const DOMRectInit& aInit) {
  nsISupports* parent = aGlobal.GetAsSupports();
  RefPtr<DOMQuad> obj = new DOMQuad(parent);
  obj->mPoints[0] = new DOMPoint(parent, aInit.mX, aInit.mY, 0, 1);
  obj->mPoints[1] =
      new DOMPoint(parent, aInit.mX + aInit.mWidth, aInit.mY, 0, 1);
  obj->mPoints[2] = new DOMPoint(parent, aInit.mX + aInit.mWidth,
                                 aInit.mY + aInit.mHeight, 0, 1);
  obj->mPoints[3] =
      new DOMPoint(parent, aInit.mX, aInit.mY + aInit.mHeight, 0, 1);
  return obj.forget();
}

already_AddRefed<DOMQuad> DOMQuad::FromQuad(const GlobalObject& aGlobal,
                                            const DOMQuadInit& aInit) {
  RefPtr<DOMQuad> obj = new DOMQuad(aGlobal.GetAsSupports());
  obj->mPoints[0] = DOMPoint::FromPoint(aGlobal, aInit.mP1);
  obj->mPoints[1] = DOMPoint::FromPoint(aGlobal, aInit.mP2);
  obj->mPoints[2] = DOMPoint::FromPoint(aGlobal, aInit.mP3);
  obj->mPoints[3] = DOMPoint::FromPoint(aGlobal, aInit.mP4);
  return obj.forget();
}

already_AddRefed<DOMQuad> DOMQuad::Constructor(const GlobalObject& aGlobal,
                                               const DOMPointInit& aP1,
                                               const DOMPointInit& aP2,
                                               const DOMPointInit& aP3,
                                               const DOMPointInit& aP4) {
  RefPtr<DOMQuad> obj = new DOMQuad(aGlobal.GetAsSupports());
  obj->mPoints[0] = DOMPoint::FromPoint(aGlobal, aP1);
  obj->mPoints[1] = DOMPoint::FromPoint(aGlobal, aP2);
  obj->mPoints[2] = DOMPoint::FromPoint(aGlobal, aP3);
  obj->mPoints[3] = DOMPoint::FromPoint(aGlobal, aP4);
  return obj.forget();
}

already_AddRefed<DOMQuad> DOMQuad::Constructor(const GlobalObject& aGlobal,
                                               const DOMRectReadOnly& aRect) {
  CSSPoint points[4];
  Float x = aRect.X(), y = aRect.Y(), w = aRect.Width(), h = aRect.Height();
  points[0] = CSSPoint(x, y);
  points[1] = CSSPoint(x + w, y);
  points[2] = CSSPoint(x + w, y + h);
  points[3] = CSSPoint(x, y + h);
  RefPtr<DOMQuad> obj = new DOMQuad(aGlobal.GetAsSupports(), points);
  return obj.forget();
}

void DOMQuad::GetHorizontalMinMax(double* aX1, double* aX2) const {
  double x1, x2;
  x1 = x2 = Point(0)->X();
  for (uint32_t i = 1; i < 4; ++i) {
    double x = Point(i)->X();
    x1 = NaNSafeMin(x1, x);
    x2 = NaNSafeMax(x2, x);
  }
  *aX1 = x1;
  *aX2 = x2;
}

void DOMQuad::GetVerticalMinMax(double* aY1, double* aY2) const {
  double y1, y2;
  y1 = y2 = Point(0)->Y();
  for (uint32_t i = 1; i < 4; ++i) {
    double y = Point(i)->Y();
    y1 = NaNSafeMin(y1, y);
    y2 = NaNSafeMax(y2, y);
  }
  *aY1 = y1;
  *aY2 = y2;
}

already_AddRefed<DOMRectReadOnly> DOMQuad::GetBounds() const {
  double x1, x2;
  double y1, y2;

  GetHorizontalMinMax(&x1, &x2);
  GetVerticalMinMax(&y1, &y2);

  RefPtr<DOMRectReadOnly> rval =
      new DOMRectReadOnly(GetParentObject(), x1, y1, x2 - x1, y2 - y1);
  return rval.forget();
}

// https://drafts.fxtf.org/geometry/#structured-serialization
bool DOMQuad::WriteStructuredClone(JSContext* aCx,
                                   JSStructuredCloneWriter* aWriter) const {
  for (const auto& point : mPoints) {
    if (!point->WriteStructuredClone(aCx, aWriter)) {
      return false;
    }
  }
  return true;
}

// static
already_AddRefed<DOMQuad> DOMQuad::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  RefPtr<DOMQuad> quad = new DOMQuad(aGlobal);
  for (auto& point : quad->mPoints) {
    point = DOMPoint::ReadStructuredClone(aCx, aGlobal, aReader);
    if (!point) {
      return nullptr;
    }
  }
  return quad.forget();
}
