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
 *   philip zhao <philip.zhao@sun.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

#include "prsystem.h"

#include "nsMessenger.h"

// xpcom
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsFileStream.h"
#include "nsIStringStream.h"
#include "nsEscape.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIFileSpec.h"
#include "nsILocalFile.h"
#include "nsISupportsObsolete.h"
#if defined(XP_MAC) || defined(XP_MACOSX)
#include "nsIAppleFileDecoder.h"
#if defined(XP_MACOSX)
#include "nsILocalFileMac.h"
#include "MoreFilesX.h"
#endif
#endif
#include "nsNativeCharsetUtils.h"

// necko
#include "nsMimeTypes.h"
#include "nsIURL.h"
#include "nsIPrompt.h"
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
#include "nsIContentViewer.h" 
#include "nsIWebShell.h" 

// embedding
#include "nsIWebBrowserPrint.h"

/* for access to docshell */
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIWebNavigation.h"

// mail
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgUtils.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"

#include "nsIMsgFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgIncomingServer.h"

#include "nsIMsgMessageService.h"

#include "nsIMsgStatusFeedback.h"
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

// undo
#include "nsITransaction.h"
#include "nsMsgTxn.h"

// charset conversions
#include "nsMsgMimeCID.h"
#include "nsIMimeConverter.h"

// Printing
#include "nsMsgPrintEngine.h"

// Save As
#include "nsIFilePicker.h"
#include "nsIStringBundle.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"
#include "nsIMIMEService.h"
#include "nsIDownload.h"

#include "nsILinkHandler.h"                                                                              

static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgSendLaterCID, NS_MSGSENDLATER_CID); 

#define FOUR_K 4096
#define MESSENGER_SAVE_DIR_PREF_NAME "messenger.save.dir"
#define MAILNEWS_ALLOW_PLUGINS_PREF_NAME "mailnews.message_display.allow.plugins"

//
// Convert an nsString buffer to plain text...
//
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsICharsetConverterManager.h"
#include "nsIContentSink.h"
#include "nsIHTMLToTextSink.h"

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

static nsresult
ConvertBufToPlainText(nsString &aConBuf)
{
  nsresult    rv;
  nsAutoString    convertedText;
  nsCOMPtr<nsIParser> parser;

  if (aConBuf.IsEmpty())
    return NS_OK;

  rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, 
                                          NS_GET_IID(nsIParser), getter_AddRefs(parser));
  if (NS_SUCCEEDED(rv) && parser)
  {
    nsCOMPtr<nsIContentSink> sink;

    sink = do_CreateInstance(NS_PLAINTEXTSINK_CONTRACTID);
    NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);

    nsCOMPtr<nsIHTMLToTextSink> textSink(do_QueryInterface(sink));
    NS_ENSURE_TRUE(textSink, NS_ERROR_FAILURE);

    textSink->Initialize(&convertedText, 0, 72);

    parser->SetContentSink(sink);

    parser->Parse(aConBuf, 0, NS_LITERAL_CSTRING("text/html"), PR_FALSE, PR_TRUE);

    //
    // Now if we get here, we need to get from ASCII text to 
    // UTF-8 format or there is a problem downstream...
    //
    if (NS_SUCCEEDED(rv))
    {
      aConBuf = convertedText;
    }
  }

  return rv;
}

nsresult ConvertAndSanitizeFileName(const char * displayName, PRUnichar ** unicodeResult, char ** result)
{
  nsCAutoString unescapedName(displayName);

  /* we need to convert the UTF-8 fileName to platform specific character set.
     The display name is in UTF-8 because it has been escaped from JS
  */ 
  NS_UnescapeURL(unescapedName);
  NS_ConvertUTF8toUCS2 ucs2Str(unescapedName);

  nsresult rv = NS_OK;
#if defined(XP_MAC)  /* reviewed for 1.4, XP_MACOSX not needed */
  /* We need to truncate the name to 31 characters, this even on MacOS X until the file API
     correctly support long file name. Using a nsILocalFile will do the trick...
  */
  nsCOMPtr<nsILocalFile> aLocalFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(aLocalFile->SetLeafName(ucs2Str)))
  {
    aLocalFile->GetLeafName(ucs2Str);
  }
#endif

  // replace platform specific path separator and illegale characters to avoid any confusion
  ucs2Str.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '-');

  if (result) {
    nsCAutoString nativeStr;
    rv =  nsMsgI18NCopyUTF16ToNative(ucs2Str, nativeStr);
    *result = ToNewCString(nativeStr);
  }

  if (unicodeResult)
    *unicodeResult = ToNewUnicode(ucs2Str);

 return rv;
}

// ***************************************************
// jefft - this is a rather obscured class serves for Save Message As File,
// Save Message As Template, and Save Attachment to a file
// 
class nsSaveAllAttachmentsState;

class nsSaveMsgListener : public nsIUrlListener,
                          public nsIMsgCopyServiceListener,
                          public nsIStreamListener,
                          public nsIObserver
{
public:
    nsSaveMsgListener(nsIFileSpec* fileSpec, nsMessenger* aMessenger);
    virtual ~nsSaveMsgListener();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIURLLISTENER
    NS_DECL_NSIMSGCOPYSERVICELISTENER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIOBSERVER

    nsCOMPtr<nsIFileSpec> m_fileSpec;
    nsCOMPtr<nsIOutputStream> m_outputStream;
    char *m_dataBuffer;
    nsCOMPtr<nsIChannel> m_channel;
    nsXPIDLCString m_templateUri;
    nsMessenger *m_messenger; // not ref counted
    nsSaveAllAttachmentsState *m_saveAllAttachmentsState;

    // rhp: For character set handling
    PRBool        m_doCharsetConversion;
    nsString      m_charset;
    enum {
	    eUnknown,
	    ePlainText,
	    eHTML
    }             m_outputFormat;
    nsString      m_msgBuffer;

    nsCString     m_contentType;    // used only when saving attachment

    nsCOMPtr<nsIWebProgressListener> mWebProgressListener;
    PRInt32 mProgress;
    PRInt32 mContentLength; 
    PRBool  mCanceled;
    PRBool  mInitialized;
    nsresult InitializeDownload(nsIRequest * aRequest, PRInt32 aBytesDownloaded);
};

class nsSaveAllAttachmentsState
{
public:
    nsSaveAllAttachmentsState(PRUint32 count,
                              const char **contentTypeArray,
                              const char **urlArray,
                              const char **displayNameArray,
                              const char **messageUriArray,
                              const char *directoryName);
    virtual ~nsSaveAllAttachmentsState();

    PRUint32 m_count;
    PRUint32 m_curIndex;
    char* m_directoryName;
    char** m_contentTypeArray;
    char** m_urlArray;
    char** m_displayNameArray;
    char** m_messageUriArray;
};

//
// nsMessenger
//
nsMessenger::nsMessenger() 
{
  mScriptObject = nsnull;
  mMsgWindow = nsnull;
  mStringBundle = nsnull;
  mSendingUnsentMsgs = PR_FALSE;
  //	InitializeFolderRoot();
}

nsMessenger::~nsMessenger()
{
    // Release search context.
    mSearchContext = nsnull;
}


NS_IMPL_ISUPPORTS3(nsMessenger, nsIMessenger, nsIObserver, nsISupportsWeakReference)
NS_IMPL_GETSET(nsMessenger, SendingUnsentMsgs, PRBool, mSendingUnsentMsgs)

