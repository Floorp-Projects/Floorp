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

#include "stdafx.h"
#include "mainfrm.h"
#include "libi18n.h"
#include "xplocale.h"
#include "cxprint.h"
#include "cxsave.h"

#include "np.h"
#include "prlog.h"
#include "prlink.h"
#include "xpstrsw.h"

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "java.h"
#endif

#include "cuvfs.h"
#include "intl_csi.h"
#include "edt.h"

#include "prefapi.h"
#include "edt.h"

#ifdef XP_WIN32
static BOOL intlUnicodeFlag(int16 wincsid);
#define INIT_FLAG_FOR_CSID(c) { CIntlWin::flagTable[(c) & MASK_FLAG_TABLE] = intlUnicodeFlag(c);}
BOOL CIntlWin::flagTable[MAX_FLAG_TABLE_NUM];
#endif // XP_WIN32


extern int INTL_DocCharSetID(MWContext *context);

enum {							// just to ignore unnecessary strcomp
		javaFontHelvetica = 0,
		javaFontTimesRoman,
		javaFontCourier,
		javaFontDialog,
		javaFontDialogInput,
		javaFontZapfDingbats
};

extern "C" {
	const char* IntlGetJavaFaceName(int csid, int javaFontID);
	BYTE IntlGetJavaCharset(int csid, int javaFontID);
	jref *intl_makeJavaString(int16 encoding, char *str, int len);

};

extern "C" {
PR_PUBLIC_API(BOOL ) CINTLWIN_TEXTOUT_E(int16 wincsid, HDC hdc, int nXStart, int nYStart, LPCSTR  lpString,int  iLength);
PR_PUBLIC_API(BOOL ) CINTLWIN_GETTEXTEXTENTPOINT_E(int wincsid, HDC hDC, LPCSTR lpString, int cbString, LPSIZE lpSize);

PR_PUBLIC_API(const char*) INTLGETJAVAFACENAME_E(int csid, int javaFontID);
PR_PUBLIC_API(BYTE ) INTLGETJAVACHARSET_E(int csid, int javaFontID);

#ifdef JAVA
PR_PUBLIC_API(jref* ) INTL_MAKEJAVASTRING_E(int16 encoding, char *str, int len);
#endif	// JAVA

PR_PUBLIC_API(int16* ) INTL_GETUNICODECSIDLIST_E(int16 * outnum);
PR_PUBLIC_API(int32 ) INTL_UNICODELEN_E(uint16* ustr);
PR_PUBLIC_API(INTL_CompoundStr* ) INTL_COMPOUNDSTRFROMUNICODE_E(uint16* inunicode, uint32 inlen);
PR_PUBLIC_API(void ) INTL_COMPOUNDSTRDESTROY_E(INTL_CompoundStr* This);
PR_PUBLIC_API(INTL_CompoundStrIterator ) INTL_COMPOUNDSTRFIRSTSTR_E(INTL_CompoundStr* This, INTL_Encoding_ID *outencoding, unsigned char** outtext);
PR_PUBLIC_API(INTL_CompoundStrIterator ) INTL_COMPOUNDSTRNEXTSTR_E(INTL_CompoundStrIterator iterator, INTL_Encoding_ID *outencoding, unsigned char** outtext);
PR_PUBLIC_API(INTL_CompoundStr* ) INTL_COMPOUNDSTRCLONE_E(INTL_CompoundStr* s1);
PR_PUBLIC_API(int16 ) INTL_DEFAULTWINCHARSETID_E(MWContext *context);
PR_PUBLIC_API(int32 ) INTL_TEXTBYTECOUNTTOCHARLEN_E(int16 csid, unsigned char* text, uint32 byteCount);
PR_PUBLIC_API(int32 ) INTL_TEXTCHARLENTOBYTECOUNT_E(int16 csid, unsigned char* text, uint32 charLen);
PR_PUBLIC_API(uint32 ) INTL_UNICODETOSTRLEN_E(INTL_Encoding_ID encoding,INTL_Unicode* ustr, uint32	ustrlen);
PR_PUBLIC_API(void ) INTL_UNICODETOSTR_E(INTL_Encoding_ID encoding,INTL_Unicode* ustr,uint32 ustrlen,unsigned char* dest, uint32 destbuflen);
};

//
//	Data type use for Unicode table loader
//
#define MAXUTABLENAME	16
typedef struct tblrsrcinfo {
	char	name[MAXUTABLENAME];
	uint16	refcount;
    HGLOBAL hTbl;
} tblrsrcinfo;

typedef struct utablename {
	uint16		csid;
	tblrsrcinfo	frominfo;
	tblrsrcinfo toinfo;

} utablename;
//
//	The following are private 
//
static void intl_WriteUnicodeConvList(int num_of_csid, int16* csidlist);
static void intl_ReadUnicodeConvList();
static void	intl_UnloadUCS2Table(uint16 csid, void *utblPtr, int from);
static tblrsrcinfo* intl_FindUTableName(uint16 csid, int from);
static void *intl_LoadUCS2Table(uint16 csid, int from);
static void	intl_UnloadUCS2Table(uint16 csid, void *utblPtr, int from);


// This structure is used for following table for default font value,
// It'll be removed after default value goes to resource file.
struct FontCharTable
{
 	int16 csid;
	char  szPropFont[LF_FACESIZE] ;
	int   iPropSize ;
	char  szFixFont[LF_FACESIZE] ;
	int   iFixSize ;
	int   iPropCharset;
	int   iFixCharset;
};

// Note: always add new comer to last line.
struct FontCharTable  fontchar_tbl[] =
{
	CS_LATIN1, 	DEF_PROPORTIONAL_FONT, 	12, DEF_FIXED_FONT, 10, ANSI_CHARSET, ANSI_CHARSET,
	CS_SJIS,  	"‚l‚r –¾’©", 			10, "‚l‚r –¾’©", 	10, SHIFTJIS_CHARSET,    SHIFTJIS_CHARSET,
	CS_BIG5, 	"Times New Roman", 		12, "Courier New",  10, CHINESEBIG5_CHARSET, CHINESEBIG5_CHARSET,
	CS_KSC_8BIT,"Times New Roman", 		12, "Courier New",  10, HANGEUL_CHARSET,     HANGEUL_CHARSET,
	CS_GB_8BIT, "Times New Roman", 		12, "Courier New",  10, 134, 134,
	CS_CP_1250, "Times New Roman", 		12, "Courier New",  10, 238,238,  
	CS_CP_1251, "Times New Roman", 		12, "Courier New",  10, 204,204,  
	CS_ARMSCII8, "ArmNet Helvetica",	12, "ArmNet Courier", 10, 160, 160,
	CS_CP_1253, "Times New Roman", 		12, "Courier New",  10, 161,161,  
	CS_8859_9, "Times New Roman", 		12, "Courier New",  10, 162,162,  
	CS_UTF8, 	DEF_PROPORTIONAL_FONT, 	12, DEF_FIXED_FONT, 10, DEFAULT_CHARSET, DEFAULT_CHARSET,
	CS_USER_DEFINED_ENCODING, 	DEF_PROPORTIONAL_FONT, 	12, DEF_FIXED_FONT, 10, ANSI_CHARSET, ANSI_CHARSET,
	0,			"", 					0, 	"", 			0,   0, 0
} ;

