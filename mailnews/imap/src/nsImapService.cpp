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

#include "msgCore.h"    // precompiled header...

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsIIMAPHostSessionList.h"
#include "nsImapService.h"
#include "nsImapUrl.h"
#include "nsImapProtocol.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolder.h"

#include "nsImapUtils.h"

#include "nsIRDFService.h"
#include "nsIEventQueueService.h"
#include "nsRDFCID.h"
#include "nsXPComCIID.h"
// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);

NS_IMPL_THREADSAFE_ADDREF(nsImapService);
NS_IMPL_THREADSAFE_RELEASE(nsImapService);


nsImapService::nsImapService()
{
    NS_INIT_REFCNT();

	// mscott - the imap service really needs to be a service listener
	// on the host session list...
	nsresult rv = nsServiceManager::GetService(kCImapHostSessionList, nsIImapHostSessionList::GetIID(),
                                  (nsISupports**)&m_sessionList);
}

nsImapService::~nsImapService()
{
	// release the host session list
	if (m_sessionList)
		(void)nsServiceManager::ReleaseService(kCImapHostSessionList, m_sessionList);
}

nsresult nsImapService::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (nsnull == aInstancePtr)
        return NS_ERROR_NULL_POINTER;
 
    if (aIID.Equals(nsIImapService::GetIID()) || aIID.Equals(kISupportsIID)) 
	{
        *aInstancePtr = (void*) ((nsIImapService*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(nsIMsgMessageService::GetIID())) 
	{
        *aInstancePtr = (void*) ((nsIMsgMessageService*)this);
        AddRef();
        return NS_OK;
    }

#if defined(NS_DEBUG)
    /*
     * Check for the debug-only interface indicating thread-safety
     */
    static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
    if (aIID.Equals(kIsThreadsafeIID))
        return NS_OK;
#endif
 
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsImapService::CreateImapConnection(PLEventQueue *aEventQueue, 
                                    nsIImapProtocol ** aImapConnection)
{
	nsIImapProtocol * protocolInstance = nsnull;
	nsresult rv = NS_OK;
	if (aImapConnection)
	{
		rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull, nsIImapProtocol::GetIID(), (void **) &protocolInstance);
		if (NS_SUCCEEDED(rv) && protocolInstance)
			rv = protocolInstance->Initialize(m_sessionList, aEventQueue);
		*aImapConnection = protocolInstance;
	}

	return rv;
}

NS_IMETHODIMP
nsImapService::SelectFolder(PLEventQueue * aClientEventQueue, 
                            nsIMsgFolder * aImapMailFolder, 
                            nsIUrlListener * aUrlListener, 
                            nsIURL ** aURL)
{


	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl,
                                          protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{
		PRBool gotFolder = PR_FALSE;
		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char *folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
			gotFolder = folderName && PL_strlen(folderName) > 0;
			urlSpec.Append("/select>/");
            urlSpec.Append(folderName);
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
            delete [] folderName;
		} // if we got a host name

		imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
		if (gotFolder)
			protocolInstance->LoadUrl(imapUrl, nsnull);
		if (aURL)
			*aURL = imapUrl; 
		else
			NS_RELEASE(imapUrl); // release our ref count from the create instance call...
	} // if we have a url to run....

	return rv;
}

// lite select, used to verify UIDVALIDITY while going on/offline
NS_IMETHODIMP
nsImapService::LiteSelectFolder(PLEventQueue * aClientEventQueue, 
                                nsIMsgFolder * aImapMailFolder, 
                                nsIUrlListener * aUrlListener, 
                                nsIURL ** aURL)
{

	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
	
	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapLiteSelectFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/';
			urlSpec.Append("/liteselect>");
			urlSpec.Append(hierarchySeparator);
			// ### FIXME - hardcode selection of the inbox - should get folder
            // name from folder
            char* folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
			urlSpec.Append(folderName);
            delete [] folderName;
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
		} // if we got a host name

		imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
		protocolInstance->LoadUrl(imapUrl, nsnull);
		if (aURL)
			*aURL = imapUrl; 
		else
			NS_RELEASE(imapUrl); // release our ref count from the create instance call...
	} // if we have a url to run....

	return rv;
}

static const char *sequenceString = "SEQUENCE";
static const char *uidString = "UID";

