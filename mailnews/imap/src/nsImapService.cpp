/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIMsgImapMailFolder.h"
#include "nsIImapIncomingServer.h"
#include "nsIImapServerSink.h"
#include "nsIImapMockChannel.h"
#include "nsImapUtils.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIRDFService.h"
#include "nsIEventQueueService.h"
#include "nsXPIDLString.h"
#include "nsRDFCID.h"
#include "nsEscape.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIPref.h"
#include "nsILoadGroup.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgFolderFlags.h"
#include "nsISubscribableServer.h"
#include "nsIMessage.h"

#define PREF_MAIL_ROOT_IMAP "mail.root.imap"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_IID(kIFileLocatorIID,      NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID,       NS_FILELOCATOR_CID);


static const char *sequenceString = "SEQUENCE";
static const char *uidString = "UID";

static PRBool gInitialized = PR_FALSE;
static PRInt32 gMIMEOnDemandThreshold = 15000;
static PRBool gMIMEOnDemand = PR_FALSE;

NS_IMPL_THREADSAFE_ADDREF(nsImapService);
NS_IMPL_THREADSAFE_RELEASE(nsImapService);
NS_IMPL_QUERY_INTERFACE5(nsImapService,
                         nsIImapService,
                         nsIMsgMessageService,
                         nsIProtocolHandler,
                         nsIMsgProtocolInfo,
                         nsIMsgMessageFetchPartService)

nsImapService::nsImapService()
{
  NS_INIT_REFCNT();
  mPrintingOperation = PR_FALSE;
  if (!gInitialized)
  {
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
    if (NS_SUCCEEDED(rv) && prefs) 
    {
	    prefs->GetBoolPref("mail.imap.mime_parts_on_demand", &gMIMEOnDemand);
	    prefs->GetIntPref("mail.imap.mime_parts_on_demand_threshold", &gMIMEOnDemandThreshold);
    }
    gInitialized = PR_TRUE;
  }
}

nsImapService::~nsImapService()
{
}

PRUnichar nsImapService::GetHierarchyDelimiter(nsIMsgFolder* aMsgFolder)
{
    PRUnichar delimiter = '/';
    if (aMsgFolder)
    {
        nsCOMPtr<nsIMsgImapMailFolder> imapFolder =
            do_QueryInterface(aMsgFolder); 
        if (imapFolder)
            imapFolder->GetHierarchyDelimiter(&delimiter);
    }
    return delimiter;
}

// N.B., this returns an escaped folder name, appropriate for putting in a url.
nsresult
nsImapService::GetFolderName(nsIMsgFolder* aImapFolder,
                             char **folderName)
{
    nsresult rv;
    nsCOMPtr<nsIMsgImapMailFolder> aFolder(do_QueryInterface(aImapFolder, &rv));
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString onlineName;
	// online name is in imap utf-7 - leave it that way
    rv = aFolder->GetOnlineName(getter_Copies(onlineName));

    if (NS_FAILED(rv)) return rv;
	if ((const char *)onlineName == nsnull || nsCRT::strlen((const char *) onlineName) == 0)
	{
		char *uri = nsnull;
		rv = aImapFolder->GetURI(&uri);
		if (NS_FAILED(rv)) return rv;
		char * hostname = nsnull;
		rv = aImapFolder->GetHostname(&hostname);
		if (NS_FAILED(rv)) return rv;
		rv = nsImapURI2FullName(kImapRootURI, hostname, uri, getter_Copies(onlineName));
		PR_FREEIF(uri);
		PR_FREEIF(hostname);
	}
	*folderName = nsEscape((const char *) onlineName, url_Path);
    return rv;
}

NS_IMETHODIMP
nsImapService::SelectFolder(nsIEventQueue * aClientEventQueue, 
                            nsIMsgFolder * aImapMailFolder, 
                            nsIUrlListener * aUrlListener, 
							              nsIMsgWindow *aMsgWindow,
                            nsIURI ** aURL)
{


	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
    NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                  "Oops ... null pointer");
    if (!aImapMailFolder || !aClientEventQueue)
        return NS_ERROR_NULL_POINTER;

    PRBool noSelect = PR_FALSE;
    aImapMailFolder->GetFlag(MSG_FOLDER_FLAG_IMAP_NOSELECT, &noSelect);

    if (noSelect) return NS_OK;

	nsCOMPtr<nsIImapUrl> imapUrl;
	nsCAutoString urlSpec;
    nsresult rv;
	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
	rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aImapMailFolder, aUrlListener, urlSpec, hierarchySeparator);

	if (NS_SUCCEEDED(rv) && imapUrl)
	{
        // nsImapUrl::SetSpec() will set the imap action properly
		// rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);
		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);

		nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
		mailNewsUrl->SetMsgWindow(aMsgWindow);
		mailNewsUrl->SetUpdatingFolder(PR_TRUE);
		imapUrl->AddChannelToLoadGroup();
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
      nsXPIDLCString folderName;
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
			urlSpec.Append("/select>");
			urlSpec.AppendWithConversion(hierarchySeparator);
      urlSpec.Append((const char *) folderName);
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
    
	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aImapMailFolder, aUrlListener, urlSpec, hierarchySeparator);

	if (NS_SUCCEEDED(rv))
	{
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append("/liteselect>");
			urlSpec.AppendWithConversion(hierarchySeparator);

            nsXPIDLCString folderName;
            GetFolderName(aImapMailFolder, getter_Copies(folderName));
			urlSpec.Append((const char *) folderName);
			rv = uri->SetSpec((char *) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                                 nsnull, aURL);
		}
	} // if we have a url to run....

	return rv;
}

NS_IMETHODIMP nsImapService::GetUrlForUri(const char *aMessageURI, nsIURI **aURL, nsIMsgWindow *aMsgWindow) 
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
      PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);;
      rv = CreateStartOfImapUrl(aMessageURI, getter_AddRefs(imapUrl), folder, nsnull, urlSpec, hierarchySeparator);
      if (NS_FAILED(rv)) return rv;
    	  imapUrl->SetImapMessageSink(imapMessageSink);

      nsCOMPtr<nsIURI> url = do_QueryInterface(imapUrl);
      nsXPIDLCString currentSpec;
      url->GetSpec(getter_Copies(currentSpec));
      urlSpec.Assign(currentSpec);

		  urlSpec.Append("fetch>");
		  urlSpec.Append(uidString);
		  urlSpec.Append(">");
		  urlSpec.AppendWithConversion(hierarchySeparator);

      nsXPIDLCString folderName;
      GetFolderName(folder, getter_Copies(folderName));
		  urlSpec.Append((const char *) folderName);
		  urlSpec.Append(">");
		  urlSpec.Append(msgKey);
		  rv = url->SetSpec((char *) urlSpec.GetBuffer());
      imapUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);
    }
  }

  return rv;
}

