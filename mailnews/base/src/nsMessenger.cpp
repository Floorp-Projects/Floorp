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
 */

#include "prsystem.h"

#include "nsMessenger.h"

// xpcom
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIFileSpecWithUI.h"
#include "nsFileStream.h"
#include "nsIStringStream.h"
#include "nsEscape.h"
#include "nsXPIDLString.h"

// necko
#include "nsMimeTypes.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIStreamConverterService.h"
#include "nsNetUtil.h"

// rdf
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"

// gecko
#include "nsLayoutCID.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIContentViewer.h" 

/* rhp - for access to webshell */
#include "nsIDOMWindow.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIScriptGlobalObject.h"


// mail
#include "nsMsgUtils.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"

#include "nsIMsgFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgIncomingServer.h"

#include "nsIMsgMessageService.h"
#include "nsIMessage.h"

#include "nsMsgStatusFeedback.h"
#include "nsMsgRDFUtils.h"

// compose
#include "nsMsgCompCID.h"
#include "nsMsgI18N.h"

// draft/folders/sendlater/etc
#include "nsIMsgCopyService.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIMsgSendLater.h" 
#include "nsIMsgSendLaterListener.h"
#include "nsIMsgDraft.h"
#include "nsIUrlListener.h"

static NS_DEFINE_CID(kIStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgSendLaterCID, NS_MSGSENDLATER_CID); 
static NS_DEFINE_CID(kMsgDraftCID, NS_MSGDRAFT_CID);
static NS_DEFINE_CID(kPrintPreviewContextCID, NS_PRINT_PREVIEW_CONTEXT_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,		NS_MSGCOPYSERVICE_CID);

#if defined(DEBUG_seth_) || defined(DEBUG_sspitzer_) || defined(DEBUG_jefft)
#define DEBUG_MESSENGER
#endif

#define FOUR_K 4096

// ***************************************************
// jefft - this is a rather obscured class serves for Save Message As File,
// Save Message As Template, and Save Attachment to a file
// 
class nsSaveAsListener : public nsIUrlListener,
                         public nsIMsgCopyServiceListener,
                         public nsIStreamListener
{
public:
    nsSaveAsListener(nsIFileSpec* fileSpec);
    virtual ~nsSaveAsListener();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIURLLISTENER
    NS_DECL_NSIMSGCOPYSERVICELISTENER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMOBSERVER

    nsCOMPtr<nsIFileSpec> m_fileSpec;
    nsCOMPtr<nsIOutputStream> m_outputStream;
    char *m_dataBuffer;
    nsCOMPtr<nsIChannel> m_channel;
    nsXPIDLCString m_templateUri;
};

//
// nsMessenger
//
nsMessenger::nsMessenger() : m_folderPath("")
{
	NS_INIT_REFCNT();
	mScriptObject = nsnull;
	mWebShell = nsnull; 
	mWindow = nsnull;
  mMsgWindow = nsnull;

	InitializeFolderRoot();
}

nsMessenger::~nsMessenger()
{
    NS_IF_RELEASE(mWindow);
    NS_IF_RELEASE(mWebShell);
}

//
// nsISupports
//
NS_IMPL_ISUPPORTS(nsMessenger, NS_GET_IID(nsIMessenger))

nsresult
nsMessenger::GetNewMessages(nsIRDFCompositeDataSource *db,
                            nsIRDFResource *folderResource)
{
	nsresult rv=NS_OK;
	nsCOMPtr<nsISupportsArray> folderArray;

	if(!folderResource || !db)
		return NS_ERROR_NULL_POINTER;

	if(NS_FAILED(NS_NewISupportsArray(getter_AddRefs(folderArray))))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(folderResource);

	DoCommand(db, NC_RDF_GETNEWMESSAGES, folderArray, nsnull);

	return rv;
}


