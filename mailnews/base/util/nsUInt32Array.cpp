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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

#include "msgCore.h"    // precompiled header...
#include "prlog.h"

#include "MailNewsTypes.h"
#include "nsUInt32Array.h"
#include "nsQuickSort.h"

MOZ_DECL_CTOR_COUNTER(nsUInt32Array)

nsUInt32Array::nsUInt32Array()
{
	MOZ_COUNT_CTOR(nsUInt32Array);
	m_nSize = 0;
	m_nMaxSize = 0;
	m_nGrowBy = 0;
	m_pData = NULL;
}

nsUInt32Array::~nsUInt32Array()
{
	MOZ_COUNT_DTOR(nsUInt32Array);
	SetSize(0);
}

/////////////////////////////////////////////////////////////////////////////

PRUint32 nsUInt32Array::GetSize() const
{
	return m_nSize;
}

PRBool nsUInt32Array::SetSize(PRUint32 nSize,
                              PRBool adjustGrowth,
                              PRUint32 nGrowBy)
{
	if (adjustGrowth)
		m_nGrowBy = nGrowBy;

#ifdef MAX_ARR_ELEMS
	if (nSize > MAX_ARR_ELEMS);
	{
		PR_ASSERT(nSize <= MAX_ARR_ELEMS); // Will fail
		return PR_FALSE;
	}
#endif

	if (nSize == 0)
	{
		// Remove all elements
		PR_Free(m_pData);
		m_nSize = 0;
		m_nMaxSize = 0;
		m_pData = NULL;
	}
	else if (m_pData == NULL)
	{
		// Create a new array
		m_nMaxSize = PR_MAX(8, nSize);
		m_pData = (PRUint32 *)PR_Calloc(1, m_nMaxSize * sizeof(PRUint32));
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
	    nsCRT::memset(&m_pData[m_nSize], 0, (nSize - m_nSize) * sizeof(PRUint32));

		m_nSize = nSize;
	}
	else
	{
		// The array needs to grow, figure out how much
		PRUint32 nMaxSize;
		nGrowBy  = PR_MAX(m_nGrowBy, PR_MIN(1024, PR_MAX(8, m_nSize / 8)));
		nMaxSize = PR_MAX(nSize, m_nMaxSize + nGrowBy);
#ifdef MAX_ARR_ELEMS
		nMaxSize = PR_MIN(MAX_ARR_ELEMS, nMaxSize);
#endif

		PRUint32 *pNewData = (PRUint32 *)PR_Malloc(nMaxSize * sizeof(PRUint32));
		if (pNewData)
		{
			// Copy the data from the old array to the new one
			nsCRT::memcpy(pNewData, m_pData, m_nSize * sizeof(PRUint32));

			// Zero out the remaining elements
			nsCRT::memset(&pNewData[m_nSize], 0, (nSize - m_nSize) * sizeof(PRUint32));
			m_nSize = nSize;
			m_nMaxSize = nMaxSize;

			// Free the old array
			PR_Free(m_pData);
			m_pData = pNewData;
		}
	}

	return nSize == m_nSize;
}

/////////////////////////////////////////////////////////////////////////////

PRUint32 &nsUInt32Array::ElementAt(PRUint32 nIndex)
{
	NS_ASSERTION(nIndex < m_nSize, "array index out of bounds");
	return m_pData[nIndex];
}

PRUint32 nsUInt32Array::GetAt(PRUint32 nIndex) const
{
	NS_ASSERTION(nIndex < m_nSize, "array index out of bounds");
	return m_pData[nIndex];
}

PRUint32 *nsUInt32Array::GetData() 
{
	return m_pData;
}

void nsUInt32Array::SetAt(PRUint32 nIndex, PRUint32 newElement)
{
	NS_ASSERTION(nIndex < m_nSize, "array index out of bounds");
	m_pData[nIndex] = newElement;
}

/////////////////////////////////////////////////////////////////////////////

PRUint32 nsUInt32Array::Add(PRUint32 newElement)
{
	PRUint32 nIndex = m_nSize;

#ifdef MAX_ARR_ELEMS
	if (nIndex >= MAX_ARR_ELEMS) 
		return -1;	     
#endif			

	SetAtGrow(nIndex, newElement);
	return nIndex;
}

