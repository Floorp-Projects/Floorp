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
#include "nsMsgKeyArray.h"


MSG_ViewIndex nsMsgKeyArray::FindIndex(MessageKey key)
{
	for (int i = 0; i < GetSize(); i++)
	{
		if ((MessageKey)(m_pData[i]) == key)
		{
			return i;
		}
	}
	return MSG_VIEWINDEXNONE;
}

void nsMsgKeyArray::SetArray(MessageKey* pData, int numElements, int numAllocated)
{
	PR_ASSERT(pData != NULL);
	PR_ASSERT(numElements >= 0);
	PR_ASSERT(numAllocated >= numElements);
	
	delete [] m_pData;			// delete previous array
	m_pData = pData;			// set new array
	m_nMaxSize = numAllocated;	// set size
	m_nSize = numElements;		// set allocated length
}