NS_IMETHODIMP nsImapService::DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
										  nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
	nsImapUrl * imapUrl = nsnull;
	nsIImapUrl * url = nsnull;
	nsresult rv = NS_OK;
    PLEventQueue *queue;
	nsIRDFService* rdf = nsnull;
 	// get the Event Queue for this thread...
    nsIEventQueueService* pEventQService = nsnull;
    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                          nsIEventQueueService::GetIID(),
                                          (nsISupports**)&pEventQService);
	if (NS_SUCCEEDED(rv) && pEventQService)
	{
		rv = pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),&queue);
		NS_RELEASE(pEventQService);
		rv = nsServiceManager::GetService(kRDFServiceCID,
										nsIRDFService::GetIID(),
										(nsISupports**)&rdf);

	}

	nsString	folderURI;
	nsMsgKey	msgKey;
	rv = nsParseImapMessageURI(aMessageURI, folderURI, &msgKey);
	if (NS_SUCCEEDED(rv))
	{
		nsIRDFResource* res;
		rv = rdf->GetResource(nsAutoCString(folderURI), &res);
		if (NS_FAILED(rv))
			return rv;
		nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
		if (NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(res, &rv));
			if (NS_SUCCEEDED(rv))
			{
				nsString2 messageIdString(eOneByte);

				messageIdString.Append(msgKey, 10);
				rv = FetchMessage(queue, folder, imapMessageSink, aUrlListener, 
					aURL, aDisplayConsumer, messageIdString.GetBuffer(), PR_TRUE);
			}
						
		}
	}
	if (rdf)
		(void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	if (pEventQService)
		(void)nsServiceManager::ReleaseService(kRDFServiceCID, pEventQService);

	return rv;
}

NS_IMETHODIMP
nsImapService::CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopy, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIURL **aURL)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}


/* fetching RFC822 messages */
/* imap4://HOST>fetch><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will set the 'SEEN' flag */
NS_IMETHODIMP
nsImapService::FetchMessage(PLEventQueue * aClientEventQueue, 
                            nsIMsgFolder * aImapMailFolder, 
                            nsIImapMessageSink * aImapMessage,
                            nsIUrlListener * aUrlListener, nsIURL ** aURL,
							nsISupports * aDisplayConsumer, 
                            const char *messageIdentifierList,
                            PRBool messageIdsAreUID)
{
	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue && aImapMessage,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
	
	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		rv = imapUrl->SetImapMessageSink(aImapMessage);
		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/fetch>");
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
			urlSpec.Append(folderName);
			urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
            delete [] folderName;
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, aDisplayConsumer);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}
// utility function to handle basic setup - will probably change when the real connection stuff is done.
nsresult nsImapService::GetImapConnectionAndUrl(PLEventQueue * aClientEventQueue, nsIImapUrl  * &imapUrl, 
												nsIImapProtocol * &protocolInstance, nsString2 &urlSpec)
{
	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
	
	nsresult rv = CreateImapConnection(aClientEventQueue, &protocolInstance);

	if (NS_SUCCEEDED(rv) && protocolInstance)
	{
		// now we need to create an imap url to load into the connection. The url needs to represent a select folder action.
		rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
                                            nsIImapUrl::GetIID(), (void **)
                                            &imapUrl);
		if (NS_SUCCEEDED(rv) && imapUrl)
			rv = CreateStartOfImapUrl(*imapUrl, urlSpec);
		else
			NS_RELEASE(protocolInstance);
	}
	return rv;
}

// these are all the urls we know how to generate. I'm going to make service methods
// for most of these...and use nsString2's to build up the string.
nsresult nsImapService::CreateStartOfImapUrl(nsIImapUrl &imapUrl, nsString2 &urlString)
{
	// hmmm this is cludgy...we need to get the incoming server, get the host and port, and generate an imap url spec
	// based on that information then tell the imap parser to parse that spec...*yuck*. I have to think of a better way
	// for automatically generating the spec based on the incoming server data...

	nsIMsgIncomingServer * server = nsnull;
	nsresult rv = imapUrl.GetServer(&server);	// no need to release server?
	if (NS_SUCCEEDED(rv) && server)
	{
		char * hostName = nsnull;
		rv = server->GetHostName(&hostName);
		if (NS_SUCCEEDED(rv) && hostName)
		{
			urlString = "imap://";
			urlString.Append(hostName);
			PR_Free(hostName);
		}
	}
	return rv;
}

/* fetching the headers of RFC822 messages */
/* imap4://HOST>header><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will not affect the 'SEEN' flag */
NS_IMETHODIMP
nsImapService::GetHeaders(PLEventQueue * aClientEventQueue, 
                          nsIMsgFolder * aImapMailFolder, 
                          nsIUrlListener * aUrlListener, 
                          nsIURL ** aURL,
                          const char *messageIdentifierList,
                          PRBool messageIdsAreUID)
{
	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
	
	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/header>");
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
			urlSpec.Append(folderName);
            delete [] folderName;
            urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, nsnull);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}


