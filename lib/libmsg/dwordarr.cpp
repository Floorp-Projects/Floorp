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
#include "msg.h"
#include "xp.h"
#include "dwordarr.h"
#include "xp_qsort.h"

#ifdef XP_WIN16
#define SIZE_T_MAX    0xFF80		            // Maximum allocation size
#define MAX_ARR_ELEMS SIZE_T_MAX/sizeof(uint32)
#endif


XPDWordArray::XPDWordArray()
{
	m_nSize = 0;
	m_nMaxSize = 0;
	m_nGrowBy = 0;
	m_pData = NULL;
}

XPDWordArray::~XPDWordArray()
{
	SetSize(0);
}

/////////////////////////////////////////////////////////////////////////////

int XPDWordArray::GetSize() const
{
	return m_nSize;
}

XP_Bool XPDWordArray::SetSize(int nSize, int nGrowBy)
{
	XP_ASSERT(nSize >= 0);

	if (nGrowBy >= 0)
		m_nGrowBy = nGrowBy;

#ifdef MAX_ARR_ELEMS
	if (nSize > MAX_ARR_ELEMS);
	{
		XP_ASSERT(nSize <= MAX_ARR_ELEMS); // Will fail
		return FALSE;
	}
#endif

	if (nSize == 0)
	{
		// Remove all elements
		XP_FREE(m_pData);
		m_nSize = 0;
		m_nMaxSize = 0;
		m_pData = NULL;
	}
	else if (m_pData == NULL)
	{
		// Create a new array
		m_nMaxSize = MAX(8, nSize);
		m_pData = (uint32 *)XP_CALLOC(1, m_nMaxSize * sizeof(uint32));
		if (m_pData)
			m_nSize = nSize;
		else
			m_nSize = m_nMaxSize = 0;
	}
	else if (nSize <= m_nMaxSize)
	{
		// The new size is within the current maximum size, make sure new
		// elements are to initialized to zero
		if (nSize > m_nSize)
			XP_MEMSET(&m_pData[m_nSize], 0, (nSize - m_nSize) * sizeof(uint32));

		m_nSize = nSize;
	}
	else
	{
		// The array needs to grow, figure out how much
		int nMaxSize;
		nGrowBy  = MAX(m_nGrowBy, MIN(1024, MAX(8, m_nSize / 8)));
		nMaxSize = MAX(nSize, m_nMaxSize + nGrowBy);
#ifdef MAX_ARR_ELEMS
		nMaxSize = MIN(MAX_ARR_ELEMS, nMaxSize);
#endif

		uint32 *pNewData = (uint32 *)XP_ALLOC(nMaxSize * sizeof(uint32));
		if (pNewData)
		{
			// Copy the data from the old array to the new one
			XP_MEMCPY(pNewData, m_pData, m_nSize * sizeof(uint32));

			// Zero out the remaining elements
			XP_MEMSET(&pNewData[m_nSize], 0, (nSize - m_nSize) * sizeof(uint32));
			m_nSize = nSize;
			m_nMaxSize = nMaxSize;

			// Free the old array
			XP_FREE(m_pData);
			m_pData = pNewData;
		}
	}

	return nSize == m_nSize;
}

/////////////////////////////////////////////////////////////////////////////

