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
    NS_DECL_NSIMAILBOXSERVICE
    NS_DECL_NSIMSGMESSAGESERVICE
    NS_DECL_NSIPROTOCOLHANDLER

protected:
	// helper functions used by the service
	nsresult PrepareMessageUrl(const char * aSrcMsgMailboxURI, nsIUrlListener * aUrlListener,
							   nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl);
	
	nsresult RunMailboxUrl(nsIURI * aMailboxUrl, nsISupports * aDisplayConsumer = nsnull);
};

#endif /* nsMailboxService_h___ */
