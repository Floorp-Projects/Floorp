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

#include "pch.h"
#ifdef _WIN32
#include <tchar.h>
#endif
#include <assert.h>
#include "cstring.h"

#ifndef _WIN32
// CoTaskMemAlloc uses the default OLE allocator to allocate a memory
// block in the same way that IMalloc::Alloc does
LPVOID NEAR
CoTaskMemAlloc(ULONG cb)
{
	LPMALLOC pMalloc;
	 
	if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
		LPVOID	lpv = pMalloc->Alloc(cb);

		pMalloc->Release();
		return lpv;
	}

	return NULL;
}

// CoTaskMemRealloc changes the size of a previously allocated memory
// block in the same way that IMalloc::Realloc does
LPVOID NEAR
CoTaskMemRealloc(LPVOID lpv, ULONG cb)
{
	LPMALLOC	pMalloc;
	
	if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
		lpv = pMalloc->Realloc(lpv, cb);
		pMalloc->Release();
		return lpv;
	}

	return NULL;
}

// CoTaskMemFree uses the default OLE allocator to free a block of
// memory previously allocated through a call to CoTaskMemAlloc
void NEAR
CoTaskMemFree(LPVOID lpv)
{
	if (lpv) {
		LPMALLOC	pMalloc;
	
		if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
			pMalloc->Free(lpv);
			pMalloc->Release();
		}
	}
}
#endif

// Used as m_pchData for all empty strings
static char g_chNil = '\0';

#ifndef _WIN32
#if defined(DEBUG_blythe)
//  Watch DBCS code from now on.
static BOOL	g_bDBCS = TRUE;
#else
static BOOL	g_bDBCS = GetSystemMetrics(SM_DBCSENABLED);
#endif
#endif

void
CString::Init()
{
	m_nDataLength = m_nAllocLength = 0;
	m_pchData = &g_chNil;
}

CString::CString(const CString& str)
{
	Init();
	AssignCopy(str.m_pchData, str.m_nDataLength);
}

CString::CString(LPCSTR lpsz)
{
	Init();
	AssignCopy(lpsz, -1);
}

#ifdef _WIN32
CString::CString(LPOLESTR lpsz)
{
	Init();
	AssignCopy(lpsz);
}
#endif

CString::CString(LPCSTR lpsz, int nLen)
{
	Init();
	AssignCopy(lpsz, nLen);
}

CString::CString(LPCSTR lpszSrc1, int nSrc1Len, LPCSTR lpszSrc2, int nSrc2Len)
{
	Init();
	AllocBuffer(nSrc1Len + nSrc2Len);
	memcpy(m_pchData, lpszSrc1, nSrc1Len);
	memcpy(&m_pchData[nSrc1Len], lpszSrc2, nSrc2Len);
}

void
CString::AllocBuffer(int nLen)
{
	if (nLen == 0) {
		Init();

	} else {
		// allocate one extra character for '\0' terminator
		m_pchData = (LPSTR)CoTaskMemAlloc(nLen + 1);
		m_pchData[nLen] = '\0';
		m_nDataLength = m_nAllocLength = nLen;
	}
}

void
CString::Empty()
{
	if (m_nAllocLength > 0)
		CoTaskMemFree(m_pchData);
	Init();
}

CString::~CString()
{
	if (m_nAllocLength > 0)
		CoTaskMemFree(m_pchData);
}

void
CString::AssignCopy(LPCSTR lpszSrcData, int nSrcLen)
{
	// nSrcLen of -1 means we should measure
	if (nSrcLen < 0)
		nSrcLen = lpszSrcData ? lstrlen(lpszSrcData) : 0;

	// check if it will fit in the existing buffer
	if (nSrcLen > m_nAllocLength) {
		// won't fit so allocate another buffer
		Empty();
		AllocBuffer(nSrcLen);
	}

	if (nSrcLen != 0)
		memcpy(m_pchData, lpszSrcData, nSrcLen);
	m_nDataLength = nSrcLen;
	m_pchData[m_nDataLength] = '\0';
}

#ifdef _WIN32
void
CString::AssignCopy(LPOLESTR lpsz)
{
	int	nBytes;

	// see how many bytes we need for the buffer
	nBytes = lpsz ? WideCharToMultiByte(CP_ACP, 0, lpsz, -1, NULL, 0, NULL, NULL) : 0;

	// don't include the null terminator in the number of bytes
	if (nBytes > 0)
		nBytes--;

	// check if it will fit in the existing buffer
	if (nBytes > m_nAllocLength) {
		// won't fit so allocate another buffer
		Empty();
		AllocBuffer(nBytes);
	}

	if (nBytes > 0)
		WideCharToMultiByte(CP_ACP, 0, lpsz, -1, m_pchData, nBytes, NULL, NULL);
	m_nDataLength = nBytes;
	m_pchData[m_nDataLength] = '\0';
}
#endif
		
// Overloaded assignment
const CString&
CString::operator =(const CString& str)
{
	AssignCopy(str.m_pchData, str.m_nDataLength);
	return *this;
}

const CString&
CString::operator =(LPCSTR lpsz)
{
	AssignCopy(lpsz, -1);
	return *this;
}

const CString&
CString::operator =(char ch)
{
	AssignCopy(&ch, 1);
	return *this;
}

#ifdef _WIN32
const CString &
CString::operator =(LPOLESTR lpsz)
{
	AssignCopy(lpsz);
	return *this;
}
#endif

// String concatenation
CString
operator +(const CString& str1, const CString& str2)
{
	return CString(str1.m_pchData, str1.m_nDataLength, str2.m_pchData, str2.m_nDataLength);
}

