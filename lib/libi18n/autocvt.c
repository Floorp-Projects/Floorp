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
/*	autodetect.c	*/

/*
 * CODESET	1st Byte		2nd Byte	3rd Byte
 * JIS		0x21-0x7E		0x21-0x7E	n/a
 * SJIS		0xA1-0xDF		n/a			n/a
 *			0x81-0x9F		0x40-0xFC	n/a
 *			0xE0-0xEF		0x40-0xFC	n/a
 * EUCJP	0x8E (SS2)		0xA1-0xDF	n/a
 *			0xA1-0xFE		0xA1-0xFE	n/a
 *			0x8F (SS3)		0xA1-0xFE	0xA1-0xFE
 * Invalid	7F,80,A0,FF
 */
			
#include "intlpriv.h"

#define ALLOW_NBSP 1

/*
 *      JIS X 0201-Roman            ESC ( J
 *      Half-width Katakana         ESC ( I
 *      JIS X 0208-1978             ESC $ @
 *      JIS X 0208-1983             ESC $ B
 *      JIS X 0212-1990             ESC $ ( D
 */
#define IsJISEscSeq(cp, len)												\
	((cp[0] == ESC) && ((len) > 2) && (										\
		((cp[1] == '$') && (cp[2] == 'B')) ||								\
		((cp[1] == '$') && (cp[2] == '@')) ||								\
		((cp[1] == '(') && (cp[2] == 'J')) ||								\
		((cp[1] == '(') && (cp[2] == 'I')) ||								\
		(((len) > 3) && (cp[1] == '$') && (cp[2] == '(') && (cp[3] == 'D')) ) )

#define IsRoman(c)			((c) < 0x80)
#define IsSJIS2ndByte(c)	(((c) > 0x3F) && ((c) < 0xFD))
#define IsLoSJIS2ndByte(c)	(((c) > 0x3F) && ((c) < 0xA1))
#define IsHiSJIS2ndByte(c)	(((c) > 0xA0) && ((c) < 0xFD))
#define IsEUCJPKana(b1)		(((b1) > 0xA0) && ((b1) < 0xE0))
#define IsEUCJPKanji(b1or2)	(((b1or2) > 0xA0) && ((b1or2) < 0xFF))

#define	YES		1
#define NO		0
#define	MAYBE	-1

PRIVATE int
isSJIS(const unsigned char *cp, int32 len)
{
	while (len) {
		if (IsRoman(*cp)) {
			cp++, len--;
		} else if (*cp == 0x80) {		/* illegal SJIS 1st byte			*/
			return NO;
		} else if ((*cp < 0xA0)) {		/* byte 1 of 2byte SJIS 1st range	*/
			if (len > 1) {
				if (IsSJIS2ndByte(cp[1])) {
					if ((*cp != 0x8E && *cp != 0x8F) || (*(cp+1) <= 0xA0))
						return YES;
					cp += 2, len -= 2;	/* valid 2 byte SJIS				*/
				} else {
					return NO;			/* invalid SJIS	2nd byte			*/
				}
			} else
				break;						/* buffer ended w/1of2 byte SJIS */
		} else if (*cp == 0xA0) {			/* illegal EUCJP byte		*/
#if ALLOW_NBSP
			cp++, len--; /* allow nbsp */
#endif
		} else if (*cp < 0xE0) {		/* SJIS half-width kana				*/
			cp++, len--;
		} else if (*cp < 0xF0) {		/* byte 1 of 2byte SJIS	 2nd range	*/
			if (len > 1) {
				if (IsSJIS2ndByte(cp[1])) {
					cp += 2, len -= 2;	/* valid 2 byte SJIS				*/
				} else {
					return NO;			/* invalid SJIS						*/
				}
			} else
				break;					/* buffer ended w/1of2 byte SJIS	*/
		} else {
			return NO;					/* invalid SJIS 1st byte			*/
		}
	}
	return MAYBE;						/* No illegal SJIS values found		*/
}

