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
#include "xp.h"
#include "prtypes.h"
#include "prlog.h"
#include "csid.h"

/*	define CHARSET For Win16 */
#ifndef _WIN32
#ifndef GB2312_CHARSET
#define GB2312_CHARSET	134
#endif  
#ifndef RUSSIAN_CHARSET
#define RUSSIAN_CHARSET 204
#endif  
#ifndef EASTEUROPE_CHARSET
#define EASTEUROPE_CHARSET 238
#endif  
#ifndef GREEK_CHARSET
#define GREEK_CHARSET 161
#endif  
#ifndef TURKISH_CHARSET
#define TURKISH_CHARSET 162
#endif  
#endif /* !_WIN32 */

#if defined(XP_OS2)
#ifndef CALLBACK
#defined CALLBACK
#endif
#endif

PR_PUBLIC_API(int) UNICODE_VERIFYCSIDLIST(int inNumOfItem, int16 *csidlist);
PR_PUBLIC_API(int) UNICODE_MENUFONTID();
PR_PUBLIC_API(int16) UNICODE_MENUFONTCSID();

/*	Private */
static BOOL iswin95();
static UINT unicode_CsidToCodePage(int16 csid);
static BYTE unicode_CsidToCharset(int16 csid);
static int16 unicode_CharsetToCsid(BYTE charset);
static int CALLBACK DetectCharset(LOGFONT FAR* lpelf, TEXTMETRIC FAR*lpntm, int FontType, LPARAM param);
static BOOL unicode_CheckFontOfCsid(HDC hdc, int16 csid);
static BOOL unicode_CheckNJWINForCsid(HDC hdc, int16 csid);
static BOOL unicode_CheckNJWINForCsid(HDC hdc, int16 csid);
static int CALLBACK DetectTwinBridge(LOGFONT FAR* lpelf, TEXTMETRIC FAR*lpntm, int FontType, LPARAM param);
static int CALLBACK DetectUnionway(LOGFONT FAR* lpelf, TEXTMETRIC FAR*lpntm, int FontType, LPARAM param);
static BOOL unicode_CheckUnionwayForCsid(HDC hdc,int16 csid);
static BOOL unicode_IsCsidAvailable(HDC hdc, int16 csid);

#ifdef _WIN32
BOOL unicode_UnicodeFontOfCodePage(HDC hdc, UINT cp);
#endif

typedef struct 
{
	int16	csid;
	UINT	cp;
	BYTE	charset;
} csidcpmap;

static csidcpmap csidcpmaptbl[] = {
	{CS_LATIN1, 	1252,	ANSI_CHARSET},
	{CS_SJIS, 		932,	SHIFTJIS_CHARSET},
	{CS_GB_8BIT,	936,	GB2312_CHARSET},
	{CS_BIG5, 		950,	CHINESEBIG5_CHARSET},
	{CS_KSC_8BIT,	949,	HANGEUL_CHARSET},
	{CS_CP_1251, 	1251,	RUSSIAN_CHARSET},
	{CS_CP_1250, 	1250,	EASTEUROPE_CHARSET},
	{CS_CP_1253, 	1253,	GREEK_CHARSET},
	{CS_8859_9, 	1254,	TURKISH_CHARSET},
	{0, 			0,		0}
};
/* This function do a csid to codepage conversion	*/
static UINT unicode_CsidToCodePage(int16 csid)
{
	csidcpmap *p;
	for(p = csidcpmaptbl; p->csid != 0 ; p++)
		if(csid == p->csid)
			return p->cp;
	return 0;
}
/* This function do a csid to codepage conversion	*/
static BYTE unicode_CsidToCharset(int16 csid)
{
	csidcpmap *p;
	for(p = csidcpmaptbl; p->csid != 0 ; p++)
		if(csid == p->csid)
			return p->charset;
	return 0;
}
/* This function do a csid to codepage conversion	*/
static int16 unicode_CharsetToCsid(BYTE charset)
{
	csidcpmap *p;
	for(p = csidcpmaptbl; p->csid != 0 ; p++)
		if(charset == p->charset)
			return p->csid;
	return CS_DEFAULT;
}
/*========================================================================================
//	For Standard Localized Windows
//========================================================================================
*/
typedef struct {
	BOOL	found;
	int16	testparam;
} EnumFontTest;

