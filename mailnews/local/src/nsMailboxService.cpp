/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"    // precompiled header...
#include "nsCOMPtr.h"

#include "nsMailboxService.h"
#include "nsIMsgMailSession.h"
#include "nsMailboxUrl.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMailboxProtocol.h"
#include "nsIMsgDatabase.h"
#include "nsMsgDBCID.h"
#include "nsMsgKeyArray.h"
#include "nsLocalUtils.h"
#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsIDocShell.h"
#include "nsIPop3Service.h"
#include "nsMsgUtils.h"
#include "nsIStreamConverterService.h"
#include "nsNetUtil.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIWebNavigation.h"
#include "prprf.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kIStreamConverterServiceCID,
                     NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);

nsMailboxService::nsMailboxService()
{
    mPrintingOperation = PR_FALSE;
    NS_INIT_REFCNT();
}

nsMailboxService::~nsMailboxService()
{}

NS_IMPL_ISUPPORTS4(nsMailboxService, nsIMailboxService, nsIMsgMessageService, nsIProtocolHandler, nsIMsgMessageFetchPartService);

nsresult nsMailboxService::ParseMailbox(nsIMsgWindow *aMsgWindow, nsFileSpec& aMailboxPath, nsIStreamListener *aMailboxParser, 
										nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
	nsCOMPtr<nsIMailboxUrl> mailboxurl;
	nsresult rv = NS_OK;

    rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            NS_GET_IID(nsIMailboxUrl),
                                            (void **) getter_AddRefs(mailboxurl));
	if (NS_SUCCEEDED(rv) && mailboxurl)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(mailboxurl);
		// okay now generate the url string
		nsFilePath filePath(aMailboxPath); // convert to file url representation...
		url->SetUpdatingFolder(PR_TRUE);
		url->SetMsgWindow(aMsgWindow);
		char * urlSpec = PR_smprintf("mailbox://%s", (const char *) filePath);
		url->SetSpec(urlSpec);
		PR_FREEIF(urlSpec);
		mailboxurl->SetMailboxParser(aMailboxParser);
		if (aUrlListener)
			url->RegisterListener(aUrlListener);

		RunMailboxUrl(url, nsnull);

		if (aURL)
		{
			*aURL = url;
			NS_IF_ADDREF(*aURL);
		}
	}

	return rv;
}
						 
nsresult nsMailboxService::CopyMessage(const char * aSrcMailboxURI,
                              nsIStreamListener * aMailboxCopyHandler,
                              PRBool moveMessage,
                              nsIUrlListener * aUrlListener,
                              nsIMsgWindow *aMsgWindow,
                              nsIURI **aURL)
{
    nsMailboxAction mailboxAction = nsIMailboxUrl::ActionMoveMessage;
    if (!moveMessage)
        mailboxAction = nsIMailboxUrl::ActionCopyMessage;
  return FetchMessage(aSrcMailboxURI, aMailboxCopyHandler, aMsgWindow, aUrlListener, nsnull, mailboxAction, nsnull, aURL);
}

nsresult nsMailboxService::CopyMessages(nsMsgKeyArray *msgKeys,
							  nsIMsgFolder *srcFolder,
                              nsIStreamListener * aMailboxCopyHandler,
                              PRBool moveMessage,
                              nsIUrlListener * aUrlListener,
                              nsIMsgWindow *aMsgWindow,
                              nsIURI **aURL)
{
  nsresult rv = NS_OK;
	nsCOMPtr<nsIMailboxUrl> mailboxurl;

  nsMailboxAction actionToUse = nsIMailboxUrl::ActionMoveMessage;
  if (!moveMessage)
     actionToUse = nsIMailboxUrl::ActionCopyMessage;

  rv = PrepareMessageCopyUrl(srcFolder, aUrlListener, actionToUse , getter_AddRefs(mailboxurl), aMsgWindow);

	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(url));
    nsCOMPtr<nsIMailboxUrl> mailboxUrl (do_QueryInterface(url));
    msgUrl->SetMsgWindow(aMsgWindow);

    mailboxUrl->SetMoveCopyMsgKeys(msgKeys->GetArray(), msgKeys->GetSize());
    rv = RunMailboxUrl(url, aMailboxCopyHandler); 
	}

	if (aURL)
		mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);

  return rv;
}

