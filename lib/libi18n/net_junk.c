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
/*	net_junk.c	*/

/*#include "intlpriv.h"*/
#include "xp.h"
#include "intl_csi.h"
#include "libi18n.h"
#include "net_junk.h"
#include "libmocha.h"


MODULE_PRIVATE int16	PeekMetaCharsetTag (char *, uint32);
MODULE_PRIVATE int16	DetectUCS2 (CCCDataObject, unsigned char *, int32);

typedef struct {
	Stream *current_stream;
	Stream *next_stream;
	CCCDataObject obj;
} NetStreamData;

extern unsigned char *One2OneCCC(CCCDataObject,unsigned char *,int32);
PRIVATE int net_AutoCharCodeConv (NET_StreamClass *stream, const char *s, int32 l);
PRIVATE int net_1to1CCC (NET_StreamClass *stream, const unsigned char *s, int32 l);

PRIVATE void net_CvtCharCodeComplete (NET_StreamClass *stream)
{
	NetStreamData *nsd=stream->data_object;
	unsigned char *uncvtbuf;	

	uncvtbuf = INTL_GetCCCUncvtbuf(nsd->obj);

	/* pass downstream any uncoverted characters */
	if (uncvtbuf[0] != '\0')
		(*nsd->next_stream->put_block)(nsd->next_stream,
		(const char *)uncvtbuf, strlen((char *)uncvtbuf));

	(*nsd->next_stream->complete)(nsd->next_stream);

	XP_FREE(nsd->next_stream);
    XP_FREE(nsd->obj);
    XP_FREE(nsd);
    return;
}

PRIVATE void net_CvtCharCodeAbort (NET_StreamClass *stream, int status)
{
	NetStreamData *nsd=stream->data_object;	
	(*nsd->next_stream->abort)(nsd->next_stream, status);

	XP_FREE(nsd->next_stream);
    XP_FREE(nsd->obj);
    XP_FREE(nsd);

    return;
}

PRIVATE int
net_CharCodeConv(	NET_StreamClass *stream,
					const unsigned char	*buf,	/* buffer for conversion	*/
					int32				bufsz)	/* buffer size in bytes		*/
{
	NetStreamData *nsd=stream->data_object;
	unsigned char	*tobuf;
	int				rv;
	CCCFunc cvtfunc;	

	cvtfunc = INTL_GetCCCCvtfunc(nsd->obj);

	tobuf = (unsigned char *)cvtfunc(nsd->obj, buf, bufsz);

	if (tobuf) {
		rv = (*nsd->next_stream->put_block) (nsd->next_stream,
						(const char *)tobuf, INTL_GetCCCLen(nsd->obj));
		if (tobuf != buf)
			XP_FREE(tobuf);
	    return(rv);
	} else {
		return(INTL_GetCCCRetval(nsd->obj));
	}
}

	/* Null Char Code Conversion module -- pass unconverted data downstream	*/
/* PRIVATE */ int
net_NoCharCodeConv (NET_StreamClass *stream, const char *s, int32 l)
{
	NetStreamData*nsd=stream->data_object;	
	return((*nsd->next_stream->put_block)(nsd->next_stream,s,l));
}

