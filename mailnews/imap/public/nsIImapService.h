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

#ifndef nsIImapService_h___
#define nsIImapService_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsFileSpec.h"

/* 9E3233E1-EBE2-11d2-95AD-000064657374 */

#define NS_IIMAPSERVICE_IID                         \
{ 0x9e3233e1, 0xebe2, 0x11d2,                 \
    { 0x95, 0xad, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

#define NS_IMAPSERVICE_CID						  \
{ /* C5852B22-EBE2-11d2-95AD-000064657374 */      \
 0xc5852b22, 0xebe2, 0x11d2,                      \
 {0x95, 0xad, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74}}

////////////////////////////////////////////////////////////////////////////////////////
// The IMAP Service is an interfaced designed to make building and running imap urls
// easier. Clients typically go to the imap service and ask it do things such as:
// get new mail, etc....
//
// Oh and in case you couldn't tell by the name, the imap service is a service! and you
// should go through the service manager to obtain an instance of it.
////////////////////////////////////////////////////////////////////////////////////////

class nsIImapProtocol;
class nsIImapMailfolder;
class nsIUrlListener;
class nsIURL;
struct PLEventQueue;

class nsIImapService : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IIMAPSERVICE_IID; return iid; }
	
	///////////////////////////////////////////////////////////////////////////////////
	// mscott -> I enventually envision that urls will come into the imap service and
	// connections will NOT go back out. The service will take the url and find or create
	// a connection to run the url in. However, in the early stages until we have
	// a real connection pool, we want to pass the protocol instance back to the imap
	// test harness so we can poke and prod it....
	//////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD CreateImapConnection (PLEventQueue *aEventQueue, nsIImapProtocol ** aImapConnection) = 0;

	// As always, you can pass in null for the url listener and the url if you don't require either.....
	// aClientEventQueue is the event queue of the event sinks. We post events into this queue.
	// mscott -- eventually this function will take in the account (identity/incoming server) associated with 
	// the request
	NS_IMETHOD SelectFolder(PLEventQueue * aClientEventQueue, nsIImapMailfolder * aImapUrl, nsIUrlListener * aUrlListener, nsIURL ** aURL) = 0;

};

#endif /* nsIImapService_h___ */