nsresult nsMailboxService::FetchMessage(const char* aMessageURI,
                                        nsISupports * aDisplayConsumer, 
                                        nsIMsgWindow * aMsgWindow,
										                    nsIUrlListener * aUrlListener,
                                        const char * aFileName, /* only used by open attachment... */
                                        nsMailboxAction mailboxAction,
                                        const PRUnichar * aCharsetOverride,
                                        nsIURI ** aURL)
{
  nsresult rv = NS_OK;
	nsCOMPtr<nsIMailboxUrl> mailboxurl;

  nsMailboxAction actionToUse = mailboxAction;

  rv = PrepareMessageUrl(aMessageURI, aUrlListener, actionToUse , getter_AddRefs(mailboxurl), aMsgWindow);

	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(url));
    msgUrl->SetMsgWindow(aMsgWindow);
    nsCOMPtr<nsIMsgI18NUrl> i18nurl (do_QueryInterface(msgUrl));
    i18nurl->SetCharsetOverRide(aCharsetOverride);

    if (aFileName)
      msgUrl->SetFileName(aFileName);

		// instead of running the mailbox url like we used to, let's try to run the url in the docshell...
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
    // if we were given a docShell, run the url in the docshell..otherwise just run it normally.
    if (NS_SUCCEEDED(rv) && docShell)
    {
      nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
      // DIRTY LITTLE HACK --> if we are opening an attachment we want the docshell to
      // treat this load as if it were a user click event. Then the dispatching stuff will be much
      // happier.
      if (mailboxAction == nsIMailboxUrl::ActionFetchPart)
      {
        docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
        loadInfo->SetLoadType(nsIDocShellLoadInfo::loadLink);
      }
	    rv = docShell->LoadURI(url, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
    }
    else
      rv = RunMailboxUrl(url, aDisplayConsumer); 
	}

	if (aURL)
		mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);

  return rv;
}

NS_IMETHODIMP nsMailboxService::FetchMimePart(nsIURI *aURI, const char *aMessageURI, nsISupports *aDisplayConsumer, nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIURI **aURL)
{
  nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(aURI));
  msgUrl->SetMsgWindow(aMsgWindow);

  // set up the url listener
	if (aUrlListener)
		msgUrl->RegisterListener(aUrlListener);
  
  return RunMailboxUrl(msgUrl, aDisplayConsumer); 
}

NS_IMETHODIMP nsMailboxService::DisplayMessage(const char* aMessageURI,
                                          nsISupports * aDisplayConsumer,
                                          nsIMsgWindow * aMsgWindow,
										                      nsIUrlListener * aUrlListener,
                                          const PRUnichar * aCharsetOveride,
                                          nsIURI ** aURL)
{
  return FetchMessage(aMessageURI, aDisplayConsumer,
                      aMsgWindow,aUrlListener, nsnull,
                      nsIMailboxUrl::ActionFetchMessage, aCharsetOveride, aURL);
}

NS_IMETHODIMP nsMailboxService::OpenAttachment(const char *aContentType, 
                                               const char *aFileName,
                                               const char *aUrl, 
                                               const char *aMessageUri, 
                                               nsISupports *aDisplayConsumer, 
                                               nsIMsgWindow *aMsgWindow, 
                                               nsIUrlListener *aUrlListener)
{
  nsCAutoString partMsgUrl(aMessageUri);
  
  // try to extract the specific part number out from the url string
  partMsgUrl += "?";
  const char *part = PL_strstr(aUrl, "part=");
  partMsgUrl += part;
  partMsgUrl += "&type=";
  partMsgUrl += aContentType;
  partMsgUrl += "&filename=";
  partMsgUrl += aFileName;
  return FetchMessage(partMsgUrl.get(), aDisplayConsumer,
                      aMsgWindow,aUrlListener, aFileName,
                      nsIMailboxUrl::ActionFetchPart, nsnull, nsnull);

}


NS_IMETHODIMP 
nsMailboxService::SaveMessageToDisk(const char *aMessageURI, 
                                    nsIFileSpec *aFile, 
                                    PRBool aAddDummyEnvelope, 
                                    nsIUrlListener *aUrlListener,
                                    nsIURI **aURL,
                                    PRBool canonicalLineEnding,
									                  nsIMsgWindow *aMsgWindow)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIMailboxUrl> mailboxurl;

	rv = PrepareMessageUrl(aMessageURI, aUrlListener, nsIMailboxUrl::ActionSaveMessageToDisk, getter_AddRefs(mailboxurl), aMsgWindow);

	if (NS_SUCCEEDED(rv))
	{
    nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(mailboxurl);
    if (msgUrl)
    {
		  msgUrl->SetMessageFile(aFile);
      msgUrl->SetAddDummyEnvelope(aAddDummyEnvelope);
      msgUrl->SetCanonicalLineEnding(canonicalLineEnding);
    }
		
    nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
		rv = RunMailboxUrl(url);
	}

	if (aURL)
		mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);
	
	return rv;
}

