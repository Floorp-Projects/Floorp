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
/*	intl_csi.c	International Character Set Information
 *
 * This file contains the INTL_CSI accessor functions
 *
 * This allows the i18n character set data to be
 * changed without (hopefully) changing all the
 * code that accesses it.
 *
 * UNFORTUNATELY, as of this date Oct. 28, 1996
 * there is not document context so we are still 
 * using the old MWContext.
 * When this is fix all that is needed should
 * be change the USE_REAL_DOCUMENT_CONTEXT flag 
 * and recompile (and test of course).
 */

#include "intlpriv.h"
#include "xp.h"
#include "libi18n.h"
#include "intl_csi.h"

struct OpaqueINTL_CharSetInfo {
	uint16 doc_csid;
	uint16 http_doc_csid;
	uint16 meta_doc_csid;
	uint16 override_doc_csid;
	uint16 win_csid;
	unsigned char *mime_charset;
	int relayout;
};

typedef enum
{
	OVERRIDE_CSID_TYPE,
	HTTP_CSID_TYPE,
	META_CSID_TYPE,
	DOC_CSID_TYPE
} CsidType;

PRIVATE uint16 get_charset_tag(char *charset_tag);

/*
 *
 *	Setup the character set info
 *
 */
void
INTL_CSIInitialize(INTL_CharSetInfo c, XP_Bool is_metacharset_reload, 
				   char *http_charset, int doc_type, uint16 default_doc_csid)
{
	uint16 http_doc_csid = get_charset_tag(http_charset);
	uint16 win_csid;

	XP_ASSERT(NULL != c);
	/*
	 * Initiliaze the Character-Set-Info (CSI)
	 * (unless if we are doing a meta_charset_reload)
	 */
	if (!is_metacharset_reload)
		INTL_CSIReset(c);

	/*
	 * Setup the CSI with the information we currently know
	 */

	/* first check if this is a metacharset reload */
	if (is_metacharset_reload)
	{
		INTL_SetCSIRelayoutFlag(c, METACHARSET_RELAYOUTDONE);
	}
	/* Check for a doc csid override */
	else if (DOC_CSID_KNOWN(INTL_GetCSIOverrideDocCSID(c)))
	{
		INTL_SetCSIDocCSID(c, INTL_GetCSIOverrideDocCSID(c));
	}
	/* if we have a HTTP charset tag then use it */
	else if (DOC_CSID_KNOWN(http_doc_csid))
	{
		INTL_SetCSIHTTPDocCSID(c, http_doc_csid);
	} 
	else /* normal case */
	{
		uint16 init_doc_csid = default_doc_csid;
		/*
		 * mail/news sometimes has the wrong meta charset
		 * so for mail/news we always ignore the meta charset tag
		 * which causes us to use the encoding the user set in the
		 * encoding menu
		 */
		if (!(MAIL_NEWS_TYPE(doc_type)))
			init_doc_csid = CS_DEFAULT;

#ifdef XP_UNIX
		/*
		 * The PostScript and Text FEs inherit the doc_csid from
		 * the parent context in order to pick up the per-window
		 * default. Apparently, this already works for some
		 * reason on Windows. Don't know about Mac. -- erik
		 */
		if ((doc_type != MWContextPostScript) &&
			(doc_type != MWContextText))
#endif /* XP_UNIX */
			INTL_SetCSIDocCSID(c, init_doc_csid /*CS_DEFAULT*/ );
	}

	/*
	 * alot of code (parsing/layout/FE) wants to know
	 * the encoding so we must set something
	 */
	if (DOC_CSID_KNOWN(INTL_GetCSIDocCSID(c)))
		win_csid = INTL_DocToWinCharSetID(INTL_GetCSIDocCSID(c));
	else
		win_csid = INTL_DocToWinCharSetID(default_doc_csid);
	INTL_SetCSIWinCSID(c, win_csid); /* true until we know otherwise */
}

