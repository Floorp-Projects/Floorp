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

#ifndef NS_LOCALUTILS_H
#define NS_LOCALUTILS_H

#include "nsFileSpec.h"
#include "nsString.h"
#include "nsIMsgIncomingServer.h"

static const char kMailboxRootURI[] = "mailbox:/";
static const char kMailboxMessageRootURI[] = "mailbox_message:/";

nsresult
nsLocalURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult);

nsresult
nsParseLocalMessageURI(const char* uri, nsCString& folderURI, PRUint32 *key);

nsresult 
nsBuildLocalMessageURI(const char* baseURI, PRUint32 key, nsCString& uri);

nsresult
nsCreateLocalBaseMessageURI(const char *baseURI, char **baseMessageURI);

#endif //NS_LOCALUTILS_H