NS_IMETHODIMP    
nsMessenger::SetWindow(nsIDOMWindowInternal *aWin, nsIMsgWindow *aMsgWindow)
{
  nsCOMPtr<nsIPrefBranchInternal> pbi = do_GetService(NS_PREFSERVICE_CONTRACTID);

  if(!aWin)
  {
    // it isn't an error to pass in null for aWin, in fact it means we are shutting
    // down and we should start cleaning things up...
    
    if (mMsgWindow)
    {
      nsCOMPtr<nsIMsgStatusFeedback> aStatusFeedback;
      
      mMsgWindow->GetStatusFeedback(getter_AddRefs(aStatusFeedback));
      if (aStatusFeedback)
        aStatusFeedback->SetDocShell(nsnull, nsnull);
      
      // Remove pref observer
      if (pbi)
        pbi->RemoveObserver(MAILNEWS_ALLOW_PLUGINS_PREF_NAME, this);
    }
    
    return NS_OK;
  }
  
  mMsgWindow = aMsgWindow;
  
  mWindow = aWin;
  
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  NS_ENSURE_TRUE(globalObj, NS_ERROR_FAILURE);

  nsIDocShell *docShell = globalObj->GetDocShell();
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDocShellTreeItem> rootDocShellAsItem;
  docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(rootDocShellAsItem));
  
  nsCOMPtr<nsIDocShellTreeNode> rootDocShellAsNode(do_QueryInterface(rootDocShellAsItem));

  if (rootDocShellAsNode) 
  {
    nsCOMPtr<nsIDocShellTreeItem> childAsItem;
    nsresult rv = rootDocShellAsNode->FindChildWithName(NS_LITERAL_STRING("messagepane").get(),
      PR_TRUE, PR_FALSE, nsnull, getter_AddRefs(childAsItem));
    
    mDocShell = do_QueryInterface(childAsItem);
    
    if (NS_SUCCEEDED(rv) && mDocShell) {
      mCurrentDisplayCharset = ""; // Important! Clear out mCurrentDisplayCharset so we reset a default charset on mDocshell the next time we try to load something into it.
      
      if (aMsgWindow) 
      {
        nsCOMPtr<nsIMsgStatusFeedback> aStatusFeedback;
        
        aMsgWindow->GetStatusFeedback(getter_AddRefs(aStatusFeedback));
        if (aStatusFeedback)
          aStatusFeedback->SetDocShell(mDocShell, mWindow);

        aMsgWindow->GetTransactionManager(getter_AddRefs(mTxnMgr));
        
        // Add pref observer
        if (pbi)
          pbi->AddObserver(MAILNEWS_ALLOW_PLUGINS_PREF_NAME, this, PR_TRUE);
        SetDisplayProperties();
      }
    }
  }
  
  // we don't always have a message pane, like in the addressbook
  // so if we don't havea docshell, use the one for the xul window.
  // we do this so OpenURL() will work.
  if (!mDocShell)
    mDocShell = docShell;
  
  return NS_OK;
}

NS_IMETHODIMP nsMessenger::SetDisplayCharset(const char * aCharset)
{
  if (mCurrentDisplayCharset.Equals(aCharset))
    return NS_OK;

  // libmime always converts to UTF-8 (both HTML and XML)
  if (mDocShell) 
  {
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);
      if (muDV) 
        muDV->SetForceCharacterSet(nsDependentCString(aCharset));

      mCurrentDisplayCharset = aCharset;
    }
  }

  return NS_OK;
}

nsresult
nsMessenger::SetDisplayProperties()
{
  // For now, the only property we will set is allowPlugins but we might do more in the future...

  nsresult rv;

  if (!mDocShell)
    return NS_ERROR_FAILURE;
 
  PRBool allowPlugins = PR_FALSE;

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    prefBranch->GetBoolPref(MAILNEWS_ALLOW_PLUGINS_PREF_NAME, &allowPlugins);
  
  return mDocShell->SetAllowPlugins(allowPlugins);
}

NS_IMETHODIMP
nsMessenger::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID))
  {
    nsDependentString prefName(aData);
    if (prefName.EqualsLiteral(MAILNEWS_ALLOW_PLUGINS_PREF_NAME))
      SetDisplayProperties();
  }

  return NS_OK;
}

nsresult
nsMessenger::PromptIfFileExists(nsFileSpec &fileSpec)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (fileSpec.Exists())
    {
        nsCOMPtr<nsIPrompt> dialog(do_GetInterface(mDocShell));
        if (!dialog) return rv;
        nsAutoString path;
        PRBool dialogResult = PR_FALSE;
        nsXPIDLString errorMessage;

        nsMsgI18NCopyNativeToUTF16(fileSpec.GetNativePathCString(), path);
        const PRUnichar *pathFormatStrings[] = { path.get() };

        if (!mStringBundle)
        {
            rv = InitStringBundle();
            if (NS_FAILED(rv)) return rv;
        }
        rv = mStringBundle->FormatStringFromName(NS_LITERAL_STRING("fileExists").get(),
                                                 pathFormatStrings, 1,
                                                 getter_Copies(errorMessage));
        if (NS_FAILED(rv)) return rv;
        rv = dialog->Confirm(nsnull, errorMessage, &dialogResult);
        if (NS_FAILED(rv)) return rv;

        if (dialogResult)
        {
            return NS_OK; // user says okay to replace
        }
        else
        {
            PRInt16 dialogReturn;
            nsCOMPtr<nsIFilePicker> filePicker =
                do_CreateInstance("@mozilla.org/filepicker;1", &rv);
            if (NS_FAILED(rv)) return rv;

            filePicker->Init(mWindow,
                             GetString(NS_LITERAL_STRING("SaveAttachment")),
                             nsIFilePicker::modeSave);
            filePicker->SetDefaultString(path);
            filePicker->AppendFilters(nsIFilePicker::filterAll);
            
            nsCOMPtr <nsILocalFile> lastSaveDir;
            rv = GetLastSaveDirectory(getter_AddRefs(lastSaveDir));
            if (NS_SUCCEEDED(rv) && lastSaveDir) {
              filePicker->SetDisplayDirectory(lastSaveDir);
            }

            rv = filePicker->Show(&dialogReturn);
            if (NS_FAILED(rv) || dialogReturn == nsIFilePicker::returnCancel) {
                // XXX todo
                // don't overload the return value like this
                // change this function to have an out boolean
                // that we check to see if the user cancelled
                return NS_ERROR_FAILURE;
            }

            nsCOMPtr<nsILocalFile> localFile;
            nsCAutoString filePath;

            rv = filePicker->GetFile(getter_AddRefs(localFile));
            if (NS_FAILED(rv)) return rv;

            rv = SetLastSaveDirectory(localFile);
            NS_ENSURE_SUCCESS(rv,rv);

            rv = localFile->GetNativePath(filePath);
            if (NS_FAILED(rv)) return rv;

            fileSpec = filePath.get();
            return NS_OK;
        }
    }
    else
    {
        return NS_OK;
    }
    return rv;
}

NS_IMETHODIMP
nsMessenger::OpenURL(const char *aURL)
{
  NS_ENSURE_ARG_POINTER(aURL);

  // This is to setup the display DocShell as UTF-8 capable...
  SetDisplayCharset("UTF-8");
  
  char *unescapedUrl = PL_strdup(aURL);
  if (!unescapedUrl)
    return NS_ERROR_OUT_OF_MEMORY;
  
  // I don't know why we're unescaping this url - I'll leave it unescaped
  // for the web shell, but the message service doesn't need it unescaped.
  nsUnescape(unescapedUrl);
  
  nsCOMPtr <nsIMsgMessageService> messageService;
  nsresult rv = GetMessageServiceFromURI(aURL, getter_AddRefs(messageService));
  
  if (NS_SUCCEEDED(rv) && messageService)
  {
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
    messageService->DisplayMessage(aURL, webShell, mMsgWindow, nsnull, nsnull, nsnull);
    mLastDisplayURI = aURL; // remember the last uri we displayed....
  }
  //If it's not something we know about, then just load the url.
  else
  {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
    if(webNav)
      rv = webNav->LoadURI(NS_ConvertASCIItoUCS2(unescapedUrl).get(), // URI string
      nsIWebNavigation::LOAD_FLAGS_NONE,  // Load flags
      nsnull,                             // Referring URI
      nsnull,                             // Post stream
      nsnull);                            // Extra headers
  }
  PL_strfree(unescapedUrl);
  return rv;
}

NS_IMETHODIMP nsMessenger::LaunchExternalURL(const char * aURL)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIExternalProtocolService> extProtService = do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  return extProtService->LoadUrl(uri); 
}

NS_IMETHODIMP
nsMessenger::LoadURL(nsIDOMWindowInternal *aWin, const char *aURL)
{
  NS_ENSURE_ARG_POINTER(aURL);
  
  SetDisplayCharset("UTF-8");
  
  NS_ConvertASCIItoUTF16 uriString(aURL);
  // Cleanup the empty spaces that might be on each end.
  uriString.Trim(" ");
  // Eliminate embedded newlines, which single-line text fields now allow:
  uriString.StripChars("\r\n");
  NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIMsgMailNewsUrl> msgurl = do_QueryInterface(uri);
  if (msgurl)
    msgurl->SetMsgWindow(mMsgWindow);
  
  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  rv = mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  loadInfo->SetLoadType(nsIDocShellLoadInfo::loadNormal);
  return mDocShell->LoadURI(uri, loadInfo, 0, PR_TRUE);
}