// This table defined how many encoding we supports in our Navigator and 
// what kind of conversion we support for each encoding.
// Note: always add new comer to last line.
//     Encoding, Encoding used for Display, supported CSID list, 0

unsigned int lang_table[] = 
{
	IDS_LANGUAGE_LATIN1, CS_LATIN1, CS_LATIN1, 0,
	IDS_LANGUAGE_LATIN2, CS_CP_1250, CS_CP_1250, CS_LATIN2, 0,
	IDS_LANGUAGE_JAPANESE, CS_SJIS, CS_AUTO | CS_SJIS, CS_SJIS, CS_JIS, CS_EUCJP, 0,
	IDS_LANGUAGE_TAIWANESE, CS_BIG5, CS_BIG5, CS_CNS_8BIT, 
                                CS_X_BIG5, CS_CNS11643_1110, CS_CNS11643_1, CS_CNS11643_2, 0,
	IDS_LANGUAGE_CHINESE, CS_GB_8BIT, CS_GB_8BIT, 
                              CS_GB2312, CS_GB2312_11, 0,
	IDS_LANGUAGE_KOREAN, CS_KSC_8BIT, CS_KSC_8BIT | CS_AUTO, CS_KSC_8BIT, CS_2022_KR, 
                             CS_KSC5601, CS_KSC5601_11, 0,
	IDS_LANGUAGE_WIN1251, CS_CP_1251, CS_CP_1251, CS_8859_5, CS_KOI8_R, 0,
	IDS_LANGUAGE_ARMENIAN, CS_ARMSCII8, CS_ARMSCII8, 0,
	IDS_LANGUAGE_GREEK, CS_CP_1253, CS_CP_1253, CS_8859_7, 0,
	IDS_LANGUAGE_TURKISH, CS_8859_9, CS_8859_9, 0,
	IDS_LANGUAGE_UTF8, CS_UTF8, CS_UTF8, CS_UTF7, CS_UCS2, CS_UCS2_SWAP, 0,
	IDS_LANGUAGE_USERDEFINED, CS_USER_DEFINED_ENCODING, CS_USER_DEFINED_ENCODING, 0,
	UINT_MAX
};

CIntlFont::CIntlFont()
{
	CString fontList;
	LPSTR lpszPropName = NULL;
	LPSTR lpszFixName = NULL;
	int32    iPropSize, iFixSize ;
	int    iPropCharset, iFixCharset;
	BOOL   ret = FALSE;
	int		i ;
	int		nLang ;
	EncodingInfo *pEncoding;

	iPropCharset = iFixCharset = 0;

#ifdef XP_WIN32
	for(int j = 0; j < MAX_FLAG_TABLE_NUM; j++)
		INIT_FLAG_FOR_CSID(j);
#endif

	pEncodingInfoTbl =  (struct EncodingInfo *) malloc( (MAXLANGNUM + 1) * sizeof (struct EncodingInfo) ) ;

	if (pEncodingInfoTbl == NULL)
	{
		TRACE("No memory for MultiLingual support.\n");
		return ;
	}


	memset(pEncodingInfoTbl, 0, (MAXLANGNUM + 1) * sizeof(struct EncodingInfo));

	nLang = 0;
	pEncoding = pEncodingInfoTbl;
	for (i = 0; (lang_table[i] != UINT_MAX) && (nLang < MAXLANGNUM); i++)
	{
		pEncoding->iLangResId = lang_table[i++];
		pEncoding->iCSID = lang_table[i++];

		// Setup default font information.
		for (int j = 0; j < MAXLANGNUM; j++)
		{
			if (fontchar_tbl[j].csid == pEncoding->iCSID)
			{
				// First, lets try to copy from the hard coded table
				strcpy(pEncoding->szPropName, fontchar_tbl[j].szPropFont);
				pEncoding->iPropSize = fontchar_tbl[j].iPropSize;
				strcpy(pEncoding->szFixName, fontchar_tbl[j].szFixFont);
				pEncoding->iFixSize = fontchar_tbl[j].iFixSize;
				pEncoding->iPropCharset = fontchar_tbl[j].iPropCharset;
				pEncoding->iFixCharset = fontchar_tbl[j].iFixCharset;
			}
		}

		char key[80];
		// Now, let's try to get it from the PREF
		sprintf(key, "intl.font%d.win.prop_font",	pEncoding->iCSID);
		lpszPropName = NULL;
		if( PREF_NOERROR == PREF_CopyCharPref(key,	&lpszPropName))
		{
			strcpy(pEncoding->szPropName, lpszPropName);
			XP_FREE(lpszPropName);
		}
		sprintf(key, "intl.font%d.win.fixed_font",	pEncoding->iCSID);
		lpszFixName = NULL;
		if( PREF_NOERROR ==  PREF_CopyCharPref(key, &lpszFixName))
		{
			strcpy(pEncoding->szFixName, lpszFixName);
			XP_FREE(lpszFixName);
		}

		sprintf(key, "intl.font%d.win.prop_size",	pEncoding->iCSID);
		if(  PREF_NOERROR == PREF_GetIntPref(key,	&iPropSize))
			pEncoding->iPropSize = (int)iPropSize;

		sprintf(key, "intl.font%d.win.fixed_size",	pEncoding->iCSID);
		if(  PREF_NOERROR == PREF_GetIntPref(key,	&iFixSize))
			pEncoding->iFixSize = (int)iFixSize;

		// Setup supported CSID list
		for (j =0; lang_table[i]; j++)
		{
			pEncoding->csid[j] = lang_table[i++] ;
#ifdef XP_WIN32
			INIT_FLAG_FOR_CSID(pEncoding->csid[j]);
#endif
		}

		pEncoding->nCodeset = j;

		nLang ++ ;
		pEncoding ++ ;

	}

	nEncoding = MAXLANGNUM;
	intl_ReadUnicodeConvList();
}

EncodingInfo * CIntlFont::GetEncodingInfo(int id)
{

	if (id < nEncoding)
		return &pEncodingInfoTbl[id];
	else
		return NULL;
}

EncodingInfo * CIntlFont::GetEncodingInfo(MWContext *pContext)
{
	int doc_csid, id;
	doc_csid = INTL_DocCharSetID(pContext)	& ~CS_AUTO ;
	id = DocCSIDtoID(doc_csid);

	return GetEncodingInfo(id);
}

int16 IntlCharsetToCsid(BYTE charset)
{
	int i;

	for (i = 0; i < MAXLANGNUM; i++)
	{
		EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(i);
	 	if (charset == pEncoding->iPropCharset)
			return (pEncoding->iCSID & ~CS_AUTO);
	}
	return CS_LATIN1;
}

