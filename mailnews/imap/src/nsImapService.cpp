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
#include "nsMsgImapCID.h"

#include "netCore.h"

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
#include "nsReadableUtils.h"
#include "nsRDFCID.h"
#include "nsEscape.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsILoadGroup.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgFolderFlags.h"
#include "nsISubscribableServer.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWebNavigation.h"
#include "nsImapStringBundle.h"
#include "plbase64.h"
#include "nsImapOfflineSync.h"
#include "nsIMsgHdr.h"
#include "nsMsgUtils.h"
#include "nsICacheService.h"
#include "nsIStreamListenerTee.h"
#include "nsNetCID.h"
#include "nsMsgI18N.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsICopyMsgStreamListener.h"
#include "nsIFileStream.h"
#include "nsIMsgParseMailMsgState.h"
#include "nsMsgLineBuffer.h"
#include "nsMsgLocalCID.h"
#include "nsIOutputStream.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDOMWindowInternal.h"
#include "nsIMessengerWindowService.h"
#include "nsIWindowMediator.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsImapProtocol.h"
#include "nsIMsgMailSession.h"
#include "nsIStreamConverterService.h"
#include "nsInt64.h"

#define PREF_MAIL_ROOT_IMAP "mail.root.imap"            // old - for backward compatibility only
#define PREF_MAIL_ROOT_IMAP_REL "mail.root.imap-rel"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);


static const char sequenceString[] = "SEQUENCE";
static const char uidString[] = "UID";

static PRBool gInitialized = PR_FALSE;
static PRInt32 gMIMEOnDemandThreshold = 15000;
static PRBool gMIMEOnDemand = PR_FALSE;

NS_IMPL_THREADSAFE_ADDREF(nsImapService)
NS_IMPL_THREADSAFE_RELEASE(nsImapService)
NS_IMPL_QUERY_INTERFACE6(nsImapService,
                         nsIImapService,
                         nsIMsgMessageService,
                         nsIProtocolHandler,
                         nsIMsgProtocolInfo,
                         nsIMsgMessageFetchPartService,
                         nsIContentHandler)

nsImapService::nsImapService()
{
  mPrintingOperation = PR_FALSE;
  if (!gInitialized)
  {
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
    if (NS_SUCCEEDED(rv) && prefBranch) 
    {
      prefBranch->GetBoolPref("mail.imap.mime_parts_on_demand", &gMIMEOnDemand);
      prefBranch->GetIntPref("mail.imap.mime_parts_on_demand_threshold", &gMIMEOnDemandThreshold);
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
    nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aMsgFolder);
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
  if (onlineName.IsEmpty())
  {
    char *uri = nsnull;
    rv = aImapFolder->GetURI(&uri);
    if (NS_FAILED(rv)) return rv;
    char * hostname = nsnull;
    rv = aImapFolder->GetHostname(&hostname);
    if (NS_FAILED(rv)) return rv;
    rv = nsImapURI2FullName(kImapRootURI, hostname, uri, getter_Copies(onlineName));
    PR_Free(uri);
    PR_Free(hostname);
  }
  // if the hierarchy delimiter is not '/', then we want to escape slashes;
  // otherwise, we do want to escape slashes.
  // we want to escape slashes and '^' first, otherwise, nsEscape will lose them
  PRBool escapeSlashes = (GetHierarchyDelimiter(aImapFolder) != (PRUnichar) '/');
  if (escapeSlashes && (const char *) onlineName)
  {
    char* escapedOnlineName;
    rv = nsImapUrl::EscapeSlashes((const char *) onlineName, &escapedOnlineName);
    if (NS_SUCCEEDED(rv))
      onlineName.Adopt(escapedOnlineName);
  }
  // need to escape everything else
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
  NS_ASSERTION (aImapMailFolder && aClientEventQueue,
    "Oops ... null pointer");
  if (!aImapMailFolder || !aClientEventQueue)
    return NS_ERROR_NULL_POINTER;
  
  if (WeAreOffline())
    return NS_MSG_ERROR_OFFLINE;
  
  PRBool canOpenThisFolder = PR_TRUE;
  nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aImapMailFolder);
  if (imapFolder)
    imapFolder->GetCanIOpenThisFolder(&canOpenThisFolder);
  
  if (!canOpenThisFolder) 
    return NS_OK;
  
  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCAutoString urlSpec;
  nsresult rv;
  PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aImapMailFolder, aUrlListener, urlSpec, hierarchySeparator);
  
  if (NS_SUCCEEDED(rv) && imapUrl)
  {
    // nsImapUrl::SetSpec() will set the imap action properly
    rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);
    
    nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
    // if no msg window, we won't put up error messages (this is almost certainly a biff-inspired get new msgs)
    if (!aMsgWindow)
      mailNewsUrl->SetSuppressErrorMsgs(PR_TRUE);
    mailNewsUrl->SetMsgWindow(aMsgWindow);
    mailNewsUrl->SetUpdatingFolder(PR_TRUE);
    imapUrl->AddChannelToLoadGroup();
    rv = SetImapUrlSink(aImapMailFolder, imapUrl);
    
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLCString folderName;
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append("/select>");
      urlSpec.Append(char(hierarchySeparator));
      urlSpec.Append((const char *) folderName);
      rv = mailNewsUrl->SetSpec(urlSpec);
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
      urlSpec.Append(char (hierarchySeparator));
      
      nsXPIDLCString folderName;
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      rv = uri->SetSpec(urlSpec);
      if (NS_SUCCEEDED(rv))
        rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl, nsnull, aURL);
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
    nsCOMPtr<nsIImapUrl> imapUrl;
    nsCAutoString urlSpec;
    PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
    rv = CreateStartOfImapUrl(aMessageURI, getter_AddRefs(imapUrl), folder, nsnull, urlSpec, hierarchySeparator);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetImapUrlSink(folder, imapUrl);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(imapUrl);
    PRBool useLocalCache = PR_FALSE;
    folder->HasMsgOffline(atoi(msgKey), &useLocalCache);
    mailnewsUrl->SetMsgIsInLocalCache(useLocalCache);

    nsCOMPtr<nsIURI> url = do_QueryInterface(imapUrl);
    url->GetSpec(urlSpec);
    urlSpec.Append("fetch>UID>");
    urlSpec.Append(char(hierarchySeparator));

    nsXPIDLCString folderName;
    GetFolderName(folder, getter_Copies(folderName));
    urlSpec.Append((const char *) folderName);
    urlSpec.Append(">");
    urlSpec.Append(msgKey);
    rv = url->SetSpec(urlSpec);
    imapUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);
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
    uri += "&filename=";
    uri += aFileName;
  }
  else
  {
    // try to extract the specific part number out from the url string
    uri += "?";
    const char *part = PL_strstr(aUrl, "part=");
    uri += part;
    uri += "&type=";
    uri += aContentType;
    uri += "&filename=";
    uri += aFileName;
  }
  
  nsCOMPtr<nsIMsgFolder> folder;
  nsXPIDLCString msgKey;
  nsXPIDLCString uriMimePart;
  nsCAutoString	folderURI;
  nsMsgKey key;
  
  rv = DecomposeImapURI(uri.get(), getter_AddRefs(folder), getter_Copies(msgKey));
  rv = nsParseImapMessageURI(uri.get(), folderURI, &key, getter_Copies(uriMimePart));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(folder, &rv));
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIImapUrl> imapUrl;
      nsCAutoString urlSpec;
      PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
      rv = CreateStartOfImapUrl(uri.get(), getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec, hierarchySeparator);
      if (NS_FAILED(rv)) 
        return rv;
    
      urlSpec.Append("/fetch>UID>");
      urlSpec.Append(char(hierarchySeparator));
    
      nsXPIDLCString folderName;
      GetFolderName(folder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      urlSpec.Append(">");
      urlSpec.Append(msgKey.get());
      urlSpec.Append(uriMimePart.get());
      
      if (uriMimePart)
      {
        nsCOMPtr<nsIMsgMailNewsUrl> mailUrl (do_QueryInterface(imapUrl));
        if (mailUrl)
        {
          mailUrl->SetSpec(urlSpec);
          mailUrl->SetFileName(nsDependentCString(aFileName));
        }
        rv =  FetchMimePart(imapUrl, nsIImapUrl::nsImapOpenMimePart, folder, imapMessageSink,
          nsnull, aDisplayConsumer, msgKey, uriMimePart);
      }
    } // if we got a message sink
  } // if we parsed the message uri
  
  return rv;
}

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
                                            const char * aCharsetOverride,
                                            nsIURI ** aURL)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgFolder> folder;
  nsXPIDLCString msgKey;
  nsXPIDLCString mimePart;
  nsCAutoString	folderURI;
  nsMsgKey key;
  
  rv = DecomposeImapURI(aMessageURI, getter_AddRefs(folder), getter_Copies(msgKey));
  if (msgKey.IsEmpty())
    return NS_MSG_MESSAGE_NOT_FOUND;
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
      PRBool shouldStoreMsgOffline = PR_FALSE;
      PRBool hasMsgOffline = PR_FALSE;
      
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
      
      nsCAutoString uriStr(aMessageURI);
      PRInt32 keySeparator = uriStr.RFindChar('#');
      if(keySeparator != -1)
      {
        PRInt32 keyEndSeparator = uriStr.FindCharInSet("/?&", 
          keySeparator); 
        PRInt32 mpodFetchPos = uriStr.Find("fetchCompleteMessage=true", PR_FALSE, keyEndSeparator);
        if (mpodFetchPos != -1)
          useMimePartsOnDemand = PR_FALSE;
      }
      
      if (folder)
      {
        folder->ShouldStoreMsgOffline(key, &shouldStoreMsgOffline);
        folder->HasMsgOffline(key, &hasMsgOffline);
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
        // if we happen to fetch the whole message, note in the url
        // whether we want to store this message offline.
        imapUrl->SetShouldStoreMsgOffline(shouldStoreMsgOffline);
        shouldStoreMsgOffline = PR_FALSE; // if we're fetching by parts, don't store offline
        msgurl->SetAddToMemoryCache(PR_FALSE);
      }
      if (imapMessageSink)
        imapMessageSink->SetNotifyDownloadedLines(shouldStoreMsgOffline);
      
      if (hasMsgOffline)
        msgurl->SetMsgIsInLocalCache(PR_TRUE);
      
      nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
      PRBool forcePeek = PR_FALSE; // should the message fetch force a peak or a traditional fetch? 

      if (NS_SUCCEEDED(rv) && prefBranch) 
         prefBranch->GetBoolPref("mailnews.mark_message_read.delay", &forcePeek);
      
      rv = FetchMessage(imapUrl, forcePeek ? nsIImapUrl::nsImapMsgFetchPeek : nsIImapUrl::nsImapMsgFetch, folder, imapMessageSink,
        aMsgWindow, aDisplayConsumer, msgKey, PR_FALSE, (mPrintingOperation) ? "print" : nsnull, aURL);
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
  
  nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(aImapUrl));
  if (aImapMailFolder && msgurl && messageIdentifierList)
  {
    PRBool useLocalCache = PR_FALSE;
    aImapMailFolder->HasMsgOffline(atoi(messageIdentifierList), &useLocalCache);  
    msgurl->SetMsgIsInLocalCache(useLocalCache);
  }
  rv = aImapUrl->SetImapMessageSink(aImapMessage);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl);
    url->GetSpec(urlSpec);
    
    // rhp: If we are displaying this message for the purpose of printing, we
    // need to append the header=print option.
    //
    if (mPrintingOperation)
      urlSpec.Append("?header=print");
    
    // mscott - this cast to a char * is okay...there's a bug in the XPIDL
    // compiler that is preventing in string parameters from showing up as
    // const char *. hopefully they will fix it soon.
    rv = url->SetSpec(urlSpec);
    
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
          aImapServer->PseudoInterruptMsgLoad(aImapMailFolder, nsnull, &interrupted);
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
      
      rv = docShell->LoadURI(url, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, PR_FALSE);
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
        rv = aChannel->AsyncOpen(aStreamListener, aCtxt);
      }
      else // do what we used to do before
      {
        // I'd like to get rid of this code as I believe that we always get a docshell
        // or stream listener passed into us in this method but i'm not sure yet...
        // I'm going to use an assert for now to figure out if this is ever getting called
#if defined(DEBUG_mscott) || defined(DEBUG_bienvenu)
        NS_ASSERTION(0, "oops...someone still is reaching this part of the code");
#endif
        nsCOMPtr<nsIEventQueue> queue;	
        // get the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> pEventQService = 
          do_GetService(kEventQueueServiceCID, &rv);
        
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
      PRBool hasMsgOffline = PR_FALSE;
      nsMsgKey key = atoi(msgKey);
      
      rv = CreateStartOfImapUrl(aSrcMailboxURI, getter_AddRefs(imapUrl), folder, aUrlListener, urlSpec, hierarchySeparator);
      
      if (folder)
      {
        nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(imapUrl));
        folder->HasMsgOffline(key, &hasMsgOffline);
        if (msgurl)
          msgurl->SetMsgIsInLocalCache(hasMsgOffline);
      }
      // now try to download the message
      nsImapAction imapAction = nsIImapUrl::nsImapOnlineToOfflineCopy;
      if (moveMessage)
        imapAction = nsIImapUrl::nsImapOnlineToOfflineMove; 
      rv = FetchMessage(imapUrl,imapAction, folder, imapMessageSink,aMsgWindow, streamSupport, msgKey, PR_FALSE, nsnull, aURL);
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
      PRUint32 numKeys = keys->GetSize();
      AllocateImapUidString(keys->GetArray(), numKeys, nsnull, messageIds);
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
        aMsgWindow, streamSupport, messageIds.get(), PR_FALSE, nsnull, aURL);
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
    if (!aMsgWindow)
      mailNewsUrl->SetSuppressErrorMsgs(PR_TRUE);

    urlSpec.Append("/search>UID>");
    urlSpec.Append(char(hierarchySeparator));
    urlSpec.Append((const char *) folderName);
    urlSpec.Append('>');
    // escape aSearchUri so that IMAP special characters (i.e. '\')
    // won't be replaced with '/' in NECKO.
    // it will be unescaped in nsImapUrl::ParseUrl().
    char *search_cmd = nsEscape((char *)aSearchUri, url_XAlphas);
    urlSpec.Append(search_cmd);
    nsCRT::free(search_cmd);
    rv = mailNewsUrl->SetSpec(urlSpec);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIEventQueue> queue;	
      // get the Event Queue for this thread...
      nsCOMPtr<nsIEventQueueService> pEventQService = 
          do_GetService(kEventQueueServiceCID, &rv);

      if (NS_FAILED(rv)) return rv;

      rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
      if (NS_FAILED(rv)) return rv;
        rv = GetImapConnectionAndLoadUrl(queue, imapUrl, nsnull, nsnull);
    }
  }
  return rv;
}

