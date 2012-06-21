/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrivateTextRange_h__
#define nsPrivateTextRange_h__

#include "nsIPrivateTextRange.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsPrivateTextRange MOZ_FINAL : public nsIPrivateTextRange
{
	NS_DECL_ISUPPORTS
public:

	nsPrivateTextRange(const nsTextRange &aTextRange);
	virtual ~nsPrivateTextRange(void);

	NS_IMETHOD    GetRangeStart(PRUint16* aRangeStart);
	NS_IMETHOD    GetRangeEnd(PRUint16* aRangeEnd);
	NS_IMETHOD    GetRangeType(PRUint16* aRangeType);
	NS_IMETHOD    GetRangeStyle(nsTextRangeStyle* aRangeStyle);

protected:

	PRUint16	mRangeStart;
	PRUint16	mRangeEnd;
	PRUint16	mRangeType;
	nsTextRangeStyle mRangeStyle;
};

class nsPrivateTextRangeList MOZ_FINAL : public nsIPrivateTextRangeList
{
	NS_DECL_ISUPPORTS
public:
	nsPrivateTextRangeList(PRUint16 aLength) : mList(aLength) {}

	void          AppendTextRange(nsRefPtr<nsPrivateTextRange>& aRange);

	NS_IMETHOD_(PRUint16)    GetLength();

	NS_IMETHOD_(already_AddRefed<nsIPrivateTextRange>)    Item(PRUint16 aIndex);
protected:
	nsTArray<nsRefPtr<nsPrivateTextRange> > mList;
};


#endif