NS_IMETHODIMP nsImapService::OpenAttachment(const char *aContentType, 
                                            const char *aFileName,
                                            const char *aUrl, 
                                            const char *aMessageUri, 
                                            nsISupports *aDisplayConsumer, 
                                            nsIMsgWindow *aMsgWindow, 
                                            nsIUrlListener *aUrlListener)
{
  nsresult rv = NS_OK;
  // okay this is a little tricky....we may have to fetch the mime part
  // or it may already be downloaded for us....the only way i can tell to 
  // distinguish the two events is to search for ?section or ?part

  nsCAutoString uri(aMessageUri);
  nsCAutoString urlString(aUrl);
  urlString.ReplaceSubstring("/;section", "?section");

  // more stuff i don't understand
  PRInt32 sectionPos = urlString.Find("?section");
  // if we have a section field then we must be dealing with a mime part we need to fetchf
  if (sectionPos > 0)
  {
    nsCAutoString mimePart;

    urlString.Right(mimePart, urlString.Length() - sectionPos); 
    uri.Append(mimePart);
    uri += "&type=";
    uri += aContentType;
  }
  else
  {
    // try to extract the specific part number out from the url string
    uri += "?";
    const char *part = PL_strstr(aUrl, "part=");
    uri += part;
    uri += "&type=";
    uri += aContentType;
  }
  
  nsCOMPtr<nsIMsgFolder> folder;
  nsXPIDLCString msgKey;
  nsXPIDLCString uriMimePart;
	nsCAutoString	folderURI;
	nsMsgKey key;

  rv = DecomposeImapURI(uri, getter_AddRefs(folder), getter_Copies(msgKey));
	rv = nsParseImapMessageURI(uri, folderURI, &key, getter_Copies(uriMimePart));
	if (NS_SUCCEEDED(rv))
	{
    nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
		if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIImapUrl> imapUrl;
      nsCAutoString urlSpec;
      PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
      rv = CreateStartOfImapUrl(uri, getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec, hierarchySeparator);
      if (NS_FAILED(rv)) 
        return rv;
      if (uriMimePart)
      {
        nsCOMPtr<nsIMsgMailNewsUrl> mailUrl (do_QueryInterface(imapUrl));
        if (mailUrl)
          mailUrl->SetFileName(aFileName);
        rv =  FetchMimePart(imapUrl, nsIImapUrl::nsImapOpenMimePart, folder, imapMessageSink,
                        nsnull, aDisplayConsumer, msgKey, uriMimePart);
      }
    } // if we got a message sink
  } // if we parsed the message uri

  return rv;
}

/* void OpenAttachment (in nsIURI aURI, in nsISupports aDisplayConsumer, in nsIMsgWindow aMsgWindow, in nsIUrlListener aUrlListener, out nsIURI aURL); */
NS_IMETHODIMP nsImapService::FetchMimePart(nsIURI *aURI, const char *aMessageURI, nsISupports *aDisplayConsumer, nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIURI **aURL)
{
	nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgFolder> folder;
  nsXPIDLCString msgKey;
  nsXPIDLCString mimePart;
	nsCAutoString	folderURI;
	nsMsgKey key;

  rv = DecomposeImapURI(aMessageURI, getter_AddRefs(folder), getter_Copies(msgKey));
	rv = nsParseImapMessageURI(aMessageURI, folderURI, &key, getter_Copies(mimePart));
	if (NS_SUCCEEDED(rv))
	{
    	nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
		if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aURI);
      nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(aURI));

      msgurl->SetMsgWindow(aMsgWindow);
      msgurl->RegisterListener(aUrlListener);

      if (mimePart)
      {
        return FetchMimePart(imapUrl, nsIImapUrl::nsImapMsgFetch, folder, imapMessageSink,
                        aURL, aDisplayConsumer, msgKey, mimePart);
      }
    }
  }
  return rv;
}


NS_IMETHODIMP nsImapService::DisplayMessage(const char* aMessageURI,
                                            nsISupports * aDisplayConsumer,  
                                            nsIMsgWindow * aMsgWindow,
                                            nsIUrlListener * aUrlListener,
                                            const PRUnichar * aCharsetOverride,
                                            nsIURI ** aURL) 
{
	nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgFolder> folder;
  nsXPIDLCString msgKey;
  nsXPIDLCString mimePart;
	nsCAutoString	folderURI;
	nsMsgKey key;

  rv = DecomposeImapURI(aMessageURI, getter_AddRefs(folder), getter_Copies(msgKey));
	rv = nsParseImapMessageURI(aMessageURI, folderURI, &key, getter_Copies(mimePart));
	if (NS_SUCCEEDED(rv))
	{
    	nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
		if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIImapUrl> imapUrl;
      nsCAutoString urlSpec;
      PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
      rv = CreateStartOfImapUrl(aMessageURI, getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec, hierarchySeparator);
      if (NS_FAILED(rv)) 
        return rv;
      if (mimePart)
      {
        return FetchMimePart(imapUrl, nsIImapUrl::nsImapMsgFetch, folder, imapMessageSink,
                        aURL, aDisplayConsumer, msgKey, mimePart);
      }

      nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(imapUrl));
      nsCOMPtr<nsIMsgI18NUrl> i18nurl (do_QueryInterface(imapUrl));
      i18nurl->SetCharsetOverRide(aCharsetOverride);

      PRUint32 messageSize;
      PRBool useMimePartsOnDemand = gMIMEOnDemand;
      nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;

      if (imapMessageSink)
        imapMessageSink->GetMessageSizeFromDB(msgKey, PR_TRUE, &messageSize);

      msgurl->SetMsgWindow(aMsgWindow);

      rv = msgurl->GetServer(getter_AddRefs(aMsgIncomingServer));
    
      if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
      {
          nsCOMPtr<nsIImapIncomingServer>
              aImapServer(do_QueryInterface(aMsgIncomingServer, &rv));
          if (NS_SUCCEEDED(rv) && aImapServer)
            aImapServer->GetMimePartsOnDemand(&useMimePartsOnDemand);
      }

      if (!useMimePartsOnDemand || (messageSize < (uint32) gMIMEOnDemandThreshold))
//                allowedToBreakApart && 
//              !GetShouldFetchAllParts() &&
//            GetServerStateParser().ServerHasIMAP4Rev1Capability() &&
      {
        imapUrl->SetFetchPartsOnDemand(PR_FALSE);
        // for now, lets try not adding these 
        msgurl->SetAddToMemoryCache(PR_TRUE);
      }
      else
      {
      // whenever we are displaying a message, we want to add it to the memory cache..
        imapUrl->SetFetchPartsOnDemand(PR_TRUE);
        msgurl->SetAddToMemoryCache(PR_FALSE);
      }
      PRBool msgLoadingFromCache = PR_FALSE;
      rv = FetchMessage(imapUrl, nsIImapUrl::nsImapMsgFetch, folder, imapMessageSink,
                        aMsgWindow, aURL, aDisplayConsumer, msgKey, PR_TRUE);
      if (NS_SUCCEEDED(rv))
      {
        imapUrl->GetMsgLoadingFromCache(&msgLoadingFromCache);
        // we're reading this msg from the cache - mark it read on the server.
        if (msgLoadingFromCache)
        {
          nsCOMPtr <nsIMsgDatabase> database;
          if (NS_SUCCEEDED(folder->GetMsgDatabase(nsnull, getter_AddRefs(database))) && database)
          {
            PRBool msgRead = PR_TRUE;
            database->IsRead(key, &msgRead);
            if (!msgRead)
            {
              nsCOMPtr<nsISupportsArray> messages;
              rv = NS_NewISupportsArray(getter_AddRefs(messages));
              if (NS_FAILED(rv)) 
                return rv;
              NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
              if (NS_FAILED(rv)) return rv;
              nsCOMPtr<nsIRDFResource> msgResource;
              nsCOMPtr<nsIMessage> message;
              rv = rdfService->GetResource(aMessageURI,
                                           getter_AddRefs(msgResource));
              if(NS_SUCCEEDED(rv))
              {
                rv = msgResource->QueryInterface(NS_GET_IID(nsIMessage),
                                                 getter_AddRefs(message));
                if (NS_SUCCEEDED(rv) && message)
                {
                  nsCOMPtr<nsISupports> msgSupport(do_QueryInterface(message, &rv));
                  if (msgSupport)
                  {
                    messages->AppendElement(msgSupport);
                    folder->MarkMessagesRead(messages, PR_TRUE);
                  }
                }
              }
            }
          }
        }
      }
    }
	}
	return rv;
}