static int CALLBACK DetectCharset(LOGFONT FAR* lpelf, TEXTMETRIC FAR*lpntm, int FontType, LPARAM param)
{
	EnumFontTest *pTest = (EnumFontTest *)param;
	if((pTest->testparam & 0x00FF) == lpelf->lfCharSet)
	{
		pTest->found = TRUE;
		return 0;	/* Done */
	}
	else
		return 1;	/* Continue */
}
static BOOL unicode_CheckFontOfCsid(HDC hdc, int16 csid)
{
	EnumFontTest	test;
	test.found = FALSE;
	test.testparam = unicode_CsidToCharset(csid);
	EnumFontFamilies(hdc, NULL,(FONTENUMPROC)DetectCharset, (LPARAM)&test);
	return test.found;
}
#ifdef _WIN32
/*========================================================================================
//	For UnicodeFont
//========================================================================================
*/
static BOOL unicode_UnicodeFontOfCodePage(HDC hdc, UINT cp)
{
	/* How can we detect Unicode font that only use those API support in
	// Both Win95 and WinNT ?	
	*/
	return FALSE;
}
#endif
/*========================================================================================
//	For NJWIN
//========================================================================================
*/
static BOOL unicode_CheckNJWINForCsid(HDC hdc, int16 csid)
{
	/* For NJWIN
	// We should first check wheather njwin.ini is in the system directory
	// It only support System Font	
	*/
	return FALSE;
}
/*========================================================================================
//	For TwinBridge
//========================================================================================
*/
static int CALLBACK DetectTwinBridge(LOGFONT FAR* lpelf, TEXTMETRIC FAR*lpntm, int FontType, LPARAM param)
{
	EnumFontTest *pTest = (EnumFontTest *)param;
	pTest->found = TRUE;
	return 0;	/* Done	*/
}
static BOOL unicode_CheckTwinBridgeForCsid(HDC hdc, int16 csid)
{
	/*	For 4.0 
	// Chinese:
	// 	Check for twin40.ini
	// 	Check for Font "Chn System"
	// Japanese:
	// 	Check for twinj40.ini
	// 	Check for Font "Jpn System"
	*/
	switch(csid)
	{
		case CS_BIG5:
		case CS_GB_8BIT:
		case CS_SJIS:
		{
			EnumFontTest test;
			test.found = FALSE;
			EnumFontFamilies(hdc, ((csid == CS_SJIS) ? "Jpn System" : "Chn System"),
								(FONTENUMPROC)DetectTwinBridge, (LPARAM)&test);
			return test.found;
		}
		case CS_KSC_8BIT:
		default:
			return FALSE;
	}
}
/*========================================================================================
//	For Unionway
//========================================================================================
*/
static int CALLBACK DetectUnionway(LOGFONT FAR* lpelf, TEXTMETRIC FAR*lpntm, int FontType, LPARAM param)
{
	int l ;
	EnumFontTest *pTest = (EnumFontTest *)param;

	if(lpelf)	{
		char *postfix;
		switch(pTest->testparam)
		{
			case CS_SJIS:
				postfix = "JDEF";
				break;
			case CS_BIG5:
				postfix = "CDEF";
				break;
			case CS_GB_8BIT:
				postfix = "PDEF";
				break;
			case CS_KSC_8BIT:
				postfix = "UDEF";
				break;
			default:
				return 1;
		}
		l = strlen(lpelf->lfFaceName) - strlen(postfix);
		if(l < 0)
			return 1;
		if(strcmp(postfix, lpelf->lfFaceName +  l) == 0)
		{
			pTest->found = TRUE;
			return 0;
		}
	}
	return 1;
}
static BOOL unicode_CheckUnionwayForCsid(HDC hdc,int16 csid)
{
	/*
	// For Uniway, we could use EnumFontFamilies to check font name.
	// xxxUDEF	? Maybe Korean ?
	// xxxPDEF	Simplified Chinese
	// xxxJDEF	Japanese
	// xxxCDEF	Chinese 
	// Maybe we should first check wheather UWTOOLS.INI or uwfont.ini is
	// in the Windows directory	
	*/
	EnumFontTest test;
	test.found = FALSE;
	test.testparam = csid;
	EnumFontFamilies(hdc, NULL,(FONTENUMPROC)DetectUnionway, (LPARAM)&test);
	return test.found;
}

