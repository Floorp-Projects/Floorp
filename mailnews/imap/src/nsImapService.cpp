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
#include "nsMsgImapCID.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsIIMAPHostSessionList.h"
#include "nsImapService.h"
#include "nsImapUrl.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolder.h"
#include "nsIImapIncomingServer.h"
#include "nsIImapServerSink.h"
#include "nsIImapMockChannel.h"
#include "nsImapUtils.h"
#include "nsIWebShell.h"
#include "nsIRDFService.h"
#include "nsIEventQueueService.h"
#include "nsXPIDLString.h"
#include "nsRDFCID.h"

#include "nsIMsgStatusFeedback.h"
#include "nsIPref.h"
#include "nsIProfile.h"

#define PREF_MAIL_ROOT_IMAP "mail.root.imap"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);

static const char *sequenceString = "SEQUENCE";
static const char *uidString = "UID";

NS_IMPL_THREADSAFE_ADDREF(nsImapService);
NS_IMPL_THREADSAFE_RELEASE(nsImapService);
NS_IMPL_QUERY_INTERFACE4(nsImapService,
                         nsIImapService,
                         nsIMsgMessageService,
                         nsIProtocolHandler,
                         nsIMsgProtocolInfo)

nsImapService::nsImapService()
{
    NS_INIT_REFCNT();
}

nsImapService::~nsImapService()
{
}

nsresult
nsImapService::GetFolderName(nsIMsgFolder* aImapFolder,
                             nsCString& folderName)
{
    nsresult rv;
    nsCOMPtr<nsIFolder> aFolder(do_QueryInterface(aImapFolder, &rv));
    if (NS_FAILED(rv)) return rv;
    char *uri = nsnull;
    rv = aFolder->GetURI(&uri);
    if (NS_FAILED(rv)) return rv;
    char * hostname = nsnull;
    rv = aImapFolder->GetHostname(&hostname);
    if (NS_FAILED(rv)) return rv;
    nsString name;
    rv = nsImapURI2FullName(kImapRootURI, hostname, uri, name);
    PR_FREEIF(uri);
    PR_FREEIF(hostname);
    if (NS_SUCCEEDED(rv))
        folderName = (const nsCString &) name;
    return rv;
}

NS_IMETHODIMP
nsImapService::SelectFolder(nsIEventQueue * aClientEventQueue, 
                            nsIMsgFolder * aImapMailFolder, 
                            nsIUrlListener * aUrlListener, 
							nsIMsgStatusFeedback *aStatusFeedback,
                            nsIURI ** aURL)
{


	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;
    nsresult rv;
	rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), aImapMailFolder, aUrlListener, urlSpec);

	if (NS_SUCCEEDED(rv) && imapUrl)
	{
        // nsImapUrl::SetSpec() will set the imap action properly
		// rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);
		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);

		nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
    	mailNewsUrl->SetStatusFeedback(aStatusFeedback);  
		mailNewsUrl->SetUpdatingFolder(PR_TRUE);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
            nsCString folderName;
            GetFolderName(aImapMailFolder, folderName);
			urlSpec.Append("/select>/");
            urlSpec.Append(folderName.GetBuffer());
		    rv = mailNewsUrl->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
                                                 imapUrl,
                                                 nsnull,
                                                 aURL);
		}
	} // if we have a url to run....

	return rv;
}

// lite select, used to verify UIDVALIDITY while going on/offline
NS_IMETHODIMP
nsImapService::LiteSelectFolder(nsIEventQueue * aClientEventQueue, 
                                nsIMsgFolder * aImapMailFolder, 
                                nsIUrlListener * aUrlListener, 
                                nsIURI ** aURL)
{

	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
	
	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv;
    
    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), aImapMailFolder, aUrlListener, urlSpec);

	if (NS_SUCCEEDED(rv))
	{
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/';
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append("/liteselect>");
			urlSpec.Append(hierarchySeparator);

            nsCString folderName;
            GetFolderName(aImapMailFolder, folderName);
			urlSpec.Append(folderName.GetBuffer());
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);
		}
	} // if we have a url to run....

	return rv;
}


NS_IMETHODIMP nsImapService::DisplayMessage(const char* aMessageURI,
                                            nsISupports * aDisplayConsumer,  
                                            nsIUrlListener * aUrlListener,
                                            nsIURI ** aURL) 
{
	nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgFolder> folder;
    nsXPIDLCString msgKey;

    rv = DecomposeImapURI(aMessageURI, getter_AddRefs(folder), getter_Copies(msgKey));
	if (NS_SUCCEEDED(rv))
	{
    	nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
		if (NS_SUCCEEDED(rv))
        {
           nsCOMPtr<nsIImapUrl> imapUrl;
           nsCAutoString urlSpec;
           rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec);
           if (NS_FAILED(rv)) return rv;

           rv = FetchMessage(imapUrl, nsIImapUrl::nsImapMsgFetch, folder, imapMessageSink,
                              aURL, aDisplayConsumer, msgKey, PR_TRUE);
        }
	}
	return rv;
}

