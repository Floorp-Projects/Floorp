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
#include "nsReadableUtils.h"
#include "nsRDFCID.h"
#include "nsEscape.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIPref.h"
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
#include "nsNetCID.h"
#include "nsMsgUtf7Utils.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsICopyMsgStreamListener.h"
#include "nsIFileStream.h"
#include "nsIMsgParseMailMsgState.h"
#include "nsMsgLineBuffer.h"
#include "nsMsgLocalCID.h"
#include "nsIOutputStream.h"
#include "xp_core.h"

#define PREF_MAIL_ROOT_IMAP "mail.root.imap"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);


static const char *sequenceString = "SEQUENCE";
static const char *uidString = "UID";

static PRBool gInitialized = PR_FALSE;
static PRInt32 gMIMEOnDemandThreshold = 15000;
static PRBool gMIMEOnDemand = PR_FALSE;

#define SAVE_BUF_SIZE 8192
class nsImapSaveAsListener : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsImapSaveAsListener(nsIFileSpec *aFileSpec, PRBool addDummyEnvelope);
  virtual ~nsImapSaveAsListener();
  nsresult SetupMsgWriteStream(nsIFileSpec *aFileSpec, PRBool addDummyEnvelope);
protected:
  nsCOMPtr<nsIOutputStream> m_outputStream;
  nsCOMPtr<nsIFileSpec> m_outputFile;
  PRBool m_addDummyEnvelope;
  PRBool m_writtenData;
  PRUint32 m_leftOver;
  char m_dataBuffer[SAVE_BUF_SIZE+1]; // temporary buffer for this save operation

};

NS_IMPL_ISUPPORTS1(nsImapSaveAsListener, nsIStreamListener)

nsImapSaveAsListener::nsImapSaveAsListener(nsIFileSpec *aFileSpec, PRBool addDummyEnvelope)
{
  m_outputFile = aFileSpec;
  m_writtenData = PR_FALSE;
  m_addDummyEnvelope = addDummyEnvelope;
  m_leftOver = 0;
  NS_INIT_REFCNT();
}

nsImapSaveAsListener::~nsImapSaveAsListener()
{
}

NS_IMETHODIMP nsImapSaveAsListener::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImapSaveAsListener::OnStopRequest(nsIRequest *request, nsISupports * aCtxt, nsresult aStatus)
{
  if (m_outputStream)
  {
    m_outputStream->Flush();
    m_outputStream->Close();
  }
  if (m_outputFile)
    m_outputFile->CloseStream();
  return NS_OK;
} 