// just a helper method to break down imap message URIs....
nsresult nsImapService::DecomposeImapURI(const char * aMessageURI, nsIMsgFolder ** aFolder, nsMsgKey *aMsgKey)
{
    NS_ENSURE_ARG_POINTER(aMessageURI);
    NS_ENSURE_ARG_POINTER(aFolder);
    NS_ENSURE_ARG_POINTER(aMsgKey);

    nsresult rv = NS_OK;
    nsCAutoString folderURI;
    rv = nsParseImapMessageURI(aMessageURI, folderURI, aMsgKey, nsnull);
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

// just a helper method to break down imap message URIs....
nsresult nsImapService::DecomposeImapURI(const char * aMessageURI, nsIMsgFolder ** aFolder, char ** aMsgKey)
{
    nsMsgKey msgKey;
    nsresult rv;
    rv = DecomposeImapURI(aMessageURI, aFolder, &msgKey);
    NS_ENSURE_SUCCESS(rv,rv);

    if (msgKey) {
      nsCAutoString messageIdString;
      messageIdString.AppendInt(msgKey);
      *aMsgKey = ToNewCString(messageIdString);
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
    
    PRBool hasMsgOffline = PR_FALSE;

    if (folder)
      folder->HasMsgOffline(atoi(msgKey), &hasMsgOffline);

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

        nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(msgUrl);
        if (mailnewsUrl)
          mailnewsUrl->SetMsgIsInLocalCache(hasMsgOffline);

        nsCOMPtr <nsIStreamListener> saveAsListener;
        mailnewsUrl->GetSaveAsListener(aAddDummyEnvelope, aFile, getter_AddRefs(saveAsListener));
        
        return FetchMessage(imapUrl, nsIImapUrl::nsImapSaveMessageToDisk, folder, imapMessageSink, aMsgWindow, saveAsListener, msgKey, PR_FALSE, nsnull, aURL);
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
                            nsISupports * aDisplayConsumer, 
                            const char *messageIdentifierList,
                            PRBool aConvertDataToText,
                            const char *aAdditionalHeader,
                            nsIURI ** aURL)
{
  // create a protocol instance to handle the request.
  NS_ASSERTION (aImapUrl && aImapMailFolder &&  aImapMessage,"Oops ... null pointer");
  if (!aImapUrl || !aImapMailFolder || !aImapMessage)
      return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl);
  if (WeAreOffline())
  {
    nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(aImapUrl));
    if (msgurl)
    {
      PRBool msgIsInLocalCache = PR_FALSE;
      msgurl->GetMsgIsInLocalCache(&msgIsInLocalCache);
      if (!msgIsInLocalCache)
      {
        nsCOMPtr<nsIMsgIncomingServer> server;

        rv = aImapMailFolder->GetServer(getter_AddRefs(server));
        if (server && aDisplayConsumer)
          rv = server->DisplayOfflineMsg(aMsgWindow);
        return rv;
      }
    }
  }

  if (aURL)
  {
    *aURL = url;
    NS_IF_ADDREF(*aURL);
  }
  nsCAutoString urlSpec;
  rv = SetImapUrlSink(aImapMailFolder, aImapUrl);

  rv = aImapUrl->SetImapMessageSink(aImapMessage);
  url->GetSpec(urlSpec);

  PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder); 

  urlSpec.Append("fetch>UID>");
  urlSpec.Append(char(hierarchySeparator));

  nsXPIDLCString folderName;
  GetFolderName(aImapMailFolder, getter_Copies(folderName));
  urlSpec.Append((const char *) folderName);
  urlSpec.Append(">");
  urlSpec.Append(messageIdentifierList);

  if (aAdditionalHeader)
  {
    urlSpec.Append("?header=");
    urlSpec.Append(aAdditionalHeader);
  }


  // mscott - this cast to a char * is okay...there's a bug in the XPIDL
  // compiler that is preventing in string parameters from showing up as
  // const char *. hopefully they will fix it soon.
  rv = url->SetSpec(urlSpec);

  rv = aImapUrl->SetImapAction(aImapAction);
  // if the display consumer is a docshell, then we should run the url in the docshell.
  // otherwise, it should be a stream listener....so open a channel using AsyncRead
  // and the provided stream listener....

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
  if (aImapMailFolder && docShell)
  {
    nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;
    rv = aImapMailFolder->GetServer(getter_AddRefs(aMsgIncomingServer));
    if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
    {
      PRBool interrupted;
      nsCOMPtr<nsIImapIncomingServer>
        aImapServer(do_QueryInterface(aMsgIncomingServer, &rv));
      if (NS_SUCCEEDED(rv) && aImapServer)
        aImapServer->PseudoInterruptMsgLoad(aImapMailFolder, aMsgWindow, &interrupted);
    }
  }
  if (NS_SUCCEEDED(rv) && docShell)
  {
    NS_ASSERTION(!aConvertDataToText, "can't convert to text when using docshell");
    rv = docShell->LoadURI(url, nsnull, nsIWebNavigation::LOAD_FLAGS_NONE, PR_FALSE);
  }
  else
  {
    nsCOMPtr<nsIStreamListener> streamListener = do_QueryInterface(aDisplayConsumer, &rv);
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl, &rv);
    if (aMsgWindow && mailnewsUrl)
      mailnewsUrl->SetMsgWindow(aMsgWindow);
    if (NS_SUCCEEDED(rv) && streamListener)
    {
      nsCOMPtr<nsIChannel> channel;
      nsCOMPtr<nsILoadGroup> loadGroup;
      if (NS_SUCCEEDED(rv) && mailnewsUrl)
        mailnewsUrl->GetLoadGroup(getter_AddRefs(loadGroup));

      rv = NewChannel(url, getter_AddRefs(channel));
      if (NS_FAILED(rv)) return rv;

      rv = channel->SetLoadGroup(loadGroup);
      if (NS_FAILED(rv)) return rv;

      if (aConvertDataToText)
      {
        nsCOMPtr<nsIStreamListener> conversionListener;
        nsCOMPtr<nsIStreamConverterService> streamConverter = do_GetService("@mozilla.org/streamConverters;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = streamConverter->AsyncConvertData("message/rfc822",
                                               "*/*",
                                               streamListener, channel, getter_AddRefs(conversionListener));
        NS_ENSURE_SUCCESS(rv, rv);
        streamListener = conversionListener; // this is our new listener.
      }
      nsCOMPtr<nsISupports> aCtxt = do_QueryInterface(url);
      //  now try to open the channel passing in our display consumer as the listener 
      rv = channel->AsyncOpen(streamListener, aCtxt);
    }
    else // do what we used to do before
    {
      // I'd like to get rid of this code as I believe that we always get a docshell
      // or stream listener passed into us in this method but i'm not sure yet...
      // I'm going to use an assert for now to figure out if this is ever getting called
#if defined(DEBUG_mscott) || defined(DEBUG_bienvenu)
      NS_ASSERTION(0, "oops...someone still is reaching this part of the code");
#endif
      nsCOMPtr<nsIEventQueue> queue;	
      // get the Event Queue for this thread...
	    nsCOMPtr<nsIEventQueueService> pEventQService = 
	             do_GetService(kEventQueueServiceCID, &rv);

      if (NS_FAILED(rv)) return rv;

      rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
      if (NS_FAILED(rv)) return rv;
      rv = GetImapConnectionAndLoadUrl(queue, aImapUrl, aDisplayConsumer, aURL);
    }
  }
  return rv;
}

