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
#include "ptrarray.h"
#include "xp_qsort.h"

#ifdef XP_WIN16
#define SIZE_T_MAX    0xFF80		            // Maximum allocation size
#define MAX_ARR_ELEMS SIZE_T_MAX/sizeof(void *)
#endif


XPPtrArray::XPPtrArray()
{
	m_nSize = 0;
	m_nMaxSize = 0;
	m_pData = NULL;
}

XPPtrArray::~XPPtrArray()
{
	SetSize(0);
}

/////////////////////////////////////////////////////////////////////////////

int XPPtrArray::GetSize() const
{
	return m_nSize;
}

XP_Bool XPPtrArray::IsValidIndex(int32 nIndex)
{
	return (nIndex < GetSize() && nIndex >= 0);
}

XP_Bool XPPtrArray::SetSize(int nSize)
{
	XP_ASSERT(nSize >= 0);
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
		m_pData = (void **)XP_CALLOC(1, m_nMaxSize * sizeof(void *));
		if (m_pData)
			m_nSize = nSize;
		else
			m_nSize = m_nMaxSize = 0;
	}
	else if (nSize <= m_nMaxSize)
	{
		// The new size is within the current maximum size, make sure new
		// elements are initialized to zero
		if (nSize > m_nSize)
			XP_MEMSET(&m_pData[m_nSize], 0, (nSize - m_nSize) * sizeof(void *));

		m_nSize = nSize;
	}
	else
	{
		// The array needs to grow, figure out how much
		int nGrowBy, nMaxSize;
		nGrowBy  = MIN(1024, MAX(8, m_nSize / 8));
		nMaxSize = MAX(nSize, m_nMaxSize + nGrowBy);
#ifdef MAX_ARR_ELEMS
		nMaxSize = MIN(MAX_ARR_ELEMS, nMaxSize);
#endif

		void **pNewData = (void **)XP_ALLOC(nMaxSize * sizeof(void *));
		if (pNewData)
		{
			// Copy the data from the old array to the new one
			XP_MEMCPY(pNewData, m_pData, m_nSize * sizeof(void *));

			// Zero out the remaining elements
			XP_MEMSET(&pNewData[m_nSize], 0, (nSize - m_nSize) * sizeof(void *));
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

void*& XPPtrArray::ElementAt(int nIndex)
{
	XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

int XPPtrArray::FindIndex (int nStartIndex, void *pToFind) const
{
	for (int i = nStartIndex; i < GetSize(); i++)
		if (m_pData[i] == pToFind)
			return i;
	return -1;
}

int XPPtrArray::FindIndexUsing(int nStartIndex, void* pToFind, XPCompareFunc* compare) const
{
	for (int i = nStartIndex; i < GetSize(); i++)
		if (compare(&m_pData[i], &pToFind) == 0)
			return i;
	return -1;
}

void *XPPtrArray::GetAt(int nIndex) const
{
	XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

void XPPtrArray::SetAt(int nIndex, void *newElement)
{
	XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
	m_pData[nIndex] = newElement;
}

/////////////////////////////////////////////////////////////////////////////

int XPPtrArray::Add(void *newElement)
{
	int nIndex = m_nSize;

#ifdef MAX_ARR_ELEMS
	if (nIndex >= MAX_ARR_ELEMS) 
		return -1;	     
#endif			

	SetAtGrow(nIndex, newElement);
	return nIndex;
}

void XPPtrArray::InsertAt(int nIndex, void *newElement, int nCount)
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
			       (nOldSize - nIndex) * sizeof(void *));
	}

	// Insert the new elements
	XP_ASSERT(nIndex + nCount <= m_nSize);
	while (nCount--)
		m_pData[nIndex++] = newElement;
}

void XPPtrArray::InsertAt(int nStartIndex, const XPPtrArray *pNewArray)
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

XP_Bool XPPtrArray::Remove(void *pToRemove)
{
	int index = FindIndex(0, pToRemove);
	if (index != -1)
	{
		RemoveAt(index);
		return TRUE;
	}
	else
		return FALSE;
}

void XPPtrArray::RemoveAll()
{
	SetSize(0);
}

void XPPtrArray::RemoveAt(int nIndex, int nCount)
{
	XP_ASSERT(nIndex >= 0);
	XP_ASSERT(nIndex + nCount <= m_nSize);

	if (nCount > 0)
	{
		// Make sure not to overstep the end of the array
		int nMoveCount = m_nSize - (nIndex + nCount);
		if (nCount && nMoveCount)
			XP_MEMMOVE(&m_pData[nIndex], &m_pData[nIndex + nCount],
		               nMoveCount * sizeof(void*));

		m_nSize -= nCount;
	}
}

void XPPtrArray::RemoveAt(int nStartIndex, const XPPtrArray *pArray)
{
	XP_ASSERT(nStartIndex >= 0);
	XP_ASSERT(pArray != NULL);

	for (int i = 0; i < pArray->GetSize(); i++)
	{
		int index = FindIndex(nStartIndex, pArray->GetAt(i));
		if (index >= 0)
			RemoveAt(index);
	}
}

void XPPtrArray::SetAtGrow(int nIndex, void *newElement)
{
	XP_ASSERT(nIndex >= 0);

	if (nIndex >= m_nSize)
		SetSize(nIndex+1);
	m_pData[nIndex] = newElement;
}

/////////////////////////////////////////////////////////////////////////////

int XPPtrArray::InsertBinary(void *newElement, int ( *compare )(const void *elem1, const void *elem2))
{
	int		current = 0;
	int		left = 0;
	int		right = GetSize() - 1;
	int		comparison = 0;
	
	while (left <= right)
	{
		current = (left + right) / 2;
		
		void *pCurrent = GetAt(current);
		comparison = compare(&pCurrent, &newElement);
										  
		if (comparison == 0)
			break;
		else if (comparison > 0)
			right = current - 1;
		else
			left = current + 1;
	}
	
	if (comparison < 0)
		current += 1;
	
	XPPtrArray::InsertAt(current, newElement);
	return current;
}

void XPPtrArray::QuickSort (int ( *compare )(const void *elem1, const void *elem2))
{
	if (m_nSize > 1)
		XP_QSORT (m_pData, m_nSize, sizeof(void*), compare);
}

/////////////////////////////////////////////////////////////////////////////

void *XPPtrArray::operator[](int nIndex) const
{
	return GetAt(nIndex);
}

void *&XPPtrArray::operator[](int nIndex)
{
	return ElementAt(nIndex);
}


/////////////////////////////////////////////////////////////////////////////
// XPSortedPtrArray

XPSortedPtrArray::XPSortedPtrArray(XPCompareFunc *compare)
                 :XPPtrArray()
{
	m_CompareFunc = compare;
}

int XPSortedPtrArray::Add(void *newElement)
{
#ifdef MAX_ARR_ELEMS
	if (m_nSize >= MAX_ARR_ELEMS) 
		return -1;	     
#endif			
	if (m_CompareFunc)
		return InsertBinary(newElement, m_CompareFunc);
	else
		return XPPtrArray::Add (newElement);
}

int XPSortedPtrArray::FindIndex(int nStartIndex, void *pToFind) const
{
	if (m_CompareFunc)
		return FindIndexUsing(nStartIndex, pToFind, m_CompareFunc);
	else
		return XPPtrArray::FindIndex(nStartIndex, pToFind);
}

int XPSortedPtrArray::FindIndexUsing(int nStartIndex, void *pToFind, XPCompareFunc *compare) const
{
	if (GetSize() == 0)
		return -1;
	if (!m_CompareFunc)
		return TRUE;

	int current = 0;
	int left = nStartIndex;
	int right = GetSize() - 1;
	int comparison = 0;
	
	while (left <= right)
	{
		current = (left + right) / 2;
		
		void *pCurrent = GetAt(current);
		comparison = compare(&pCurrent, &pToFind);
										  
		if (comparison == 0)
			break;
		else if (comparison > 0)
			right = current - 1;
		else
			left = current + 1;
	}
	
	if (comparison != 0)
		current = -1;
	
	return current;
}

/////////////////////////////////////////////////////////////////////////////

//-----------------------------------
// These functions are not to be called if the array is sorted (i.e. has a compare func)
//-----------------------------------

void XPSortedPtrArray::SetAt(int index, void *newElement)
{ 
	if (!m_CompareFunc)
		XPPtrArray::SetAt (index, newElement);
	else
		XP_ASSERT(FALSE); // Illegal operation because the array is sorted
}


void XPSortedPtrArray::InsertAt(int index, void *newElement, int count)
{ 
	if (!m_CompareFunc)
		XPPtrArray::InsertAt (index, newElement, count);
	else
		XP_ASSERT(FALSE); // Illegal operation because the array is sorted
}


void XPSortedPtrArray::InsertAt(int index, const XPPtrArray *array)
{ 
	if (!m_CompareFunc)
		XPPtrArray::InsertAt (index, array);
	else
		XP_ASSERT(FALSE); // Illegal operation because the array is sorted
}


/////////////////////////////////////////////////////////////////////////////
// Diagnostics

#ifdef DEBUG
XP_Bool XPPtrArray::VerifySort() const
{
	return TRUE;
}

XP_Bool XPSortedPtrArray::VerifySort() const
{
	// Check that the assumption of sorting in the array is valid.
	if (GetSize() > 0 && m_CompareFunc)
	{
	    void *cur =  GetAt(0);
	    for (int i = 1; i < GetSize(); i++)
		{
			void *prev = cur;
			cur = GetAt(i);
			if (m_CompareFunc(&cur, &prev) < 0)
			{
				XP_ASSERT(FALSE);
				return FALSE;
			}
		}
	}
	
	return TRUE;
}
#endif


///////////////////////////////////////////////////////////////////////////////


msg_StringArray::msg_StringArray(XP_Bool ownsMemory, XPCompareFunc *compare)
                :XPSortedPtrArray(compare)
{
	m_ownsMemory = ownsMemory;
}

msg_StringArray::~msg_StringArray()
{
	RemoveAll();
}

int msg_StringArray::Add(void *string)
{
	return XPPtrArray::Add(m_ownsMemory ? XP_STRDUP((char *)string) : string);
}

void msg_StringArray::RemoveAll()
{
    if (m_ownsMemory)
    {
        for (int i = 0; i < GetSize(); i++)
        {
            void *v = (void *)GetAt(i);
            XP_FREEIF(v);
        }
    }
    XPSortedPtrArray::RemoveAll(); // call the base class to shrink m_pData list
}

XP_Bool msg_StringArray::ImportTokenList(const char *list, const char *tokenSeparators /* = " ," */)
{
	// Tokenizes the input string and builds up the array of strings based on the 
	// optional caller-provided token separators. 

	XP_ASSERT(m_ownsMemory); // must own the memory for the substrings
	if (list && m_ownsMemory)
	{
		char *scratch = XP_STRDUP(list); // make a copy cause strtok will change it
		if (scratch)
		{
			char *elem = XP_STRTOK(scratch, tokenSeparators);
			if (elem)
			{
				Add (elem);
				while (NULL != (elem = XP_STRTOK(NULL, tokenSeparators)))
					Add (elem);
			}
			XP_FREE(scratch);
			return TRUE;
		}
	}
	return FALSE;
}

char *msg_StringArray::ExportTokenList(const char *separator /* = " ," */)
{
	// Catenates all the member strings into a big string separated by optional
	// caller-provided string. The return value must be freed by the caller

	int i, len = 0;
	int lenSep = XP_STRLEN(separator);

	for (i = 0; i < GetSize(); i++)
		len += XP_STRLEN(GetAt(i)) + lenSep;

	char *list = (char *)XP_ALLOC(len + 1);
	if (list)
	{
		*list = '\0';
		for (i = 0; i < GetSize(); i++)
		{
			if (i > 0)
				XP_STRCAT(list, separator);
			XP_STRCAT(list, GetAt(i));
		}
	}
	return list;
}
