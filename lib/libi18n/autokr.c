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
/*	autokr.c	*/

#include "intlpriv.h"
#include "xp.h"
#include "libi18n.h"

MODULE_PRIVATE int16 intl_detect_KCSID( uint16 defaultCSID, const unsigned char *buf, int32 len )
{
	register const unsigned char	*cp = buf;
	/* CS_2022_KR is 7bit. Scan to end of 7bit data or legitimate KOREAN ESC sequence. */
	while( len && !( *cp & 0x80 ) ){
		/* CS_2022_KR  ESC $ ) C */
		if(( cp[0] == ESC && len > 3 && ( cp[1] == '$' && cp[2] == ')' && cp[3] == 'C' ) ) ||
			(SI == *cp) || (SO == *cp) )
			return CS_2022_KR;
		cp++, len--;
	}

	if( len > 0 )   return CS_KSC_8BIT;		/* it is not Roman */

	return CS_ASCII;						/* Could be any of the 3... */
}



/* Auto Detect Korean Char Code Conversion	: jliu */
MODULE_PRIVATE unsigned char *
autoKCCC (CCCDataObject obj, const unsigned char *s, int32 l)
{
	int16 doc_csid = 0;

	/* Use 1st stream data block to guess doc Korean CSID.	*/
	doc_csid = intl_detect_KCSID (INTL_GetCCCDefaultCSID(obj),(const unsigned char *) s, l);

	if( doc_csid == CS_ASCII ){	/* return s unconverted and				*/
		INTL_SetCCCLen(obj, l);
		return (unsigned char *)s;				/* autodetect next block of stream data	*/
	}

		/* Setup converter function for success streams data blocks	*/
	
	(void)INTL_GetCharCodeConverter( doc_csid, INTL_GetCCCToCSID(obj), obj );
	INTL_CallCCCReportAutoDetect(obj, doc_csid);
	
	
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
	return (unsigned char *)(INTL_GetCCCCvtfunc(obj)) (obj, (const unsigned char*)s, l);
}