nsresult
nsMessenger::SaveAttachment(nsIFileSpec * fileSpec,
                            const char * unescapedUrl,
                            const char * messageUri,
                            const char * contentType,
                            void *closure)
{
  nsIMsgMessageService * messageService = nsnull;
  nsSaveAllAttachmentsState *saveState= (nsSaveAllAttachmentsState*) closure;
  nsCOMPtr<nsIMsgMessageFetchPartService> fetchService;
  nsCAutoString urlString;
  nsCOMPtr<nsIURI> URL;
  nsCAutoString fullMessageUri(messageUri);

  // XXX todo
  // document the ownership model of saveListener
  // whacky ref counting here...what's the deal? when does saveListener get released? it's not clear.
  nsSaveMsgListener *saveListener = new nsSaveMsgListener(fileSpec, this);
  if (!saveListener)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(saveListener);

  saveListener->m_contentType = contentType;
  if (saveState)
      saveListener->m_saveAllAttachmentsState = saveState;

  urlString = unescapedUrl;
  urlString.ReplaceSubstring("/;section", "?section");
  nsresult rv = CreateStartupUrl(urlString.get(), getter_AddRefs(URL));

  if (NS_SUCCEEDED(rv))
  {
    rv = GetMessageServiceFromURI(messageUri, &messageService);
    if (NS_SUCCEEDED(rv))
    {
      fetchService = do_QueryInterface(messageService);
      // if the message service has a fetch part service then we know we can fetch mime parts...
      if (fetchService)
      {
        PRInt32 sectionPos = urlString.Find("?section");
        nsCString mimePart;
        urlString.Right(mimePart, urlString.Length() - sectionPos);
        fullMessageUri.Append(mimePart);
   
        messageUri = fullMessageUri.get();
      }

      nsCOMPtr<nsIStreamListener> convertedListener;
      saveListener->QueryInterface(NS_GET_IID(nsIStreamListener),
                                 getter_AddRefs(convertedListener));

#if !defined(XP_MAC) && !defined(XP_MACOSX)
      // if the content type is bin hex we are going to do a hokey hack and make sure we decode the bin hex 
      // when saving an attachment to disk..
      if (contentType && !nsCRT::strcasecmp(APPLICATION_BINHEX, contentType))
      {
        nsCOMPtr<nsIStreamListener> listener (do_QueryInterface(convertedListener));
        nsCOMPtr<nsIStreamConverterService> streamConverterService = do_GetService("@mozilla.org/streamConverters;1", &rv);
        nsCOMPtr<nsISupports> channelSupport = do_QueryInterface(saveListener->m_channel);
          
        rv = streamConverterService->AsyncConvertData(APPLICATION_BINHEX,
                                                      "*/*", 
                                                      listener,
                                                      channelSupport,
                                                      getter_AddRefs(convertedListener));
      }
#endif
      if (fetchService)
        rv = fetchService->FetchMimePart(URL, messageUri, convertedListener, mMsgWindow, nsnull,nsnull);
      else
        rv = messageService->DisplayMessage(messageUri, convertedListener, mMsgWindow, nsnull, nsnull, nsnull); 
    } // if we got a message service
  } // if we created a url

  if (NS_FAILED(rv))
  {
      NS_IF_RELEASE(saveListener);
      Alert("saveAttachmentFailed");
  }
	return rv;
}

NS_IMETHODIMP
nsMessenger::OpenAttachment(const char * aContentType, const char * aURL, const
                            char * aDisplayName, const char * aMessageUri)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsIMsgMessageService> messageService;
  rv = GetMessageServiceFromURI(aMessageUri, getter_AddRefs(messageService));
  if (messageService)
  {
    rv = messageService->OpenAttachment(aContentType, aDisplayName, aURL, aMessageUri, mDocShell, mMsgWindow, nsnull);
  }
	return rv;
}

NS_IMETHODIMP
nsMessenger::SaveAttachmentToFolder(const char * contentType, const char * url, const char * displayName, 
                                    const char * messageUri, nsILocalFile * aDestFolder, nsILocalFile ** aOutFile)
{
  nsresult rv = NS_OK;
  nsXPIDLCString unescapedFileName;
  
  rv = ConvertAndSanitizeFileName(displayName, nsnull, getter_Copies(unescapedFileName));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFileSpec> fileSpec;

  rv = NS_NewFileSpecFromIFile(aDestFolder, getter_AddRefs(fileSpec));
  NS_ENSURE_SUCCESS(rv, rv);

  fileSpec->SetLeafName(unescapedFileName);

  // ok now we have a file spec for the destination
  nsCAutoString unescapedUrl;
  unescapedUrl.Assign(url);

  nsUnescape(unescapedUrl.BeginWriting());
  rv = SaveAttachment(fileSpec, unescapedUrl.get(), messageUri, contentType, nsnull);

  // before we return, we need to convert our file spec back to a nsIFile to return to the caller
  nsCOMPtr<nsILocalFile> outputFile;
  nsFileSpec actualSpec;
  fileSpec->GetFileSpec(&actualSpec);
  NS_FileSpecToIFile(&actualSpec, getter_AddRefs(outputFile));

  NS_IF_ADDREF(*aOutFile = outputFile);

  return rv;
}

