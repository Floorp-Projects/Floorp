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
#ifndef _DWordArray_H_
#define _DWordArray_H_

class XPDWordArray
{
public:
	// Construction/destruction
	                   XPDWordArray();
	                   ~XPDWordArray();

	// State/attribute member functions
	int                GetSize() const;
	XP_Bool            SetSize(int nNewSize, int nGrowBy = -1);

	// Accessor member functions
	uint32            &ElementAt(int nIndex);
	uint32             GetAt(int nIndex) const;
	uint32            *GetData();
	void               SetAt(int nIndex, uint32 newElement);

	// Insertion/deletion member functions
	int                Add(uint32 newElement);
	uint               Add(uint32 *elementPtr, uint numElements); 
	void	           InsertAt(int nIndex, uint32 newElement, int nCount = 1);
	void               InsertAt(int nStartIndex, const XPDWordArray *pNewArray);
	void               RemoveAll();
	void               RemoveAt(int nIndex, int nCount = 1);
	void               SetAtGrow(int nIndex, uint32 newElement);

	// Sorting member functions
	void               QuickSort(int (*compare) (const void *elem1, const void *elem2) = NULL);

	// Overloaded operators
	uint32             operator[](int nIndex) const { return GetAt(nIndex); }
	uint32            &operator[](int nIndex) { return ElementAt(nIndex); }

	// Miscellaneous member functions
	uint32            *CloneData(); 
	void               CopyArray(XPDWordArray *oldA);
	void               CopyArray(XPDWordArray &oldA);

protected:
    // Member data
	int m_nSize;
	int m_nMaxSize;
	int m_nGrowBy;
	uint32* m_pData;
};


#endif // _DWordArray_H_
