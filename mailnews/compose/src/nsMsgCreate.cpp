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
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIMimeStreamConverter.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "nsICharsetConverterManager.h"
#include "prprf.h"
#include "nsMsgCreate.h" 
#include "nsMsgCompUtils.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsIEnumerator.h"
#include "nsIMessage.h"
#include "nsIRDFResource.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCopy.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIIOService.h"
#include "nsMsgMimeCID.h"

// CID's needed
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kPrefCID,            NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID,      NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

//
// Implementation...
//
nsMsgDraft::nsMsgDraft()
{
	NS_INIT_REFCNT();

  mURI = nsnull;
  mMessageService = nsnull;
  mOutType = nsMimeOutput::nsMimeMessageDraftOrTemplate;
  mAddInlineHeaders = PR_FALSE;
}

nsMsgDraft::~nsMsgDraft()
{
  if (mMessageService)
  {
    ReleaseMessageServiceFromURI(mURI, mMessageService);
    mMessageService = nsnull;
  }

  PR_FREEIF(mURI);
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgDraft, nsCOMTypeInfo<nsIMsgDraft>::GetIID());

/* this function will be used by the factory to generate an Message Compose Fields Object....*/
nsresult 
NS_NewMsgDraft(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgDraft *pQuote = new nsMsgDraft();
		if (pQuote)
			return pQuote->QueryInterface(aIID, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

// stream converter
static NS_DEFINE_CID(kStreamConverterCID,    NS_MAILNEWS_MIME_STREAM_CONVERTER_CID);

nsIMessage *
GetIMessageFromURI(const PRUnichar *msgURI)
{
  nsresult                  rv;
  nsIRDFResource            *myRDFNode = nsnull;
  nsCAutoString              convertString(msgURI);

  nsIMessage                *returnMessage;

  NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
	if (NS_FAILED(rv) || (!rdfService))
		return nsnull;

  rdfService->GetResource(convertString, &myRDFNode);
  if (!myRDFNode)
    return nsnull;

  myRDFNode->QueryInterface(nsCOMTypeInfo<nsIMessage>::GetIID(), (void **)&returnMessage);
  NS_IF_RELEASE(myRDFNode);
  return returnMessage;
}

nsresult    
nsMsgDraft::ProcessDraftOrTemplateOperation(const PRUnichar *msgURI, nsMimeOutputType aOutType, 
                                            nsIMessage **aMsgToReplace)
{
nsresult  rv;

  mOutType = aOutType;

  if (!msgURI)
    return NS_ERROR_INVALID_ARG;

  nsString        convertString(msgURI);
  mURI = convertString.ToNewCString();

  if (!mURI)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = GetMessageServiceFromURI(mURI, &mMessageService);
  if (NS_FAILED(rv) && !mMessageService)
  {
    return rv;
  }

  NS_ADDREF(this);

  // Now, we can create a mime parser (nsIStreamConverter)!
  nsCOMPtr<nsIStreamConverter> mimeParser;
  rv = nsComponentManager::CreateInstance(kStreamConverterCID, 
                                          NULL, nsCOMTypeInfo<nsIStreamConverter>::GetIID(), 
                                          (void **) getter_AddRefs(mimeParser)); 
  if (NS_FAILED(rv) || !mimeParser)
  {
    Release();
    ReleaseMessageServiceFromURI(mURI, mMessageService);
    mMessageService = nsnull;
#ifdef NS_DEBUG
    printf("Failed to create MIME stream converter...\n");
#endif
    return rv;
  }
  
  // Set us as the output stream for HTML data from libmime...
  nsCOMPtr<nsIMimeStreamConverter> mimeConverter = do_QueryInterface(mimeParser);
  if (mimeConverter)
  {
	    mimeConverter->SetMimeOutputType(mOutType);  // Set the type of output for libmime
      mimeConverter->SetForwardInline(mAddInlineHeaders);
  }

  nsCOMPtr<nsIStreamListener> convertedListener = do_QueryInterface(mimeParser);
  if (!convertedListener)
  {
    Release();
    ReleaseMessageServiceFromURI(mURI, mMessageService);
    mMessageService = nsnull;
#ifdef NS_DEBUG
    printf("Unable to get the nsIStreamListener interface from libmime\n");
#endif
    return NS_ERROR_UNEXPECTED;
  }  

  nsCOMPtr<nsIChannel> dummyChannel;
  NS_WITH_SERVICE(nsIIOService, netService, kIOServiceCID, &rv);

  nsCOMPtr<nsIURI> aURL;
  rv = CreateStartupUrl(mURI, getter_AddRefs(aURL));

  rv = netService->NewInputStreamChannel(aURL, 
                                         nsnull,      // contentType
                                         -1,          // contentLength
                                         nsnull,      // inputStream
                                         nsnull,      // loadGroup
                                         nsnull,      // originalURI
                                         getter_AddRefs(dummyChannel));
  if (NS_FAILED(mimeParser->AsyncConvertData(nsnull, nsnull, nsnull, dummyChannel)))
  {
    Release();
    ReleaseMessageServiceFromURI(mURI, mMessageService);
    mMessageService = nsnull;
#ifdef NS_DEBUG
    printf("Unable to set the output stream for the mime parser...\ncould be failure to create internal libmime data\n");
#endif

    return NS_ERROR_UNEXPECTED;
  }

  // Make sure we return this if requested!
  if (aMsgToReplace)
    *aMsgToReplace = GetIMessageFromURI(msgURI);

  // Now, just plug the two together and get the hell out of the way!
  rv = mMessageService->DisplayMessage(mURI, convertedListener, nsnull, nsnull);

  ReleaseMessageServiceFromURI(mURI, mMessageService);
  mMessageService = nsnull;
  Release();

	if (NS_FAILED(rv))
    return rv;    
  else
    return NS_OK;
}

nsresult
nsMsgDraft::OpenDraftMsg(const PRUnichar *msgURI, nsIMessage **aMsgToReplace,
                         PRBool addInlineHeaders)
{
PRUnichar     *HackUpAURIToPlayWith(void);      // RICHIE - forward declare for now

  if (!msgURI)
  {
    printf("RICHIE: DO THIS UNTIL THE FE CAN REALLY SUPPORT US!\n");
    msgURI = HackUpAURIToPlayWith();
  }
  
  mAddInlineHeaders = addInlineHeaders;
  return ProcessDraftOrTemplateOperation(msgURI, nsMimeOutput::nsMimeMessageDraftOrTemplate, 
                                         aMsgToReplace);
}

nsresult
nsMsgDraft::OpenEditorTemplate(const PRUnichar *msgURI, nsIMessage **aMsgToReplace)
{
  return ProcessDraftOrTemplateOperation(msgURI, nsMimeOutput::nsMimeMessageEditorTemplate, 
                                         aMsgToReplace);
}

//////////////////////////////////////////////////////////////////////////////////////////
// RICHIE - This is a temp routine to get a URI from the Drafts folder and use that
// for loading into the compose window.
//////////////////////////////////////////////////////////////////////////////////////////
// RICHIE - EVERYTHING AFTER THIS COMMENT IS A TEMP HACK!!!
//////////////////////////////////////////////////////////////////////////////////////////
PRUnichar *
HackUpAURIToPlayWith(void)
{
  //PRUnichar           *playURI = nsnull;
  char                *folderURI = nsnull;
  nsresult            rv = NS_OK;

  // temporary hack to get the current identity
  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  nsCOMPtr<nsIMsgIdentity> identity = nsnull;
  rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));
  if (NS_FAILED(rv)) 
    return nsnull;

  // 
  // Find the users drafts folder...
  //
  identity->GetDraftFolder(&folderURI);

  // 
  // Now, get the drafts folder...
  //
  nsCOMPtr <nsIMsgFolder> folder;
  rv = LocateMessageFolder(identity, nsMsgSaveAsDraft, folderURI, getter_AddRefs(folder));
  PR_FREEIF(folderURI);
  if (NS_FAILED(rv) || !folder)
    return nsnull;

  nsCOMPtr <nsISimpleEnumerator> enumerator;
  rv = folder->GetMessages(getter_AddRefs(enumerator));
  if (NS_FAILED(rv) || (!enumerator))
  {
    // RICHIE - Possible bug that will bite us in this hack...
    printf("*** NOTICE *** If you failed, more than likely, this is the problem\ndescribed by Bug #10344.\n\7");    
    return nsnull;
  }

  PRBool hasMore = PR_FALSE;
  rv = enumerator->HasMoreElements(&hasMore);

  if (!NS_SUCCEEDED(rv) || !hasMore) 
    return nsnull;

  nsCOMPtr<nsISupports>   currentItem;
  rv = enumerator->GetNext(getter_AddRefs(currentItem));
  if (NS_FAILED(rv))
    return nsnull;

  nsCOMPtr<nsIMessage>      message;
  message = do_QueryInterface(currentItem); 
  if (!message)
    return nsnull;

  nsCOMPtr<nsIRDFResource>  myRDFNode;
  myRDFNode = do_QueryInterface(message, &rv);
  if (NS_FAILED(rv) || (!myRDFNode))
    return nsnull;

  char    *tURI;
  myRDFNode->GetValue(&tURI);
  nsString   workURI(tURI);
  nsAllocator::Free(tURI);
  return workURI.ToNewUnicode(); 
}