NS_IMETHODIMP
nsMessenger::SaveAttachment(const char * contentType, const char * url,
                            const char * displayName, const char * messageUri)
{
  NS_ENSURE_ARG_POINTER(url);

#ifdef DEBUG_MESSENGER
  printf("nsMessenger::SaveAttachment(%s)\n",url);
#endif    

  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  char *unescapedUrl = nsnull;
  nsCOMPtr<nsIFilePicker> filePicker =
      do_CreateInstance("@mozilla.org/filepicker;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt16 dialogResult;
  nsCOMPtr<nsILocalFile> localFile;
  nsCOMPtr<nsILocalFile> lastSaveDir;
  nsCOMPtr<nsIFileSpec> fileSpec;
  nsXPIDLCString filePath;

  unescapedUrl = PL_strdup(url);
  if (!unescapedUrl)
    return NS_ERROR_OUT_OF_MEMORY;

  nsUnescape(unescapedUrl);
  
  nsXPIDLString defaultDisplayString;
  rv = ConvertAndSanitizeFileName(displayName, getter_Copies(defaultDisplayString), nsnull);
  if (NS_FAILED(rv)) goto done;

  filePicker->Init(mWindow, GetString(NS_LITERAL_STRING("SaveAttachment")),
                   nsIFilePicker::modeSave);
  filePicker->SetDefaultString(defaultDisplayString);
  filePicker->AppendFilters(nsIFilePicker::filterAll);
  
  rv = GetLastSaveDirectory(getter_AddRefs(lastSaveDir));
  if (NS_SUCCEEDED(rv) && lastSaveDir) {
    filePicker->SetDisplayDirectory(lastSaveDir);
  }

  rv = filePicker->Show(&dialogResult);
  if (NS_FAILED(rv) || dialogResult == nsIFilePicker::returnCancel)
    goto done;

  rv = filePicker->GetFile(getter_AddRefs(localFile));
  if (NS_FAILED(rv)) goto done;
  
  (void)SetLastSaveDirectory(localFile);

  rv = NS_NewFileSpecFromIFile(localFile, getter_AddRefs(fileSpec));
  if (NS_FAILED(rv)) goto done;

  rv = SaveAttachment(fileSpec, unescapedUrl, messageUri, contentType, nsnull);

done:
  PR_FREEIF(unescapedUrl);
  return rv;
}


NS_IMETHODIMP
nsMessenger::SaveAllAttachments(PRUint32 count,
                                const char **contentTypeArray,
                                const char **urlArray,
                                const char **displayNameArray,
                                const char **messageUriArray)
{
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    nsCOMPtr<nsIFilePicker> filePicker =
        do_CreateInstance("@mozilla.org/filepicker;1", &rv);
    nsCOMPtr<nsILocalFile> localFile;
    nsCOMPtr<nsILocalFile> lastSaveDir;
    nsCOMPtr<nsIFileSpec> fileSpec;
    nsXPIDLCString dirName;
    nsSaveAllAttachmentsState *saveState = nsnull;
    PRInt16 dialogResult;
    char *unescapedUrl = nsnull;

    if (NS_FAILED(rv)) goto done;

    filePicker->Init(mWindow,
                     GetString(NS_LITERAL_STRING("SaveAllAttachments")),
                     nsIFilePicker::modeGetFolder);

    rv = GetLastSaveDirectory(getter_AddRefs(lastSaveDir));
    if (NS_SUCCEEDED(rv) && lastSaveDir) {
      filePicker->SetDisplayDirectory(lastSaveDir);
    }
    
    rv = filePicker->Show(&dialogResult);
    if (NS_FAILED(rv) || dialogResult == nsIFilePicker::returnCancel)
        goto done;

    rv = filePicker->GetFile(getter_AddRefs(localFile));
    if (NS_FAILED(rv)) goto done;

    rv = SetLastSaveDirectory(localFile);
    if (NS_FAILED(rv)) 
      goto done;

    rv = localFile->GetNativePath(dirName);
    if (NS_FAILED(rv)) goto done;
    rv = NS_NewFileSpec(getter_AddRefs(fileSpec));
    if (NS_FAILED(rv)) goto done;

    saveState = new nsSaveAllAttachmentsState(count,
                                              contentTypeArray,
                                              urlArray,
                                              displayNameArray,
                                              messageUriArray, 
                                              (const char*) dirName);
    {
        nsFileSpec aFileSpec((const char *) dirName);
        unescapedUrl = PL_strdup(urlArray[0]);
        nsUnescape(unescapedUrl);

        nsXPIDLCString unescapedName;
        rv = ConvertAndSanitizeFileName(displayNameArray[0], nsnull, getter_Copies(unescapedName));
        if (NS_FAILED(rv))
          goto done;

        aFileSpec += unescapedName.get();
        rv = PromptIfFileExists(aFileSpec);
        if (NS_FAILED(rv)) return rv;
        fileSpec->SetFromFileSpec(aFileSpec);
        rv = SaveAttachment(fileSpec, unescapedUrl, messageUriArray[0], 
                            contentTypeArray[0], (void *)saveState);
    }
done:

    PR_FREEIF (unescapedUrl);

    return rv;
}

enum MESSENGER_SAVEAS_FILE_TYPE 
{
 EML_FILE_TYPE =  0,
 HTML_FILE_TYPE = 1,
 TEXT_FILE_TYPE = 2,
 ANY_FILE_TYPE =  3
};
#define HTML_FILE_EXTENSION ".htm" 
#define HTML_FILE_EXTENSION2 ".html"
#define TEXT_FILE_EXTENSION ".txt"
#define EML_FILE_EXTENSION  ".eml"

NS_IMETHODIMP
nsMessenger::SaveAs(const char *aURI, PRBool aAsFile, nsIMsgIdentity *aIdentity, const PRUnichar *aMsgFilename)
{
  NS_ENSURE_ARG_POINTER(aURI);
  
  nsCOMPtr<nsIMsgMessageService> messageService;
  nsCOMPtr<nsIUrlListener> urlListener;
  nsSaveMsgListener *saveListener = nsnull;
  nsCOMPtr<nsIURI> url;
  nsCOMPtr<nsIStreamListener> convertedListener;
  PRInt32 saveAsFileType = EML_FILE_TYPE;
  
  nsresult rv = GetMessageServiceFromURI(aURI, getter_AddRefs(messageService));
  if (NS_FAILED(rv)) 
    goto done;
  
  if (aAsFile) 
  {
    nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1", &rv);
    if (NS_FAILED(rv)) 
      goto done;

    filePicker->Init(mWindow, GetString(NS_LITERAL_STRING("SaveMailAs")),
                     nsIFilePicker::modeSave);

    // if we have a non-null filename use it, otherwise use default save message one
    if (aMsgFilename)    
      filePicker->SetDefaultString(nsDependentString(aMsgFilename));
    else {
      filePicker->SetDefaultString(GetString(NS_LITERAL_STRING("defaultSaveMessageAsFileName")));
    }

    // because we will be using GetFilterIndex()
    // we must call AppendFilters() one at a time, 
    // in MESSENGER_SAVEAS_FILE_TYPE order
    filePicker->AppendFilter(GetString(NS_LITERAL_STRING("EMLFiles")),
                             NS_LITERAL_STRING("*.eml"));
    filePicker->AppendFilters(nsIFilePicker::filterHTML);     
    filePicker->AppendFilters(nsIFilePicker::filterText);
    filePicker->AppendFilters(nsIFilePicker::filterAll);
    
    // by default, set the filter type as "all"
    // because the default string is "message.eml"
    // and type is "all", we will use the filename extension, and will save as rfc/822 (.eml)
    // but if the user just changes the name (to .html or .txt), we will save as those types
    // if the type is "all".
    // http://bugzilla.mozilla.org/show_bug.cgi?id=96134#c23
    filePicker->SetFilterIndex(ANY_FILE_TYPE);

    PRInt16 dialogResult;
    
    nsCOMPtr <nsILocalFile> lastSaveDir;
    rv = GetLastSaveDirectory(getter_AddRefs(lastSaveDir));
    if (NS_SUCCEEDED(rv) && lastSaveDir) 
    {
      filePicker->SetDisplayDirectory(lastSaveDir);
    }
    
    nsCOMPtr<nsILocalFile> localFile;
    nsAutoString fileName;
    rv = filePicker->Show(&dialogResult);
    if (NS_FAILED(rv) || dialogResult == nsIFilePicker::returnCancel)
      goto done;
    
    rv = filePicker->GetFile(getter_AddRefs(localFile));
    if (NS_FAILED(rv)) 
      goto done;

    if (dialogResult == nsIFilePicker::returnReplace) {
      // be extra safe and only delete when the file is really a file
      PRBool isFile;
      rv = localFile->IsFile(&isFile);
      if (NS_SUCCEEDED(rv) && isFile) {
        rv = localFile->Remove(PR_FALSE /* recursive delete */);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    
    rv = SetLastSaveDirectory(localFile);
    if (NS_FAILED(rv)) 
      goto done;
    
    rv = filePicker->GetFilterIndex(&saveAsFileType);
    if (NS_FAILED(rv)) 
      goto done;

    rv = localFile->GetLeafName(fileName);
    NS_ENSURE_SUCCESS(rv, rv);

    switch ( saveAsFileType )
    {
      // Add the right extenstion based on filter index and build a new localFile.
      case HTML_FILE_TYPE:
          if ( (fileName.RFind(HTML_FILE_EXTENSION, PR_TRUE, -1, sizeof(HTML_FILE_EXTENSION)-1) == kNotFound) &&
               (fileName.RFind(HTML_FILE_EXTENSION2, PR_TRUE, -1, sizeof(HTML_FILE_EXTENSION2)-1) == kNotFound) ) {
           fileName.AppendLiteral(HTML_FILE_EXTENSION2);
           localFile->SetLeafName(fileName);
        }
        break;
      case TEXT_FILE_TYPE:
        if (fileName.RFind(TEXT_FILE_EXTENSION, PR_TRUE, -1, sizeof(TEXT_FILE_EXTENSION)-1) == kNotFound) {
         fileName.AppendLiteral(TEXT_FILE_EXTENSION);
         localFile->SetLeafName(fileName);
        }
        break;
      case EML_FILE_TYPE:
        if (fileName.RFind(EML_FILE_EXTENSION, PR_TRUE, -1, sizeof(EML_FILE_EXTENSION)-1) == kNotFound) {
         fileName.AppendLiteral(EML_FILE_EXTENSION);
         localFile->SetLeafName(fileName);
        }
        break;
      case ANY_FILE_TYPE:
      default:
        // If no extension found then default it to .eml. Otherwise, 
        // set the right file type based on the specified extension.
        PRBool noExtensionFound = PR_FALSE;
        if (fileName.RFind(".", 1) != kNotFound)
        {
          if ( (fileName.RFind(HTML_FILE_EXTENSION, PR_TRUE, -1, sizeof(HTML_FILE_EXTENSION)-1) != kNotFound) ||
               (fileName.RFind(HTML_FILE_EXTENSION2, PR_TRUE, -1, sizeof(HTML_FILE_EXTENSION2)-1) != kNotFound) )
            saveAsFileType = HTML_FILE_TYPE;
          else if (fileName.RFind(TEXT_FILE_EXTENSION, PR_TRUE, -1, sizeof(TEXT_FILE_EXTENSION)-1) != kNotFound)
            saveAsFileType = TEXT_FILE_TYPE;
          else if (fileName.RFind(EML_FILE_EXTENSION, PR_TRUE, -1, sizeof(EML_FILE_EXTENSION)-1) != kNotFound)
            saveAsFileType = EML_FILE_TYPE;
          else
            noExtensionFound = PR_TRUE;  
        } 
        else 
          noExtensionFound = PR_TRUE;
        // Set default file type here.
        if (noExtensionFound)
        {
          saveAsFileType = EML_FILE_TYPE; 
          fileName.AppendLiteral(EML_FILE_EXTENSION);
          localFile->SetLeafName(fileName);
        }
        break;
    }
   
    // XXX argh!  converting from nsILocalFile to nsFileSpec ... oh baby, lets drop from unicode to ascii too
    //        nsXPIDLString path;
    //        localFile->GetUnicodePath(getter_Copies(path));
    nsCOMPtr<nsIFileSpec> fileSpec;
    rv = NS_NewFileSpecFromIFile(localFile, getter_AddRefs(fileSpec));
    if (NS_FAILED(rv)) 
      goto done;
    
    // XXX todo
    // document the ownership model of saveListener
    saveListener = new nsSaveMsgListener(fileSpec, this);
    if (!saveListener) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    NS_ADDREF(saveListener);
    
    rv = saveListener->QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));
    if (NS_FAILED(rv)) 
      goto done;
    
    if (saveAsFileType == EML_FILE_TYPE) 
    {
      rv = messageService->SaveMessageToDisk(aURI, fileSpec, PR_FALSE,
        urlListener, nsnull,
        PR_FALSE, mMsgWindow);
    }
    else
    {      
      nsCAutoString urlString(aURI);
      
      // we can't go RFC822 to TXT until bug #1775 is fixed
      // so until then, do the HTML to TXT conversion in
      // nsSaveMsgListener::OnStopRequest(), see ConvertBufToPlainText()
      //
      // Setup the URL for a "Save As..." Operation...
      // For now, if this is a save as TEXT operation, then do
      // a "printing" operation
      if (saveAsFileType == TEXT_FILE_TYPE) 
      {
        saveListener->m_outputFormat = nsSaveMsgListener::ePlainText;
        saveListener->m_doCharsetConversion = PR_TRUE;
        urlString.AppendLiteral("?header=print");  
      }
      else 
      {
        saveListener->m_outputFormat = nsSaveMsgListener::eHTML;
        saveListener->m_doCharsetConversion = PR_FALSE;
        urlString.AppendLiteral("?header=saveas");  
      }
      
      rv = CreateStartupUrl(urlString.get(), getter_AddRefs(url));
      NS_ASSERTION(NS_SUCCEEDED(rv), "CreateStartupUrl failed");
      if (NS_FAILED(rv)) 
        goto done;
      
      saveListener->m_channel = nsnull;
      rv = NS_NewInputStreamChannel(getter_AddRefs(saveListener->m_channel),
        url, 
        nsnull);                // inputStream
      NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewInputStreamChannel failed");
      if (NS_FAILED(rv)) 
        goto done;
      
      nsCOMPtr<nsIStreamConverterService> streamConverterService = do_GetService("@mozilla.org/streamConverters;1");
      nsCOMPtr<nsISupports> channelSupport = do_QueryInterface(saveListener->m_channel);
      
      // we can't go RFC822 to TXT until bug #1775 is fixed
      // so until then, do the HTML to TXT conversion in
      // nsSaveMsgListener::OnStopRequest(), see ConvertBufToPlainText()
      rv = streamConverterService->AsyncConvertData(MESSAGE_RFC822,
        TEXT_HTML,
        saveListener,
        channelSupport,
        getter_AddRefs(convertedListener));
      NS_ASSERTION(NS_SUCCEEDED(rv), "AsyncConvertData failed");
      if (NS_FAILED(rv)) 
        goto done;
      
      rv = messageService->DisplayMessage(urlString.get(), convertedListener, mMsgWindow,
        nsnull, nsnull, nsnull);
    }
  }
  else
  {
    // ** save as Template
    nsCOMPtr<nsIFileSpec> fileSpec;
    nsFileSpec tmpFileSpec("nsmail.tmp");
    rv = NS_NewFileSpecWithSpec(tmpFileSpec, getter_AddRefs(fileSpec));
    if (NS_FAILED(rv)) goto done;
    
    // XXX todo
    // document the ownership model of saveListener
    saveListener = new nsSaveMsgListener(fileSpec, this);
    if (!saveListener) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    NS_ADDREF(saveListener);
    
    if (aIdentity)
      rv = aIdentity->GetStationeryFolder(getter_Copies(saveListener->m_templateUri));
    if (NS_FAILED(rv)) 
      goto done;

    PRBool needDummyHeader =
      PL_strcasestr(saveListener->m_templateUri, "mailbox://") 
      != nsnull;
    PRBool canonicalLineEnding =
      PL_strcasestr(saveListener->m_templateUri, "imap://")
      != nsnull;

    rv = saveListener->QueryInterface(
      NS_GET_IID(nsIUrlListener),
      getter_AddRefs(urlListener));
    if (NS_FAILED(rv)) 
      goto done;
    
    rv = messageService->SaveMessageToDisk(aURI, fileSpec, 
      needDummyHeader,
      urlListener, nsnull,
      canonicalLineEnding, mMsgWindow); 
  }

done:
  if (NS_FAILED(rv)) 
  {
    // XXX todo
    // document the ownership model of saveListener
    NS_IF_RELEASE(saveListener);
    Alert("saveMessageFailed");
  }
  return rv;
}

nsresult
nsMessenger::Alert(const char *stringName)
{
    nsresult rv = NS_OK;

    if (mDocShell)
    {
        nsCOMPtr<nsIPrompt> dialog(do_GetInterface(mDocShell));

        if (dialog) {
            rv = dialog->Alert(nsnull,
                               GetString(NS_ConvertASCIItoUCS2(stringName)).get());
        }
    }
    return rv;
}

nsresult
nsMessenger::DoCommand(nsIRDFCompositeDataSource* db, const nsACString& command,
                       nsISupportsArray *srcArray, 
                       nsISupportsArray *argumentArray)
{

	nsresult rv;

    nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
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

	rv = DoCommand(db, NS_LITERAL_CSTRING(NC_RDF_DELETE), parentArray, deletedArray);

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

  NS_ENSURE_ARG_POINTER(srcResource);
  NS_ENSURE_ARG_POINTER(dstResource);
  NS_ENSURE_ARG_POINTER(argumentArray);
	
  nsCOMPtr<nsIMsgFolder> srcFolder;
  nsCOMPtr<nsISupportsArray> folderArray;
    
  srcFolder = do_QueryInterface(srcResource);
  if(!srcFolder)
    return NS_ERROR_NO_INTERFACE;
  
  nsCOMPtr<nsISupports> srcFolderSupports(do_QueryInterface(srcFolder));
  if(srcFolderSupports)
    argumentArray->InsertElementAt(srcFolderSupports, 0);

  rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
  NS_ENSURE_SUCCESS(rv, rv);
  
  folderArray->AppendElement(dstResource);
  if (isMove)
    rv = DoCommand(database, NS_LITERAL_CSTRING(NC_RDF_MOVE), folderArray, argumentArray);
  else
    rv = DoCommand(database, NS_LITERAL_CSTRING(NC_RDF_COPY), folderArray, argumentArray);
  return rv;

}

NS_IMETHODIMP
nsMessenger::MessageServiceFromURI(const char *uri, nsIMsgMessageService **msgService)
{
   nsresult rv;
   NS_ENSURE_ARG_POINTER(uri);
   NS_ENSURE_ARG_POINTER(msgService);
 
   rv = GetMessageServiceFromURI(uri, msgService);
   NS_ENSURE_SUCCESS(rv,rv);
   return rv;
}

NS_IMETHODIMP
nsMessenger::CopyFolders(nsIRDFCompositeDataSource *database,
                          nsIRDFResource *dstResource,
                          nsISupportsArray *argumentArray, // nsIFolders
                          PRBool isMoveFolder)
{
	nsresult rv;

	if(!dstResource || !argumentArray)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> folderArray;
    
	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));

	NS_ENSURE_SUCCESS(rv,rv);

	folderArray->AppendElement(dstResource);
	
  if (isMoveFolder)
    return DoCommand(database, NS_LITERAL_CSTRING(NC_RDF_MOVEFOLDER), folderArray, argumentArray);

  return DoCommand(database, NS_LITERAL_CSTRING(NC_RDF_COPYFOLDER), folderArray, argumentArray);
	
}

