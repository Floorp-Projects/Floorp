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

#ifndef _ByteArray_H_
#define _ByteArray_H_

#if !defined(_WINDOWS) && !defined(XP_OS2)
typedef uint8       BYTE;
#endif

class XPByteArray 
{
public:
	// Construction/destruction
	                   XPByteArray();
	                   ~XPByteArray();

	// State/attribute member functions
	int                GetSize() const;
	XP_Bool            SetSize(int nNewSize);

	// Accessor member functions
	BYTE              &ElementAt(int nIndex);
	BYTE               GetAt(int nIndex) const;
	void               SetAt(int nIndex, BYTE newElement);

	// Insertion/deletion member functions
	int                Add(BYTE newElement);
	void	           InsertAt(int nIndex, BYTE newElement, int nCount = 1);
	void               InsertAt(int nStartIndex, const XPByteArray *pNewArray);
	void               RemoveAll();
	void               RemoveAt(int nIndex, int nCount = 1);
	void               SetAtGrow(int nIndex, BYTE newElement);

	// Overloaded operators
	BYTE               operator[](int nIndex) const { return GetAt(nIndex); }
	BYTE              &operator[](int nIndex) { return ElementAt(nIndex); }

	// Use the result carefully, it is only valid until another function called on the array
	BYTE              *GetArray(void) {return((BYTE *)m_pData);}

protected:
    // Member data
	int m_nSize;
	int m_nMaxSize;
	BYTE* m_pData;
};

#endif
