/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
// nsMsgKeyArray.cpp : implementation file
//

#include "msgCore.h"    // precompiled header...

#include "nsMsgKeyArray.h"


nsMsgViewIndex nsMsgKeyArray::FindIndex(nsMsgKey key)
{
	for (PRUint32 i = 0; i < GetSize(); i++)
	{
		if ((nsMsgKey)(m_pData[i]) == key)
		{
			return i;
		}
	}
	return nsMsgViewIndex_None;
}

void nsMsgKeyArray::SetArray(nsMsgKey* pData, int numElements, int numAllocated)
{
	NS_ASSERTION(pData != NULL, "storage is NULL");
	NS_ASSERTION(numElements >= 0, "negative number of elements");
	NS_ASSERTION(numAllocated >= numElements, "num elements more than array size");
	
	delete [] m_pData;			// delete previous array
	m_pData = pData;			// set new array
	m_nMaxSize = numAllocated;	// set size
	m_nSize = numElements;		// set allocated length
}
