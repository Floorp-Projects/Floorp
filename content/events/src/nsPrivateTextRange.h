/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsPrivateTextRange_h__
#define nsPrivateTextRange_h__

#include "nsIPrivateTextRange.h"

class nsPrivateTextRange : public nsIPrivateTextRange 
{
	NS_DECL_ISUPPORTS
public:

	nsPrivateTextRange(PRUint16 aRangeStart, PRUint16 aRangeEnd, PRUint16 aRangeType);
	virtual ~nsPrivateTextRange(void);

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

class nsPrivateTextRangeList: public nsIPrivateTextRangeList 
{
	NS_DECL_ISUPPORTS
public:
	
	nsPrivateTextRangeList(PRUint16 aLength, nsIPrivateTextRange** aList);
	virtual ~nsPrivateTextRangeList(void);

	NS_IMETHOD    GetLength(PRUint16* aLength);

	NS_IMETHOD    Item(PRUint16 aIndex, nsIPrivateTextRange** aReturn);

protected:

	PRUint16				mLength;
	nsIPrivateTextRange**	mList;
};


#endif
