/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsPrivateTextRange.h"

static NS_DEFINE_IID(kIPrivateTextRange, NS_IPRIVATETEXTRANGE_IID);
static NS_DEFINE_IID(kIPrivateTextRangeList,NS_IPRIVATETEXTRANGELIST_IID);

nsPrivateTextRange::nsPrivateTextRange(PRUint16 aRangeStart, PRUint16 aRangeEnd, PRUint16 aRangeType)
:	mRangeStart(aRangeStart),
	mRangeEnd(aRangeEnd),
	mRangeType(aRangeType)
{
	NS_INIT_REFCNT();
}

nsPrivateTextRange::~nsPrivateTextRange(void)
{

}

NS_IMPL_ADDREF(nsPrivateTextRange)
NS_IMPL_RELEASE(nsPrivateTextRange)

nsresult nsPrivateTextRange::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIPrivateTextRange)) {
    *aInstancePtrResult = (void*) ((nsIPrivateTextRange*)this);
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_METHOD nsPrivateTextRange::GetRangeStart(PRUint16* aRangeStart)
{
	*aRangeStart = mRangeStart;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::SetRangeStart(PRUint16 aRangeStart) 
{
	mRangeStart = aRangeStart;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::GetRangeEnd(PRUint16* aRangeEnd)
{
	*aRangeEnd = mRangeEnd;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::SetRangeEnd(PRUint16 aRangeEnd)
{
	mRangeEnd = aRangeEnd;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::GetRangeType(PRUint16* aRangeType)
{
	*aRangeType = mRangeType;
	return NS_OK;
}

NS_METHOD nsPrivateTextRange::SetRangeType(PRUint16 aRangeType)
{
	mRangeType = aRangeType;
	return NS_OK;
}


nsPrivateTextRangeList::nsPrivateTextRangeList(PRUint16 aLength,nsIPrivateTextRange** aList)
:	mLength(aLength),
	mList(aList)
{
	if (aList==nsnull)
		aLength = 0;

	NS_INIT_REFCNT();
}

nsPrivateTextRangeList::~nsPrivateTextRangeList(void)
{
	int	i;
	for(i=0;i<mLength;i++)
		mList[i]->Release();
}

NS_IMPL_ADDREF(nsPrivateTextRangeList)
NS_IMPL_RELEASE(nsPrivateTextRangeList)

nsresult nsPrivateTextRangeList::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIPrivateTextRangeList)) {
    *aInstancePtrResult = (void*) ((nsIPrivateTextRangeList*)this);
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_METHOD nsPrivateTextRangeList::GetLength(PRUint16* aLength)
{
	*aLength = mLength;
	return NS_OK;
}

NS_METHOD nsPrivateTextRangeList::Item(PRUint16 aIndex, nsIPrivateTextRange** aReturn)
{
	if (aIndex>mLength) {
		*aReturn = nsnull;
		return NS_ERROR_FAILURE;
	}

	mList[aIndex]->AddRef();
	*aReturn = mList[aIndex];

	return NS_OK;
}