// Noop, used to update a folder (causes server to send changes).
NS_IMETHODIMP
nsImapService::Noop(PLEventQueue * aClientEventQueue, 
                    nsIMsgFolder * aImapMailFolder,
                    nsIUrlListener * aUrlListener,
                    nsIURL ** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectNoopFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/selectnoop>");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
			urlSpec.Append(folderName);
            delete [] folderName;
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, nsnull);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}

// Expunge, used to "compress" an imap folder,removes deleted messages.
NS_IMETHODIMP
nsImapService::Expunge(PLEventQueue * aClientEventQueue, 
                       nsIMsgFolder * aImapMailFolder,
                       nsIUrlListener * aUrlListener, 
                       nsIURL ** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapExpungeFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/Expunge>");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
			urlSpec.Append(folderName);
            delete [] folderName;
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, nsnull);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}

/* old-stle biff that doesn't download headers */
NS_IMETHODIMP
nsImapService::Biff(PLEventQueue * aClientEventQueue, 
                    nsIMsgFolder * aImapMailFolder,
                    nsIUrlListener * aUrlListener, 
                    nsIURL ** aURL,
                    PRUint32 uidHighWater)
{
	static const char *formatString = "biff>%c%s>%ld";
	
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapExpungeFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/Biff>");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
            urlSpec.Append(folderName);
            delete [] folderName;
			urlSpec.Append(">");
			urlSpec.Append(uidHighWater, 10);
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, nsnull);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}

NS_IMETHODIMP
nsImapService::DeleteMessages(PLEventQueue * aClientEventQueue, 
                              nsIMsgFolder * aImapMailFolder, 
                              nsIUrlListener * aUrlListener, 
                              nsIURL ** aURL,
                              const char *messageIdentifierList,
                              PRBool messageIdsAreUID)
{
	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
	
	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/deletemsg>");
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
            urlSpec.Append(folderName);
			urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, nsnull);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}

// Delete all messages in a folder, used to empty trash
NS_IMETHODIMP
nsImapService::DeleteAllMessages(PLEventQueue * aClientEventQueue, 
                                 nsIMsgFolder * aImapMailFolder,
                                 nsIUrlListener * aUrlListener, 
                                 nsIURL ** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectNoopFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/deleteallmsgs>");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
			urlSpec.Append(folderName);
            delete [] folderName;
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, nsnull);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}

NS_IMETHODIMP
nsImapService::AddMessageFlags(PLEventQueue * aClientEventQueue,
                               nsIMsgFolder * aImapMailFolder, 
                               nsIUrlListener * aUrlListener, 
                               nsIURL ** aURL,
                               const char *messageIdentifierList,
                               imapMessageFlagsType flags,
                               PRBool messageIdsAreUID)
{
	return DiddleFlags(aClientEventQueue, aImapMailFolder, aUrlListener, aURL, messageIdentifierList,
		"addmsgflags", flags, messageIdsAreUID);
}

NS_IMETHODIMP
nsImapService::SubtractMessageFlags(PLEventQueue * aClientEventQueue,
                                    nsIMsgFolder * aImapMailFolder, 
                                    nsIUrlListener * aUrlListener, 
                                    nsIURL ** aURL,
                                    const char *messageIdentifierList,
                                    imapMessageFlagsType flags,
                                    PRBool messageIdsAreUID)
{
	return DiddleFlags(aClientEventQueue, aImapMailFolder, aUrlListener, aURL, messageIdentifierList,
		"subtractmsgflags", flags, messageIdsAreUID);
}

NS_IMETHODIMP
nsImapService::SetMessageFlags(PLEventQueue * aClientEventQueue,
                               nsIMsgFolder * aImapMailFolder, 
                               nsIUrlListener * aUrlListener, 
                               nsIURL ** aURL,
                               const char *messageIdentifierList,
                               imapMessageFlagsType flags,
                               PRBool messageIdsAreUID)
{
	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.

	return DiddleFlags(aClientEventQueue, aImapMailFolder, aUrlListener, aURL, messageIdentifierList,
		"setmsgflags", flags, messageIdsAreUID);
}

