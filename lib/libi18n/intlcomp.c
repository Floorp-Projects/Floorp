/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*	intlcomp.c	*/
/*
	This file implement 
		INTL_MatchOneChar
		INTL_MatchOneCaseChar
		INTL_Strstr
		INTL_Strcasestr
*/
#include "intlpriv.h"
#include "pintlcmp.h"
#if defined(XP_MAC)
#include <TextUtils.h>
#endif

#define CHECK_CSID_AND_ASSERT(csid)	\
{ \
	XP_ASSERT(CS_UNKNOWN != (csid));	/*   Please don't pass in CS_UNKNOWN here, you need to know the csid  */	\
	XP_ASSERT(CS_DEFAULT != (csid));	/*   Please don't pass in CS_DEFAULT here, you need to know the csid  */	\
	XP_ASSERT(CS_ASCII != (csid));	/*   Please don't pass in CS_ASCII here, you need to know the csid  */		\
}
/*	Private Function Prototype */
extern unsigned char lower_lookup_ascii[];
#define INTL_SingleByteToLower(lower, ch)	((ch & 0x80) ? (lower[(ch & 0x7f)]) : (lower_lookup_ascii[ch]))
MODULE_PRIVATE void INTL_DoubleByteToLower(DoubleByteToLowerMap *, unsigned char* , unsigned char* );


PRIVATE void intl_strip_CRLF(unsigned char* str)
{
	unsigned char* in;
	unsigned char* out;
	for(in = out = str; 0 != *in; in++)
	{
		if((CR != *in) && (LF !=  *in))
			*out++ = *in;
	}
	*out = 0;
}
/*
	Function intl_caseless_normalize
	This function have side effect to modify the string it got. 
	It will normalize the string in a caseless matter
*/
PRIVATE void intl_caseless_normalize(int16 csid, unsigned char* str)
{
	unsigned char *sb_tolowermap = INTL_GetSingleByteToLowerMap(csid);
	unsigned char *p;

	CHECK_CSID_AND_ASSERT(csid);	
	XP_ASSERT(NULL != str);

	intl_strip_CRLF(str);

	if(SINGLEBYTE == INTL_CharSetType(csid)) {
		/*	for singlebyte csid */
		for(p = str; *p != 0 ; p++)
			*p = INTL_SingleByteToLower(sb_tolowermap, *p);
		return;
	}
	else
	{
		/*	for multibyte csid */
		DoubleByteToLowerMap *db_tolowermap = INTL_GetDoubleByteToLowerMap(csid);
		unsigned char *p;
		int l;
		for(p = str; *p != 0; p += l)
		{
			l = INTL_CharLen(csid ,p);	/* *** FIX ME: IMPROVE PERFORMANCE */
			switch(l)
			{
			case 1:
				*p = INTL_SingleByteToLower(sb_tolowermap, *p);
				break;
			case 2:
				if(0 == *(p+1))
				{
					/* Check weather we hit partial characters. This happen when we use wrong csid */
					/* However, we should not pass array bondary even we use wrong csid */
					XP_ASSERT(FALSE);
					return;	/* get partial characters, return */
				}
				INTL_DoubleByteToLower(db_tolowermap, p, p);
				break;
			default:
				{
					unsigned char *ck;
					/* Check weather we hit partial characters. This happen when we use wrong csid */
					/* However, we should not pass array bondary even we use wrong csid */
					for(ck = p+l-1; ck != p ;ck--)
					{
						if(0 == *ck)
						{
							XP_ASSERT(FALSE);
							return;	/* get partial characters, return */
						}
					}
					/* We current do not handle 3 byte normalization. We need  to work on this for UTF8 */
				}
				break;
			}

		}
	}
}



PRIVATE void INTL_DoubleByteToLower(DoubleByteToLowerMap *db_tolowermap, unsigned char* lowertext, unsigned char* text)
{
	DoubleByteToLowerMap *p;
	for(p = db_tolowermap; !((p->src_b1 == 0) && (p->src_b2_start == 0)); p++)
	{
		if( (p->src_b1       == text[0]) &&
			(p->src_b2_start <= text[1] ) &&
			(p->src_b2_end   >= text[1]) )
		{
			lowertext[0] = p->dest_b1;
			lowertext[1] = text[1] - p->src_b2_start + p->dest_b2_start;
			return;
		}
		else 
		{	/*	The map have to be sorted order to implement a fast search */
			if(p->src_b1 > text[0])
				break;
			else {
				if((p->src_b1 == text[0]) && (p->src_b2_start > text[1]))
					break;
			}
		}
	}
	lowertext[0] = text[0];
	lowertext[1] = text[1];
	return;
}

