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
/*	euc-kt to iso2022-kr	*/

#include "intlpriv.h"


extern int MK_OUT_OF_MEMORY;


/* net_euckr2iso(obj, eucbuf, eucbufsz)
 * Args:
 *	eucbuf:		Ptr to a buf of EUC chars
 *	eucbufsz:	Size in bytes of eucbuf
 *	uncvtbuf:	If entire buffer was converted, uncvtbuf[0] will be nul,
 *		else this points to EUC chars that were NOT converted
 *		and mz_euckr2iso() with additional EUC chars appended.
 * Return:
 *	Returns NULL on failure, otherwise it returns a pointer to a buffer of
 *	converted characters.  Caller must XP_FREE() this memory.
 *
 * Description:
 *
 *
 */

/*
	rewrite by Frank Tang

	About Unconverted Buffer:
		Since ISO-2022-KR will never be used as internal code. We do not need to make
		sure the output stream do not have partial characters. Therefore, we could
		ignore the unconverted buffer and never use it. We always treat the 
		stream in a byte oriented way.
	
*/

MODULE_PRIVATE unsigned char *
mz_euckr2iso(	CCCDataObject		obj,
			const unsigned char	*eucbuf,	/* EUC buffer for conversion	*/
			int32				eucbufsz)	/* EUC buffer size in bytes		*/
{
 	char unsigned			*tobuf = NULL;
 	int32					tobufsz;
 	register unsigned char	*tobufp, *eucp;		/* current byte in bufs	*/
 	register unsigned char	*tobufep, *eucep;	/* end of buffers		*/

 	/* Allocate a dest buffer:

           4 bytes: ESC $ ) C   
           2 bytes: CR LF
           * 2:     SI + one byte 
           1 bytes: SI
           1 bytes: NULL    

	 */

	tobufsz = 4 + 2 + eucbufsz * 2 + 1 + 1;


	if ((tobuf = (unsigned char *)XP_ALLOC(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}

 	tobufp = tobuf;
 	tobufep = tobufp + tobufsz - 3;		/* save space for terminating null */
 	eucp = (unsigned char *)eucbuf;
 	eucep = (unsigned char *)eucbuf + eucbufsz - 1;	/* save space for nul */

	if (!IsIns5601_87_ESC(obj))
	{
		Ins5601_87_ESC(tobufp, obj);
		*tobufp++ = (unsigned char)CR;		
		*tobufp++ = (unsigned char)NL;
	}


	while ((tobufp <= tobufep) && (eucp <= eucep))
	{

		if (*eucp & 0x80) {
			if (IsIns5601_87_SI(obj))
				Ins5601_87_SO(tobufp, obj);
			*tobufp++ = *eucp++ & 0x7f;
		} 
		else {
			if(IsIns5601_87_SO(obj))
				Ins5601_87_SI(tobufp, obj);
			*tobufp++ = *eucp++;
		}
	}

        if(IsIns5601_87_SO(obj))
 	   Ins5601_87_SI(tobufp, obj);
             
	*tobufp =  '\0';						/* null terminate dest. data */
	INTL_SetCCCLen(obj,  tobufp - tobuf);			/* length not counting null	*/

	return(tobuf);
}