NS_IMETHODIMP nsImapSaveAsListener::OnDataAvailable(nsIRequest* request, 
                                  nsISupports* aSupport,
                                  nsIInputStream* inStream, 
                                  PRUint32 srcOffset,
                                  PRUint32 count)
{
  nsresult rv;
  PRUint32 available;
  rv = inStream->Available(&available);
  if (!m_writtenData)
  {
    m_writtenData = PR_TRUE;
    rv = SetupMsgWriteStream(m_outputFile, m_addDummyEnvelope);
    NS_ENSURE_SUCCESS(rv, rv);
  }


  PRUint32 readCount, maxReadCount = SAVE_BUF_SIZE - m_leftOver;
  PRUint32 writeCount;
  char *start, *end;
  PRUint32 linebreak_len = 0;

  while (count > 0)
  {
      if (count < (PRInt32) maxReadCount)
          maxReadCount = count;
      rv = inStream->Read(m_dataBuffer + m_leftOver,
                          maxReadCount,
                          &readCount);
      if (NS_FAILED(rv)) return rv;

      m_leftOver += readCount;
      m_dataBuffer[m_leftOver] = '\0';

      start = m_dataBuffer;
      end = PL_strstr(start, "\r");
      if (!end)
          end = PL_strstr(start, "\n");
      else if (*(end+1) == nsCRT::LF && linebreak_len == 0)
          linebreak_len = 2;

      if (linebreak_len == 0) // not initialize yet
          linebreak_len = 1;

      count -= readCount;
      maxReadCount = SAVE_BUF_SIZE - m_leftOver;

      if (!end && count > (PRInt32) maxReadCount)
          // must be a very very long line; sorry cannot handle it
          return NS_ERROR_FAILURE;

      while (start && end)
      {
          if (PL_strncasecmp(start, "X-Mozilla-Status:", 17) &&
              PL_strncasecmp(start, "X-Mozilla-Status2:", 18) &&
              PL_strncmp(start, "From - ", 7))
          {
              rv = m_outputStream->Write(start, end-start, &writeCount);
              rv = m_outputStream->Write(CRLF, 2, &writeCount);
          }
          start = end+linebreak_len;
          if (start >= m_dataBuffer + m_leftOver)
          {
              maxReadCount = SAVE_BUF_SIZE;
              m_leftOver = 0;
              break;
          }
          end = PL_strstr(start, "\r");
          if (!end)
              end = PL_strstr(start, "\n");
          if (start && !end)
          {
              m_leftOver -= (start - m_dataBuffer);
              nsCRT::memcpy(m_dataBuffer, start,
                            m_leftOver+1); // including null
              maxReadCount = SAVE_BUF_SIZE - m_leftOver;
          }
      }
      if (NS_FAILED(rv)) return rv;
  }
  return rv;
  
  //  rv = m_outputStream->WriteFrom(inStream, PR_MIN(available, count), &bytesWritten);
}