PUBLIC XP_Bool INTL_MatchOneChar(int16 csid, unsigned char *text1,unsigned char *text2,int *charlen)
{
	if((INTL_CharSetType(csid) == SINGLEBYTE) ) {
		unsigned char *sb_tolowermap;
		*charlen = 1;
		sb_tolowermap = INTL_GetSingleByteToLowerMap(csid);
		return( INTL_SingleByteToLower(sb_tolowermap,text1[0]) == INTL_SingleByteToLower(sb_tolowermap, text2[0]));
	}
	else
	{
		int l1, l2;
		l1 = INTL_CharLen(csid ,text1);	/* *** FIX ME: IMPROVE PERFORMANCE */
		l2 = INTL_CharLen(csid ,text2);	/* *** FIX ME: IMPROVE PERFORMANCE */
		if(l1 != l2)
			return FALSE;
		if(l1 == 1)
		{
			unsigned char *sb_tolowermap;
			*charlen = 1;
			sb_tolowermap = INTL_GetSingleByteToLowerMap(csid);
			return( INTL_SingleByteToLower(sb_tolowermap,text1[0]) == INTL_SingleByteToLower(sb_tolowermap, text2[0]));
		}
		else
		{
			if(l1 == 2)
			{
				DoubleByteToLowerMap *db_tolowermap;
				unsigned char lowertext1[2], lowertext2[2];
				*charlen = 2;
				db_tolowermap = INTL_GetDoubleByteToLowerMap(csid);
				INTL_DoubleByteToLower(db_tolowermap, lowertext1, text1);
				INTL_DoubleByteToLower(db_tolowermap, lowertext2, text2);
				return( ( lowertext1[0] ==  lowertext2[0] ) &&
						( lowertext1[1] ==  lowertext2[1] ) );
			}
			else
			{
				/* for character which is neither one byte nor two byte, we cannot ignore case for them */
				int i;
				*charlen = l1;
				for(i=0;i<l1;i++)
				{
					if(text1[i] != text2[i])
						return FALSE;
				}
				return TRUE;
			}
		}
	}
}
PUBLIC XP_Bool INTL_MatchOneCaseChar(int16 csid, unsigned char *text1,unsigned char *text2,int *charlen)
{
	if((INTL_CharSetType(csid) == SINGLEBYTE) ) {
		*charlen = 1;
		return( text1[0]== text2[0]);
	}
	else
	{
		int i,len;
		*charlen = len  = INTL_CharLen(csid, (unsigned char *) text1);	/* *** FIX ME: IMPROVE PERFORMANCE */
		for(i=0 ; i < len; i++)
		{
			if(text1[i] != text2[i]) 
				return FALSE;
		}
		return TRUE;
	}
}
PUBLIC
char *INTL_Strstr(int16 csid, const char *s1,const char *s2)
{
	int len;
	char *p1, *pp1, *p2;
	if((s2==NULL) || (*s2 == '\0'))
		return (char *)s1;
	if((s1==NULL) || (*s1 == '\0'))
		return NULL;
	
	for(p1=(char*)s1; *p1  ;p1 = INTL_NextChar(csid ,p1))	/* *** FIX ME: IMPROVE PERFORMANCE */
	{
		for(p2=(char*)s2, pp1=p1 ;  
			((*pp1) && (*p2) && INTL_MatchOneCaseChar(csid, (unsigned char*)pp1, (unsigned char*)p2, &len)); 
			pp1 += len, p2 += len)	/* *** FIX ME: IMPROVE PERFORMANCE */
				;	/* do nothing in the loop */
		if(*p2 == '\0')
			return p1;
	}
	return NULL;
}

/*
To Do:
	We should take advantage of INTL_GetNormalizeStr to improve the performance of this
*/
PUBLIC
char *INTL_Strcasestr(int16 csid, const char *s1, const char *s2)
{
	int len;
	char *p1, *pp1, *p2;
	if((s2==NULL) || (*s2 == '\0'))
		return (char *)s1;
	if((s1==NULL) || (*s1 == '\0'))
		return NULL;

	for(p1=(char*)s1; *p1  ;p1 = INTL_NextChar(csid , p1))	/* *** FIX ME: IMPROVE PERFORMANCE */
	{
		for(p2=(char*)s2, pp1=p1 ;  
			((*pp1) && (*p2) && INTL_MatchOneChar(csid, (unsigned char*)pp1, (unsigned char*)p2, &len)); 
			pp1 += len, p2 += len)	/* *** FIX ME: IMPROVE PERFORMANCE */
				;	/* do nothing in the loop */
		if(*p2 == '\0')
			return p1;
	}
	return NULL;
}