PRIVATE int
isEUCJP(const unsigned char *cp, int32 len)
{
	while (len) {
		if (IsRoman(*cp)) {			/* Roman						*/
			cp++, len--;
		} else if (*cp == SS2) {		/* EUCJP JIS201 half-width kana */
			if (len > 1) {
				if (IsEUCJPKana(cp[1]))
					cp += 2, len -= 2;		/* valid half-width kana */
				else
					return NO;				/* invalid 2of3 byte EUC */ 
			} else
				break;						/* buffer ended w/1of2 byte EUC	*/
		} else if (*cp == SS3) {			/* EUCJP JIS212					*/
			 if (len > 1) {
			 	if (IsEUCJPKanji(cp[1])) {
			 		if (len > 2) {
				 		if (IsEUCJPKanji(cp[2]))
							cp += 2, len -= 2;	/* valid 3 byte EUCJP		*/
						else
							return NO;		/* invalid 3of3 byte EUCJP	*/
					} else
						break;				/* buffer ended w/2of3 byte EUCJP */
				} else
					return NO;				/* invalid 2of3 byte EUCJP	*/
			} else
				break;						/* buffer ended w/1of3 byte EUCJP */
		} else if (*cp == 0xA0) {			/* illegal EUCJP byte		*/
#if ALLOW_NBSP
			cp++, len--; /* allow nbsp */
#else
			return NO;
#endif
		} else if (*cp < 0xF0) {		/* EUCJP JIS208 (overlaps SJIS)		*/
			if (len > 1) {
			 	if (IsEUCJPKanji(cp[1]))
					cp += 2, len -= 2;		/* valid 2 byte EUCJP		*/
				else
					return NO;				/* invalid 2of2 byte EUCJP	*/
			} else
				break;						/* buffer ended w/1of2 byte EUCJP */
		} else if (*cp < 0xFF) {		/* EUCJP JIS208 only:			*/
			if (len > 1) {
			 	if (IsEUCJPKanji(cp[1]))
					return YES;			/* valid 2 byte EUCJP, invalid SJIS	*/
				else
					return NO;				/* invalid 2of2 byte EUCJP	*/
			} else
				break;						/* buffer ended w/1of2 byte EUCJP */
		} else {
			return NO;					/* invalid EUCJP 1st byte: 0xFF	*/
		}
	}
	return MAYBE;
}