nsresult nsImapService::DiddleFlags(PLEventQueue * aClientEventQueue, 
                                    nsIMsgFolder * aImapMailFolder, 
                                    nsIUrlListener * aUrlListener,
                                    nsIURL ** aURL,
                                    const char *messageIdentifierList,
                                    const char *howToDiddle,
                                    imapMessageFlagsType flags,
                                    PRBool messageIdsAreUID)
{
	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
	
	nsIImapProtocol * protocolInstance = nsnull;
	nsIImapUrl * imapUrl = nsnull;
	nsString2 urlSpec(eOneByte);

	nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, imapUrl, protocolInstance, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append('/');
			urlSpec.Append(howToDiddle);
			urlSpec.Append('>');
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.Append(hierarchySeparator);
            char *folderName = nsnull;
            aImapMailFolder->GetName(&folderName);
            urlSpec.Append(folderName);
            delete [] folderName;
			urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
			urlSpec.Append('>');
			urlSpec.Append(flags, 10);
			rv = imapUrl->SetSpec(urlSpec.GetBuffer());
			imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
			protocolInstance->LoadUrl(imapUrl, nsnull);
			if (aURL)
				*aURL = imapUrl; 
			else
				NS_RELEASE(imapUrl); // release our ref count from the create instance call...
		}
	}
	return rv;
}

nsresult
nsImapService::SetImapUrlSink(nsIMsgFolder* aMsgFolder,
                                nsIImapUrl* aImapUrl)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsISupports* aInst = nsnull;

    NS_ASSERTION (aMsgFolder && aImapUrl, "Oops ... null pointers");
    if (!aMsgFolder || !aImapUrl)
        return rv;
    
    rv = aMsgFolder->QueryInterface(nsIImapLog::GetIID(), (void**)&aInst);
    if (NS_SUCCEEDED(rv) && aInst)
        aImapUrl->SetImapLog((nsIImapLog*) aInst);
    NS_IF_RELEASE (aInst);
    aInst = nsnull;
    
    rv = aMsgFolder->QueryInterface(nsIImapMailFolderSink::GetIID(), 
                                   (void**)&aInst);
    if (NS_SUCCEEDED(rv) && aInst)
        aImapUrl->SetImapMailFolderSink((nsIImapMailFolderSink*) aInst);
    NS_IF_RELEASE (aInst);
    aInst = nsnull;
    
    rv = aMsgFolder->QueryInterface(nsIImapMessageSink::GetIID(), 
                                   (void**)&aInst);
    if (NS_SUCCEEDED(rv) && aInst)
        aImapUrl->SetImapMessageSink((nsIImapMessageSink*) aInst);
    NS_IF_RELEASE (aInst);
    aInst = nsnull;
    
    rv = aMsgFolder->QueryInterface(nsIImapExtensionSink::GetIID(), 
                                   (void**)&aInst);
    if (NS_SUCCEEDED(rv) && aInst)
        aImapUrl->SetImapExtensionSink((nsIImapExtensionSink*) aInst);
    NS_IF_RELEASE (aInst);
    aInst = nsnull;
    
    rv = aMsgFolder->QueryInterface(nsIImapMiscellaneousSink::GetIID(), 
                                   (void**)&aInst);
    if (NS_SUCCEEDED(rv) && aInst)
        aImapUrl->SetImapMiscellaneousSink((nsIImapMiscellaneousSink*) aInst);
    NS_IF_RELEASE (aInst);
    aInst = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsImapService::DiscoverAllFolders(PLEventQueue* aClientEventQueue,
                                  nsIMsgFolder* aImapMailFolder,
                                  nsIUrlListener* aUrlListener,
                                  nsIURL** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsIImapProtocol* aProtocol = nsnull;
    nsIImapUrl* aImapUrl = nsnull;
    nsString2 urlSpec(eOneByte);

    nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, aImapUrl,
                                          aProtocol, urlSpec);
    if (NS_SUCCEEDED (rv) && aImapUrl && aProtocol)
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            urlSpec.Append("/discoverallboxes");
            rv = aImapUrl->SetSpec(urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
            {
                aImapUrl->RegisterListener(aUrlListener);
                aProtocol->LoadUrl(aImapUrl, nsnull);
                if (aURL)
                {
                    *aURL = aImapUrl;
                    NS_ADDREF(*aURL);
                }
            }
        }
    }
    NS_IF_RELEASE(aImapUrl);
    NS_IF_RELEASE(aProtocol);
    return rv;
}

NS_IMETHODIMP
nsImapService::DiscoverAllAndSubscribedFolders(PLEventQueue* aClientEventQueue,
                                              nsIMsgFolder* aImapMailFolder,
                                              nsIUrlListener* aUrlListener,
                                              nsIURL** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsIImapProtocol* aProtocol = nsnull;
    nsIImapUrl* aImapUrl = nsnull;
    nsString2 urlSpec(eOneByte);

    nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, aImapUrl,
                                          aProtocol, urlSpec);
    if (NS_SUCCEEDED (rv) && aImapUrl && aProtocol)
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            urlSpec.Append("/discoverallandsubscribedboxes");
            rv = aImapUrl->SetSpec(urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
            {
                aImapUrl->RegisterListener(aUrlListener);
                aProtocol->LoadUrl(aImapUrl, nsnull);
                if (aURL)
                {
                    *aURL = aImapUrl;
                    NS_ADDREF(*aURL);
                }
            }
        }
    }
    NS_IF_RELEASE(aImapUrl);
    NS_IF_RELEASE(aProtocol);
    return rv;
}