int CIntlFont::DocCSIDtoID(int doc_csid)
{
	int i, j;
	EncodingInfo  *pEncoding ;

	pEncoding = &pEncodingInfoTbl[0];

	for (i = 0; i < nEncoding; i++)
	{
		for (j = 0; j < pEncoding->nCodeset; j++)
		{
		 	if (doc_csid == pEncoding->csid[j])
			{
				return i ;
			}
		}
		pEncoding ++ ;
	}
	// Note!!!:  return 0 here is not good idea, but it can avoid crash.
	//           need to revisit this code later.
	return 0;
}

char * CIntlFont::GetEncodingName(int id)
{
	if (id < nEncoding)
		return szLoadString(pEncodingInfoTbl[id].iLangResId);
	else	// Never should come here
		return "Unknown";
}

void  CIntlFont::WriteToIniFile()
{
	EncodingInfo *pEncoding;
	pEncoding = &pEncodingInfoTbl[0];
	for (int i = 0; i < MAXLANGNUM; i++)
	{
		char key[80];

		sprintf(key, "intl.font%d.win.prop_font",	pEncoding->iCSID);
		PREF_SetCharPref(key,	pEncoding->szPropName);

		sprintf(key, "intl.font%d.win.fixed_font",	pEncoding->iCSID);
		PREF_SetCharPref(key,	pEncoding->szFixName);

		sprintf(key, "intl.font%d.win.prop_size",	pEncoding->iCSID);
		PREF_SetIntPref(key,	(int32)pEncoding->iPropSize);

		sprintf(key, "intl.font%d.win.fixed_size",	pEncoding->iCSID);
		PREF_SetIntPref(key,	(int32)pEncoding->iFixSize);

		pEncoding ++ ;
	}
}


BYTE IntlGetLfCharset(int csid)
{
	int id;
	if (csid == 0)
		csid = INTL_DefaultWinCharSetID(0);
	csid = csid & ~CS_AUTO;  // mask off AUTO bit

	if (csid == 0 || csid == CS_LATIN1 || csid == CS_UNKNOWN)
		return ANSI_CHARSET;

	id = theApp.m_pIntlFont->DocCSIDtoID(csid);
	EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(id);
	return pEncoding->iPropCharset;
}

const char *IntlGetUIFixFaceName(int csid)
{
	int id ;
	if (csid == 0)
		csid = INTL_DefaultWinCharSetID(0);
	csid = csid & ~CS_AUTO;  // mask off AUTO bit

	id = theApp.m_pIntlFont->DocCSIDtoID(csid);
	EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(id);
	return pEncoding->szFixName;
}

const char *IntlGetUIPropFaceName(int csid)
{
	int id ;
	if (csid == 0)
		csid = INTL_DefaultWinCharSetID(0);
	csid = csid & ~CS_AUTO;  // mask off AUTO bit

	id = theApp.m_pIntlFont->DocCSIDtoID(csid);
	EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(id);
	return pEncoding->szPropName;
}


//------------------------------------------------------------
//  Function called from cross-platform code
//------------------------------------------------------------
extern "C" int FE_StrColl(const char* s1, const char* s2)
{
	return	lstrcmp(s1, s2);
}

extern "C" size_t FE_StrfTime(MWContext *context, char *result, size_t maxsize, int format,
    const struct tm *timeptr)
{
#ifdef XP_WIN16
	switch(format)
	{
	case XP_TIME_FORMAT:
		return strftime (result, maxsize, "%H:%M", timeptr);
	case XP_WEEKDAY_TIME_FORMAT:
		return strftime (result, maxsize, "%a %H:%M", timeptr);
	case XP_DATE_TIME_FORMAT:
		return strftime (result, maxsize, "%x %H:%M", timeptr);
	case XP_LONG_DATE_TIME_FORMAT:
		return strftime (result, maxsize, "%c", timeptr);
	default:
		result[0] = '\0';
		return 0;
	}
#else
	int wincsid = INTL_DefaultWinCharSetID(context);
	LANGID langid;
	langid = GetSystemDefaultLangID();

	if ((wincsid & MULTIBYTE) &&
		 sysInfo.m_bDBCS &&
	     !((GetSystemDefaultLangID() == 0x0411  &&  wincsid == CS_SJIS) ||
	       (GetSystemDefaultLangID() == 0x0412 && wincsid == CS_KSC_8BIT) ||
	       (GetSystemDefaultLangID() == 0x0812 && wincsid == CS_KSC_8BIT) ||
	       (GetSystemDefaultLangID() == 0x0804 && wincsid == CS_GB_8BIT) ||
	       (GetSystemDefaultLangID() == 0x1004 && wincsid == CS_GB_8BIT) ||
	       (GetSystemDefaultLangID() == 0x0C04 && wincsid == CS_BIG5) ||
	       (GetSystemDefaultLangID() == 0x0404 && wincsid == CS_BIG5)))
	{				// In multibyte, if system doesn't match with current win_csid
		switch(format)
		{
		case XP_TIME_FORMAT:
			return strftime (result, maxsize, "%H:%M", timeptr);
		case XP_WEEKDAY_TIME_FORMAT:
			return strftime (result, maxsize, "%a %H:%M", timeptr);
		case XP_DATE_TIME_FORMAT:
			return strftime (result, maxsize, "%x %H:%M", timeptr);
		case XP_LONG_DATE_TIME_FORMAT:
			return strftime (result, maxsize, "%c", timeptr);
		default:
			result[0] = '\0';
			return 0;
		}
	}
	else
	{
		SYSTEMTIME daytime;
		int n, m;
		daytime.wYear = 1900 + timeptr->tm_year;
		daytime.wMonth = timeptr->tm_mon + 1;
		daytime.wDayOfWeek = timeptr->tm_wday;
		daytime.wDay = timeptr->tm_mday;
		daytime.wHour = timeptr->tm_hour;
		daytime.wMinute = timeptr->tm_min;
		daytime.wSecond = timeptr->tm_sec;
		daytime.wMilliseconds = 0;

		*result = '\0';


		switch(format)
		{
		case XP_TIME_FORMAT:
			// return strftime (result, maxsize, "%X", timeptr);
			if (n = GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT | TIME_NOSECONDS | TIME_NOTIMEMARKER, &daytime, NULL, result, maxsize))
				return n;
			else
				return strftime (result, maxsize, "%H:%M", timeptr);
		case XP_WEEKDAY_TIME_FORMAT:
			// return strftime (result, maxsize, "%a %X", timeptr);
			if (GetDateFormat(LOCALE_SYSTEM_DEFAULT,0,&daytime,"ddd ", result, maxsize))
			{
				n = strlen(result);
				if (m = GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT | TIME_NOSECONDS | TIME_NOTIMEMARKER, &daytime, NULL, result+n, maxsize-n))
					return n + m;
				else
					return strftime (result, maxsize, "%a %H:%M", timeptr);
			}
			else
				return strftime (result, maxsize, "%a %H:%M", timeptr);
		case XP_DATE_TIME_FORMAT:
			// return strftime (result, maxsize, "%x %X", timeptr);
			if (GetDateFormat(LOCALE_SYSTEM_DEFAULT,DATE_SHORTDATE, &daytime,NULL, result, maxsize))
			{
				n = strlen(result);
				result[n++] = ' ';
				if (m = GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT | TIME_NOSECONDS | TIME_NOTIMEMARKER, &daytime, NULL, result+n, maxsize-n))
					return m + n;
				else
					return strftime (result, maxsize, "%x %H:%M", timeptr);
			}
			else
				return strftime (result, maxsize, "%x %H:%M", timeptr);
		case XP_LONG_DATE_TIME_FORMAT:
			// return strftime (result, maxsize, "%x %X", timeptr);
			if (GetDateFormat(LOCALE_SYSTEM_DEFAULT,DATE_LONGDATE, &daytime,NULL, result, maxsize))
			{
				n = strlen(result);
				result[n++] = ' ';
				if (m = GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT | TIME_NOTIMEMARKER, &daytime, NULL, result+n, maxsize-n))
					return m + n;
				else
					return strftime (result, maxsize, "%x %H:%M", timeptr);
			}
			else
				return strftime (result, maxsize, "%x %H:%M", timeptr);
		default:
			result[0] = '\0';
			return 0;
		}
	}
