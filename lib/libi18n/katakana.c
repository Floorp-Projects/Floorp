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

/*
 *	katakana.c
 *
 *	Half- to Full-width Katakana Conversion for SJIS and EUC.
 *	based on the book of Ken Lunde 
 *  (Understanding Japanese Information Processing, Published by O'Reilly; 09/1993; ISBN: 1565920430).
*/


#if defined(MOZ_MAIL_NEWS)

#include "intlpriv.h"
#include "prefapi.h"
#include "katakana.h"

#define ISMARU(A)	(A >= 202 && A <= 206)
#define ISNIGORI(A)	((A >= 182 && A <= 196) || (A >= 202 && A <= 206) || (A == 179))
#define HANKATA(c)	(c >= 161 && c <= 223)			/* 0xa1 - 0xdf */

/* pref related prototype and variables */
PUBLIC int PR_CALLBACK intl_SetSendHankakuKana(const char * newpref, void * data);

static XP_Bool pref_callback_installed = FALSE;
static XP_Bool send_hankaku_kana = FALSE;
static const char *pref_send_hankaku_kana = "mailnews.send_hankaku_kana";


static void han2zen(XP_Bool insjis, unsigned char *inbuf, uint32 inlen, 
					unsigned char *outbuf, XP_Bool *composite)
{
	unsigned char	c1 = *inbuf++;
	unsigned char	c2;
	unsigned char	tmp = c1;
	unsigned char	junk;
	XP_Bool			maru = FALSE;
	XP_Bool			nigori = FALSE;
	unsigned char	mtable[][2] = {
		{129,66},{129,117},{129,118},{129,65},{129,69},{131,146},{131,64},
		{131,66},{131,68},{131,70},{131,72},{131,131},{131,133},{131,135},
		{131,98},{129,91},{131,65},{131,67},{131,69},{131,71},{131,73},
		{131,74},{131,76},{131,78},{131,80},{131,82},{131,84},{131,86},
		{131,88},{131,90},{131,92},{131,94},{131,96},{131,99},{131,101},
		{131,103},{131,105},{131,106},{131,107},{131,108},{131,109},
		{131,110},{131,113},{131,116},{131,119},{131,122},{131,125},
		{131,126},{131,128},{131,129},{131,130},{131,132},{131,134},
		{131,136},{131,137},{131,138},{131,139},{131,140},{131,141},
		{131,143},{131,147},{131,74},{129,75}
	};
	
	if (inlen > 1)
		{
		if (insjis)
			{
			c2 = *inbuf;
			if (c2 == 222 && ISNIGORI(c1))
				nigori = TRUE;
			if (c2 == 223 && ISMARU(c1))
				maru = TRUE;
			}
		else	/* EUC */
			{
			junk = *inbuf++;
			c2 = *inbuf;
			if (junk == SS2)				/* If the variable junk is SS2, we have another half-
											width katakana. */
				{
				if (c2 == 222 && ISNIGORI(c1))
					nigori = TRUE;
				if (c2 == 223 && ISMARU(c1))
					maru = TRUE;
				}
			}
		}
	if (HANKATA(tmp))						/* Check to see if tmp is in half-width katakana range. */
		{
		c1 = mtable[tmp - 161][0];			/* Calculate first byte using mapping table */
		c2 = mtable[tmp - 161][1];			/* Calculate second byte using mapping table */
		}
	if (nigori)
		{
		if ((c2 >= 74 && c2 <= 103) || (c2 >= 110 && c2 <= 122))
			c2++;
		else if (c1 == 131 && c2 == 69)
			c2 = 148;
		}
	else if (maru && c2 >= 110 && c2 <= 122)
		c2 += 2;
		
	*outbuf++ = c1;
	*outbuf = c2;
	*composite = maru || nigori;
}

/*
 * Half to full Katakana conversion for SJIS. Caller need to allocate outbuf (x2 of inbuf).
 */
MODULE_PRIVATE void INTL_SjisHalf2FullKana(unsigned char *inbuf, uint32 inlen, unsigned char *outbuf, uint32 *byteused)
{
	XP_Bool composite;
		
	han2zen(TRUE, inbuf, inlen, outbuf, &composite);
	*byteused = composite ? 2 : 1;
}

/*
 * Half to full Katakana conversion for EUC. Caller need to allocate outbuf (x3 of inbuf).
 */
MODULE_PRIVATE void INTL_EucHalf2FullKana(unsigned char *inbuf, uint32 inlen, unsigned char *outbuf, uint32 *byteused)
{
	XP_Bool composite;
		
	han2zen(FALSE, inbuf, inlen, outbuf, &composite);
	*byteused = composite ? 3 : 1;		/* 2 chars plus SS2 or 1 char */
}

/* callback routine invoked by prefapi when the pref value changes */
PUBLIC int PR_CALLBACK intl_SetSendHankakuKana(const char * newpref, void * data)
{
	return PREF_GetBoolPref(pref_send_hankaku_kana, &send_hankaku_kana);
}

MODULE_PRIVATE XP_Bool INTL_GetSendHankakuKana()
{
	if (!pref_callback_installed)
	{
		PREF_GetBoolPref(pref_send_hankaku_kana, &send_hankaku_kana);
		PREF_RegisterCallback(pref_send_hankaku_kana, intl_SetSendHankakuKana, NULL);
		pref_callback_installed = TRUE;
	}
	
	return send_hankaku_kana;
}

#endif /* MOZ_MAIL_NEWS */