NS_IMETHODIMP    
nsMessenger::SetWindow(nsIDOMWindow *aWin, nsIMsgWindow *aMsgWindow)
{
	if(!aWin)
		return NS_ERROR_NULL_POINTER;

  mMsgWindow = aMsgWindow;

  nsAutoString  webShellName("messagepane");
  NS_IF_RELEASE(mWindow);
  mWindow = aWin;
  NS_ADDREF(aWin);

#ifdef DEBUG
  /* rhp - Needed to access the webshell to drive message display */
  printf("nsMessenger::SetWindow(): Getting the webShell of interest...\n");
#endif

  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  if (!globalObj) 
  {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell *webShell = nsnull;
  nsIWebShell *rootWebShell = nsnull;

  globalObj->GetWebShell(&webShell);
  if (nsnull == webShell) 
  {
    return NS_ERROR_FAILURE;
  }

  webShell->GetRootWebShell(rootWebShell);
  if (nsnull != rootWebShell) 
  {
    nsresult rv = rootWebShell->FindChildWithName(webShellName.GetUnicode(), mWebShell);
#ifdef NS_DEBUG
    if (NS_SUCCEEDED(rv) && nsnull != mWebShell)
        printf("nsMessenger::SetWindow(): Got the webShell %s.\n", (const char *) nsAutoCString(webShellName));
    else
        printf("nsMessenger::SetWindow(): Failed to find webshell %s.\n", (const char *) nsAutoCString(webShellName));
#endif
	if (mWebShell)
	{
		if (aMsgWindow)
		{
			nsCOMPtr<nsIMsgStatusFeedback> aStatusFeedback;

			aMsgWindow->GetStatusFeedback(getter_AddRefs(aStatusFeedback));
			m_docLoaderObserver = do_QueryInterface(aStatusFeedback);
			if (aStatusFeedback)
				aStatusFeedback->SetWebShell(mWebShell, mWindow);
			mWebShell->SetDocLoaderObserver(m_docLoaderObserver);
            NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
            if(NS_SUCCEEDED(rv))
	            mailSession->SetTemporaryMsgWindow(aMsgWindow);

            aMsgWindow->GetTransactionManager(getter_AddRefs(mTxnMgr));
		}
	}
    NS_RELEASE(rootWebShell);
  }

  NS_RELEASE(webShell);

  // libmime always converts to UTF-8 (both HTML and XML)
  if (nsnull != mWebShell) 
  {
	  nsAutoString aForceCharacterSet("UTF-8");
    nsCOMPtr<nsIContentViewer> cv;
    mWebShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);
      if (muDV) {
        muDV->SetForceCharacterSet(aForceCharacterSet.GetUnicode());
      }
    }
  }

  return NS_OK;
}


// this should really go through all the pop servers and initialize all
// folder roots
void nsMessenger::InitializeFolderRoot()
{
    nsresult rv;
    
	// get the current identity from the mail session....
    NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                    NS_MSGACCOUNTMANAGER_PROGID, &rv)
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = accountManager->GetCurrentServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return;
    
    nsCOMPtr<nsIFileSpec> folderRoot;
    if (NS_SUCCEEDED(rv) && server)
        rv = server->GetLocalPath(getter_AddRefs(folderRoot));
    
    if (NS_SUCCEEDED(rv) && folderRoot) {
        // everyone should have a inbox so let's
        // tack that folder name on to the root path...
        rv = folderRoot->GetFileSpec(&m_folderPath);
        if (NS_SUCCEEDED(rv))
          m_folderPath += "Inbox";
    } // if we have a folder root for the current server
}

NS_IMETHODIMP
nsMessenger::OpenURL(const char * url)
{
	if (url)
	{
#ifdef DEBUG_MESSENGER
		printf("nsMessenger::OpenURL(%s)\n",url);
#endif    
        char* unescapedUrl = PL_strdup(url);
        if (unescapedUrl)
        {
          nsUnescape(unescapedUrl);
          
          nsIMsgMessageService * messageService = nsnull;
          nsresult rv = GetMessageServiceFromURI(unescapedUrl,
                                                 &messageService);
          
          if (NS_SUCCEEDED(rv) && messageService)
          {
			      messageService->DisplayMessage(unescapedUrl, mWebShell, mMsgWindow, nsnull, nsnull);
			      ReleaseMessageServiceFromURI(unescapedUrl, messageService);
          }
		      //If it's not something we know about, then just load the url.
          else
          {
			      nsAutoString urlStr(unescapedUrl);
			      if (mWebShell) {
				    mWebShell->LoadURL(urlStr.GetUnicode());
			    }
          }
          PL_strfree(unescapedUrl);
        }
        else
        {
          return NS_ERROR_OUT_OF_MEMORY;
        }
	}
	return NS_OK;
}