// this method streams a message to the passed in consumer, with an optional stream converter
// and additional header (e.g., "header=filter")
NS_IMETHODIMP
nsImapService::StreamMessage(const char *aMessageURI, nsISupports *aConsumer, 
					nsIMsgWindow *aMsgWindow,
					nsIUrlListener *aUrlListener, 
                                        PRBool aConvertData,
                                        const char *aAdditionalHeader,
					nsIURI **aURL)
{
  nsCOMPtr<nsIMsgFolder> folder;
  nsXPIDLCString msgKey;
  nsXPIDLCString mimePart;
  nsCAutoString	folderURI;
  nsMsgKey key;
  
  nsresult rv = DecomposeImapURI(aMessageURI, getter_AddRefs(folder), getter_Copies(msgKey));
  if (msgKey.IsEmpty())
    return NS_MSG_MESSAGE_NOT_FOUND;
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
      nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(imapUrl));
      
      PRBool shouldStoreMsgOffline = PR_FALSE;
      PRBool hasMsgOffline = PR_FALSE;
      
      nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;
      
      msgurl->SetMsgWindow(aMsgWindow);
      
      rv = msgurl->GetServer(getter_AddRefs(aMsgIncomingServer));
      
      if (folder)
      {
        folder->ShouldStoreMsgOffline(key, &shouldStoreMsgOffline);
        folder->HasMsgOffline(key, &hasMsgOffline);
      }
      
      imapUrl->SetFetchPartsOnDemand(PR_FALSE);
      msgurl->SetAddToMemoryCache(PR_TRUE);
      if (imapMessageSink)
        imapMessageSink->SetNotifyDownloadedLines(shouldStoreMsgOffline);
      
      if (hasMsgOffline)
        msgurl->SetMsgIsInLocalCache(PR_TRUE);
      
      rv = FetchMessage(imapUrl, nsIImapUrl::nsImapMsgFetchPeek, folder, imapMessageSink,
        aMsgWindow, aConsumer, msgKey, aConvertData, aAdditionalHeader, aURL);
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
    PR_Free(hostname);
    return rv;
  }
  
  if (((const char*)username) && username[0])
    *((char **)getter_Copies(escapedUsername)) = nsEscape(username, url_XAlphas);
  
  PRInt32 port = IMAP_PORT;
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = aImapMailFolder->GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv)) 
  {
    server->GetPort(&port);
    if (port == -1 || port == 0) port = IMAP_PORT;
  }
  
  // now we need to create an imap url to load into the connection. The url
  // needs to represent a select folder action. 
  rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
    NS_GET_IID(nsIImapUrl), (void **) imapUrl);
  if (NS_SUCCEEDED(rv) && *imapUrl)
  {
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(*imapUrl, &rv);
    if (NS_SUCCEEDED(rv) && mailnewsUrl && aUrlListener)
      mailnewsUrl->RegisterListener(aUrlListener);
    nsCOMPtr<nsIMsgMessageUrl> msgurl(do_QueryInterface(*imapUrl));
    (*imapUrl)->SetExternalLinkUrl(PR_FALSE);
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
    rv = mailnewsUrl->SetSpec(urlSpec);
    
    hierarchyDelimiter = kOnlineHierarchySeparatorUnknown;
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aImapMailFolder);
    if (imapFolder)
      imapFolder->GetHierarchyDelimiter(&hierarchyDelimiter);
  }
  
  PR_Free(hostname);
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
      urlSpec.Append(char (hierarchySeparator));
      
      nsXPIDLCString folderName;
      
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      urlSpec.Append(">");
      urlSpec.Append(messageIdentifierList);
      rv = uri->SetSpec(urlSpec);
      
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
      urlSpec.Append(char (hierarchySeparator));
      
      nsXPIDLCString folderName;
      
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      rv = uri->SetSpec(urlSpec);
      if (NS_SUCCEEDED(rv))
        rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
        nsnull, aURL);
    }
  }
  return rv;
}
// FolderStatus, used to update message counts
NS_IMETHODIMP
nsImapService::UpdateFolderStatus(nsIEventQueue * aClientEventQueue, 
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
    aImapMailFolder, aUrlListener, urlSpec, hierarchySeparator);
  if (NS_SUCCEEDED(rv))
  {
    
    rv = imapUrl->SetImapAction(nsIImapUrl::nsImapFolderStatus);
    rv = SetImapUrlSink(aImapMailFolder, imapUrl);
    
    nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);
    
    if (NS_SUCCEEDED(rv))
    {
      
      urlSpec.Append("/folderstatus>");
      urlSpec.Append(char(hierarchySeparator));
      
      nsXPIDLCString folderName;
      
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      rv = uri->SetSpec(urlSpec);
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
      urlSpec.Append(char(hierarchySeparator));
      
      nsXPIDLCString folderName;
      
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      rv = uri->SetSpec(urlSpec);
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
			urlSpec.Append(char(hierarchySeparator));

            nsXPIDLCString folderName;
            GetFolderName(aImapMailFolder, getter_Copies(folderName));
            urlSpec.Append((const char *) folderName);
			urlSpec.Append(">");
			urlSpec.AppendInt(uidHighWater);
			rv = uri->SetSpec(urlSpec);
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

    // If it's an aol server then use 'deletefolder' url to 
    // remove all msgs first and then remove the folder itself.
    PRBool removeFolderAndMsgs = PR_FALSE;
    nsCOMPtr<nsIMsgIncomingServer> server;
    if (NS_SUCCEEDED(folder->GetServer(getter_AddRefs(server))) && server)
    {
      nsCOMPtr <nsIImapIncomingServer> imapServer = do_QueryInterface(server);
      if (imapServer) 
        imapServer->GetIsAOLServer(&removeFolderAndMsgs);

    }

    PRUnichar hierarchySeparator = GetHierarchyDelimiter(folder);
    rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), folder, urlListener, urlSpec, hierarchySeparator);
    if (NS_SUCCEEDED(rv))
    {
        rv = SetImapUrlSink(folder, imapUrl);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

            if (removeFolderAndMsgs)
              urlSpec.Append("/deletefolder>");
            else
              urlSpec.Append("/delete>");
            urlSpec.Append(char(hierarchySeparator));
            
            nsXPIDLCString folderName;
            rv = GetFolderName(folder, getter_Copies(folderName));
            if (NS_SUCCEEDED(rv))
            {
                urlSpec.Append((const char *) folderName);
                rv = uri->SetSpec(urlSpec);
                if (NS_SUCCEEDED(rv))
                {
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
  nsresult rv;
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
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
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
      urlSpec.Append(char (hierarchySeparator));
      
      nsXPIDLCString folderName;
      
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      urlSpec.Append(">");
      urlSpec.Append(messageIdentifierList);
      rv = uri->SetSpec(urlSpec);
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
      urlSpec.Append(char (hierarchySeparator));
      nsXPIDLCString folderName;
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      rv = uri->SetSpec(urlSpec);
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
      urlSpec.Append(char(hierarchySeparator));
      nsXPIDLCString folderName;
      GetFolderName(aImapMailFolder, getter_Copies(folderName));
      urlSpec.Append((const char *) folderName);
      urlSpec.Append(">");
      urlSpec.Append(messageIdentifierList);
      urlSpec.Append('>');
      urlSpec.AppendInt(flags);
      rv = uri->SetSpec(urlSpec);
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

  nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl);
  mailnewsUrl->SetFolder(aMsgFolder);

  return NS_OK;
}

NS_IMETHODIMP
nsImapService::DiscoverAllFolders(nsIEventQueue* aClientEventQueue,
                                  nsIMsgFolder* aImapMailFolder,
                                  nsIUrlListener* aUrlListener,
                                  nsIMsgWindow *  aMsgWindow,
                                  nsIURI** aURL)
{
  NS_ASSERTION (aImapMailFolder && aClientEventQueue, 
                "Oops ... null aClientEventQueue or aImapMailFolder");
  if (!aImapMailFolder || ! aClientEventQueue)
      return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCAutoString urlSpec;

  PRUnichar hierarchySeparator = GetHierarchyDelimiter(aImapMailFolder);
  nsresult rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl),
                                     aImapMailFolder,
                                     aUrlListener, urlSpec, hierarchySeparator);
  if (NS_SUCCEEDED (rv))
  {
    rv = SetImapUrlSink(aImapMailFolder, imapUrl);

    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);
      nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(imapUrl);
      if (mailnewsurl)
        mailnewsurl->SetMsgWindow(aMsgWindow);
      urlSpec.Append("/discoverallboxes");
      nsCOMPtr <nsIURI> url = do_QueryInterface(imapUrl, &rv);
		  rv = uri->SetSpec(urlSpec);
      if (NS_SUCCEEDED(rv))
         rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
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
			rv = uri->SetSpec(urlSpec);
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
            if (folderPath && *folderPath)
            {
                nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);

                urlSpec.Append("/discoverchildren>");
				urlSpec.Append(char(hierarchySeparator));
                urlSpec.Append(folderPath);
    			// mscott - this cast to a char * is okay...there's a bug in the XPIDL
				// compiler that is preventing in string parameters from showing up as
				// const char *. hopefully they will fix it soon.
				rv = uri->SetSpec(urlSpec);

        // Make sure the uri has the same hierarchy separator as the one in msg folder 
        // obj if it's not kOnlineHierarchySeparatorUnknown (ie, '^').
        char uriDelimiter;
        nsresult rv1 = aImapUrl->GetOnlineSubDirSeparator(&uriDelimiter);
        if (NS_SUCCEEDED (rv1) && hierarchySeparator != kOnlineHierarchySeparatorUnknown &&
            uriDelimiter != hierarchySeparator)
          aImapUrl->SetOnlineSubDirSeparator((char)hierarchySeparator);


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
            if (!folderPath || !*folderPath)
                return NS_ERROR_NULL_POINTER;

            nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);
            urlSpec.Append("/discoverlevelchildren>");
            urlSpec.AppendInt(level);
            urlSpec.Append(char(hierarchySeparator)); // hierarchySeparator "/"
            urlSpec.Append(folderPath);

            rv = uri->SetSpec(urlSpec);
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
                                                 aImapUrl,
                                                 nsnull, aURL);
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

    if (!sameServer) 
    {
      NS_ASSERTION(PR_FALSE, "can't use this method to copy across servers");
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
        imapUrl->AddChannelToLoadGroup();  //we get the loadGroup from msgWindow
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
        urlSpec.Append(char(hierarchySeparator));

        nsXPIDLCString folderName;
        GetFolderName(aSrcFolder, getter_Copies(folderName));
        urlSpec.Append((const char *) folderName);
        urlSpec.Append('>');
        urlSpec.Append(messageIds);
        urlSpec.Append('>');
        urlSpec.Append(char(hierarchySeparator));
        folderName.Adopt(nsCRT::strdup(""));
        GetFolderName(aDstFolder, getter_Copies(folderName));
        urlSpec.Append((const char *) folderName);

    		rv = uri->SetSpec(urlSpec);
        if (NS_SUCCEEDED(rv))
            rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
                                             nsnull, aURL);
    }
    return rv;
}

