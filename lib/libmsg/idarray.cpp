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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
// idarray.cpp : implementation file
//
#include "msg.h"
#include "xp.h"
#include "msgcom.h"
#include "idarray.h"


MSG_ViewIndex IDArray::FindIndex(MessageKey id)
{
	for (int i = 0; i < GetSize(); i++)
	{
		if ((MessageKey)(m_pData[i]) == id)
		{
			return i;
		}
	}
	return MSG_VIEWINDEXNONE;
}

void IDArray::SetArray(MessageKey* pData, int numElements, int numAllocated)
{
	XP_ASSERT(pData != NULL);
	XP_ASSERT(numElements >= 0);
	XP_ASSERT(numAllocated >= numElements);
	
	delete [] m_pData;			// delete previous array
	m_pData = pData;			// set new array
	m_nMaxSize = numAllocated;	// set size
	m_nSize = numElements;		// set allocated length
}
