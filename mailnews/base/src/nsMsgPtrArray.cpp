/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// ptrarray.cpp
/* for MIN */
#include "xp.h"

#include "plstr.h"
#include "prlog.h"
#include "prmem.h"

#include "nsMsgPtrArray.h"
#include "xp_qsort.h"
#ifndef XP_MAC  // pkc
#ifndef MAX /* avoid redefinition warning... but this really should not be here.. */
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* MAX */
#endif 


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

XPPtrArray::XPPtrArray()
{
	m_pData = NULL;
	m_nSize = m_nMaxSize = m_nGrowBy = 0;
}

XPPtrArray::~XPPtrArray()
{
//	XP_ASSERT_VALID(this);

	delete[] (char*)m_pData;
}

PRBool XPPtrArray::SetSize(int nNewSize, int nGrowBy)
{
//	XP_ASSERT_VALID(this);
	PR_ASSERT(nNewSize >= 0);

	if (nGrowBy != -1)
		m_nGrowBy = nGrowBy;  // set new size

	if (nNewSize == 0)
	{
		// shrink to nothing
		delete[] (char*)m_pData;
		m_pData = NULL;
		m_nSize = m_nMaxSize = 0;
	}
	else if (m_pData == NULL)
	{
		// create one with exact size
#ifdef SIZE_T_MAX
		PR_ASSERT(nNewSize <= SIZE_T_MAX/sizeof(void*));    // no overflow
#endif
		m_pData = (void**) new char[nNewSize * sizeof(void*)];
		if (m_pData == NULL)
		{
			m_nSize = 0;
			return PR_FALSE;
		}

		memset(m_pData, 0, nNewSize * sizeof(void*));  // zero fill

		m_nSize = m_nMaxSize = nNewSize;
	}
	else if (nNewSize <= m_nMaxSize)
	{
		// it fits
		if (nNewSize > m_nSize)
		{
			// initialize the new elements

			memset(&m_pData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(void*));

		}

		m_nSize = nNewSize;
	}
	else
	{
		// otherwise, grow array
		int nGrowBy = m_nGrowBy;
		if (nGrowBy == 0)
		{
			// heuristically determine growth when nGrowBy == 0
			//  (this avoids heap fragmentation in many situations)
			nGrowBy = MIN(1024, MAX(4, m_nSize / 8));
		}
#ifdef MAX_ARR_ELEMS
			if (m_nSize + nGrowBy > MAX_ARR_ELEMS)
				nGrowBy = MAX_ARR_ELEMS - m_nSize;
#endif
		int nNewMax;
		if (nNewSize < m_nMaxSize + nGrowBy)
			nNewMax = m_nMaxSize + nGrowBy;  // granularity
		else
			nNewMax = nNewSize;  // no slush

#ifdef SIZE_T_MAX
		if (nNewMax >= SIZE_T_MAX/sizeof(void*))
			return PR_FALSE;
		PR_ASSERT(nNewMax <= SIZE_T_MAX/sizeof(void*)); // no overflow
#endif
		PR_ASSERT(nNewMax >= m_nMaxSize);  // no wrap around
		void** pNewData = (void**) new char[nNewMax * sizeof(void*)];

		if (pNewData != NULL)
		{
			// copy new data from old
			memcpy(pNewData, m_pData, m_nSize * sizeof(void*));

			// construct remaining elements
			PR_ASSERT(nNewSize > m_nSize);

			memset(&pNewData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(void*));


			// get rid of old stuff (note: no destructors called)
			delete[] (char*)m_pData;
			m_pData = pNewData;
			m_nSize = nNewSize;
			m_nMaxSize = nNewMax;
		}
		else
		{
			return PR_FALSE;
		}
	}
	return PR_TRUE;
}

