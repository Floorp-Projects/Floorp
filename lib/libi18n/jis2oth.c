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
/*	jis2oth.c	*/
/*	other: SJIS or EUC */

#include "intlpriv.h"


extern int MK_OUT_OF_MEMORY;


/* net_jis2other(obj, jisbuf, jisbufsz, uncvtbuf)
 * Args:
 *	jisbuf:		Ptr to a buf of JIS chars
 *	jisbufsz:	Size in bytes of jisbuf
 *	jismode:	Ptr to encoding mode, use as arg for next call to
 *		jis2other() for rest of current SJIS data.  First call should
 *		initialize mode to ASCII (0).					
 *	uncvtbuf:	If entire buffer was converted, uncvtbuf[0] will be nul,
 *		else this points to SJIS chars that were NOT converted
 *		and jis2other() with additional SJIS chars appended.
 *	obj->cvtflag:	Specifies converting to either EUC or SJIS.
 * Return:
 *	Returns NULL on failure, otherwise it returns a pointer to a buffer of
 *	converted JIS characters.  Caller must XP_FREE() this memory.
 *
 * Description:
 *
 *	Allocate destination buffer (for SJIS or EUC).
 *
 *	Set JIS mode state based upon ESC sequences.  Also if NL or CR,
 *	mode is reset to JIS-Roman.
 *
 *  If JIS mode is JIS x208 and converting to EUC, set 8th bits of next 2 bytes.
 *
 *  If JIS mode is JIS x208-1983 and converting to SJIS, use the
 *	JIS to SJIS algorithm.
 *
 *  If JIS mode is JIS x212 and converting to EUC, output SS3 and set 8th
 *	bits of next 2 bytes.  (This mode only set when converting to EUC.)
 *
 *	If JIS Half-width Katakana and converting to EUC, output SS2 followed
 *	by the 2 bytes w/8th bits set.
 *
 *	If JIS Half-width Katakana and converting to SJIS, output the 2 bytes
 *	w/8th bits set.
 *
 *	If any other JIS mode, then assume Latin1 and just copy the next byte.
 *
 *	If either SJIS buffer does not contain complete JIS char or JIS buffer
 *	is full, then return unconverted SJIS to caller.  Caller should
 *	append more data and recall jis2other.
 */


MODULE_PRIVATE unsigned char *
jis2other(	CCCDataObject		obj,
			const unsigned char	*jisbuf,	/* JIS buffer for conversion*/
			int32				jisbufsz)	/* JIS buffer size in bytes	*/
{
 	unsigned char			*tobuf = NULL;
 	int32					tobufsz;
 	unsigned char	*tobufp, *jisp;		/* current byte in bufs	*/
 	unsigned char	*tobufep, *jisep;	/* end of buffers		*/
 	int32					uncvtlen;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);

#define sjisbuf		tobuf
#define sjisbufsz	tobufsz
#define sjisp		tobufp
#define sjisep		tobufep