CString
operator +(const CString& str, LPCSTR lpsz)
{
	return CString(str.m_pchData, str.m_nDataLength, lpsz, lpsz ? lstrlen(lpsz) : 0);
}

CString
operator +(LPCSTR lpsz, const CString& str)
{
	return CString(lpsz, lpsz ? lstrlen(lpsz) : 0, str.m_pchData, str.m_nDataLength);
}

CString
operator +(const CString& str, char ch)
{
	return CString(str.m_pchData, str.m_nDataLength, &ch, 1);
}

CString
operator +(char ch, const CString& str)
{
	return CString(&ch, 1, str.m_pchData, str.m_nDataLength);
}

void
CString::ConcatInPlace(LPCSTR lpszSrc, int nSrcLen)
{
	if (nSrcLen > 0) {
		int	nNewAllocLength = m_nDataLength + nSrcLen;

		if (nNewAllocLength > m_nAllocLength) {
			if (m_nAllocLength == 0) {
				m_pchData = (LPSTR)CoTaskMemAlloc(nNewAllocLength + 1);
			
			} else {
				// realloc the existing memory
				m_pchData = (LPSTR)CoTaskMemRealloc(m_pchData, nNewAllocLength + 1);
			}

			m_nAllocLength = nNewAllocLength;
		}
	
		memcpy(&m_pchData[m_nDataLength], lpszSrc, nSrcLen);
		m_nDataLength += nSrcLen;
		m_pchData[m_nDataLength] = '\0';
	}
}

const CString&
CString::operator +=(LPCSTR lpsz)
{
	ConcatInPlace(lpsz, lpsz ? lstrlen(lpsz) : 0);
	return *this;
}

const CString&
CString::operator +=(char ch)
{
	ConcatInPlace(&ch, 1);
	return *this;
}

const CString&
CString::operator +=(const CString& str)
{
	ConcatInPlace(str.m_pchData, str.m_nDataLength);
	return *this;
}

// Searching (return starting index, or -1 if not found)
// look for a single character match
int
CString::Find(char ch) const
{
	LPSTR	lpsz;

#ifdef _WIN32
	lpsz = _tcschr(m_pchData, ch);
#else
	assert(!IsDBCSLeadByte(ch));
	
	// Visual C++ 1.52 runtime has no multi-byte aware version
	// of strchr
	if (g_bDBCS) {
		if (ch == '\0')
			lpsz = m_pchData + m_nDataLength;

		else {
			// Use the MBCS routine to step through the characters
			for (lpsz = m_pchData; *lpsz; lpsz = AnsiNext(lpsz)) {
				if (*lpsz == ch)
					break;
			}
			if (*lpsz == '\0') {
				lpsz = NULL; // No match.
			}
		}

	} else {
		lpsz = strchr(m_pchData, ch);
	}
#endif
	
	return lpsz == NULL ? -1 : (int)(lpsz - m_pchData);
}

int
CString::ReverseFind(char ch) const
{
	LPSTR	lpsz;

#ifdef _WIN32
	// This routine is multi-byte aware
	lpsz = _tcsrchr(m_pchData, (_TUCHAR)ch);
#else
	assert(!IsDBCSLeadByte(ch));

	// Visual C++ 1.52 runtime has no multi-byte aware version
	// of strrchr
	if (g_bDBCS) {
		LPSTR	lpszLast = NULL;

		// Walk forward through the string remembering the last
		// match
		for (lpsz = m_pchData; *lpsz; lpsz = AnsiNext(lpsz)) {
			if (*lpsz == ch)
				lpszLast = lpsz;
		}

		lpsz = lpszLast;

	} else {
		lpsz = strrchr(m_pchData, ch);
	}
#endif
	
	return lpsz == NULL ? -1 : (int)(lpsz - m_pchData);
}

// Simple sub-string extraction
CString
CString::Mid(int nFirst) const
{
	return Mid(nFirst, m_nDataLength - nFirst);
}

CString
CString::Mid(int nFirst, int nCount) const
{
	assert(nFirst >= 0);
	assert(nCount >= 0);

	// out-of-bounds requests return sensible things
	if (nFirst > m_nDataLength)
		nFirst = m_nDataLength;
	if (nFirst + nCount > m_nDataLength)
		nCount = m_nDataLength - nFirst;

	return CString(m_pchData + nFirst, nCount);
}

CString CString::Right(int nCount) const
{
	assert(nCount >= 0);

	if (nCount > m_nDataLength)
		nCount = m_nDataLength;

	return CString(m_pchData + m_nDataLength - nCount, nCount);
}

CString CString::Left(int nCount) const
{
	assert(nCount >= 0);

	if (nCount > m_nDataLength)
		nCount = m_nDataLength;

	return CString(m_pchData, nCount);
}

// Load from string resource
BOOL
CString::LoadString(HINSTANCE hInstance, UINT nID)
{
	char	buf[256];
	int		nLen;

	nLen = ::LoadString(hInstance, nID, buf, sizeof(buf));
	if (nLen == 0)
		return FALSE;  // resource does not exist

	AssignCopy(buf, nLen);
	return TRUE;
}

// Set string from window text
BOOL
CString::GetWindowText(HWND hwnd)
{
	int	nLen = GetWindowTextLength(hwnd);

	if (nLen > m_nAllocLength) {
		Empty();
		AllocBuffer(nLen);
	}

	m_nDataLength = ::GetWindowText(hwnd, m_pchData, m_nAllocLength + 1);
	return TRUE;
}

LPSTR
CString::BufferSetLength(int nNewLength)
{
	assert(nNewLength >= 0);

	if (nNewLength > m_nAllocLength) {
		Empty();
		AllocBuffer(nNewLength);
	}
	
	m_nDataLength = nNewLength;
	m_pchData[nNewLength] = '\0';
	return m_pchData;
}