NS_IMETHODIMP
nsMessenger::RenameFolder(nsIRDFCompositeDataSource* db,
                          nsIRDFResource* folderResource,
                          const PRUnichar* name)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (!db || !folderResource || !name || !*name) return rv;
    nsCOMPtr<nsISupportsArray> folderArray;
    nsCOMPtr<nsISupportsArray> argsArray;

  rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
  if (NS_FAILED(rv)) return rv;
  folderArray->AppendElement(folderResource);
  rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
  if(NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIRDFLiteral> nameLiteral;

    rdfService->GetLiteral(name, getter_AddRefs(nameLiteral));
    argsArray->AppendElement(nameLiteral);
    rv = DoCommand(db, NS_LITERAL_CSTRING(NC_RDF_RENAME), folderArray, argsArray);
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::CompactFolder(nsIRDFCompositeDataSource* db,
                           nsIRDFResource* folderResource, PRBool forAll)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  
  if (!db || !folderResource) return rv;
  nsCOMPtr<nsISupportsArray> folderArray;

  rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
  if (NS_FAILED(rv)) return rv;
  folderArray->AppendElement(folderResource);
  if (forAll)
    rv = DoCommand(db, NS_LITERAL_CSTRING(NC_RDF_COMPACTALL),  folderArray, nsnull);
  else
    rv = DoCommand(db, NS_LITERAL_CSTRING(NC_RDF_COMPACT),  folderArray, nsnull);
  if (NS_SUCCEEDED(rv) && mTxnMgr)
      mTxnMgr->Clear();
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
  rv = DoCommand(db, NS_LITERAL_CSTRING(NC_RDF_EMPTYTRASH), folderArray, nsnull);
  if (NS_SUCCEEDED(rv) && mTxnMgr)
      mTxnMgr->Clear();
  return rv;
}