NS_IMETHODIMP
nsMessenger::OpenAttachment(const char * url, const char * displayName, 
                            const char * messageUri)
{
  nsresult rv = NS_ERROR_FAILURE;
  char *unescapedUrl = nsnull;
  nsIMsgMessageService * messageService = nsnull;

	if (url)
	{
#ifdef DEBUG_MESSENGER
		printf("nsMessenger::OpenAttachment(%s)\n",url);
#endif    
        unescapedUrl = PL_strdup(url);
        if (unescapedUrl)
        {
          nsUnescape(unescapedUrl);
          
          nsCOMPtr<nsIFileSpec> aSpec;
          nsCOMPtr<nsIFileSpecWithUI>
            fileSpec(getter_AddRefs(NS_CreateFileSpecWithUI()));
          
          if (!fileSpec) goto done;

          rv = fileSpec->ChooseOutputFile("Save Attachment", displayName,
                                          nsIFileSpecWithUI::eAllFiles);
          if (NS_FAILED(rv)) goto done;
            
          aSpec = do_QueryInterface(fileSpec, &rv);
          if (NS_FAILED(rv)) goto done;

          nsSaveAsListener *aListener = new nsSaveAsListener(aSpec);
          if (aListener)
          {
            NS_ADDREF(aListener);
            nsCOMPtr<nsIURI> aURL;

            nsAutoString urlString = unescapedUrl;
            char *urlCString = urlString.ToNewCString();
            rv = CreateStartupUrl(urlCString, getter_AddRefs(aURL));
            nsCRT::free(urlCString);
            if (NS_FAILED(rv)) 
            {
              NS_RELEASE(aListener);
              goto done;
            }
            aListener->m_channel = null_nsCOMPtr();
            rv = NS_NewInputStreamChannel(aURL,
                                          nsnull,      // contentType
                                          -1,          // contentLength
                                          nsnull,      // inputStream
                                          nsnull,      // loadGroup
                                          nsnull,      // notificationCallbacks
                                          nsIChannel::LOAD_NORMAL,
                                          nsnull,      // originalURI
                                          0, 0, 
                                          getter_AddRefs(aListener->m_channel));
            if (NS_FAILED(rv))
            {
              NS_RELEASE(aListener);
              goto done;
            }
            nsAutoString from, to;
            from = "message/rfc822";
            to = "text/xul";
            NS_WITH_SERVICE(nsIStreamConverterService,
                            streamConverterService,  
                            kIStreamConverterServiceCID, &rv);
            nsCOMPtr<nsISupports> channelSupport =
              do_QueryInterface(aListener->m_channel);
            nsCOMPtr<nsIStreamListener> convertedListener;
            rv = streamConverterService->AsyncConvertData(
              from.GetUnicode(), to.GetUnicode(), aListener,
              channelSupport, getter_AddRefs(convertedListener));

            rv = GetMessageServiceFromURI(messageUri, &messageService);
            
            if (NS_SUCCEEDED(rv) && messageService)
            {
              rv = messageService->DisplayMessage(messageUri,
                                                  convertedListener,mMsgWindow,
                                                  nsnull, nsnull); 
            }
          }
          else
          {
            rv = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        else
        {
          rv =  NS_ERROR_OUT_OF_MEMORY;
        }
	}
done:
    if (messageService)
      ReleaseMessageServiceFromURI(unescapedUrl, messageService);
    PR_FREEIF(unescapedUrl);
	return rv;
}


NS_IMETHODIMP
nsMessenger::SaveAs(const char* url, PRBool asFile, nsIMsgIdentity* identity)
{
	nsresult rv = NS_OK;
    nsIMsgMessageService* messageService = nsnull;
    
    if (url)
    {
      rv = GetMessageServiceFromURI(url, &messageService);

      if (NS_SUCCEEDED(rv) && messageService)
      {
        nsCOMPtr<nsIFileSpec> aSpec;
        PRUint32 saveAsFileType = 0; // 0 - raw, 1 = html, 2 = text;

        if (asFile)
        {
          nsCOMPtr<nsIFileSpecWithUI>
            fileSpec(getter_AddRefs(NS_CreateFileSpecWithUI()));
 
          if (!fileSpec) {
            rv = NS_ERROR_FAILURE;
            goto done;
          }

          rv = fileSpec->ChooseOutputFile("Save Message", "",
                                nsIFileSpecWithUI::eAllMailOutputFilters);
          if (NS_FAILED(rv)) goto done;
          char *fileName = nsnull;
          fileSpec->GetLeafName(&fileName);

          nsCString tmpFilenameString = fileName;
      
          nsCRT::free(fileName);

          if (tmpFilenameString.RFind(".htm", PR_TRUE) != -1)
            saveAsFileType = 1;
          else if (tmpFilenameString.RFind(".txt", PR_TRUE) != -1)
            saveAsFileType = 2;
          
          aSpec = do_QueryInterface(fileSpec, &rv);
        }
        else
        {
          nsFileSpec tmpFileSpec("msgTemp.eml");
          rv = NS_NewFileSpecWithSpec(tmpFileSpec, getter_AddRefs(aSpec));
        }
        if (NS_FAILED(rv)) goto done;
        nsCOMPtr<nsIUrlListener> urlListener;
        if (aSpec)
        {
          if (asFile)
          {
            switch (saveAsFileType) 
            {
              case 0:
              default:
                messageService->SaveMessageToDisk(url, aSpec, PR_TRUE,
                                                  urlListener, nsnull,
                                                  PR_FALSE);
                break;
              case 1:
              case 2:
              {
                nsSaveAsListener *aListener = new nsSaveAsListener(aSpec);
                if (aListener)
                {
                  NS_ADDREF(aListener);
                  nsCOMPtr<nsIURI> aURL;
                  nsAutoString urlString = url;
                  urlString += "?header=none";
                  char *urlCString = urlString.ToNewCString();
                  rv = CreateStartupUrl(urlCString, getter_AddRefs(aURL));
                  nsCRT::free(urlCString);
                  if (NS_FAILED(rv)) 
                  {
                    NS_RELEASE(aListener);
                    goto done;
                  }
                  aListener->m_channel = null_nsCOMPtr();
                  rv = NS_NewInputStreamChannel(aURL, 
                                                nsnull,      // contentType
                                                -1,          // contentLength
                                                nsnull,      // inputStream
                                                nsnull,      // loadGroup
                                                nsnull,      // notificationCallbacks
                                                nsIChannel::LOAD_NORMAL,
                                                nsnull,      // originalURI
                                                0, 0, 
                                                getter_AddRefs(aListener->m_channel));
                  if (NS_FAILED(rv))
                  {
                    NS_RELEASE(aListener);
                    goto done;
                  }
                  nsAutoString from, to;
                  from = MESSAGE_RFC822;
                  to = saveAsFileType == 1 ? TEXT_HTML : TEXT_PLAIN;
                  NS_WITH_SERVICE(nsIStreamConverterService,
                                  streamConverterService,  
                                  kIStreamConverterServiceCID, &rv);
                  nsCOMPtr<nsISupports> channelSupport =
                    do_QueryInterface(aListener->m_channel);
                  nsCOMPtr<nsIStreamListener> convertedListener;
                  rv = streamConverterService->AsyncConvertData(
                    from.GetUnicode(), to.GetUnicode(), aListener,
                    channelSupport, getter_AddRefs(convertedListener));
                  if (NS_SUCCEEDED(rv))
                    messageService->DisplayMessage(url, convertedListener,mMsgWindow,
                                                   nsnull, nsnull);
                }
              }
            }
          }
          else
          { 
              // ** save as Template
              PRBool needDummyHeader = PR_TRUE;
              PRBool canonicalLineEnding = PR_FALSE;
              nsSaveAsListener *aListener = new nsSaveAsListener(aSpec);

              if (aListener)
              {
                if (identity)
                  rv = identity->GetStationeryFolder(
                    getter_Copies(aListener->m_templateUri));
                if (NS_FAILED(rv)) return rv;
                needDummyHeader =
                  PL_strcasestr(aListener->m_templateUri, "mailbox://") 
                  != nsnull;
                canonicalLineEnding =
                  PL_strcasestr(aListener->m_templateUri, "imap://")
                  != nsnull;
                rv = aListener->QueryInterface(
                  NS_GET_IID(nsIUrlListener),
                  getter_AddRefs(urlListener));
                if (NS_FAILED(rv))
                {
                  delete aListener;
                  return rv;
                }
                NS_ADDREF(aListener); 
                // nsUrlListenerManager uses nsVoidArray
                // to keep trach of all listeners we have
                // to manually add refs ourself
                messageService->SaveMessageToDisk(url, aSpec, 
                                                  needDummyHeader,
                                                  urlListener, nsnull,
                                                  canonicalLineEnding); 
              }
          }
        }
      }
    }
done:
    if (messageService)
        ReleaseMessageServiceFromURI(url, messageService);

	return rv;
}

nsresult
nsMessenger::DoCommand(nsIRDFCompositeDataSource* db, char *command,
                       nsISupportsArray *srcArray, 
                       nsISupportsArray *argumentArray)
{

	nsresult rv;

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> commandResource;
	rv = rdfService->GetResource(command, getter_AddRefs(commandResource));
	if(NS_SUCCEEDED(rv))
	{
		rv = db->DoCommand(srcArray, commandResource, argumentArray);
	}

	return rv;

}

NS_IMETHODIMP
nsMessenger::DeleteMessages(nsIRDFCompositeDataSource *database,
                            nsIRDFResource *srcFolderResource,
                            nsISupportsArray *resourceArray)
{
	nsresult rv;

	if(!database || !srcFolderResource || !resourceArray)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> folderArray;

	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	folderArray->AppendElement(srcFolderResource);
	
	rv = DoCommand(database, NC_RDF_DELETE, folderArray, resourceArray);

	return rv;
}

NS_IMETHODIMP nsMessenger::DeleteFolders(nsIRDFCompositeDataSource *db,
                                         nsIRDFResource *parentResource,
                                         nsIRDFResource *deletedFolderResource)
{
	nsresult rv;

	if(!db || !parentResource || !deletedFolderResource)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> parentArray, deletedArray;

	rv = NS_NewISupportsArray(getter_AddRefs(parentArray));

	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	rv = NS_NewISupportsArray(getter_AddRefs(deletedArray));

	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	parentArray->AppendElement(parentResource);
	deletedArray->AppendElement(deletedFolderResource);

	rv = DoCommand(db, NC_RDF_DELETE, parentArray, deletedArray);

	return NS_OK;
}

NS_IMETHODIMP
nsMessenger::CopyMessages(nsIRDFCompositeDataSource *database,
                          nsIRDFResource *srcResource, // folder
						  nsIRDFResource *dstResource,
                          nsISupportsArray *argumentArray, // nsIMessages
                          PRBool isMove)
{
	nsresult rv;

	if(!srcResource || !dstResource || !argumentArray)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIMsgFolder> srcFolder;
	nsCOMPtr<nsISupportsArray> folderArray;
    
	srcFolder = do_QueryInterface(srcResource);
	if(!srcFolder)
		return NS_ERROR_NO_INTERFACE;

	nsCOMPtr<nsISupports> srcFolderSupports(do_QueryInterface(srcFolder));
	if(srcFolderSupports)
		argumentArray->InsertElementAt(srcFolderSupports, 0);

	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	folderArray->AppendElement(dstResource);
	
	rv = DoCommand(database, isMove ? (char *)NC_RDF_MOVE : (char *)NC_RDF_COPY, folderArray, argumentArray);
	return rv;

}


NS_IMETHODIMP
nsMessenger::MarkMessageRead(nsIRDFCompositeDataSource *database,
                             nsIRDFResource *messageResource, PRBool markRead)
{
	if(!database || !messageResource)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;


	nsCOMPtr<nsISupportsArray> resourceArray;

	rv = NS_NewISupportsArray(getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	resourceArray->AppendElement(messageResource);

	rv = MarkMessagesRead(database, resourceArray, markRead);

	return rv;
}

NS_IMETHODIMP
nsMessenger::MarkMessagesRead(nsIRDFCompositeDataSource *database,
                              nsISupportsArray *resourceArray,
                              PRBool markRead)
{
	nsresult rv;

	if(!database || !resourceArray)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> argumentArray;

	rv = NS_NewISupportsArray(getter_AddRefs(argumentArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	if(markRead)
		rv = DoCommand(database, NC_RDF_MARKREAD, resourceArray, argumentArray);
	else
		rv = DoCommand(database, NC_RDF_MARKUNREAD, resourceArray,  argumentArray);

	return rv;

}

NS_IMETHODIMP
nsMessenger::MarkAllMessagesRead(nsIRDFCompositeDataSource *database,
                                 nsIRDFResource *folderResource)
{
	nsresult rv=NS_OK;
	nsCOMPtr<nsISupportsArray> folderArray;

	if(!folderResource || !database)
		return NS_ERROR_NULL_POINTER;

	if(NS_FAILED(NS_NewISupportsArray(getter_AddRefs(folderArray))))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(folderResource);

	DoCommand(database, NC_RDF_MARKALLMESSAGESREAD, folderArray, nsnull);

	return rv;
}

NS_IMETHODIMP
nsMessenger::MarkMessageFlagged(nsIRDFCompositeDataSource *database,
                                nsIRDFResource *messageResource,
                                PRBool markFlagged)
{
	if(!database || !messageResource)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	nsCOMPtr<nsISupportsArray> resourceArray;

	rv = NS_NewISupportsArray(getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	resourceArray->AppendElement(messageResource);

	rv = MarkMessagesFlagged(database, resourceArray, markFlagged);

	return rv;
}

NS_IMETHODIMP
nsMessenger::MarkMessagesFlagged(nsIRDFCompositeDataSource *database,
                                 nsISupportsArray *resourceArray,
                                 PRBool markFlagged)
{
	nsresult rv;

	if(!database || !resourceArray)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> argumentArray;

	rv = NS_NewISupportsArray(getter_AddRefs(argumentArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	if(markFlagged)
		rv = DoCommand(database, NC_RDF_MARKFLAGGED, resourceArray, argumentArray);
	else
		rv = DoCommand(database, NC_RDF_MARKUNFLAGGED, resourceArray,  argumentArray);

	return rv;

}

NS_IMETHODIMP
nsMessenger::NewFolder(nsIRDFCompositeDataSource *database, nsIRDFResource *parentFolderResource,
						const char *name)
{
	nsresult rv;
	nsCOMPtr<nsISupportsArray> nameArray, folderArray;

	if(!parentFolderResource || !name)
		return NS_ERROR_NULL_POINTER;

	rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(parentFolderResource);

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_SUCCEEDED(rv))
	{
		nsAutoString nameStr = name;
		nsCOMPtr<nsIRDFLiteral> nameLiteral;

		rdfService->GetLiteral(nameStr.GetUnicode(), getter_AddRefs(nameLiteral));
		nameArray->AppendElement(nameLiteral);
		rv = DoCommand(database, NC_RDF_NEWFOLDER, folderArray, nameArray);
	}
	return rv;
}

NS_IMETHODIMP
nsMessenger::RenameFolder(nsIRDFCompositeDataSource* db,
                          nsIRDFResource* folderResource,
                          const char* name)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (!db || !folderResource || !name || !*name) return rv;
  nsCOMPtr<nsISupports> streamSupport;
  rv = NS_NewCharInputStream(getter_AddRefs(streamSupport), name);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupportsArray> folderArray;
    nsCOMPtr<nsISupportsArray> argsArray;

    rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
    if (NS_FAILED(rv)) return rv;
    folderArray->AppendElement(folderResource);
    rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
    if (NS_FAILED(rv)) return rv;
    argsArray->AppendElement(streamSupport);
    rv = DoCommand(db, NC_RDF_RENAME, folderArray, argsArray);
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::CompactFolder(nsIRDFCompositeDataSource* db,
                           nsIRDFResource* folderResource)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  
  if (!db || !folderResource) return rv;
  nsCOMPtr<nsISupportsArray> folderArray;

  rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
  if (NS_FAILED(rv)) return rv;
  folderArray->AppendElement(folderResource);
  rv = DoCommand(db, NC_RDF_COMPACT, folderArray, nsnull);

  return rv;
}

NS_IMETHODIMP
nsMessenger::EmptyTrash(nsIRDFCompositeDataSource* db,
                        nsIRDFResource* folderResource)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  
  if (!db || !folderResource) return rv;
  nsCOMPtr<nsISupportsArray> folderArray;

  rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
  if (NS_FAILED(rv)) return rv;
  folderArray->AppendElement(folderResource);
  rv = DoCommand(db, NC_RDF_EMPTYTRASH, folderArray, nsnull);

  return rv;
}

NS_IMETHODIMP
nsMessenger::Undo()
{
  nsresult rv = NS_OK;
  if (mTxnMgr)
  {
    PRInt32 numTxn = 0;
    rv = mTxnMgr->GetNumberOfUndoItems(&numTxn);
    if (NS_SUCCEEDED(rv) && numTxn > 0)
      mTxnMgr->Undo();
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::Redo()
{
  nsresult rv = NS_OK;
  if (mTxnMgr)
  {
    PRInt32 numTxn = 0;
    rv = mTxnMgr->GetNumberOfRedoItems(&numTxn);
    if (NS_SUCCEEDED(rv) && numTxn > 0)
      mTxnMgr->Redo();
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::GetTransactionManager(nsITransactionManager* *aTxnMgr)
{
  if (!mTxnMgr || !aTxnMgr)
    return NS_ERROR_NULL_POINTER;

  *aTxnMgr = mTxnMgr;
  NS_ADDREF(*aTxnMgr);

  return NS_OK;
}

NS_IMETHODIMP nsMessenger::SetDocumentCharset(const PRUnichar *characterSet)
{
	// Set a default charset of the webshell. 
	if (nsnull != mWebShell) {
    nsCOMPtr<nsIContentViewer> cv;
    mWebShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);
      if (muDV) {
		    muDV->SetDefaultCharacterSet(characterSet);
      }
    }
	}
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. 
////////////////////////////////////////////////////////////////////////////////////
class SendLaterListener: public nsIMsgSendLaterListener
{
public:
  SendLaterListener(void);
  virtual ~SendLaterListener(void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  /* void OnStartSending (in PRUint32 aTotalMessageCount); */
  NS_IMETHOD OnStartSending(PRUint32 aTotalMessageCount);

  /* void OnProgress (in PRUint32 aCurrentMessage, in PRUint32 aTotalMessage); */
  NS_IMETHOD OnProgress(PRUint32 aCurrentMessage, PRUint32 aTotalMessage);

  /* void OnStatus (in wstring aMsg); */
  NS_IMETHOD OnStatus(const PRUnichar *aMsg);

  /* void OnStopSending (in nsresult aStatus, in wstring aMsg, in PRUint32 aTotalTried, in PRUint32 aSuccessful); */
  NS_IMETHOD OnStopSending(nsresult aStatus, const PRUnichar *aMsg, PRUint32 aTotalTried, PRUint32 aSuccessful);
};

NS_IMPL_ISUPPORTS(SendLaterListener, NS_GET_IID(nsIMsgSendLaterListener));

SendLaterListener::SendLaterListener()
{
  NS_INIT_REFCNT();
}

SendLaterListener::~SendLaterListener()
{
}

nsresult
SendLaterListener::OnStartSending(PRUint32 aTotalMessageCount)
{
  return NS_OK;
}

nsresult
SendLaterListener::OnProgress(PRUint32 aCurrentMessage, PRUint32 aTotalMessage)
{
  return NS_OK;
}

nsresult
SendLaterListener::OnStatus(const PRUnichar *aMsg)
{
  return NS_OK;
}

nsresult
SendLaterListener::OnStopSending(nsresult aStatus, const PRUnichar *aMsg, PRUint32 aTotalTried, 
                                 PRUint32 aSuccessful) 
{
#ifdef NS_DEBUG
  if (NS_SUCCEEDED(aStatus))
    printf("SendLaterListener::OnStopSending: Tried to send %d messages. %d successful.\n",
            aTotalTried, aSuccessful);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMessenger::SendUnsentMessages()
{
	nsresult rv;
	nsCOMPtr<nsIMsgSendLater> pMsgSendLater; 
	rv = nsComponentManager::CreateInstance(kMsgSendLaterCID, NULL,NS_GET_IID(nsIMsgSendLater),
																					(void **)getter_AddRefs(pMsgSendLater)); 
	if (NS_SUCCEEDED(rv) && pMsgSendLater) 
	{ 
#ifdef DEBUG
		printf("We succesfully obtained a nsIMsgSendLater interface....\n"); 
#endif

    SendLaterListener *sendLaterListener = new SendLaterListener();
    if (!sendLaterListener)
        return NS_ERROR_FAILURE;

    NS_ADDREF(sendLaterListener);
    pMsgSendLater->AddListener(sendLaterListener);

    // temporary hack to get the current identity
    NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                    NS_MSGACCOUNTMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgIdentity> identity;
    rv = accountManager->GetCurrentIdentity(getter_AddRefs(identity));
    if (NS_FAILED(rv)) return rv;
      
    pMsgSendLater->SendUnsentMessages(identity, nsnull); 
    NS_RELEASE(sendLaterListener);
	} 
	return NS_OK;
}

NS_IMETHODIMP
nsMessenger::LoadFirstDraft()
{
	nsresult              rv = NS_ERROR_FAILURE;
	nsCOMPtr<nsIMsgDraft> pMsgDraft; 

	rv = nsComponentManager::CreateInstance(kMsgDraftCID, NULL, NS_GET_IID(nsIMsgDraft),
																					(void **)getter_AddRefs(pMsgDraft)); 
	if (NS_SUCCEEDED(rv) && pMsgDraft) 
	{ 
#ifdef DEBUG
		printf("We succesfully obtained a nsIMsgDraft interface....\n");
#endif


    // This should really pass in a URI, but for now, just to test, we can pass in nsnull
    rv = pMsgDraft->OpenDraftMsg(nsnull, nsnull, PR_FALSE); 
  } 

  return rv;
}

NS_IMETHODIMP nsMessenger::DoPrint()
{
#ifdef DEBUG_MESSENGER
  printf("nsMessenger::DoPrint()\n");
#endif

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContentViewer> viewer;

  NS_ASSERTION(mWebShell,"can't print, there is no webshell");
  if (!mWebShell) {
	return rv;
  }

  mWebShell->GetContentViewer(getter_AddRefs(viewer));

  if (viewer) 
  {
    nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
    if (viewerFile) {
      rv = viewerFile->Print();
    }
#ifdef DEBUG_MESSENGER
    else {
	    printf("the content viewer does not support printing\n");
    }
#endif
  }
#ifdef DEBUG_MESSENGER
  else {
	printf("failed to get the viewer for printing\n");
  }
#endif
  
  return rv;
}

NS_IMETHODIMP nsMessenger::DoPrintPreview()
{
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
#ifdef DEBUG_MESSENGER
  printf("nsMessenger::DoPrintPreview() not implemented yet\n");
#endif
  return rv;  
}

nsSaveAsListener::nsSaveAsListener(nsIFileSpec* aSpec)
{
    NS_INIT_REFCNT();
    if (aSpec)
      m_fileSpec = do_QueryInterface(aSpec);
    m_dataBuffer = nsnull;
}

nsSaveAsListener::~nsSaveAsListener()
{
}

// 
// nsISupports
//
NS_IMPL_ISUPPORTS3(nsSaveAsListener, nsIUrlListener,
                   nsIMsgCopyServiceListener, nsIStreamListener)

// 
// nsIUrlListener
// 
NS_IMETHODIMP
nsSaveAsListener::OnStartRunningUrl(nsIURI* url)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSaveAsListener::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  nsresult rv = exitCode;

  if (m_fileSpec)
  {
    m_fileSpec->Flush();
    m_fileSpec->CloseStream();
    if (NS_FAILED(rv)) goto done;
    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) goto done;
    nsCOMPtr<nsIRDFResource> res;
    rv = rdf->GetResource(m_templateUri, getter_AddRefs(res));
    if (NS_FAILED(rv)) goto done;
    nsCOMPtr<nsIMsgFolder> templateFolder;
    templateFolder = do_QueryInterface(res, &rv);
    if (NS_FAILED(rv)) goto done;
    NS_WITH_SERVICE(nsIMsgCopyService, copyService, kMsgCopyServiceCID, &rv);
    if (NS_FAILED(rv)) goto done;
    rv = copyService->CopyFileMessage(m_fileSpec, templateFolder, nsnull,
                                      PR_TRUE, this, nsnull);
  }

done:
  if (NS_FAILED(rv))
  {
    if (m_fileSpec)
    {
      nsFileSpec realSpec;
      m_fileSpec->GetFileSpec(&realSpec);
      realSpec.Delete(PR_FALSE);
      Release(); // no more work to be done; kill ourself
    }
  }
  return rv;
}

NS_IMETHODIMP
nsSaveAsListener::OnStartCopy(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSaveAsListener::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSaveAsListener::SetMessageKey(PRUint32 aKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSaveAsListener::GetMessageId(nsCString* aMessageId)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSaveAsListener::OnStopCopy(nsresult aStatus)
{
  if (m_fileSpec)
  {
    nsFileSpec realSpec;
    m_fileSpec->GetFileSpec(&realSpec);
    realSpec.Delete(PR_FALSE);
  }
  Release(); // all done kill ourself
  return aStatus;
}

NS_IMETHODIMP
nsSaveAsListener::OnStartRequest(nsIChannel* aChannel, nsISupports* aSupport)
{
  if (m_fileSpec)
    m_fileSpec->GetOutputStream(getter_AddRefs(m_outputStream));
  if (!m_dataBuffer)
    m_dataBuffer = (char*) PR_CALLOC(FOUR_K+1);
  return NS_OK;
}

NS_IMETHODIMP
nsSaveAsListener::OnStopRequest(nsIChannel* aChannel, nsISupports* aSupport,
                                nsresult status, const PRUnichar* aMsg)
{
    // close down the file stream and release ourself
  if (m_fileSpec)
  {
    m_fileSpec->Flush();
    m_fileSpec->CloseStream();
    m_outputStream = null_nsCOMPtr();
  }
  Release(); // all done kill ourself
  return NS_OK;
}

NS_IMETHODIMP
nsSaveAsListener::OnDataAvailable(nsIChannel* aChannel, 
                                  nsISupports* aSupport,
                                  nsIInputStream* inStream, 
                                  PRUint32 srcOffset,
                                  PRUint32 count)
{
  nsresult rv = NS_OK;
  if (m_dataBuffer && m_outputStream)
  {
    PRUint32 available, readCount, maxReadCount = FOUR_K;
    PRUint32 writeCount;
    rv = inStream->Available(&available);
    while (NS_SUCCEEDED(rv) && available)
    {
      if (maxReadCount > available)
        maxReadCount = available;
      rv = inStream->Read(m_dataBuffer, maxReadCount, &readCount);
      if (NS_SUCCEEDED(rv))
        rv = m_outputStream->Write(m_dataBuffer, readCount, &writeCount);
      available -= readCount;
    }
  }
  return rv;
}