PRUint32 nsUInt32Array::Add(PRUint32 *elementPtr, PRUint32 numElements) 
{ 
	if (SetSize(m_nSize + numElements))
		nsCRT::memcpy(m_pData + m_nSize - numElements, elementPtr, numElements * sizeof(PRUint32)); 

	return m_nSize; 
} 

PRUint32 *nsUInt32Array::CloneData() 
{ 
	PRUint32 *copyOfData = (PRUint32 *)PR_Malloc(m_nSize * sizeof(PRUint32)); 
	if (copyOfData) 
		nsCRT::memcpy(copyOfData, m_pData, m_nSize * sizeof(PRUint32)); 

	return copyOfData; 
} 

void nsUInt32Array::InsertAt(PRUint32 nIndex, PRUint32 newElement, PRUint32 nCount)
{
	NS_ASSERTION(nCount > 0, "can't insert 0 elements");

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
		nsCRT::memmove(&m_pData[nIndex + nCount], &m_pData[nIndex],
			       (nOldSize - nIndex) * sizeof(PRUint32));
	}

	// Insert the new elements
	NS_ASSERTION(nIndex + nCount <= m_nSize, "setting size failed");
	while (nCount--)
		m_pData[nIndex++] = newElement;
}

void nsUInt32Array::InsertAt(PRUint32 nStartIndex, const nsUInt32Array *pNewArray)
{
  NS_ASSERTION(pNewArray, "inserting null array");

  if (pNewArray)
  {
	  if (pNewArray->GetSize() > 0)
	  {
		  InsertAt(nStartIndex, pNewArray->GetAt(0), pNewArray->GetSize());
		  for (PRUint32 i = 1; i < pNewArray->GetSize(); i++)
			  m_pData[nStartIndex + i] = pNewArray->GetAt(i);
	  }
  }
}

void nsUInt32Array::RemoveAll()
{
	SetSize(0);
}

void nsUInt32Array::RemoveAt(PRUint32 nIndex, PRUint32 nCount)
{
	NS_ASSERTION(nIndex + nCount <= m_nSize, "removing past end of array");

	if (nCount > 0)
	{
		// Make sure not to overstep the end of the array
		int nMoveCount = m_nSize - (nIndex + nCount);
		if (nCount && nMoveCount)
			nsCRT::memmove(&m_pData[nIndex], &m_pData[nIndex + nCount],
		               nMoveCount * sizeof(PRUint32));

		m_nSize -= nCount;
	}
}

void nsUInt32Array::SetAtGrow(PRUint32 nIndex, PRUint32 newElement)
{
	if (nIndex >= m_nSize)
		SetSize(nIndex+1);
	m_pData[nIndex] = newElement;
}

/////////////////////////////////////////////////////////////////////////////

void nsUInt32Array::CopyArray(nsUInt32Array *oldA)
{
	CopyArray(*oldA);
}

void nsUInt32Array::CopyArray(nsUInt32Array &oldA)
{
	if (m_pData)
		PR_Free(m_pData);
	m_nSize = oldA.m_nSize;
	m_nMaxSize = oldA.m_nMaxSize;
	m_pData = (PRUint32 *)PR_Malloc(m_nSize * sizeof(PRUint32));
	if (m_pData)
		nsCRT::memcpy(m_pData, oldA.m_pData, m_nSize * sizeof(PRUint32));
}

/////////////////////////////////////////////////////////////////////////////

static int PR_CALLBACK CompareDWord (const void *v1, const void *v2, void *)
{
	// QuickSort callback to compare array values
	PRUint32 i1 = *(PRUint32 *)v1;
	PRUint32 i2 = *(PRUint32 *)v2;
	return i1 - i2;
}

void nsUInt32Array::QuickSort (int (* PR_CALLBACK compare) (const void *elem1, const void *elem2, void *data))
{
	if (m_nSize > 1)
		NS_QuickSort(m_pData, m_nSize, sizeof(PRUint32), compare ? compare : CompareDWord, nsnull);
}
