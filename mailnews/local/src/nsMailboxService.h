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

#ifndef nsMailboxService_h___
#define nsMailboxService_h___

#include "nscore.h"
#include "nsISupports.h"

#include "nsIMailboxService.h"
#include "nsIMsgMessageService.h"
#include "nsIMailboxUrl.h"
#include "nsIURL.h"
#include "nsIUrlListener.h"
#include "nsIStreamListener.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsIProtocolHandler.h"

class nsMailboxService : public nsIMailboxService, public nsIMsgMessageService, public nsIProtocolHandler
{
public:

	nsMailboxService();
	virtual ~nsMailboxService();
	
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIMailboxService Interface 
	////////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD ParseMailbox(nsFileSpec& aMailboxPath, nsIStreamListener * aMailboxParser, 
							nsIUrlListener * aUrlListener, nsIURI ** aURL);
	

	NS_IMETHOD DisplayMessageNumber(const char *url,
                                    PRUint32 aMessageNumber,
                                    nsISupports * aDisplayConsumer,
									nsIUrlListener * aUrlListener,
                                    nsIURI ** aURL);

	////////////////////////////////////////////////////////////////////////////////////////
	// End suppport for the nsIMailboxService Interface
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIMsgMessageService Interface 
	////////////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopy, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIURI **aURL);

	NS_IMETHOD DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
							  nsIUrlListener * aUrlListener, nsIURI ** aURL);

	NS_IMETHOD SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, PRBool aAppendToFile, 
								 nsIUrlListener *aUrlListener, nsIURI **aURL);

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIProtocolHandler Interface 
	////////////////////////////////////////////////////////////////////////////////////////
    NS_DECL_NSIPROTOCOLHANDLER
	////////////////////////////////////////////////////////////////////////////////////////
	// End support of nsIProtocolHandler interface 
	////////////////////////////////////////////////////////////////////////////////////////


protected:
	// helper functions used by the service
	nsresult PrepareMessageUrl(const char * aSrcMsgMailboxURI, nsIUrlListener * aUrlListener,
							   nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl);
	
	nsresult RunMailboxUrl(nsIURI * aMailboxUrl, nsISupports * aDisplayConsumer = nsnull);
};

#endif /* nsMailboxService_h___ */
