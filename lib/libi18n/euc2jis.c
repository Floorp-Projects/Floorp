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
/*	euc2jis.c	*/

#include "intlpriv.h"
#ifdef XP_MAC
#include "katakana.h"
#endif


extern int MK_OUT_OF_MEMORY;


/* net_euc2jis(obj, eucbuf, eucbufsz)
 * Args:
 *	eucbuf:		Ptr to a buf of EUC chars
 *	eucbufsz:	Size in bytes of eucbuf
 *	obj->eucmode:	Ptr to encoding mode, use as arg for next call to
 *		mz_euc2jis() for rest of current EUC data.  First call should
 *		initialize mode to ASCII (0).					
 *	uncvtbuf:	If entire buffer was converted, uncvtbuf[0] will be nul,
 *		else this points to EUC chars that were NOT converted
 *		and mz_euc2jis() with additional EUC chars appended.
 * Return:
 *	Returns NULL on failure, otherwise it returns a pointer to a buffer of
 *	converted characters.  Caller must XP_FREE() this memory.
 *
 * Description:
 *
 *	Allocate destination buffer.
 *
 *	Ouput JIS ESC sequences based upon which EUC code set.
 *
 *  No conversion is needed for ASCII/JIS Roman characters.
 *
 *	Clear 8th bit of 1-byte Half-width Katakana.  Half-width Katakana
 *  is not widely used and its ESC sequence may not be recognized
 *  by some software.  It's use on the internet is discouraged...
 *
 *	Clear 8th bits of 2-byte JIS X 212-1990 chars.  JIS-212
 *  is not widely used and its ESC sequence may not be recognized
 *  by some software.  These chars do not have corresponding chars
 *	in JIS-208 or SJIS.
 *
 *	Clear 8th bits of 2-byte JIS X 208-1993 chars.  These are the commonly
 *	used chars (along with JIS-Roman).
 *
 *	Bytes which do not fall in the EUC valid character codes are treated
 *	like JIS-Roman.
 *
 *	If either EUC buffer does not contain a complete EUC char or dest buffer
 *	is full, then return unconverted EUC to caller.  Caller should
 *	append more data and recall mz_euc2jis.
 */


MODULE_PRIVATE unsigned char *
mz_euc2jis(	CCCDataObject		obj,
			const unsigned char	*eucbuf,	/* EUC buffer for conversion	*/
			int32				eucbufsz)	/* EUC buffer size in bytes		*/
{
 	unsigned char			*tobuf = NULL;
 	int32					tobufsz;
 	register unsigned char	*tobufp, *eucp;		/* current byte in bufs	*/
 	register unsigned char	*tobufep, *eucep;	/* end of buffers		*/
 	int32					uncvtlen;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);
#ifdef FEATURE_KATAKANA
 	unsigned char outbuf[2];				/* for 1 byte katakana */
 	uint32 byteused;						/* for 1 byte katakana */