nsresult nsImapService::FetchMimePart(nsIImapUrl * aImapUrl,
                            nsImapAction aImapAction,
                            nsIMsgFolder * aImapMailFolder, 
                            nsIImapMessageSink * aImapMessage,
                            nsIURI ** aURL,
							              nsISupports * aDisplayConsumer, 
                            const char *messageIdentifierList,
                            const char *mimePart) 
{
  nsresult rv = NS_OK;

	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
  NS_ASSERTION (aImapUrl && aImapMailFolder &&  aImapMessage,"Oops ... null pointer");
  if (!aImapUrl || !aImapMailFolder || !aImapMessage)
      return NS_ERROR_NULL_POINTER;

  nsCAutoString urlSpec;
  rv = SetImapUrlSink(aImapMailFolder, aImapUrl);
  nsImapAction actionToUse = aImapAction;
  if (actionToUse == nsImapUrl::nsImapOpenMimePart)
    actionToUse = nsIImapUrl::nsImapMsgFetch;

  rv = aImapUrl->SetImapMessageSink(aImapMessage);
  if (NS_SUCCEEDED(rv))
	{
      nsXPIDLCString currentSpec;
      nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl);
      url->GetSpec(getter_Copies(currentSpec));
      urlSpec.Assign(currentSpec);

      PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder); 

	  urlSpec.Append("fetch>");
	  urlSpec.Append(uidString);
	  urlSpec.Append(">");
	  urlSpec.AppendWithConversion(hierarchySeparator);

	  nsXPIDLCString folderName;
	  GetFolderName(aImapMailFolder, getter_Copies(folderName));
	  urlSpec.Append((const char *) folderName);
	  urlSpec.Append(">");
	  urlSpec.Append(messageIdentifierList);
    urlSpec.Append(mimePart);


    // rhp: If we are displaying this message for the purpose of printing, we
    // need to append the header=print option.
    //
    if (mPrintingOperation)
      urlSpec.Append("?header=print");

		  // mscott - this cast to a char * is okay...there's a bug in the XPIDL
		  // compiler that is preventing in string parameters from showing up as
		  // const char *. hopefully they will fix it soon.
		  rv = url->SetSpec((char *) urlSpec.GetBuffer());

	    rv = aImapUrl->SetImapAction(actionToUse /* nsIImapUrl::nsImapMsgFetch */);
	   if (aImapMailFolder && aDisplayConsumer)
	   {
			nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;
			rv = aImapMailFolder->GetServer(getter_AddRefs(aMsgIncomingServer));
			if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
			{
				PRBool interrupted;
				nsCOMPtr<nsIImapIncomingServer>
					aImapServer(do_QueryInterface(aMsgIncomingServer, &rv));
				if (NS_SUCCEEDED(rv) && aImapServer)
					aImapServer->PseudoInterruptMsgLoad(aImapUrl, &interrupted);
			}
	   }
      // if the display consumer is a docshell, then we should run the url in the docshell.
      // otherwise, it should be a stream listener....so open a channel using AsyncRead
      // and the provided stream listener....

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
      if (NS_SUCCEEDED(rv) && docShell)
      {
        nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
        // DIRTY LITTLE HACK --> if we are opening an attachment we want the docshell to
        // treat this load as if it were a user click event. Then the dispatching stuff will be much
        // happier.
        if (aImapAction == nsImapUrl::nsImapOpenMimePart)
        {
          docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
          loadInfo->SetLoadType(nsIDocShellLoadInfo::loadLink);
        }
        
        rv = docShell->LoadURI(url, loadInfo);
      }
      else
      {
        nsCOMPtr<nsIStreamListener> aStreamListener = do_QueryInterface(aDisplayConsumer, &rv);
        if (NS_SUCCEEDED(rv) && aStreamListener)
        {
          nsCOMPtr<nsIChannel> aChannel;
		      nsCOMPtr<nsILoadGroup> aLoadGroup;
		      nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl, &rv);
		      if (NS_SUCCEEDED(rv) && mailnewsUrl)
			      mailnewsUrl->GetLoadGroup(getter_AddRefs(aLoadGroup));

          rv = NewChannel(url, getter_AddRefs(aChannel));
          if (NS_FAILED(rv)) return rv;

          nsCOMPtr<nsISupports> aCtxt = do_QueryInterface(url);
          //  now try to open the channel passing in our display consumer as the listener 
          rv = aChannel->AsyncRead(aStreamListener, aCtxt);
        }
        else // do what we used to do before
        {
          // I'd like to get rid of this code as I believe that we always get a docshell
          // or stream listener passed into us in this method but i'm not sure yet...
          // I'm going to use an assert for now to figure out if this is ever getting called
#ifdef DEBUG_mscott
          NS_ASSERTION(0, "oops...someone still is reaching this part of the code");
#endif
          nsCOMPtr<nsIEventQueue> queue;	
          // get the Event Queue for this thread...
	        NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);

          if (NS_FAILED(rv)) return rv;

          rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
          if (NS_FAILED(rv)) return rv;
          rv = GetImapConnectionAndLoadUrl(queue, aImapUrl, aDisplayConsumer, aURL);
        }
      }
	}
  return rv;
}

//
// rhp: Right now, this is the same as simple DisplayMessage, but it will change
// to support print rendering.
//
NS_IMETHODIMP nsImapService::DisplayMessageForPrinting(const char* aMessageURI,
                                                        nsISupports * aDisplayConsumer,  
                                                        nsIMsgWindow * aMsgWindow,
                                                        nsIUrlListener * aUrlListener,
                                                        nsIURI ** aURL) 
{
  mPrintingOperation = PR_TRUE;
  nsresult rv = DisplayMessage(aMessageURI, aDisplayConsumer, aMsgWindow, aUrlListener, nsnull, aURL);
  mPrintingOperation = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsImapService::CopyMessage(const char * aSrcMailboxURI, nsIStreamListener *
                           aMailboxCopy, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **aURL)
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
			      PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
            rv = CreateStartOfImapUrl(aSrcMailboxURI, getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec, hierarchySeparator);

            // now try to download the message
            nsImapAction imapAction = nsIImapUrl::nsImapOnlineToOfflineCopy;
            if (moveMessage)
                imapAction = nsIImapUrl::nsImapOnlineToOfflineMove; 
			      rv = FetchMessage(imapUrl,imapAction, folder, imapMessageSink,aMsgWindow, aURL, streamSupport, msgKey, PR_TRUE);
        } // if we got an imap message sink
    } // if we decomposed the imap message 
    return rv;
}

