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
#include "nsIURL.h"
#include "nsIUrlListener.h"
#include "nsIStreamListener.h"
#include "nsFileSpec.h"

////////////////////////////////////////////////////////////////////////////////////////
// The Mailbox Service is an interface designed to make building and running mailbox urls
// easier. I'm not sure if this service will go away when the new networking model comes
// on line (as part of the N2 project). So I reserve the right to change my mind and take
// this service away =).
////////////////////////////////////////////////////////////////////////////////////////

class nsMailboxService : public nsIMailboxService
{
public:

	nsMailboxService();
	virtual ~nsMailboxService();
	
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIMailboxService Interface 
	////////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD ParseMailbox(const nsFileSpec& aMailboxPath, nsIStreamListener * aMailboxParser, 
							nsIUrlListener * aUrlListener, nsIURL ** aURL);
	
	NS_IMETHOD CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopy, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIURL **aURL);

	NS_IMETHOD DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
							  nsIUrlListener * aUrlListener, nsIURL ** aURL);

	NS_IMETHOD DisplayMessageNumber(const nsFileSpec& aMailboxPath, PRUint32 aMessageNumber, nsISupports * aDisplayConsumer,
									nsIUrlListener * aUrlListener, nsIURL ** aURL);

	////////////////////////////////////////////////////////////////////////////////////////
	// End suppport for the nsIMailboxService Interface
	////////////////////////////////////////////////////////////////////////////////////////

};

#endif /* nsMailboxService_h___ */
