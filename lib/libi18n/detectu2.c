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
/*	detectu2.c	*/

#include "intlpriv.h"
#include "xp.h"
#include "libi18n.h"


/* 
	This function returns csid if it finds that the data is UCS-2 or
	UCS-2 needs swap.
 */
#define UCS2_GIVEUP_COUNT	512	/* Test at most 512 byte */
#define UCS2_00_TRIGGER		8	/* Need at least 4 0x00 to make our sample meanful */ 
#define UCS2_DETECT_DIFF    4   

MODULE_PRIVATE int DetectUCS2(CCCDataObject obj, unsigned char *pSrc, int len)
{
	/* need to fix DetectUCS2 to look for UTF8 */
	if(len < 2) return(CS_DEFAULT);

	if(((pSrc[0]<<8) | pSrc[1]) == BYTE_ORDER_MARK)
		return(CS_UCS2);
	
	if(((pSrc[0]<<8) | pSrc[1]) == NEEDS_SWAP_MARK)
			return(CS_UCS2_SWAP);
	return(CS_DEFAULT);
}