NS_IMETHODIMP
nsImapService::CopyMessage(const char * aSrcMailboxURI, nsIStreamListener *
                           aMailboxCopy, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIURI **aURL)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsISupports> streamSupport;
    if (!aSrcMailboxURI || !aMailboxCopy) return rv;
    streamSupport = do_QueryInterface(aMailboxCopy, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgFolder> folder;
    nsXPIDLCString msgKey;
    rv = DecomposeImapURI(aSrcMailboxURI, getter_AddRefs(folder), getter_Copies(msgKey));
	if (NS_SUCCEEDED(rv))
	{
    	nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
		if (NS_SUCCEEDED(rv))
		{
            nsCOMPtr<nsIImapUrl> imapUrl;
            nsCAutoString urlSpec;
            rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec);

            // now try to display the message
			rv = FetchMessage(imapUrl, nsIImapUrl::nsImapMsgFetch, folder, imapMessageSink,
                              aURL, streamSupport, msgKey, PR_TRUE);
           if (NS_SUCCEEDED(rv) && moveMessage)
           {
               nsCOMPtr<nsIEventQueue> queue;	
               // get the Event Queue for this thread...
	           NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);

               if (NS_FAILED(rv)) return rv;

               rv = pEventQService->GetThreadEventQueue(PR_GetCurrentThread(), getter_AddRefs(queue));
               if (NS_FAILED(rv)) return rv;
               
               // ** jt -- this really isn't an optimal way of deleting a
               // list of messages but I don't have a better way at this
               // moment
               rv = AddMessageFlags(queue, folder, aUrlListener, nsnull,
                                    msgKey,
                                    kImapMsgDeletedFlag,
                                    PR_TRUE);
               // ** jt -- force to update the folder
               if (NS_SUCCEEDED(rv))
                  rv = SelectFolder(queue, folder, aUrlListener, nsnull, nsnull);
            } // if move message
        } // if we got an imap message sink
    } // if we decomposed the imap message 
    return rv;
}

// just a helper method to break down imap message URIs....
nsresult nsImapService::DecomposeImapURI(const char * aMessageURI, nsIMsgFolder ** aFolder, char ** aMsgKey)
{
    nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 
    if (NS_FAILED(rv)) return rv;

	nsCAutoString	folderURI;
    nsMsgKey msgKey;
	rv = nsParseImapMessageURI(aMessageURI, folderURI, &msgKey);
	if (NS_SUCCEEDED(rv))
	{
		nsIRDFResource* res;
		rv = rdf->GetResource(folderURI, &res);
		if (NS_FAILED(rv))	return rv;
        rv = res->QueryInterface(NS_GET_IID(nsIMsgFolder), (void **) aFolder);
        // convert the message key into a string
        if (msgKey)
        {
            nsCAutoString messageIdString;
            messageIdString.Append(msgKey, 10 /* base 10 */);
            *aMsgKey = messageIdString.ToNewCString();
        }
    }

    return rv;
}

NS_IMETHODIMP nsImapService::SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, 
												  PRBool aAddDummyEnvelope, nsIUrlListener *aUrlListener, nsIURI **aURL)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgFolder> folder;
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsXPIDLCString msgKey;

    rv = DecomposeImapURI(aMessageURI, getter_AddRefs(folder), getter_Copies(msgKey));
    if (NS_FAILED(rv)) return rv;
    
    nsCAutoString urlSpec;
    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec);
    if (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(imapUrl, &rv);
        if (NS_FAILED(rv)) return rv;
        msgUrl->SetMessageFile(aFile);
        msgUrl->SetAddDummyEnvelope(aAddDummyEnvelope);

        return FetchMessage(imapUrl, nsIImapUrl::nsImapSaveMessageToDisk, folder, imapMessageSink, aURL, imapMessageSink, msgKey, PR_TRUE);
    }

	return rv;
}

/* fetching RFC822 messages */
/* imap4://HOST>fetch><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will set the 'SEEN' flag */

NS_IMETHODIMP
nsImapService::FetchMessage(nsIImapUrl * aImapUrl,
                            nsIImapUrl::nsImapAction aImapAction,
                            nsIMsgFolder * aImapMailFolder, 
                            nsIImapMessageSink * aImapMessage,
                            nsIURI ** aURL,
							nsISupports * aDisplayConsumer, 
                            const char *messageIdentifierList,
                            PRBool messageIdsAreUID)
{
	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapUrl && aImapMailFolder &&  aImapMessage,"Oops ... null pointer");
    if (!aImapUrl || !aImapMailFolder || !aImapMessage)
        return NS_ERROR_NULL_POINTER;

    nsCAutoString urlSpec;
    nsresult rv = aImapUrl->SetImapAction(aImapAction /* nsIImapUrl::nsImapMsgFetch */);
    rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

	rv = aImapUrl->SetImapMessageSink(aImapMessage);
	if (NS_SUCCEEDED(rv))
	{
        nsXPIDLCString currentSpec;
        nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl);
        url->GetSpec(getter_Copies(currentSpec));
        urlSpec = currentSpec;

	    char hierarchySeparator = '/'; // ### fixme - should get from folder
		urlSpec.Append("fetch>");
		urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
		urlSpec.Append(">");
		urlSpec.Append(hierarchySeparator);

        nsCString folderName;
        GetFolderName(aImapMailFolder, folderName);
		urlSpec.Append(folderName.GetBuffer());
		urlSpec.Append(">");
		urlSpec.Append(messageIdentifierList);


		// mscott - this cast to a char * is okay...there's a bug in the XPIDL
		// compiler that is preventing in string parameters from showing up as
		// const char *. hopefully they will fix it soon.
		rv = url->SetSpec((char *) urlSpec.GetBuffer());

        // if the display consumer is a webshell, then we should run the url in the webshell.
        // otherwise, we'll run it normally....

        nsCOMPtr<nsIWebShell> webShell = do_QueryInterface(aDisplayConsumer, &rv);
        if (NS_SUCCEEDED(rv) && webShell)
            rv = webShell->LoadURI(url, "view", nsnull, PR_TRUE);
        else
        {
            nsCOMPtr<nsIEventQueue> queue;	
            // get the Event Queue for this thread...
	        NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);

            if (NS_FAILED(rv)) return rv;

            rv = pEventQService->GetThreadEventQueue(PR_GetCurrentThread(), getter_AddRefs(queue));
            if (NS_FAILED(rv)) return rv;
            rv = GetImapConnectionAndLoadUrl(queue, aImapUrl,
                                             aDisplayConsumer, aURL);

        }
	}
	return rv;
}

