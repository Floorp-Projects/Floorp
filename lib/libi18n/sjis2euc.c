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
/*	sjis2euc.c	*/

#include "intlpriv.h"


extern int MK_OUT_OF_MEMORY;


									/* SJIS to EUC Algorithm.		*/
#define TwoByteSJIS2EUC(sjisp, eucp, offset) {					\
 	*eucp = ((*sjisp++ - offset) << 1) | 0x80; /* 1st EUC byte */ \
 	if (*sjisp < 0x9F) {			/* check 2nd SJIS byte */	\
 		*eucp++ -= 1;				/* adjust 1st EUC byte */	\
 		if (*sjisp > 0x7F)										\
 			*eucp++ = (*sjisp++ - 0x20) | 0x80;					\
 		else													\
 			*eucp++ = (*sjisp++ - 0x1F) | 0x80;					\
 	} else {													\
 		eucp++;													\
 		*eucp++ = (*sjisp++ - 0x7E) | 0x80;						\
 	}															\
}
				
/* net_sjis2euc(sjisbuf, sjisbufsz)
 * Args:
 *	sjisbuf:	Ptr to a buf of SJIS chars
 *	sjisbufsz:	Size in bytes of sjisbuf
 *	uncvtbuf:	If entire buffer was converted, uncvtbuf[0] will be nul,
 *		else this points to SJIS chars that were NOT converted
 *		and mz_sjis2euc() with additional SJIS chars appended.
 * Return:
 *	Returns NULL on failure, otherwise it returns a pointer to a buffer of
 *	converted SJIS characters.  Caller must XP_FREE() this memory.
 *
 * Description:
 * 	Allocate destination EUC buffer.
 *
 * 	If byte in ASCII range, just copy it to EUC buffer.
 * 	If Half-width SJIS katakana (1 byte), convert to Half-width EUC katakana.
 * 	If 2-byte SJIS, convert to 2-byte EUC.
 * 	Otherwise assume user-defined SJIS, just copy 2 bytes.
 *
 *	If either SJIS buffer does not contain complete SJIS char or EUC buffer
 *	is full, then return unconverted SJIS to caller.  Caller should
 *	append more data and recall mz_sjis2euc.
 */


MODULE_PRIVATE unsigned char *
mz_sjis2euc(	CCCDataObject		obj,
			const unsigned char	*sjisbuf,	/* SJIS buf for conversion	*/
			int32				sjisbufsz)	/* SJIS buf size in bytes	*/
{
 	unsigned char			*tobuf = NULL;
 	int32					tobufsz;
 	register unsigned char	*sjisp, *tobufp;	/* current byte in bufs	*/
 	register unsigned char	*sjisep, *tobufep;	/* end of buffers		*/
 	int32					uncvtlen;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);
 	
 										/* Allocate a EUC buffer:		*/
		/* In the worst case ( all Half-width Kanas ), the converted	*/
		/* EUC will be 2X the size of the SJIS + 1 for nul byte			*/
	uncvtlen = strlen((char *)uncvtbuf);
	tobufsz = ((sjisbufsz  + uncvtlen) << 1) + 1;
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
 	tobufep = tobufp + tobufsz - 1;		/* save space for terminating null */

WHILELOOP: 	
									/* While SJIS data && space in EUC buf. */
 	while ((sjisp <= sjisep) && (tobufp <= tobufep)) {
		if (*sjisp < 0x80) {
 										/* ASCII/JIS-Roman 				*/
 			*tobufp++ = *sjisp++;

 		} else if (*sjisp < 0xA0) {
 										/* 1st byte of 2-byte low SJIS. */
 			if (sjisp+1 > sjisep)		/* No 2nd byte in SJIS buffer?	*/
 				break;

 			TwoByteSJIS2EUC(sjisp, tobufp, 0x70);

 		} else if (*sjisp==0xA0) {
										/* SJIS half-width space.	*/
										/* Just treat like Roman??	*/
 			*tobufp++ = *sjisp++;

 		} else if (*sjisp < 0xE0) {
										/* SJIS half-width katakana		*/
			*tobufp++ = SS2;
			*tobufp++ = *sjisp | 0x80;	/* Set 8th bit for EUC & SJIS */
			sjisp++;

 		} else if (*sjisp < 0xF0) {
										/* 1st byte of 2-byte high SJIS */
 			if (sjisp+1 > sjisep)		/* No 2nd byte in SJIS buffer? */
 				break;

 			TwoByteSJIS2EUC(sjisp, tobufp, 0xB0);
 		} else {
										/* User Defined SJIS: copy bytes */
 			if (sjisp+1 > sjisep)		/* No 2nd byte in SJIS buf?	*/
 				break;

 			*tobufp++ = *sjisp++;			/* Just copy 2 bytes.	*/
 			*tobufp++ = *sjisp++;
 		}
 	}
 	
 	if (uncvtbuf[0] != '\0') {
 										/* jisp pts to 1st unprocessed char in
 										 * jisbuf.  Some may have been processed
 										 * while processing unconverted chars,
 										 * so set up ptrs not to process them
 										 * twice.
 										 */
 		sjisp = (unsigned char *)sjisbuf + (sjisp - uncvtbuf - uncvtlen);
 		sjisep = (unsigned char *)sjisbuf + sjisbufsz - 1;
 		uncvtbuf[0] = '\0';		/* No more uncoverted chars.	*/
 		goto WHILELOOP;					/* Process new data				*/
 	}

	*tobufp = '\0';						/* null terminate EUC data */
	INTL_SetCCCLen(obj,  tobufp - tobuf);			/* length not counting null	*/

 	if (sjisp <= sjisep) {				/* uncoverted SJIS?		*/
		tobufp = uncvtbuf;			/* reuse the tobufp as a TEMP */
 		while (sjisp <= sjisep)
 			*tobufp++ = *sjisp++;
 		*tobufp = '\0';					/* null terminate		*/
 	}
	return(tobuf);
}
 