NS_IMETHODIMP nsMailboxService::GetUrlForUri(const char *aMessageURI, nsIURI **aURL, nsIMsgWindow *aMsgWindow)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMailboxUrl> mailboxurl;
  rv = PrepareMessageUrl(aMessageURI, nsnull, nsIMailboxUrl::ActionFetchMessage, getter_AddRefs(mailboxurl), aMsgWindow);
  if (NS_SUCCEEDED(rv) && mailboxurl)
    rv = mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);
  return rv;
}

nsresult nsMailboxService::DisplayMessageNumber(const char *url,
                                                PRUint32 aMessageNumber,
                                                nsISupports * aDisplayConsumer,
                                                nsIUrlListener * aUrlListener,
                                                nsIURI ** aURL)
{
	// mscott - this function is no longer supported...
	NS_ASSERTION(0, "deprecated method");
	return NS_OK;
}

// Takes a mailbox url, this method creates a protocol instance and loads the url
// into the protocol instance.
nsresult nsMailboxService::RunMailboxUrl(nsIURI * aMailboxUrl, nsISupports * aDisplayConsumer)
{
	// create a protocol instance to run the url..
	nsresult rv = NS_OK;
	nsMailboxProtocol * protocol = new nsMailboxProtocol(aMailboxUrl);

	if (protocol)
	{
    rv = protocol->Initialize(aMailboxUrl);
    if (NS_FAILED(rv)) 
    {
      delete protocol;
      return rv;
    }
		NS_ADDREF(protocol);
		rv = protocol->LoadUrl(aMailboxUrl, aDisplayConsumer);
		NS_RELEASE(protocol); // after loading, someone else will have a ref cnt on the mailbox
	}
		
	return rv;
}

nsresult nsMailboxService::PrepareMessageCopyUrl(nsIMsgFolder *folder, nsIUrlListener * aUrlListener,
											 nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl,
											 nsIMsgWindow *msgWindow)
{
	nsresult rv = NS_OK;
	rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            NS_GET_IID(nsIMailboxUrl),
                                            (void **) aMailboxUrl);

	if (NS_SUCCEEDED(rv) && aMailboxUrl && *aMailboxUrl)
	{
		// okay now generate the url string
		char * urlSpec;
		nsCAutoString folderURI;
    nsCOMPtr<nsIFileSpec> localPath;
		nsFileSpec folderPath;
    rv = folder->GetPath(getter_AddRefs(localPath));
    if (NS_SUCCEEDED(rv)) 
      localPath->GetFileSpec(&folderPath);

		if (NS_SUCCEEDED(rv))
		{
			// set up the url spec and initialize the url with it.
			nsFilePath filePath(folderPath); // convert to file url representation...

      urlSpec = PR_smprintf("mailbox://%s", (const char *) filePath);
            
			nsCOMPtr <nsIMsgMailNewsUrl> url = do_QueryInterface(*aMailboxUrl);
    		url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);
   
      (*aMailboxUrl)->SetMailboxAction(aMailboxAction);

			// set up the url listener
			if (aUrlListener)
				rv = url->RegisterListener(aUrlListener);

			url->SetMsgWindow(msgWindow);
      nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(url);
      if (msgUrl)
      {
//        msgUrl->SetOriginalSpec(aSrcMsgMailboxURI);
//        msgUrl->SetUri(aSrcMsgMailboxURI);
      }

		} // if we got a url
	} // if we got a url

	return rv;
}

// This function takes a message uri, converts it into a file path & msgKey 
// pair. It then turns that into a mailbox url object. It also registers a url
// listener if appropriate. AND it can take in a mailbox action and set that field
// on the returned url as well.
nsresult nsMailboxService::PrepareMessageUrl(const char * aSrcMsgMailboxURI, nsIUrlListener * aUrlListener,
											 nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl,
											 nsIMsgWindow *msgWindow)
{
	nsresult rv = NS_OK;
	rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            NS_GET_IID(nsIMailboxUrl),
                                            (void **) aMailboxUrl);

	if (NS_SUCCEEDED(rv) && aMailboxUrl && *aMailboxUrl)
	{
		// okay now generate the url string
		char * urlSpec;
		nsCAutoString folderURI;
		nsFileSpec folderPath;
		nsMsgKey msgKey;
    const char *part = PL_strstr(aSrcMsgMailboxURI, "part=");
		rv = nsParseLocalMessageURI(aSrcMsgMailboxURI, folderURI, &msgKey);
    NS_ENSURE_SUCCESS(rv,rv);
		rv = nsLocalURI2Path(kMailboxRootURI, folderURI.get(), folderPath);

		if (NS_SUCCEEDED(rv))
		{
			// set up the url spec and initialize the url with it.
			nsFilePath filePath(folderPath); // convert to file url representation...
            nsXPIDLCString escapedFilePath;
            *((char**)getter_Copies(escapedFilePath)) =
                                    nsEscape(filePath, url_Path);
      // we're not using the escapedFilePath at this point because it doesn't work.
      if (mPrintingOperation)
          urlSpec = PR_smprintf("mailbox://%s?number=%d&header=print", (const char *) filePath, msgKey);
      else if (part)
          urlSpec = PR_smprintf("mailbox://%s?number=%d&%s", (const char *)
                                filePath, msgKey, part);
      else
          urlSpec = PR_smprintf("mailbox://%s?number=%d", (const char *) filePath, msgKey);
            
			nsCOMPtr <nsIMsgMailNewsUrl> url = do_QueryInterface(*aMailboxUrl);
			url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);
   
      (*aMailboxUrl)->SetMailboxAction(aMailboxAction);

			// set up the url listener
			if (aUrlListener)
				rv = url->RegisterListener(aUrlListener);

			url->SetMsgWindow(msgWindow);
      nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(url);
      if (msgUrl)
      {
        msgUrl->SetOriginalSpec(aSrcMsgMailboxURI);
        msgUrl->SetUri(aSrcMsgMailboxURI);
      }

		} // if we got a url
	} // if we got a url

	return rv;
}

