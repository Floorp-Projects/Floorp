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

#ifndef __CSTRING_H_
#define __CSTRING_H_

class CString {
	public:
		// Constructors
		CString();
		CString(const CString& stringSrc);
		CString(LPCSTR lpsz);
#ifdef _WIN32
		CString(LPOLESTR lpsz);
#endif

		// Attributes & Operations
		int		GetLength() const;
		BOOL	IsEmpty() const;
		void	Empty();                      // free up the data
		
		char	GetAt(int nIndex) const;      // 0 based
		char	operator[](int nIndex) const; // same as GetAt
		void	SetAt(int nIndex, char ch);
		operator LPCSTR() const;              // as a 'C' string

		// overloaded assignment
		const CString& operator=(const CString& str);
		const CString& operator=(char ch);
		const CString& operator=(LPCSTR lpsz);
#ifdef _WIN32
		const CString& operator=(LPOLESTR lpsz);
#endif
		
		// string concatenation
		const CString& operator+=(const CString& str);
		const CString& operator+=(char ch);
		const CString& operator+=(LPCSTR lpsz);

		friend CString operator+(const CString& str1, const CString& str2);
		friend CString operator+(const CString& str, char ch);
		friend CString operator+(char ch, const CString& str);
		friend CString operator+(const CString& str, LPCSTR lpsz);
		friend CString operator+(LPCSTR lpsz, const CString& str);

		// string comparison
		int Compare(LPCSTR lpsz) const;         // straight character
		int CompareNoCase(LPCSTR lpsz) const;   // ignore case

		// upper/lower conversion
		void MakeUpper();
		void MakeLower();

		// searching (return starting index, or -1 if not found)
		// look for a single character match
		int Find(char ch) const;               // like "C" strchr
		int ReverseFind(char ch) const;
		
		// simple sub-string extraction
		CString Mid(int nFirst, int nCount) const;
		CString Mid(int nFirst) const;
		CString Left(int nCount) const;
		CString Right(int nCount) const;
		
		// load from string resource
		BOOL 	LoadString(HINSTANCE, UINT nID);   // 255 chars max

		// set string from window text
		BOOL 	GetWindowText(HWND hwnd);

		// sets the buffer length and data length to nNewLength, and
		// returns the new address of the buffer
		LPSTR	BufferSetLength(int nNewLength);

	// Implementation
	public:
		~CString();

	private:
		LPSTR 	m_pchData;  	// NULL terminated string
		int		m_nDataLength;  // doesn't include NULL terminator
		int		m_nAllocLength; // doesn't include NULL terminator

		// implementation helpers
		CString(LPCSTR lpszSrc1, int nSrc1Len, LPCSTR lpszSrc2, int nSrc2Len);
		CString(LPCSTR lpsz, int nLen);
		void Init();//public to call explicitly when allocating an array of CStrings
		void AllocBuffer(int nLen);
		void AssignCopy(LPCSTR lpszSrc, int nSrcLen = -1);
		void ConcatInPlace(LPCSTR lpszSrc, int nSrcLen);
#ifdef _WIN32
		void AssignCopy(LPOLESTR lpsz);
#endif
};

inline CString::CString() {Init();}

inline int CString::GetLength() const {return m_nDataLength;}
inline BOOL CString::IsEmpty() const {return m_nDataLength == 0;}

inline char CString::GetAt(int nIndex) const {return m_pchData[nIndex];}
inline char CString::operator[](int nIndex) const {return m_pchData[nIndex];}
inline void CString::SetAt(int nIndex, char ch) {m_pchData[nIndex] = ch;}
inline CString::operator LPCSTR () const {return (LPCSTR)m_pchData;}

inline int CString::Compare(LPCSTR lpsz) const {return lstrcmp(m_pchData, lpsz);}
inline int CString::CompareNoCase(LPCSTR lpsz) const {return lstrcmpi(m_pchData, lpsz);}

inline void CString::MakeUpper() {AnsiUpper(m_pchData);}
inline void CString::MakeLower() {AnsiLower(m_pchData);}

#endif /* __CSTRING_H_ */