nsresult nsImapService::OfflineAppendFromFile(nsIFileSpec* aFileSpec,
                                              nsIURI *aUrl,
                                     nsIMsgFolder* aDstFolder,
                                     const char* messageId, // te be replaced
                                     PRBool inSelectedState, // needs to be in
                                     nsIUrlListener* aListener,
                                     nsIURI** aURL,
                                     nsISupports* aCopyState)
{
  nsCOMPtr <nsIMsgDatabase> destDB;
  nsresult rv = aDstFolder->GetMsgDatabase(nsnull, getter_AddRefs(destDB));
  // ### might need to send some notifications instead of just returning

  if (NS_SUCCEEDED(rv) && destDB)
  {
    nsMsgKey fakeKey;
    destDB->GetNextFakeOfflineMsgKey(&fakeKey);
    
    nsCOMPtr <nsIMsgOfflineImapOperation> op;
    rv = destDB->GetOfflineOpForKey(fakeKey, PR_TRUE, getter_AddRefs(op));
    if (NS_SUCCEEDED(rv) && op)
    {
      nsXPIDLCString destFolderUri;

      aDstFolder->GetURI(getter_Copies(destFolderUri));
      op->SetOperation(nsIMsgOfflineImapOperation::kAppendDraft); // ### do we care if it's a template?
      op->SetDestinationFolderURI(destFolderUri);
      nsCOMPtr <nsIOutputStream> offlineStore;
      rv = aDstFolder->GetOfflineStoreOutputStream(getter_AddRefs(offlineStore));

      if (NS_SUCCEEDED(rv) && offlineStore)
      {
        PRInt64 curOfflineStorePos = 0;
        nsCOMPtr <nsISeekableStream> seekable = do_QueryInterface(offlineStore);
        if (seekable)
          seekable->Tell(&curOfflineStorePos);
        else
        {
          NS_ASSERTION(PR_FALSE, "needs to be a random store!");
          return NS_ERROR_FAILURE;
        }


        nsCOMPtr <nsIInputStream> inputStream;
        nsCOMPtr <nsIMsgParseMailMsgState> msgParser = do_CreateInstance(NS_PARSEMAILMSGSTATE_CONTRACTID, &rv);
        msgParser->SetMailDB(destDB);

        if (NS_SUCCEEDED(rv))
          rv = aFileSpec->GetInputStream(getter_AddRefs(inputStream));
        if (NS_SUCCEEDED(rv) && inputStream)
        {
          // now, copy the temp file to the offline store for the dest folder.
          PRInt32 inputBufferSize = 10240;
          nsMsgLineStreamBuffer *inputStreamBuffer = new nsMsgLineStreamBuffer(inputBufferSize, PR_TRUE /* allocate new lines */, PR_FALSE /* leave CRLFs on the returned string */);
          PRUint32 fileSize;
          aFileSpec->GetFileSize(&fileSize);
          PRUint32 bytesWritten;
          rv = NS_OK;
//            rv = inputStream->Read(inputBuffer, inputBufferSize, &bytesRead);
//            if (NS_SUCCEEDED(rv) && bytesRead > 0)
          msgParser->SetState(nsIMsgParseMailMsgState::ParseHeadersState);
          // set the env pos to fake key so the msg hdr will have that for a key
          msgParser->SetEnvelopePos(fakeKey);
          PRBool needMoreData = PR_FALSE;
          char * newLine = nsnull;
          PRUint32 numBytesInLine = 0;
          do
          {
            newLine = inputStreamBuffer->ReadNextLine(inputStream, numBytesInLine, needMoreData); 
            if (newLine)
            {
              msgParser->ParseAFolderLine(newLine, numBytesInLine);
              rv = offlineStore->Write(newLine, numBytesInLine, &bytesWritten);
              nsCRT::free(newLine);
            }
          }
          while (newLine);
          nsCOMPtr <nsIMsgDBHdr> fakeHdr;

          msgParser->FinishHeader();
          msgParser->GetNewMsgHdr(getter_AddRefs(fakeHdr));
          if (fakeHdr)
          {
            if (NS_SUCCEEDED(rv) && fakeHdr)
            {
              PRUint32 resultFlags;
              nsInt64 tellPos = curOfflineStorePos;
              fakeHdr->SetMessageOffset((PRUint32) tellPos);
              fakeHdr->OrFlags(MSG_FLAG_OFFLINE | MSG_FLAG_READ, &resultFlags);
              fakeHdr->SetOfflineMessageSize(fileSize);
              destDB->AddNewHdrToDB(fakeHdr, PR_TRUE /* notify */);
              aDstFolder->SetFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
            }
          }
          // tell the listener we're done.
          inputStream = nsnull;
          aFileSpec->CloseStream();
          aListener->OnStopRunningUrl(aUrl, NS_OK);
          delete inputStreamBuffer;
        }
      }
    }
  }
          
         
  if (destDB)
    destDB->Close(PR_TRUE);
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
                                     nsISupports* aCopyState,
                                     nsIMsgWindow *aMsgWindow)
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
        nsCOMPtr <nsIMsgMailNewsUrl> msgUrl = do_QueryInterface(imapUrl);
        if (msgUrl && aMsgWindow)
        {
          //we get the loadGroup from msgWindow
          msgUrl->SetMsgWindow(aMsgWindow);
          imapUrl->AddChannelToLoadGroup();
        }

        SetImapUrlSink(aDstFolder, imapUrl);
        imapUrl->SetMsgFileSpec(aFileSpec);
        imapUrl->SetCopyState(aCopyState);

        nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);

        if (inSelectedState)
            urlSpec.Append("/appenddraftfromfile>");
        else
            urlSpec.Append("/appendmsgfromfile>");

        urlSpec.Append(char(hierarchySeparator));
        
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

        rv = uri->SetSpec(urlSpec);
        if (WeAreOffline())
        {
          return OfflineAppendFromFile(aFileSpec, uri, aDstFolder, messageId, inSelectedState, aListener, aURL, aCopyState);
          // handle offline append to drafts or templates folder here.
        }
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
  NS_ENSURE_ARG(aImapUrl);

  if (WeAreOffline())
  {
    nsImapAction imapAction;

    // the only thing we can do offline is fetch messages.
    // ### TODO - need to look at msg copy, save attachment, etc. when we
    // have offline message bodies.
    aImapUrl->GetImapAction(&imapAction);
    if (imapAction != nsIImapUrl::nsImapMsgFetch && imapAction != nsIImapUrl::nsImapSaveMessageToDisk)
      return NS_MSG_ERROR_OFFLINE;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgIncomingServer> aMsgIncomingServer;
  nsCOMPtr<nsIMsgMailNewsUrl> msgUrl = do_QueryInterface(aImapUrl);
  rv = msgUrl->GetServer(getter_AddRefs(aMsgIncomingServer));
    
  if (aURL)
      NS_IF_ADDREF(*aURL = msgUrl);

  if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
  {
    nsCOMPtr<nsIImapIncomingServer> aImapServer(do_QueryInterface(aMsgIncomingServer, &rv));
    if (NS_SUCCEEDED(rv) && aImapServer)
      rv = aImapServer->GetImapConnectionAndLoadUrl(aClientEventQueue,
                                                    aImapUrl, aConsumer);
  }
  return rv;
}

