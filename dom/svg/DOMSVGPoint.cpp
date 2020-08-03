/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGPoint.h"

#include "DOMSVGPointList.h"
#include "gfx2DGlue.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "SVGPoint.h"
#include "mozilla/dom/DOMMatrix.h"
#include "mozilla/dom/SVGElement.h"

// See the architecture comment in DOMSVGPointList.h.

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

float DOMSVGPoint::X() {
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations();  // May make HasOwner() == false
  }
  return HasOwner() ? InternalItem().mX : mPt.mX;
}

void DOMSVGPoint::SetX(float aX, ErrorResult& rv) {
  if (mIsAnimValItem || mIsReadonly) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (HasOwner()) {
    if (InternalItem().mX == aX) {
      return;
    }
    AutoChangePointListNotifier notifier(this);
    InternalItem().mX = aX;
    return;
  }
  mPt.mX = aX;
}

float DOMSVGPoint::Y() {
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations();  // May make HasOwner() == false
  }
  return HasOwner() ? InternalItem().mY : mPt.mY;
}

void DOMSVGPoint::SetY(float aY, ErrorResult& rv) {
  if (mIsAnimValItem || mIsReadonly) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (HasOwner()) {
    if (InternalItem().mY == aY) {
      return;
    }
    AutoChangePointListNotifier notifier(this);
    InternalItem().mY = aY;
    return;
  }
  mPt.mY = aY;
}

already_AddRefed<nsISVGPoint> DOMSVGPoint::MatrixTransform(
    const DOMMatrix2DInit& aMatrix, ErrorResult& aRv) {
  RefPtr<DOMMatrixReadOnly> matrix =
      DOMMatrixReadOnly::FromMatrix(GetParentObject(), aMatrix, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  const auto* matrix2D = matrix->GetInternal2D();
  if (!matrix2D->IsFinite()) {
    aRv.ThrowTypeError<MSG_NOT_FINITE>("MatrixTransform matrix");
    return nullptr;
  }
  auto pt = matrix2D->TransformPoint(HasOwner() ? InternalItem() : mPt);
  nsCOMPtr<nsISVGPoint> newPoint = new DOMSVGPoint(ToPoint(pt));
  return newPoint.forget();
}

}  // namespace dom
}  // namespace mozilla
