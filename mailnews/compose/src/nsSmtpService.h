/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
    static PRBool findServerByKey (nsISupports *element, void *aData);
    static PRBool findServerByHostname (nsISupports* element, void *aData);
    
    nsresult createKeyedServer(const char* key,
                               nsISmtpServer **aResult = nsnull);
    nsresult saveKeyList();
    
    nsCOMPtr<nsISupportsArray> mSmtpServers;
    nsCOMPtr<nsISmtpServer> mDefaultSmtpServer;
    nsCOMPtr<nsISmtpServer> mSessionDefaultServer;

    nsCAutoString mServerKeyList;

    PRBool mSmtpServersLoaded;
};

#endif /* nsSmtpService_h___ */