NS_IMETHODIMP
nsImapService::DiscoverChildren(PLEventQueue* aClientEventQueue,
                                nsIMsgFolder* aImapMailFolder,
                                nsIUrlListener* aUrlListener,
                                nsIURL** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsIImapProtocol* aProtocol = nsnull;
    nsIImapUrl* aImapUrl = nsnull;
    nsString2 urlSpec(eOneByte);

    nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, aImapUrl,
                                          aProtocol, urlSpec);
    if (NS_SUCCEEDED (rv) && aImapUrl && aProtocol)
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            PRBool gotFolder = PR_FALSE;
            char *folderName = nsnull;
            
            aImapMailFolder->GetName(&folderName);
            if (folderName && *folderName != nsnull)
            {
                // **** fix me with host specific hierarchySeparator please
                urlSpec.Append("/discoverchildren>/");
                urlSpec.Append(folderName);
                rv = aImapUrl->SetSpec(urlSpec.GetBuffer());
                if (NS_SUCCEEDED(rv))
                {
                    aImapUrl->RegisterListener(aUrlListener);
                    aProtocol->LoadUrl(aImapUrl, nsnull);
                    if (aURL)
                    {
                        *aURL = aImapUrl;
                        NS_ADDREF(*aURL);
                    }
                }
            }
            else
            {
                rv = NS_ERROR_NULL_POINTER;
            }
            delete [] folderName;
        }
    }
    NS_IF_RELEASE(aImapUrl);
    NS_IF_RELEASE(aProtocol);
    return rv;
}

NS_IMETHODIMP
nsImapService::DiscoverLevelChildren(PLEventQueue* aClientEventQueue,
                                     nsIMsgFolder* aImapMailFolder,
                                     nsIUrlListener* aUrlListener,
                                     PRInt32 level,
                                     nsIURL** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsIImapProtocol* aProtocol = nsnull;
    nsIImapUrl* aImapUrl = nsnull;
    nsString2 urlSpec(eOneByte);

    nsresult rv = GetImapConnectionAndUrl(aClientEventQueue, aImapUrl,
                                          aProtocol, urlSpec);
    if (NS_SUCCEEDED (rv) && aImapUrl && aProtocol)
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            PRBool gotFolder = PR_FALSE;
            char *folderName = nsnull;
            
            aImapMailFolder->GetName(&folderName);
            if (folderName && *folderName != nsnull)
            {
                urlSpec.Append("/discoverlevelchildren>");
                urlSpec.Append(level);
                // **** fix me with host specific hierarchySeparator please
                urlSpec.Append("/"); // hierarchySeparator "/"
                urlSpec.Append(folderName);
                rv = aImapUrl->SetSpec(urlSpec.GetBuffer());
                if (NS_SUCCEEDED(rv))
                {
                    aImapUrl->RegisterListener(aUrlListener);
                    aProtocol->LoadUrl(aImapUrl, nsnull);
                    if (aURL)
                    {
                        *aURL = aImapUrl;
                        NS_ADDREF(*aURL);
                    }
                }
            }
            else
            {
                rv = NS_ERROR_NULL_POINTER;
            }
            delete [] folderName;
        }
    }
    NS_IF_RELEASE(aImapUrl);
    NS_IF_RELEASE(aProtocol);
    return rv;
}


#ifdef HAVE_PORT

/* fetching the headers of RFC822 messages */
/* imap4://HOST>header><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will not affect the 'SEEN' flag */
char *CreateImapMessageHeaderUrl(const char *imapHost,
								 const char *mailbox,
								 char hierarchySeparator,
								 const char *messageIdentifierList,
								 XP_Bool messageIdsAreUID)
{
	static const char *formatString = "header>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIdentifierList));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
   /* Reviewed 4.51 safe use of sprintf */
        	
	return returnString;
}

/* Noop, used to reset timer or download new headers for a selected folder */
char *CreateImapMailboxNoopUrl(const char *imapHost, 
								 const char *mailbox,
								 char  hierarchySeparator)
{
	static const char *formatString = "selectnoop>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), 
				formatString, 
				hierarchySeparator, 
				mailbox);   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* expunge, used in traditional imap delete model */
