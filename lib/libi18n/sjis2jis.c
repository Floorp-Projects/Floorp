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
/*	sjis2jis.c	*/

#include "intlpriv.h"
#if defined(MOZ_MAIL_NEWS)
#include "katakana.h"
#endif

extern int MK_OUT_OF_MEMORY;


									/* SJIS to JIS Algorithm.		*/
#define TwoByteSJIS2JIS(sjisp, jisp, offset) {					\
 	*jisp = (*sjisp++ - offset) << 1; /* assign 1st byte */		\
 	if (*sjisp < 0x9F) {			/* check 2nd SJIS byte */	\
 		*jisp++ -= 1;				/* adjust 1st JIS byte */	\
 		if (*sjisp > 0x7F)										\
 			*jisp++ = *sjisp++ - 0x20;							\
 		else													\
 			*jisp++ = *sjisp++ - 0x1F;							\
 	} else {													\
 		jisp++;													\
 		*jisp++ = *sjisp++ - 0x7E;								\
 	}															\
}
				
/* net_sjis2jis(obj, sjisbuf, sjisbufsz)
 * Args:
 *	sjisbuf:	Ptr to a buf of SJIS chars
 *	sjisbufsz:	Size in bytes of sjisbuf
 *	jismode:	Ptr to encoding mode, use as arg for next call to
 *		mz_sjis2jis() for rest of current SJIS data.  First call should
 *		initialize mode to ASCII (0).					
 *	uncvtbuf:	If entire buffer was converted, uncvtbuf[0] will be null,
 *		else this points to SJIS chars that were NOT converted
 *		and mz_sjis2jis() with additional SJIS chars appended.
 * Return:
 *	Returns NULL on failure, otherwise it returns a pointer to a buffer of
 *	converted SJIS characters.  Caller must XP_FREE() this memory.
 *
 * Description:
 * 	Allocate destination JIS buffer.
 *
 * 	If the SJIS to JIS conversion changes JIS encoding, output proper ESC
 * 	sequence.
 *
 * 	If byte in ASCII range, just copy it to JIS buffer.
 * 	If Half-width SJIS katakana (1 byte), convert to Half-width JIS katakana.
 *	--- Now Half-width SJIS katakana is converted to 2-byte JIS katakana. ---
 * 	If 2-byte SJIS, convert to 2-byte JIS.
 * 	Otherwise assume user-defined SJIS, just copy 2 bytes.
 *
 *	If either SJIS buffer does not contain complete SJIS char or JIS buffer
 *	is full, then return unconverted SJIS to caller.  Caller should
 *	append more data and recall mz_sjis2jis.
 */

MODULE_PRIVATE unsigned char *
mz_sjis2jis(	CCCDataObject		obj,
			const unsigned char	*sjisbuf,	/* SJIS buf for conversion	*/
			int32				sjisbufsz)	/* SJIS buf size in bytes	*/
{
 	unsigned char			*tobuf = NULL;
 	int32					tobufsz;
 	register unsigned char	*sjisp, *tobufp;	/* current byte in bufs	*/
 	register unsigned char	*sjisep, *toep;		/* end of buffers		*/
 	int32					uncvtlen;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);
#if defined(MOZ_MAIL_NEWS)
  	unsigned char kanabuf[2];					/* for half-width kana */
 	uint32	byteused;							/* for half-width kana */