NS_IMETHODIMP
nsImapService::MoveFolder(nsIEventQueue* eventQueue, nsIMsgFolder* srcFolder,
                          nsIMsgFolder* dstFolder, nsIUrlListener* urlListener, 
                          nsIMsgWindow *msgWindow, nsIURI** url)
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
            nsCOMPtr<nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
            if (mailNewsUrl)
              mailNewsUrl->SetMsgWindow(msgWindow);
            char hierarchySeparator = kOnlineHierarchySeparatorUnknown;
            nsXPIDLCString folderName;
            
            nsCOMPtr<nsIURI> uri = do_QueryInterface(imapUrl);
            GetFolderName(srcFolder, getter_Copies(folderName));
            urlSpec.Append("/movefolderhierarchy>");
            urlSpec.Append(hierarchySeparator);
            urlSpec.Append((const char *) folderName);
            urlSpec.Append('>');
            folderName.Adopt(nsCRT::strdup(""));
            GetFolderName(dstFolder, getter_Copies(folderName));
            if ( folderName && folderName[0])
            {
               urlSpec.Append(hierarchySeparator);
               urlSpec.Append((const char *) folderName);
            }
            rv = uri->SetSpec(urlSpec);
            if (NS_SUCCEEDED(rv))
            {
                GetFolderName(srcFolder, getter_Copies(folderName));
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
                          nsIMsgWindow *msgWindow, nsIURI** url)
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
            nsCOMPtr<nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
            if (mailNewsUrl)
              mailNewsUrl->SetMsgWindow(msgWindow);
            nsXPIDLCString folderName;
            GetFolderName(srcFolder, getter_Copies(folderName));
            urlSpec.Append("/rename>");
            urlSpec.Append(char(hierarchySeparator));
            urlSpec.Append((const char *) folderName);
            urlSpec.Append('>');
            urlSpec.Append(char(hierarchySeparator));


            nsCAutoString cStrFolderName(NS_STATIC_CAST(const char *, folderName));
            // Unescape the name before looking for parent path
            nsUnescape(cStrFolderName.BeginWriting());
            PRInt32 leafNameStart = 
            cStrFolderName.RFindChar(hierarchySeparator);
            if (leafNameStart != -1)
            {
                cStrFolderName.SetLength(leafNameStart+1);
                urlSpec.Append(cStrFolderName);
            }

            nsCAutoString utfNewName;
            CopyUTF16toMUTF7(nsDependentString(newLeafName), utfNewName);
            char* escapedNewName = nsEscape(utfNewName.get(), url_Path);
            if (!escapedNewName) return NS_ERROR_OUT_OF_MEMORY;
            nsXPIDLCString escapedSlashName;
            rv = nsImapUrl::EscapeSlashes(escapedNewName, getter_Copies(escapedSlashName));
            NS_ENSURE_SUCCESS(rv, rv);
            nsCRT::free(escapedNewName);
            urlSpec.Append(escapedSlashName.get());

            rv = uri->SetSpec(urlSpec);
            if (NS_SUCCEEDED(rv))
            {
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
            urlSpec.Append(char(hierarchySeparator));
            if (!folderName.IsEmpty())
            {
              nsXPIDLCString canonicalName;

              nsImapUrl::ConvertToCanonicalFormat(folderName, (char) hierarchySeparator, getter_Copies(canonicalName));
              urlSpec.Append((const char *) canonicalName);
              urlSpec.Append(char(hierarchySeparator));
            }

            nsCAutoString utfNewName;
            rv = CopyUTF16toMUTF7(nsDependentString(newFolderName), utfNewName);
            NS_ENSURE_SUCCESS(rv, rv);
            char* escapedFolderName = nsEscape(utfNewName.get(), url_Path);
            urlSpec.Append(escapedFolderName);
            nsCRT::free(escapedFolderName);
    
            rv = uri->SetSpec(urlSpec);
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
            urlSpec.Append(char(hierarchySeparator));
            if (!folderName.IsEmpty())
            {
                urlSpec.Append((const char *) folderName);
                urlSpec.Append(char(hierarchySeparator));
            }
            nsCAutoString utfNewName; 
            CopyUTF16toMUTF7(nsDependentString(newFolderName), utfNewName);
            char* escapedFolderName = nsEscape(utfNewName.get(), url_Path);
            urlSpec.Append(escapedFolderName);
            nsCRT::free(escapedFolderName);
    
            rv = uri->SetSpec(urlSpec);
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
          urlSpec.Append(char(hierarchySeparator));
          if (!folderName.IsEmpty())
          {
            urlSpec.Append((const char *) folderName);
            rv = uri->SetSpec(urlSpec);
            if (NS_SUCCEEDED(rv))
              rv = GetImapConnectionAndLoadUrl(aClientEventQueue, imapUrl,
              nsnull,
              aURL);
          }
        } // if (NS_SUCCEEDED(rv))
    } // if (NS_SUCCEEDED(rv) && imapUrl)
    return rv;
}




NS_IMETHODIMP nsImapService::GetScheme(nsACString &aScheme)
{
  aScheme = "imap";
  return NS_OK; 
}

NS_IMETHODIMP nsImapService::GetDefaultPort(PRInt32 *aDefaultPort)
{
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = IMAP_PORT;

    return NS_OK;
}

NS_IMETHODIMP nsImapService::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD | ALLOWS_PROXY;
    return NS_OK;
}

NS_IMETHODIMP nsImapService::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // allow imap to run on any port
    *_retval = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsImapService::GetDefaultDoBiff(PRBool *aDoBiff)
{
    NS_ENSURE_ARG_POINTER(aDoBiff);
    // by default, do biff for IMAP servers
    *aDoBiff = PR_TRUE;    
    return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetDefaultServerPort(PRBool isSecure, PRInt32 *aDefaultPort)
{
    nsresult rv = NS_OK;

    // Return Secure IMAP Port if secure option chosen i.e., if isSecure is TRUE
    if (isSecure)
       *aDefaultPort = SECURE_IMAP_PORT;
    else    
        rv = GetDefaultPort(aDefaultPort);

    return rv;
}

nsresult nsImapService::CreateSubscribeURI(nsIMsgIncomingServer *server, char *folderName, nsIURI **retURI)
{
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = server->GetRootFolder(getter_AddRefs(rootMsgFolder));
  if (NS_FAILED(rv)) return rv;
  if (!rootMsgFolder) return NS_ERROR_FAILURE;
  PRUnichar hierarchyDelimiter;
  nsCAutoString urlSpec;
  nsCOMPtr <nsIImapUrl> imapUrl;

  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), rootMsgFolder, nsnull /* listener */, urlSpec, hierarchyDelimiter);
  if (NS_SUCCEEDED(rv) && imapUrl)
  {
    rv = SetImapUrlSink(rootMsgFolder, imapUrl);
    if (NS_SUCCEEDED(rv))
    {
        imapUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) retURI);
        urlSpec.Append("/subscribe>");
        urlSpec.Append(char (hierarchyDelimiter));
        char *escapedFolderName = nsEscape(folderName, url_Path);
        urlSpec.Append(escapedFolderName);
        nsCRT::free(escapedFolderName);
        rv = (*retURI)->SetSpec(urlSpec);
         
    }
  }
  return rv;
}

// this method first tries to find an exact username and hostname match with the given url
// then, tries to find any account on the passed in imap host in case this is a url to 
// a shared imap folder.
nsresult nsImapService::GetServerFromUrl(nsIImapUrl *aImapUrl, nsIMsgIncomingServer **aServer)
{
    nsCAutoString userPass;
    nsCAutoString hostName;
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl);
    nsresult rv;
    
    nsXPIDLCString folderName;

    // if we can't get a folder name out of the url then I think this is an error
    aImapUrl->CreateCanonicalSourceFolderPathString(getter_Copies(folderName));
    if (folderName.IsEmpty())
      rv = mailnewsUrl->GetFileName(folderName);
    if (NS_FAILED(rv)) 
      return rv;
    
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
             do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
      return rv;
    
    rv = accountManager->FindServerByURI(mailnewsUrl, PR_FALSE, aServer);

    // look for server with any user name, in case we're trying to subscribe
    // to a folder with some one else's user name like the following
    // "IMAP://userSharingFolder@server1/SharedFolderName"

    if (NS_FAILED(rv) || !aServer)
    {
      nsCAutoString turl;
      nsCOMPtr<nsIURL> url = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

      mailnewsUrl->GetSpec(turl);
      rv = url->SetSpec(turl);
      if (NS_FAILED(rv)) return rv;

      url->SetUserPass(NS_LITERAL_CSTRING(""));
      rv = accountManager->FindServerByURI(url, PR_FALSE, aServer);
      if (*aServer)
        aImapUrl->SetExternalLinkUrl(PR_TRUE);
    }
    // if we can't extract the imap server from this url then give up!!!
    if (NS_FAILED(rv))
      return rv;
    NS_ENSURE_TRUE(*aServer, NS_ERROR_FAILURE);
    return rv;
}