char *CreateImapMailboxExpungeUrl(const char *imapHost, 
								 const char *mailbox,
								 char  hierarchySeparator)
{
	static const char *formatString = "expunge>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), 
				formatString, 
				hierarchySeparator, 
				mailbox);   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* Creating a mailbox */
/* imap4://HOST>create>MAILBOXPATH */
char *CreateImapMailboxCreateUrl(const char *imapHost, const char *mailbox,char hierarchySeparator)
{
	static const char *formatString = "create>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}

/* discover the children of this mailbox */
char *CreateImapChildDiscoveryUrl(const char *imapHost, const char *mailbox,char hierarchySeparator)
{
	static const char *formatString = "discoverchildren>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}
/* discover the n-th level deep children of this mailbox */
char *CreateImapLevelChildDiscoveryUrl(const char *imapHost, const char *mailbox,char hierarchySeparator, int n)
{
	static const char *formatString = "discoverlevelchildren>%d>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, n, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}

/* deleting a mailbox */
/* imap4://HOST>delete>MAILBOXPATH */
char *CreateImapMailboxDeleteUrl(const char *imapHost, const char *mailbox, char hierarchySeparator)
{
	static const char *formatString = "delete>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}

/* renaming a mailbox */
/* imap4://HOST>rename>OLDNAME>NEWNAME */
char *CreateImapMailboxRenameLeafUrl(const char *imapHost, 
								 const char *oldBoxPathName,
								 char hierarchySeparator,
								 const char *newBoxLeafName)
{
	static const char *formatString = "rename>%c%s>%c%s";

	char *returnString = NULL;
	
	/* figure out the new mailbox name */
	char *slash;
	char *newPath = XP_ALLOC(XP_STRLEN(oldBoxPathName) + XP_STRLEN(newBoxLeafName) + 1);
	if (newPath)
	{
		XP_STRCPY (newPath, oldBoxPathName);
		slash = XP_STRRCHR (newPath, '/'); 
		if (slash)
			slash++;
		else
			slash = newPath;	/* renaming a 1st level box */
			
		XP_STRCPY (slash, newBoxLeafName);
		
		
		returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(oldBoxPathName) + XP_STRLEN(newPath));
		if (returnString)
			sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, oldBoxPathName, hierarchySeparator, newPath);
   /* Reviewed 4.51 safe use of sprintf */
                
		XP_FREE( newPath);
	}
	
	return returnString;
}

/* renaming a mailbox, moving hierarchy */
/* imap4://HOST>movefolderhierarchy>OLDNAME>NEWNAME */
/* oldBoxPathName is the old name of the child folder */
/* destinationBoxPathName is the name of the new parent */
char *CreateImapMailboxMoveFolderHierarchyUrl(const char *imapHost, 
								              const char *oldBoxPathName,
								              char  oldHierarchySeparator,
								              const char *newBoxPathName,
								              char  newHierarchySeparator)
{
	static const char *formatString = "movefolderhierarchy>%c%s>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(oldBoxPathName) + XP_STRLEN(newBoxPathName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, oldHierarchySeparator, oldBoxPathName, newHierarchySeparator, newBoxPathName);
   /* Reviewed 4.51 safe use of sprintf */

	return returnString;
}

/* listing available mailboxes */
/* imap4://HOST>list>referenceName>MAILBOXPATH */
/* MAILBOXPATH can contain wildcard */
/* **** jefft -- I am using this url to detect whether an mailbox
   exists on the Imap sever
 */
char *CreateImapListUrl(const char *imapHost,
						const char *mailbox,
						const char hierarchySeparator)
{
	static const char *formatString = "list>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost,
											  XP_STRLEN(formatString) +
											  XP_STRLEN(mailbox) + 1);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString,
				hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */

	return returnString;
}

/* biff */
char *CreateImapBiffUrl(const char *imapHost,
						const char *mailbox,
						char hierarchySeparator,
						uint32 uidHighWater)
{
	static const char *formatString = "biff>%c%s>%ld";
	
		/* 22 enough for huge uid string  */
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + 
														XP_STRLEN(mailbox) + 22);

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox, (long)uidHighWater);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}


static const char *sequenceString = "SEQUENCE";
static const char *uidString = "UID";