nsresult 
nsImapService::CreateStartOfImapUrl(nsIImapUrl ** imapUrl,
                                    nsIMsgFolder* aImapMailFolder,
                                    nsIUrlListener * aUrlListener,
                                    nsCString & urlSpec)
{
	nsresult rv = NS_OK;
    char *hostname = nsnull;
    char *username = nsnull;
    
    rv = aImapMailFolder->GetHostname(&hostname);
    if (NS_FAILED(rv)) return rv;
    rv = aImapMailFolder->GetUsername(&username);
    if (NS_FAILED(rv))
    {
        PR_FREEIF(hostname);
        return rv;
    }
	// now we need to create an imap url to load into the connection. The url
    // needs to represent a select folder action. 
	rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
                                            nsIImapUrl::GetIID(), (void **)
                                            imapUrl);
	if (NS_SUCCEEDED(rv) && imapUrl)
    {
        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(*imapUrl, &rv);
        if (NS_SUCCEEDED(rv) && mailnewsUrl && aUrlListener)
            mailnewsUrl->RegisterListener(aUrlListener);

		(*imapUrl)->Initialize(username);

        // *** jefft -- let's only do hostname now. I'll do username later
        // when the incoming server works were done. We might also need to
        // pass in the port number
        urlSpec = "imap://";
#if 0
        urlSpec.Append(username);
        urlSpec.Append('@');
#endif
        urlSpec.Append(hostname);
		urlSpec.Append(':');
		urlSpec.Append("143"); // mscott -- i know this is bogus...i'm i a hurry =)

        // *** jefft - force to parse the urlSpec in order to search for
        // the correct incoming server
 		// mscott - this cast to a char * is okay...there's a bug in the XPIDL
		// compiler that is preventing in string parameters from showing up as
		// const char *. hopefully they will fix it soon.
		rv = mailnewsUrl->SetSpec((char *) urlSpec.GetBuffer());

    }

    PR_FREEIF(hostname);
    PR_FREEIF(username);
	return rv;
}

/* fetching the headers of RFC822 messages */
/* imap4://HOST>header><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will not affect the 'SEEN' flag */
NS_IMETHODIMP
nsImapService::GetHeaders(nsIEventQueue * aClientEventQueue, 
                          nsIMsgFolder * aImapMailFolder, 
                          nsIUrlListener * aUrlListener, 
                          nsIURI ** aURL,
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
	
	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl),aImapMailFolder, aUrlListener, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{
        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

        rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/header>");
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.Append(hierarchySeparator);

            nsCString folderName;

            GetFolderName(aImapMailFolder, folderName);
			urlSpec.Append(folderName.GetBuffer());
            urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());

            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);

		}
	}
	return rv;
}


// Noop, used to update a folder (causes server to send changes).
NS_IMETHODIMP
nsImapService::Noop(nsIEventQueue * aClientEventQueue, 
                    nsIMsgFolder * aImapMailFolder,
                    nsIUrlListener * aUrlListener,
                    nsIURI ** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectNoopFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);
        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/selectnoop>");
			urlSpec.Append(hierarchySeparator);

            nsCString folderName;

            GetFolderName(aImapMailFolder, folderName);
			urlSpec.Append(folderName.GetBuffer());
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);
        }
	}
	return rv;
}

// Expunge, used to "compress" an imap folder,removes deleted messages.
NS_IMETHODIMP
nsImapService::Expunge(nsIEventQueue * aClientEventQueue, 
                       nsIMsgFolder * aImapMailFolder,
                       nsIUrlListener * aUrlListener, 
                       nsIURI ** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
	if (NS_SUCCEEDED(rv))
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapExpungeFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/Expunge>");
			urlSpec.Append(hierarchySeparator);

            nsCString folderName;

            GetFolderName(aImapMailFolder, folderName);
			urlSpec.Append(folderName.GetBuffer());
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);
		}
	}
	return rv;
}

/* old-stle biff that doesn't download headers */
NS_IMETHODIMP
nsImapService::Biff(nsIEventQueue * aClientEventQueue, 
                    nsIMsgFolder * aImapMailFolder,
                    nsIUrlListener * aUrlListener, 
                    nsIURI ** aURL,
                    PRUint32 uidHighWater)
{
  // static const char *formatString = "biff>%c%s>%ld";
	
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapExpungeFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);
        
        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder

			urlSpec.Append("/Biff>");
			urlSpec.Append(hierarchySeparator);

            nsCString folderName;
            GetFolderName(aImapMailFolder, folderName);
            urlSpec.Append(folderName.GetBuffer());
			urlSpec.Append(">");
			urlSpec.Append(uidHighWater, 10);
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);
		}
	}
	return rv;
}

