/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsPop3Service_h___
#define nsPop3Service_h___

#include "nscore.h"

#include "nsIPop3Service.h"
#include "nsIPop3URL.h"
#include "nsIUrlListener.h"
#include "nsIStreamListener.h"
#include "nsFileSpec.h"
#include "nsIProtocolHandler.h"
#include "nsIMsgProtocolInfo.h"


class nsPop3Service : public nsIPop3Service,
                      public nsIProtocolHandler,
                      public nsIMsgProtocolInfo
{
public:

	nsPop3Service();
	virtual ~nsPop3Service();
	
	NS_DECL_ISUPPORTS
    NS_DECL_NSIPOP3SERVICE
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIMSGPROTOCOLINFO

protected:
	// convience function to make constructing of the pop3 url easier...
	nsresult BuildPop3Url(char * urlSpec, nsIMsgFolder *inbox,
                          nsIPop3IncomingServer *, nsIUrlListener * aUrlListener,
                          nsIURI ** aUrl, nsIMsgWindow *aMsgWindow, PRInt32 popPort);

	nsresult RunPopUrl(nsIMsgIncomingServer * aServer, nsIURI * aUrlToRun);
};

#endif /* nsPop3Service_h___ */