#endif
}


CXPResSwitcher g_xpResSwitcher; 

/* XP_GetString
 *
 * This one takes XP string ID (which is used mainly by libnet, libsec) 
 * and loads String from resource file (netscape.rc3).
 */
extern "C" char *XP_GetString(int id)
{
	int16  resid;
	resid = id + 7000;

 	char *buf = szLoadString(resid, &g_xpResSwitcher);
	return buf;

}

/*
  INTL_DocCharSetID and INTL_DefaultDocCharSetID getting closer and closer
  For 2.1, we need to remove one of them.
*/
int INTL_DocCharSetID(MWContext *context)
{
	int doccsid;
	if (context == NULL)
		return theApp.m_iCSID;
	else if (doccsid = INTL_GetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context)))
		return doccsid;
#ifndef MOZ_NGLAYOUT
	else if (context->type == MWContextPrint && ((CPrintCX *)((context)->fe.cx))->m_iCSID)
		return ((CPrintCX *)((context)->fe.cx))->m_iCSID;
#endif
	else if (GetFrame(context) && GetFrame(context)->m_iCSID)
		return GetFrame(context)->m_iCSID;
	else
		return theApp.m_iCSID;
}

/*
 This routine will retrieve default URL charset Id
 from conversion engine.
*/
extern int16 INTL_DefaultDocCharSetID(MWContext * context)
{
	int doccsid;
	if (context == NULL)
		return theApp.m_iCSID;
	else if (doccsid = INTL_GetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context)))
		return doccsid;
#ifndef MOZ_NGLAYOUT
	else if (context->type == MWContextPrint && ((CPrintCX *)((context)->fe.cx))->m_iCSID)
		return ((CPrintCX *)((context)->fe.cx))->m_iCSID;
#endif
	else if (context->type == MWContextSaveToDisk && ((CSaveCX*)((context)->fe.cx))->m_iCSID)
		return ((CSaveCX *)((context)->fe.cx))->m_iCSID;
	else  if (GetFrame(context) && ((CMainFrame *)GetFrame(context))->m_iCSID)
		return  ((CMainFrame *)GetFrame(context))->m_iCSID;
	else
		return theApp.m_iCSID;
}

extern "C" uint16 FE_DefaultDocCharSetID(MWContext * context)
{
	if (context == NULL)
		return theApp.m_iCSID;
#ifndef MOZ_NGLAYOUT
	else if (context->type == MWContextPrint && ((CPrintCX *)((context)->fe.cx))->m_iCSID)
		return ((CPrintCX *)((context)->fe.cx))->m_iCSID;
#endif
	else if (context->type == MWContextSaveToDisk && ((CSaveCX*)((context)->fe.cx))->m_iCSID)
		return ((CSaveCX *)((context)->fe.cx))->m_iCSID;
	else if (GetFrame(context) && ((CMainFrame *)GetFrame(context))->m_iCSID)
		return  ((CMainFrame *)GetFrame(context))->m_iCSID;
	else
		return theApp.m_iCSID;
}

/*
 This routine will relayout current document.
 It's called from xp rountine to do CharSet conversion
 based on new doc_csid found from <Meta ...> 
*/
extern void INTL_Relayout(MWContext * pContext)
{
	if(pContext == NULL || ABSTRACTCX(pContext) == NULL || ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		return;
	}

    //  Let the page load, so that it is in the cache (bug fix 51701)
    //      or will get a page that is partially "transfer interrupted"
    //      if just doing a Reload() as that part of the page is
    //      possibly already cached before the entire page is downloaded
    //      leading to lossy display.
    //  Another way to do this is to ignore the cache altogether for a
    //      more immediate effect:  Reload(NET_SUPER_RELOAD)
    //      but that is not nice to the cache, or the user if working
    //      offline, etc.
#ifdef EDITOR
    if(EDT_IS_EDITOR(pContext)){
        EDT_RefreshLayout(pContext);
    } else
#endif
        ABSTRACTCX(pContext)->NiceReload();
}


/*
See information in
Unicode Functions Supported by Windows 95
PSS ID Number: Q125671
*/
int CIntlWin::m_iConvBufSize = 0;
#ifdef XP_WIN32
LPWSTR CIntlWin::m_wConvBuf = NULL;
#endif

BOOL CIntlWin::FontSelectIgnorePitch(int16 wincsid)
{
	return ( (wincsid & MULTIBYTE)	
		);
}
BOOL CIntlWin::FontSelectIgnoreCharset(int16 wincsid)
{
	return (  UseUnicodeFontAPI(wincsid)  || 
		  ( CS_USER_DEFINED_ENCODING == wincsid)
		);
}
BOOL CIntlWin::AllocConvBuffer(int iLength)
{
#ifdef WIN32
	if (m_iConvBufSize == 0)
	{
		if (iLength < 1024)
			iLength = 1024;
		m_wConvBuf = (WCHAR *)malloc(iLength);
		m_iConvBufSize = iLength;
	}
	else
	{
		m_wConvBuf = (WCHAR *) realloc(m_wConvBuf, iLength);
		m_iConvBufSize = iLength;
	}
#endif
	return TRUE;
}

int16 CIntlWin::m_system_locale_csid = 0;

