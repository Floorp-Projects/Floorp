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

#ifndef __INTLWIN_H
#define __INTLWIN_H

#define DEFAULT_WINCSID 0

#define MAXLANGNUM 11

class	CyaFont;

typedef struct EncodingInfo
{
	int  iLangResId ;				// Language Resource ID 
	int  iCSID;		 				// Font's csid which defined by csid.h
	char szPropName[LF_FACESIZE]; 	// Proportional font name
	int  iPropSize;					// Proportional font size
	int  iPropCharset;				// Proportional font's charset-id defined MS
	char szFixName[LF_FACESIZE];	// Fix font name
	int  iFixSize;					// Fix font size 
	int  iFixCharset;				// Fix font's charset-id defined by MS
	int  nCodeset;
	int  csid[5];					// Supported charset list per language
}  EncodingInfo;


class CIntlFont 
{
// Constructor
public:
	CIntlFont();  // this will setup font tables

private:
	EncodingInfo *pEncodingInfoTbl;
	int nEncoding;

public:   // Control functions for Font Table
	void WriteToIniFile(void);
	int  DocCSIDtoID(int doc_csid);
	EncodingInfo *GetEncodingInfo (int id);
	EncodingInfo *GetEncodingInfo (MWContext *pContext);
	void ChangeFont(int id, struct EncodingInfo *ef_font);
	char * GetEncodingName(int id);
};


#define TABLE_UNICODE_API_FLAG 1

#define MAX_FLAG_TABLE_NUM	0x100
#define MASK_FLAG_TABLE		0xFF

class CIntlWin
{
public:
#ifdef XP_WIN32
	static LPWSTR  m_wConvBuf;
#endif
	static int	   m_iConvBufSize;
	static BOOL AllocConvBuffer(int iLength);
public:
	static BOOL FontSelectIgnorePitch(int16 wincsid); 
	static BOOL FontSelectIgnoreCharset(int16 wincsid); 
	static BOOL TextOutWithCyaFont(CyaFont *theNSFont, int16 wincsid, HDC hDC,  
								  int nXStart, int nYStart, LPCSTR  lpString,int  iLength);
	static BOOL GetTextExtentPointWithCyaFont(CyaFont *theNSFont, int wincsid, HDC hDC, LPCSTR lpString, int cbString, LPSIZE lpSize);

	static int MultiByteToWideChar(int16 wincsid, LPCSTR lpMultiByte, int len);
	static CSize GetTextExtent(int16 wincsid, HDC pDC, LPCTSTR pString, int iLength);
	static BOOL TextOut(int16 wincsid, HDC pDC, int nXStart, int nYStart, LPCSTR  lpString,int  iLength);
	static BOOL GetTextExtentPoint(int wincsid, HDC hDC, LPCSTR lpString, int cbString, LPSIZE lpSize);
	static BOOL ExtTextOut(int16 wincsid, HDC pDC, int x, int y, UINT nOptions, LPCRECT lpRect, LPCSTR lpszString, UINT nCount, LPINT lpDxWidths);
#ifdef XP_WIN32
	static int DrawTextEx(int16 wincsid, HDC hdc, LPSTR lpchText, int cchText,LPRECT lprc,UINT dwDTFormat,LPDRAWTEXTPARAMS lpDTParams);
#endif
	static int DrawText(int16 wincsid, HDC hdc, LPSTR lpchText, int cchText,LPRECT lprc,UINT dwDTFormat);

private:
	static int16  m_system_locale_csid;
//#ifdef netscape_font_module
public:
//private:
//#endif // netscape_font_module

#ifdef XP_WIN32
	inline static BOOL UseUnicodeFontAPI(int16 wincsid) { return flagTable[(wincsid) & MASK_FLAG_TABLE];};
#else
	inline static BOOL UseUnicodeFontAPI(int16 wincsid) { return FALSE; };
#endif
	static BOOL UseVirtualFont(); 

	static int16 GetSystemLocaleCsid();
	static int16 CodePageToCsid(UINT cp);

#ifdef XP_WIN32
	static BOOL flagTable[MAX_FLAG_TABLE_NUM];
#endif
};

int16 IntlCharsetToCsid(BYTE charset);
BYTE IntlGetLfCharset(int csid);
const char *IntlGetUIPropFaceName(int csid);
const char *IntlGetUIFixFaceName(int csid);

#endif