NS_IMETHODIMP
nsImapService::CopyMessages(nsMsgKeyArray *keys, nsIMsgFolder *srcFolder, nsIStreamListener *aMailboxCopy, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **aURL)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsISupports> streamSupport;
    if (!keys || !aMailboxCopy) 
		return NS_ERROR_NULL_POINTER;
    streamSupport = do_QueryInterface(aMailboxCopy, &rv);
    if (!streamSupport || NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgFolder> folder = srcFolder;
    nsXPIDLCString msgKey;
	if (NS_SUCCEEDED(rv))
	{
    nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
		if (NS_SUCCEEDED(rv))
		{
			nsCString messageIds;

			AllocateImapUidString(keys->GetArray(), keys->GetSize(), messageIds);
      nsCOMPtr<nsIImapUrl> imapUrl;
      nsCAutoString urlSpec;
			PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
      rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec, hierarchySeparator);
			nsImapAction action;
			if (moveMessage)
				action = nsIImapUrl::nsImapOnlineToOfflineMove;
			else
				action = nsIImapUrl::nsImapOnlineToOfflineCopy;
			imapUrl->SetCopyState(aMailboxCopy);
            // now try to display the message
			rv = FetchMessage(imapUrl, action, folder, imapMessageSink,
                              aMsgWindow, aURL, streamSupport, messageIds.GetBuffer(), PR_TRUE);
			// ### end of copy operation should know how to do the delete.if this is a move

     } // if we got an imap message sink
  } // if we decomposed the imap message 
  return rv;
}

NS_IMETHODIMP nsImapService::Search(nsIMsgSearchSession *aSearchSession, nsIMsgWindow *aMsgWindow, nsIMsgFolder *aMsgFolder, const char *aSearchUri)
{
  nsresult rv = NS_OK;
  nsCAutoString	folderURI;

  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCOMPtr <nsIUrlListener> urlListener = do_QueryInterface(aSearchSession);

  nsCAutoString urlSpec;
  PRUnichar hierarchySeparator = GetHierarchyDelimiter(aMsgFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aMsgFolder, urlListener, urlSpec, hierarchySeparator);
  if (NS_FAILED(rv)) 
        return rv;
  nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(imapUrl));

  msgurl->SetMsgWindow(aMsgWindow);
  msgurl->SetSearchSession(aSearchSession);
  imapUrl->AddChannelToLoadGroup();
  rv = SetImapUrlSink(aMsgFolder, imapUrl);

  if (NS_SUCCEEDED(rv))
  {
	nsXPIDLCString folderName;
    GetFolderName(aMsgFolder, getter_Copies(folderName));

	nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
	urlSpec.Append("/search>UID>");
	urlSpec.AppendWithConversion(hierarchySeparator);
    urlSpec.Append((const char *) folderName);
    urlSpec.Append('>');
    urlSpec.Append(aSearchUri);
    rv = mailNewsUrl->SetSpec((char *) urlSpec.GetBuffer());
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIEventQueue> queue;	
      // get the Event Queue for this thread...
	    NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);

      if (NS_FAILED(rv)) return rv;

      rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
      if (NS_FAILED(rv)) return rv;
       rv = GetImapConnectionAndLoadUrl(queue, imapUrl, nsnull, nsnull);
    }
  }
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
	rv = nsParseImapMessageURI(aMessageURI, folderURI, &msgKey, nsnull);
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFResource> res;
		rv = rdf->GetResource(folderURI, getter_AddRefs(res));
		if (NS_FAILED(rv))	return rv;
        rv = res->QueryInterface(NS_GET_IID(nsIMsgFolder), (void **) aFolder);
        // convert the message key into a string
        if (msgKey)
        {
            nsCAutoString messageIdString;
            messageIdString.AppendInt(msgKey, 10 /* base 10 */);
            *aMsgKey = messageIdString.ToNewCString();
        }
    }

    return rv;
}

NS_IMETHODIMP nsImapService::SaveMessageToDisk(const char *aMessageURI, 
                                               nsIFileSpec *aFile, 
                                               PRBool aAddDummyEnvelope, 
                                               nsIUrlListener *aUrlListener, 
                                               nsIURI **aURL,
                                               PRBool canonicalLineEnding,
											   nsIMsgWindow *aMsgWindow)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgFolder> folder;
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsXPIDLCString msgKey;

    rv = DecomposeImapURI(aMessageURI, getter_AddRefs(folder), getter_Copies(msgKey));
    if (NS_FAILED(rv)) return rv;
    
    nsCAutoString urlSpec;
  	PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
    rv = CreateStartOfImapUrl(aMessageURI, getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(imapUrl, &rv);
        if (NS_FAILED(rv)) return rv;
        msgUrl->SetMessageFile(aFile);
        msgUrl->SetAddDummyEnvelope(aAddDummyEnvelope);
        msgUrl->SetCanonicalLineEnding(canonicalLineEnding);

        return FetchMessage(imapUrl, nsIImapUrl::nsImapSaveMessageToDisk, folder, imapMessageSink, aMsgWindow, aURL, imapMessageSink, msgKey, PR_TRUE);
    }

	return rv;
}

/* fetching RFC822 messages */
/* imap4://HOST>fetch><UID/SEQUENCE>>MAILBOXPATH>x */
/*   'x' is the message UID or sequence number list */
/* will set the 'SEEN' flag */