PRIVATE int
net_AutoCharCodeConv (NET_StreamClass *stream, const char *s, int32 l)
{
	NetStreamData*nsd=stream->data_object;
	int16	doc_csid;
	unsigned char	*tobuf = NULL;
	int				rv;
	CCCFunc cvtfunc;

	cvtfunc = INTL_GetCCCCvtfunc(nsd->obj);

/* for debugging -- erik */
#if 0
	{
		static FILE *f = NULL;

		if (!f)
		{
			f = fopen("/tmp/zzz", "w");
		}

		if (f && s && (l > 0))
		{
			(void) fwrite(s, 1, l, f);
		}
	}
#endif /* 0 */

	if (cvtfunc != NULL)
		tobuf = (unsigned char *)cvtfunc(nsd->obj, (unsigned char *)s, l);
	else
	{
		/* Look at the first block and see if we determine
		 * what the charset is from that block.
		 */

		/*  Somehow NET_PlainTextConverter() put a "<plaintext>" in the
			first block. Try to bypass that block
			We need this so we can detect UCS2 for the NT UCS2 plantext
		*/
		if((l == 11) && (strncmp(s, "<plaintext>", 11)==0))
		{
			return((*nsd->next_stream->put_block)(nsd->next_stream,s,l));
		} 
		/* check for unicode (ucs2) */
		doc_csid = DetectUCS2(nsd->obj, (unsigned char *)s, l);
	 	if(doc_csid == CS_DEFAULT)
		{
			doc_csid = PeekMetaCharsetTag((char *)s, l) ;
			if (doc_csid == CS_ASCII) /* the header said ascii. */
			{
				nsd->current_stream->put_block = (MKStreamWriteFunc) net_NoCharCodeConv;
				return((*nsd->next_stream->put_block)(nsd->next_stream,s,l));
			}
		}

		/* We looked at the first block but did not determine
		 * what the charset is.  Install the default converter
		 * now.  It could be a standard or an auto-detecting converter.
		 */
		if (doc_csid != CS_DEFAULT)
		{
			(void) INTL_GetCharCodeConverter(doc_csid, 0, nsd->obj);
			INTL_CallCCCReportAutoDetect(nsd->obj, doc_csid);
		}
		else
			(void) INTL_GetCharCodeConverter(INTL_GetCCCDefaultCSID(nsd->obj),0,nsd->obj);
		cvtfunc = INTL_GetCCCCvtfunc(nsd->obj);

		/* If no conversion needed, change put_block module for successive
		 * data blocks.  For current data block, return unmodified buffer.
		 */
		if (cvtfunc == NULL) 
		{
			return((*nsd->next_stream->put_block)(nsd->next_stream,s,l));
		}

		/* For initial block, must call converter directly.  Success calls
		 * to the converter will be called directly from net_CharCodeConv()
		 */
	}

	if (tobuf == NULL)
		tobuf = (unsigned char *)cvtfunc(nsd->obj, (unsigned char *)s, l);

	if (tobuf) {
		rv = (*nsd->next_stream->put_block) (nsd->next_stream,
			(const char *)tobuf, INTL_GetCCCLen(nsd->obj));
		if (tobuf != (unsigned char*)s)
			XP_FREE(tobuf);
    	return(rv);
	} else {
		return(INTL_GetCCCRetval(nsd->obj));
	}
}

	/* One-byte-to-one-byte Char Code Conversion module.
	 * Table driven.  Table provided by FE.
	 */
PRIVATE int
net_1to1CCC (NET_StreamClass *stream, const unsigned char *s, int32 l)
{
	NetStreamData *nsd=stream->data_object;	
	(void) One2OneCCC (nsd->obj, (unsigned char *)s, l);
	return((*nsd->next_stream->put_block)(nsd->next_stream,
								(const char *)s, INTL_GetCCCLen(nsd->obj)));
}


/*
 * We are always ready for writing, but the next stream might not
 *   be so, since we aren't willing to buffer, tell netlib the
 *   next stream's buffer size
 */
PRIVATE unsigned int net_CvtCharCodeWriteReady (NET_StreamClass *stream)
{
	NetStreamData *nsd=stream->data_object;	
    return ((*nsd->next_stream->is_write_ready)(nsd->next_stream));
}

PRIVATE void
net_report_autodetect(void *closure, CCCDataObject obj, uint16 doc_csid)
{
	NetStreamData *nsd = (NetStreamData *)closure;
	iDocumentContext  doc_context = nsd->current_stream->window_id;
	CCCFunc cvtfunc = INTL_GetCCCCvtfunc(obj);
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(doc_context);

	INTL_SetCSIDocCSID(c, doc_csid);
	/* I hope it is okay, to set the win_csid */
	INTL_SetCSIWinCSID(c, INTL_GetCCCToCSID(obj));
	if (cvtfunc == NULL)
		nsd->current_stream->put_block = (MKStreamWriteFunc) net_NoCharCodeConv;
}

