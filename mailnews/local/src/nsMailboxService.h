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
    NS_DECL_NSIMAILBOXSERVICE
    NS_DECL_NSIMSGMESSAGESERVICE
    NS_DECL_NSIPROTOCOLHANDLER

protected:
  PRBool        mPrintingOperation; 

	// helper functions used by the service
	nsresult PrepareMessageUrl(const char * aSrcMsgMailboxURI, nsIUrlListener * aUrlListener,
							   nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl,
							   nsIMsgWindow *msgWindow);

	nsresult RunMailboxUrl(nsIURI * aMailboxUrl, nsISupports * aDisplayConsumer = nsnull);

  nsresult FetchMessage(const char* aMessageURI,
                        nsISupports * aDisplayConsumer, 
                        nsIMsgWindow * aMsgWindow,
										    nsIUrlListener * aUrlListener,
                        nsMailboxAction mailboxAction,
                        nsIURI ** aURL);
};

#endif /* nsMailboxService_h___ */
