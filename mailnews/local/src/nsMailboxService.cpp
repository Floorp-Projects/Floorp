/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsNetUtil.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIWebNavigation.h"
#include "prprf.h"
#include "nsEscape.h"
#include "nsIMsgHdr.h"
#include "nsIFileURL.h"

static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);

nsMailboxService::nsMailboxService()
{
    mPrintingOperation = PR_FALSE;
}

nsMailboxService::~nsMailboxService()
{}

NS_IMPL_ISUPPORTS4(nsMailboxService, nsIMailboxService, nsIMsgMessageService, nsIProtocolHandler, nsIMsgMessageFetchPartService)

nsresult nsMailboxService::ParseMailbox(nsIMsgWindow *aMsgWindow, nsFileSpec& aMailboxPath, nsIStreamListener *aMailboxParser, 
										nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
  nsresult rv;
  nsCOMPtr<nsIMailboxUrl> mailboxurl = do_CreateInstance(kCMailboxUrl, &rv);
  if (NS_SUCCEEDED(rv) && mailboxurl)
  {
    nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(mailboxurl);
    // okay now generate the url string
    nsFilePath filePath(aMailboxPath); // convert to file url representation...
    nsCAutoString buf;
    NS_EscapeURL((const char *)filePath,-1,
                     esc_Minimal|esc_Forced|esc_AlwaysCopy,buf);
    url->SetUpdatingFolder(PR_TRUE);
    url->SetMsgWindow(aMsgWindow);
    char *temp = PR_smprintf("mailbox://%s", buf.get());
    url->SetSpec(nsDependentCString(temp));
    PR_Free(temp);
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
  NS_ENSURE_ARG(srcFolder);
  nsCOMPtr<nsIMailboxUrl> mailboxurl;

  nsMailboxAction actionToUse = nsIMailboxUrl::ActionMoveMessage;
  if (!moveMessage)
     actionToUse = nsIMailboxUrl::ActionCopyMessage;

  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  nsCOMPtr <nsIMsgDatabase> db;
  srcFolder->GetMsgDatabase(aMsgWindow, getter_AddRefs(db));
  if (db)
  {
    db->GetMsgHdrForKey(msgKeys->GetAt(0), getter_AddRefs(msgHdr));
    if (msgHdr)
    {
      nsXPIDLCString uri;
      srcFolder->GetUriForMsg(msgHdr, getter_Copies(uri));
      rv = PrepareMessageUrl(uri, aUrlListener, actionToUse , getter_AddRefs(mailboxurl), aMsgWindow);

      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
        nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(url));
        nsCOMPtr<nsIMailboxUrl> mailboxUrl (do_QueryInterface(url));
        msgUrl->SetMsgWindow(aMsgWindow);

        mailboxUrl->SetMoveCopyMsgKeys(msgKeys->GetArray(), msgKeys->GetSize());
        rv = RunMailboxUrl(url, aMailboxCopyHandler); 
      }
    }
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
                                        const char * aCharsetOverride,
                                        nsIURI ** aURL)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMailboxUrl> mailboxurl;
  
  nsMailboxAction actionToUse = mailboxAction;
  
  nsCOMPtr <nsIURI> url;

  nsCAutoString uriString(aMessageURI);

  if (!strncmp(aMessageURI, "file:", 5))
  {
    PRInt64 fileSize;
    nsCOMPtr<nsIURI> fileUri;
    rv = NS_NewURI(getter_AddRefs(fileUri), aMessageURI);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsIFileURL> fileUrl = do_QueryInterface(fileUri, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsIFile> file;
    rv = fileUrl->GetFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
    file->GetFileSize(&fileSize);
    nsCAutoString uriString(aMessageURI);
    uriString.ReplaceSubstring(NS_LITERAL_CSTRING("file:"), NS_LITERAL_CSTRING("mailbox:"));
    uriString.Append(NS_LITERAL_CSTRING("&number=0"));
    rv = NS_NewURI(getter_AddRefs(url), uriString);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgMailNewsUrl> msgurl = do_QueryInterface(url);
    if (msgurl)
    {
      msgurl->SetMsgWindow(aMsgWindow);
      nsCOMPtr <nsIMailboxUrl> mailboxUrl = do_QueryInterface(msgurl, &rv);
      mailboxUrl->SetMessageSize((PRUint32) fileSize);
      nsCOMPtr <nsIMsgHeaderSink> headerSink;
       // need to tell the header sink to capture some headers to create a fake db header
       // so we can do reply to a .eml file or a rfc822 msg attachment.
      if (aMsgWindow)
        aMsgWindow->GetMsgHeaderSink(getter_AddRefs(headerSink));
      if (headerSink)
      {
        nsCOMPtr <nsIMsgDBHdr> dummyHeader;
        headerSink->GetDummyMsgHeader(getter_AddRefs(dummyHeader));
        if (dummyHeader)
          dummyHeader->SetMessageSize((PRUint32) fileSize);
      }
    }
  }
  else
  {

    // this happens with forward inline of message/rfc822 attachment
    // opened in a stand-alone msg window.
    PRInt32 typeIndex = uriString.Find("&type=application/x-message-display");
    if (typeIndex != kNotFound)
    {
      uriString.Cut(typeIndex, sizeof("&type=application/x-message-display") - 1);
      rv = NS_NewURI(getter_AddRefs(url), uriString.get());
      mailboxurl = do_QueryInterface(url);
    }
    else
      rv = PrepareMessageUrl(aMessageURI, aUrlListener, actionToUse , getter_AddRefs(mailboxurl), aMsgWindow);
  
    if (NS_SUCCEEDED(rv))
    {
      url = do_QueryInterface(mailboxurl);
      nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(url));
      msgUrl->SetMsgWindow(aMsgWindow);
      nsCOMPtr<nsIMsgI18NUrl> i18nurl (do_QueryInterface(msgUrl));
      i18nurl->SetCharsetOverRide(aCharsetOverride);
      if (aFileName)
        msgUrl->SetFileName(nsDependentCString(aFileName));
    }
  }
    
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
    rv = docShell->LoadURI(url, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, PR_FALSE);
  }
  else
    rv = RunMailboxUrl(url, aDisplayConsumer); 
 
  if (aURL && mailboxurl)
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
                                               const char * aCharsetOveride,
                                               nsIURI ** aURL)
{
  return FetchMessage(aMessageURI, aDisplayConsumer,
    aMsgWindow,aUrlListener, nsnull,
    nsIMailboxUrl::ActionFetchMessage, aCharsetOveride, aURL);
}

