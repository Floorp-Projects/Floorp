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

// ptrarray.h
#ifndef PTR_ARRAY_H_
#define PTR_ARRAY_H_

typedef int XPCompareFunc(const void *, const void * );

#ifdef DEBUG
// These are here to stop people calling
#define VIRTUAL_DEBUG virtual
#else
#define VIRTUAL_DEBUG
#endif // DEBUG

//*****************************************************************************

class XPPtrArray 
{

public:

// Construction
	XPPtrArray();

// Attributes
	int GetSize() const;
	int GetUpperBound() const;
	VIRTUAL_DEBUG PRBool SetSize(int nNewSize, int nGrowBy = -1);
	PRBool IsValidIndex(PRInt32 nIndex);
// Operations
	// Clean up
	void FreeExtra();
	virtual void RemoveAll();

	// Accessing elements
	void* GetAt(int nIndex) const;
	VIRTUAL_DEBUG void SetAt(int nIndex, void* newElement);
	void*& ElementAt(int nIndex);
	virtual int FindIndex (int nStartIndex, void *pToFind) const;
	virtual int FindIndexUsing(int nStartIndex, void *pToFind, XPCompareFunc* compare) const;

	// Potentially growing the array
	VIRTUAL_DEBUG void SetAtGrow(int nIndex, void* newElement);
	virtual int Add(void* newElement);

	// overloaded operator helpers
	void* operator[](int nIndex) const;
	void*& operator[](int nIndex);

	// Operations that move elements around
	VIRTUAL_DEBUG void	InsertAt(int nIndex, void* newElement, int nCount = 1);
	void	RemoveAt(int nIndex, int nCount = 1);
	VIRTUAL_DEBUG void	InsertAt(int nStartIndex, const XPPtrArray* pNewArray);
	void	RemoveAt(int nStartIndex, const XPPtrArray* pNewArray);
	PRBool Remove(void *pToRemove);
	void	QuickSort(XPCompareFunc *compare);
	int		InsertBinary(void* newElement, XPCompareFunc *compare);
#ifdef DEBUG
	virtual PRBool VerifySort() const;
#endif // DEBUG

// Implementation
protected:
	void** m_pData;   // the actual array of data
	int m_nSize;     // # of elements (upperBound - 1)
	int m_nMaxSize;  // max allocated
	int m_nGrowBy;   // grow amount

public:
	virtual ~XPPtrArray();
#ifdef _DEBUG
	void AssertValid() const;
#endif

};

//*****************************************************************************

class XPSortedPtrArray : public XPPtrArray
{
public:
	XPSortedPtrArray(XPCompareFunc *compare);
	virtual int Add(void* newElement);

	// PROMISE: this method will always call the compare-func with pToFind as the
	//			second parameter.
	virtual int FindIndex(int nStartIndex, void *pToFind) const;

	// These functions can only be used on non-sorted PtrArray objects (i.e. compareFunc is NULL)
	virtual void SetAt(int nIndex, void* newElement);
	virtual void InsertAt(int nIndex, void* newElement, int nCount = 1);
	virtual void InsertAt(int nStartIndex, const XPPtrArray* pNewArray);

#ifdef DEBUG
	virtual PRBool VerifySort() const;
#endif // DEBUG

protected:
	// Allows search to use a different compare func from the sort.  This is used
	// by MSG_FolderCache, to find the cache element corresponding to a folder.
	// The corresponding element and folder have identical relative paths, and the
	// relative path is used by both compare funcs, which is why it works.
	// to the found element
	int FindIndexUsing(int nStartIndex, void *pToFind, XPCompareFunc* compare) const;

	XPCompareFunc*	m_CompareFunc; // NULL to simulate unsorted base-class.
};


//*****************************************************************************
// msg_StringArray is a subclass of the generic pointer array class
// which is intended for string operations. 
// - It can "own" the strings, or not.
// - It can be sorted, if you provide a CompareFunc, or not, if you don't

class msg_StringArray : public XPSortedPtrArray
{
public:
	// Overrides from base class
	msg_StringArray (PRBool ownsMemory, XPCompareFunc *compare = NULL); 
	virtual ~msg_StringArray ();

	const char *GetAt (int i) const { return (const char*) XPPtrArray::GetAt(i); }
	virtual int Add (void *string);
	virtual void InsertAt(int nIndex, void *string, int nCount = 1);
	virtual void RemoveAll ();

	PRBool ImportTokenList (const char *list, const char *tokenSeps = ", ");
	char *ExportTokenList (const char *tokenSep = ", ");

protected:
	PRBool m_ownsMemory;
};

//*****************************************************************************

class MSG_FolderInfo;

class MSG_FolderArray : public XPSortedPtrArray
{
public:
	// Override constructor to allow folderArrays to be sorted
	MSG_FolderArray (XPCompareFunc *compare = NULL) : XPSortedPtrArray (compare) {}

	// Encapsulate this typecast, so we don't have to do it all over the place.
	MSG_FolderInfo *GetAt(int i) const { return (MSG_FolderInfo*) XPPtrArray::GetAt(i); }
};

#endif