NS_IMETHODIMP
nsImapService::FetchMessage(nsIImapUrl * aImapUrl,
                            nsImapAction aImapAction,
                            nsIMsgFolder * aImapMailFolder, 
                            nsIImapMessageSink * aImapMessage,
                            nsIMsgWindow *aMsgWindow,
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
    nsresult rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

    rv = aImapUrl->SetImapMessageSink(aImapMessage);
    if (NS_SUCCEEDED(rv))
	{
      nsXPIDLCString currentSpec;
      nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl);
      url->GetSpec(getter_Copies(currentSpec));
      urlSpec.Assign(currentSpec);

      PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder); 

	  urlSpec.Append("fetch>");
	  urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
	  urlSpec.Append(">");
	  urlSpec.AppendWithConversion(hierarchySeparator);

	  nsXPIDLCString folderName;
	  GetFolderName(aImapMailFolder, getter_Copies(folderName));
	  urlSpec.Append((const char *) folderName);
	  urlSpec.Append(">");
	  urlSpec.Append(messageIdentifierList);


    // rhp: If we are displaying this message for the purpose of printing, we
    // need to append the header=print option.
    //
    if (mPrintingOperation)
      urlSpec.Append("?header=print");

		  // mscott - this cast to a char * is okay...there's a bug in the XPIDL
		  // compiler that is preventing in string parameters from showing up as
		  // const char *. hopefully they will fix it soon.
		  rv = url->SetSpec((char *) urlSpec.GetBuffer());

	    rv = aImapUrl->SetImapAction(aImapAction);
	   if (aImapMailFolder && aDisplayConsumer)
	   {
			nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;
			rv = aImapMailFolder->GetServer(getter_AddRefs(aMsgIncomingServer));
			if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
			{
				PRBool interrupted;
				nsCOMPtr<nsIImapIncomingServer>
					aImapServer(do_QueryInterface(aMsgIncomingServer, &rv));
				if (NS_SUCCEEDED(rv) && aImapServer)
					aImapServer->PseudoInterruptMsgLoad(aImapUrl, &interrupted);
			}
	   }
      // if the display consumer is a docshell, then we should run the url in the docshell.
      // otherwise, it should be a stream listener....so open a channel using AsyncRead
      // and the provided stream listener....

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
      if (NS_SUCCEEDED(rv) && docShell)
      {      
        rv = docShell->LoadURI(url, nsnull);
      }
      else
      {
        nsCOMPtr<nsIStreamListener> aStreamListener = do_QueryInterface(aDisplayConsumer, &rv);
        if (NS_SUCCEEDED(rv) && aStreamListener)
        {
          nsCOMPtr<nsIChannel> aChannel;
          nsCOMPtr<nsILoadGroup> aLoadGroup;
          nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl, &rv);
          if (NS_SUCCEEDED(rv) && mailnewsUrl)
          {
            if (aMsgWindow)
              mailnewsUrl->SetMsgWindow(aMsgWindow);
            mailnewsUrl->GetLoadGroup(getter_AddRefs(aLoadGroup));
          }
          rv = NewChannel(url, getter_AddRefs(aChannel));
          if (NS_FAILED(rv)) return rv;

          rv = aChannel->SetLoadGroup(aLoadGroup);
          if (NS_FAILED(rv)) return rv;

          nsCOMPtr<nsISupports> aCtxt = do_QueryInterface(url);
          //  now try to open the channel passing in our display consumer as the listener 
          rv = aChannel->AsyncRead(aStreamListener, aCtxt);
        }
        else // do what we used to do before
        {
          // I'd like to get rid of this code as I believe that we always get a docshell
          // or stream listener passed into us in this method but i'm not sure yet...
          // I'm going to use an assert for now to figure out if this is ever getting called
#ifdef DEBUG_mscott
          NS_ASSERTION(0, "oops...someone still is reaching this part of the code");
#endif
          nsCOMPtr<nsIEventQueue> queue;	
          // get the Event Queue for this thread...
	        NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);

          if (NS_FAILED(rv)) return rv;

          rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
          if (NS_FAILED(rv)) return rv;
          rv = GetImapConnectionAndLoadUrl(queue, aImapUrl, aDisplayConsumer, aURL);
        }
      }
	}
	return rv;
}

