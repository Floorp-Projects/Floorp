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
#include "xp.h"
#include "prmem.h"
#include "plstr.h"
#include "intl_csi.h"

INTL_CharSetInfo 
LO_GetDocumentCharacterSetInfo(MWContext *context)
{
    /* MOZ_FUNCTION_STUB; */
    return NULL;
}

void 
INTL_CharSetIDToName(int16 csid, char  *charset)
{
  if (charset) {
	PR_ASSERT(INTL_CsidToCharsetNamePt(csid));
	if (INTL_CsidToCharsetNamePt(csid))
	  PL_strcpy(charset,(char *)INTL_CsidToCharsetNamePt(csid));
	else 
	  charset = NULL;
  }
}


/* INTL_ResourceCharSet(void) */
char *INTL_ResourceCharSet(void)
{
    return ("iso-8859-1");
}

/* ----------- CSI CSID ----------- */
void
INTL_SetCSIDocCSID (INTL_CharSetInfo c, int16 doc_csid)
{
	return;
}

int16
INTL_GetCSIDocCSID(INTL_CharSetInfo c)
{
	return 0;
}

unsigned char *INTL_ConvertLineWithoutAutoDetect(
    int16 fromcsid,
    int16 tocsid,
    unsigned char *pSrc,
    uint32 block_size)
{
	return NULL;
}

/* 
 * INTL_DocToWinCharSetID,
 * Based on DefaultDocCSID, it determines which Win CSID to use for Display
 */
int16 
INTL_DocToWinCharSetID(int16 csid)
{
    /* MOZ_FUNCTION_STUB; */
    return CS_FE_ASCII;
}

int16
INTL_DefaultDocCharSetID(iDocumentContext context)
{
	return CS_DEFAULT;
}

unsigned char *INTL_ConvMailToWinCharCode(
    iDocumentContext context,
    unsigned char *bit7buff,
    uint32 block_size
)
{
	return NULL;
}