PUBLIC unsigned char* INTL_GetNormalizeStr(int16 csid, unsigned char* str)
{
	char* n_str = NULL;
	StrAllocCopy(n_str, (char*) str);
	XP_ASSERT(n_str);	/* Should only come here if Memory Not Enough */

	CHECK_CSID_AND_ASSERT(csid);	

	if(NULL != n_str)
		intl_caseless_normalize(csid, (unsigned char*)n_str);
	return (unsigned char*)n_str;
}

#ifdef MOZ_MAIL_NEWS

PUBLIC unsigned char* INTL_GetNormalizeStrFromRFC1522(int16 csid, unsigned char* rfc1522header)
{
	char* n_header = (char*) INTL_DecodeMimePartIIStr((char*)rfc1522header, csid, FALSE);
	if(NULL == n_header)	/* INTL_DecodeMimePartIIStr() may return NULL- Mean no conversion */
		StrAllocCopy(n_header, (char*) rfc1522header);
	XP_ASSERT(n_header);	/* Should only come here if Memory Not Enough */

	CHECK_CSID_AND_ASSERT(csid);	

	if(NULL != n_header)
		intl_caseless_normalize(csid, (unsigned char*)n_header);
	return (unsigned char*)n_header;
}

#endif  /* MOZ_MAIL_NEWS */

PUBLIC XP_Bool	INTL_StrContains(	
	int16 strcsid, unsigned char* normalizedStr, unsigned char* normalizedSubstr)
{
	/* 
		It is the caller's responsibility to make sure the normalizedstr1 and normalizedstr2 
		are normalized by calling 
		INTL_GetNormalizeStr() or
		INTL_GetNormalizeStrFromRFC1522()
	*/
	char* p;
	int l_char;
	int l_idx;
	int l_substr = XP_STRLEN((char*) normalizedSubstr);
	int l_str = XP_STRLEN((char*) normalizedStr);

	CHECK_CSID_AND_ASSERT(strcsid);	

	for(p = (char*)normalizedStr, l_idx = 0, l_char = 0; (0 != *p) && (l_idx < l_str) ; p += l_char, l_idx += l_char)
	{
		l_char = INTL_CharLen(strcsid, (unsigned char*)p);	/* *** FIX ME: Should do better tune for performance here */
		if(0 == XP_STRNCMP(p, (char*) normalizedSubstr, l_substr))
			return TRUE;
	}
	return FALSE;
}
PUBLIC XP_Bool	INTL_StrIs(		
	int16 strcsid, unsigned char* normalizedStr, unsigned char* normalizedSubstr)
{
	/* 
		It is the caller's responsibility to make sure the normalizedstr1 and normalizedstr2 
		are normalized by calling 
		INTL_GetNormalizeStr() or
		INTL_GetNormalizeStrFromRFC1522()
	*/

	CHECK_CSID_AND_ASSERT(strcsid);	

	return (0 == XP_STRCMP((char*) normalizedStr, (char*) normalizedSubstr));
}
PUBLIC XP_Bool	INTL_StrBeginWith(	
	int16 strcsid, unsigned char* normalizedStr, unsigned char* normalizedSubstr)
{
	/* 
		It is the caller's responsibility to make sure the normalizedstr1 and normalizedstr2 
		are normalized by calling 
		INTL_GetNormalizeStr() or
		INTL_GetNormalizeStrFromRFC1522()
	*/

	CHECK_CSID_AND_ASSERT(strcsid);	

	return (0 == XP_STRNCMP((char*) normalizedStr, (char*) normalizedSubstr, XP_STRLEN((char*) normalizedSubstr)));
}