NS_IMETHODIMP
nsImapService::DeleteFolder(nsIEventQueue* eventQueue,
                            nsIMsgFolder* folder,
                            nsIUrlListener* urlListener,
                            nsIURI** url)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    NS_ASSERTION(eventQueue && folder, 
                 "Oops ... [DeleteFolder] null eventQueue or folder");
    if (!eventQueue || ! folder)
        return rv;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;

    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), folder, urlListener, urlSpec);
    if (NS_SUCCEEDED(rv))
    {
        rv = SetImapUrlSink(folder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            char hierarchySeparator = kOnlineHierarchySeparatorUnknown;
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            urlSpec.Append("/delete>");
            urlSpec.Append(hierarchySeparator);
            
            nsCString folderName;
            rv = GetFolderName(folder, folderName);
            if (NS_SUCCEEDED(rv))
            {
                urlSpec.Append(folderName.GetBuffer());
                rv = uri->SetSpec((char*) urlSpec.GetBuffer());
                if (NS_SUCCEEDED(rv))
                    rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                         nsnull,
                                                         url);
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::DeleteMessages(nsIEventQueue * aClientEventQueue, 
                              nsIMsgFolder * aImapMailFolder, 
                              nsIUrlListener * aUrlListener, 
                              nsIURI ** aURL,
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
	
	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append("/deletemsg>");
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.Append(hierarchySeparator);

            nsCString folderName;

            GetFolderName(aImapMailFolder, folderName);
            urlSpec.Append(folderName.GetBuffer());
			urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);

        }
	}
	return rv;
}

// Delete all messages in a folder, used to empty trash
NS_IMETHODIMP
nsImapService::DeleteAllMessages(nsIEventQueue * aClientEventQueue, 
                                 nsIMsgFolder * aImapMailFolder,
                                 nsIUrlListener * aUrlListener, 
                                 nsIURI ** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
	if (NS_SUCCEEDED(rv))
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectNoopFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append("/deleteallmsgs>");
			urlSpec.Append(hierarchySeparator);
            nsCString folderName;
            GetFolderName(aImapMailFolder, folderName);
			urlSpec.Append(folderName.GetBuffer());
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);
		}
	}
	return rv;
}

NS_IMETHODIMP
nsImapService::AddMessageFlags(nsIEventQueue * aClientEventQueue,
                               nsIMsgFolder * aImapMailFolder, 
                               nsIUrlListener * aUrlListener, 
                               nsIURI ** aURL,
                               const char *messageIdentifierList,
                               imapMessageFlagsType flags,
                               PRBool messageIdsAreUID)
{
	return DiddleFlags(aClientEventQueue, aImapMailFolder, aUrlListener, aURL, messageIdentifierList,
		"addmsgflags", flags, messageIdsAreUID);
}

NS_IMETHODIMP
nsImapService::SubtractMessageFlags(nsIEventQueue * aClientEventQueue,
                                    nsIMsgFolder * aImapMailFolder, 
                                    nsIUrlListener * aUrlListener, 
                                    nsIURI ** aURL,
                                    const char *messageIdentifierList,
                                    imapMessageFlagsType flags,
                                    PRBool messageIdsAreUID)
{
	return DiddleFlags(aClientEventQueue, aImapMailFolder, aUrlListener, aURL, messageIdentifierList,
		"subtractmsgflags", flags, messageIdsAreUID);
}

