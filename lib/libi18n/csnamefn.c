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
/*	csnamefn.c	*/

#include "intlpriv.h"

extern csname2id_t csname2id_tbl[];

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_DOCINFO_1;
extern int XP_DOCINFO_2;
extern int XP_DOCINFO_3;
extern int XP_DOCINFO_4;

PUBLIC int16
INTL_CharSetNameToID(char	*charset)
{
	csname2id_t	*csn2idp;
	int16			csid;

			/* Parse the URL charset string for the charset ID.
			 * If no MIME Content-Type charset pararm., default.
			 * HTML specifies ASCII, but let user override cuz
			 * of prior conventions (i.e. Japan).
			 */
	csn2idp = csname2id_tbl;
	csid = csn2idp->cs_id;		/* 1st entry is default codeset ID	*/

	if (charset != NULL) {		/* Linear search for charset string */
		while (*(csn2idp->cs_name) != '\0') {
 			if (strcasecomp(charset, (char *)csn2idp->cs_name) == 0) {
 				return(csn2idp->cs_id);
 			}
 			csn2idp++;
		}
 	}
	return(csn2idp->cs_id);		/* last entry is CS_UNKNOWN	*/
}
PUBLIC unsigned char *INTL_CsidToCharsetNamePt(int16 csid)
{
	csname2id_t	*csn2idp;

	csid &= ~CS_AUTO;
	csn2idp = &csname2id_tbl[1];	/* First one is reserved, skip it. */
	csid &= 0xff;

	/* Linear search for charset string */
	while (*(csn2idp->cs_name) != '\0') {
		if ((csn2idp->cs_id & 0xff) == csid)
			return csn2idp->cs_name;
 		csn2idp++;
	}
	return (unsigned char *)"";
}

PUBLIC unsigned char *INTL_CsidToJavaCharsetNamePt(int16 csid)
{
	csname2id_t	*csn2idp;

	csn2idp = &csname2id_tbl[1];	/* First one is reserved, skip it. */
	csid &= 0xff;

	/* Linear search for charset string */
	while (*(csn2idp->cs_name) != '\0') {
		if ((csn2idp->cs_id & 0xff) == csid)
			return csn2idp->java_name;
 		csn2idp++;
	}
	return (unsigned char *)"";
}


PUBLIC void 
INTL_CharSetIDToName(int16 csid, char  *charset)
{
	if (charset) {	
		strcpy(charset,(char *)INTL_CsidToCharsetNamePt(csid));
	}
}

PUBLIC void 
INTL_CharSetIDToJavaName(int16 csid, char  *charset)
{
	if (charset) {	
		strcpy(charset,(char *)INTL_CsidToJavaCharsetNamePt(csid));
	}
}

PUBLIC const char* PR_CALLBACK 
INTL_CharSetIDToJavaCharSetName(int16 csid) {
	return (char *)INTL_CsidToJavaCharsetNamePt(csid);
}

PUBLIC char *
INTL_CharSetDocInfo(iDocumentContext context)
{
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	register int16			doc_csid = INTL_GetCSIDocCSID(c);
	register csname2id_t	*csn2idp;
	char					*s = NULL;
	int						detected = 0;

	if (doc_csid == CS_DEFAULT) {
		doc_csid = INTL_DefaultDocCharSetID(context) & ~CS_AUTO;	/* Get CSID from prefs	*/
	} else if (doc_csid & CS_AUTO) {
		doc_csid &= ~CS_AUTO;				/* mask off bit for name lookup */
		detected = 1;
	} else {
		StrAllocCopy(s, INTL_GetCSIMimeCharset(c));	/* string from MIME header */

		if (doc_csid == CS_UNKNOWN)
				StrAllocCat(s, XP_GetString(XP_DOCINFO_1));
		return(s);
	}
			/* Look up name for default & autodetected CSIDs		*/
#if defined(XP_WIN) || defined(XP_OS2)
	csn2idp = &csname2id_tbl[1] ;	/* skip first default one	*/
	for (; *(csn2idp->cs_name) != '\0'; csn2idp++)
#else
	for (csn2idp = csname2id_tbl; *(csn2idp->cs_name) != '\0'; csn2idp++)
#endif
	{
 		if (doc_csid == csn2idp->cs_id) {
			StrAllocCopy(s, (char *)csn2idp->cs_name);
			if (detected)
				StrAllocCat(s, XP_GetString(XP_DOCINFO_2));
			else
				StrAllocCat(s, XP_GetString(XP_DOCINFO_3));
			return(s);
 		}
	}
	StrAllocCopy(s, INTL_GetCSIMimeCharset(c));	/* string from MIME header */
	StrAllocCat(s, XP_GetString(XP_DOCINFO_4));
	return (s);
}




#if defined(XP_WIN) || defined(XP_OS2)


/*
 This routine will change the default URL charset to
 newCharset, BTW newCharset is passed from UI.
*/
void
FE_ChangeURLCharset(const char *charset)
{
	csname2id_t	*csn2idp;
	char			*cp;

	if (charset == NULL)
		return;

	csn2idp = csname2id_tbl;

	cp = (char *)charset;
	if (cp)
		while (*cp != '\0') {
			*cp = tolower(*cp);
			cp++;
    	}

	while (*(csn2idp->cs_name) != '\0') {
 		if (strcasecomp(charset, (char *)csn2idp->cs_name) == 0) {
				INTL_ChangeDefaultCharSetID(csn2idp->cs_id);
 				return;
 			}
 			csn2idp++;
		}
}

void
INTL_ChangeDefaultCharSetID(int16 csid)
{
	csname2id_tbl[0].cs_id = csid;
}

#endif /* XP_WIN */



