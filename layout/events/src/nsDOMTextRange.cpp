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

#include "nsDOMTextRange.h"

static NS_DEFINE_IID(kIDOMTextRange, NS_IDOMTEXTRANGE_IID);
static NS_DEFINE_IID(kIDOMTextRangeList,NS_IDOMTEXTRANGELIST_IID);

nsDOMTextRange::nsDOMTextRange(PRUint16 aRangeStart, PRUint16 aRangeEnd, PRUint16 aRangeType)
:	mRangeStart(aRangeStart),
	mRangeEnd(aRangeEnd),
	mRangeType(aRangeType)
{
	NS_INIT_REFCNT();
}

nsDOMTextRange::~nsDOMTextRange(void)
{

}

NS_IMPL_ADDREF(nsDOMTextRange)
NS_IMPL_RELEASE(nsDOMTextRange)

nsresult nsDOMTextRange::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMTextRange)) {
    *aInstancePtrResult = (void*) ((nsIDOMTextRange*)this);
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_METHOD nsDOMTextRange::GetRangeStart(PRUint16* aRangeStart)
{
	*aRangeStart = mRangeStart;
	return NS_OK;
}

NS_METHOD nsDOMTextRange::SetRangeStart(PRUint16 aRangeStart) 
{
	mRangeStart = aRangeStart;
	return NS_OK;
}

NS_METHOD nsDOMTextRange::GetRangeEnd(PRUint16* aRangeEnd)
{
	*aRangeEnd = mRangeEnd;
	return NS_OK;
}

NS_METHOD nsDOMTextRange::SetRangeEnd(PRUint16 aRangeEnd)
{
	mRangeEnd = aRangeEnd;
	return NS_OK;
}

NS_METHOD nsDOMTextRange::GetRangeType(PRUint16* aRangeType)
{
	*aRangeType = mRangeType;
	return NS_OK;
}

NS_METHOD nsDOMTextRange::SetRangeType(PRUint16 aRangeType)
{
	mRangeType = aRangeType;
	return NS_OK;
}


nsDOMTextRangeList::nsDOMTextRangeList(PRUint16 aLength,nsIDOMTextRange** aList)
:	mLength(aLength),
	mList(aList)
{
	if (aList==nsnull)
		aLength = 0;

	NS_INIT_REFCNT();
}

nsDOMTextRangeList::~nsDOMTextRangeList(void)
{
	int	i;
	for(i=0;i<mLength;i++)
		mList[i]->Release();
}

NS_IMPL_ADDREF(nsDOMTextRangeList)
NS_IMPL_RELEASE(nsDOMTextRangeList)

nsresult nsDOMTextRangeList::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMTextRangeList)) {
    *aInstancePtrResult = (void*) ((nsIDOMTextRangeList*)this);
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_METHOD nsDOMTextRangeList::GetLength(PRUint16* aLength)
{
	*aLength = mLength;
	return NS_OK;
}

NS_METHOD nsDOMTextRangeList::Item(PRUint16 aIndex, nsIDOMTextRange** aReturn)
{
	if (aIndex>mLength) {
		*aReturn = nsnull;
		return NS_ERROR_FAILURE;
	}

	mList[aIndex]->AddRef();
	*aReturn = mList[aIndex];

	return NS_OK;
}