NS_IMETHODIMP nsMessenger::GetUndoTransactionType(PRUint32 *txnType)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!txnType || !mTxnMgr)
        return rv;
    *txnType = nsMessenger::eUnknown;
    nsCOMPtr<nsITransaction> txn;
    rv = mTxnMgr->PeekUndoStack(getter_AddRefs(txn));
    if (NS_SUCCEEDED(rv) && txn)
    {
        nsCOMPtr<nsMsgTxn> msgTxn = do_QueryInterface(txn, &rv);
        if (NS_SUCCEEDED(rv) && msgTxn)
            rv = msgTxn->GetTransactionType(txnType);
    }
    return rv;
}

NS_IMETHODIMP nsMessenger::CanUndo(PRBool *bValue)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!bValue || !mTxnMgr)
        return rv;
    *bValue = PR_FALSE;
    PRInt32 count = 0;
    rv = mTxnMgr->GetNumberOfUndoItems(&count);
    if (NS_SUCCEEDED(rv) && count > 0)
        *bValue = PR_TRUE;
    return rv;
}

NS_IMETHODIMP nsMessenger::GetRedoTransactionType(PRUint32 *txnType)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!txnType || !mTxnMgr)
        return rv;
    *txnType = nsMessenger::eUnknown;
    nsCOMPtr<nsITransaction> txn;
    rv = mTxnMgr->PeekRedoStack(getter_AddRefs(txn));
    if (NS_SUCCEEDED(rv) && txn)
    {
        nsCOMPtr<nsMsgTxn> msgTxn = do_QueryInterface(txn, &rv);
        if (NS_SUCCEEDED(rv) && msgTxn)
            rv = msgTxn->GetTransactionType(txnType);
    }
    return rv;
}

NS_IMETHODIMP nsMessenger::CanRedo(PRBool *bValue)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!bValue || !mTxnMgr)
        return rv;
    *bValue = PR_FALSE;
    PRInt32 count = 0;
    rv = mTxnMgr->GetNumberOfRedoItems(&count);
    if (NS_SUCCEEDED(rv) && count > 0)
        *bValue = PR_TRUE;
    return rv;
}

