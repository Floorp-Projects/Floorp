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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt  <hyatt@netscape.com>
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "CHeaderSniffer.h"
#include "UMacUnicode.h"

#include "UCustomNavServicesDialogs.h"

#include "netCore.h"

#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "nsIStringEnumerator.h"
#include "nsIPrefService.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDownload.h"
#include "nsILocalFileMac.h"

const char* const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

CHeaderSniffer::CHeaderSniffer(nsIWebBrowserPersist* aPersist, nsIFile* aFile, nsIURI* aURL,
                nsIDOMDocument* aDocument, nsIInputStream* aPostData,
                const nsAString& aSuggestedFilename, PRBool aBypassCache, ESaveFormat aSaveFormat)
: mPersist(aPersist)
, mTmpFile(aFile)
, mURL(aURL)
, mDocument(aDocument)
, mPostData(aPostData)
, mDefaultFilename(aSuggestedFilename)
, mBypassCache(aBypassCache)
, mSaveFormat(aSaveFormat)
{
}

CHeaderSniffer::~CHeaderSniffer()
{
}

NS_IMPL_ISUPPORTS1(CHeaderSniffer, nsIWebProgressListener)

#pragma mark -

// Implementation of nsIWebProgressListener
/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
CHeaderSniffer::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, 
                                PRUint32 aStatus)
{  
  if (aStateFlags & nsIWebProgressListener::STATE_START)
  {
    nsCOMPtr<nsIWebBrowserPersist> kungFuDeathGrip(mPersist);   // be sure to keep it alive while we save
                                                                // since it owns us as a listener
    nsCOMPtr<nsIWebProgressListener> kungFuSuicideGrip(this);   // and keep ourselves alive
    
    nsresult rv;
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest, &rv);
    if (!channel) return rv;
    channel->GetContentType(mContentType);
    
    nsCOMPtr<nsIURI> origURI;
    channel->GetOriginalURI(getter_AddRefs(origURI));
    
    // Get the content-disposition if we're an HTTP channel.
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel)
      httpChannel->GetResponseHeader(nsCAutoString("content-disposition"), mContentDisposition);
    
    mPersist->CancelSave();
    PRBool exists;
    mTmpFile->Exists(&exists);
    if (exists)
        mTmpFile->Remove(PR_FALSE);

    rv = PerformSave(origURI, mSaveFormat);
    if (NS_FAILED(rv))
    {
      // put up some UI
      
    }
  }
  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
CHeaderSniffer::OnProgressChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           PRInt32 aCurSelfProgress, 
           PRInt32 aMaxSelfProgress, 
           PRInt32 aCurTotalProgress, 
           PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