nsresult 
nsImapService::CreateStartOfImapUrl(const char * aImapURI, nsIImapUrl ** imapUrl,
                                    nsIMsgFolder* aImapMailFolder,
                                    nsIUrlListener * aUrlListener,
                                    nsCString & urlSpec, 
									                  PRUnichar &hierarchyDelimiter)
{
	  nsresult rv = NS_OK;
    char *hostname = nsnull;
    nsXPIDLCString username;
    nsXPIDLCString escapedUsername;
    
    rv = aImapMailFolder->GetHostname(&hostname);
    if (NS_FAILED(rv)) return rv;
    rv = aImapMailFolder->GetUsername(getter_Copies(username));
    if (NS_FAILED(rv))
    {
        PR_FREEIF(hostname);
        return rv;
    }
    
    if (((const char*)username) && username[0])
        *((char **)getter_Copies(escapedUsername)) = nsEscape(username, url_XAlphas);

    PRInt32 port = IMAP_PORT;
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = aImapMailFolder->GetServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv)) {
        server->GetPort(&port);
        if (port == -1 || port == 0) port = IMAP_PORT;
    }
    
	// now we need to create an imap url to load into the connection. The url
  // needs to represent a select folder action. 
	rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
                                          NS_GET_IID(nsIImapUrl), (void **)
                                          imapUrl);
	if (NS_SUCCEEDED(rv) && imapUrl)
  {
      nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(*imapUrl, &rv);
      if (NS_SUCCEEDED(rv) && mailnewsUrl && aUrlListener)
          mailnewsUrl->RegisterListener(aUrlListener);
      nsCOMPtr<nsIMsgMessageUrl> msgurl(do_QueryInterface(*imapUrl));
      msgurl->SetUri(aImapURI);

      urlSpec = "imap://";
      urlSpec.Append((const char *) escapedUsername);
      urlSpec.Append('@');
      urlSpec.Append(hostname);
		  urlSpec.Append(':');
          
		  urlSpec.AppendInt(port);

      // *** jefft - force to parse the urlSpec in order to search for
      // the correct incoming server
 		  // mscott - this cast to a char * is okay...there's a bug in the XPIDL
		  // compiler that is preventing in string parameters from showing up as
		  // const char *. hopefully they will fix it soon.
		  rv = mailnewsUrl->SetSpec((char *) urlSpec.GetBuffer());

		  hierarchyDelimiter = kOnlineHierarchySeparatorUnknown;
		  nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aImapMailFolder);
		  if (imapFolder)
			  imapFolder->GetHierarchyDelimiter(&hierarchyDelimiter);
  }

  PR_FREEIF(hostname);
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
	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);

	nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),aImapMailFolder, aUrlListener, urlSpec, hierarchySeparator);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{
        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

        rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{

			urlSpec.Append("/header>");
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.AppendWithConversion(hierarchySeparator);

            nsXPIDLCString folderName;

            GetFolderName(aImapMailFolder, getter_Copies(folderName));
			urlSpec.Append((const char *) folderName);
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
	nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectNoopFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);
        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			urlSpec.Append("/selectnoop>");
			urlSpec.AppendWithConversion(hierarchySeparator);

            nsXPIDLCString folderName;

            GetFolderName(aImapMailFolder, getter_Copies(folderName));
			urlSpec.Append((const char *) folderName);
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
	nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
	if (NS_SUCCEEDED(rv))
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapExpungeFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

		if (NS_SUCCEEDED(rv))
		{

			urlSpec.Append("/Expunge>");
			urlSpec.AppendWithConversion(hierarchySeparator);

            nsXPIDLCString folderName;

            GetFolderName(aImapMailFolder, getter_Copies(folderName));
			urlSpec.Append((const char *) folderName);
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
	nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapExpungeFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);
        
        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

		if (NS_SUCCEEDED(rv))
		{
			urlSpec.Append("/Biff>");
			urlSpec.AppendWithConversion(hierarchySeparator);

            nsXPIDLCString folderName;
            GetFolderName(aImapMailFolder, getter_Copies(folderName));
            urlSpec.Append((const char *) folderName);
			urlSpec.Append(">");
			urlSpec.AppendInt(uidHighWater, 10);
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), folder, urlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv))
    {
        rv = SetImapUrlSink(folder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            urlSpec.Append("/delete>");
            urlSpec.AppendWithConversion(hierarchySeparator);
            
            nsXPIDLCString folderName;
            rv = GetFolderName(folder, getter_Copies(folderName));
            if (NS_SUCCEEDED(rv))
            {
                urlSpec.Append((const char *) folderName);
                rv = uri->SetSpec((char*) urlSpec.GetBuffer());
                if (NS_SUCCEEDED(rv))
                {
                    rv = ResetImapConnection(imapUrl, 
                                             (const char*) folderName);
                    rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                         nsnull,
                                                         url);
                }
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
	nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append("/deletemsg>");
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.AppendWithConversion(hierarchySeparator);

            nsXPIDLCString folderName;

            GetFolderName(aImapMailFolder, getter_Copies(folderName));
            urlSpec.Append((const char *) folderName);
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
	nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
	if (NS_SUCCEEDED(rv))
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectNoopFolder);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append("/deleteallmsgs>");
			urlSpec.AppendWithConversion(hierarchySeparator);
            nsXPIDLCString folderName;
            GetFolderName(aImapMailFolder, getter_Copies(folderName));
			urlSpec.Append((const char *) folderName);
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
	nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator); 
	if (NS_SUCCEEDED(rv) && imapUrl)
	{

		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgFetch);
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);

		if (NS_SUCCEEDED(rv))
		{
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

			urlSpec.Append('/');
			urlSpec.Append(howToDiddle);
			urlSpec.Append('>');
			urlSpec.Append(messageIdsAreUID ? uidString : sequenceString);
			urlSpec.Append(">");
			urlSpec.AppendWithConversion(hierarchySeparator);
            nsXPIDLCString folderName;
            GetFolderName(aImapMailFolder, getter_Copies(folderName));
            urlSpec.Append((const char *) folderName);
			urlSpec.Append(">");
			urlSpec.Append(messageIdentifierList);
			urlSpec.Append('>');
			urlSpec.AppendInt(flags, 10);
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
   
  rv = aMsgFolder->QueryInterface(NS_GET_IID(nsIImapMailFolderSink), 
                                 (void**)&aInst);
  if (NS_SUCCEEDED(rv) && aInst)
      aImapUrl->SetImapMailFolderSink((nsIImapMailFolderSink*) aInst);
  NS_IF_RELEASE (aInst);
  aInst = nsnull;
  
  rv = aMsgFolder->QueryInterface(NS_GET_IID(nsIImapMessageSink), 
                                 (void**)&aInst);
  if (NS_SUCCEEDED(rv) && aInst)
      aImapUrl->SetImapMessageSink((nsIImapMessageSink*) aInst);
  NS_IF_RELEASE (aInst);
  aInst = nsnull;
  
  rv = aMsgFolder->QueryInterface(NS_GET_IID(nsIImapExtensionSink), 
                                 (void**)&aInst);
  if (NS_SUCCEEDED(rv) && aInst)
      aImapUrl->SetImapExtensionSink((nsIImapExtensionSink*) aInst);
  NS_IF_RELEASE (aInst);
  aInst = nsnull;
  
  rv = aMsgFolder->QueryInterface(NS_GET_IID(nsIImapMiscellaneousSink), 
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
    nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(aImapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
    nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(aImapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
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
								const char *folderPath,
                                nsIURI** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> aImapUrl;
    nsCAutoString urlSpec;

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
    nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(aImapUrl),
                                          aImapMailFolder,
                                          aUrlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED (rv))
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            if (folderPath && (nsCRT::strlen(folderPath) > 0))
            {
                nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);

                urlSpec.Append("/discoverchildren>");
				urlSpec.AppendWithConversion(hierarchySeparator);
                urlSpec.Append(folderPath);
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
									 const char *folderPath,
                                     PRInt32 level,
                                     nsIURI** aURL)
{
    NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                  "Oops ... null aClientEventQueue or aImapMailFolder");
    if (!aImapMailFolder || ! aClientEventQueue)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> aImapUrl;
    nsCAutoString urlSpec;

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
    nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(aImapUrl),
                                          aImapMailFolder,aUrlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED (rv) && aImapUrl)
    {
        rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

        if (NS_SUCCEEDED(rv))
        {
            if (folderPath && (nsCRT::strlen(folderPath) > 0))
            {
                nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);
                urlSpec.Append("/discoverlevelchildren>");
                urlSpec.AppendInt(level);
                urlSpec.AppendWithConversion(hierarchySeparator); // hierarchySeparator "/"
                urlSpec.Append(folderPath);

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
                                 nsISupports* copyState,
                                 nsIMsgWindow *aMsgWindow)
{
    NS_ASSERTION(aSrcFolder && aDstFolder && messageIds && aClientEventQueue,
                 "Fatal ... missing key parameters");
    if (!aClientEventQueue || !aSrcFolder || !aDstFolder || !messageIds ||
        *messageIds == 0)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr <nsIMsgIncomingServer> srcServer;
    nsCOMPtr <nsIMsgIncomingServer> dstServer;

    rv = aSrcFolder->GetServer(getter_AddRefs(srcServer));
    if(NS_FAILED(rv)) return rv;

    rv = aDstFolder->GetServer(getter_AddRefs(dstServer));
    if(NS_FAILED(rv)) return rv;

    PRBool sameServer;
    rv = dstServer->Equals(srcServer, &sameServer);
    if(NS_FAILED(rv)) return rv;

    if (!sameServer) {
        // *** can only take message from the same imap host and user accnt
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aSrcFolder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aSrcFolder, aUrlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv))
    {
        SetImapUrlSink(aSrcFolder, imapUrl);
        imapUrl->SetCopyState(copyState);

        nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(imapUrl));

        msgurl->SetMsgWindow(aMsgWindow);
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
        urlSpec.AppendWithConversion(hierarchySeparator);

        nsXPIDLCString folderName;
        GetFolderName(aSrcFolder, getter_Copies(folderName));
        urlSpec.Append((const char *) folderName);
        urlSpec.Append('>');
        urlSpec.Append(messageIds);
        urlSpec.Append('>');
        urlSpec.AppendWithConversion(hierarchySeparator);
        folderName = "";
        GetFolderName(aDstFolder, getter_Copies(folderName));
        urlSpec.Append((const char *) folderName);

    		rv = uri->SetSpec((char *) urlSpec.GetBuffer());
        if (NS_SUCCEEDED(rv))
            rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                             nsnull, aURL);
    }
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

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aDstFolder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aDstFolder, aListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv))
    {
        SetImapUrlSink(aDstFolder, imapUrl);
        imapUrl->SetMsgFileSpec(aFileSpec);
        imapUrl->SetCopyState(aCopyState);

        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

        if (inSelectedState)
            urlSpec.Append("/appenddraftfromfile>");
        else
            urlSpec.Append("/appendmsgfromfile>");

        urlSpec.AppendWithConversion(hierarchySeparator);
        
        nsXPIDLCString folderName;
        GetFolderName(aDstFolder, getter_Copies(folderName));
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
nsImapService::ResetImapConnection(nsIImapUrl* aImapUrl, 
                                   const char *folderName)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl = do_QueryInterface(aImapUrl, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = msgUrl->GetServer(getter_AddRefs(aMsgIncomingServer));
    if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
    {
        nsCOMPtr<nsIImapIncomingServer> aImapServer =
            do_QueryInterface(aMsgIncomingServer, &rv);
        if (NS_SUCCEEDED(rv) && aImapServer)
            rv = aImapServer->ResetConnection(folderName);
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

	PRUnichar default_hierarchySeparator = GetHierarchyDelimiter(dstFolder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), dstFolder, urlListener, urlSpec, default_hierarchySeparator);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(dstFolder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            char hierarchySeparator = kOnlineHierarchySeparatorUnknown;
            nsXPIDLCString folderName;
            
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);
            GetFolderName(srcFolder, getter_Copies(folderName));
            urlSpec.Append("/movefolderhierarchy>");
            urlSpec.Append(hierarchySeparator);
            urlSpec.Append((const char *) folderName);
            urlSpec.Append('>');
            folderName = "";
            GetFolderName(dstFolder, getter_Copies(folderName));
            urlSpec.Append(hierarchySeparator);
            urlSpec.Append((const char *) folderName);
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
            {
                GetFolderName(srcFolder, getter_Copies(folderName));
                rv = ResetImapConnection(imapUrl, (const char*)folderName);
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull,
                                                 url);
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::RenameLeaf(nsIEventQueue* eventQueue, nsIMsgFolder* srcFolder,
                          const PRUnichar* newLeafName, nsIUrlListener* urlListener,
                          nsIURI** url)
{
    NS_ASSERTION(eventQueue && srcFolder && newLeafName && *newLeafName,
                 "Oops ... [RenameLeaf] null pointers");
    if (!eventQueue || !srcFolder || !newLeafName || !*newLeafName)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(srcFolder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), srcFolder, urlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv))
    {
        rv = SetImapUrlSink(srcFolder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            nsXPIDLCString folderName;
            GetFolderName(srcFolder, getter_Copies(folderName));
            urlSpec.Append("/rename>");
            urlSpec.AppendWithConversion(hierarchySeparator);
            urlSpec.Append((const char *) folderName);
            urlSpec.Append('>');
            urlSpec.AppendWithConversion(hierarchySeparator);

			char *utfNewName = CreateUtf7ConvertedStringFromUnicode( newLeafName);

			nsCAutoString cStrFolderName(NS_STATIC_CAST(const char *, folderName));
            PRInt32 leafNameStart = 
                cStrFolderName.RFindChar('/'); // ** troublesome hierarchyseparator
            if (leafNameStart != -1)
            {
                cStrFolderName.SetLength(leafNameStart+1);
                urlSpec.Append(cStrFolderName.GetBuffer());
            }

            urlSpec.Append(utfNewName);

			nsCRT::free(utfNewName);

            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
            {
                rv = ResetImapConnection(imapUrl, (const char *) folderName);
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull, url);
            }
        } // if (NS_SUCCEEDED(rv))
    } // if (NS_SUCCEEDED(rv) && imapUrl)
    return rv;
}