NS_IMETHODIMP
nsMailboxService::StreamMessage(const char *aMessageURI, nsISupports *aConsumer, 
					nsIMsgWindow *aMsgWindow,
					nsIUrlListener *aUrlListener, 
                                        PRBool /* aConvertData */,
                                        const char *aAdditionalHeader,
					nsIURI **aURL)
{
    // The mailbox protocol object will look for "header=filter" or 
    // "header=attach" to decide if it wants to convert the data instead of 
    // using aConvertData. It turns out to be way too hard to pass aConvertData 
    // all the way over to the mailbox protocol object.
    nsCAutoString aURIString(aMessageURI);
    if (aAdditionalHeader)
    {
      aURIString.FindChar('?') == kNotFound ? aURIString += "?" : aURIString += "&";
      aURIString += "header=";
      aURIString += aAdditionalHeader;
    }

    return FetchMessage(aURIString.get(), aConsumer, aMsgWindow, aUrlListener, nsnull, 
                                        nsIMailboxUrl::ActionFetchMessage, nsnull, aURL);
}

NS_IMETHODIMP nsMailboxService::OpenAttachment(const char *aContentType, 
                                               const char *aFileName,
                                               const char *aUrl, 
                                               const char *aMessageUri, 
                                               nsISupports *aDisplayConsumer, 
                                               nsIMsgWindow *aMsgWindow, 
                                               nsIUrlListener *aUrlListener)
{
  nsCOMPtr <nsIURI> URL;
  nsCAutoString urlString(aUrl);
  urlString += "&type=";
  urlString += aContentType;
  urlString += "&filename=";
  urlString += aFileName;
  CreateStartupUrl(urlString.get(), getter_AddRefs(URL));
  nsresult rv;
  // try to run the url in the docshell...
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
  // if we were given a docShell, run the url in the docshell..otherwise just run it normally.
  if (NS_SUCCEEDED(rv) && docShell)
  {
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    // DIRTY LITTLE HACK --> since we are opening an attachment we want the docshell to
    // treat this load as if it were a user click event. Then the dispatching stuff will be much
    // happier.
    docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
    loadInfo->SetLoadType(nsIDocShellLoadInfo::loadLink);
    return docShell->LoadURI(URL, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, PR_FALSE);
  }
  return RunMailboxUrl(URL, aDisplayConsumer); 
  
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
  if (!strncmp(aMessageURI, "file:", 5) || PL_strstr(aMessageURI, "type=application/x-message-display")
    || !strncmp(aMessageURI, "mailbox:", 8))
    return NS_NewURI(aURL, aMessageURI);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMailboxUrl> mailboxurl;
  rv = PrepareMessageUrl(aMessageURI, nsnull, nsIMailboxUrl::ActionFetchMessage, getter_AddRefs(mailboxurl), aMsgWindow);
  if (NS_SUCCEEDED(rv) && mailboxurl)
    rv = mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);
  return rv;
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