/* fetching RFC822 messages */
/* imap4://HOST>fetch><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will set the 'SEEN' flag */
char *CreateImapMessageFetchUrl(const char *imapHost,
								const char *mailbox,
								char hierarchySeparator,
								const char *messageIdentifierList,
								XP_Bool messageIdsAreUID)
{
	static const char *formatString = "fetch>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIdentifierList));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}

/* fetching the headers of RFC822 messages */
/* imap4://HOST>header><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will not affect the 'SEEN' flag */
char *CreateImapMessageHeaderUrl(const char *imapHost,
								 const char *mailbox,
								 char hierarchySeparator,
								 const char *messageIdentifierList,
								 XP_Bool messageIdsAreUID)
{
	static const char *formatString = "header>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIdentifierList));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
   /* Reviewed 4.51 safe use of sprintf */
        	
	return returnString;
}

/* search an online mailbox */
/* imap4://HOST>search><UID/SEQUENCE>>MAILBOXPATH>SEARCHSTRING */
/*   'x' is the message sequence number list */
char *CreateImapSearchUrl(const char *imapHost,
						  const char *mailbox,
						  char hierarchySeparator,
						  const char *searchString,
						  XP_Bool messageIdsAreUID)
{
	static const char *formatString = "search>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(searchString));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, searchString);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;	
}

/* delete messages */
/* imap4://HOST>deletemsg><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
char *CreateImapDeleteMessageUrl(const char *imapHost,
								 const char *mailbox,
								 char hierarchySeparator,
								 const char *messageIds,
								 XP_Bool idsAreUids)
{
	static const char *formatString = "deletemsg>%s>%c%s>%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* delete all messages */
/* imap4://HOST>deleteallmsgs>MAILBOXPATH */
char *CreateImapDeleteAllMessagesUrl(const char *imapHost,
								     const char *mailbox,
								     char hierarchySeparator)
{
	static const char *formatString = "deleteallmsgs>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* store +flags url */
/* imap4://HOST>store+flags><UID/SEQUENCE>>MAILBOXPATH>x>f */
/*   'x' is the message UID or sequence number list */
/*   'f' is the byte of flags */
char *CreateImapAddMessageFlagsUrl(const char *imapHost,
								   const char *mailbox,
								   char hierarchySeparator,
								   const char *messageIds,
								   imapMessageFlagsType flags,
								   XP_Bool idsAreUids)
{
	static const char *formatString = "addmsgflags>%s>%c%s>%s>%d";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds) + 10);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* store -flags url */
/* imap4://HOST>store-flags><UID/SEQUENCE>>MAILBOXPATH>x>f */
/*   'x' is the message UID or sequence number list */
/*   'f' is the byte of flags */
char *CreateImapSubtractMessageFlagsUrl(const char *imapHost,
								        const char *mailbox,
								        char hierarchySeparator,
								        const char *messageIds,
								        imapMessageFlagsType flags,
								        XP_Bool idsAreUids)
{
	static const char *formatString = "subtractmsgflags>%s>%c%s>%s>%d";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds) + 10);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* set flags url, make the flags match */
char *CreateImapSetMessageFlagsUrl(const char *imapHost,
								        const char *mailbox,
								   		char  hierarchySeparator,
								        const char *messageIds,
								        imapMessageFlagsType flags,
								        XP_Bool idsAreUids)
{
	static const char *formatString = "setmsgflags>%s>%c%s>%s>%d";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(sequenceString) + XP_STRLEN(mailbox) + XP_STRLEN(messageIds) + 10);
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* copy messages from one online box to another */
/* imap4://HOST>onlineCopy><UID/SEQUENCE>>SOURCEMAILBOXPATH>x>
			DESTINATIONMAILBOXPATH */
/*   'x' is the message UID or sequence number list */
char *CreateImapOnlineCopyUrl(const char *imapHost,
							  const char *sourceMailbox,
							  char  sourceHierarchySeparator,
							  const char *messageIds,
							  const char *destinationMailbox,
							  char  destinationHierarchySeparator,
							  XP_Bool idsAreUids,
							  XP_Bool isMove)
{
	static const char *formatString = "%s>%s>%c%s>%s>%c%s";
	static const char *moveString   = "onlinemove";
	static const char *copyString   = "onlinecopy";


	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(moveString) + XP_STRLEN(sequenceString) + XP_STRLEN(sourceMailbox) + XP_STRLEN(messageIds) + XP_STRLEN(destinationMailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, 
														isMove ? moveString : copyString, 
														idsAreUids ? uidString : sequenceString, 
														sourceHierarchySeparator, sourceMailbox,
														messageIds,
														destinationHierarchySeparator, destinationMailbox);

   /* Reviewed 4.51 safe use of sprintf */
	
	
	return returnString;
}

/* copy messages from one online box to another */
/* imap4://HOST>onlineCopy><UID/SEQUENCE>>SOURCEMAILBOXPATH>x>
			DESTINATIONMAILBOXPATH */
