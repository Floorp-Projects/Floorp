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

#ifndef nsIMailboxUrl_h___
#define nsIMailboxUrl_h___

#include "nscore.h"
#include "nsIURL.h"

#include "nsISupports.h"

/* include all of our event sink interfaces */
#include "nsIStreamListener.h"

/* C272A1C1-C166-11d2-804E-006008128C4E */

#define NS_IMAILBOXURL_IID                         \
{ 0xc272a1c1, 0xc166, 0x11d2,                  \
    { 0x80, 0x4e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

class nsIMailboxUrl : public nsIURL
{
public:

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the mailbox specific event sinks to bind to to your url
	///////////////////////////////////////////////////////////////////////////////

	// Mailbox urls which parse a mailbox folder require a consumer of the stream which
	// will represent the mailbox. This consumer is the mailbox parser. As data from
	// the mailbox folder is read in, the data will be written to a stream and the consumer
	// will be notified through nsIStreamListenter::OnDataAvailable that the stream has data
	// available...
	// mscott: I wonder if the caller should be allowed to create and set the stream they want
	// the data written to as well? Hmm....
	
	NS_IMETHOD SetMailboxParser(nsIStreamListener * aConsumer) = 0;

	NS_IMETHOD GetMailboxParser(nsIStreamListener ** aConsumer) = 0;
	
	// mscott: this interface really belongs in nsIURL and I will move it there after talking
	// it over with core netlib. This error message replaces the err_msg which was in the 
	// old URL_struct. Also, it should probably be a nsString or a PRUnichar *. I don't know what
	// XP_GetString is going to return in mozilla. 

	NS_IMETHOD SetErrorMessage (char * errorMessage) = 0;
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const = 0;
};

#endif /* nsIMailboxUrl_h___ */