NS_IMETHODIMP nsImapService::NewURI(const nsACString &aSpec,
                                    const char *aOriginCharset, // ignored 
                                    nsIURI *aBaseURI,
                                    nsIURI **_retval)
{
  nsCOMPtr<nsIImapUrl> aImapUrl;
  nsresult rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull, NS_GET_IID(nsIImapUrl), getter_AddRefs(aImapUrl));
  if (NS_SUCCEEDED(rv))
  {
    // now extract lots of fun information...
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl);
    //nsCAutoString unescapedSpec(aSpec);
    // nsUnescape(unescapedSpec.BeginWriting());

    // set the spec
    if (aBaseURI) 
    {
      nsCAutoString newSpec;
      aBaseURI->Resolve(aSpec, newSpec);
      mailnewsUrl->SetSpec(newSpec);
    } 
    else 
    {
      mailnewsUrl->SetSpec(aSpec);
    }

    nsXPIDLCString folderName;

    // if we can't get a folder name out of the url then I think this is an error
    aImapUrl->CreateCanonicalSourceFolderPathString(getter_Copies(folderName));
    if (folderName.IsEmpty())
      rv = mailnewsUrl->GetFileName(folderName);
    if (NS_FAILED(rv)) 
      return rv;

    nsCOMPtr <nsIMsgIncomingServer> server;
    rv = GetServerFromUrl(aImapUrl, getter_AddRefs(server));
    // if we can't extract the imap server from this url then give up!!!
    if (NS_FAILED(rv)) 
      return rv;
    NS_ENSURE_TRUE(server, NS_ERROR_FAILURE);

    // now try to get the folder in question...
    nsCOMPtr<nsIMsgFolder> rootFolder;
    server->GetRootFolder(getter_AddRefs(rootFolder));

    if (rootFolder && !folderName.IsEmpty())
    {
      nsCOMPtr<nsIMsgFolder> folder;
      nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder, &rv);
      nsCOMPtr <nsIMsgImapMailFolder> subFolder;
      if (imapRoot)
      {
        imapRoot->FindOnlineSubFolder(folderName, getter_AddRefs(subFolder));
        folder = do_QueryInterface(subFolder, &rv);
      }
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIImapMessageSink> msgSink = do_QueryInterface(folder);
        rv = aImapUrl->SetImapMessageSink(msgSink);

        nsCOMPtr<nsIMsgFolder> msgFolder = do_QueryInterface(folder);
        rv = SetImapUrlSink(msgFolder, aImapUrl);	
        nsXPIDLCString msgKey;

         nsXPIDLCString messageIdString;
         aImapUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
         if (messageIdString.get())
        {
          PRBool useLocalCache = PR_FALSE;
          msgFolder->HasMsgOffline(atoi(messageIdString), &useLocalCache);  
          mailnewsUrl->SetMsgIsInLocalCache(useLocalCache);
        }
      }
    }

    // if we are fetching a part, be sure to enable fetch parts on demand
    PRBool mimePartSelectorDetected = PR_FALSE;
    aImapUrl->GetMimePartSelectorDetected(&mimePartSelectorDetected);
    if (mimePartSelectorDetected)
      aImapUrl->SetFetchPartsOnDemand(PR_TRUE);

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
    nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(imapUrl);

    // XXX this mock channel stuff is wrong -- the channel really should be owning the URL
    // and the originalURL, not the other way around
    rv = imapUrl->InitializeURIforMockChannel();
    rv = imapUrl->GetMockChannel(getter_AddRefs(mockChannel));
    if (NS_FAILED(rv) || !mockChannel) 
    {
      // this is a funky condition...it means we've already run the url once
      // and someone is trying to get us to run it again...
      imapUrl->Initialize(); // force a new mock channel to get created.
      rv = imapUrl->InitializeURIforMockChannel();
      rv = imapUrl->GetMockChannel(getter_AddRefs(mockChannel));
      if (!mockChannel) return NS_ERROR_FAILURE;
    }

    PRBool externalLinkUrl;
    imapUrl->GetExternalLinkUrl(&externalLinkUrl);
    if (externalLinkUrl)
    {
      // everything after here is to handle clicking on an external link. We only want
      // to do this if we didn't run the url through the various nsImapService methods,
      // which we can tell by seeing if the sinks have been setup on the url or not.
      nsCOMPtr <nsIMsgIncomingServer> server;
      rv = GetServerFromUrl(imapUrl, getter_AddRefs(server));
      NS_ENSURE_SUCCESS(rv, rv);
      nsXPIDLCString folderName;
      imapUrl->CreateCanonicalSourceFolderPathString(getter_Copies(folderName));
      if (folderName.IsEmpty())
      {
        rv = mailnewsUrl->GetFileName(folderName);
        if (!folderName.IsEmpty())
          NS_UnescapeURL(folderName);
      }
      // if the parent is null, then the folder doesn't really exist, so see if the user
      // wants to subscribe to it./
      nsCOMPtr<nsIMsgFolder> aFolder;
      // now try to get the folder in question...
      nsCOMPtr<nsIMsgFolder> rootFolder;
      server->GetRootFolder(getter_AddRefs(rootFolder));
      nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
      nsCOMPtr <nsIMsgImapMailFolder> subFolder;
      if (imapRoot)
      {
        imapRoot->FindOnlineSubFolder(folderName, getter_AddRefs(subFolder));
        aFolder = do_QueryInterface(subFolder);
      }
      nsCOMPtr <nsIMsgFolder> parent;
      if (aFolder)
        aFolder->GetParent(getter_AddRefs(parent));
      nsXPIDLCString serverKey;
      nsCAutoString userPass;
      rv = mailnewsUrl->GetUserPass(userPass);
      server->GetKey(getter_Copies(serverKey));
      char *fullFolderName = nsnull;
      if (parent)
        fullFolderName = ToNewCString(folderName);
      if (!parent && !folderName.IsEmpty())// check if this folder is another user's folder
      {
        fullFolderName = nsIMAPNamespaceList::GenerateFullFolderNameWithDefaultNamespace(serverKey.get(), folderName.get(), userPass.get(), kOtherUsersNamespace, nsnull);
        // if this is another user's folder, let's see if we're already subscribed to it.
        rv = imapRoot->FindOnlineSubFolder(fullFolderName, getter_AddRefs(subFolder));
        aFolder = do_QueryInterface(subFolder);
        if (aFolder)
          aFolder->GetParent(getter_AddRefs(parent));
      }
      // if we couldn't get the fullFolderName, then we probably couldn't find
      // the other user's namespace, in which case, we shouldn't try to subscribe to it.
      if (!parent && !folderName.IsEmpty() && fullFolderName)
      {
        // this folder doesn't exist - check if the user wants to subscribe to this folder.
        nsCOMPtr<nsIPrompt> dialog;
        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        wwatch->GetNewPrompter(nsnull, getter_AddRefs(dialog));

        nsXPIDLString statusString, confirmText;
        nsCOMPtr<nsIStringBundle> bundle;
        rv = IMAPGetStringBundle(getter_AddRefs(bundle));
        NS_ENSURE_SUCCESS(rv, rv);
        // need to convert folder name from mod-utf7 to unicode
        nsAutoString unescapedName;
        if (NS_FAILED(CopyMUTF7toUTF16(nsDependentCString(fullFolderName), unescapedName)))
            CopyASCIItoUTF16(nsDependentCString(fullFolderName), unescapedName);
        const PRUnichar *formatStrings[1] = { unescapedName.get() };

        rv = bundle->FormatStringFromID(IMAP_SUBSCRIBE_PROMPT,
                                          formatStrings, 1,
                                          getter_Copies(confirmText));
        NS_ENSURE_SUCCESS(rv,rv);

        PRBool confirmResult = PR_FALSE;
        rv = dialog->Confirm(nsnull, confirmText, &confirmResult);
        NS_ENSURE_SUCCESS(rv, rv);

        if (confirmResult)
        {
          nsCOMPtr <nsIImapIncomingServer> imapServer = do_QueryInterface(server);
          if (imapServer)
          {
            nsCOMPtr <nsIURI> subscribeURI;
            // now we have the real folder name to try to subscribe to. Let's try running
            // a subscribe url and returning that as the uri we've created.
            // We need to convert this to unicode because that's what subscribe wants :-(
            // It's already in mod-utf7.
            nsAutoString unicodeName;
            unicodeName.AssignWithConversion(fullFolderName);
            rv = imapServer->SubscribeToFolder(unicodeName.get(), PR_TRUE, getter_AddRefs(subscribeURI));
            if (NS_SUCCEEDED(rv) && subscribeURI)
            {
              nsCOMPtr <nsIImapUrl> imapSubscribeUrl = do_QueryInterface(subscribeURI);
              if (imapSubscribeUrl)
                imapSubscribeUrl->SetExternalLinkUrl(PR_TRUE);
              nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(subscribeURI);
              if (mailnewsUrl)
              {
                nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
                NS_ENSURE_SUCCESS(rv, rv);
                nsCOMPtr <nsIMsgWindow> msgWindow;
                rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
                if (NS_SUCCEEDED(rv) && msgWindow)
                {
                  mailnewsUrl->SetMsgWindow(msgWindow);
                  nsCOMPtr <nsIUrlListener> listener = do_QueryInterface(rootFolder);
                  if (listener)
                    mailnewsUrl->RegisterListener(listener);
                }
              }
            }
          }
        }
        // error out this channel, so it'll stop trying to run the url.
        rv = NS_ERROR_FAILURE;
        *_retval = nsnull;
        PR_Free(fullFolderName);
      }
      else if (fullFolderName)// this folder exists - check if this is a click on a link to the folder
      {     // in which case, we'll select it.
        nsCOMPtr <nsIMsgFolder> imapFolder;
        nsCOMPtr <nsIImapServerSink> serverSink;

        mailnewsUrl->GetFolder(getter_AddRefs(imapFolder));
        imapUrl->GetImapServerSink(getter_AddRefs(serverSink));
        // need to see if this is a link click - one way is to check if the url is set up correctly
        // if not, it's probably a url click. We need a better way of doing this. 
        if (!imapFolder)
        {
          nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv, rv);
          nsCOMPtr <nsIMsgWindow> msgWindow;
          rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
          if (NS_SUCCEEDED(rv) && msgWindow)
          {
            nsXPIDLCString uri;
            rootFolder->GetURI(getter_Copies(uri));
	    uri.Append('/');
            uri.Append(fullFolderName);
            msgWindow->SelectFolder(uri.get());
            // error out this channel, so it'll stop trying to run the url.
            *_retval = nsnull;
            rv = NS_ERROR_FAILURE;
          }
          else
          {
            // make sure the imap action is selectFolder, so the content type
            // will be x-application-imapfolder, so ::HandleContent will
            // know to open a new 3 pane window.
            imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);
          }
        }
      }
    }
    if (NS_SUCCEEDED(rv))
      NS_IF_ADDREF(*_retval = mockChannel);
    return rv;
}