CHeaderSniffer::OnLocationChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           nsIURI *location)
{
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
CHeaderSniffer::OnStatusChange(nsIWebProgress *aWebProgress, 
               nsIRequest *aRequest, 
               nsresult aStatus, 
               const PRUnichar *aMessage)
{
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
CHeaderSniffer::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  return NS_OK;
}

#pragma mark -

static ESaveFormat SaveFormatFromPrefValue(PRInt32 inPrefValue)
{
  switch (inPrefValue)
  {
    case 0:   return eSaveFormatHTMLComplete;
    default:  // fall through
    case 1:   return eSaveFormatHTML;
    case 2:   return eSaveFormatPlainText;
  }
}

static PRInt32 PrefValueFromSaveFormat(ESaveFormat inSaveFormat)
{
  switch (inSaveFormat)
  {
    case eSaveFormatPlainText:    return 2;
    default:  // fall through
    case eSaveFormatHTML:         return 1;
    case eSaveFormatHTMLComplete: return 0;
  }
}

nsresult CHeaderSniffer::PerformSave(nsIURI* inOriginalURI, const ESaveFormat inSaveFormat)
{
    nsresult rv;
    // Are we an HTML document? If so, we will want to append an accessory view to
    // the save dialog to provide the user with the option of doing a complete
    // save vs. a single file save.
    PRBool isHTML = (mDocument && mContentType.Equals("text/html") ||
                     mContentType.Equals("text/xml") ||
                     mContentType.Equals("application/xhtml+xml"));
    
    // Next find out the directory that we should start in.
    nsCOMPtr<nsIPrefService> prefs(do_GetService("@mozilla.org/preferences-service;1", &rv));
    if (!prefs)
        return rv;
    nsCOMPtr<nsIPrefBranch> dirBranch;
    prefs->GetBranch("browser.download.", getter_AddRefs(dirBranch));
    PRInt32 filterIndex = 0;
    if (inSaveFormat != eSaveFormatUnspecified) {
      filterIndex = PrefValueFromSaveFormat(inSaveFormat);
    }
    else if (dirBranch) {
        nsresult rv = dirBranch->GetIntPref("save_converter_index", &filterIndex);
        if (NS_FAILED(rv))
            filterIndex = 0;
    }

    // We need to figure out what file name to use.
    nsAutoString defaultFileName;
    if (!mContentDisposition.IsEmpty()) {
        // (1) Use the HTTP header suggestion.
        PRInt32 index = mContentDisposition.Find("filename=");
        if (index >= 0) {
            // Take the substring following the prefix.
            index += 9;
            nsCAutoString filename;
            mContentDisposition.Right(filename, mContentDisposition.Length() - index);
            defaultFileName = NS_ConvertUTF8toUCS2(filename);
        }
    }
    
    if (defaultFileName.IsEmpty()) {
        nsCOMPtr<nsIURL> url(do_QueryInterface(mURL));
        if (url) {
            nsCAutoString fileNameCString;
            url->GetFileName(fileNameCString); // (2) For file URLs, use the file name.
            defaultFileName = NS_ConvertUTF8toUCS2(fileNameCString);
        }
    }
    
    if (defaultFileName.IsEmpty() && mDocument && isHTML) {
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(mDocument));
        if (htmlDoc)
            htmlDoc->GetTitle(defaultFileName); // (3) Use the title of the document.
    }
    
    if (defaultFileName.IsEmpty()) {
        // (4) Use the caller provided name.
        defaultFileName = mDefaultFilename;
    }

    if (defaultFileName.IsEmpty() && mURL) {
        // (5) Use the host.
        nsCAutoString hostName;
        mURL->GetHost(hostName);
        defaultFileName = NS_ConvertUTF8toUCS2(hostName);
    }
    
    // One last case to handle about:blank and other fruity untitled pages.
    if (defaultFileName.IsEmpty())
        defaultFileName.AssignLiteral("untitled");
        
    // Validate the file name to ensure legality.
    for (PRUint32 i = 0; i < defaultFileName.Length(); i++)
        if (defaultFileName[i] == ':' || defaultFileName[i] == '/')
            defaultFileName.SetCharAt(i, PRUnichar(' '));
            
    // Make sure the appropriate extension is appended to the suggested file name.
    nsCOMPtr<nsIURI> fileURI(do_CreateInstance("@mozilla.org/network/standard-url;1"));
    nsCOMPtr<nsIURL> fileURL(do_QueryInterface(fileURI, &rv));
    if (!fileURL)
        return rv;
     
    fileURL->SetFilePath(NS_ConvertUCS2toUTF8(defaultFileName));
    
    nsCAutoString fileExtension;
    fileURL->GetFileExtension(fileExtension);
    
    PRBool setExtension = PR_FALSE;
    if (mContentType.Equals("text/html")) {
        if (fileExtension.IsEmpty() || (!fileExtension.Equals("htm") && !fileExtension.Equals("html"))) {
            defaultFileName.AppendLiteral(".html");
            setExtension = PR_TRUE;
        }
    }
    
    if (!setExtension && fileExtension.IsEmpty()) {
        nsCOMPtr<nsIMIMEService> mimeService(do_GetService("@mozilla.org/mime;1", &rv));
        if (!mimeService)
            return rv;
        nsCOMPtr<nsIMIMEInfo> mimeInfo;
        rv = mimeService->GetFromTypeAndExtension(mContentType, EmptyCString(), getter_AddRefs(mimeInfo));
        if (!mimeInfo)
          return rv;

        nsCOMPtr<nsIUTF8StringEnumerator> extensions;
        mimeInfo->GetFileExtensions(getter_AddRefs(extensions));
        
        PRBool hasMore;
        extensions->HasMore(&hasMore);
        if (hasMore) {
            nsCAutoString ext;
            extensions->GetNext(ext);
            defaultFileName.Append(PRUnichar('.'));
            defaultFileName.Append(NS_ConvertUTF8toUCS2(ext));
        }
    }

    // Now it's time to pose the save dialog.
    FSSpec       destFileSpec;
    bool         isReplacing = false;

    {
        Str255          defaultName;
        bool            result;

        CPlatformUCSConversion::GetInstance()->UCSToPlatform(defaultFileName, defaultName);
#ifndef XP_MACOSX
        char            tempBuf1[256], tempBuf2[64];
        ::CopyPascalStringToC(defaultName, tempBuf1);        
        ::CopyCStringToPascal(NS_TruncNodeName(tempBuf1, tempBuf2), defaultName);
#endif
        if (isHTML) {
            ESaveFormat saveFormat = SaveFormatFromPrefValue(filterIndex);
            UNavServicesDialogs::LCustomFileDesignator customDesignator;
            result = customDesignator.AskDesignateFile(defaultName, saveFormat);
            if (!result)
                return NS_OK;       // user canceled
            filterIndex = PrefValueFromSaveFormat(saveFormat);
            customDesignator.GetFileSpec(destFileSpec);
            isReplacing = customDesignator.IsReplacing();
        }
        else {
            UNavServicesDialogs::LFileDesignator stdDesignator;
            result = stdDesignator.AskDesignateFile(defaultName);
            if (!result)
                return NS_OK;       // user canceled
            stdDesignator.GetFileSpec(destFileSpec);
            isReplacing = stdDesignator.IsReplacing();        
        }

        // After the dialog is dismissed, process all activation an update events right away.
        // The save dialog code calls UDesktop::Activate after dismissing the dialog. All that
        // does is activate the now frontmost LWindow which was behind the dialog. It does not
        // remove the activate event from the queue. If that event is not processed and removed
        // before we show the progress window, bad things happen. Specifically, the progress
        // dialog will show in front and then, shortly thereafter, the window which was behind this save
        // dialog will be moved to the front.
        
        if (LEventDispatcher::GetCurrentEventDispatcher()) { // Can this ever be NULL?
            EventRecord theEvent;
            while (::WaitNextEvent(updateMask | activMask, &theEvent, 0, nil))
                LEventDispatcher::GetCurrentEventDispatcher()->DispatchEvent(theEvent);
        }
    }

    // only save the pref if the frontend didn't specify a format
    if (inSaveFormat == eSaveFormatUnspecified && isHTML)
      dirBranch->SetIntPref("save_converter_index", filterIndex);

    nsCOMPtr<nsILocalFileMac> destFile;
    
    rv = NS_NewLocalFileWithFSSpec(&destFileSpec, PR_TRUE, getter_AddRefs(destFile));
    if (NS_FAILED(rv))
      return rv;
    
    if (isReplacing) {
      PRBool exists;
      rv = destFile->Exists(&exists);
      if (NS_SUCCEEDED(rv) && exists)
        rv = destFile->Remove(PR_TRUE);
      if (NS_FAILED(rv))
        return rv;
    }

    // if the user chose plain text, set the content type
    if (isHTML && filterIndex == 2)
      mContentType = "text/plain";
    
    nsCOMPtr<nsISupports> sourceData;
    if (isHTML && filterIndex != 1)
        sourceData = do_QueryInterface(mDocument);    // HTML complete
    else
        sourceData = do_QueryInterface(mURL);         // HTML or text only

    return InitiateDownload(sourceData, destFile, inOriginalURI);
}

