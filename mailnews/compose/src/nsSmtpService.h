/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsSmtpService_h___
#define nsSmtpService_h___

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsISmtpService.h"
#include "nsISmtpServer.h"
#include "nsIProtocolHandler.h"

////////////////////////////////////////////////////////////////////////////////////////
// The Smtp Service is an interfaced designed to make building and running mail to urls
// easier. I'm not sure if this service will go away when the new networking model comes
// on line (as part of the N2 project). So I reserve the right to change my mind and take
// this service away =).
////////////////////////////////////////////////////////////////////////////////////////

class nsSmtpService : public nsISmtpService, public nsIProtocolHandler
{
public:

	nsSmtpService();
	virtual ~nsSmtpService();
	
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////
	// we suppport the nsISmtpService interface 
	////////////////////////////////////////////////////////////////////////    
	NS_DECL_NSISMTPSERVICE

	//////////////////////////////////////////////////////////////////////////
	// we suppport the nsIProtocolHandler interface 
	//////////////////////////////////////////////////////////////////////////
    NS_DECL_NSIPROTOCOLHANDLER

protected:
    nsresult loadSmtpServers();

    
private:
    nsCOMPtr<nsISupportsArray> mSmtpServers;
    nsCOMPtr<nsISmtpServer> mDefaultSmtpServer;
};

#endif /* nsSmtpService_h___ */