NS_IMETHODIMP
nsImapService::SetMessageFlags(nsIEventQueue * aClientEventQueue,
                               nsIMsgFolder * aImapMailFolder, 
                               nsIUrlListener * aUrlListener, 
                               nsIURI ** aURL,
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

nsresult nsImapService::DiddleFlags(nsIEventQueue * aClientEventQueue, 
                                    nsIMsgFolder * aImapMailFolder, 
                                    nsIUrlListener * aUrlListener,
                                    nsIURI ** aURL,
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
	
	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;

	nsresult rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec); 
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			char hierarchySeparator = '/'; // ### fixme - should get from folder
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append('/');
			urlSpec.Append(howToDiddle);
			urlSpec.Append('>');
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.Append(hierarchySeparator);
            nsCString folderName;
            GetFolderName(aImapMailFolder, folderName);
            urlSpec.Append(folderName.GetBuffer());
			urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
			urlSpec.Append('>');
			urlSpec.Append(flags, 10);
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                  nsnull, aURL);
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
	nsCOMPtr <nsIMsgIncomingServer> incomingServer;
	nsCOMPtr <nsIImapServerSink> imapServerSink;

    NS_ASSERTION (aMsgFolder && aImapUrl, "Oops ... null pointers");
    if (!aMsgFolder || !aImapUrl)
        return rv;
    
	rv = aMsgFolder->GetServer(getter_AddRefs(incomingServer));
	if (NS_SUCCEEDED(rv) && incomingServer)
	{
		imapServerSink = do_QueryInterface(incomingServer);
		if (imapServerSink)
			aImapUrl->SetImapServerSink(imapServerSink);
	}
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
nsImapService::DiscoverAllFolders(nsIEventQueue* aClientEventQueue,
                                  nsIMsgFolder* aImapMailFolder,
                                  nsIUrlListener* aUrlListener,
                                  nsIURI** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> aImapUrl;
    nsCAutoString urlSpec;

    nsresult rv = CreateStartOfImapUrl(getter_AddRefs(aImapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
    if (NS_SUCCEEDED (rv))
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);

            urlSpec.Append("/discoverallboxes");
			nsCOMPtr <nsIURI> url = do_QueryInterface(aImapUrl, &rv);
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, aImapUrl,
                                                 nsnull, aURL);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::DiscoverAllAndSubscribedFolders(nsIEventQueue* aClientEventQueue,
                                              nsIMsgFolder* aImapMailFolder,
                                              nsIUrlListener* aUrlListener,
                                              nsIURI** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> aImapUrl;
    nsCAutoString urlSpec;

    nsresult rv = CreateStartOfImapUrl(getter_AddRefs(aImapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
    if (NS_SUCCEEDED (rv) && aImapUrl)
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);

            urlSpec.Append("/discoverallandsubscribedboxes");
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, aImapUrl,
                                                 nsnull, aURL);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::DiscoverChildren(nsIEventQueue* aClientEventQueue,
                                nsIMsgFolder* aImapMailFolder,
                                nsIUrlListener* aUrlListener,
                                nsIURI** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> aImapUrl;
    nsCAutoString urlSpec;

    nsresult rv = CreateStartOfImapUrl(getter_AddRefs(aImapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec);
    if (NS_SUCCEEDED (rv))
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            nsCString folderName;
            GetFolderName(aImapMailFolder, folderName);
            if (folderName.Length() > 0)
            {
                nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);

                // **** fix me with host specific hierarchySeparator please
                urlSpec.Append("/discoverchildren>/");
                urlSpec.Append(folderName.GetBuffer());
    			// mscott - this cast to a char * is okay...there's a bug in the XPIDL
				// compiler that is preventing in string parameters from showing up as
				// const char *. hopefully they will fix it soon.
				rv = uri->SetSpec((char *) urlSpec.GetBuffer());
                if (NS_SUCCEEDED(rv))
                    rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
                                                     aImapUrl,
                                                     nsnull, aURL);
            }
            else
            {
                rv = NS_ERROR_NULL_POINTER;
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::DiscoverLevelChildren(nsIEventQueue* aClientEventQueue,
                                     nsIMsgFolder* aImapMailFolder,
                                     nsIUrlListener* aUrlListener,
                                     PRInt32 level,
                                     nsIURI** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> aImapUrl;
    nsCAutoString urlSpec;

    nsresult rv = CreateStartOfImapUrl(getter_AddRefs(aImapUrl),
                                          aImapMailFolder,aUrlListener, urlSpec);
    if (NS_SUCCEEDED (rv) && aImapUrl)
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            nsCString folderName;
            GetFolderName(aImapMailFolder, folderName);
            if (folderName.Length() > 0)
            {
                nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);
                urlSpec.Append("/discoverlevelchildren>");
                urlSpec.Append(level);
                // **** fix me with host specific hierarchySeparator please
                urlSpec.Append("/"); // hierarchySeparator "/"
                urlSpec.Append(folderName.GetBuffer());

				rv = uri->SetSpec((char *) urlSpec.GetBuffer());
                if (NS_SUCCEEDED(rv))
                    rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
                                                     aImapUrl,
                                                     nsnull, aURL);
            }
            else
                rv = NS_ERROR_NULL_POINTER;
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::OnlineMessageCopy(nsIEventQueue* aClientEventQueue,
                                 nsIMsgFolder* aSrcFolder,
                                 const char* messageIds,
                                 nsIMsgFolder* aDstFolder,
                                 PRBool idsAreUids,
                                 PRBool isMove,
                                 nsIUrlListener* aUrlListener,
                                 nsIURI** aURL,
                                 nsISupports* copyState)
{
    NS_ASSERTION(aSrcFolder && aDstFolder && messageIds && aClientEventQueue,
                 "Fatal ... missing key parameters");
    if (!aClientEventQueue || !aSrcFolder || !aDstFolder || !messageIds ||
        *messageIds == 0)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_ERROR_FAILURE;
    char *srcHostname = nsnull, *srcUsername = nsnull;
    char *dstHostname = nsnull, *dstUsername = nsnull;
    rv = aSrcFolder->GetHostname(&srcHostname);
    if (NS_FAILED(rv)) return rv;
    rv = aDstFolder->GetHostname(&dstHostname);
    if (NS_FAILED(rv))
    {
        PR_FREEIF(srcHostname);
        return rv;
    }
    rv = aSrcFolder->GetUsername(&srcUsername);
    if (NS_FAILED(rv))
    {
        PR_FREEIF(srcHostname);
        PR_FREEIF(dstHostname);
        return rv;
    }
    rv = aDstFolder->GetUsername(&dstUsername);
    if (NS_FAILED(rv))
    {
        PR_FREEIF(srcHostname);
        PR_FREEIF(srcUsername);
        PR_FREEIF(dstHostname);
        return rv;
    }
    if (PL_strcmp(srcHostname, dstHostname) ||
        PL_strcmp(srcUsername, dstUsername))
    {
        // *** can only take message from the same imap host and user accnt
        PR_FREEIF(srcHostname);
        PR_FREEIF(srcUsername);
        PR_FREEIF(dstHostname);
        PR_FREEIF(dstUsername);
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;

    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), aSrcFolder, aUrlListener, urlSpec);
    if (NS_SUCCEEDED(rv))
    {
        // **** fix me with real host hierarchy separator
        char hierarchySeparator = kOnlineHierarchySeparatorUnknown;
        SetImapUrlSink(aSrcFolder, imapUrl);
        imapUrl->SetCopyState(copyState);

        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

        if (isMove)
            urlSpec.Append("/onlinemove>");
        else
            urlSpec.Append("/onlinecopy>");
        if (idsAreUids)
            urlSpec.Append(uidString);
        else
            urlSpec.Append(sequenceString);
        urlSpec.Append('>');
        urlSpec.Append(hierarchySeparator);

        nsCString folderName;
        GetFolderName(aSrcFolder, folderName);
        urlSpec.Append(folderName.GetBuffer());
        urlSpec.Append('>');
        urlSpec.Append(messageIds);
        urlSpec.Append('>');
        urlSpec.Append(hierarchySeparator);
        folderName = "";
        GetFolderName(aDstFolder, folderName);
        urlSpec.Append(folderName);

		rv = uri->SetSpec((char *) urlSpec.GetBuffer());
        if (NS_SUCCEEDED(rv))
            rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                             nsnull, aURL);
    }
    PR_FREEIF(srcHostname);
    PR_FREEIF(srcUsername);
    PR_FREEIF(dstHostname);
    PR_FREEIF(dstUsername);
    return rv;
}