NS_IMETHODIMP
nsMessenger::Undo(nsIMsgWindow *msgWindow)
{
  nsresult rv = NS_OK;
  if (mTxnMgr)
  {
    PRInt32 numTxn = 0;
    rv = mTxnMgr->GetNumberOfUndoItems(&numTxn);
    if (NS_SUCCEEDED(rv) && numTxn > 0)
    {
        nsCOMPtr<nsITransaction> txn;
        rv = mTxnMgr->PeekUndoStack(getter_AddRefs(txn));
        if (NS_SUCCEEDED(rv) && txn)
        {
            nsCOMPtr<nsMsgTxn> msgTxn = do_QueryInterface(txn, &rv);
            if (NS_SUCCEEDED(rv) && msgTxn)
                msgTxn->SetMsgWindow(msgWindow);
        }
        mTxnMgr->UndoTransaction();
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::Redo(nsIMsgWindow *msgWindow)
{
  nsresult rv = NS_OK;
  if (mTxnMgr)
  {
    PRInt32 numTxn = 0;
    rv = mTxnMgr->GetNumberOfRedoItems(&numTxn);
    if (NS_SUCCEEDED(rv) && numTxn > 0)
    {
        nsCOMPtr<nsITransaction> txn;
        rv = mTxnMgr->PeekRedoStack(getter_AddRefs(txn));
        if (NS_SUCCEEDED(rv) && txn)
        {
            nsCOMPtr<nsMsgTxn> msgTxn = do_QueryInterface(txn, &rv);
            if (NS_SUCCEEDED(rv) && msgTxn)
                msgTxn->SetMsgWindow(msgWindow);
        }
        mTxnMgr->RedoTransaction();
    }
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

NS_IMETHODIMP nsMessenger::SetDocumentCharset(const char *characterSet)
{
	// We want to redisplay the currently selected message (if any) but forcing the 
  // redisplay to use characterSet
  if (!mLastDisplayURI.IsEmpty())
  {
    nsCOMPtr <nsIMsgMessageService> messageService;
    nsresult rv = GetMessageServiceFromURI(mLastDisplayURI.get(), getter_AddRefs(messageService));
    
    if (NS_SUCCEEDED(rv) && messageService)
    {
      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
      messageService->DisplayMessage(mLastDisplayURI.get(), webShell, mMsgWindow, nsnull, characterSet, nsnull);
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
  SendLaterListener(nsIMessenger *);
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
protected:
  nsWeakPtr m_messenger; 
};

NS_IMPL_ISUPPORTS1(SendLaterListener, nsIMsgSendLaterListener)

SendLaterListener::SendLaterListener(nsIMessenger *aMessenger)
{
  m_messenger = do_GetWeakReference(aMessenger);
}

SendLaterListener::~SendLaterListener()
{
  nsCOMPtr <nsIMessenger> messenger = do_QueryReferent(m_messenger);
  // best to be defensive about this, in case OnStopSending doesn't get called.
  if (messenger)
    messenger->SetSendingUnsentMsgs(PR_FALSE);
  m_messenger = nsnull;
}

nsresult
SendLaterListener::OnStartSending(PRUint32 aTotalMessageCount)
{
  // this never gets called :-(
  nsCOMPtr <nsIMessenger> messenger = do_QueryReferent(m_messenger);
  if (messenger)
    messenger->SetSendingUnsentMsgs(PR_TRUE);
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

  nsCOMPtr <nsIMessenger> messenger = do_QueryReferent(m_messenger);
  if (messenger)
    messenger->SetSendingUnsentMsgs(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsMessenger::SendUnsentMessages(nsIMsgIdentity *aIdentity, nsIMsgWindow *aMsgWindow)
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

    SendLaterListener *sendLaterListener = new SendLaterListener(this);
    if (!sendLaterListener)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(sendLaterListener);
    pMsgSendLater->AddListener(sendLaterListener);
    pMsgSendLater->SetMsgWindow(aMsgWindow);
    mSendingUnsentMsgs = PR_TRUE;
 
    pMsgSendLater->SendUnsentMessages(aIdentity); 
    NS_RELEASE(sendLaterListener);
	} 
	return NS_OK;
}

nsSaveMsgListener::nsSaveMsgListener(nsIFileSpec* aSpec, nsMessenger *aMessenger)
{
    m_fileSpec = do_QueryInterface(aSpec);
    m_messenger = aMessenger;
    m_dataBuffer = nsnull;

    // rhp: for charset handling
    m_doCharsetConversion = PR_FALSE;
    m_saveAllAttachmentsState = nsnull;
    mProgress = 0;
    mContentLength = -1;
    mCanceled = PR_FALSE;
    m_outputFormat = eUnknown;
    mInitialized = PR_FALSE;
}

nsSaveMsgListener::~nsSaveMsgListener()
{
}

// 
// nsISupports
//
NS_IMPL_ISUPPORTS4(nsSaveMsgListener, nsIUrlListener, nsIMsgCopyServiceListener, nsIStreamListener, nsIObserver)


NS_IMETHODIMP
nsSaveMsgListener::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, "oncancel"))
      mCanceled = PR_TRUE;

  return NS_OK;
}

// 
// nsIUrlListener
// 
NS_IMETHODIMP
nsSaveMsgListener::OnStartRunningUrl(nsIURI* url)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSaveMsgListener::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  nsresult rv = exitCode;
  PRBool killSelf = PR_TRUE;

  if (m_fileSpec)
  {
    m_fileSpec->Flush();
    m_fileSpec->CloseStream();
    if (NS_FAILED(rv)) goto done;
    if (m_templateUri) { // ** save as template goes here
        nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
        if (NS_FAILED(rv)) goto done;
        nsCOMPtr<nsIRDFResource> res;
        rv = rdf->GetResource(m_templateUri, getter_AddRefs(res));
        if (NS_FAILED(rv)) goto done;
        nsCOMPtr<nsIMsgFolder> templateFolder;
        templateFolder = do_QueryInterface(res, &rv);
        if (NS_FAILED(rv)) goto done;
        nsCOMPtr<nsIMsgCopyService> copyService = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID);
        if (copyService)
          rv = copyService->CopyFileMessage(m_fileSpec, templateFolder, nsnull, PR_TRUE, this, nsnull);
        killSelf = PR_FALSE;
    }
  }

done:
  if (NS_FAILED(rv))
  {
    if (m_fileSpec)
    {
      nsFileSpec realSpec;
      m_fileSpec->GetFileSpec(&realSpec);
      realSpec.Delete(PR_FALSE);
    }
    if (m_messenger)
    {
        m_messenger->Alert("saveMessageFailed");
    }
  }
  if (killSelf)
      Release(); // no more work needs to be done; kill ourself

  return rv;
}

NS_IMETHODIMP
nsSaveMsgListener::OnStartCopy(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSaveMsgListener::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSaveMsgListener::SetMessageKey(PRUint32 aKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSaveMsgListener::GetMessageId(nsCString* aMessageId)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSaveMsgListener::OnStopCopy(nsresult aStatus)
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

// initializes the progress window if we are going to show one
// and for OSX, sets creator flags on the output file
nsresult nsSaveMsgListener::InitializeDownload(nsIRequest * aRequest, PRInt32 aBytesDownloaded)
{
  nsresult rv = NS_OK;

  mInitialized = PR_TRUE;
  nsCOMPtr<nsIChannel> channel (do_QueryInterface(aRequest));

  if (!channel)
    return rv;

  // Set content length if we haven't already got it.
  if (mContentLength == -1)
      channel->GetContentLength(&mContentLength);

  if (!m_contentType.IsEmpty())
  {
      nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID));
      nsCOMPtr<nsIMIMEInfo> mimeinfo;

      mimeService->GetFromTypeAndExtension(m_contentType, EmptyCString(), getter_AddRefs(mimeinfo)); 
      nsFileSpec realSpec;
      m_fileSpec->GetFileSpec(&realSpec);

      // Create nsILocalFile from a nsFileSpec.
      nsCOMPtr<nsILocalFile> outputFile;
      NS_FileSpecToIFile(&realSpec, getter_AddRefs(outputFile)); 

      // create a download progress window
      // XXX: we don't want to show the progress dialog if the download is really small.
      // but what is a small download? Well that's kind of arbitrary
      // so make an arbitrary decision based on the content length of the attachment
      if (mContentLength != -1 && mContentLength > aBytesDownloaded * 2)
      {
        nsCOMPtr<nsIDownload> dl = do_CreateInstance("@mozilla.org/download;1", &rv);
        if (dl && outputFile)
        {
          PRTime timeDownloadStarted = PR_Now();

          nsCOMPtr<nsIURI> outputURI;
          NS_NewFileURI(getter_AddRefs(outputURI), outputFile);

          nsCOMPtr<nsIURI> url;
          channel->GetURI(getter_AddRefs(url));
          rv = dl->Init(url, outputURI, nsnull, mimeinfo, timeDownloadStarted, nsnull);

          dl->SetObserver(this);
          // now store the web progresslistener 
          mWebProgressListener = do_QueryInterface(dl);
        }
      }

#if defined(XP_MAC) || defined(XP_MACOSX)
      /* if we are saving an appledouble or applesingle attachment, we need to use an Apple File Decoder */
      if ((nsCRT::strcasecmp(m_contentType.get(), APPLICATION_APPLEFILE) == 0) ||
          (nsCRT::strcasecmp(m_contentType.get(), MULTIPART_APPLEDOUBLE) == 0))
      {        

        nsCOMPtr<nsIAppleFileDecoder> appleFileDecoder = do_CreateInstance(NS_IAPPLEFILEDECODER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv) && appleFileDecoder)
        {
          rv = appleFileDecoder->Initialize(m_outputStream, outputFile);
          if (NS_SUCCEEDED(rv))
            m_outputStream = do_QueryInterface(appleFileDecoder, &rv);
        }
      }
      else
      {
          if (mimeinfo)
          {
            PRUint32 aMacType;
            PRUint32 aMacCreator;
            if (NS_SUCCEEDED(mimeinfo->GetMacType(&aMacType)) && NS_SUCCEEDED(mimeinfo->GetMacCreator(&aMacCreator)))
            {
              nsCOMPtr<nsILocalFileMac> macFile =  do_QueryInterface(outputFile, &rv);
              if (NS_SUCCEEDED(rv) && macFile)
              {
                macFile->SetFileCreator((OSType)aMacCreator);
                macFile->SetFileType((OSType)aMacType);
              }
            }
          }
      }
#endif // XP_MACOSX
  }

  return rv;
}

NS_IMETHODIMP
nsSaveMsgListener::OnStartRequest(nsIRequest* request, nsISupports* aSupport)
{
    nsresult rv = NS_OK;

    if (m_fileSpec)
        rv = m_fileSpec->GetOutputStream(getter_AddRefs(m_outputStream));
    if (NS_FAILED(rv) && m_messenger)
        m_messenger->Alert("saveAttachmentFailed");
    else if (!m_dataBuffer)
        m_dataBuffer = (char*) PR_CALLOC(FOUR_K+1);

    return rv;
}

NS_IMETHODIMP
nsSaveMsgListener::OnStopRequest(nsIRequest* request, nsISupports* aSupport,
                                nsresult status)
{
  nsresult    rv = NS_OK;

  // rhp: If we are doing the charset conversion magic, this is different
  // processing, otherwise, its just business as usual.
  //
  if ( (m_doCharsetConversion) && (m_fileSpec) )
  {
    char        *conBuf = nsnull;
    PRUint32    conLength = 0; 

    // If we need text/plain, then we need to convert the HTML and then convert
    // to the systems charset
    //
    if (m_outputFormat == ePlainText)
    {
      ConvertBufToPlainText(m_msgBuffer);
      rv = nsMsgI18NSaveAsCharset(TEXT_PLAIN, nsMsgI18NFileSystemCharset(), 
                                  m_msgBuffer.get(), &conBuf); 
      if ( NS_SUCCEEDED(rv) && (conBuf) )
        conLength = strlen(conBuf);
    }

    if ( (NS_SUCCEEDED(rv)) && (conBuf) )
    {
      PRUint32      writeCount;
      rv = m_outputStream->Write(conBuf, conLength, &writeCount);
      if (conLength != writeCount)
        rv = NS_ERROR_FAILURE;
    }

    PR_FREEIF(conBuf);
  }

  // close down the file stream and release ourself
  if (m_fileSpec)
  {
    m_fileSpec->Flush();
    m_fileSpec->CloseStream();
    m_outputStream = nsnull;
  }
  
  if (m_saveAllAttachmentsState)
  {
      m_saveAllAttachmentsState->m_curIndex++;
      if (m_saveAllAttachmentsState->m_curIndex <
          m_saveAllAttachmentsState->m_count)
      {
          nsSaveAllAttachmentsState *state = m_saveAllAttachmentsState;
          PRUint32 i = state->m_curIndex;
          nsCOMPtr<nsIFileSpec> fileSpec;
          nsFileSpec aFileSpec ((const char *) state->m_directoryName);
          char * unescapedUrl = nsnull;

          nsXPIDLCString unescapedName;
          rv = NS_NewFileSpec(getter_AddRefs(fileSpec));
          if (NS_FAILED(rv)) goto done;
          unescapedUrl = PL_strdup(state->m_urlArray[i]);
          nsUnescape(unescapedUrl);
 
          rv = ConvertAndSanitizeFileName(state->m_displayNameArray[i], nsnull, 
                                          getter_Copies(unescapedName));
          if (NS_FAILED(rv))
            goto done;

          aFileSpec += unescapedName;
          rv = m_messenger->PromptIfFileExists(aFileSpec);
          if (NS_FAILED(rv)) goto done;
          fileSpec->SetFromFileSpec(aFileSpec);
          rv = m_messenger->SaveAttachment(fileSpec,
                                           unescapedUrl,
                                           state->m_messageUriArray[i],
                                           state->m_contentTypeArray[i],
                                           (void *)state);
      done:
          if (NS_FAILED(rv))
          {
              delete state;
              m_saveAllAttachmentsState = nsnull;
          }
          PR_FREEIF(unescapedUrl);
      }
      else
      {
          delete m_saveAllAttachmentsState;
          m_saveAllAttachmentsState = nsnull;
      }
  }

  if(mWebProgressListener)
  {
    mWebProgressListener->OnProgressChange(nsnull, nsnull, mContentLength, mContentLength, mContentLength, mContentLength);
    mWebProgressListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, NS_OK);
    mWebProgressListener = nsnull; // break any circular dependencies between the progress dialog and use
  }

  Release(); // all done kill ourself
  return NS_OK;
}