// inOriginalURI is always a URI. inSourceData can be an nsIURI or an nsIDOMDocument, depending
// on what we're saving. It's that way for nsIWebBrowserPersist.
nsresult CHeaderSniffer::InitiateDownload(nsISupports* inSourceData, nsILocalFile* inDestFile, nsIURI* inOriginalURI)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIWebBrowserPersist> webPersist = do_CreateInstance(persistContractID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIURI> sourceURI = do_QueryInterface(inSourceData);

  PRInt64 timeNow = PR_Now();
  
  nsAutoString fileDisplayName;
  inDestFile->GetLeafName(fileDisplayName);
  
  nsCOMPtr<nsIDownload> downloader = do_CreateInstance(NS_DOWNLOAD_CONTRACTID);
  // dlListener attaches to its progress dialog here, which gains ownership
  rv = downloader->Init(inOriginalURI, inDestFile, fileDisplayName.get(), nsnull, timeNow, webPersist);
  if (NS_FAILED(rv)) return rv;
    
  PRInt32 flags = nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION | 
                  nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;
  if (mBypassCache)
    flags |= nsIWebBrowserPersist::PERSIST_FLAGS_BYPASS_CACHE;
  else
    flags |= nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;

  webPersist->SetPersistFlags(flags);
    
  if (sourceURI)
  {
    rv = webPersist->SaveURI(sourceURI, nsnull, nsnull, mPostData, nsnull, inDestFile);
  }
  else
  {
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(inSourceData, &rv);
    if (!domDoc) return rv;  // should never happen
    
    PRInt32 encodingFlags = 0;
    nsCOMPtr<nsILocalFile> filesFolder;
    
    if (!mContentType.Equals("text/plain")) {
      // Create a local directory in the same dir as our file.  It
      // will hold our associated files.
      filesFolder = do_CreateInstance("@mozilla.org/file/local;1");
      nsAutoString unicodePath;
      inDestFile->GetPath(unicodePath);
      filesFolder->InitWithPath(unicodePath);
      
      nsAutoString leafName;
      filesFolder->GetLeafName(leafName);
      nsAutoString nameMinusExt(leafName);
      PRInt32 index = nameMinusExt.RFind(".");
      if (index >= 0)
          nameMinusExt.Left(nameMinusExt, index);
      nameMinusExt.AppendLiteral(" Files"); // XXXdwh needs to be localizable!
      filesFolder->SetLeafName(nameMinusExt);
      PRBool exists = PR_FALSE;
      filesFolder->Exists(&exists);
      if (!exists) {
          rv = filesFolder->Create(nsILocalFile::DIRECTORY_TYPE, 0755);
          if (NS_FAILED(rv))
            return rv;
      }
    }
    else
    {
      encodingFlags |= nsIWebBrowserPersist::ENCODE_FLAGS_FORMATTED |
                        nsIWebBrowserPersist::ENCODE_FLAGS_ABSOLUTE_LINKS |
                        nsIWebBrowserPersist::ENCODE_FLAGS_NOFRAMES_CONTENT;
    }
    rv = webPersist->SaveDocument(domDoc, inDestFile, filesFolder, mContentType.get(), encodingFlags, 80);
  }
  
  return rv;
}