int16 CIntlWin::GetSystemLocaleCsid()
{
	if( 0 == m_system_locale_csid )
	{
#ifdef XP_WIN32
		m_system_locale_csid = CodePageToCsid(GetACP());
#else
		m_system_locale_csid = CodePageToCsid(GetKBCodePage()); // In Win3.1 We cannot call GetACP(), we can just call GetKBCodePage()
#endif
	}
	if( 0 == m_system_locale_csid )	// if still cannot find the csid, Fall back to Latin 1
	{
		m_system_locale_csid = CS_LATIN1;
	}
	return m_system_locale_csid;
}
int16 CIntlWin::CodePageToCsid(UINT cp)
{
	switch(cp)
	{
	case 1250:
	case 852:	// MS-DOS Slavic OEM in the case of Win16 GetKBCodePage 
		return CS_CP_1250;

	case 1251:
	case 855:	// IBM Cyrillic OEM in the case of Win16 GetKBCodePage 
	case 866:	// MS-DOS Russian OEM in the case of Win16 GetKBCodePage 
		return CS_CP_1251;

	case 1252:
	case 437:	// US OEM in the case of Win16 GetKBCodePage 
	case 850:	// MS-DOS Multilingual OEM in the case of Win16 GetKBCodePage 
	case 860:	// MS-DOS Portutuese OEM in the case of Win16 GetKBCodePage 
	case 861:	// MS-DOS Icelandic OEM in the case of Win16 GetKBCodePage 
	case 863:	// MS-DOS Canadian-French OEM in the case of Win16 GetKBCodePage 
	case 865:	// MS-DOS Nordic OEM in the case of Win16 GetKBCodePage 
		return CS_LATIN1;

	case 1253:
	case 737:	// Greek OEM in the case of Win16 GetKBCodePage 
	case 869:	// IBM Modern Greek OEM in the case of Win16 GetKBCodePage 
		return CS_CP_1253;

	case 1254:
	case 857:	// IBM Turkish OEM in the case of Win16 GetKBCodePage 
		return CS_CP_1254; // CS_CP_1254;

	case 1255:
		return CS_UNKNOWN; // CS_CP_1255;

	case 1256:
		return CS_UNKNOWN; // CS_CP_1256;

	case 775:	// Baltic OEM in the case of Win16 GetKBCodePage 
	case 1257:
		return CS_UNKNOWN; // CS_CP_1257;

	case 932:
		return CS_SJIS;

	case 936:
		return CS_GB_8BIT;

	case 949:
	case 1361:	// Korean (Johab) OEM in the case of Win16 GetKBCodePage 
		return CS_KSC_8BIT;

	case 950:
		return CS_BIG5;

	default:
		return CS_UNKNOWN;
	}
}
int CIntlWin::MultiByteToWideChar(int16 wincsid, LPCSTR lpMultiByte, int iLength)
{
	if((lpMultiByte == NULL) || (lpMultiByte[0] == '\0') || (iLength == 0))
		return 0;
#ifdef WIN32
	if (iLength == -1)
		iLength = strlen(lpMultiByte);
	if (((iLength+1) * 2) > m_iConvBufSize)
		AllocConvBuffer((iLength+1) * 2);

	iLength = INTL_TextToUnicode(wincsid, (unsigned char*)lpMultiByte, iLength,  m_wConvBuf, m_iConvBufSize) ;
#endif
	return iLength;
}
//
//	UseUnicodeFontAPI return
//
//	csid		Latin1/Symbol/		UTF8	MultiByte	Other Single Byte		
//				UserDefined/Digbats			J/K/SC/TC	CE/Greek/Cyr/Turk	
//	Platform
//	Win 95 NonDB 	FALSE			TRUE	TRUE		FALSE (Need test)
//	Win NT NonDB 	FALSE			TRUE	TRUE		TRUE  (Need test)
//	Win 95 DB		FALSE			TRUE	(1) 		FALSE
//	Win NT DB		FALSE			TRUE	(1)			TRUE(2)
//
//	(1) FALSE if it is equal to the system locale's csid.
//	(2) Could be true or false. But need more work for Edit control if use TRUE
//
#ifdef XP_WIN32
static BOOL intlUnicodeFlag(int16 wincsid)
{
	if(! theApp.m_bUseUnicodeFont)
		return FALSE;

	if(0 == wincsid)	// Unknown csid
		return FALSE;

	if(CS_UTF8 == wincsid)
		return TRUE;

	if(	(CS_LATIN1 == wincsid) ||	(CS_SYMBOL == wincsid) ||
		(CS_DINGBATS == wincsid) ||	(CS_USER_DEFINED_ENCODING == wincsid)) 
	{
		return FALSE;
	}

	if( wincsid & MULTIBYTE)
	{
		if(CIntlWin::GetSystemLocaleCsid() == wincsid)
			return FALSE;
		else
			return TRUE;
	}
	else	// Other SingleByte csid
	{
		if(sysInfo.m_bWinNT)
			return TRUE;
		else
			return FALSE;
	}
}
#endif


BOOL CIntlWin::UseVirtualFont()
{
	return theApp.m_bUseVirtualFont; 
}
BOOL CIntlWin::GetTextExtentPoint(int wincsid, HDC hDC, LPCSTR pString, int iLength, LPSIZE lpSize)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return FALSE;
#else
	if(0 == iLength)
	{
		lpSize->cx = 0;
		lpSize->cy = 0;
		return TRUE;
	}
	wincsid = INTL_DocToWinCharSetID(wincsid) & ~ CS_AUTO;
#ifdef XP_WIN32
	int wlen;

    if ( CIntlWin::UseUnicodeFontAPI(wincsid))
	{
		// Handle UTF8 by using multifont
		if((wincsid == CS_UTF8) && CIntlWin::UseVirtualFont())
		{
			return CIntlUnicodeVirtualFontStrategy::GetTextExtentPoint(hDC, pString, iLength, lpSize );
		}

		if(wlen = MultiByteToWideChar(wincsid, pString, iLength))
		{
			return  ::GetTextExtentPoint32W(hDC, m_wConvBuf, wlen, lpSize);
		}
	}
#endif	// XP_WIN32
	if(wincsid == CS_UTF8)
	{
		return CIntlUnicodeVirtualFontStrategy::GetTextExtentPoint(hDC, pString, iLength, lpSize );
	} 

#ifdef XP_WIN32	// The final fallback
	return::GetTextExtentPoint32(hDC, pString, iLength, lpSize);
#else
	return::GetTextExtentPoint(hDC, pString, iLength, lpSize);
#endif
#endif /* MOZ_NGLAYOUT */
}

BOOL CIntlWin::GetTextExtentPointWithCyaFont(CyaFont *theNSFont,int wincsid, HDC hDC, LPCSTR pString, int iLength, LPSIZE lpSize)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return FALSE;
#else
	if(0 == iLength)
	{
		lpSize->cx = 0;
		lpSize->cy = 0;
		return TRUE;
	}
	// wincsid = INTL_DocToWinCharSetID(wincsid) & ~ CS_AUTO;
	XP_ASSERT(((wincsid & CS_AUTO) == 0) && (wincsid != CS_JIS) && (wincsid!=CS_EUCJP));
#ifdef XP_WIN32
	int wlen;

    if ( CIntlWin::UseUnicodeFontAPI(wincsid))
	{
		// Handle UTF8 by using multifont
		if((wincsid == CS_UTF8) && CIntlWin::UseVirtualFont())
		{
			return CIntlUnicodeVirtualFontStrategy::GetTextExtentPointWithCyaFont(theNSFont,hDC, pString, iLength, lpSize );
		}

		if(wlen = MultiByteToWideChar(wincsid, pString, iLength))
		{
			return  theNSFont->MeasureTextSize(hDC,	(char *)m_wConvBuf, wlen*sizeof(*m_wConvBuf), NULL, 0, lpSize);
		}
	}