nsresult nsImapSaveAsListener::SetupMsgWriteStream(nsIFileSpec *aFileSpec, PRBool addDummyEnvelope)
{
  nsresult rv = NS_ERROR_FAILURE;

  // if the file already exists, delete it.
  // do this before we get the outputstream
  nsFileSpec fileSpec;
  aFileSpec->GetFileSpec(&fileSpec);
  fileSpec.Delete(PR_FALSE);

  rv = aFileSpec->GetOutputStream(getter_AddRefs(m_outputStream));
  NS_ENSURE_SUCCESS(rv,rv);

  if (m_outputStream && addDummyEnvelope)
  {
    nsCAutoString result;
    char *ct;
    PRUint32 writeCount;
    time_t now = time ((time_t*) 0);
    ct = ctime(&now);
    ct[24] = 0;
    result = "From - ";
    result += ct;
    result += MSG_LINEBREAK;
    
    m_outputStream->Write(result.get(), result.Length(),
                               &writeCount);
    result = "X-Mozilla-Status: 0001";
    result += MSG_LINEBREAK;
    m_outputStream->Write(result.get(), result.Length(),
                               &writeCount);
    result =  "X-Mozilla-Status2: 00000000";
    result += MSG_LINEBREAK;
    m_outputStream->Write(result.get(), result.Length(),
                               &writeCount);
  }
  return rv;
}

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
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
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


	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
  NS_ASSERTION (aImapMailFolder && aClientEventQueue,
                "Oops ... null pointer");
  if (!aImapMailFolder || !aClientEventQueue)
      return NS_ERROR_NULL_POINTER;

  if (WeAreOffline())
    return NS_MSG_ERROR_OFFLINE;

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
			urlSpec.AppendWithConversion(hierarchySeparator);
      urlSpec.Append((const char *) folderName);
      rv = mailNewsUrl->SetSpec(urlSpec.get());
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
			rv = uri->SetSpec(urlSpec.get());
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
      imapUrl->SetImapFolder(folder);
      if (folder)
      {
        nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(imapUrl);
        if (mailnewsUrl)
        {
          PRBool useLocalCache = PR_FALSE;
          folder->HasMsgOffline(atoi(msgKey), &useLocalCache);  
          mailnewsUrl->SetMsgIsInLocalCache(useLocalCache);
        }
      }

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
		  rv = url->SetSpec(urlSpec.get());
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
      PRBool shouldStoreMsgOffline = PR_FALSE;
      PRBool hasMsgOffline = PR_FALSE;

      if (folder)
      {
        folder->ShouldStoreMsgOffline(key, &shouldStoreMsgOffline);
        folder->HasMsgOffline(key, &hasMsgOffline);
      }

      if (imapMessageSink)
        imapMessageSink->SetNotifyDownloadedLines(shouldStoreMsgOffline);

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
      if (hasMsgOffline)
        msgurl->SetMsgIsInLocalCache(PR_TRUE);

      rv = FetchMessage(imapUrl, nsIImapUrl::nsImapMsgFetch, folder, imapMessageSink,
                        aMsgWindow, aURL, aDisplayConsumer, msgKey, PR_TRUE);
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
		  rv = url->SetSpec(urlSpec.get());

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
        
        rv = docShell->LoadURI(url, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
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
#ifdef DEBUG_mscott
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
                              aMsgWindow, aURL, streamSupport, messageIds.get(), PR_TRUE);
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
    // escape aSearchUri so that IMAP special characters (i.e. '\')
    // won't be replaced with '/' in NECKO.
    // it will be unescaped in nsImapUrl::ParseUrl().
    char *search_cmd = nsEscape((char *)aSearchUri, url_XAlphas);
    urlSpec.Append(search_cmd);
    nsCRT::free(search_cmd);
    rv = mailNewsUrl->SetSpec(urlSpec.get());
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
    rv = rdf->GetResource(folderURI.get(), getter_AddRefs(res));
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
      messageIdString.AppendInt(msgKey, 10 /* base 10 */);
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

        nsImapSaveAsListener *saveAsListener = new nsImapSaveAsListener(aFile, aAddDummyEnvelope);
        
        return FetchMessage(imapUrl, nsIImapUrl::nsImapSaveMessageToDisk, folder, imapMessageSink, aMsgWindow, aURL, saveAsListener, msgKey, PR_TRUE);
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

  nsresult rv = NS_OK;
  nsXPIDLCString currentSpec;
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
  rv = url->SetSpec(urlSpec.get());

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
    rv = docShell->LoadURI(url, nsnull, nsIWebNavigation::LOAD_FLAGS_NONE);
  }
  else
  {
    nsCOMPtr<nsIStreamListener> aStreamListener = do_QueryInterface(aDisplayConsumer, &rv);
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl, &rv);
    if (aMsgWindow && mailnewsUrl)
      mailnewsUrl->SetMsgWindow(aMsgWindow);
    if (NS_SUCCEEDED(rv) && aStreamListener)
    {
      nsCOMPtr<nsIChannel> aChannel;
      nsCOMPtr<nsILoadGroup> aLoadGroup;
      if (NS_SUCCEEDED(rv) && mailnewsUrl)
        mailnewsUrl->GetLoadGroup(getter_AddRefs(aLoadGroup));

      rv = NewChannel(url, getter_AddRefs(aChannel));
      if (NS_FAILED(rv)) return rv;

      rv = aChannel->SetLoadGroup(aLoadGroup);
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
#ifdef DEBUG_mscott
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
		  rv = mailnewsUrl->SetSpec(urlSpec.get());

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
			rv = uri->SetSpec(urlSpec.get());

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
			rv = uri->SetSpec(urlSpec.get());
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
			rv = uri->SetSpec(urlSpec.get());
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
			rv = uri->SetSpec(urlSpec.get());
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
                rv = uri->SetSpec(urlSpec.get());
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
			rv = uri->SetSpec(urlSpec.get());
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
			rv = uri->SetSpec(urlSpec.get());
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
			rv = uri->SetSpec(urlSpec.get());
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

  aImapUrl->SetImapFolder(aMsgFolder);

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
		  rv = uri->SetSpec(urlSpec.get());
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
			rv = uri->SetSpec(urlSpec.get());
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
				rv = uri->SetSpec(urlSpec.get());

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
            if (folderPath && (nsCRT::strlen(folderPath) > 0))
            {
                nsCOMPtr<nsIURI> uri = do_QueryInterface(aImapUrl);
                urlSpec.Append("/discoverlevelchildren>");
                urlSpec.AppendInt(level);
                urlSpec.AppendWithConversion(hierarchySeparator); // hierarchySeparator "/"
                urlSpec.Append(folderPath);

				rv = uri->SetSpec(urlSpec.get());
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
        folderName.Adopt(nsCRT::strdup(""));
        GetFolderName(aDstFolder, getter_Copies(folderName));
        urlSpec.Append((const char *) folderName);

    		rv = uri->SetSpec(urlSpec.get());
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
        PRUint32 curOfflineStorePos = 0;
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
              fakeHdr->SetMessageOffset(curOfflineStorePos);
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

        rv = uri->SetSpec(urlSpec.get());
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
  {
      *aURL = msgUrl;
      NS_IF_ADDREF(*aURL);
  }

  if (NS_SUCCEEDED(rv) && aMsgIncomingServer)
  {
    nsCOMPtr<nsIImapIncomingServer> aImapServer(do_QueryInterface(aMsgIncomingServer, &rv));
    if (NS_SUCCEEDED(rv) && aImapServer)
      rv = aImapServer->GetImapConnectionAndLoadUrl(aClientEventQueue,
                                                        aImapUrl,
                                                        aConsumer);
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
            rv = uri->SetSpec(urlSpec.get());
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
            urlSpec.AppendWithConversion(hierarchySeparator);
            urlSpec.Append((const char *) folderName);
            urlSpec.Append('>');
            urlSpec.AppendWithConversion(hierarchySeparator);

			char *utfNewName = CreateUtf7ConvertedStringFromUnicode( newLeafName);

			nsCAutoString cStrFolderName(NS_STATIC_CAST(const char *, folderName));
      // Unescape the name before looking for parent path
      nsUnescape(NS_CONST_CAST(char*, cStrFolderName.get()));
      PRInt32 leafNameStart = 
            cStrFolderName.RFindChar(hierarchySeparator);
            if (leafNameStart != -1)
            {
                cStrFolderName.SetLength(leafNameStart+1);
                urlSpec.Append(cStrFolderName);
            }
            char *escapedNewName = nsEscape(utfNewName, url_Path);
            if (!escapedNewName) return NS_ERROR_OUT_OF_MEMORY;
            nsXPIDLCString escapedSlashName;
            rv = nsImapUrl::EscapeSlashes((const char *) escapedNewName, getter_Copies(escapedSlashName));
            if (!escapedSlashName) return NS_ERROR_OUT_OF_MEMORY;
            urlSpec.Append(escapedSlashName.get());
            nsCRT::free(escapedNewName);
			nsCRT::free(utfNewName);

            rv = uri->SetSpec(urlSpec.get());
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
            urlSpec.AppendWithConversion(hierarchySeparator);
            if ((const char *) folderName && nsCRT::strlen(folderName) > 0)
            {
              nsXPIDLCString canonicalName;

              nsImapUrl::ConvertToCanonicalFormat(folderName, (char) hierarchySeparator, getter_Copies(canonicalName));
              urlSpec.Append((const char *) canonicalName);
              urlSpec.AppendWithConversion(hierarchySeparator);
            }
	    char *utfNewName = CreateUtf7ConvertedStringFromUnicode( newFolderName);
	    char *escapedFolderName = nsEscape(utfNewName, url_Path);
            urlSpec.Append(escapedFolderName);
            nsCRT::free(escapedFolderName);
            nsCRT::free(utfNewName);
    
            rv = uri->SetSpec(urlSpec.get());
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
    
            rv = uri->SetSpec(urlSpec.get());
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
            rv = uri->SetSpec(urlSpec.get());
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
	NS_MsgSACat(&returnString, formatString);;
	
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

NS_IMETHODIMP nsImapService::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD;
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

NS_IMETHODIMP nsImapService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
  nsCOMPtr<nsIImapUrl> aImapUrl;
	nsresult rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull, NS_GET_IID(nsIImapUrl), getter_AddRefs(aImapUrl));
  if (NS_SUCCEEDED(rv))
  {
    // now extract lots of fun information...
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl);
    nsCAutoString unescapedSpec(aSpec);
    // nsUnescape(NS_CONST_CAST(char*, unescapedSpec.get()));
    mailnewsUrl->SetSpec(unescapedSpec.get()); // set the url spec...
    
    nsXPIDLCString userName;
    nsXPIDLCString hostName;
    nsXPIDLCString folderName;
    
    // extract the user name and host name information...
    rv = mailnewsUrl->GetHost(getter_Copies(hostName));
    if (NS_FAILED(rv)) return rv;
    rv = mailnewsUrl->GetPreHost(getter_Copies(userName));
    if (NS_FAILED(rv)) return rv;

    if ((const char *) userName)
      nsUnescape(NS_CONST_CAST(char*,(const char*)userName));

    // if we can't get a folder name out of the url then I think this is an error
    aImapUrl->CreateCanonicalSourceFolderPathString(getter_Copies(folderName));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
             do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
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

    *_retval = mockChannel;
    NS_IF_ADDREF(*_retval);

    return rv;
}

NS_IMETHODIMP
nsImapService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    nsresult rv;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = prefs->SetFilePref(PREF_MAIL_ROOT_IMAP, aPath, PR_FALSE /* set default */);
    return rv;
}       