PUBLIC Stream *
INTL_ConvCharCode (int         format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *mwcontext)
{
	NetStreamData *nsd;
    CCCDataObject	obj;
    Stream *stream;
	iDocumentContext doc_context = (iDocumentContext)mwcontext;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(doc_context);
	XP_Bool is_metacharset_reload;
	uint16 default_doc_csid = INTL_DefaultDocCharSetID(mwcontext);
/*
    Should this be ?
	uint16 default_doc_csid = FE_DefaultDocCharSetID(mwcontext);
*/


    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    stream = XP_NEW_ZAP(Stream);
    if(stream == NULL)
        return(NULL);

    stream->name           = "CharCodeConverter";
    stream->complete       = (MKStreamCompleteFunc) net_CvtCharCodeComplete;
    stream->abort          = (MKStreamAbortFunc) net_CvtCharCodeAbort;

    stream->is_write_ready = (MKStreamWriteReadyFunc) net_CvtCharCodeWriteReady;
    stream->window_id      = doc_context;

	/* initialize the doc_csid (etc.) unless if this is a reload caused by meta charset */
	if ((NET_RESIZE_RELOAD == URL_s->resize_reload) 
		&& (METACHARSET_FORCERELAYOUT == INTL_GetCSIRelayoutFlag(c)))
		is_metacharset_reload = TRUE;
	else
		is_metacharset_reload = FALSE;

	INTL_CSIInitialize(c, is_metacharset_reload, URL_s->charset,
						mwcontext->type, default_doc_csid);

	obj = INTL_CreateDocumentCCC(c, default_doc_csid);
    if (obj == NULL) {
		XP_FREE(stream);
		return(NULL);
	}

    nsd = XP_NEW_ZAP(NetStreamData);
    if(nsd == NULL) {
		XP_FREE(stream);
		XP_FREE(obj);
		return(NULL);
	}
	nsd->current_stream = stream;
	nsd->obj = obj;
    stream->data_object = nsd;  /* document info object */
	INTL_SetCCCReportAutoDetect(obj, net_report_autodetect, nsd);


	if (INTL_GetCSIDocCSID(c) == CS_DEFAULT || INTL_GetCSIDocCSID(c) == CS_UNKNOWN)
	{
		/* we know the default converter but do not install it yet. 
		 * Instead wait until the first block and see if we can determine
		 * what the actual charset is from http/meta tags or from the 
		 * first block. By delaying we can avoid a reload if
		 * we get a different charset from http/meta tag or the first block.
		 */
		stream->put_block	= (MKStreamWriteFunc) net_AutoCharCodeConv;
	}
	else
	{
		if (INTL_GetCCCCvtfunc(obj) == NULL)
			stream->put_block	= (MKStreamWriteFunc) net_NoCharCodeConv;
    	else if (INTL_GetCCCCvtfunc(obj) == (CCCFunc)One2OneCCC)
    		stream->put_block	= (MKStreamWriteFunc) net_1to1CCC;
    	else
    		stream->put_block	= (MKStreamWriteFunc) net_CharCodeConv;
	}

    TRACEMSG(("Returning stream from NET_CvtCharCodeConverter\n"));
	
	/* remap content type to be to INTERNAL_PARSER
 	 */
	StrAllocCopy(URL_s->content_type, INTERNAL_PARSER);

#ifdef JSDEBUGGER
	nsd->next_stream = LM_StreamBuilder(format_out, NULL, URL_s, mwcontext);
#else
	nsd->next_stream = NET_StreamBuilder(format_out, URL_s, doc_context);
#endif /* JSDEBUGGER */

    if(!nsd->next_stream)
	  {
		XP_FREE(obj);
		XP_FREE(stream);
		XP_FREE(nsd);
		return(NULL);
	  }

    return stream;
}

