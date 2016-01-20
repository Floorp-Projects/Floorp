/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleTextRange.h"

#include "TextRange-inl.h"
#include "xpcAccessibleDocument.h"

#include "nsIMutableArray.h"
#include "nsComponentManagerUtils.h"
#include "nsQueryObject.h"

using namespace mozilla;
using namespace mozilla::a11y;

// nsISupports and cycle collection

NS_IMPL_CYCLE_COLLECTION(xpcAccessibleTextRange,
                         mRange.mRoot,
                         mRange.mStartContainer,
                         mRange.mEndContainer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(xpcAccessibleTextRange)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleTextRange)
  NS_INTERFACE_MAP_ENTRY(xpcAccessibleTextRange)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessibleTextRange)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(xpcAccessibleTextRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE(xpcAccessibleTextRange)

// nsIAccessibleTextRange

NS_IMETHODIMP
xpcAccessibleTextRange::GetStartContainer(nsIAccessibleText** aAnchor)
{
  NS_ENSURE_ARG_POINTER(aAnchor);
  NS_IF_ADDREF(*aAnchor = ToXPCText(mRange.StartContainer()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetStartOffset(int32_t* aOffset)
{
  NS_ENSURE_ARG_POINTER(aOffset);
  *aOffset = mRange.StartOffset();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetEndContainer(nsIAccessibleText** aAnchor)
{
  NS_ENSURE_ARG_POINTER(aAnchor);
  NS_IF_ADDREF(*aAnchor = ToXPCText(mRange.EndContainer()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetEndOffset(int32_t* aOffset)
{
  NS_ENSURE_ARG_POINTER(aOffset);
  *aOffset = mRange.EndOffset();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetContainer(nsIAccessible** aContainer)
{
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_IF_ADDREF(*aContainer = ToXPC(mRange.Container()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetEmbeddedChildren(nsIArray** aList)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcList =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<Accessible*> objects;
  mRange.EmbeddedChildren(&objects);

  uint32_t len = objects.Length();
  for (uint32_t idx = 0; idx < len; idx++)
    xpcList->AppendElement(static_cast<nsIAccessible*>(ToXPC(objects[idx])), false);

  xpcList.forget(aList);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::Compare(nsIAccessibleTextRange* aOtherRange,
                                bool* aResult)
{

  RefPtr<xpcAccessibleTextRange> xpcRange(do_QueryObject(aOtherRange));
  if (!xpcRange || !aResult)
    return NS_ERROR_INVALID_ARG;

  *aResult = (mRange == xpcRange->mRange);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::CompareEndPoints(uint32_t aEndPoint,
                                         nsIAccessibleTextRange* aOtherRange,
                                         uint32_t aOtherRangeEndPoint,
                                         int32_t* aResult)
{
  RefPtr<xpcAccessibleTextRange> xpcRange(do_QueryObject(aOtherRange));
  if (!xpcRange || !aResult)
    return NS_ERROR_INVALID_ARG;

  TextPoint p = (aEndPoint == EndPoint_Start) ?
    mRange.StartPoint() : mRange.EndPoint();
  TextPoint otherPoint = (aOtherRangeEndPoint == EndPoint_Start) ?
    xpcRange->mRange.StartPoint() : xpcRange->mRange.EndPoint();

  if (p == otherPoint)
    *aResult = 0;
  else
    *aResult = p < otherPoint ? -1 : 1;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetText(nsAString& aText)
{
  nsAutoString text;
  mRange.Text(text);
  aText.Assign(text);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::GetBounds(nsIArray** aRectList)
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::Move(uint32_t aUnit, int32_t aCount)
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::MoveStart(uint32_t aUnit, int32_t aCount)
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::MoveEnd(uint32_t aUnit, int32_t aCount)
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::Normalize(uint32_t aUnit)
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::Crop(nsIAccessible* aContainer, bool* aSuccess)
{
  Accessible* container = aContainer->ToInternalAccessible();
  NS_ENSURE_TRUE(container, NS_ERROR_INVALID_ARG);

  *aSuccess = mRange.Crop(container);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::FindText(const nsAString& aText, bool aIsBackward,
                                 bool aIsIgnoreCase,
                                 nsIAccessibleTextRange** aRange)
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::FindAttr(uint32_t aAttr, nsIVariant* aVal,
                                 bool aIsBackward,
                                 nsIAccessibleTextRange** aRange)
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::AddToSelection()
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::RemoveFromSelection()
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::Select()
{
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTextRange::ScrollIntoView(uint32_t aHow)
{
  return NS_OK;
}