/* append message from file url */
/* imap://HOST>appendmsgfromfile>DESTINATIONMAILBOXPATH */
/* imap://HOST>appenddraftfromfile>DESTINATIONMAILBOXPATH>UID>messageId */
NS_IMETHODIMP
nsImapService::AppendMessageFromFile(nsIEventQueue* aClientEventQueue,
                                     nsIFileSpec* aFileSpec,
                                     nsIMsgFolder* aDstFolder,
                                     const char* messageId, // te be replaced
                                     PRBool idsAreUids,
                                     PRBool inSelectedState, // needs to be in
                                     nsIUrlListener* aListener,
                                     nsIURI** aURL,
                                     nsISupports* aCopyState)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!aClientEventQueue || !aFileSpec || !aDstFolder)
        return rv;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;

    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), aDstFolder, aListener, urlSpec);
    if (NS_SUCCEEDED(rv))
    {
        // **** fix me with real host hierarchy separator
        char hierarchySeparator = kOnlineHierarchySeparatorUnknown;
        SetImapUrlSink(aDstFolder, imapUrl);
        imapUrl->SetMsgFileSpec(aFileSpec);
        imapUrl->SetCopyState(aCopyState);

        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

        if (inSelectedState)
            urlSpec.Append("/appenddraftfromfile>");
        else
            urlSpec.Append("/appendmsgfromfile>");

        urlSpec.Append(hierarchySeparator);
        
        nsCString folderName;
        GetFolderName(aDstFolder, folderName);
        urlSpec.Append(folderName);

        if (inSelectedState)
        {
            urlSpec.Append('>');
            if (idsAreUids)
                urlSpec.Append(uidString);
            else
                urlSpec.Append(sequenceString);
            urlSpec.Append('>');
            if (messageId)
                urlSpec.Append(messageId);
        }

        rv = uri->SetSpec((char *) urlSpec.GetBuffer());
        if (NS_SUCCEEDED(rv))
            rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                             nsnull, aURL);
    }
    return rv;
}

