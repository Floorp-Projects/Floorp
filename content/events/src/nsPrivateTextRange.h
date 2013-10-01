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
#include "mozilla/TextEvents.h"

class nsPrivateTextRange MOZ_FINAL : public nsIPrivateTextRange
{
	NS_DECL_ISUPPORTS
public:

	nsPrivateTextRange(const mozilla::TextRange &aTextRange);
	virtual ~nsPrivateTextRange(void);

	NS_IMETHOD GetRangeStart(uint16_t* aRangeStart) MOZ_OVERRIDE;
	NS_IMETHOD GetRangeEnd(uint16_t* aRangeEnd) MOZ_OVERRIDE;
	NS_IMETHOD GetRangeType(uint16_t* aRangeType) MOZ_OVERRIDE;
	NS_IMETHOD GetRangeStyle(mozilla::TextRangeStyle* aRangeStyle) MOZ_OVERRIDE;

protected:

	uint16_t	mRangeStart;
	uint16_t	mRangeEnd;
	uint16_t	mRangeType;
	mozilla::TextRangeStyle mRangeStyle;
};

class nsPrivateTextRangeList MOZ_FINAL : public nsIPrivateTextRangeList
{
	NS_DECL_ISUPPORTS
public:
	nsPrivateTextRangeList(uint16_t aLength) : mList(aLength) {}

	void          AppendTextRange(nsRefPtr<nsPrivateTextRange>& aRange);

	NS_IMETHOD_(uint16_t)    GetLength() MOZ_OVERRIDE;

	NS_IMETHOD_(already_AddRefed<nsIPrivateTextRange>)    Item(uint16_t aIndex) MOZ_OVERRIDE;
protected:
	nsTArray<nsRefPtr<nsPrivateTextRange> > mList;
};


#endif