/*   'x' is the message UID or sequence number list */
char *CreateImapOnToOfflineCopyUrl(const char *imapHost,
							       const char *sourceMailbox,
							       char  sourceHierarchySeparator,
							       const char *messageIds,
							       const char *destinationMailbox,
							       XP_Bool idsAreUids,
							       XP_Bool isMove)
{
	static const char *formatString = "%s>%s>%c%s>%s>%c%s";
	static const char *moveString   = "onlinetoofflinemove";
	static const char *copyString   = "onlinetoofflinecopy";


	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(moveString) + XP_STRLEN(sequenceString) + XP_STRLEN(sourceMailbox) + XP_STRLEN(messageIds) + XP_STRLEN(destinationMailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, 
														isMove ? moveString : copyString, 
														idsAreUids ? uidString : sequenceString, 
														sourceHierarchySeparator, sourceMailbox,
														messageIds, 
														kOnlineHierarchySeparatorUnknown, destinationMailbox);
   /* Reviewed 4.51 safe use of sprintf */

	
	
	return returnString;
}

/* copy messages from an offline box to an online box */
/* imap4://HOST>offtoonCopy>SOURCEMAILBOXPATH>x>
			DESTINATIONMAILBOXPATH */
/*   'x' is the size of the message to upload */
char *CreateImapOffToOnlineCopyUrl(const char *imapHost,
							       const char *destinationMailbox,
							       char  destinationHierarchySeparator)
{
	static const char *formatString = "offlinetoonlinecopy>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(destinationMailbox));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, destinationHierarchySeparator, destinationMailbox);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* get mail account rul */
/* imap4://HOST>NETSCAPE */
char *CreateImapManageMailAccountUrl(const char *imapHost)
{
	static const char *formatString = "netscape";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + 1);
	StrAllocCat(returnString, formatString);;
	
	return returnString;
}

/* append message from file url */
/* imap4://HOST>appendmsgfromfile>DESTINATIONMAILBOXPATH */
char *CreateImapAppendMessageFromFileUrl(const char *imapHost,
										 const char *destinationMailboxPath,
										 const char hierarchySeparator,
										 XP_Bool isDraft)
{
	const char *formatString = isDraft ? "appenddraftfromfile>%c%s" :
		"appendmsgfromfile>%c%s";
	char *returnString = 
	  createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) +
						   XP_STRLEN(destinationMailboxPath));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, 
				hierarchySeparator, destinationMailboxPath);
   /* Reviewed 4.51 safe use of sprintf */

	return returnString;
}

/* Subscribe to a mailbox on the given IMAP host */
char *CreateIMAPSubscribeMailboxURL(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "subscribe>%c%s";	

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, delimiter, mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* Unsubscribe from a mailbox on the given IMAP host */
char *CreateIMAPUnsubscribeMailboxURL(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "unsubscribe>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, delimiter, mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}


/* Refresh the ACL for a folder on the given IMAP host */
char *CreateIMAPRefreshACLForFolderURL(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "refreshacl>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, delimiter, mailboxName);
	
	return returnString;

}

/* Refresh the ACL for all folders on the given IMAP host */
char *CreateIMAPRefreshACLForAllFoldersURL(const char *imapHost)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "refreshallacls>/";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* Auto-Upgrade to IMAP subscription */
char *CreateIMAPUpgradeToSubscriptionURL(const char *imapHost, XP_Bool subscribeToAll)
{
	static char *formatString = "upgradetosubscription>/";
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString));
	if (subscribeToAll)
		formatString[XP_STRLEN(formatString)-1] = '.';

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* do a status command on a folder on the given IMAP host */
char *CreateIMAPStatusFolderURL(const char *imapHost, const char *mailboxName, char  hierarchySeparator)
{
	static const char *formatString = "folderstatus>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));

	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), 
				formatString, 
				hierarchySeparator, 
				mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* Refresh the admin url for a folder on the given IMAP host */
char *CreateIMAPRefreshFolderURLs(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "refreshfolderurls>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, delimiter, mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* Force the reload of all parts of the message given in url */
char *IMAP_CreateReloadAllPartsUrl(const char *url)
{
	char *returnUrl = PR_smprintf("%s&allparts", url);
	return returnUrl;
}

/* Explicitly LIST a given mailbox, and refresh its flags in the folder list */
char *CreateIMAPListFolderURL(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "listfolder>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, XP_STRLEN(formatString) + XP_STRLEN(mailboxName));
	if (returnString)
		sprintf(returnString + XP_STRLEN(returnString), formatString, delimiter, mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}
#endif
