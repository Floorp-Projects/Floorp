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

#include "nsINetService.h"
#include "nsPop3Service.h"
#include "nsIMsgIncomingServer.h"
#include "nsIPop3IncomingServer.h"

#include "nsPop3URL.h"
#include "nsPop3Sink.h"
#include "nsPop3Protocol.h"

#include "nsCOMPtr.h"

nsPop3Service::nsPop3Service()
{
    NS_INIT_REFCNT();
}

nsPop3Service::~nsPop3Service()
{}

NS_IMPL_THREADSAFE_ISUPPORTS(nsPop3Service, nsIPop3Service::GetIID());

NS_IMETHODIMP nsPop3Service::CheckForNewMail(nsIUrlListener * aUrlListener,
							   nsIMsgFolder *inbox, 
                               nsIPop3IncomingServer *popServer,
                               nsIURI ** aURL)
{
	NS_LOCK_INSTANCE();
	nsresult rv = NS_OK;
	char * hostname = nsnull;

    nsCOMPtr<nsIMsgIncomingServer> server;
	nsCOMPtr<nsIURI> url;

	server = do_QueryInterface(popServer);
    if (server) 
		server->GetHostName(&hostname);
    
	if (NS_SUCCEEDED(rv) && popServer && hostname)
	{
        // now construct a pop3 url...
        char * urlSpec = PR_smprintf("pop3://%s?check", hostname);
        rv = BuildPop3Url(urlSpec, inbox, popServer, aUrlListener, getter_AddRefs(url));
        PR_FREEIF(urlSpec);
		if (hostname) PL_strfree(hostname);
    }

    
	if (NS_SUCCEEDED(rv) && url) 
		rv = RunPopUrl(server, url);

	if (aURL && url) // we already have a ref count on pop3url...
	{
		*aURL = url; // transfer ref count to the caller...
		NS_IF_ADDREF(*aURL);
	}
	
	NS_UNLOCK_INSTANCE();
	return rv;
}


nsresult nsPop3Service::GetNewMail(nsIUrlListener * aUrlListener,
                                   nsIPop3IncomingServer *popServer,
                                   nsIURI ** aURL)
{
	NS_LOCK_INSTANCE();
	nsresult rv = NS_OK;
	char * popHost = nsnull;
	nsCOMPtr<nsIURI> url;

	nsCOMPtr<nsIMsgIncomingServer> server;
	server = do_QueryInterface(popServer);    

    if (server) 
		server->GetHostName(&popHost);
    
    
	if (NS_SUCCEEDED(rv) && popServer)
	{
        // now construct a pop3 url...
        char * urlSpec = PR_smprintf("pop3://%s", popHost);
        rv = BuildPop3Url(urlSpec, nsnull, popServer, aUrlListener, getter_AddRefs(url));
        PR_FREEIF(urlSpec);
	}
    
	if (NS_SUCCEEDED(rv) && url) 
		RunPopUrl(server, url);

    if (popHost) PL_strfree(popHost);

	if (aURL && url) // we already have a ref count on pop3url...
	{
		*aURL = url; // transfer ref count to the caller...
		NS_IF_ADDREF(*aURL);
	}
	
	NS_UNLOCK_INSTANCE();
	return rv;
}

nsresult nsPop3Service::BuildPop3Url(const char * urlSpec,
									 nsIMsgFolder *inbox,
                                     nsIPop3IncomingServer *server,
									 nsIUrlListener * aUrlListener,
                                     nsIURI ** aUrl)
{
	nsresult rv = NS_OK;
	// create a sink to run the url with
	nsPop3Sink * pop3Sink = new nsPop3Sink();
	if (pop3Sink)
	{
		pop3Sink->SetPopServer(server);
		pop3Sink->SetFolder(inbox);
	}

	// now create a pop3 url and a protocol instance to run the url....
	nsPop3URL * pop3Url = new nsPop3URL();
	if (pop3Url)
	{
		pop3Url->SetPop3Sink(pop3Sink);
		pop3Url->SetSpec(urlSpec);
	}

	// does the caller want to listen to the url?
	if (aUrlListener)
		pop3Url->RegisterListener(aUrlListener);

	if (aUrl)
		rv = pop3Url->QueryInterface(nsIURI::GetIID(), (void **) aUrl);
	else  // hmmm delete is protected...what can we do here? no one has a ref cnt on the object...
		NS_IF_RELEASE(pop3Url);

	return rv;
}

nsresult nsPop3Service::RunPopUrl(nsIMsgIncomingServer * aServer, nsIURI * aUrlToRun)
{
	nsresult rv = NS_OK;
	if (aServer && aUrlToRun)
	{
		char * userName = nsnull;
		char * popPassword = nsnull;

		// load up required server information
		rv = aServer->GetUsername(&userName);
        rv = aServer->GetPassword(&popPassword);

		nsPop3Protocol * protocol = new nsPop3Protocol(aUrlToRun);
		if (protocol)
		{
			protocol->SetUsername(userName);
			protocol->SetPassword(popPassword);
			rv = protocol->LoadUrl(aUrlToRun);
		}

		if (popPassword) PL_strfree(popPassword);
		if (userName) PL_strfree(userName);
	} // if server

	return rv;
}