NS_IMETHODIMP nsMailboxService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = nsCRT::strdup("mailbox");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsMailboxService::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = -1;  // mailbox doesn't use a port!!!!!
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 	
}

NS_IMETHODIMP nsMailboxService::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsMailboxService::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD;
    return NS_OK; 	
}

NS_IMETHODIMP nsMailboxService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	nsCOMPtr<nsIMailboxUrl> aMsgUrl;
	nsresult rv = NS_OK;
  if (PL_strstr(aSpec, "?uidl=") || PL_strstr(aSpec, "&uidl="))
  {
    nsCOMPtr<nsIPop3Service> pop3Service = 
             do_GetService(kCPop3ServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIProtocolHandler> handler = do_QueryInterface(pop3Service,
                                                             &rv);
    if (NS_SUCCEEDED(rv))
        rv = handler->NewURI(aSpec, aBaseURI, _retval);
  }
  else
  {
    rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            NS_GET_IID(nsIMailboxUrl),
                                            getter_AddRefs(aMsgUrl));
        
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIURL> aUrl = do_QueryInterface(aMsgUrl);
      aUrl->SetSpec((char *) aSpec);
      aMsgUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
    }
  }

	return rv;
}

NS_IMETHODIMP nsMailboxService::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
	nsresult rv = NS_OK;
	nsMailboxProtocol * protocol = new nsMailboxProtocol(aURI);
	if (protocol)
	{
    rv = protocol->Initialize(aURI);
    if (NS_FAILED(rv)) 
    {
      delete protocol;
      return rv;
    }
		rv = protocol->QueryInterface(NS_GET_IID(nsIChannel), (void **) _retval);
	}
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}

nsresult nsMailboxService::DisplayMessageForPrinting(const char* aMessageURI,
                                                      nsISupports * aDisplayConsumer,
                                                      nsIMsgWindow * aMsgWindow,
										                                  nsIUrlListener * aUrlListener,
                                                      nsIURI ** aURL)
{
  mPrintingOperation = PR_TRUE;
  nsresult rv = FetchMessage(aMessageURI, aDisplayConsumer, aMsgWindow,aUrlListener, nsnull, 
                             nsIMailboxUrl::ActionFetchMessage, nsnull, aURL);
  mPrintingOperation = PR_FALSE;
  return rv;
}

NS_IMETHODIMP nsMailboxService::Search(nsIMsgSearchSession *aSearchSession, nsIMsgWindow *aMsgWindow, nsIMsgFolder *aMsgFolder, const char *aMessageUri)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult 
nsMailboxService::DecomposeMailboxURI(const char * aMessageURI, nsIMsgFolder ** aFolder, nsMsgKey *aMsgKey)
{
  NS_ENSURE_ARG_POINTER(aMessageURI);
  NS_ENSURE_ARG_POINTER(aFolder);
  NS_ENSURE_ARG_POINTER(aMsgKey);
  
  nsresult rv = NS_OK;
  nsCAutoString folderURI;
  rv = nsParseLocalMessageURI(aMessageURI, folderURI, aMsgKey);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1",&rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(folderURI.get(), getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = res->QueryInterface(NS_GET_IID(nsIMsgFolder), (void **) aFolder);
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

NS_IMETHODIMP
nsMailboxService::MessageURIToMsgHdr(const char *uri, nsIMsgDBHdr **_retval)
{
  NS_ENSURE_ARG_POINTER(uri);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIMsgFolder> folder;
  nsMsgKey msgKey;

  rv = DecomposeMailboxURI(uri, getter_AddRefs(folder), &msgKey);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = folder->GetMessageHeader(msgKey, _retval);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}