NS_IMETHODIMP
nsImapService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    NS_ENSURE_ARG(aPath);
    
    nsFileSpec spec;
    nsresult rv = aPath->GetFileSpec(&spec);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsILocalFile> localFile;
    NS_FileSpecToIFile(&spec, getter_AddRefs(localFile));
    if (!localFile) return NS_ERROR_FAILURE;
    
    return NS_SetPersistentFile(PREF_MAIL_ROOT_IMAP_REL, PREF_MAIL_ROOT_IMAP, localFile);
}       

NS_IMETHODIMP
nsImapService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    
    PRBool havePref;
    nsCOMPtr<nsILocalFile> localFile;    
    rv = NS_GetPersistentFile(PREF_MAIL_ROOT_IMAP_REL,
                              PREF_MAIL_ROOT_IMAP,
                              NS_APP_IMAP_MAIL_50_DIR,
                              havePref,
                              getter_AddRefs(localFile));
        
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpecFromIFile(localFile, getter_AddRefs(outSpec));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!havePref || !exists) 
    {
        rv = NS_SetPersistentFile(PREF_MAIL_ROOT_IMAP_REL, PREF_MAIL_ROOT_IMAP, localFile);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to set root dir pref.");
    }
        
    NS_IF_ADDREF(*aResult = outSpec);
    return NS_OK;
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
nsImapService::GetCanLoginAtStartUp(PRBool *aCanLoginAtStartUp)
{
        NS_ENSURE_ARG_POINTER(aCanLoginAtStartUp);
        *aCanLoginAtStartUp = PR_TRUE;
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
nsImapService::GetCanGetMessages(PRBool *aCanGetMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetMessages);
    *aCanGetMessages = PR_TRUE;
    return NS_OK;
}        

NS_IMETHODIMP
nsImapService::GetCanGetIncomingMessages(PRBool *aCanGetIncomingMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetIncomingMessages);
    *aCanGetIncomingMessages = PR_TRUE;
    return NS_OK;
}    

NS_IMETHODIMP
nsImapService::GetShowComposeMsgLink(PRBool *showComposeMsgLink)
{
    NS_ENSURE_ARG_POINTER(showComposeMsgLink);
    *showComposeMsgLink = PR_TRUE;
    return NS_OK;
}        

NS_IMETHODIMP
nsImapService::GetNeedToBuildSpecialFolderURIs(PRBool *needToBuildSpecialFolderURIs)
{
    NS_ENSURE_ARG_POINTER(needToBuildSpecialFolderURIs);
    *needToBuildSpecialFolderURIs = PR_TRUE;
    return NS_OK;
}        

NS_IMETHODIMP
nsImapService::GetSpecialFoldersDeletionAllowed(PRBool *specialFoldersDeletionAllowed)
{
    NS_ENSURE_ARG_POINTER(specialFoldersDeletionAllowed);
    *specialFoldersDeletionAllowed = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetListOfFoldersWithPath(nsIImapIncomingServer *aServer, nsIMsgWindow *aMsgWindow, const char *folderPath)
{
  nsresult rv;

#ifdef DEBUG_sspitzer
  printf("GetListOfFoldersWithPath(%s)\n",folderPath);
#endif
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aServer);
  if (!server) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  rv = server->GetRootMsgFolder(getter_AddRefs(rootMsgFolder));

  if (NS_FAILED(rv)) return rv;
  if (!rootMsgFolder) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIUrlListener> listener = do_QueryInterface(aServer, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!listener) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEventQueue> queue;
  // get the Event Queue for this thread...
  nsCOMPtr<nsIEventQueueService> pEventQService = 
    do_GetService(kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
  if (NS_FAILED(rv)) return rv;
  
  // Locate the folder so that the correct hierarchical delimiter is used in the folder
  // pathnames, otherwise root's (ie, '^') is used and this is wrong.
  nsCOMPtr<nsIMsgFolder> msgFolder;
  if (rootMsgFolder && folderPath && (*folderPath))
  {
    // If the folder path contains 'INBOX' of any forms, we need to convert it to uppercase
    // before finding it under the root folder. We do the same in PossibleImapMailbox().
    nsCAutoString tempFolderName(folderPath);
    nsCAutoString tokenStr, remStr, changedStr;
    PRInt32 slashPos = tempFolderName.FindChar('/');
    if (slashPos > 0)
    {
      tempFolderName.Left(tokenStr,slashPos);
      tempFolderName.Right(remStr, tempFolderName.Length()-slashPos);
    }
    else
      tokenStr.Assign(tempFolderName);
    
    if ((nsCRT::strcasecmp(tokenStr.get(), "INBOX")==0) && (nsCRT::strcmp(tokenStr.get(), "INBOX") != 0))
      changedStr.Append("INBOX");
    else
      changedStr.Append(tokenStr);
    
    if (slashPos > 0 ) 
      changedStr.Append(remStr);
    
    
    rv = rootMsgFolder->FindSubFolder(changedStr, getter_AddRefs(msgFolder));
  }
  
  rv = DiscoverChildren(queue, msgFolder, listener, folderPath, nsnull);
  if (NS_FAILED(rv)) return rv;
  
  return NS_OK;
}

NS_IMETHODIMP
nsImapService::GetListOfFoldersOnServer(nsIImapIncomingServer *aServer, nsIMsgWindow *aMsgWindow)
{
	nsresult rv;

        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aServer);
	if (!server) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIMsgFolder> rootMsgFolder;
        rv = server->GetRootMsgFolder(getter_AddRefs(rootMsgFolder));

	if (NS_FAILED(rv)) return rv;
	if (!rootMsgFolder) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIUrlListener> listener = do_QueryInterface(aServer, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!listener) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIEventQueue> queue;
        // get the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> pEventQService = 
                 do_GetService(kEventQueueServiceCID, &rv);
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
                               const PRUnichar* aFolderName, 
                               nsIUrlListener* urlListener, nsIURI** url)
{
    NS_ENSURE_ARG_POINTER(eventQueue);
    NS_ENSURE_ARG_POINTER(aFolder);
    NS_ENSURE_ARG_POINTER(aFolderName);
    
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
            urlSpec.Append(char(hierarchySeparator));
            nsCAutoString utfFolderName;
            rv = CopyUTF16toMUTF7(nsDependentString(aFolderName), utfFolderName);
            NS_ENSURE_SUCCESS(rv, rv);
            char* escapedFolderName = nsEscape(utfFolderName.get(), url_Path);
            urlSpec.Append(escapedFolderName);
            nsCRT::free(escapedFolderName);
            rv = uri->SetSpec(urlSpec);
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
            urlSpec.Append(char(hierarchySeparator));
            nsCAutoString utfFolderName;
            rv = CopyUTF16toMUTF7(nsDependentString(folderName), utfFolderName);
            NS_ENSURE_SUCCESS(rv, rv);
            char* escapedFolderName = nsEscape(utfFolderName.get(), url_Path);
            urlSpec.Append(escapedFolderName);
            nsCRT::free(escapedFolderName);
            rv = uri->SetSpec(urlSpec);
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull, url);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapService::GetFolderAdminUrl(nsIEventQueue *aClientEventQueue,
                      nsIMsgFolder *anImapFolder,
                      nsIMsgWindow   *aMsgWindow,
                      nsIUrlListener *aUrlListener,
                      nsIURI** aURL)
{
  NS_ENSURE_ARG_POINTER(aClientEventQueue);
  NS_ENSURE_ARG_POINTER(anImapFolder);
  NS_ENSURE_ARG_POINTER(aMsgWindow);
  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCAutoString urlSpec;
  nsresult rv;
  PRUnichar hierarchySeparator = GetHierarchyDelimiter(anImapFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), anImapFolder, aUrlListener, urlSpec, hierarchySeparator);
  
  if (NS_SUCCEEDED(rv) && imapUrl)
  {
    // nsImapUrl::SetSpec() will set the imap action properly
    rv = imapUrl->SetImapAction(nsIImapUrl::nsImapRefreshFolderUrls);
    
    nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
    mailNewsUrl->SetMsgWindow(aMsgWindow);
    mailNewsUrl->SetUpdatingFolder(PR_TRUE);
    imapUrl->AddChannelToLoadGroup();
    rv = SetImapUrlSink(anImapFolder, imapUrl);
    
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLCString folderName;
      GetFolderName(anImapFolder, getter_Copies(folderName));
      urlSpec.Append("/refreshfolderurls>");
      urlSpec.Append(char(hierarchySeparator));
      urlSpec.Append((const char *) folderName);
      rv = mailNewsUrl->SetSpec(urlSpec);
      if (NS_SUCCEEDED(rv))
        rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
        imapUrl,
        nsnull,
        aURL);
    }
  } // if we have a url to run....
  
  return rv;
}

