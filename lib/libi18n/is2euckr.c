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
/*	is2euckr.c	*/

#include "intlpriv.h"


extern int MK_OUT_OF_MEMORY;


/* net_iso2euckr(obj, isobuf, isobufsz, uncvtbuf)
 * Args:
 *	isobuf:		Ptr to a buf of iso-2022-kr chars
 *	isobufsz:	Size in bytes of isobuf
 *	jismode:	Ptr to encoding mode, use as arg for next call to
 *		mz_iso2euckr() for rest of current 2022-kr data.  First call should
 *		initialize mode to ASCII (0).					
 *	uncvtbuf:	If entire buffer was converted, uncvtbuf[0] will be nul,
 *		else this points to iso-2022-kr chars that were NOT converted
 *		and mz_iso2euckr() with additional iso-2022-kr chars appended.
 * Return:
 *	Returns NULL on failure, otherwise it returns a pointer to a buffer of
 *	converted EUC-KR characters.  Caller must XP_FREE() this memory.
 *
 * Description:
 *
 *	Allocate destination buffer (for EUC-KR).
 *
 *	Set mode state based upon ESC sequence and SO/SI.
 *
 *  If mode is KSC 5601, set 8th bits of next 2 bytes.
 *
 *	If any other mode, then assume ASCII and strip the 8th bit.
 *
 *	If either 2022-kr buffer does not contain complete char or EUC-KR buffer
 *	is full, then return unconverted 2022-kr to caller.  Caller should
 *	append more data and recall mz_iso2euckr.
 */


MODULE_PRIVATE unsigned char *
mz_iso2euckr(	CCCDataObject		obj,
			const unsigned char	*isobuf,	/* 2022-kr buffer for conversion */
			int32				isobufsz)	/* 2022-kr buffer size in bytes	*/
{
 	unsigned char			*tobuf = NULL;
 	int32					tobufsz;
 	unsigned char	*tobufp, *isop;		/* current byte in bufs	*/
 	unsigned char	*tobufep, *isoep;	/* end of buffers		*/
 	int32					uncvtlen;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);

#define euckrbufsz	tobufsz
#define euckrbuf		tobuf
#define euckrp		tobufp
#define euckrep		tobufep
 										/* Allocate a dest buffer:		*/
		/* 2022-kr is usually longer than EUC-KR because of ESC seq.
		 *
		 * In the worst case (all ASCII), converted EUC-KR will be the same
		 * length as the original 2022-kr + 1 for nul byte
		 */
	uncvtlen = strlen((char *)uncvtbuf);
	tobufsz = isobufsz + uncvtlen + 1;

	if (!tobufsz) {
		return NULL;
	}

	if ((tobuf = (unsigned char *)XP_ALLOC(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}
										/* Initialize pointers, etc.	*/
 	isop = (unsigned char *)isobuf;
 	isoep = isop + isobufsz - 1;

#define uncvtp	tobufp	/* use tobufp as temp */ 		 		
							/* If prev. unconverted chars, append unconverted
							 * chars w/new chars and try to process.
							 */
 	if (uncvtbuf[0] != '\0') {
 		uncvtp = uncvtbuf + uncvtlen;
 		while (uncvtp < (uncvtbuf + UNCVTBUF_SIZE) &&
														isop <= isoep)
 			*uncvtp++ = *isop++;
 		*uncvtp = '\0';						/* nul terminate	*/
 		isop = uncvtbuf;				/* process unconverted first */
 		isoep = uncvtp - 1;
 	}
#undef uncvtp
 	
 	tobufp = tobuf;
 	tobufep = tobufp + tobufsz - 2;		/* save space for terminating null */
 	
WHILELOOP: 	
	INTL_SetCCCJismode(obj, KSC_5601_87);  /* jliu doesn't want to change Tony's code too much*/
							/* While 2022-kr data && space in EUC-KR buf. */
 	while ((tobufp <= tobufep) && (isop <= isoep)) {

		if( isop[0] == ESC && isoep - isop > 3 && ( isop[1] == '$' && isop[2] == ')' 
			&& isop[3] == 'C' ) ){
			/* eat that ESC seq. */
			isop += 4;
		} else if (*isop == SO) {
			/* obj->jismode |= SHIFT_OUT; */
			INTL_SetCCCJismode(obj, INTL_GetCCCJismode(obj) | SHIFT_OUT);
			isop++;
		} else if (*isop == SI) {
			INTL_SetCCCJismode(obj, INTL_GetCCCJismode(obj) & (~SHIFT_OUT));
			isop++;
		} else if (INTL_GetCCCJismode(obj) == (KSC_5601_87 | SHIFT_OUT)) {
			if(*isop == 0x20)  /* jliu */
			{
				*euckrp++ = *isop++ ;
			}
 			else
			{
				if ((isop+1) > isoep)		/* Incomplete 2Byte char in JIS buf? */
 					break;

				*euckrp++ = *isop++ | 0x80;
				*euckrp++ = *isop++ | 0x80;
			}
		} else if ((0xA1 <= *isop) && (*isop <= 0xFE)) {
				/*	Somehow we hit EUC_KR data, let it through */
				if ((isop+1) > isoep)		/* Incomplete 2Byte char in JIS buf? */
 					break;
				*euckrp++ = *isop++ ;
				*euckrp++ = *isop++ ;
		} else {
											/* Unknown type: no conversion	*/
			*euckrp++ = *isop++ & 0x7f;
		}
	}
	
 	if (uncvtbuf[0] != '\0') {
										/* Just processed unconverted chars:
										 * isop pts to 1st unprocessed char in
 										 * isobuf.  Some may have been processed
 										 * while processing unconverted chars,
 										 * so set up ptrs not to process them
 										 * twice.
 										 */
										/* If nothing was converted, this can
										 * only happen if there was not
										 * enough 2022-kr data.  Stop and get
										 * more data.
										 */
		if (isop == uncvtbuf) {	/* Nothing converted */
			*tobufp = '\0';
			return(NULL);
		}
 		isoep = (unsigned char *)isobuf + isobufsz - 1 ;
 		isop = (unsigned char *)isobuf + (isop - uncvtbuf - uncvtlen);
 		uncvtbuf[0] = '\0';			/* No more uncoverted chars.	*/
 		goto WHILELOOP;					/* Process new data				*/
 	}

	*tobufp = '\0';						/* null terminate dest. data */
	INTL_SetCCCLen(obj,  tobufp - tobuf);			/* length not counting null	*/

 	if (isop <= isoep) {				/* unconverted 2022-kr?		*/
		tobufp = uncvtbuf;				/* reuse the tobufp as a TEMP */
 		while (isop <= isoep)
 			*tobufp++ = *isop++;
 		*tobufp = '\0';					/* null terminate		*/
 	}
	return(tobuf);
}