PRIVATE void
set_csid(INTL_CharSetInfo c, uint16 csid, CsidType type)
{
	XP_ASSERT(NULL != c);

	if (OVERRIDE_CSID_TYPE == type)
	{
		c->override_doc_csid = csid;
		c->doc_csid = csid;
	}
	else if (HTTP_CSID_TYPE == type)
	{
		c->http_doc_csid = csid;
		if (CS_DEFAULT == c->override_doc_csid)
			c->doc_csid = csid;
	}
	else if (META_CSID_TYPE == type)
	{
		c->meta_doc_csid = csid;
		if (   (CS_DEFAULT == c->override_doc_csid)
			&& (CS_DEFAULT == c->http_doc_csid))
			c->doc_csid = csid;
	}
	else if (DOC_CSID_TYPE == type)
	{
		if (   (CS_DEFAULT == c->override_doc_csid)
			&& (CS_DEFAULT == c->http_doc_csid)
			&& (CS_DEFAULT == c->meta_doc_csid))
			c->doc_csid = csid;
	}
	else
	{
		XP_ASSERT(0);
	}
}

void
INTL_CSIReportMetaCharsetTag(INTL_CharSetInfo c, char *charset_tag, int type)
{
	int16 doc_csid;

	XP_ASSERT(charset_tag);
	doc_csid = INTL_CharSetNameToID(charset_tag);

	/* ignore invalid tags */
	if (doc_csid == CS_UNKNOWN)
		return;

	/* mail and news ignores meta charset tags since some are wrong */
	if (MAIL_NEWS_TYPE(type))
		return;

	/* only honor the first (meta or http) charset tag */
	if (INTL_GetCSIRelayoutFlag(c) == METACHARSET_NONE)
	{
		uint16 old_doc_csid = INTL_GetCSIDocCSID(c);

		INTL_SetCSIMetaDocCSID(c, doc_csid);
		/* ignore subsequent meta charset tags */
		INTL_SetCSIRelayoutFlag(c, METACHARSET_HASCHARSET);

		/* 
		 * if we already set up the converter wrong we have to reload
		 */
		if (DOC_CSID_KNOWN(old_doc_csid)
			&& ((old_doc_csid & ~CS_AUTO) != doc_csid))
			INTL_SetCSIRelayoutFlag(c, METACHARSET_REQUESTRELAYOUT);
		/*
		 * if we told the FE the wrong win_csid we have to reload
		 * (we had to tell the FE something so it could do layout
		 *  while we were looking for the metacharset tag)
		 */
		else if (INTL_DocToWinCharSetID(doc_csid) != INTL_GetCSIWinCSID(c))
			INTL_SetCSIRelayoutFlag(c, METACHARSET_REQUESTRELAYOUT);
	}
}

PRIVATE uint16 get_charset_tag(char *charset_tag)
{
	uint16 csid;

	/* validate the http_charset */
	if ((NULL == charset_tag) || ('\0' == *charset_tag))
		return CS_UNKNOWN;

	csid = INTL_CharSetNameToID(charset_tag);

	if (CS_DEFAULT == csid)
		csid = CS_UNKNOWN;

	return csid;
}


INTL_CharSetInfo 
LO_GetDocumentCharacterSetInfo(MWContext *context)
{
	XP_ASSERT(context);
	XP_ASSERT(INTL_TAG == context->INTL_tag);
	XP_ASSERT(NULL != context->INTL_CSIInfo);
	return(context->INTL_CSIInfo);
}


INTL_CharSetInfo
INTL_CSICreate(void)
{
	INTL_CharSetInfo c;

#if (defined(DEBUG_bstell) || defined(DEBUG_nhotta) || defined(DEBUG_ftang))
#define BSTELLS_FREE_TRICK_OFFSET 64
	/* I use (abuse) the malloc system to find improper frees */
	{
		char *p;
		p = (char *)XP_CALLOC(1, 
			BSTELLS_FREE_TRICK_OFFSET+sizeof(struct OpaqueINTL_CharSetInfo));
		c = (INTL_CharSetInfo)(p + BSTELLS_FREE_TRICK_OFFSET);
	}
#else
	c = (INTL_CharSetInfo)XP_CALLOC(1, sizeof(struct OpaqueINTL_CharSetInfo));
#endif
	return c;
}