NS_IMETHODIMP
nsImapService::CreateFolder(nsIEventQueue* eventQueue, nsIMsgFolder* parent,
                            const PRUnichar* newFolderName, 
                            nsIUrlListener* urlListener, nsIURI** url)
{
    NS_ASSERTION(eventQueue && parent && newFolderName && *newFolderName,
                 "Oops ... [CreateFolder] null pointers");
    if (!eventQueue || !parent || !newFolderName || !*newFolderName)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(parent);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), parent, urlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(parent, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            nsXPIDLCString folderName;
            GetFolderName(parent, getter_Copies(folderName));
            urlSpec.Append("/create>");
            urlSpec.AppendWithConversion(hierarchySeparator);
            if ((const char *) folderName && nsCRT::strlen(folderName) > 0)
            {
                urlSpec.Append((const char *) folderName);
                urlSpec.AppendWithConversion(hierarchySeparator);
            }
			char *utfNewName = CreateUtf7ConvertedStringFromUnicode( newFolderName);
			char *escapedFolderName = nsEscape(utfNewName, url_Path);
            urlSpec.Append(escapedFolderName);
			nsCRT::free(escapedFolderName);
			nsCRT::free(utfNewName);
    
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                     nsnull,
                                                     url);
        } // if (NS_SUCCEEDED(rv))
    } // if (NS_SUCCEEDED(rv) && imapUrl)
    return rv;
}

NS_IMETHODIMP
nsImapService::EnsureFolderExists(nsIEventQueue* eventQueue, nsIMsgFolder* parent,
                            const PRUnichar* newFolderName, 
                            nsIUrlListener* urlListener, nsIURI** url)
{
    NS_ASSERTION(eventQueue && parent && newFolderName && *newFolderName,
                 "Oops ... [EnsureExists] null pointers");
    if (!eventQueue || !parent || !newFolderName || !*newFolderName)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(parent);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), parent, urlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(parent, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            nsXPIDLCString folderName;
            GetFolderName(parent, getter_Copies(folderName));
            urlSpec.Append("/ensureExists>");
            urlSpec.AppendWithConversion(hierarchySeparator);
            if ((const char *) folderName && nsCRT::strlen(folderName) > 0)
            {
                urlSpec.Append((const char *) folderName);
                urlSpec.AppendWithConversion(hierarchySeparator);
            }
			char *utfNewName = CreateUtf7ConvertedStringFromUnicode( newFolderName);
			char *escapedFolderName = nsEscape(utfNewName, url_Path);
            urlSpec.Append(escapedFolderName);
			nsCRT::free(escapedFolderName);
			nsCRT::free(utfNewName);
    
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                     nsnull,
                                                     url);
        } // if (NS_SUCCEEDED(rv))
    } // if (NS_SUCCEEDED(rv) && imapUrl)
    return rv;
}


NS_IMETHODIMP
nsImapService::ListFolder(nsIEventQueue* aClientEventQueue,
                                nsIMsgFolder* aImapMailFolder,
                                nsIUrlListener* aUrlListener,
                                nsIURI** aURL)
{
    NS_ASSERTION(aClientEventQueue && aImapMailFolder ,
                 "Oops ... [RenameLeaf] null pointers");
    if (!aClientEventQueue || !aImapMailFolder)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;

	PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aImapMailFolder, aUrlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(aImapMailFolder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            nsXPIDLCString folderName;
            GetFolderName(aImapMailFolder, getter_Copies(folderName));
            urlSpec.Append("/listfolder>");
            urlSpec.AppendWithConversion(hierarchySeparator);
            if ((const char *) folderName && nsCRT::strlen(folderName) > 0)
			{
                urlSpec.Append((const char *) folderName);
				rv = uri->SetSpec((char*) urlSpec.GetBuffer());
				if (NS_SUCCEEDED(rv))
					rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
														 nsnull,
														 aURL);
			}
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
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = IMAP_PORT;
	return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetDefaultServerPort(PRInt32 *aDefaultPort)
{
    return GetDefaultPort(aDefaultPort);
}

NS_IMETHODIMP nsImapService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
  nsCOMPtr<nsIImapUrl> aImapUrl;
	nsresult rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull, NS_GET_IID(nsIImapUrl), getter_AddRefs(aImapUrl));
  if (NS_SUCCEEDED(rv))
  {
    // now extract lots of fun information...
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl);
    nsCAutoString unescapedSpec(aSpec);
    nsUnescape(unescapedSpec);
    mailnewsUrl->SetSpec((char *) unescapedSpec); // set the url spec...
    
    nsXPIDLCString userName;
    nsXPIDLCString hostName;
    nsXPIDLCString folderName;
    
    // extract the user name and host name information...
    rv = mailnewsUrl->GetHost(getter_Copies(hostName));
    if (NS_FAILED(rv)) return rv;
    rv = mailnewsUrl->GetPreHost(getter_Copies(userName));
    if (NS_FAILED(rv)) return rv;
    // if we can't get a folder name out of the url then I think this is an error
    aImapUrl->CreateCanonicalSourceFolderPathString(getter_Copies(folderName));
    if (NS_FAILED(rv)) return rv;
    
    NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                    NS_MSGACCOUNTMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = accountManager->FindServer(userName, hostName, "imap",
                                    getter_AddRefs(server));
    // if we can't extract the imap server from this url then give up!!!
    if (NS_FAILED(rv)) return rv;
    NS_ENSURE_TRUE(server, NS_ERROR_FAILURE);

    // now try to get the folder in question...
    nsCOMPtr<nsIFolder> aRootFolder;
    server->GetRootFolder(getter_AddRefs(aRootFolder));

    if (aRootFolder && folderName && (* ((const char *) folderName)) )
    {
      nsCOMPtr<nsIFolder> aFolder;
      rv = aRootFolder->FindSubFolder(folderName, getter_AddRefs(aFolder));
      if (NS_SUCCEEDED(rv))
      {
          nsCOMPtr<nsIImapMessageSink> msgSink = do_QueryInterface(aFolder);
          rv = aImapUrl->SetImapMessageSink(msgSink);

          nsCOMPtr<nsIMsgFolder> msgFolder = do_QueryInterface(aFolder);
          rv = SetImapUrlSink(msgFolder, aImapUrl);	
      }
    }

    // we got an imap url, so be sure to return it...
    aImapUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
  }

  return rv;
}