#endif	// XP_WIN32
	if(wincsid == CS_UTF8)
	{
		return CIntlUnicodeVirtualFontStrategy::GetTextExtentPointWithCyaFont(theNSFont,hDC, pString, iLength, lpSize );
	} 

	return( theNSFont->MeasureTextSize( hDC, (char *)pString, iLength, NULL, 0, lpSize) );
// #ifdef XP_WIN32	// The final fallback
//	return::GetTextExtentPoint32(hDC, pString, iLength, lpSize);
// #else
//	return::GetTextExtentPoint(hDC, pString, iLength, lpSize);
// #endif
#endif /* MOZ_NGLAYOUT */
}

CSize CIntlWin::GetTextExtent(int16 wincsid, HDC pDC, LPCTSTR pString, int iLength)
{
	CSize csSize;
	BOOL bRetval;
    bRetval = CIntlWin::GetTextExtentPoint(wincsid, pDC, pString, iLength, &csSize);
	return(csSize);
}

BOOL CIntlWin::TextOut(int16 wincsid, HDC hDC, int nXStart, int nYStart, LPCSTR  lpString,int  iLength)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return FALSE;
#else
	if(0 == iLength)
		return TRUE;
	wincsid = INTL_DocToWinCharSetID(wincsid) & ~ CS_AUTO;
#ifdef XP_WIN32
	int wlen;
    if ( CIntlWin::UseUnicodeFontAPI(wincsid))
	{
		// Handle UTF8 by using multifont
		if((wincsid == CS_UTF8)  && CIntlWin::UseVirtualFont())
		{
			return CIntlUnicodeVirtualFontStrategy::TextOut(
				hDC, nXStart, nYStart, lpString, iLength);
		}
		if(wlen = MultiByteToWideChar(wincsid, lpString, iLength))
		{
			return ::TextOutW(hDC, nXStart, nYStart, m_wConvBuf, wlen);
		}
	}
#endif // XP_WIN32
	if(wincsid == CS_UTF8)
	{
		return CIntlUnicodeVirtualFontStrategy::TextOut(hDC, nXStart, nYStart, lpString, iLength);
	}    
	return ::TextOut(hDC, nXStart, nYStart, lpString, iLength);		// The final fallback
#endif /* MOZ_NGLAYOUT */
}

//based on BOOL CIntlWin::TextOut(int16 wincsid, HDC hDC, int nXStart, int nYStart, LPCSTR  lpString,int  iLength)
BOOL CIntlWin::TextOutWithCyaFont(CyaFont *theNSFont, int16 wincsid, HDC hDC,  
								  int nXStart, int nYStart, LPCSTR  lpString,int  iLength)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return FALSE;
#else
	if(0 == iLength)
		return TRUE;
#ifdef XP_WIN32
	int wlen;
    if ( CIntlWin::UseUnicodeFontAPI(wincsid))
	{
		// Handle UTF8 by using multifont
		if((wincsid == CS_UTF8)  && CIntlWin::UseVirtualFont())
		{
			return( CIntlUnicodeVirtualFontStrategy::TextOutWithCyaFont(theNSFont, hDC, nXStart, nYStart, lpString, iLength) );
		}
		if(wlen = MultiByteToWideChar(wincsid, lpString, iLength))
		{
			// the encording flag in theNSFont makes it call ::TextOutW()
			if( theNSFont->drawText(hDC, nXStart, nYStart, (char *)m_wConvBuf, wlen * 2) == FONTERR_OK)
				return(TRUE);
			else
				return(FALSE);
			// return ::TextOutW(hDC, nXStart, nYStart, m_wConvBuf, wlen);
		}
	}
#endif // XP_WIN32
	if(wincsid == CS_UTF8)
	{
		// todo 
		return CIntlUnicodeVirtualFontStrategy::TextOut(hDC, nXStart, nYStart, lpString, iLength);
	}    
	//return ::TextOut(hDC, nXStart, nYStart, lpString, iLength);		// The final fallback
	if( theNSFont->drawText(hDC, nXStart, nYStart, (char *)lpString, iLength) == FONTERR_OK )
		return(TRUE);
	else
		return(FALSE);
#endif /* MOZ_NGLAYOUT */
}

//	*** Fix Me: Need to change to support UTF8
BOOL CIntlWin::ExtTextOut(int16 wincsid, HDC pDC, int x, int y, UINT nOptions, LPCRECT lpRect, LPCSTR lpszString, UINT nCount, LPINT lpDxWidths)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return FALSE;
#else
	if(0 == nCount)
		return TRUE;
	wincsid = INTL_DocToWinCharSetID(wincsid) & ~ CS_AUTO;
#ifdef XP_WIN32
	int wlen;
    if ( CIntlWin::UseUnicodeFontAPI(wincsid))
	{
		// Handle UTF8 by using multifont
		if((wincsid == CS_UTF8)  && CIntlWin::UseVirtualFont())
		{
			// Fix Me: For now, igore the option and rect. Just use TextOut
			// Need to implement the CIntlUnicodeVirtualFontStrategy::ExTextOut() later
			return CIntlUnicodeVirtualFontStrategy::TextOut(
				pDC, x, y, lpszString, nCount);
		}
		if(wlen = MultiByteToWideChar(wincsid, lpszString, nCount))
		{
            return ::ExtTextOutW(pDC, x, y, nOptions, lpRect, m_wConvBuf, wlen, lpDxWidths);
		}
	}
#endif
	if(wincsid == CS_UTF8)
	{
		// Fix Me: For now, igore the option and rect. Just use TextOut
		// Need to implement the CIntlUnicodeVirtualFontStrategy::ExTextOut() later
		return CIntlUnicodeVirtualFontStrategy::TextOut(
			pDC, x, y, lpszString, nCount);
	}
    return  ::ExtTextOut(pDC, x, y, nOptions, lpRect, lpszString, nCount, lpDxWidths);
#endif /* MOZ_NGLAYOUT */
}

