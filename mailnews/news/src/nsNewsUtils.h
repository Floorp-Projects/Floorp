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

#ifndef NS_NEWSUTILS_H
#define NS_NEWSUTILS_H

#include "nsFileSpec.h"
#include "nsString.h"
#include "nsIMsgIncomingServer.h"

class nsIMsgNewsFolder;

static const char kNewsRootURI[] = "news:/";
static const char kNewsMessageRootURI[] = "news_message:/";

#define kNewsRootURILen 6
#define kNewsMessageRootURILen 14

extern nsresult
nsNewsURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult);

extern nsresult
nsParseNewsMessageURI(const char* uri, nsCString& messageUriWithoutKey, PRUint32 *key);

extern nsresult 
nsBuildNewsMessageURI(const char *baseURI, PRUint32 key, nsCString& uri);

extern nsresult
nsCreateNewsBaseMessageURI(const char *baseURI, char **baseMessageURI);

extern nsresult nsGetNewsGroupFromUri(const char *uri, nsIMsgNewsFolder **aFolder);

#endif //NS_NEWSUTILS_H

