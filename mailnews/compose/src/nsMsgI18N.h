/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsMsgI18N_H_
#define _nsMsgI18N_H_

#include "nsMsgTransition.h"

NS_BEGIN_EXTERN_C

char      *nsMsgI18NEncodeMimePartIIStr(const char *header, const char *charset, PRBool bUseMime);
PRBool    nsMsgI18Nstateful_charset(const char *charset);
PRBool    nsMsgI18N7bit_data_part(const char *charset, const char *string, const PRUint32 size);
char      *nsMsgI18NGetAcceptLanguage(void); 

const char *msgCompHeaderInternalCharset(void);

char * nsMsgI18NGetDefaultMailCharset(void);
nsresult ConvertFromUnicode(const nsString& aCharset, 
                                   const nsString& inString,
                                   char** outCString);
nsresult ConvertToUnicode(const nsString& aCharset, 
                                 const char* inCString, 
                                 nsString& outString);

nsresult nsMsgI18NDecodeMimePartIIStr(const nsString& header, nsString& charset, nsString& decodedString);

const char *nsMsgI18NParseMetaCharset(nsFileSpec* fileSpec);

//
// THIS IS BAD STUFF...MAKE IT GO AWAY!!!
//
#include "intl_csi.h"

void				nsMsgI18NDestroyCharCodeConverter(CCCDataObject);
unsigned char *		nsMsgI18NCallCharCodeConverter(CCCDataObject,const unsigned char *,int32);
int					nsMsgI18NGetCharCodeConverter(int16 ,int16 ,CCCDataObject);
CCCDataObject		nsMsgI18NCreateCharCodeConverter();
int16				nsMsgI18NGetCSIWinCSID(INTL_CharSetInfo);
INTL_CharSetInfo	LO_GetDocumentCharacterSetInfo(MWContext *);
int16				nsMsgI18NGetCSIDocCSID(INTL_CharSetInfo obj);
int16				nsMsgI18NDefaultWinCharSetID(MWContext *);
int16				nsMsgI18NDefaultMailCharSetID(int16 csid);
int16				nsMsgI18NDefaultNewsCharSetID(int16 csid);
void				nsMsgI18NMessageSendToNews(XP_Bool toNews);
CCCDataObject nsMsgI18NCreateDocToMailConverter(iDocumentContext context, XP_Bool isHTML, unsigned char *buffer, 
                                            uint32 buffer_size);


NS_END_EXTERN_C

#endif /* _nsMsgI18N_H_ */
