/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsPop3Service_h___
#define nsPop3Service_h___

#include "nscore.h"

#include "nsIPop3Service.h"
#include "nsIPop3URL.h"
#include "nsIUrlListener.h"
#include "nsIStreamListener.h"
#include "nsFileSpec.h"
#include "nsIProtocolHandler.h"


class nsPop3Service : public nsIPop3Service, nsIProtocolHandler
{
public:

	nsPop3Service();
	virtual ~nsPop3Service();
	
	NS_DECL_ISUPPORTS
    NS_DECL_NSIPOP3SERVICE
    NS_DECL_NSIPROTOCOLHANDLER

protected:
	// convience function to make constructing of the pop3 url easier...
	nsresult BuildPop3Url(char * urlSpec, nsIMsgFolder *inbox,
                          nsIPop3IncomingServer *, nsIUrlListener * aUrlListener,
                          nsIURI ** aUrl);

	nsresult RunPopUrl(nsIMsgIncomingServer * aServer, nsIURI * aUrlToRun);
};

#endif /* nsPop3Service_h___ */