void XPPtrArray::FreeExtra()
{
//	XP_ASSERT_VALID(this);

	if (m_nSize != m_nMaxSize)
	{
		// shrink to desired size
#ifdef SIZE_T_MAX
		PR_ASSERT(m_nSize <= SIZE_T_MAX/sizeof(void*)); // no overflow
#endif
		void** pNewData = NULL;
		if (m_nSize != 0)
		{
			pNewData = (void**) new char[m_nSize * sizeof(void*)];
			// copy new data from old
			memcpy(pNewData, m_pData, m_nSize * sizeof(void*));
		}

		// get rid of old stuff (note: no destructors called)
		delete[] (char*)m_pData;
		m_pData = pNewData;
		m_nMaxSize = m_nSize;
	}
}

/////////////////////////////////////////////////////////////////////////////

void XPPtrArray::SetAtGrow(int nIndex, void* newElement)
{
//	XP_ASSERT_VALID(this);
	PR_ASSERT(nIndex >= 0);

	if (nIndex >= m_nSize)
		SetSize(nIndex+1);
	m_pData[nIndex] = newElement;
}

void XPPtrArray::InsertAt(int nIndex, void* newElement, int nCount)
{
//	XP_ASSERT_VALID(this);
	PR_ASSERT(nIndex >= 0);    // will expand to meet need
	PR_ASSERT(nCount > 0);     // zero or negative size not allowed

	if (nIndex >= m_nSize)
	{
		// adding after the end of the array
		SetSize(nIndex + nCount);  // grow so nIndex is valid
	}
	else
	{
		// inserting in the middle of the array
		int nOldSize = m_nSize;
		SetSize(m_nSize + nCount);  // grow it to new size
		// shift old data up to fill gap
		memmove(&m_pData[nIndex+nCount], &m_pData[nIndex],
			(nOldSize-nIndex) * sizeof(void*));

		// re-init slots we copied from

		memset(&m_pData[nIndex], 0, nCount * sizeof(void*));

	}

	// insert new value in the gap
	PR_ASSERT(nIndex + nCount <= m_nSize);
	while (nCount--)
		m_pData[nIndex++] = newElement;
}

// returns PR_TRUE if ptr was found, and therefore removed.
PRBool XPPtrArray::Remove(void *pToRemove)
{
	int index = FindIndex(0, pToRemove);
	if (index != -1)
	{
		RemoveAt(index);
		return PR_TRUE;
	}
	else
		return PR_FALSE;

}

void XPPtrArray::RemoveAt(int nIndex, int nCount)
{
//	XP_ASSERT_VALID(this);
	PR_ASSERT(nIndex >= 0);
	PR_ASSERT(nCount >= 0);
	PR_ASSERT(nIndex + nCount <= m_nSize);

	// just remove a range
	int nMoveCount = m_nSize - (nIndex + nCount);

	if (nMoveCount)
		XP_MEMMOVE(&m_pData[nIndex], &m_pData[nIndex + nCount],
			nMoveCount * sizeof(void*));
	m_nSize -= nCount;
}

void XPPtrArray::InsertAt(int nStartIndex, const XPPtrArray* pNewArray)
{
//	XP_ASSERT_VALID(this);
	PR_ASSERT(pNewArray != NULL);
//	XP_ASSERT_VALID(pNewArray);
	PR_ASSERT(nStartIndex >= 0);

	if (pNewArray->GetSize() > 0)
	{
		InsertAt(nStartIndex, pNewArray->GetAt(0), pNewArray->GetSize());
		for (int i = 0; i < pNewArray->GetSize(); i++)
			SetAt(nStartIndex + i, pNewArray->GetAt(i));
	}
}

// Remove all elements in pArray from 'this'
void XPPtrArray::RemoveAt (int nStartIndex, const XPPtrArray *pArray)
{
	PR_ASSERT(pArray!= NULL);
	PR_ASSERT(nStartIndex >= 0);

	if (pArray->GetSize() > 0)
		for (int i = 0; i < pArray->GetSize(); i++)
		{
			int index = FindIndex(nStartIndex, pArray->GetAt(i));
			if (index >= 0)
				RemoveAt (index);
		}
}

