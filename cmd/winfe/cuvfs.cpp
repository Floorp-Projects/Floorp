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

//--------------------------------------------------------------------------------------------------------
//	Author: Frank Tang ftang@netscape.com x2913
//
//	Text Handlering Routien for Unicode rendering 
//--------------------------------------------------------------------------------------------------------
#include "stdafx.h"
#include "cuvfs.h"
#include "cuvfm.h"
#include "libi18n.h"

//------------------------------------------------------------------------------------------------
//	INTL_CompoundStrFromUTF8
//------------------------------------------------------------------------------------------------
static INTL_CompoundStr* INTL_CompoundStrFromUTF8(LPCSTR pString, int length)
{
	WCHAR ucs2[512];
	int ulen;
	INTL_CompoundStr *This = NULL;
	ulen = CASTINT(INTL_TextToUnicode(CS_UTF8, (unsigned char*)pString, length, ucs2, 512));
	This = INTL_CompoundStrFromUnicode(ucs2, ulen);
	return This;
}

//------------------------------------------------------------------------------------------------
//
//	CIntlUnicodeVirtualFontStrategy::GetTextExtentPoint
//
//------------------------------------------------------------------------------------------------
BOOL CIntlUnicodeVirtualFontStrategy::GetTextExtentPoint(HDC hDC, LPCSTR pString, int iLength, LPSIZE pSize)
{
	pSize->cx = 0;
	pSize->cy = 0;

	CUnicodeVirtualFontMgr ufm(hDC);
	INTL_CompoundStr *str = INTL_CompoundStrFromUTF8(pString, iLength);
	if(str)
	{	
		CDC * pDC = CDC::FromHandle(hDC);
		INTL_Encoding_ID encoding;
		unsigned char *text;
		INTL_CompoundStrIterator iter;
		for(iter = INTL_CompoundStrFirstStr((INTL_CompoundStrIterator) str, &encoding, &text);
				iter != NULL;
					iter = INTL_CompoundStrNextStr(iter, &encoding, &text))
		{
			SIZE textsize;
			pDC->SelectObject(ufm.GetCsidFont(encoding));
			int textlength = XP_STRLEN((char*)text);
			VERIFY(CIntlWin::GetTextExtentPoint(encoding, hDC, (char*)text, textlength, &textsize));
			pSize->cx += textsize.cx;
			if(textsize.cy > pSize->cy)
				pSize->cy = textsize.cy;
		}
		INTL_CompoundStrDestroy(str);
	}
	// The destructor of CUnicodeVirtualFontMgr will reset the font.
	return TRUE;
}
//------------------------------------------------------------------------------------------------
//
//	CIntlUnicodeVirtualFontStrategy::TextOut
//
//------------------------------------------------------------------------------------------------
BOOL CIntlUnicodeVirtualFontStrategy::TextOut(HDC hDC, int nXStart, int nYStart, LPCSTR pString, int iLength)
{
	// Save the current font on stack
	CUnicodeVirtualFontMgr ufm(hDC);
	INTL_CompoundStr *str = INTL_CompoundStrFromUTF8(pString, iLength);
	if(str)
	{	
		CDC * pDC = CDC::FromHandle(hDC);
		INTL_Encoding_ID encoding;
		unsigned char *text;
		INTL_CompoundStrIterator iter;
		for(iter = INTL_CompoundStrFirstStr((INTL_CompoundStrIterator) str, &encoding, &text);
				iter != NULL;
					iter = INTL_CompoundStrNextStr(iter, &encoding, &text))
		{
			SIZE textsize;
			pDC->SelectObject(ufm.GetCsidFont(encoding));
			int textlength = XP_STRLEN((char*)text);
			VERIFY(CIntlWin::TextOut(encoding, hDC, nXStart, nYStart + ufm.CacularAscentDelta(hDC), (char*)text, textlength));
			VERIFY(CIntlWin::GetTextExtentPoint(encoding, hDC, (char*)text, textlength, &textsize));
			nXStart += textsize.cx;
		}
		INTL_CompoundStrDestroy(str);
	}
	// The destructor of CUnicodeVirtualFontMgr will reset the font.
	return TRUE;
}

BOOL CIntlUnicodeVirtualFontStrategy::GetTextExtentPointWithCyaFont(CyaFont *theNSFont, HDC hDC, LPCSTR pString, int iLength, LPSIZE pSize)
{

	pSize->cx = 0;
	pSize->cy = 0;

	CUnicodeVirtualFontMgr ufm(hDC);	// Change this to get attribute from CyaFont instead of hDC 
	INTL_CompoundStr *str = INTL_CompoundStrFromUTF8(pString, iLength);
	if(str)
	{	
		CDC * pDC = CDC::FromHandle(hDC);
		INTL_Encoding_ID encoding;
		unsigned char *text;
		INTL_CompoundStrIterator iter;
		for(iter = INTL_CompoundStrFirstStr((INTL_CompoundStrIterator) str, &encoding, &text);
				iter != NULL;
					iter = INTL_CompoundStrNextStr(iter, &encoding, &text))
		{
			SIZE textsize;
			CyaFont *encodingFont = ufm.GetCsidCyaFont(hDC, encoding);

			encodingFont->PrepareDrawText( hDC );	// Select the Font in the DC

			int textlength = XP_STRLEN((char*)text);
			VERIFY(CIntlWin::GetTextExtentPointWithCyaFont(encodingFont ,encoding, hDC, (char*)text, textlength, &textsize));

			encodingFont->EndDrawText( hDC );		// Restore the old  Font in the DC
			
			pSize->cx += textsize.cx;
			if(textsize.cy > pSize->cy)
				pSize->cy = textsize.cy;
		}
		INTL_CompoundStrDestroy(str);
	}

	// The destructor of CUnicodeVirtualFontMgr will reset the font.
	return TRUE;
	
}	// GetTextExtentPointWithCyaFont

BOOL CIntlUnicodeVirtualFontStrategy::TextOutWithCyaFont(CyaFont *theNSFont, HDC hDC, int nXStart, int nYStart, LPCSTR lpString, int iLength)
{
	// Save the current font on stack
	CUnicodeVirtualFontMgr ufm(hDC);	/* May be we should pass theNSFont instead */
	INTL_CompoundStr *str = INTL_CompoundStrFromUTF8(lpString, iLength);
	if(str)
	{	
		CDC * pDC = CDC::FromHandle(hDC);
		INTL_Encoding_ID encoding;
		unsigned char *text;
		INTL_CompoundStrIterator iter;
		for(iter = INTL_CompoundStrFirstStr((INTL_CompoundStrIterator) str, &encoding, &text);
				iter != NULL;
					iter = INTL_CompoundStrNextStr(iter, &encoding, &text))
		{
			CyaFont *encodingFont = ufm.GetCsidCyaFont(hDC, encoding);
			SIZE textsize;
			int textlength = XP_STRLEN((char*)text);

			encodingFont->PrepareDrawText( hDC );	// Select the Font in the DC
			
			VERIFY(CIntlWin::TextOutWithCyaFont(encodingFont,encoding, hDC, nXStart, nYStart + ufm.CacularAscentDelta(hDC), (char*)text, textlength));
			VERIFY(CIntlWin::GetTextExtentPoint(encoding, hDC, (char*)text, textlength, &textsize));

			encodingFont->EndDrawText( hDC );		// Restore the old  Font in the DC
			
			nXStart += textsize.cx;
		}
		INTL_CompoundStrDestroy(str);
	}
	// The destructor of CUnicodeVirtualFontMgr will reset the font.
	return TRUE;
}	// TextOutWithCyaFont

