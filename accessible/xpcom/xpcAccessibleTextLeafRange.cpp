/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleTextLeafRange.h"

#include "nsIAccessible.h"
#include "TextLeafRange.h"
#include "xpcAccessibleDocument.h"

using namespace mozilla;
using namespace mozilla::a11y;

// xpcAccessibleTextLeafPoint

NS_IMPL_ADDREF(xpcAccessibleTextLeafPoint)
NS_IMPL_RELEASE(xpcAccessibleTextLeafPoint)

NS_IMPL_QUERY_INTERFACE(xpcAccessibleTextLeafPoint, nsIAccessibleTextLeafPoint)

xpcAccessibleTextLeafPoint::xpcAccessibleTextLeafPoint(
    nsIAccessible* aAccessible, int32_t aOffset)
    : mAccessible(nullptr), mOffset(0) {
  // When constructing a text point it will actualize the offset
  // and adjust the accessible to the appropriate leaf. These
  // might differ from the given constructor arguments.
  if (aAccessible) {
    TextLeafPoint point(aAccessible->ToInternalGeneric(), aOffset);
    mAccessible = ToXPC(point.mAcc);
    mOffset = point.mOffset;
  }
}

NS_IMETHODIMP xpcAccessibleTextLeafPoint::GetAccessible(
    nsIAccessible** aAccessible) {
  NS_ENSURE_ARG_POINTER(aAccessible);
  RefPtr<nsIAccessible> acc = mAccessible;
  acc.forget(aAccessible);
  return NS_OK;
}

NS_IMETHODIMP xpcAccessibleTextLeafPoint::SetAccessible(
    nsIAccessible* aAccessible) {
  mAccessible = aAccessible;
  return NS_OK;
}

NS_IMETHODIMP xpcAccessibleTextLeafPoint::GetOffset(int32_t* aOffset) {
  NS_ENSURE_ARG_POINTER(aOffset);
  *aOffset = mOffset;
  return NS_OK;
}
NS_IMETHODIMP xpcAccessibleTextLeafPoint::SetOffset(int32_t aOffset) {
  mOffset = aOffset;
  return NS_OK;
}

NS_IMETHODIMP xpcAccessibleTextLeafPoint::FindBoundary(
    AccessibleTextBoundary aBoundaryType, uint32_t aDirection, uint32_t aFlags,
    nsIAccessibleTextLeafPoint** aPoint) {
  TextLeafPoint thisPoint = ToPoint();
  if (!thisPoint) {
    return NS_ERROR_FAILURE;
  }

  TextLeafPoint result = thisPoint.FindBoundary(
      aBoundaryType, static_cast<nsDirection>(aDirection),
      static_cast<TextLeafPoint::BoundaryFlags>(aFlags));
  RefPtr<xpcAccessibleTextLeafPoint> point = new xpcAccessibleTextLeafPoint(
      result ? ToXPC(result.mAcc) : nullptr, result ? result.mOffset : 0);
  point.forget(aPoint);
  return NS_OK;
}

TextLeafPoint xpcAccessibleTextLeafPoint::ToPoint() {
  if (mAccessible) {
    return TextLeafPoint(mAccessible->ToInternalGeneric(), mOffset);
  }

  return TextLeafPoint();
}
