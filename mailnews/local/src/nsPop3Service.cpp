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

#include "nsPop3Service.h"
#include "nsIMsgIncomingServer.h"
#include "nsIPop3IncomingServer.h"

#include "nsPop3URL.h"
#include "nsPop3Sink.h"
#include "nsPop3Protocol.h"
#include "nsCOMPtr.h"
#include "nsMsgLocalCID.h"
#include "nsXPIDLString.h"

#define POP3_PORT 110 // The IANA port for Pop3

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kPop3UrlCID, NS_POP3URL_CID);

nsPop3Service::nsPop3Service()
{
    NS_INIT_REFCNT();
}

nsPop3Service::~nsPop3Service()
{}

NS_IMPL_THREADSAFE_ADDREF(nsPop3Service);
NS_IMPL_THREADSAFE_RELEASE(nsPop3Service);

nsresult nsPop3Service::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsIPop3Service::GetIID()) || aIID.Equals(kISupportsIID))
	{
        *aInstancePtr = (void*) ((nsIPop3Service*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	if (aIID.Equals(nsIProtocolHandler::GetIID()))
	{
		*aInstancePtr = (void *) ((nsIProtocolHandler*) this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}

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
        char * urlSpec = PR_smprintf("pop3://%s:%d?check", hostname, POP3_PORT);
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
        char * urlSpec = PR_smprintf("pop3://%s:%d", popHost, POP3_PORT);
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

nsresult nsPop3Service::BuildPop3Url(char * urlSpec,
									 nsIMsgFolder *inbox,
                                     nsIPop3IncomingServer *server,
									 nsIUrlListener * aUrlListener,
                                     nsIURI ** aUrl)
{
	nsPop3Sink * pop3Sink = new nsPop3Sink();
	if (pop3Sink)
	{
		pop3Sink->SetPopServer(server);
		pop3Sink->SetFolder(inbox);
	}

	// now create a pop3 url and a protocol instance to run the url....
	nsCOMPtr<nsIPop3URL> pop3Url;
	nsresult rv = nsComponentManager::CreateInstance(kPop3UrlCID,
                                            nsnull,
                                            nsCOMTypeInfo<nsIPop3URL>::GetIID(),
                                            getter_AddRefs(pop3Url));
	if (pop3Url)
	{
		nsXPIDLCString userName;
		nsCOMPtr<nsIMsgIncomingServer> msgServer = do_QueryInterface(server);
		msgServer->GetUsername(getter_Copies(userName));

		pop3Url->SetPop3Sink(pop3Sink);
		pop3Url->SetUsername(userName);

		if (aUrlListener)
		{
			nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(pop3Url);
			if (mailnewsurl)
				mailnewsurl->RegisterListener(aUrlListener);
		}


		if (aUrl)
		{
			rv = pop3Url->QueryInterface(nsCOMTypeInfo<nsIURI>::GetIID(), (void **) aUrl);
			if (*aUrl)
			{
				(*aUrl)->SetSpec(urlSpec);
				// the following is only a temporary work around hack because necko
				// is loosing our port when the url is just scheme://host:port.
				// when they fix this bug I can remove the following code where we
				// manually set the port.
				(*aUrl)->SetPort(POP3_PORT);
			}
		}
	}

	return rv;
}

nsresult nsPop3Service::RunPopUrl(nsIMsgIncomingServer * aServer, nsIURI * aUrlToRun)
{
	nsresult rv = NS_OK;
	if (aServer && aUrlToRun)
	{
		nsXPIDLCString userName;

		// load up required server information
		rv = aServer->GetUsername(getter_Copies(userName));

		nsPop3Protocol * protocol = new nsPop3Protocol(aUrlToRun);
		if (protocol)
		{
			protocol->SetUsername(userName);
			rv = protocol->LoadUrl(aUrlToRun);
		}
	} // if server

	return rv;
}


NS_IMETHODIMP nsPop3Service::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = PL_strdup("pop3");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsPop3Service::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = POP3_PORT;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 	
}

NS_IMETHODIMP nsPop3Service::MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI, char **_retval)
{
	// no such thing as relative urls for smtp.....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsPop3Service::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	// i just haven't implemented this yet...I will be though....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsPop3Service::NewChannel(const char *verb, nsIURI *aURI, nsIEventSinkGetter *eventSinkGetter, nsIChannel **_retval)
{
	// mscott - right now, I don't like the idea of returning channels to the caller. They just want us
	// to run the url, they don't want a channel back...I'm going to be addressing this issue with
	// the necko team in more detail later on.
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}