#endif
	
 										/* Allocate a JIS buffer:			*/
		/* JIS is longer than SJIS because of ESC seq.  In the worst case
		 * ( alternating Half-width Kana and Roman chars ), converted
		 * JIS will be 4X the size of the original SJIS + 1 for nul byte.
		 * Worst case: single half-width kana:
		 *	ESC ( I KANA ESC ( J
		 */
	uncvtlen = strlen((char *)uncvtbuf);
	tobufsz = ((sjisbufsz + uncvtlen) << 2) + 8;
	if ((tobuf = (unsigned char *)XP_ALLOC(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}
										/* Initialize pointers, etc.	*/
 	sjisp = (unsigned char *)sjisbuf;
 	sjisep = sjisp + sjisbufsz - 1;

#define uncvtp	tobufp	/* use tobufp as temp */ 	
							/* If prev. unconverted chars, append unconverted
							 * chars w/new chars and try to process.
							 */
 	if (uncvtbuf[0] != '\0') {
 		uncvtp = uncvtbuf + uncvtlen;
 		while (uncvtp < (uncvtbuf + UNCVTBUF_SIZE) &&
													sjisp <= sjisep)
 			*uncvtp++ = *sjisp++;
 		*uncvtp = '\0';						/* nul terminate	*/
 		sjisp = uncvtbuf;				/* process unconverted first */
 		sjisep = uncvtp - 1;
 	}
#undef uncvtp
 	
 	tobufp = tobuf;
 	toep = tobufp + tobufsz - 2;		/* save space for terminating null */
 	
WHILELOOP: 	
									/* While SJIS data && space in JIS buf. */
 	while ((sjisp <= sjisep) && (tobufp <= toep)) {
		if (*sjisp < 0x80) {
 										/* ASCII/JIS-Roman 				*/
 			if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 				InsASCII_ESC(tobufp, obj);
 			}
 			*tobufp++ = *sjisp++;

 		} else if (*sjisp < 0xA0) {
 										/* 1st byte of 2-byte low SJIS. */
 			if (sjisp+1 > sjisep)		/* No 2nd byte in SJIS buffer?	*/
 				break;

 			if (INTL_GetCCCJismode(obj) != JIS_208_83) {
 				Ins208_83_ESC(tobufp, obj);
 			}

 			TwoByteSJIS2JIS(sjisp, tobufp, 0x70);

 		} else if (*sjisp==0xA0) {
										/* SJIS half-width space.	*/
										/* Just treat like Roman??	*/
 			if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 				InsASCII_ESC(tobufp, obj);
 			}
 			*tobufp++ = *sjisp++;

 		} else if (*sjisp < 0xE0) {
										/* SJIS half-width katakana		*/
#if defined(MOZ_MAIL_NEWS)
			if (!INTL_GetCCCCvtflag_SendHankakuKana(obj)) {
 				if (INTL_GetCCCJismode(obj) != JIS_208_83) {
 					Ins208_83_ESC(tobufp, obj);
 				}
				INTL_SjisHalf2FullKana(sjisp, (uint32)sjisep - (uint32)sjisp + 1, kanabuf, &byteused);
															/* SJIS Katakana is 0x8340-0x8396 */
 				*tobufp++ = ((kanabuf[0] - 0x70) << 1) - 1;	/* assign 1st byte */
				if (kanabuf[1] > 0x7F)										
					*tobufp++ = kanabuf[1] - 0x20;
				else							
					*tobufp++ = kanabuf[1] - 0x1F;
				sjisp += byteused;
			} else {
	 			if (INTL_GetCCCJismode(obj) != JIS_HalfKana) {
	 				InsHalfKana_ESC(tobufp, obj);
	 			}
	 			*tobufp++ = *sjisp & 0x7F;
				sjisp++;
 	 		}
#else
 			if (INTL_GetCCCJismode(obj) != JIS_HalfKana) {
 				InsHalfKana_ESC(tobufp, obj);
 			}
 			*tobufp++ = *sjisp & 0x7F;
			sjisp++;
#endif
 		} else if (*sjisp < 0xF0) {
										/* 1st byte of 2-byte high SJIS */
 			if (sjisp+1 > sjisep)		/* No 2nd byte in SJIS buffer? */
 				break;

 			if (INTL_GetCCCJismode(obj) != JIS_208_83) {
 				Ins208_83_ESC(tobufp, obj);
 			}

 			TwoByteSJIS2JIS(sjisp, tobufp, 0xB0);
 		} else {
										/* User Defined SJIS: copy bytes */
 			if (sjisp+1 > sjisep)		/* No 2nd byte in SJIS buf?	*/
 				break;

 			if (INTL_GetCCCJismode(obj) != JIS_208_83) {
 				Ins208_83_ESC(tobufp, obj);
 			}

 			*tobufp++ = *sjisp++;			/* Just copy 2 bytes.	*/
 			*tobufp++ = *sjisp++;
 		}
 	}
 	
 	if (uncvtbuf[0] != '\0') {
 										/* tobufp pts to 1st unprocessed char in
 										 * tobuf.  Some may have been processed
 										 * while processing unconverted chars,
 										 * so set up ptrs not to process them
 										 * twice.
 										 */
 		sjisp = (unsigned char *)sjisbuf + (sjisp - uncvtbuf - uncvtlen);
											/* save space for term. null */
 		sjisep = (unsigned char *)sjisbuf + sjisbufsz - 1;
 		uncvtbuf[0] = '\0';		/* No more uncoverted chars.	*/
 		goto WHILELOOP;					/* Process new data				*/
 	}

 	if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 		INTL_SetCCCJismode(obj, JIS_Roman);
 		InsASCII_ESC(tobufp, obj);
	}

	*tobufp = '\0';						/* null terminate JIS data */
	INTL_SetCCCLen(obj,  tobufp - tobuf);			/* length not counting null	*/

 	if (sjisp <= sjisep) {				/* uncoverted SJIS?		*/
		tobufp = uncvtbuf;			/* reuse the tobufp as a TEMP */
 		while (sjisp <= sjisep)
 			*tobufp++ = *sjisp++;
 		*tobufp = '\0';					/* null terminate		*/
 	}
	return(tobuf);
}
 
