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

#ifndef __CUVFS_H
#define __CUVFS_H
#include "stdafx.h"
//------------------------------------------------------------------------------------------------
//
//	CIntlUnicodeVirtualFontStrategy
//
//------------------------------------------------------------------------------------------------
//
//	CIntlUnicodeVirtualFontStrategy class is a static class that handles UTF8 Virtual Font
//	text rendering. 
//	The implementation of CIntlUnicodeVirtualFontStrategy will switch the font and
//  call the member of CIntlWin to do the actual drawing
//  
class CIntlUnicodeVirtualFontStrategy {
public:
	static BOOL GetTextExtentPoint(HDC hDC, LPCSTR pString, int iLength, LPSIZE lpSize);
	static BOOL TextOut(HDC hDC, int nXStart, int nYStart, LPCSTR lpString, int iLength);
	static BOOL GetTextExtentPointWithCyaFont(CyaFont *theNSFont, HDC hDC, LPCSTR pString, int iLength, LPSIZE lpSize);
	static BOOL TextOutWithCyaFont(CyaFont *theNSFont, HDC hDC, int nXStart, int nYStart, LPCSTR lpString, int iLength);

};
#endif
