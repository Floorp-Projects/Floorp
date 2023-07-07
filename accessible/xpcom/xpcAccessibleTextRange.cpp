/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleTextRange.h"

#include "TextRange-inl.h"

#include "nsQueryObject.h"
#include "xpcAccessibleDocument.h"

using namespace mozilla;
using namespace mozilla::a11y;

// nsISupports and cycle collection

NS_INTERFACE_MAP_BEGIN(xpcAccessibleTextRange)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleTextRange)
  NS_INTERFACE_MAP_ENTRY(xpcAccessibleTextRange)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessibleTextRange)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(xpcAccessibleTextRange)
NS_IMPL_RELEASE(xpcAccessibleTextRange)

a11y::TextRange xpcAccessibleTextRange::Range() {
  return a11y::TextRange(mRoot->ToInternalGeneric(),
                         mStartContainer->ToInternalGeneric(), mStartOffset,
                         mEndContainer->ToInternalGeneric(), mEndOffset);
}

void xpcAccessibleTextRange::SetRange(TextRange& aRange) {
  mRoot = ToXPCText(aRange.Root());
  mStartContainer = ToXPCText(aRange.StartContainer());
  mStartOffset = aRange.StartOffset();
  mEndContainer = ToXPCText(aRange.EndContainer());
  mEndOffset = aRange.EndOffset();
}

// nsIAccessibleTextRange

NS_IMETHODIMP
xpcAccessibleTextRange::GetStartContainer(nsIAccessibleText** aAnchor) {
  NS_ENSURE_ARG_POINTER(aAnchor);
  NS_IF_ADDREF(*aAnchor = mStartContainer);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetStartOffset(int32_t* aOffset) {
  NS_ENSURE_ARG_POINTER(aOffset);
  *aOffset = mStartOffset;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetEndContainer(nsIAccessibleText** aAnchor) {
  NS_ENSURE_ARG_POINTER(aAnchor);
  NS_IF_ADDREF(*aAnchor = mEndContainer);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetEndOffset(int32_t* aOffset) {
  NS_ENSURE_ARG_POINTER(aOffset);
  *aOffset = mEndOffset;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetContainer(nsIAccessible** aContainer) {
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_IF_ADDREF(*aContainer = ToXPC(Range().Container()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::Compare(nsIAccessibleTextRange* aOtherRange,
                                bool* aResult) {
  RefPtr<xpcAccessibleTextRange> xpcRange(do_QueryObject(aOtherRange));
  if (!xpcRange || !aResult) return NS_ERROR_INVALID_ARG;

  *aResult = (Range() == xpcRange->Range());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::CompareEndPoints(uint32_t aEndPoint,
                                         nsIAccessibleTextRange* aOtherRange,
                                         uint32_t aOtherRangeEndPoint,
                                         int32_t* aResult) {
  RefPtr<xpcAccessibleTextRange> xpcRange(do_QueryObject(aOtherRange));
  if (!xpcRange || !aResult) return NS_ERROR_INVALID_ARG;

  TextRange thisRange = Range();
  TextRange otherRange = xpcRange->Range();
  TextPoint p = (aEndPoint == EndPoint_Start) ? thisRange.StartPoint()
                                              : thisRange.EndPoint();
  TextPoint otherPoint = (aOtherRangeEndPoint == EndPoint_Start)
                             ? otherRange.StartPoint()
                             : otherRange.EndPoint();

  if (p == otherPoint) {
    *aResult = 0;
  } else {
    *aResult = p < otherPoint ? -1 : 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::Crop(nsIAccessible* aContainer, bool* aSuccess) {
  Accessible* container = aContainer->ToInternalGeneric();
  NS_ENSURE_TRUE(container, NS_ERROR_INVALID_ARG);

  TextRange range = Range();
  *aSuccess = range.Crop(container);
  if (*aSuccess) {
    SetRange(range);
  }
  return NS_OK;
}