NS_IMETHODIMP
nsImapService::IssueCommandOnMsgs(nsIEventQueue *aClientEventQueue,
                      nsIMsgFolder *anImapFolder,
                      nsIMsgWindow   *aMsgWindow,
                      const char *aCommand,
                      const char *uids,
                      nsIURI** aURL)
{
  NS_ENSURE_ARG_POINTER(aClientEventQueue);
  NS_ENSURE_ARG_POINTER(anImapFolder);
  NS_ENSURE_ARG_POINTER(aMsgWindow);
  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCAutoString urlSpec;
  nsresult rv;
  PRUnichar hierarchySeparator = GetHierarchyDelimiter(anImapFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), anImapFolder, nsnull, urlSpec, hierarchySeparator);
  
  if (NS_SUCCEEDED(rv) && imapUrl)
  {
    // nsImapUrl::SetSpec() will set the imap action properly
    rv = imapUrl->SetImapAction(nsIImapUrl::nsImapUserDefinedMsgCommand);
    
    nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
    mailNewsUrl->SetMsgWindow(aMsgWindow);
    mailNewsUrl->SetUpdatingFolder(PR_TRUE);
    imapUrl->AddChannelToLoadGroup();
    rv = SetImapUrlSink(anImapFolder, imapUrl);
    
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLCString folderName;
      GetFolderName(anImapFolder, getter_Copies(folderName));
      urlSpec.Append("/");
      urlSpec.Append(aCommand);
      urlSpec.Append(">");
      urlSpec.Append(uidString);
      urlSpec.Append(">");
      urlSpec.Append(char(hierarchySeparator));
      urlSpec.Append((const char *) folderName);
      urlSpec.Append(">");
      urlSpec.Append(uids);
      rv = mailNewsUrl->SetSpec(urlSpec);
      if (NS_SUCCEEDED(rv))
        rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
        imapUrl,
        nsnull,
        aURL);
    }
  } // if we have a url to run....
  
  return rv;
}

NS_IMETHODIMP
nsImapService::FetchCustomMsgAttribute(nsIEventQueue *aClientEventQueue,
                      nsIMsgFolder *anImapFolder,
                      nsIMsgWindow   *aMsgWindow,
                      const char *aAttribute,
                      const char *uids,
                      nsIURI** aURL)
{
  NS_ENSURE_ARG_POINTER(aClientEventQueue);
  NS_ENSURE_ARG_POINTER(anImapFolder);
  NS_ENSURE_ARG_POINTER(aMsgWindow);
  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCAutoString urlSpec;
  nsresult rv;
  PRUnichar hierarchySeparator = GetHierarchyDelimiter(anImapFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), anImapFolder, nsnull, urlSpec, hierarchySeparator);
  
  if (NS_SUCCEEDED(rv) && imapUrl)
  {
    // nsImapUrl::SetSpec() will set the imap action properly
    rv = imapUrl->SetImapAction(nsIImapUrl::nsImapUserDefinedFetchAttribute);
    
    nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
    mailNewsUrl->SetMsgWindow(aMsgWindow);
    mailNewsUrl->SetUpdatingFolder(PR_TRUE);
    imapUrl->AddChannelToLoadGroup();
    rv = SetImapUrlSink(anImapFolder, imapUrl);
    
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLCString folderName;
      GetFolderName(anImapFolder, getter_Copies(folderName));
      urlSpec.Append("/customFetch>UID>");
      urlSpec.Append(char(hierarchySeparator));
      urlSpec.Append((const char *) folderName);
      urlSpec.Append(">");
      urlSpec.Append(uids);
      urlSpec.Append(">");
      urlSpec.Append(aAttribute);
      rv = mailNewsUrl->SetSpec(urlSpec);
      if (NS_SUCCEEDED(rv))
        rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
        imapUrl,
        nsnull,
        aURL);
    }
  } // if we have a url to run....
  
  return rv;
}

NS_IMETHODIMP
nsImapService::StoreCustomKeywords(nsIEventQueue *aClientEventQueue,
                      nsIMsgFolder *anImapFolder,
                      nsIMsgWindow   *aMsgWindow,
                      const char *flagsToAdd,
                      const char *flagsToSubtract,
                      const char *uids,
                      nsIURI** aURL)
{
  NS_ENSURE_ARG_POINTER(aClientEventQueue);
  NS_ENSURE_ARG_POINTER(anImapFolder);
  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCAutoString urlSpec;
  nsresult rv;
  PRUnichar hierarchySeparator = GetHierarchyDelimiter(anImapFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), anImapFolder, nsnull, urlSpec, hierarchySeparator);
  
  if (NS_SUCCEEDED(rv) && imapUrl)
  {
    // nsImapUrl::SetSpec() will set the imap action properly
    rv = imapUrl->SetImapAction(nsIImapUrl::nsImapMsgStoreCustomKeywords);
    
    nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(imapUrl);
    mailNewsUrl->SetMsgWindow(aMsgWindow);
    mailNewsUrl->SetUpdatingFolder(PR_TRUE);
    imapUrl->AddChannelToLoadGroup();
    rv = SetImapUrlSink(anImapFolder, imapUrl);
    
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLCString folderName;
      GetFolderName(anImapFolder, getter_Copies(folderName));
      urlSpec.Append("/customKeywords>UID>");
      urlSpec.Append(char(hierarchySeparator));
      urlSpec.Append((const char *) folderName);
      urlSpec.Append(">");
      urlSpec.Append(uids);
      urlSpec.Append(">");
      urlSpec.Append(flagsToAdd);
      urlSpec.Append(">");
      urlSpec.Append(flagsToSubtract);
      rv = mailNewsUrl->SetSpec(urlSpec);
      if (NS_SUCCEEDED(rv))
        rv = GetImapConnectionAndLoadUrl(aClientEventQueue,
        imapUrl,
        nsnull,
        aURL);
    }
  } // if we have a url to run....
  
  return rv;
}


NS_IMETHODIMP
nsImapService::DownloadMessagesForOffline(const char *messageIds, nsIMsgFolder *aFolder, nsIUrlListener *aUrlListener, nsIMsgWindow *aMsgWindow)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  NS_ENSURE_ARG_POINTER(messageIds);
  
  nsCOMPtr<nsIImapUrl> imapUrl;
  nsCAutoString urlSpec;
  nsresult rv;
  PRUnichar hierarchySeparator = GetHierarchyDelimiter(aFolder);
  rv = CreateStartOfImapUrl(nsnull, getter_AddRefs(imapUrl), aFolder, nsnull,
                            urlSpec, hierarchySeparator);
  if (NS_SUCCEEDED(rv) && imapUrl)
  {
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr <nsIURI> runningURI;
        // need to pass in stream listener in order to get the channel created correctly
        nsCOMPtr<nsIImapMessageSink> imapMessageSink(do_QueryInterface(aFolder, &rv));
        rv = FetchMessage(imapUrl, nsImapUrl::nsImapMsgDownloadForOffline,aFolder, imapMessageSink, 
                            aMsgWindow, nsnull, messageIds, PR_FALSE, nsnull, getter_AddRefs(runningURI));
        if (runningURI && aUrlListener)
        {
          nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(runningURI));

          if (msgurl)
            msgurl->RegisterListener(aUrlListener);
        }
      }
  }
  return rv;
}

NS_IMETHODIMP
nsImapService::MessageURIToMsgHdr(const char *uri, nsIMsgDBHdr **_retval)
{
  NS_ENSURE_ARG_POINTER(uri);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIMsgFolder> folder;
  nsMsgKey msgKey;

  rv = DecomposeImapURI(uri, getter_AddRefs(folder), &msgKey);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = folder->GetMessageHeader(msgKey, _retval);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

NS_IMETHODIMP
nsImapService::PlaybackAllOfflineOperations(nsIMsgWindow *aMsgWindow, nsIUrlListener *aListener, nsISupports **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult rv;
  nsImapOfflineSync *goOnline = new nsImapOfflineSync(aMsgWindow, aListener, nsnull);
  if (goOnline)
  {
    rv = goOnline->QueryInterface(NS_GET_IID(nsISupports), (void **) aResult); 
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_SUCCEEDED(rv) && *aResult)
      return goOnline->ProcessNextOperation();
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsImapService::DownloadAllOffineImapFolders(nsIMsgWindow *aMsgWindow, nsIUrlListener *aListener)
{
  nsImapOfflineDownloader *downloadForOffline = new nsImapOfflineDownloader(aMsgWindow, aListener);
  if (downloadForOffline)
  {
    // hold reference to this so it won't get deleted out from under itself.
    NS_ADDREF(downloadForOffline); 
    nsresult rv = downloadForOffline->ProcessNextOperation();
    NS_RELEASE(downloadForOffline);
    return rv;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP nsImapService::GetCacheSession(nsICacheSession **result)
{
  nsresult rv = NS_OK;
  if (!mCacheSession)
  {
    nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = serv->CreateSession("IMAP-memory-only", nsICache::STORE_IN_MEMORY, nsICache::STREAM_BASED, getter_AddRefs(mCacheSession));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mCacheSession->SetDoomEntriesIfExpired(PR_FALSE);
  }

  *result = mCacheSession;
  NS_IF_ADDREF(*result);
  return rv;
}

NS_IMETHODIMP 
nsImapService::HandleContent(const char * aContentType, nsIInterfaceRequestor* aWindowContext, nsIRequest *request)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(request);
  
  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsCRT::strcasecmp(aContentType, "x-application-imapfolder") == 0)
  {
    nsCOMPtr<nsIURI> uri;
    rv = aChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    if (uri)
    {
      request->Cancel(NS_BINDING_ABORTED);
      nsCOMPtr<nsIWindowMediator> mediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv, rv);
      nsCAutoString uriStr;

      uri->GetSpec(uriStr);

      // imap uri's are unescaped, so unescape the url.
      NS_UnescapeURL(uriStr);
      nsCOMPtr <nsIMessengerWindowService> messengerWindowService = do_GetService(NS_MESSENGERWINDOWSERVICE_CONTRACTID,&rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = messengerWindowService->OpenMessengerWindowWithUri("mail:3pane", uriStr.get(), nsMsgKey_None);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    // The content-type was not x-application-imapfolder
    return NS_ERROR_WONT_HANDLE_CONTENT;
  }

  return rv;
}