MODULE_PRIVATE int16
intl_detect_JCSID (uint16 defaultCSID, const unsigned char *buf, int32 len)
{
	register const unsigned char	*cp = buf;
	int		sjisFlag;
	int		eucjpFlag;

	/* JIS is 7bit. Scan to end of 7bit data or legitimate JIS ESC sequence. */
	while (len && (IsRoman(*cp) || (*cp == 0xA0))) { /* allow nbsp */
		if (IsJISEscSeq(cp, len))
			return CS_JIS;
		cp++, len--;
	}

	/* If len > 0, must be either SJIS or EUC because there's 8bit data */
	while (len) {
		if (*cp == 0x80) {
			return CS_DEFAULT;/* illegal byte1 (SJIS & EUCJP) */
		}
		if (*cp < 0x8E)
			return CS_SJIS;		/* Illegal EUCJP 1st byte	*/
		if (*cp == 0xA0) {
#if ALLOW_NBSP
			cp++; len--;
			continue; /* allow nbsp */
#else
			return CS_DEFAULT;/* illegal byte1 (SJIS & EUCJP) */
#endif
		}
		if ( (*cp > 0xEF) && (*cp < 0xFF) )	/* illegal SJIS 1st byte	*/
			return CS_EUCJP;
		if (*cp == 0xFF) {
			return CS_DEFAULT;/* illegal byte1 (SJIS & EUCJP) */
		}
		
		/* At this point. 1st byte is 0x8E, 0x8F, or 0xA1-0xEF.				*/
		/* If 1st Byte is 0xE0-0xEF inclusive, then it's 2byte SJIS or EUC	*/
		if ((*cp > 0xDF) && (*cp < 0xF0)) {
			if (len > 1) {
				if (cp[1] < 0x41) {			/* illegal byte2 (SJIS & EUCJP) */
					return CS_DEFAULT;
				}
				if (cp[1] < 0xA1)
					return CS_SJIS;			/* Illegal EUCJP 2nd byte */
				if (cp[1] > 0xFC)
					return CS_EUCJP;		/* illegal SJIS 2nd byte */
				cp += 2, len -= 2;			/* Skip 2 byte character */

				/* Gobble up single byte characters and continue outer loop	*/
				while (len && IsRoman(*cp)) {
						cp++, len--;
				}
				continue;
			} else {
				len = 0;
				break;						/* No more chars in buffer 		*/
			}
		}
		/* 1st Byte is 0xA1-DF inclusive:
		 * 1byte SJIS kana or 1of2 byte SJIS or EUC
		 */
		break;							/* break and handle ambiguous cases	*/
	}

	if (len) {
		eucjpFlag = isEUCJP(cp, len);
		if (YES == eucjpFlag)
			return CS_EUCJP;

		sjisFlag = isSJIS(cp, len);
		if (YES == sjisFlag)
			return CS_SJIS;

		/* Neither one is YES, look at NO : MAYBE Pair */
		if ((NO == eucjpFlag) && (MAYBE == sjisFlag))
			return CS_SJIS;
		if ((MAYBE == eucjpFlag) && (NO == sjisFlag))
			return CS_EUCJP;
	}

	/* Some servers relied upon the previous Nav3.0 default for ambiguous SJIS/EUC encoding. */
#define USE_ACKBAR_LOGIC 1

	/* Now, both are NO or both are MAYBE, look at default */
	if (len) {						/* Must be ambiguous -- EUC or SJIS		*/
#if USE_ACKBAR_LOGIC

#ifdef XP_MAC
		defaultCSID = CS_SJIS_AUTO;					 /* simulate Akbar old charset hints */
#else
		defaultCSID = CS_JIS;
#endif
		if (defaultCSID == CS_SJIS) {
            eucjpFlag = isEUCJP(cp, len);
            if (eucjpFlag == YES)
                return CS_EUCJP;
            else
                return CS_SJIS;
        } else if (defaultCSID == CS_EUCJP) {
            sjisFlag = isSJIS(cp, len);
            if (sjisFlag == YES)
                return CS_SJIS;
            else
                return CS_EUCJP;
        } else {                            /* default is JIS */
            sjisFlag = isSJIS(cp, len);
            if (sjisFlag == YES)
                return CS_SJIS;
            eucjpFlag = isEUCJP(cp, len);
            if (eucjpFlag == YES)
                return CS_EUCJP;
            if (sjisFlag == NO) {
                if (eucjpFlag != NO)        /* SJIS-NO, EUCJP-MAYBE */
                    return CS_EUCJP;
            } else {
                if (eucjpFlag == NO)        /* SJIS-MAYBE, EUCJP-NO */
                    return CS_SJIS;
                else {                      /* both MAYBE */
                    return CS_EUCJP;        /* have to pick one... */
                }
            }
        }
#else
		if (CS_SJIS == defaultCSID) {
			if (MAYBE == sjisFlag)
				return CS_SJIS;
		} else if (CS_EUCJP == defaultCSID) {
			if (MAYBE == eucjpFlag)
				return CS_EUCJP;
		} else {							/* default is JIS */
			if ((MAYBE == eucjpFlag) && (MAYBE == sjisFlag))		/* pick one- EUC */
				return CS_EUCJP;
		}
#endif
	}
	return CS_ASCII;				/* Could be any of the 3... */
}

	/* Auto Detect Japanese Char Code Conversion	*/
MODULE_PRIVATE unsigned char *
autoJCCC (CCCDataObject obj, const unsigned char *s, int32 l)
{
	int16 doc_csid = 0;
	uint16 detected_doc_csid;

	/* try to determine doc Japanese CSID.	*/
	doc_csid = intl_detect_JCSID((uint16)(INTL_GetCCCDefaultCSID(obj)&~CS_AUTO),
											(const unsigned char *) s,l);
	if (doc_csid == CS_ASCII) {	/* return s unconverted and				*/
		INTL_SetCCCLen(obj, l);
		return (unsigned char *)s;	/* autodetect next block of stream data	*/
	}
	if (doc_csid == CS_DEFAULT) { /* found unexpected chars */
		doc_csid = INTL_GetCCCDefaultCSID(obj) & ~CS_AUTO;
		detected_doc_csid = CS_DEFAULT;
	} else {
		detected_doc_csid = doc_csid | CS_AUTO;
	}
	/* Setup converter function for success streams data blocks	*/
	(void) INTL_GetCharCodeConverter(doc_csid, INTL_GetCCCToCSID(obj), obj);
	INTL_CallCCCReportAutoDetect(obj, detected_doc_csid);

	/* If no conversion needed, change put_block module for successive
	 * data blocks.  For current data block, return unmodified buffer.
	 */
	if (INTL_GetCCCCvtfunc(obj) == NULL) {
		INTL_SetCCCLen(obj, l);
		return((unsigned char *) s);
	}
	/* For initial block, must call converter directly.  Success calls
	 * to the converter will be called directly from net_CharCodeConv()
	 */
	return (unsigned char *)(INTL_GetCCCCvtfunc(obj)) (obj, (const unsigned char	*)s, l);
}