#endif

 										/* Allocate a dest buffer:		*/
		/* JIS is longer than EUC because of ESC seq.  In the worst case
		 * ( <SS2> <Half-width Kana> <Roman> ... ), the converted JIS will
		 * be 2-2/3 times the size of the original EUC + 1 for nul byte.
		 * Worst case: single half-width kana:
		 *	ESC ( I KANA ESC ( J
		 */
	uncvtlen = strlen((const char *)uncvtbuf);
										/* 3 times length of EUC	*/
	tobufsz = eucbufsz + uncvtlen + ((eucbufsz + uncvtlen)<<2) + 8;
	if ((tobuf = (unsigned char *)XP_ALLOC(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}
										/* Initialize pointers, etc.	*/
 	eucp = (unsigned char *)eucbuf;
 	eucep = eucp + eucbufsz - 1;

#define uncvtp	tobufp	/* use tobufp as temp */ 		 		
							/* If prev. unconverted chars, append unconverted
							 * chars w/new chars and try to process.
							 */
 	if (uncvtbuf[0] != '\0') {
 		uncvtp = (unsigned char *)uncvtbuf + uncvtlen;
 		while (uncvtp < ((unsigned char *)uncvtbuf + UNCVTBUF_SIZE) &&
														eucp <= eucep)
 			*uncvtp++ = *eucp++;
 		*uncvtp = '\0';						/* nul terminate	*/
 		eucp = (unsigned char *)uncvtbuf; /* process unconverted first */
 		eucep = uncvtp - 1;
 	}
#undef uncvtp
 	
 	tobufp = tobuf;
 	tobufep = tobufp + tobufsz - 2;		/* save space for terminating null */
 	
WHILELOOP: 	
									/* While EUC data && space in dest. buf. */
 	while ((tobufp <= tobufep) && (eucp <= eucep)) {
		if (*eucp < SS2) {				/* ASCII/JIS-Roman or invalid EUC	*/
 			if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 				InsASCII_ESC(tobufp, obj);
 			}
 			*tobufp++ = *eucp++;
		} else if (*eucp == SS2) {		/* Half-width Katakana			*/
#ifdef FEATURE_KATAKANA
 			if (eucp+1 > eucep)			/* No 2nd byte in EUC buffer?	*/
 				break;
 			if (INTL_GetCCCJismode(obj) != JIS_208_83) {
 				Ins208_83_ESC(tobufp, obj);
 			}
			eucp++;							/* skip SS2	*/
			INTL_EucHalf2FullKana(eucp, (uint32)eucep - (uint32)eucp + 1, outbuf, &byteused);
			*tobufp++ = outbuf[0] & 0x7F;
			*tobufp++ = outbuf[1] & 0x7F;
			eucp += byteused;
#else
 			if (eucp+1 > eucep)			/* No 2nd byte in EUC buffer?	*/
 				break;
 			if (INTL_GetCCCJismode(obj) != JIS_HalfKana) {
 				InsHalfKana_ESC(tobufp, obj);
 			}
			eucp++;							/* skip SS2	*/
 			*tobufp++ = *eucp & 0x7F;
			eucp++;
#endif

		} else if (*eucp == SS3) {		/* JIS X 0212-1990				*/
 			if (eucp+2 > eucep)			/* No 2nd & 3rd bytes in EUC buffer? */
 				break;
			if (*(eucp+1) <= 0xA0 || *(eucp+2) <= 0xA0) { /* Invalid EUC212	*/
 				if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 					InsASCII_ESC(tobufp, obj);
				}
				*tobufp++ = *eucp++;			/* process 1 byte as Roman */
 			} else {
 				if (INTL_GetCCCJismode(obj) != JIS_212_90) {
					Ins212_90_ESC(tobufp, obj);
 				}
				eucp++;							/* skip SS3	*/
 				*tobufp++ = *eucp & 0x7F;
				eucp++;
 				*tobufp++ = *eucp & 0x7F;
				eucp++;
			}
		} else if (*eucp < 0xA0) {		/* Invalid EUC: treat as Roman	*/
 			if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 				InsASCII_ESC(tobufp, obj);
 			}
 			*tobufp++ = *eucp++;
		} else {						/* JIS X 0208-1990				*/
 			if (eucp+1 > eucep)			/* No 2nd byte in EUC buffer?	*/
 				break;
			if (*(eucp+1) < 0xA0) {		/* 1st byte OK, check if 2nd is valid */
 				if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 					InsASCII_ESC(tobufp, obj);
				}
				*tobufp++ = *eucp++;			/* process 1 byte as Roman	*/
 			} else {
 				if (INTL_GetCCCJismode(obj) != JIS_208_83) {
 					Ins208_83_ESC(tobufp, obj);
 				}
 				*tobufp++ = *eucp & 0x7F;
				eucp++;
 				*tobufp++ = *eucp & 0x7F;
				eucp++;
			}
		}
	}
	
 	if (uncvtbuf[0] != '\0') {
										/* Just processed unconverted chars:
										 * eucp pts to 1st unprocessed char in
 										 * eucbuf.  Some may have been processed
 										 * while processing unconverted chars,
 										 * so set up ptrs not to process them
 										 * twice.
 										 */
										/* If nothing was converted, this can
										 * only happen if there was not
										 * enough EUC data.  Stop and get
										 * more data.
										 */
		if (eucp == (unsigned char *)uncvtbuf) {	/* Nothing converted */
			*tobufp = '\0';
			return(NULL);
		}
 		eucp = (unsigned char *)eucbuf +
 							(eucp - (unsigned char *)uncvtbuf - uncvtlen);
 		eucep = (unsigned char *)eucbuf + eucbufsz - 1;	/* save space for nul */
 		uncvtbuf[0] = '\0';		/* No more uncoverted chars.	*/
 		goto WHILELOOP;					/* Process new data				*/
 	}
 	
 	if (INTL_GetCCCJismode(obj) != JIS_Roman) {
 		INTL_SetCCCJismode(obj, JIS_Roman);
 		InsASCII_ESC(tobufp, obj);
	}

	*tobufp = '\0';						/* null terminate dest. data */
	INTL_SetCCCLen(obj,  tobufp - tobuf);			/* length not counting null	*/

 	if (eucp <= eucep) {				/* uncoverted EUC?			*/
		tobufp = (unsigned char *)uncvtbuf;/* reuse the tobufp as a TEMP */
 		while (eucp <= eucep)
 			*tobufp++ = *eucp++;
 		*tobufp = '\0';					/* null terminate			*/
 	}
	return(tobuf);
}
				
