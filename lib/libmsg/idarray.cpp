/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