NS_IMETHODIMP nsImapService::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
    // imap can't open and return a channel right away...the url needs to go in the imap url queue 
    // until we find a connection which can run the url..in order to satisfy necko, we're going to return
    // a mock imap channel....

    nsresult rv = NS_OK;
    nsCOMPtr<nsIImapMockChannel> mockChannel;
    nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aURI, &rv);
    if (NS_FAILED(rv)) return rv;

    // XXX this mock channel stuff is wrong -- the channel really should be owning the URL
    // and the originalURL, not the other way around
    rv = imapUrl->GetMockChannel(getter_AddRefs(mockChannel));
    if (NS_FAILED(rv) || !mockChannel) return NS_ERROR_FAILURE;

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

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_ImapMailDirectory50, aResult);
    if (NS_FAILED(rv)) return rv;

    rv = SetDefaultLocalPath(*aResult);
    return rv;
}
    
NS_IMETHODIMP
nsImapService::GetServerIID(nsIID* *aServerIID)
{
    *aServerIID = new nsIID(NS_GET_IID(nsIImapIncomingServer));
    return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetRequiresUsername(PRBool *aRequiresUsername)
{
	NS_ENSURE_ARG_POINTER(aRequiresUsername);
	*aRequiresUsername = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetPreflightPrettyNameWithEmailAddress(PRBool *aPreflightPrettyNameWithEmailAddress)
{
	NS_ENSURE_ARG_POINTER(aPreflightPrettyNameWithEmailAddress);
	*aPreflightPrettyNameWithEmailAddress = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetCanDelete(PRBool *aCanDelete)
{
        NS_ENSURE_ARG_POINTER(aCanDelete);
        *aCanDelete = PR_TRUE;
        return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetCanDuplicate(PRBool *aCanDuplicate)
{
        NS_ENSURE_ARG_POINTER(aCanDuplicate);
        *aCanDuplicate = PR_TRUE;
        return NS_OK;
}        

NS_IMETHODIMP
nsImapService::GetDefaultCopiesAndFoldersPrefsToServer(PRBool *aDefaultCopiesAndFoldersPrefsToServer)
{
    NS_ENSURE_ARG_POINTER(aDefaultCopiesAndFoldersPrefsToServer);
    // when an imap server is created, the copies and folder prefs for the associated identity
    // don't point to folders on the server.  they'll point to the one on "Local Folders"
   	*aDefaultCopiesAndFoldersPrefsToServer = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsImapService::BuildSubscribeDatasourceWithPath(nsIImapIncomingServer *aServer, nsIMsgWindow *aMsgWindow, const char *folderPath)
{
	nsresult rv;

#ifdef DEBUG_sspitzer
	printf("BuildSubscribeDatasourceWithPath(%s)\n",folderPath);
#endif
	nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aServer);
	if (!server) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIFolder> rootFolder;
    rv = server->GetRootFolder(getter_AddRefs(rootFolder));
	if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!rootMsgFolder) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIUrlListener> listener = do_QueryInterface(aServer, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!listener) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIEventQueue> queue;
    // get the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;

	rv = DiscoverChildren(queue, rootMsgFolder, listener, folderPath, nsnull);
    if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsImapService::BuildSubscribeDatasource(nsIImapIncomingServer *aServer, nsIMsgWindow *aMsgWindow)
{
	nsresult rv;

        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aServer);
	if (!server) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIFolder> rootFolder;
        rv = server->GetRootFolder(getter_AddRefs(rootFolder));
	if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!rootMsgFolder) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIUrlListener> listener = do_QueryInterface(aServer, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!listener) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIEventQueue> queue;
        // get the Event Queue for this thread...
        NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
        if (NS_FAILED(rv)) return rv;

	rv = DiscoverAllAndSubscribedFolders(queue, rootMsgFolder, listener, nsnull);
        if (NS_FAILED(rv)) return rv;

	return NS_OK;
} 

NS_IMETHODIMP
nsImapService::SubscribeFolder(nsIEventQueue* eventQueue, 
                               nsIMsgFolder* aFolder,
                               const PRUnichar* folderName, 
                               nsIUrlListener* urlListener, nsIURI** url)
{
    NS_ENSURE_ARG_POINTER(eventQueue);
    NS_ENSURE_ARG_POINTER(aFolder);
    NS_ENSURE_ARG_POINTER(folderName);
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;
    PRUnichar hierarchySeparator = GetHierarchyDelimiter(aFolder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aFolder, urlListener,
                              urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(aFolder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);
            urlSpec.Append("/subscribe>");
            urlSpec.AppendWithConversion(hierarchySeparator);
            char *utfFolderName =
                CreateUtf7ConvertedStringFromUnicode(folderName);
            char *escapedFolderName = nsEscape(utfFolderName, url_Path);
            urlSpec.Append(escapedFolderName);
            nsCRT::free(escapedFolderName);
            nsCRT::free(utfFolderName);
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull, url);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::UnsubscribeFolder(nsIEventQueue* eventQueue, 
                               nsIMsgFolder* aFolder,
                               const PRUnichar* folderName, 
                               nsIUrlListener* urlListener, nsIURI** url)
{
    NS_ENSURE_ARG_POINTER(eventQueue);
    NS_ENSURE_ARG_POINTER(aFolder);
    NS_ENSURE_ARG_POINTER(folderName);
    
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    nsresult rv;
    PRUnichar hierarchySeparator = GetHierarchyDelimiter(aFolder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aFolder, urlListener,
                              urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv) && imapUrl)
    {
        rv = SetImapUrlSink(aFolder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);
            urlSpec.Append("/unsubscribe>");
            urlSpec.AppendWithConversion(hierarchySeparator);
            char *utfFolderName =
                CreateUtf7ConvertedStringFromUnicode(folderName);
            char *escapedFolderName = nsEscape(utfFolderName, url_Path);
            urlSpec.Append(escapedFolderName);
            nsCRT::free(escapedFolderName);
            nsCRT::free(utfFolderName);
            rv = uri->SetSpec((char*) urlSpec.GetBuffer());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull, url);
        }
    }
    return rv;
}