// This function takes a message uri, converts it into a file path & msgKey 
// pair. It then turns that into a mailbox url object. It also registers a url
// listener if appropriate. AND it can take in a mailbox action and set that field
// on the returned url as well.
nsresult nsMailboxService::PrepareMessageUrl(const char * aSrcMsgMailboxURI, nsIUrlListener * aUrlListener,
                                             nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl,
                                             nsIMsgWindow *msgWindow)
{
  nsresult rv = CallCreateInstance(kCMailboxUrl, aMailboxUrl);
  if (NS_SUCCEEDED(rv) && aMailboxUrl && *aMailboxUrl)
  {
    // okay now generate the url string
    char * urlSpec;
    nsCAutoString folderURI;
    nsFileSpec folderPath;
    nsMsgKey msgKey;
    const char *part = PL_strstr(aSrcMsgMailboxURI, "part=");
    const char *header = PL_strstr(aSrcMsgMailboxURI, "header=");
    rv = nsParseLocalMessageURI(aSrcMsgMailboxURI, folderURI, &msgKey);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = nsLocalURI2Path(kMailboxRootURI, folderURI.get(), folderPath);
    
    if (NS_SUCCEEDED(rv))
    {
      // set up the url spec and initialize the url with it.
      nsFilePath filePath(folderPath); // convert to file url representation...
      nsCAutoString buf;
      NS_EscapeURL((const char *)filePath,-1,
                   esc_Directory|esc_Forced|esc_AlwaysCopy,buf);
      if (mPrintingOperation)
        urlSpec = PR_smprintf("mailbox://%s?number=%d&header=print", buf.get(), msgKey);
      else if (part)
        urlSpec = PR_smprintf("mailbox://%s?number=%d&%s", buf.get(), msgKey, part);
      else if (header)
        urlSpec = PR_smprintf("mailbox://%s?number=%d&%s", buf.get(), msgKey, header);
      else
        urlSpec = PR_smprintf("mailbox://%s?number=%d", buf.get(), msgKey);
      
      nsCOMPtr <nsIMsgMailNewsUrl> url = do_QueryInterface(*aMailboxUrl);
      url->SetSpec(nsDependentCString(urlSpec));
      PR_Free(urlSpec);
      
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

NS_IMETHODIMP nsMailboxService::GetScheme(nsACString &aScheme)
{
	aScheme = "mailbox";
	return NS_OK;
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
    *result = URI_STD | URI_FORBIDS_AUTOMATIC_DOCUMENT_REPLACEMENT |
        URI_DANGEROUS_TO_LOAD;
    return NS_OK; 	
}

NS_IMETHODIMP nsMailboxService::NewURI(const nsACString &aSpec,
                                       const char *aOriginCharset,
                                       nsIURI *aBaseURI,
                                       nsIURI **_retval)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> aMsgUri = do_CreateInstance(kCMailboxUrl, &rv);
      
  if (NS_SUCCEEDED(rv))
  {
    if (aBaseURI) 
    {
      nsCAutoString newSpec;
      rv = aBaseURI->Resolve(aSpec, newSpec);
      if (NS_FAILED(rv))
        return rv;
      aMsgUri->SetSpec(newSpec);
    } 
    else 
    {
      aMsgUri->SetSpec(aSpec);
    }
    NS_ADDREF(*_retval = aMsgUri);
  }

  return rv;
}

NS_IMETHODIMP nsMailboxService::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsresult rv = NS_OK;
  nsCAutoString spec;
  aURI->GetSpec(spec);

  nsACString::const_iterator b, e;
  if (FindInReadable(NS_LITERAL_CSTRING("?uidl="), spec.BeginReading(b), spec.EndReading(e)) ||
      FindInReadable(NS_LITERAL_CSTRING("&uidl="), spec.BeginReading(b), spec.EndReading(e)))
  {
    nsCOMPtr<nsIProtocolHandler> handler = 
             do_GetService(kCPop3ServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr <nsIURI> pop3Uri;

      rv = handler->NewURI(spec, "" /* ignored */, aURI, getter_AddRefs(pop3Uri));
      NS_ENSURE_SUCCESS(rv, rv);
      return handler->NewChannel(pop3Uri, _retval);
    }
  }
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
  rv = rdf->GetResource(folderURI, getter_AddRefs(res));
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