void
INTL_CSIDestroy(INTL_CharSetInfo c)
{
	XP_ASSERT(c);
#if (defined(DEBUG_bstell) || defined(DEBUG_nhotta) || defined(DEBUG_ftang))
	{
		char *p = (char *)c;
		XP_FREE(p - BSTELLS_FREE_TRICK_OFFSET);
	}
#else
	XP_FREE(c);
#endif
}

void
INTL_CSIReset(INTL_CharSetInfo c)
{
	INTL_SetCSIOverrideDocCSID (c, CS_DEFAULT);
	INTL_SetCSIHTTPDocCSID     (c, CS_DEFAULT);
	INTL_SetCSIMetaDocCSID     (c, CS_DEFAULT);
	INTL_SetCSIDocCSID         (c, CS_DEFAULT);
	INTL_SetCSIWinCSID         (c, CS_DEFAULT);
	INTL_SetCSIMimeCharset(c, NULL);
	INTL_SetCSIRelayoutFlag(c, METACHARSET_NONE);
}

/* ----------- CSI CSID ----------- */
/*  Override is strongest. Must set doc_csid */
void
INTL_SetCSIOverrideDocCSID (INTL_CharSetInfo c, int16 doc_csid)
{
	set_csid(c, doc_csid, OVERRIDE_CSID_TYPE);
}

/*  Next strongest. Set doc_csid if no override */
void
INTL_SetCSIHTTPDocCSID (INTL_CharSetInfo c, int16 doc_csid)
{
	set_csid(c, doc_csid, HTTP_CSID_TYPE);
}

/*  Next strongest. Set doc_csid if no override */
void
INTL_SetCSIMetaDocCSID (INTL_CharSetInfo c, int16 doc_csid)
{
	set_csid(c, doc_csid, META_CSID_TYPE);
}

/*  Meta charset is weakest. Only set doc_csid if no http or override */
void
INTL_SetCSIDocCSID (INTL_CharSetInfo c, int16 doc_csid)
{
	set_csid(c, doc_csid, DOC_CSID_TYPE);
}

int16
INTL_GetCSIDocCSID(INTL_CharSetInfo c)
{
	XP_ASSERT(c);
	return c->doc_csid;
}

int16
INTL_GetCSIMetaDocCSID(INTL_CharSetInfo c)
{
	XP_ASSERT(c);
	return c->meta_doc_csid;
}

int16
INTL_GetCSIOverrideDocCSID(INTL_CharSetInfo c)
{
	XP_ASSERT(c);
	return c->override_doc_csid;
}

/* ----------- Window CSID ----------- */

void
INTL_SetCSIWinCSID(INTL_CharSetInfo c, int16 win_csid)
{
	XP_ASSERT(c);
	c->win_csid = win_csid;
}

int16
INTL_GetCSIWinCSID(INTL_CharSetInfo c)
{
	XP_ASSERT(c);
	return c->win_csid;
}

/* ----------- Mime CSID ----------- */
char *
INTL_GetCSIMimeCharset (INTL_CharSetInfo c)
{
	XP_ASSERT(c);
	return (char *)c->mime_charset;
}

void
INTL_SetCSIMimeCharset(INTL_CharSetInfo c, char *mime_charset)
{
	unsigned char *p;
	XP_ASSERT(c);

	if (c->mime_charset)
	{
		XP_FREE(c->mime_charset);
		c->mime_charset = NULL;
	}

	if (NULL == mime_charset)
		return;

	c->mime_charset = (unsigned char *)XP_STRDUP(mime_charset);
	if (NULL == c->mime_charset)
		return;

	/* Legitimate charset names must be in ASCII and case insensitive	*/
	/* convert to lower case */
	for (p=c->mime_charset; *p; p++)
		*p = tolower(*p);
	return;
}

/* ----------- Relayout Flag ----------- */
int16
INTL_GetCSIRelayoutFlag(INTL_CharSetInfo c)
{
	XP_ASSERT(c);
	return c->relayout;
}

void
INTL_SetCSIRelayoutFlag(INTL_CharSetInfo c, int16 relayout)
{
	XP_ASSERT(c);
	c->relayout = relayout;
}