#ifdef XP_WIN32
int CIntlWin::DrawTextEx(int16 wincsid, HDC hdc, LPSTR lpchText, int cchText,LPRECT lprc,UINT dwDTFormat,LPDRAWTEXTPARAMS lpDTParams)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return 0;
#else
	wincsid = INTL_DocToWinCharSetID(wincsid) & ~ CS_AUTO;

	int iRetval;
	int wlen;
	if (cchText == -1)
		cchText = strlen(lpchText);

    if ( CIntlWin::UseUnicodeFontAPI(wincsid))
	{

		// DrawTextExW and DrawTextW is not working on Win95 right now. See Note above
		if( (!((wincsid == CS_UTF8)  && CIntlWin::UseVirtualFont()))
			&& (sysInfo.m_bWinNT)
			&& (wlen = MultiByteToWideChar(wincsid, lpchText, cchText))
		  )
		{
			return ::DrawTextW(hdc, m_wConvBuf, wlen, lprc, dwDTFormat);
		}
		int x, y;
		SIZE sz;
		CIntlWin::GetTextExtentPoint(wincsid, hdc, lpchText, cchText, &sz);

		//	Caculate X 
		x = lprc->left;
		if(dwDTFormat & DT_CENTER)	
			x = (lprc->right + lprc->right - sz.cx) / 2;
		else if(dwDTFormat & DT_RIGHT)
			x = lprc->right - sz.cx;

		//	Caculate Y
		y = lprc->top ;
		if(dwDTFormat & DT_VCENTER)
			y = ( lprc->top + lprc->bottom - sz.cy ) / 2;
		else if(dwDTFormat & DT_BOTTOM)
			y = lprc->bottom - sz.cy;

		if(dwDTFormat & DT_CALCRECT)
		{
			lprc->right = x + sz.cx;
			return sz.cy;
		}		
		return  CIntlWin::TextOut(wincsid, hdc, x, y, lpchText, cchText);
	}
    else
	{
		if (sysInfo.m_bWinNT)
			iRetval =  ::DrawText(hdc, lpchText, cchText, lprc, dwDTFormat);
		else
			iRetval =  ::DrawTextEx(hdc, lpchText, cchText, lprc, dwDTFormat, lpDTParams);
	}
	return iRetval;
#endif /* MOZ_NGLAYOUT */
}
#endif

int CIntlWin::DrawText(int16 wincsid, HDC hdc, LPSTR lpchText, int cchText,LPRECT lprc,UINT dwDTFormat)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return 0;
#else
#ifdef _WIN32
	return CIntlWin::DrawTextEx( wincsid,  hdc,  lpchText,  cchText, lprc, dwDTFormat, NULL);
#else
	wincsid = INTL_DocToWinCharSetID(wincsid) & ~ CS_AUTO;
	if (cchText == -1)
		cchText = strlen(lpchText);

	if(wincsid == CS_UTF8)
	{
		int x, y;
		SIZE sz;
		CIntlUnicodeVirtualFontStrategy::GetTextExtentPoint(hdc, lpchText, cchText, &sz);

		//	Caculate X 
		x = lprc->left;
		if(dwDTFormat & DT_CENTER)	
			x = (lprc->right + lprc->right - sz.cx) / 2;
		else if(dwDTFormat & DT_RIGHT)
			x = lprc->right - sz.cx;

		//	Caculate Y
		y = lprc->top;
		if(dwDTFormat & DT_VCENTER)
			y = ( lprc->top + lprc->bottom - sz.cy ) / 2;
		else if(dwDTFormat & DT_BOTTOM)
			y = lprc->bottom - sz.cy;

		if(dwDTFormat & DT_CALCRECT)
		{
			lprc->right = x + sz.cx;
			return sz.cy;
		}		
		return  CIntlUnicodeVirtualFontStrategy::TextOut(hdc, x, y, lpchText, cchText);
    }
    return  ::DrawText(hdc, lpchText, cchText, lprc, dwDTFormat );
#endif
#endif /* MOZ_NGLAYOUT */
}

extern "C" void *FE_GetSingleByteTable(int16 from_csid, int16 to_csid, int resourceid)
{
	HRSRC	hrsrc;
	HGLOBAL	hRes;
	char 	szName[256];

	strcpy(szName, (const char *)INTL_CsidToCharsetNamePt(from_csid));
	strcpy(szName + strlen(szName), "_TO_");
	strcpy(szName + strlen(szName), (const char *)INTL_CsidToCharsetNamePt(to_csid));
	for (char *p = szName; *p; p++)
	{
		if (*p == '-')  	*p = '_';
	}

	hrsrc = ::FindResource(AfxGetResourceHandle(), szName, RT_RCDATA);

	XP_ASSERT(hrsrc);

	if (!hrsrc) {
		TRACE1("FE_GetSingleByteTable cannot find resource: %s\n", szName);
		return NULL;
	}

	hRes = ::LoadResource(AfxGetResourceHandle(), hrsrc);

	XP_ASSERT(hRes);

	if (!hRes) {
		TRACE1("FE_GetSingleByteTable cannot load resource: %s\n", szName);
		return NULL;		
	}

	return (void *) hRes;
}

extern "C" char *FE_LockTable(void **hres)
{
	return (LPSTR)::LockResource((HGLOBAL) hres);
}

extern "C" void FE_FreeSingleByteTable(void **hres) 
{
#ifndef XP_WIN32	
	::UnlockResource((HGLOBAL) hres);
#endif
	::FreeResource((HGLOBAL) hres);
}

/*--------------------------------------------------------------------------*/

#define MAXNUMOFCSIDLIST 64
#define UNICODECONVERSIONCHARTLIST "UnicodeConversionCharsetList"


#ifdef XP_WIN32
#define UNICODEDLL "UNI3200.DLL"
#define LIBRARYLOADOK(l)	(l != NULL)
#define UNICODE_VERIFYCSIDLIST_SYM	"UNICODE_VERIFYCSIDLIST"
#else
#define UNICODEDLL "UNI1600.DLL"
#define LIBRARYLOADOK(l)	(l > HINSTANCE_ERROR)
#define UNICODE_VERIFYCSIDLIST_SYM	"_UNICODE_VERIFYCSIDLIST"
#endif

static int intl_VerifyCsidList(int inNumOfItem, int16 *csidlist)
{
    int iRetval = 0;
    HINSTANCE hUniLib = LoadLibrary(UNICODEDLL);
    if(LIBRARYLOADOK(hUniLib)) {
        typedef int (*func)(int, int16 *);
        func VerifyProc = (func)GetProcAddress(hUniLib, UNICODE_VERIFYCSIDLIST_SYM);
        ASSERT(VerifyProc);
        if(VerifyProc) {
            iRetval = VerifyProc(inNumOfItem, csidlist);
            VerifyProc = NULL;
        }
        
        FreeLibrary(hUniLib);
        hUniLib = NULL;
    }
    return(iRetval);
}

