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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
*/

#ifndef NS_IMAPUTILS_H
#define NS_IMAPUTILS_H

#include "nsFileSpec.h"
#include "nsString.h"
#include "nsIMsgIncomingServer.h"

static const char kImapRootURI[] = "imap:/";
static const char kImapMessageRootURI[] = "imap_message:/";

extern nsresult
nsImapURI2Path(const char* rootURI, const char* uriStr, 
               nsFileSpec& pathResult);

extern nsresult
nsImapURI2FullName(const char* rootURI, const char* hostname, const char* uriStr,
                   char **name);

extern nsresult
nsParseImapMessageURI(const char* uri, nsCString& folderURI, PRUint32 *key, char **part);

extern nsresult 
nsBuildImapMessageURI(const char *baseURI, PRUint32 key, nsCString& uri);

extern nsresult
nsCreateImapBaseMessageURI(const char *baseURI, char **baseMessageURI);

void AllocateImapUidString(PRUint32 *msgUids, PRUint32 msgCount, nsCString &returnString);

char*
CreateUtf7ConvertedString(const char * aSourceString, 
                      PRBool aConvertToUtf7Imap);

nsresult CreateUnicodeStringFromUtf7(const char *aSourceString, PRUnichar **result);

char *
CreateUtf7ConvertedStringFromUnicode(const PRUnichar *aSourceString);

#endif //NS_IMAPUTILS_H