nsresult
nsImapService::GetImapConnectionAndLoadUrl(nsIEventQueue* aClientEventQueue,
                                           nsIImapUrl* aImapUrl,
                                           nsISupports* aConsumer,
                                           nsIURI** aURL)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;
	nsCOMPtr<nsIMsgMailNewsUrl> msgUrl = do_QueryInterface(aImapUrl);
    rv = msgUrl->GetServer(getter_AddRefs(aMsgIncomingServer));
    
    if (aURL)
    {
        *aURL = msgUrl;
        NS_IF_ADDREF(*aURL);
    }

    if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
    {
        nsCOMPtr<nsIImapIncomingServer>
            aImapServer(do_QueryInterface(aMsgIncomingServer, &rv));
        if (NS_SUCCEEDED(rv) && aImapServer)
            rv = aImapServer->GetImapConnectionAndLoadUrl(aClientEventQueue,
                                                          aImapUrl,
                                                          aConsumer);
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::MoveFolder(nsIEventQueue* eventQueue, nsIMsgFolder* srcFolder,
                          nsIMsgFolder* dstFolder, 
                          nsIUrlListener* urlListener, nsIURI** url)
{
    NS_ASSERTION(eventQueue && srcFolder && dstFolder, 
                 "Oops ... null pointer");
    if (!eventQueue || !srcFolder || !dstFolder)
        return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;

    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), dstFolder, urlListener, urlSpec);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(dstFolder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            char hierarchySeparator = kOnlineHierarchySeparatorUnknown;
            nsCString folderName;
            
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);
            GetFolderName(srcFolder, folderName);
            urlSpec.Append("/movefolderhierarchy>");
            urlSpec.Append(hierarchySeparator);
            urlSpec.Append(folderName.GetBuffer());
            urlSpec.Append('>');
            folderName.SetLength(0);
            GetFolderName(dstFolder, folderName);
            urlSpec.Append(hierarchySeparator);
            urlSpec.Append(folderName.GetBuffer());
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull,
                                                 url);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::RenameLeaf(nsIEventQueue* eventQueue, nsIMsgFolder* srcFolder,
                          const char* newLeafName, nsIUrlListener* urlListener,
                          nsIURI** url)
{
    NS_ASSERTION(eventQueue && srcFolder && newLeafName && *newLeafName,
                 "Oops ... [RenameLeaf] null pointers");
    if (!eventQueue || !srcFolder || !newLeafName || !*newLeafName)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;

    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), srcFolder, urlListener, urlSpec);
    if (NS_SUCCEEDED(rv))
    {
        rv = SetImapUrlSink(srcFolder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            char hierarchySeparator = '/';
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            nsCString folderName;
            GetFolderName(srcFolder, folderName);
            urlSpec.Append("/rename>");
            urlSpec.Append(hierarchySeparator);
            urlSpec.Append(folderName.GetBuffer());
            urlSpec.Append('>');
            urlSpec.Append(hierarchySeparator);
            PRInt32 leafNameStart = 
                folderName.RFindChar('/'); // ** troublesome hierarchyseparator
            if (leafNameStart != -1)
            {
                folderName.SetLength(leafNameStart+1);
                urlSpec.Append(folderName.GetBuffer());
            }
            urlSpec.Append(newLeafName);
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull, url);
        } // if (NS_SUCCEEDED(rv))
    } // if (NS_SUCCEEDED(rv) && imapUrl)
    return rv;
}

NS_IMETHODIMP
nsImapService::CreateFolder(nsIEventQueue* eventQueue, nsIMsgFolder* parent,
                            const char* newFolderName, 
                            nsIUrlListener* urlListener, nsIURI** url)
{
    NS_ASSERTION(eventQueue && parent && newFolderName && *newFolderName,
                 "Oops ... [RenameLeaf] null pointers");
    if (!eventQueue || !parent || !newFolderName || !*newFolderName)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;

    rv = CreateStartOfImapUrl(getter_AddRefs(imapUrl), parent, urlListener, urlSpec);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(parent, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            char hierarchySeparator = '/';
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            nsCString folderName;
            GetFolderName(parent, folderName);
            urlSpec.Append("/create>");
            urlSpec.Append(hierarchySeparator);
            if (folderName.Length() > 0)
            {
                urlSpec.Append(folderName.GetBuffer());
                urlSpec.Append(hierarchySeparator);
            }
            urlSpec.Append(newFolderName);
    
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                     nsnull,
                                                     url);
        } // if (NS_SUCCEEDED(rv))
    } // if (NS_SUCCEEDED(rv) && imapUrl)
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(messageIdentifierList));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
   /* Reviewed 4.51 safe use of sprintf */
        	
	return returnString;
}

/* Noop, used to reset timer or download new headers for a selected folder */
char *CreateImapMailboxNoopUrl(const char *imapHost, 
								 const char *mailbox,
								 char  hierarchySeparator)
{
	static const char *formatString = "selectnoop>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailbox));

	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), 
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
	
	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailbox));

	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), 
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}

/* discover the children of this mailbox */
char *CreateImapChildDiscoveryUrl(const char *imapHost, const char *mailbox,char hierarchySeparator)
{
	static const char *formatString = "discoverchildren>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}
/* discover the n-th level deep children of this mailbox */
char *CreateImapLevelChildDiscoveryUrl(const char *imapHost, const char *mailbox,char hierarchySeparator, int n)
{
	static const char *formatString = "discoverlevelchildren>%d>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, n, hierarchySeparator, mailbox);
   /* Reviewed 4.51 safe use of sprintf */
        
	return returnString;
}

/* deleting a mailbox */
/* imap4://HOST>delete>MAILBOXPATH */
char *CreateImapMailboxDeleteUrl(const char *imapHost, const char *mailbox, char hierarchySeparator)
{
	static const char *formatString = "delete>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, hierarchySeparator, mailbox);
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
	char *newPath = PR_Malloc(nsCRT::strlen(oldBoxPathName) + nsCRT::strlen(newBoxLeafName) + 1);
	if (newPath)
	{
		nsCRT::strcpy (newPath, oldBoxPathName);
		slash = nsCRT::strrchr (newPath, '/'); 
		if (slash)
			slash++;
		else
			slash = newPath;	/* renaming a 1st level box */
			
		nsCRT::strcpy (slash, newBoxLeafName);
		
		
		returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(oldBoxPathName) + nsCRT::strlen(newPath));
		if (returnString)
			sprintf(returnString + nsCRT::strlen(returnString), formatString, hierarchySeparator, oldBoxPathName, hierarchySeparator, newPath);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(oldBoxPathName) + nsCRT::strlen(newBoxPathName));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, oldHierarchySeparator, oldBoxPathName, newHierarchySeparator, newBoxPathName);
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
											  nsCRT::strlen(formatString) +
											  nsCRT::strlen(mailbox) + 1);
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString,
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
	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + 
														nsCRT::strlen(mailbox) + 22);

	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, hierarchySeparator, mailbox, (long)uidHighWater);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(messageIdentifierList));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(messageIdentifierList));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, messageIdentifierList);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(searchString));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, messageIdsAreUID ? uidString : sequenceString, hierarchySeparator, mailbox, searchString);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(messageIds));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, hierarchySeparator, mailbox);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(messageIds) + 10);
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(messageIds) + 10);
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(mailbox) + nsCRT::strlen(messageIds) + 10);
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, idsAreUids ? uidString : sequenceString, hierarchySeparator, mailbox, messageIds, (int) flags);
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


	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(moveString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(sourceMailbox) + nsCRT::strlen(messageIds) + nsCRT::strlen(destinationMailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, 
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


	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(moveString) + nsCRT::strlen(sequenceString) + nsCRT::strlen(sourceMailbox) + nsCRT::strlen(messageIds) + nsCRT::strlen(destinationMailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, 
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(destinationMailbox));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, destinationHierarchySeparator, destinationMailbox);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}

