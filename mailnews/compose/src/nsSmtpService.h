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

#ifndef nsSmtpService_h___
#define nsSmtpService_h___

#include "nscore.h"
#include "nsISmtpService.h"

////////////////////////////////////////////////////////////////////////////////////////
// The Smtp Service is an interfaced designed to make building and running mail to urls
// easier. I'm not sure if this service will go away when the new networking model comes
// on line (as part of the N2 project). So I reserve the right to change my mind and take
// this service away =).
////////////////////////////////////////////////////////////////////////////////////////

class nsSmtpService : public nsISmtpService
{
public:

	nsSmtpService();
	virtual ~nsSmtpService();
	
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsISmtpService interface 
	////////////////////////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// SendMailMessage requires the file name of the message to send, the sender, a comma delimited list of recipients.
	// It builds an Smtp url, makes an smtp connection and runs the url. If you want a handle on the running task, pass in 
	// a valid nsIURL ptr. You can later interrupt this action by asking the netlib service manager to interrupt the url you 
	// are given back. Remember to release aURL when you are done with it. Pass nsnull in for aURL if you don't care about 
	// the returned URL.
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD SendMailMessage(const nsFilePath& aFilePath, const nsString& aRecipients, nsIUrlListener * aUrlListener, 
							   nsIURL ** aURL);

	////////////////////////////////////////////////////////////////////////////////////////
	// End support of nsISmtpService interface 
	////////////////////////////////////////////////////////////////////////////////////////
};

#endif /* nsSmtpService_h___ */