NS_IMETHODIMP
nsImapService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    
    nsresult rv;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_FAILED(rv)) return rv;
    
    PRBool havePref = PR_FALSE;
    nsCOMPtr<nsILocalFile> prefLocal;
    nsCOMPtr<nsIFile> localFile;
    rv = prefs->GetFileXPref(PREF_MAIL_ROOT_IMAP, getter_AddRefs(prefLocal));
    if (NS_SUCCEEDED(rv)) {
        localFile = prefLocal;
        havePref = PR_TRUE;
    }
    if (!localFile) {
        rv = NS_GetSpecialDirectory(NS_APP_IMAP_MAIL_50_DIR, getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
        havePref = PR_FALSE;
    }
        
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) {
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
    }
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsXPIDLCString pathBuf;
    rv = localFile->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpec(getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    outSpec->SetNativePath(pathBuf);
    
    if (!havePref || !exists)
        rv = SetDefaultLocalPath(outSpec);
        
    *aResult = outSpec;
    NS_IF_ADDREF(*aResult);
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
    nsCOMPtr<nsIEventQueueService> pEventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;

  // Locate the folder so that the correct hierarchical delimiter is used in the folder
  // pathnames, otherwise root's (ie, '^') is used and this is wrong.
  nsCOMPtr<nsIMsgFolder> msgFolder;
  nsCOMPtr<nsIFolder> subFolder;
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


    rv = rootMsgFolder->FindSubFolder(changedStr.get(), getter_AddRefs(subFolder));
    if (NS_SUCCEEDED(rv))
      msgFolder = do_QueryInterface(subFolder);
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
            rv = uri->SetSpec(urlSpec.get());
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
            rv = uri->SetSpec(urlSpec.get());
            if (NS_SUCCEEDED(rv))
                rv = GetImapConnectionAndLoadUrl(eventQueue, imapUrl,
                                                 nsnull, url);
        }
    }
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
                            aMsgWindow, getter_AddRefs(runningURI), nsnull, messageIds, PR_TRUE);
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
    return downloadForOffline->ProcessNextOperation();

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
