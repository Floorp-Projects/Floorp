/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsUint8Array.h"

MOZ_DECL_CTOR_COUNTER(nsUint8Array)

nsUint8Array::nsUint8Array()
{
	m_pData = nsnull;
	m_nSize = m_nMaxSize = m_nGrowBy = 0;
}

nsUint8Array::~nsUint8Array()
{
	delete[] (PRUint8*)m_pData;
}

void nsUint8Array::SetSize(PRInt32 nNewSize, PRInt32 nGrowBy)
{
//	AssertValid(this);
	NS_ASSERTION(nNewSize >= 0, "can't set size to negative value");

	if (nGrowBy != -1)
		m_nGrowBy = nGrowBy;  // set new size

	if (nNewSize == 0)
	{
		// shrink to nothing
		delete[] (PRUint8*)m_pData;
		m_pData = nsnull;
		m_nSize = m_nMaxSize = 0;
	}
	else if (m_pData == nsnull)
	{
		// create one with exact size
		m_pData = (PRUint8*) new PRUint8[nNewSize * sizeof(PRUint8)];

		memset(m_pData, 0, nNewSize * sizeof(PRUint8));  // zero fill

		m_nSize = m_nMaxSize = nNewSize;
	}
	else if (nNewSize <= m_nMaxSize)
	{
    // it fits
    if (nNewSize > m_nSize)
    {
	    // initialize the new elements

      nsCRT::memset(&m_pData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(PRUint8));

    }

		m_nSize = nNewSize;
	}
	else
	{
		// otherwise, grow array
		PRInt32 nGrowBy = m_nGrowBy;
		if (nGrowBy == 0)
		{
			nGrowBy = PR_MIN(1024, PR_MAX(4, m_nSize / 8));
		}
		PRInt32 nNewMax;
		if (nNewSize < m_nMaxSize + nGrowBy)
			nNewMax = m_nMaxSize + nGrowBy;  // granularity
		else
			nNewMax = nNewSize;  // no slush

		NS_ASSERTION(nNewMax >= m_nMaxSize, "no wraparound");  // no wrap around
		PRUint8* pNewData = (PRUint8*) new PRUint8[nNewMax * sizeof(PRUint8)];

		// copy new data from old
    nsCRT::memcpy(pNewData, m_pData, m_nSize * sizeof(PRUint8));

		NS_ASSERTION(nNewSize > m_nSize, "did't grow size");

    nsCRT::memset(&pNewData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(PRUint8));

		delete[] (PRUint8*)m_pData;
		m_pData = pNewData;
		m_nSize = nNewSize;
		m_nMaxSize = nNewMax;
	}
}

void nsUint8Array::FreeExtra()
{

	if (m_nSize != m_nMaxSize)
	{
		// shrink to desired size
		PRUint8* pNewData = nsnull;
		if (m_nSize != 0)
		{
			pNewData = (PRUint8*) new PRUint8[m_nSize * sizeof(PRUint8)];
			// copy new data from old
			memcpy(pNewData, m_pData, m_nSize * sizeof(PRUint8));
		}

		// get rid of old stuff (note: no destructors called)
		delete[] (PRUint8*)m_pData;
		m_pData = pNewData;
		m_nMaxSize = m_nSize;
	}
}

/////////////////////////////////////////////////////////////////////////////

void nsUint8Array::SetAtGrow(PRInt32 nIndex, PRUint8 newElement)
{
	NS_ASSERTION(nIndex >= 0, "can't insert at negative index");

	if (nIndex >= m_nSize)
		SetSize(nIndex+1);
	m_pData[nIndex] = newElement;
}

nsresult nsUint8Array::InsertAt(PRInt32 nIndex, PRUint8 newElement, PRInt32 nCount)
{
	NS_ASSERTION(nIndex >= 0, "can't insert at negative index");
	NS_ASSERTION(nCount > 0, "have to insert something");     // zero or negative size not allowed

	if (nIndex >= m_nSize)
	{
		// adding after the end of the array
		SetSize(nIndex + nCount);  // grow so nIndex is valid
	}
	else
	{
		// inserting in the middle of the array
		PRInt32 nOldSize = m_nSize;
		SetSize(m_nSize + nCount);  // grow it to new size
		// shift old data up to fill gap
    nsCRT::memmove(&m_pData[nIndex+nCount], &m_pData[nIndex],
			(nOldSize-nIndex) * sizeof(PRUint8));

		// re-init slots we copied from

		nsCRT::memset(&m_pData[nIndex], 0, nCount * sizeof(PRUint8));

	}

	// insert new value in the gap
	NS_ASSERTION(nIndex + nCount <= m_nSize, "overflow");
	while (nCount--)
		m_pData[nIndex++] = newElement;
  return NS_OK;
}

void nsUint8Array::RemoveAt(PRInt32 nIndex, PRInt32 nCount)
{
  if (nIndex < 0 || nCount < 0 || nIndex + nCount > m_nSize)
  {
    NS_ASSERTION(PR_FALSE, "bad remove index or count");
    return;
  }

	// just remove a range
	PRInt32 nMoveCount = m_nSize - (nIndex + nCount);

	NS_ASSERTION(nMoveCount >= 0, "can't remove nothing");
	if (nMoveCount >= 0)
    nsCRT::memmove(&m_pData[nIndex], &m_pData[nIndex + nCount],
			nMoveCount * sizeof(PRUint8));
	m_nSize -= nCount;
}

nsresult nsUint8Array::InsertAt(PRInt32 nStartIndex, nsUint8Array* pNewArray)
{
  NS_ENSURE_ARG(pNewArray);
	NS_ASSERTION(nStartIndex >= 0, "start index must be positive");

	if (pNewArray->GetSize() > 0)
	{
		InsertAt(nStartIndex, pNewArray->GetAt(0), pNewArray->GetSize());
		for (PRInt32 i = 0; i < pNewArray->GetSize(); i++)
			SetAt(nStartIndex + i, pNewArray->GetAt(i));
	}
  return NS_OK;
}


 PRInt32 nsUint8Array::GetSize() const
	{ return m_nSize; }
 PRInt32 nsUint8Array::GetUpperBound() const
	{ return m_nSize-1; }
 void nsUint8Array::RemoveAll()
	{ SetSize(0); }
 PRUint8 nsUint8Array::GetAt(PRInt32 nIndex) const
	{ NS_ASSERTION(nIndex >= 0 && nIndex < m_nSize, "out of bounds");
		return m_pData[nIndex]; }
 void nsUint8Array::SetAt(PRInt32 nIndex, PRUint8 newElement)
	{ NS_ASSERTION(nIndex >= 0 && nIndex < m_nSize, "out of bounds");
		m_pData[nIndex] = newElement; }
 PRUint8& nsUint8Array::ElementAt(PRInt32 nIndex)
	{ NS_ASSERTION(nIndex >= 0 && nIndex < m_nSize, "out of bounds");
		return m_pData[nIndex]; }
 PRInt32 nsUint8Array::Add(PRUint8 newElement)
	{ PRInt32 nIndex = m_nSize;
		SetAtGrow(nIndex, newElement);
		return nIndex; }
 PRUint8 nsUint8Array::operator[](PRInt32 nIndex) const
	{ return GetAt(nIndex); }
 PRUint8& nsUint8Array::operator[](PRInt32 nIndex)
	{ return ElementAt(nIndex); }


/////////////////////////////////////////////////////////////////////////////
// Diagnostics

#ifdef _DEBUG

void nsUint8Array::AssertValid() const
{
}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