void XPPtrArray::QuickSort (int ( *compare )(const void *elem1, const void *elem2 ))
{
	if (m_nSize > 1)
		XP_QSORT (m_pData, m_nSize, sizeof(void*), compare);
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

 int XPPtrArray::GetSize() const
	{ return m_nSize; }
 int XPPtrArray::GetUpperBound() const
	{ return m_nSize-1; }
 void XPPtrArray::RemoveAll()
	{ SetSize(0); }
 void* XPPtrArray::GetAt(int nIndex) const
	{ PR_ASSERT(nIndex >= 0 && nIndex < m_nSize);
		return m_pData[nIndex]; }
 void XPPtrArray::SetAt(int nIndex, void* newElement)
	{ PR_ASSERT(nIndex >= 0 && nIndex < m_nSize);
		m_pData[nIndex] = newElement; }
 void*& XPPtrArray::ElementAt(int nIndex)
	{ PR_ASSERT(nIndex >= 0 && nIndex < m_nSize);
		return m_pData[nIndex]; }
int XPPtrArray::Add(void* newElement)
{
	int nIndex = m_nSize;
#ifdef XP_WIN16
		if (nIndex >= 0x10000L / 4L) 
		{
//			PR_ASSERT(PR_FALSE);
			return -1;	     
		}
#endif			
		SetAtGrow(nIndex, newElement);
		return nIndex; }
 void* XPPtrArray::operator[](int nIndex) const
	{ return GetAt(nIndex); }
 void*& XPPtrArray::operator[](int nIndex)
	{ return ElementAt(nIndex); }


PRBool XPPtrArray::IsValidIndex(PRInt32 nIndex)
{
	return (nIndex < GetSize() && nIndex >= 0);
}

int XPPtrArray::InsertBinary(void* newElement, int ( *compare )(const void *elem1, const void *elem2 ))
{
	int		current = 0;
	int		left = 0;
	int		right = GetSize() - 1;
	int		comparison = 0;
	
	while (left <= right)
	{
		current = (left + right) / 2;
		
		void* pCurrent = GetAt(current);
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

/////////////////////////////////////////////////////////////////////////////
// XPSortedPtrArray

XPSortedPtrArray::XPSortedPtrArray(XPCompareFunc *compare) 
{
	m_CompareFunc = compare;
}

int XPSortedPtrArray::Add(void* newElement)
{
#ifdef XP_WIN16
	if (m_nSize >= 0x10000L / 4L) 
	{
//			PR_ASSERT(PR_FALSE);
		return -1;	     
	}
#endif			
	if (m_CompareFunc)
		return InsertBinary(newElement, m_CompareFunc);
	else
		return XPPtrArray::Add (newElement);
}

int XPSortedPtrArray::FindIndex(int nStartIndex, void* pToFind) const
{
	if (m_CompareFunc)
		return FindIndexUsing(nStartIndex, pToFind, m_CompareFunc);
	else
		return XPPtrArray::FindIndex (nStartIndex, pToFind);
}

int XPSortedPtrArray::FindIndexUsing(int nStartIndex, void* pToFind, XPCompareFunc* compare) const
{
	if (GetSize() == 0)
		return -1;

	int		current = 0;
	int		left = nStartIndex;
	int		right = GetSize() - 1;
	int		comparison = 0;
	
	while (left <= right)
	{
		current = (left + right) / 2;
		
		void* pCurrent = GetAt(current);
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
// Diagnostics

#ifdef _DEBUG
void XPPtrArray::AssertValid() const
{
	if (m_pData == NULL)
	{
		PR_ASSERT(m_nSize == 0);
		PR_ASSERT(m_nMaxSize == 0);
	}
	else
	{
		PR_ASSERT(m_nSize >= 0);
		PR_ASSERT(m_nMaxSize >= 0);
		PR_ASSERT(m_nSize <= m_nMaxSize);
	}
}
#endif //_DEBUG


#ifdef DEBUG
PRBool XPPtrArray::VerifySort() const
{
	return PR_TRUE;
}

PRBool XPSortedPtrArray::VerifySort() const
{
	// Check that the assumption of sorting in the array is valid.
	if (GetSize() <= 0)
		return PR_TRUE;
	if (!m_CompareFunc)
		return PR_TRUE;

    void* cur =  GetAt(0);
    for (int i = 1; i < GetSize(); i++)
    {
		void* prev = cur;
		cur = GetAt(i);
		if (m_CompareFunc(&cur, &prev) < 0)
		{
			PR_ASSERT(PR_FALSE);
			return PR_FALSE;
		}
    }
    return PR_TRUE;
}

#endif // DEBUG

//-----------------------------------
// These functions are not to be called if the array is sorted (i.e. has a compare func)
//-----------------------------------

void XPSortedPtrArray::SetAt(int index, void *newElement)
{ 
	if (!m_CompareFunc)
		XPPtrArray::SetAt (index, newElement);
	else
		PR_ASSERT(PR_FALSE); // Illegal operation because the array is sorted
}


void XPSortedPtrArray::InsertAt(int index, void *newElement, int count)
{ 
	if (!m_CompareFunc)
		XPPtrArray::InsertAt (index, newElement, count);
	else
		PR_ASSERT(PR_FALSE); // Illegal operation because the array is sorted
}


void XPSortedPtrArray::InsertAt(int index, const XPPtrArray *array)
{ 
	if (!m_CompareFunc)
		XPPtrArray::InsertAt (index, array);
	else
		PR_ASSERT(PR_FALSE); // Illegal operation because the array is sorted
}


///////////////////////////////////////////////////////////////////////////////


msg_StringArray::msg_StringArray (PRBool ownsMemory, XPCompareFunc *compare) : XPSortedPtrArray(compare)
{
	m_ownsMemory = ownsMemory;
}


msg_StringArray::~msg_StringArray()
{
	RemoveAll();
}


int msg_StringArray::Add (void *string)
{
	return XPSortedPtrArray::Add (m_ownsMemory ? PL_strdup((char*)string) : string);
}


void msg_StringArray::InsertAt (int nIndex, void *string, int nCount /*= 1*/)
{
	XPSortedPtrArray::InsertAt (nIndex, m_ownsMemory ? PL_strdup((char*)string) : string, nCount);
}


void msg_StringArray::RemoveAll ()
{
	if (m_ownsMemory)
	{
		for (int i = 0; i < GetSize(); i++)
		{
			void *v = (void*) GetAt (i);
			PR_FREEIF(v);
		}
	}
	XPSortedPtrArray::RemoveAll(); // call the base class to shrink m_pData list
}


PRBool msg_StringArray::ImportTokenList (const char *list, const char *tokenSeparators /* = " ," */)
{
	// Tokenizes the input string and builds up the array of strings based on the 
	// optional caller-provided token separators. 

	PR_ASSERT(m_ownsMemory); // must own the memory for the substrings
	if (list && m_ownsMemory)
	{
		char *scratch = PL_strdup(list); // make a copy cause strtok will change it
		if (scratch)
		{
			char *elem = strtok (scratch, tokenSeparators);
			if (elem)
			{
				Add (elem);
				while (NULL != (elem = strtok (NULL, tokenSeparators)))
					Add (elem);
			}
			PR_Free(scratch);
			return PR_TRUE;
		}
	}
	return PR_FALSE;
}



char *msg_StringArray::ExportTokenList (const char *separator /* = " ," */)
{
	// Catenates all the member strings into a big string separated by optional
	// caller-provided string. The return value must be freed by the caller

	if (0 == GetSize())
		return NULL;

	int i, len = 0;
	int lenSep = PL_strlen(separator);

	for (i = 0; i < GetSize(); i++)
		len += PL_strlen (GetAt(i)) + lenSep;

	char *list = (char*) PR_Malloc(len + 1);
	if (list)
	{
		*list = '\0';
		for (i = 0; i < GetSize(); i++)
		{
			if (i > 0)
				PL_strcat(list, separator);
			PL_strcat(list, GetAt(i));
		}
	}
	return list;
}

