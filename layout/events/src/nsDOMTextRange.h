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

#ifndef nsDOMTextRange_h__
#define nsDOMTextRange_h__

#include "nsIDOMTextRange.h"
#include "nsIDOMTextRangeList.h"

class nsDOMTextRange : public nsIDOMTextRange 
{
	NS_DECL_ISUPPORTS
public:

	nsDOMTextRange(PRUint16 aRangeStart, PRUint16 aRangeEnd, PRUint16 aRangeType);
	~nsDOMTextRange(void);

	NS_IMETHOD    GetRangeStart(PRUint16* aRangeStart);
	NS_IMETHOD    SetRangeStart(PRUint16 aRangeStart);

	NS_IMETHOD    GetRangeEnd(PRUint16* aRangeEnd);
	NS_IMETHOD    SetRangeEnd(PRUint16 aRangeEnd);

	NS_IMETHOD    GetRangeType(PRUint16* aRangeType);
	NS_IMETHOD    SetRangeType(PRUint16 aRangeType);

protected:

	PRUint16	mRangeStart;
	PRUint16	mRangeEnd;
	PRUint16	mRangeType;
};

class nsDOMTextRangeList: public nsIDOMTextRangeList 
{
	NS_DECL_ISUPPORTS
public:
	
	nsDOMTextRangeList(PRUint16 aLength,nsIDOMTextRange** aList);
	~nsDOMTextRangeList(void);

	NS_IMETHOD    GetLength(PRUint16* aLength);

	NS_IMETHOD    Item(PRUint16 aIndex, nsIDOMTextRange** aReturn);

protected:

	PRUint16			mLength;
	nsIDOMTextRange**	mList;
};


#endif