PUBLIC XP_Bool	INTL_StrEndWith(	
	int16 strcsid, unsigned char* normalizedStr, unsigned char* normalizedSubstr)
{
	/* 
		It is the caller's responsibility to make sure the normalizedstr1 and normalizedstr2 
		are normalized by calling 
		INTL_GetNormalizeStr() or
		INTL_GetNormalizeStrFromRFC1522()
	*/
	char* p;
	int l_char;
	int l_idx;
	int l_substr = XP_STRLEN((char*) normalizedSubstr);
	int l_str = XP_STRLEN((char*) normalizedStr);
	int l_stop = l_str - l_substr;

	CHECK_CSID_AND_ASSERT(strcsid);	

	for(p = (char*)normalizedStr, l_idx = 0, l_char = 0; (0 != *p) && (l_idx < l_stop) ; p += l_char, l_idx += l_char)
		l_char = INTL_CharLen(strcsid, (unsigned char*)p);	/* *** FIX ME: Should do better tune for performance here */
	if(l_idx != l_stop)
		return FALSE;
	return (0 == XP_STRCMP(p, (char*) normalizedSubstr));
}


#ifdef MOZ_MAIL_NEWS


#if defined(XP_WIN32)
/* 
 * Set locale to the system default locale. 
 */
static char *system_default_locale_string = NULL;
static void SetLocaleToSystemDefaultLocale(void)
{
	char *str;

	if (system_default_locale_string == NULL)
	{
		if (system_default_locale_string = (char *) malloc(128))
			GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SENGLANGUAGE, system_default_locale_string, 128);
	}
	str = setlocale(LC_COLLATE, system_default_locale_string);
}
#endif /* XP_WIN32 */

#if defined(XP_WIN32)
static char *FEINTL_CreateCollationKeyUsingOS(const char *in_string, int16 wincsid)
{
	char *out_string = NULL;
	size_t tmp_len;
	
	/* Currently only supports Latin1. */
	if (wincsid != CS_ASCII && wincsid != CS_LATIN1 && wincsid != CS_DEFAULT)
		return NULL;

	/* Prepare a larger buffer for a sort key. */
	tmp_len = XP_STRLEN(in_string)*4;

	/* Set up the system defualt locale */
	SetLocaleToSystemDefaultLocale();

	if (out_string = (unsigned char *) XP_ALLOC(tmp_len))
	{
		size_t rt;
		*out_string = '\0';
		rt = strxfrm(out_string, in_string, tmp_len);

		/* Realloc the buffer if it's smaller and try again. */
		if (rt > tmp_len)
		{
			tmp_len = rt;
			if (out_string = (unsigned char *) XP_REALLOC((void *) out_string, tmp_len))
				rt = strxfrm(out_string, in_string, tmp_len);
		}

		/* Return the output of strxfrm if success. */
		if (rt != (size_t)-1 && *out_string)
			out_string = out_string;
		else
			XP_FREEIF(out_string);
	}

	return out_string;
}
#endif /* XP_WIN32 */
#if defined(XP_MAC)
static char *FEINTL_CreateCollationKeyUsingOS(const char *in_string, int16 wincsid)
{
	char *out_string = NULL;
	char *temp_string;
	int in_string_len = XP_STRLEN(in_string);
	int i;
	
	/* Currently only supports Latin1. */
	if (wincsid != CS_ASCII && wincsid != CS_LATIN1 && wincsid != CS_DEFAULT)
		return NULL;

	/* INTL_ConvertLineWithoutAutoDetect may alter input string. */
	temp_string = XP_STRDUP(in_string);
	if (temp_string != NULL)
	{
		/* Convert to MacRoman. */
		out_string = (char *) INTL_ConvertLineWithoutAutoDetect (wincsid, CS_MAC_ROMAN, (unsigned char *) temp_string, in_string_len);
		/* Set the converted string if conversion was applied and the input string was not altered. */
		if (out_string != NULL && out_string != temp_string)
		{
			XP_FREE(temp_string);
			temp_string = out_string;
		}
		out_string = (char *) XP_ALLOC(in_string_len * 2 + 1);
		if (out_string != NULL)
		{
			/* Copy original string. */
			for (i = 0; i < in_string_len; i++)
				out_string[i*2+1] = temp_string[i];
			UppercaseStripDiacritics(temp_string, in_string_len, FontToScript(1));
			/* Copy uppercased string. */
			for (i = 0; i < in_string_len; i++)
				out_string[i*2] = temp_string[i];
			/* Terminate the string. */
			out_string[in_string_len * 2] = '\0';
		}
		XP_FREE(temp_string);
	}

	return out_string;
}
#endif /* XP_MAC */
#if defined(XP_UNIX)
static char *FEINTL_CreateCollationKeyUsingOS(const char *in_string, int16 wincsid)
{
	return NULL;
}
#endif /* XP_UNIX */

