/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*	cvchcode.c	*/

#include "intlpriv.h"
#include "xp.h"
#include "intl_csi.h"
#include "libi18n.h"

int16  PeekMetaCharsetTag (char *, uint32);

/*
 * DocumentContext access to creating a
 * Character Code Converter
 *
 * This should be split into two layers:
 *  1) a non-DocumentContext access to creating a Character Code Converter
 *  2) a DocumentContext interface to above
 */
PUBLIC CCCDataObject
INTL_CreateDocumentCCC(INTL_CharSetInfo c, uint16 default_doc_csid)
{
    CCCDataObject obj;

    /* 
	 * create the CCC object
	 */
	obj = INTL_CreateCharCodeConverter();;
    if (obj == NULL)
        return(NULL);
	INTL_SetCCCDefaultCSID(obj, default_doc_csid);

	/*
	 * if we know the doc_csid then get the converter
	 */
	if (DOC_CSID_KNOWN(INTL_GetCSIDocCSID(c)))
	{
		(void) INTL_GetCharCodeConverter(INTL_GetCSIDocCSID(c), 0, obj);
		XP_ASSERT(INTL_GetCSIWinCSID(c) == INTL_GetCCCToCSID(obj));
	}
	else {
		/* we know what the default converter is but do not install
		 * it yet. Wait until the first block and see if we determine
		 * what the charset is from that block.
		 */
	}

    return obj;
}

PUBLIC void INTL_CCCReportMetaCharsetTag(MWContext *context, char *charset_tag)
{

	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

	INTL_CSIReportMetaCharsetTag(c, charset_tag, context->type);
}

#ifdef MOZ_MAIL_NEWS

#if 0
/* 
 INTL_ConvWinToMailCharCode
   Converts 8bit encoding to 7bit mail encoding. It decides which 7bit and 8bit encoding 
   to use based on current default language.
 input:
   char *pSrc;  			// Source display buffer
 output:
   char *pDest; 			//  Destination buffer
                            // pDest == NULL means either conversion fail
                            // or does OneToOne conversion
*/
PUBLIC
unsigned char *INTL_ConvWinToMailCharCode(iDocumentContext context, unsigned char *pSrc, uint32 block_size)
{
    CCCDataObject	obj;
	unsigned char *pDest;
	int16 wincsid;
	CCCFunc cvtfunc;

    obj = INTL_CreateCharCodeConverter();
	if (obj == NULL)
		return NULL;

	wincsid =  INTL_DefaultWinCharSetID(context);
	/* Converts 8bit Window codeset to 7bit Mail Codeset.   */
	(void) INTL_GetCharCodeConverter(wincsid, INTL_DefaultMailCharSetID(wincsid), obj);

	cvtfunc = INTL_GetCCCCvtfunc(obj);
	if (cvtfunc)
		pDest = (unsigned char *)cvtfunc(obj, pSrc, block_size);
	else
		pDest = NULL ;

	XP_FREE(obj);

	if (pDest == pSrc)  /* converted to input buffer           */
	{                           /* no additional memory been allocated */
		return NULL;
	}
	return pDest ;
}


#endif /* if 0 */


PRIVATE void
mail_report_autodetect(void *closure, CCCDataObject obj, uint16 doc_csid)
{
	iDocumentContext doc_context = (iDocumentContext)closure;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(doc_context);

	INTL_SetCSIDocCSID(c, doc_csid);
	INTL_SetCSIWinCSID(c, INTL_GetCCCToCSID(obj));
}

#endif  /* MOZ_MAIL_NEWS */

#ifndef XP_WIN
#ifndef XP_MAC
/*
 * FE_DefaultDocCharSetID - get the UI charset encoding setting
 *
 * gets the currently selected charset encoding for this document 
 * (not the global default, not the detected document encoding)
 *
 */