uint32 &XPDWordArray::ElementAt(int nIndex)
{
	XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

uint32 XPDWordArray::GetAt(int nIndex) const
{
	XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

uint32 *XPDWordArray::GetData() 
{
	return m_pData;
}

void XPDWordArray::SetAt(int nIndex, uint32 newElement)
{
	XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
	m_pData[nIndex] = newElement;
}

/////////////////////////////////////////////////////////////////////////////

int XPDWordArray::Add(uint32 newElement)
{
	int nIndex = m_nSize;

#ifdef MAX_ARR_ELEMS
	if (nIndex >= MAX_ARR_ELEMS) 
		return -1;	     
#endif			

	SetAtGrow(nIndex, newElement);
	return nIndex;
}

uint XPDWordArray::Add(uint32 *elementPtr, uint numElements) 
{ 
	if (SetSize(m_nSize + numElements))
		XP_MEMCPY(m_pData + m_nSize, elementPtr, numElements * sizeof(uint32)); 

	return m_nSize; 
} 

uint32 *XPDWordArray::CloneData() 
{ 
	uint32 *copyOfData = (uint32 *)XP_ALLOC(m_nSize * sizeof(uint32)); 
	if (copyOfData) 
		XP_MEMCPY(copyOfData, m_pData, m_nSize * sizeof(uint32)); 

	return copyOfData; 
} 

void XPDWordArray::InsertAt(int nIndex, uint32 newElement, int nCount)
{
	XP_ASSERT(nIndex >= 0);
	XP_ASSERT(nCount > 0);

	if (nIndex >= m_nSize)
	{
		// If the new element is after the end of the array, grow the array
		SetSize(nIndex + nCount);
	}
	else
	{
		// The element is being insert inside the array
		int nOldSize = m_nSize;
		SetSize(m_nSize + nCount);

		// Move the data after the insertion point
		XP_MEMMOVE(&m_pData[nIndex + nCount], &m_pData[nIndex],
			       (nOldSize - nIndex) * sizeof(uint32));
	}

	// Insert the new elements
	XP_ASSERT(nIndex + nCount <= m_nSize);
	while (nCount--)
		m_pData[nIndex++] = newElement;
}

void XPDWordArray::InsertAt(int nStartIndex, const XPDWordArray *pNewArray)
{
	XP_ASSERT(nStartIndex >= 0);
	XP_ASSERT(pNewArray != NULL);

	if (pNewArray->GetSize() > 0)
	{
		InsertAt(nStartIndex, pNewArray->GetAt(0), pNewArray->GetSize());
		for (int i = 1; i < pNewArray->GetSize(); i++)
			m_pData[nStartIndex + i] = pNewArray->GetAt(i);
	}
}

void XPDWordArray::RemoveAll()
{
	SetSize(0);
}

void XPDWordArray::RemoveAt(int nIndex, int nCount)
{
	XP_ASSERT(nIndex >= 0);
	XP_ASSERT(nIndex + nCount <= m_nSize);

	if (nCount > 0)
	{
		// Make sure not to overstep the end of the array
		int nMoveCount = m_nSize - (nIndex + nCount);
		if (nCount && nMoveCount)
			XP_MEMMOVE(&m_pData[nIndex], &m_pData[nIndex + nCount],
		               nMoveCount * sizeof(uint32));

		m_nSize -= nCount;
	}
}

void XPDWordArray::SetAtGrow(int nIndex, uint32 newElement)
{
	XP_ASSERT(nIndex >= 0);

	if (nIndex >= m_nSize)
		SetSize(nIndex+1);
	m_pData[nIndex] = newElement;
}

/////////////////////////////////////////////////////////////////////////////

void XPDWordArray::CopyArray(XPDWordArray *oldA)
{
	CopyArray(*oldA);
}

void XPDWordArray::CopyArray(XPDWordArray &oldA)
{
	if (m_pData)
		XP_FREE(m_pData);
	m_nSize = oldA.m_nSize;
	m_nMaxSize = oldA.m_nMaxSize;
	m_pData = (uint32 *)XP_ALLOC(m_nSize * sizeof(uint32));
	if (m_pData)
		XP_MEMCPY(m_pData, oldA.m_pData, m_nSize * sizeof(uint32));
}

/////////////////////////////////////////////////////////////////////////////

static int CompareDWord (const void *v1, const void *v2)
{
	// QuickSort callback to compare array values
	uint32 i1 = *(uint32 *)v1;
	uint32 i2 = *(uint32 *)v2;
	return i1 - i2;
}

void XPDWordArray::QuickSort (int (*compare) (const void *elem1, const void *elem2))
{
	if (m_nSize > 1)
		XP_QSORT (m_pData, m_nSize, sizeof(void*), compare ? compare : CompareDWord);
}
