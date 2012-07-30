/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrivateTextRange.h"


nsPrivateTextRange::nsPrivateTextRange(const nsTextRange &aTextRange)
  : mRangeStart(PRUint16(aTextRange.mStartOffset)),
    mRangeEnd(PRUint16(aTextRange.mEndOffset)),
    mRangeType(PRUint16(aTextRange.mRangeType)),
    mRangeStyle(aTextRange.mRangeStyle)
{
}

nsPrivateTextRange::~nsPrivateTextRange(void)
{
}

NS_IMPL_ISUPPORTS1(nsPrivateTextRange, nsIPrivateTextRange)

NS_METHOD nsPrivateTextRange::GetRangeStart(PRUint16* aRangeStart)
{
	*aRangeStart = mRangeStart;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::GetRangeEnd(PRUint16* aRangeEnd)
{
	*aRangeEnd = mRangeEnd;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::GetRangeType(PRUint16* aRangeType)
{
	*aRangeType = mRangeType;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::GetRangeStyle(nsTextRangeStyle* aTextRangeStyle)
{
	NS_ENSURE_ARG_POINTER(aTextRangeStyle);
	*aTextRangeStyle = mRangeStyle;
	return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsPrivateTextRangeList, nsIPrivateTextRangeList)

void nsPrivateTextRangeList::AppendTextRange(nsRefPtr<nsPrivateTextRange>& aRange)
{
	mList.AppendElement(aRange);
}

NS_METHOD_(PRUint16) nsPrivateTextRangeList::GetLength()
{
  return static_cast<PRUint16>(mList.Length());
}

NS_METHOD_(already_AddRefed<nsIPrivateTextRange>) nsPrivateTextRangeList::Item(PRUint16 aIndex)
{
  nsRefPtr<nsPrivateTextRange> ret = mList.ElementAt(aIndex);
  if (ret) {
    nsPrivateTextRange *retPtr = nullptr;
    ret.swap(retPtr);
    return retPtr;
  }
  return nullptr;
}

