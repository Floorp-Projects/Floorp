/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgI18N_H_
#define _nsMsgI18N_H_

#include "nscore.h"
#include "msgCore.h"


#ifndef kMAX_CSNAME
#define kMAX_CSNAME 64
#endif

NS_MSG_BASE char      *nsMsgI18NEncodeMimePartIIStr(const char *header, const char *charset, PRBool bUseMime);
NS_MSG_BASE PRBool    nsMsgI18Nstateful_charset(const char *charset);
NS_MSG_BASE PRBool    nsMsgI18N7bit_data_part(const char *charset, const char *string, const PRUint32 size);
NS_MSG_BASE char      *nsMsgI18NGetAcceptLanguage(void); 

NS_MSG_BASE const char *msgCompHeaderInternalCharset(void);
NS_MSG_BASE const nsString& nsMsgI18NFileSystemCharset(void);

NS_MSG_BASE char * nsMsgI18NGetDefaultMailCharset(void);
NS_MSG_BASE nsresult ConvertFromUnicode(const nsString& aCharset, 
                                   const nsString& inString,
                                   char** outCString);
NS_MSG_BASE nsresult ConvertToUnicode(const nsString& aCharset, 
                                 const char* inCString, 
                                 nsString& outString);

NS_MSG_BASE nsresult nsMsgI18NDecodeMimePartIIStr(const nsString& header, nsString& charset, nsString& decodedString, PRBool eatContinuations=PR_TRUE);

NS_MSG_BASE const char *nsMsgI18NParseMetaCharset(nsFileSpec* fileSpec);

NS_MSG_BASE nsresult nsMsgI18NConvertToEntity(const nsString& inString, nsString* outString);

NS_MSG_BASE nsresult nsMsgI18NSaveAsCharset(const char* contentType, const char* charset, const PRUnichar* inString, char** outString);

#endif /* _nsMsgI18N_H_ */
