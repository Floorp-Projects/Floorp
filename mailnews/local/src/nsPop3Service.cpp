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

#include "msgCore.h"    // precompiled header...

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsINetService.h"
#include "nsPop3Service.h"
#include "nsIMsgIncomingServer.h"
#include "nsIPop3IncomingServer.h"

#include "nsPop3URL.h"
#include "nsPop3Sink.h"
#include "nsPop3Protocol.h"

nsPop3Service::nsPop3Service()
{
    NS_INIT_REFCNT();
}

nsPop3Service::~nsPop3Service()
{}

NS_IMPL_THREADSAFE_ISUPPORTS(nsPop3Service, nsIPop3Service::GetIID());

NS_IMETHODIMP
nsPop3Service::CheckForNewMail(nsIUrlListener * aUrlListener,
                               nsIPop3IncomingServer *popServer,
                               nsIURL ** aURL)
{
	NS_LOCK_INSTANCE();
	nsresult rv = NS_OK;
	char * userName = nsnull;
	char * popPassword = nsnull;
	char * hostname = nsnull;

    nsIMsgIncomingServer *server = nsnull;
	nsIPop3URL * pop3Url = nsnull;

    rv = popServer->QueryInterface(nsIMsgIncomingServer::GetIID(),
                                   (void **)&server);
    if (NS_SUCCEEDED(rv) && server) {
		// load up required server information
		server->GetUserName(&userName);
        server->GetPassword(&popPassword);
		server->GetHostName(&hostname);
        NS_RELEASE(server);
    }
    
	if (NS_SUCCEEDED(rv) && popServer)
	{
        // now construct a pop3 url...
        char * urlSpec = PR_smprintf("pop3://%s?check", hostname);
        rv = BuildPop3Url(urlSpec, popServer, &pop3Url);
        PR_FREEIF(urlSpec);
    }
    
	if (NS_SUCCEEDED(rv) && pop3Url) 
	{
		// does the caller want to listen to the url?
		if (aUrlListener)
			pop3Url->RegisterListener(aUrlListener);

		nsPop3Protocol * protocol = new nsPop3Protocol(pop3Url);
		if (protocol)
		{
			protocol->SetUsername(userName);
			protocol->SetPassword(popPassword);
			protocol->Load(pop3Url);
		} // if pop server 
	}

	if (aURL && pop3Url) // we already have a ref count on pop3url...
		*aURL = pop3Url; // transfer ref count to the caller...
	else
		NS_IF_RELEASE(pop3Url); // release our ref...
	
	NS_UNLOCK_INSTANCE();
	return rv;
}

nsresult nsPop3Service::GetNewMail(nsIUrlListener * aUrlListener,
                                   nsIPop3IncomingServer *popServer,
                                   nsIURL ** aURL)
{
	NS_LOCK_INSTANCE();
	nsresult rv = NS_OK;
    char * userName = nsnull;
    char * popPassword = nsnull;
	char * popHost = nsnull;

	nsIMsgIncomingServer * server = nsnull;
	nsIPop3URL * pop3Url = nsnull;

    rv = popServer->QueryInterface(nsIMsgIncomingServer::GetIID(),
                                   (void **)&server);
    // convert normal host to POP host.
    // XXX - this doesn't handle QI failing very well
    if (NS_SUCCEEDED(rv) && server) {
		// load up required server information
        server->GetUserName(&userName);
		server->GetPassword(&popPassword);
		server->GetHostName(&popHost);
        NS_IF_RELEASE(server);
    }
    
    
	if (NS_SUCCEEDED(rv) && popServer)
	{
        // now construct a pop3 url...
        char * urlSpec = PR_smprintf("pop3://%s", popHost);
        rv = BuildPop3Url(urlSpec, popServer, &pop3Url);
        PR_FREEIF(urlSpec);
	}
    
	if (NS_SUCCEEDED(rv) && pop3Url) 
	{
		// does the caller want to listen to the url?
		if (aUrlListener)
			pop3Url->RegisterListener(aUrlListener);

		nsPop3Protocol * protocol = new nsPop3Protocol(pop3Url);
		if (protocol)
		{
			protocol->SetUsername(userName);
			protocol->SetPassword(popPassword);
			protocol->Load(pop3Url);
		} // if pop server 
	}

	if (aURL && pop3Url) // we already have a ref count on pop3url...
		*aURL = pop3Url; // transfer ref count to the caller...
	else
		NS_IF_RELEASE(pop3Url); // release our ref...
	
	NS_UNLOCK_INSTANCE();
	return rv;
}

nsresult nsPop3Service::BuildPop3Url(const char * urlSpec,
                                     nsIPop3IncomingServer *server,
                                     nsIPop3URL ** aUrl)
{
	nsresult rv = NS_OK;
	// create a sink to run the url with
	nsPop3Sink * pop3Sink = new nsPop3Sink();
	if (pop3Sink)
		pop3Sink->SetPopServer(server);

	// now create a pop3 url and a protocol instance to run the url....
	nsPop3URL * pop3Url = new nsPop3URL(nsnull, nsnull);
	if (pop3Url)
	{
		pop3Url->SetPop3Sink(pop3Sink);
		pop3Url->ParseURL(urlSpec);
	}

	if (aUrl)
		rv = pop3Url->QueryInterface(nsIPop3URL::GetIID(), (void **) aUrl);
	else  // hmmm delete is protected...what can we do here? no one has a ref cnt on the object...
		NS_IF_RELEASE(pop3Url);	 
//		delete pop3Url;

	return rv;
}