NS_IMETHODIMP
nsSaveMsgListener::OnDataAvailable(nsIRequest* request, 
                                  nsISupports* aSupport,
                                  nsIInputStream* inStream, 
                                  PRUint32 srcOffset,
                                  PRUint32 count)
{
  nsresult rv = NS_ERROR_FAILURE;
  // first, check to see if we've been canceled....
  if (mCanceled) // then go cancel our underlying channel too
    return request->Cancel(NS_BINDING_ABORTED);

  if (!mInitialized)
    InitializeDownload(request, count);


  if (m_dataBuffer && m_outputStream)
  {
    mProgress += count;
    PRUint32 available, readCount, maxReadCount = FOUR_K;
    PRUint32 writeCount;
    rv = inStream->Available(&available);
    while (NS_SUCCEEDED(rv) && available)
    {
      if (maxReadCount > available)
        maxReadCount = available;
      memset(m_dataBuffer, 0, FOUR_K+1);
      rv = inStream->Read(m_dataBuffer, maxReadCount, &readCount);

      // rhp:
      // Ok, now we do one of two things. If we are sending out HTML, then
      // just write it to the HTML stream as it comes along...but if this is
      // a save as TEXT operation, we need to buffer this up for conversion 
      // when we are done. When the stream converter for HTML-TEXT gets in place,
      // this magic can go away.
      //
      if (NS_SUCCEEDED(rv))
      {
        if ( (m_doCharsetConversion) && (m_outputFormat == ePlainText) )
          AppendUTF8toUTF16(Substring(m_dataBuffer, m_dataBuffer + readCount),
                            m_msgBuffer);
        else
          rv = m_outputStream->Write(m_dataBuffer, readCount, &writeCount);

        available -= readCount;
      }
    }

    if (NS_SUCCEEDED(rv) && mWebProgressListener) // Send progress notification.
        mWebProgressListener->OnProgressChange(nsnull, request, mProgress, mContentLength, mProgress, mContentLength);
  }
  return rv;
}

#define MESSENGER_STRING_URL       "chrome://messenger/locale/messenger.properties"

nsresult
nsMessenger::InitStringBundle()
{
    nsresult res = NS_OK;
    if (!mStringBundle)
    {
		const char propertyURL[] = MESSENGER_STRING_URL;

		nsCOMPtr<nsIStringBundleService> sBundleService = 
		         do_GetService(NS_STRINGBUNDLE_CONTRACTID, &res);
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			res = sBundleService->CreateBundle(propertyURL,
                                               getter_AddRefs(mStringBundle));
		}
    }
    return res;
}

const nsAdoptingString
nsMessenger::GetString(const nsAFlatString& aStringName)
{
    nsresult rv = NS_OK;
    PRUnichar *ptrv = nsnull;

    if (!mStringBundle)
        rv = InitStringBundle();

    if (mStringBundle)
        rv = mStringBundle->GetStringFromName(aStringName.get(), &ptrv);

    if (NS_FAILED(rv) || !ptrv)
        ptrv = ToNewUnicode(aStringName);

    return nsAdoptingString(ptrv);
}

nsSaveAllAttachmentsState::nsSaveAllAttachmentsState(PRUint32 count,
                                                     const char **contentTypeArray,
                                                     const char **urlArray,
                                                     const char **nameArray,
                                                     const char **uriArray,
                                                     const char *dirName)
{
    PRUint32 i;
    NS_ASSERTION(count && urlArray && nameArray && uriArray && dirName, 
                 "fatal - invalid parameters\n");
    
    m_count = count;
    m_curIndex = 0;
    m_contentTypeArray = new char*[count];
    m_urlArray = new char*[count];
    m_displayNameArray = new char*[count];
    m_messageUriArray = new char*[count];
    for (i = 0; i < count; i++)
    {
        m_contentTypeArray[i] = nsCRT::strdup(contentTypeArray[i]);
        m_urlArray[i] = nsCRT::strdup(urlArray[i]);
        m_displayNameArray[i] = nsCRT::strdup(nameArray[i]);
        m_messageUriArray[i] = nsCRT::strdup(uriArray[i]);
    }
    m_directoryName = nsCRT::strdup(dirName);
}

nsSaveAllAttachmentsState::~nsSaveAllAttachmentsState()
{
    PRUint32 i;
    for (i = 0; i < m_count; i++)
    {
        nsCRT::free(m_contentTypeArray[i]);
        nsCRT::free(m_urlArray[i]);
        nsCRT::free(m_displayNameArray[i]);
        nsCRT::free(m_messageUriArray[i]);
    }
    delete[] m_contentTypeArray;
    delete[] m_urlArray;
    delete[] m_displayNameArray;
    delete[] m_messageUriArray;
    nsCRT::free(m_directoryName);
}

nsresult
nsMessenger::GetLastSaveDirectory(nsILocalFile **aLastSaveDir)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // this can fail, and it will, on the first time we call it, as there is no default for this pref.
  nsCOMPtr <nsILocalFile> localFile;
  rv = prefBranch->GetComplexValue(MESSENGER_SAVE_DIR_PREF_NAME, NS_GET_IID(nsILocalFile), getter_AddRefs(localFile));
  if (NS_SUCCEEDED(rv)) {
    NS_IF_ADDREF(*aLastSaveDir = localFile);
  }
  return rv;
}

nsresult
nsMessenger::SetLastSaveDirectory(nsILocalFile *aLocalFile)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIFile> file = do_QueryInterface(aLocalFile, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // if the file is a directory, just use it for the last dir chosen
  // otherwise, use the parent of the file as the last dir chosen.
  // IsDirectory() will return error on saving a file, as the
  // file doesn't exist yet.
  PRBool isDirectory;
  rv = file->IsDirectory(&isDirectory);
  if (NS_SUCCEEDED(rv) && isDirectory) {
    rv = prefBranch->SetComplexValue(MESSENGER_SAVE_DIR_PREF_NAME, NS_GET_IID(nsILocalFile), aLocalFile);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else {
    nsCOMPtr <nsIFile> parent;
    rv = file->GetParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv,rv);
   
    nsCOMPtr <nsILocalFile> parentLocalFile = do_QueryInterface(parent, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = prefBranch->SetComplexValue(MESSENGER_SAVE_DIR_PREF_NAME, NS_GET_IID(nsILocalFile), parentLocalFile);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

