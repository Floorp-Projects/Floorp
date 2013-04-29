/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrivateTextRange.h"


nsPrivateTextRange::nsPrivateTextRange(const nsTextRange &aTextRange)
  : mRangeStart(uint16_t(aTextRange.mStartOffset)),
    mRangeEnd(uint16_t(aTextRange.mEndOffset)),
    mRangeType(uint16_t(aTextRange.mRangeType)),
    mRangeStyle(aTextRange.mRangeStyle)
{
}

nsPrivateTextRange::~nsPrivateTextRange(void)
{
}

NS_IMPL_ISUPPORTS1(nsPrivateTextRange, nsIPrivateTextRange)

NS_METHOD nsPrivateTextRange::GetRangeStart(uint16_t* aRangeStart)
{
	*aRangeStart = mRangeStart;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::GetRangeEnd(uint16_t* aRangeEnd)
{
	*aRangeEnd = mRangeEnd;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::GetRangeType(uint16_t* aRangeType)
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

NS_METHOD_(uint16_t) nsPrivateTextRangeList::GetLength()
{
  return static_cast<uint16_t>(mList.Length());
}

NS_METHOD_(already_AddRefed<nsIPrivateTextRange>) nsPrivateTextRangeList::Item(uint16_t aIndex)
{
  nsRefPtr<nsPrivateTextRange> ret = mList.ElementAt(aIndex);
  return ret.forget();
}