uint16
FE_DefaultDocCharSetID(iDocumentContext context)
{
	uint16 csid;

	/* need to implement a FE_DefaultDocCharSetID() which only returns
	   the FE default, ignoring the doc_csid in the the context. 
	   The Current INTL_DefaultDocCharSetID
	   tries to be smart and return the doc_csid if it is known, 
	   which causes a problem in HERE !!
	*/
#ifdef REAL_FIX_AFTER_B3
	/* ftang, bstell: Need to change to this code in early 4.0 B4 cycle */
	csid = FE_DefaultDocCharSetID(context);	  /* Need to implement FE_DefaultDocCharSetID before active this */

	if((CS_JIS == csid) || (CS_EUCJP == csid) || (CS_SJIS == csid) || 
		(CS_KSC_8BIT == csid) ||   (CS_2022_KR == csid))
	{
		csid |= CS_AUTO;
	}
#else /* 4.0 B3 temp fix */
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

	csid = INTL_GetCSIDocCSID(c);
	if (csid == 0) {
		csid = INTL_DefaultDocCharSetID(context);
	}
#ifdef MOZ_MAIL_NEWS
	else
	{
		csid = INTL_DefaultMailCharSetID(csid);
		/* Turn on the auto converter in case people do receive SJIS mail */
		if((CS_JIS == csid) || (CS_EUCJP == csid) || (CS_SJIS == csid) || 
			(CS_KSC_8BIT == csid) ||   (CS_2022_KR == csid))
		{
			csid |= CS_AUTO;
		}
	}
#endif  /* MOZ_MAIL_NEWS */
#endif
	return csid;
}
#endif /*XP_MAC*/
#endif 

#ifdef MOZ_MAIL_NEWS

/* 
 INTL_ConvMailToWinCharCode
   Converts mail encoding to display charset which is used by current window. 
   It decides which Display charset to use based on current default language.
 input:
   iDocumentContext context;     the context (window ID)
   char *pSrc;  		// Source buffer
   uint32 block_size;      the length of the source buffer
 output:
   char *pDest; 		//  Destination buffer
                        // pDest == NULL means either conversion fail
                        // or does OneToOne conversion
*/
PUBLIC
unsigned char *INTL_ConvMailToWinCharCode(iDocumentContext context,
	unsigned char *pSrc, uint32 block_size)
{
    CCCDataObject	obj;
	unsigned char *pDest;
	Stream stream;
	CCCFunc cvtfunc;
	int16 csid;

    obj = INTL_CreateCharCodeConverter();
	if (obj == NULL)
		return 0;

	(void) memset(&stream, 0, sizeof(stream));
	stream.window_id = context;

	csid = FE_DefaultDocCharSetID(context);

	/* Converts 7bit news codeset to 8bit windows Codeset.   */
	(void) INTL_GetCharCodeConverter(csid, 0, obj);


	INTL_SetCCCReportAutoDetect(obj, mail_report_autodetect, context );

	cvtfunc = INTL_GetCCCCvtfunc(obj);
	if (cvtfunc)
		pDest = (unsigned char *)cvtfunc(obj, pSrc, block_size);
	else
		pDest = NULL ;

	XP_FREE(obj);
	if (pSrc == pDest)
		return NULL ;
	return pDest ;
}
#endif /* MOZ_MAIL_NEWS */


#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
/*
	This is the ugly hack for Korean News and Mail
	Our libmsg code assume mail and news use the same code.
	Unfortunately, Korean use different encoding in news and mail
	We have to tell the DocToMailCoverter convert to different encoding
	according to the reciver.

	Problem 1:
	It is easy to decide which encoding we should send if the receipt is
	only Newsgroup or only email. But what should we do if the receipt 
	include both newsgroup and personal address ?
	
		Currently we always send News encoding if the recipt include
	Newsgroup.

	Problem 2:
	There are no way I can pass such information to the code between
	msgsendp.cpp and msgsend.cpp. So I create such a hack here. 

*/
static XP_Bool intl_message_to_newsgroup = FALSE;
PUBLIC
void
INTL_MessageSendToNews(XP_Bool toNews)
{
	intl_message_to_newsgroup = toNews;
}