BOOL unicode_IsCsidAvailable(HDC hdc, int16 csid)
{
	/* We always sure this three csid is available. */
	if( (csid == CS_LATIN1) || (csid == CS_SYMBOL) || (csid == CS_DINGBATS))
		return TRUE;
	/* Check the charset in font */
	if(unicode_CheckFontOfCsid(hdc, csid))
		return TRUE;
	/* For 32 bit Windows, we check both code page and Unicode font also */
#ifdef _WIN32
	{
		UINT cp = unicode_CsidToCodePage(csid);
		if((cp != 0) && (IsValidCodePage(cp) || unicode_UnicodeFontOfCodePage(hdc, cp)))
			return TRUE;
	}
#endif
	/* For Windows 3.1 and 95, we check those hacking package for Multibyte system */
#ifdef _WIN32
	if((csid & MULTIBYTE) && iswin95() )
#else
	if(csid & MULTIBYTE)
#endif
	{
		if(	unicode_CheckUnionwayForCsid(hdc, csid) ||
			unicode_CheckTwinBridgeForCsid(hdc, csid)	||
			unicode_CheckNJWINForCsid(hdc, csid))
			return TRUE;
	}
	return FALSE;
}
static BOOL iswin95()
{
	DWORD dwVer;
	int iVer;
	static BOOL initialized = FALSE;
	static BOOL iswin95 = FALSE;
	if(initialized)
		return iswin95;
	// The following code are copy from styles.cpp
	dwVer = GetVersion();
	iVer = (LOBYTE(LOWORD(dwVer))*100) + HIBYTE(LOWORD(dwVer));
	if( iVer >= 395)
		iswin95 = TRUE;
	else
		iswin95 = FALSE;
	initialized = TRUE;
	return iswin95;
}
PR_PUBLIC_API(int) UNICODE_VERIFYCSIDLIST(int inNumOfItem, int16 *csidlist)
{
	int iIn, iOut;
	HDC hdc = CreateIC("DISPLAY", NULL,NULL, NULL);
	if(hdc == NULL)
		return inNumOfItem;
	for(iIn=iOut=0; iIn < inNumOfItem; iIn++)
	{
		if(unicode_IsCsidAvailable(hdc,csidlist[iIn]))
			csidlist[iOut++] = csidlist[iIn];
	}
	DeleteDC(hdc);
	return iOut;
}

PR_PUBLIC_API(int16) UNICODE_MENUFONTCSID()
{
	HDC hdc = CreateIC("DISPLAY", NULL,NULL, NULL);
	if(hdc)
	{
		TEXTMETRIC metrics;
		int16 csid;
		SelectObject(hdc, GetStockObject(SYSTEM_FONT));
		GetTextMetrics(hdc, &metrics);
		DeleteDC(hdc);
		csid = unicode_CharsetToCsid(metrics.tmCharSet);
		if(csid != CS_DEFAULT)
			return csid;
	}
	return CS_LATIN1;
}
PR_PUBLIC_API(int) UNICODE_MENUFONTID()
{
#ifdef _WIN32
	if(iswin95())
		return DEFAULT_GUI_FONT;
	else
#endif	/* _WIN32	*/
		return	SYSTEM_FONT;
}