#define eucbufsz	tobufsz
#define eucbuf		tobuf
#define eucp		tobufp
#define eucep		tobufep
 										/* Allocate a dest buffer:		*/
		/* JIS is usually longer than SJIS or EUC because of ESC seq.
		 *
		 * In the worst case (all Roman), converted SJIS will be the same
		 * length as the original JIS + 1 for nul byte
		 *
		 * In the worst case ( <ESC> ( I <rest Half-width Kana>... ),
		 * converted EUC will be 2X - 2 the size of the original JIS + 1 for nul
		 * byte.
		 */
	uncvtlen = strlen((char *)uncvtbuf);
	if (!INTL_GetCCCCvtflag(obj))
		tobufsz = jisbufsz + uncvtlen + 1;
	else
		tobufsz = (jisbufsz + uncvtlen) << 1;

	if (!tobufsz) {
		return NULL;
	}

	if ((tobuf = (unsigned char *)XP_ALLOC(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}
										/* Initialize pointers, etc.	*/
 	jisp = (unsigned char *)jisbuf;
 	jisep = jisp + jisbufsz - 1;

#define uncvtp	tobufp	/* use tobufp as temp */ 		 		
							/* If prev. unconverted chars, append unconverted
							 * chars w/new chars and try to process.
							 */
 	if (uncvtbuf[0] != '\0') {
 		uncvtp = uncvtbuf + uncvtlen;
 		while (uncvtp < (uncvtbuf + UNCVTBUF_SIZE) &&
														jisp <= jisep)
 			*uncvtp++ = *jisp++;
 		*uncvtp = '\0';						/* nul terminate	*/
 		jisp = uncvtbuf;				/* process unconverted first */
 		jisep = uncvtp - 1;
 	}
#undef uncvtp
 	
 	tobufp = tobuf;
 	tobufep = tobufp + tobufsz - 2;		/* save space for terminating null */
 	
WHILELOOP: 	
									/* While JIS data && space in SJIS buf. */
 	while ((tobufp <= tobufep) && (jisp <= jisep)) {
		if (*jisp == ESC) {
 			if ((jisep - jisp) < 2)	/* Incomplete ESC seq in JIS buf?		*/
 				break;
			switch (jisp[1]) {
				case '(':
					switch (jisp[2]) {
						case 'J':				/* JIS X 0201-Roman			*/
						case 'B':				/* ASCII	*/
							INTL_SetCCCJismode(obj, JIS_Roman);
							jisp += 3;			/* remove ESC seq.	*/
							break;
						case 'I':				/* Half-width katakana	*/
							INTL_SetCCCJismode(obj, JIS_HalfKana);
							jisp += 3;			/* remove ESC seq.	*/
							break;
						default:				/* pass thru invalid ESC seq. */
							*tobufp++ = *jisp++;
							*tobufp++ = *jisp++;
					}
					break;
				case DOLLAR:
					switch (jisp[2]) {
				   		case 'B':				/* JIS X 0208-1983	*/
						case '@':				/* JIS X 0208-1978 (old-JIS) */
							INTL_SetCCCJismode(obj, JIS_208_83);
							jisp += 3;			/* remove rest of ESC seq.	*/
							break;
						case '(':
 							if ((jisep - jisp) < 3)	/* Full ESC seq in buf? */
 								goto abortwhile;
							switch (jisp[3]) {
								case 'D':			/* JIS X 0212-1990	*/
									if (!INTL_GetCCCCvtflag(obj))	/* No JIS212 in SJIS */
										INTL_SetCCCJismode(obj, JIS_208_83);
									else
										INTL_SetCCCJismode(obj, JIS_212_90);
									jisp += 4;	/* remove rest of ESC seq.	*/
									break;
								default:		/* pass thru invalid ESC seq. */
									*tobufp++ = *jisp++;
									*tobufp++ = *jisp++;
									break;
							}
							break;
						default:				/* pass thru invalid ESC seq. */
							*tobufp++ = *jisp++;
							*tobufp++ = *jisp++;
					}
					break;
				case '-':
					switch (jisp[2]) {
						case 'A':				/* ISO8859-1			*/
							INTL_SetCCCJismode(obj, JIS_Roman);
							jisp += 3;			/* remove rest of ESC seq.	*/
							break;
						default:				/* pass thru invalid ESC seq. */
							*tobufp++ = *jisp++;
							*tobufp++ = *jisp++;
					}
					break;
				default:						/* pass thru invalid ESC seq. */
					*tobufp++ = *jisp++;
			}
		} else if (*jisp == NL || *jisp == CR) {
			INTL_SetCCCJismode(obj, JIS_Roman);
			*tobufp++ = *jisp++;
		} else if (INTL_GetCCCJismode(obj) == JIS_208_83) {
 			if ((jisp+1) > jisep)		/* Incomplete 2Byte char in JIS buf? */
 				break;

			if (INTL_GetCCCCvtflag(obj)) {				/* Convert JIS 208 to EUC		*/
				*eucp++ = *jisp | 0x80;
				jisp++;
				*eucp++ = *jisp | 0x80;
				jisp++;

			} else {				/* Convert JIS-208 to SJIS: 	Same as	*/
									/* euc2sjis.c's EUC208-to-SJIS algorithm */
									/* except JIS 8th bit is clear. */
				if (*jisp < 0x5F)			/* Convert 1st SJIS byte	*/
					*sjisp++ = ((*jisp + 1) >> 1) + 0x70;
				else
					*sjisp++ = ((*jisp + 1) >> 1) + 0xB0;
											/* Convert 2nd SJIS byte	*/

				if ((*jisp++) & 1) {		/* if 1st JIS byte is odd	*/
					if (*jisp > 0x5F)
						*sjisp = *jisp + 0x20;
					else
						*sjisp = *jisp + 0x1F;
				} else {
					*sjisp = *jisp + 0x7E;
				}
				sjisp++;
				jisp++;
			}
		} else if (INTL_GetCCCJismode(obj) == JIS_212_90) {
										/* only "to EUC" supports 212	*/
 			if ((jisp+1) > jisep)		/* Incomplete 2Byte char in JIS buf? */
 				break;
			*eucp++ = SS3;
			*eucp++ = *jisp | 0x80;
			jisp++;
			*eucp++ = *jisp | 0x80;
			jisp++;
		} else if (INTL_GetCCCJismode(obj) == JIS_HalfKana) {
				if (INTL_GetCCCCvtflag(obj)) {
					*eucp++ = SS2;
				}
				*tobufp++ = *jisp | 0x80;		/* Set 8th bit for EUC & SJIS */
				jisp++;
		} else {
											/* Unknown type: no conversion	*/
			*tobufp++ = *jisp++;
		}
	}
abortwhile:
	
 	if (uncvtbuf[0] != '\0') {
										/* Just processed unconverted chars:
										 * jisp pts to 1st unprocessed char in
 										 * jisbuf.  Some may have been processed
 										 * while processing unconverted chars,
 										 * so set up ptrs not to process them
 										 * twice.
 										 */
										/* If nothing was converted, this can
										 * only happen if there was not
										 * enough JIS data.  Stop and get
										 * more data.
										 */
		if (jisp == uncvtbuf) {	/* Nothing converted */
			*tobufp = '\0';
			return(NULL);
		}
 		jisep = (unsigned char *)jisbuf + jisbufsz - 1 ;
 		jisp = (unsigned char *)jisbuf + (jisp - uncvtbuf - uncvtlen);
 		uncvtbuf[0] = '\0';			/* No more uncoverted chars.	*/
 		goto WHILELOOP;					/* Process new data				*/
 	}

	*tobufp = '\0';						/* null terminate dest. data */
	INTL_SetCCCLen(obj,  tobufp - tobuf);			/* length not counting null	*/

 	if (jisp <= jisep) {				/* uncoverted JIS?		*/
		tobufp = uncvtbuf;				/* reuse the tobufp as a TEMP */
 		while (jisp <= jisep)
 			*tobufp++ = *jisp++;
 		*tobufp = '\0';					/* null terminate		*/
 	}
	return(tobuf);
}
				