//
//	Read and Write charset List for Unicode conversion
//
static void intl_ReadUnicodeConvList()
{
	CString charsetlist;
	int16 csidlist[MAXNUMOFCSIDLIST];
	int num_of_csid;

	// Find the charsetlist from preference
	charsetlist = theApp.GetProfileString("INTL", UNICODECONVERSIONCHARTLIST, "");

	// If we cannot find it from preference , find it from resource
	if(strlen((const char*)charsetlist) == 0)
		charsetlist = szLoadString(IDS_CHARSET_LIST_FOR_UNICODE);

	// Otherwise, it is very wrong, however, we still give it
	// CS_LATIN1 , CS_SYMBOL, and CS_DINGBATS
	if(strlen((const char*)charsetlist) == 0)
		charsetlist = 
		"iso-8859-1,x-cp1250,x-cp1251,x-cp1253,iso-8859-9,Shift_JIS,euc-kr,gb2312,big5,adobe-symbol-encoding,x-dingbats";

	for(num_of_csid=0; num_of_csid < MAXNUMOFCSIDLIST; )
	{
		int duplicate;
		int csid; 
		int i;
		int lastone = 0;
		int len = charsetlist.GetLength();
		int thislen = charsetlist.Find(','); 
		if(thislen == -1)
		{
			lastone = 1;
			thislen = len;
		}
		
		csid=INTL_CharSetNameToID((char *)(const char *)charsetlist.Left(thislen));
		// Check to see is that duplicate ?
		for(i=0, duplicate=0;i < num_of_csid; i ++)
		{
			if(csid == csidlist[i])
				duplicate = 1;
		}
		// If not duplicate, add it to the list
		if(duplicate == 0 )
			csidlist[num_of_csid++] = csid;
		
		if(lastone == 0 )
			charsetlist = charsetlist.Right(len - thislen - 1);
		else
			break;
	}  
	// Write it to the Prefs
	intl_WriteUnicodeConvList(num_of_csid, csidlist);

	// Filter through our csid checking code - intl_VerifyCsidList()
	num_of_csid = intl_VerifyCsidList(num_of_csid, csidlist);

	// OK, now we have the list. Call the xp function to initialize 
	// Unicode Converter
	INTL_SetUnicodeCSIDList(num_of_csid, csidlist);	

}
static void intl_WriteUnicodeConvList(int num_of_csid, int16* csidlist)
{
	char str[512];
	int first, i;
	char * cur;
	// Now, convert those csid to string
	for(first=1,i=0,cur=str;i < num_of_csid;i++)
	{
		if(first)
			first = 0;
		else
			*cur++ = ',';
		INTL_CharSetIDToName(csidlist[i], cur);
		cur += strlen(cur);
	}
	// And write to the Profile
	theApp.WriteProfileString("INTL", UNICODECONVERSIONCHARTLIST, str);
}


extern "C" {

BYTE IntlGetJavaCharset(int csid, int javaFontID)
{
	// currently, we ignore javaFontID. We may need to use it later
	switch(csid)
	{
		case CS_DINGBATS:
		case CS_SYMBOL:
			return SYMBOL_CHARSET;
		default:
			return IntlGetLfCharset(csid);
	}
}
const char* IntlGetJavaFaceName(int csid, int javaFontID)
{
	switch(csid)
	{
		case CS_DINGBATS:
			return "Wingdings";
		case CS_SYMBOL:
			return "Symbol";
		default:
			switch(javaFontID)
			{
				case javaFontTimesRoman:
				case javaFontDialog:
				case javaFontHelvetica:
					return IntlGetUIPropFaceName(csid);
				case javaFontDialogInput:
				case javaFontCourier:
					return IntlGetUIFixFaceName(csid);
				default:
					return IntlGetUIPropFaceName(csid);
			}	
	}
}
};
extern "C" {

PR_PUBLIC_API(const char* )
	INTLGETJAVAFACENAME_E(int csid, int javaFontID)
		{ return IntlGetJavaFaceName(csid, javaFontID); }
PR_PUBLIC_API(BYTE )
	INTLGETJAVACHARSET_E(int csid, int javaFontID)
		{ return IntlGetJavaCharset(csid, javaFontID); }
PR_PUBLIC_API(int16* )
	INTL_GETUNICODECSIDLIST_E(int16 * outnum)
		{ return INTL_GetUnicodeCSIDList(outnum); }
PR_PUBLIC_API(int32 )
	INTL_UNICODELEN_E(uint16* ustr)
		{ return INTL_UnicodeLen(ustr); } 
PR_PUBLIC_API(INTL_CompoundStr* )
	INTL_COMPOUNDSTRFROMUNICODE_E(uint16* inunicode, uint32 inlen)
		{ return INTL_CompoundStrFromUnicode(inunicode, inlen); }
PR_PUBLIC_API(void )
	INTL_COMPOUNDSTRDESTROY_E(INTL_CompoundStr* This)
		{ INTL_CompoundStrDestroy(This); }
PR_PUBLIC_API(INTL_CompoundStrIterator )
	INTL_COMPOUNDSTRFIRSTSTR_E(INTL_CompoundStr* This, INTL_Encoding_ID *outencoding, unsigned char** outtext)
		{ return INTL_CompoundStrFirstStr(This, outencoding,outtext); }
PR_PUBLIC_API(INTL_CompoundStrIterator )
	INTL_COMPOUNDSTRNEXTSTR_E(INTL_CompoundStrIterator iterator, INTL_Encoding_ID *outencoding, unsigned char** outtext)
		{ return INTL_CompoundStrNextStr(iterator, outencoding, outtext); }
PR_PUBLIC_API(INTL_CompoundStr* ) 		
	INTL_COMPOUNDSTRCLONE_E(INTL_CompoundStr* s1)
		{ return INTL_CompoundStrClone(s1); }
PR_PUBLIC_API(int16 )
	INTL_DEFAULTWINCHARSETID_E(MWContext *context)
		{ return INTL_DefaultWinCharSetID(context); }
PR_PUBLIC_API(int32 )
	INTL_TEXTBYTECOUNTTOCHARLEN_E(int16 csid, unsigned char* text, uint32 byteCount)
		{ return INTL_TextByteCountToCharLen(csid, text, byteCount); }
PR_PUBLIC_API(int32 )
	INTL_TEXTCHARLENTOBYTECOUNT_E(int16 csid, unsigned char* text, uint32 charLen)
		{ return INTL_TextCharLenToByteCount(csid, text,  charLen); }
PR_PUBLIC_API(uint32 )
	INTL_UNICODETOSTRLEN_E(INTL_Encoding_ID encoding,INTL_Unicode* ustr, uint32	ustrlen)
		{ return INTL_UnicodeToStrLen(encoding,ustr,ustrlen); }
PR_PUBLIC_API(void )
	INTL_UNICODETOSTR_E(INTL_Encoding_ID encoding,INTL_Unicode* ustr,uint32 ustrlen,unsigned char* dest, uint32 destbuflen)
		{ INTL_UnicodeToStr(encoding,ustr,ustrlen,dest,destbuflen); }

#ifdef JAVA
PR_PUBLIC_API(jref * )
	INTL_MAKEJAVASTRING_E(int16 encoding, char *str, int len)
		{ return intl_makeJavaString(encoding, str, len); }
#endif	// JAVA

PR_PUBLIC_API(BOOL )
	CINTLWIN_TEXTOUT_E(int16 wincsid, HDC hdc, int nXStart, int nYStart, LPCSTR  lpString,int  iLength)
		{ return CIntlWin::TextOut( wincsid,  hdc,  nXStart,  nYStart,   lpString,  iLength); }
PR_PUBLIC_API(BOOL )
	CINTLWIN_GETTEXTEXTENTPOINT_E(int wincsid, HDC hDC, LPCSTR lpString, int cbString, LPSIZE lpSize)
		{ return CIntlWin::GetTextExtentPoint( wincsid,  hDC,  lpString,  cbString,  lpSize); }

};	// extern "C"