/* get mail account rul */
/* imap4://HOST>NETSCAPE */
char *CreateImapManageMailAccountUrl(const char *imapHost)
{
	static const char *formatString = "netscape";

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + 1);
	StrAllocCat(returnString, formatString);;
	
	return returnString;
}


/* Subscribe to a mailbox on the given IMAP host */
char *CreateIMAPSubscribeMailboxURL(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "subscribe>%c%s";	

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailboxName));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, delimiter, mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* Unsubscribe from a mailbox on the given IMAP host */
char *CreateIMAPUnsubscribeMailboxURL(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "unsubscribe>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailboxName));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, delimiter, mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}


/* Refresh the ACL for a folder on the given IMAP host */
char *CreateIMAPRefreshACLForFolderURL(const char *imapHost, const char *mailboxName, char delimiter)
{
	static const char *formatString = "refreshacl>%c%s";

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailboxName));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, delimiter, mailboxName);
	
	return returnString;

}

/* Refresh the ACL for all folders on the given IMAP host */
char *CreateIMAPRefreshACLForAllFoldersURL(const char *imapHost)
{
	/* we don't need the hierarchy delimiter, so just use slash ("/") */
	static const char *formatString = "refreshallacls>/";

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* Auto-Upgrade to IMAP subscription */
char *CreateIMAPUpgradeToSubscriptionURL(const char *imapHost, XP_Bool subscribeToAll)
{
	static char *formatString = "upgradetosubscription>/";
	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString));
	if (subscribeToAll)
		formatString[nsCRT::strlen(formatString)-1] = '.';

	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;

}

/* do a status command on a folder on the given IMAP host */
char *CreateIMAPStatusFolderURL(const char *imapHost, const char *mailboxName, char  hierarchySeparator)
{
	static const char *formatString = "folderstatus>%c%s";
	
	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailboxName));

	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), 
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailboxName));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, delimiter, mailboxName);
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

	char *returnString = createStartOfIMAPurl(imapHost, nsCRT::strlen(formatString) + nsCRT::strlen(mailboxName));
	if (returnString)
		sprintf(returnString + nsCRT::strlen(returnString), formatString, delimiter, mailboxName);
   /* Reviewed 4.51 safe use of sprintf */
	
	return returnString;
}
#endif


NS_IMETHODIMP nsImapService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
        *aScheme = nsCRT::strdup("imap");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsImapService::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = IMAP_PORT;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 	
}

NS_IMETHODIMP nsImapService::MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI, char **_retval)
{
	// no such thing as relative urls for smtp.....
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsImapService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	// i just haven't implemented this yet...I will be though....
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsImapService::NewChannel(const char *verb, nsIURI *aURI, nsILoadGroup *aGroup, nsIEventSinkGetter *eventSinkGetter, nsIChannel **_retval)
{
    // imap can't open and return a channel right away...the url needs to go in the imap url queue 
    // until we find a connection which can run the url..in order to satisfy necko, we're going to return
    // a mock imap channel....

    nsresult rv = NS_OK;
    nsCOMPtr<nsIImapMockChannel> mockChannel;
    nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aURI, &rv);

    if (NS_FAILED(rv)) return rv;

    rv = imapUrl->GetMockChannel(getter_AddRefs(mockChannel));
    if (NS_FAILED(rv) || !mockChannel) return rv;

    mockChannel->SetLoadGroup(aGroup);
    *_retval = mockChannel;
    NS_IF_ADDREF(*_retval);

    return rv;
}

NS_IMETHODIMP
nsImapService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->SetFilePref(PREF_MAIL_ROOT_IMAP, aPath, PR_FALSE /* set default */);
    return rv;
}       

NS_IMETHODIMP
nsImapService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->GetFilePref(PREF_MAIL_ROOT_IMAP, aResult);
    if (NS_SUCCEEDED(rv)) return rv;

    NS_WITH_SERVICE(nsIProfile, profile, NS_PROFILE_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsFileSpec dir;
    rv = profile->GetCurrentProfileDir(&dir);
    if (NS_FAILED(rv)) return rv;

    // we want <profile>/ImapMail not <profile>
    dir += "ImapMail";

    rv = NS_NewFileSpecWithSpec(dir, aResult);
    if (NS_FAILED(rv)) return rv;

    rv = SetDefaultLocalPath(*aResult);
    return rv;
}
    