static char *INTL_CreateCollationKeyUsingOS(const char *in_string, int16 wincsid)
{
	char *out_string;
	unsigned char *tmp;

	/* Create a collatable string */
	if (INTL_CharSetType(wincsid) == SINGLEBYTE)
	{
		/* Front End call to create a collation key. */
		out_string = FEINTL_CreateCollationKeyUsingOS(in_string, wincsid);
		if (out_string)
			return out_string;

		/* Otherwise just lowercase the string. */
		out_string = XP_STRDUP(in_string);
		tmp = (unsigned char *) out_string;
		while (*tmp)
		{
			*tmp = (unsigned char) XP_TO_LOWER((int)*tmp);
			tmp++;
		}
	}
	else if (wincsid & MULTIBYTE)
	{
		out_string = XP_STRDUP(in_string);
		tmp = (unsigned char *) out_string;
		while (*tmp)
		{
			/* Lower case for Ascii */
			if (*tmp < 128)
			{
				*tmp = (unsigned char) XP_TO_LOWER((int)*tmp);
				tmp++;
			}
			else
			{
				int bytes = INTL_CharLen(wincsid, tmp);
				/* ShiftJIS specific, shift hankaku kana in front of zenkaku. */
				if (wincsid == CS_SJIS)
				{
					if (*tmp >= 0xA0 && *tmp < 0xE0)
					{
						*tmp -= (0xA0 - 0x81);
					}
					else if (*tmp >= 0x81 && *tmp < 0xA0)
					{
						*tmp += (0xA0 - 0x81);
					} 
				}
				tmp += bytes;
			}
		}
	}

	return out_string;
}

#if defined(LIBNLS_COLLATE)
static Collation	*collation = NULL;
static char *INTL_CreateCollationKeyUsingLibNLS(const char *in_string, int16 wincsid)
{
	return NULL;
}
#endif /* LIBNLS_COLLATE */

/*
 * Create a collation key using default system locale. 
 */
PUBLIC char *INTL_CreateCollationKeyByDefaultLocale(const char *in_string, int16 wincsid, int32 collation_flag)
{
	char *out_string;

	/* For future enhancement */
	collation_flag = 0;

	/* CS_DEFAULT is not accepted by i18n unicode converter. 
	 * In future, this should be taken care by the caller.
	 */
	if (CS_DEFAULT == wincsid)
		wincsid = CS_LATIN1;

	/* Create a collation key. */
#if defined(LIBNLS_COLLATE)
	out_string = INTL_CreateCollationKeyUsingLibNLS(in_string);
#else
	out_string = INTL_CreateCollationKeyUsingOS(in_string, wincsid);
#endif

	return out_string;
}

/*
 * Compare two collation keys. 
 */
PUBLIC int INTL_Compare_CollationKey(const char *key1, const char *key2)
{
	return XP_MEMCMP((const void *) key1, (const void *) key2, XP_STRLEN(key1));
}

/*
 * Decode, convert and create a message header. Then create and return a collatable string. 
 */
PUBLIC char *INTL_DecodeMimePartIIAndCreateCollationKey(const char *header, int16 wincsid, int32 collation_flag) 
{
	char *temp_string;
	char *decoded_string;
	char *out_string;

	/* For future enhancement */
	collation_flag = 0;

	/* Allocate the temp string because INTL_DecodeMimePartIIStr may alter input string. */
	temp_string = XP_STRDUP(header);
	if (temp_string == NULL)
		return NULL;

	/* Decode and Convert */
	decoded_string = INTL_DecodeMimePartIIStr(temp_string, wincsid, FALSE);

	/* Free the temp string. */
	if (decoded_string != temp_string)
		XP_FREE(temp_string);
	/* No decode or conversion done, allocate for out string. */
	if (decoded_string == NULL)
		decoded_string = XP_STRDUP(header);
	if (decoded_string == NULL)
		return NULL;

	/* Create a collation key. */
	out_string = INTL_CreateCollationKeyByDefaultLocale(decoded_string, wincsid, collation_flag);

	/* Return decoded string in case no collation key created. */
	if (out_string != NULL)
		XP_FREE(decoded_string);
	else
		out_string = decoded_string;


    return out_string; 
} 

#endif  /* MOZ_MAIL_NEWS */