PUBLIC
CCCDataObject
INTL_CreateDocToMailConverter(iDocumentContext context, XP_Bool isHTML, unsigned char *buffer, uint32 buffer_size)
{
      CCCDataObject selfObj;
      int16 p_doc_csid = CS_DEFAULT;
	  CCCFunc cvtfunc;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

      /* Ok!! let's create the object here
         We need the conversion obj to keep the state information */
      selfObj = INTL_CreateCharCodeConverter();
      if(selfObj == NULL)
           return NULL;

              /* First, let's determine the from_csid and to_csid */
              /* It is a TEXT_HTML !!! Let's use our PeekMetaCharsetTag to get a csid */
      if(isHTML)
               p_doc_csid = PeekMetaCharsetTag((char*) buffer, buffer_size);

      if(p_doc_csid == CS_DEFAULT)
      {
              /* got default, try to get the doc_csid from context */
              if((context == 0 ) || (INTL_GetCSIDocCSID(c) == CS_DEFAULT))
                      p_doc_csid = INTL_DefaultDocCharSetID(context);
              else
                      p_doc_csid = INTL_GetCSIDocCSID(c);
#ifdef XP_MAC   
			/* To Make Macintosh happe when there is a HTML Mail composer send
			   out plain text file. It put the MacRoman text into the temp file and then let this 
			   function load it. Because I change the mac version of INTL_DefaultDocCharSetID() so now
			   it listen to the front window to tell me which doc csid it is. It is Latin1 in the
			   case email. But the data is really MacRoman. So we add this hack here. Which only change
			   the doccsid if it is not HTML
			 */ 
			                  
			if(! isHTML)
			{
				switch(p_doc_csid)
				{
					case CS_LATIN1:
						p_doc_csid = CS_MAC_ROMAN; 
						break;
					case CS_LATIN2:
						p_doc_csid = CS_MAC_CE; 
						break;
					case CS_8859_5:
					case CS_KOI8_R:
						p_doc_csid = CS_MAC_CYRILLIC; 
						break;
					case CS_8859_7:
						p_doc_csid = CS_MAC_GREEK; 
						break;
					case CS_8859_9:
						p_doc_csid = CS_MAC_TURKISH; 
						break;
					default:
						break;
				}
			} 
#endif

              /* The doc_csid from the context (or default) has CS_AUTO bit. */
              /* So let's try to call the auto detection function */
			  /* ftang add this: The CS_AUTO is still buggy, */
              if(
				  (CS_JIS == p_doc_csid) || (CS_SJIS == p_doc_csid) || (CS_EUCJP == p_doc_csid) ||
				  (CS_KSC_8BIT == p_doc_csid) || (CS_2022_KR == p_doc_csid)  ||
				  (CS_JIS_AUTO == p_doc_csid) || (CS_SJIS_AUTO == p_doc_csid) || (CS_EUCJP_AUTO == p_doc_csid) ||
				  (CS_KSC_8BIT_AUTO == p_doc_csid) /* || (CS_2022_KR_AUTO == p_doc_csid) */				  
				)
			  {
				 uint16 default_csid = INTL_DefaultDocCharSetID(context);

			     switch(p_doc_csid & ~CS_AUTO)
				 {
					 case CS_JIS:
					 case CS_SJIS:
					 case CS_EUCJP:
						  p_doc_csid = intl_detect_JCSID((uint16)(default_csid&~CS_AUTO), buffer, buffer_size);
						  break;
					 case CS_KSC_8BIT:
					 case CS_2022_KR:
						  p_doc_csid = intl_detect_KCSID((uint16)(default_csid&~CS_AUTO), buffer, buffer_size);
						  break;
					 /* Probably need to take care UCS2 / UTF8 and UTF7 here */
					 case CS_BIG5:
					 case CS_CNS_8BIT:
					 case CS_UTF8:
					 case CS_UTF7:
					 case CS_UCS2:
					 case CS_UCS2_SWAP:
					 default:
						 XP_ASSERT(FALSE);
				 }
			  }
      }
      /* Now, we get the converter */
      (void) INTL_GetCharCodeConverter(p_doc_csid, 
#ifdef MOZ_MAIL_NEWS
                                       (intl_message_to_newsgroup ? 
                                        INTL_DefaultNewsCharSetID(p_doc_csid) :
                                        INTL_DefaultMailCharSetID(p_doc_csid)), 
#else /* must be MOZ_MAIL_COMPOSE */
                                        INTL_DefaultMailCharSetID(p_doc_csid), 
#endif /* MOZ_MAIL_NEWS */
                                       selfObj);
      /* If the cvtfunc == NULL, we don't need to do conversion */
	  cvtfunc = INTL_GetCCCCvtfunc(selfObj);
      if(! (cvtfunc) )
      {
      		  XP_FREE(selfObj);
              return NULL;
      }
      else
              return selfObj;
}

#endif /* MOZ_MAIL_COMPOSE */
