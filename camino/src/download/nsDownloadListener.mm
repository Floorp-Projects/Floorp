/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 *   Calum Robinson <calumr@mac.com>
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
 
#import "NSString+Utils.h"

#include "nsDownloadListener.h"

#include "nsIURIFixup.h"
#include "nsIWebProgress.h"
#include "nsIFileURL.h"
#include "netCore.h"
#include "nsNetError.h"

nsDownloadListener::nsDownloadListener()
: mDownloadStatus(NS_OK)
, mBypassCache(PR_FALSE)
, mNetworkTransfer(PR_FALSE)
, mGotFirstStateChange(PR_FALSE)
, mUserCanceled(PR_FALSE)
,	mSentCancel(PR_FALSE)
, mDownloadPaused(PR_FALSE)
{
  mStartTime = LL_ZERO;
}

nsDownloadListener::~nsDownloadListener()
{
}

NS_IMPL_ISUPPORTS_INHERITED5(nsDownloadListener, CHDownloader, 
                             nsIInterfaceRequestor,
                             nsIDownload,
                             nsITransfer, nsIWebProgressListener,
                             nsIWebProgressListener2)

// Implementation of nsIInterfaceRequestor
NS_IMETHODIMP 
nsDownloadListener::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  return QueryInterface(aIID, aInstancePtr);
}

#pragma mark -

/* void init (in nsIURI aSource, in nsIURI aTarget, in AString aDisplayName, in wstring openingWith, in long long startTime, in nsICancelable aCancelable); */
NS_IMETHODIMP
nsDownloadListener::Init(nsIURI *aSource, nsIURI *aTarget, const nsAString &aDisplayName,
        nsIMIMEInfo* aMIMEInfo, PRInt64 startTime, nsILocalFile* aTempFile,
        nsICancelable* aCancelable)
{ 
  // get the local file corresponding to the given target URI
  nsCOMPtr<nsILocalFile> targetFile;
  {
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aTarget);
    if (fileURL)
    {
      nsCOMPtr<nsIFile> file;
      fileURL->GetFile(getter_AddRefs(file));
      if (file)
        targetFile = do_QueryInterface(file);
    }
  }
  NS_ENSURE_TRUE(targetFile, NS_ERROR_INVALID_ARG);

  CreateDownloadDisplay(); // call the base class to make the download UI

  // Note: This forms a cycle, which will be broken in DownloadDone
  mCancelable = aCancelable;

  // This is a file save if the cancelable object is a webbrowserpersist
  nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(aCancelable));
  SetIsFileSave(persist != NULL);
  
  mDestination = aTarget;
  mDestinationFile = targetFile;
  mURI = aSource;
  mStartTime = startTime;

  InitDialog();
  return NS_OK;
}

/* readonly attribute nsIURI source; */
NS_IMETHODIMP
nsDownloadListener::GetSource(nsIURI * *aSource)
{
  NS_ENSURE_ARG_POINTER(aSource);
  NS_IF_ADDREF(*aSource = mURI);
  return NS_OK;
}

/* readonly attribute nsIURI target; */
NS_IMETHODIMP
nsDownloadListener::GetTarget(nsIURI * *aTarget)
{
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_IF_ADDREF(*aTarget = mDestination);
  return NS_OK;
}

/* readonly attribute nsICancelable cancelable; */
NS_IMETHODIMP
nsDownloadListener::GetCancelable(nsICancelable * *aCancelable)
{
  NS_ENSURE_ARG_POINTER(aCancelable);
  NS_IF_ADDREF(*aCancelable = mCancelable);
  return NS_OK;
}

/* readonly attribute PRInt32 percentComplete; */
NS_IMETHODIMP
nsDownloadListener::GetPercentComplete(PRInt32 *aPercentComplete)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint64 amountTransferred; */
NS_IMETHODIMP
nsDownloadListener::GetAmountTransferred(PRUint64 *aAmountTransferred)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint64 size; */
NS_IMETHODIMP
nsDownloadListener::GetSize(PRUint64 *aSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute wstring displayName; */
NS_IMETHODIMP
nsDownloadListener::GetDisplayName(PRUnichar * *aDisplayName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long long startTime; */
NS_IMETHODIMP
nsDownloadListener::GetStartTime(PRInt64 *aStartTime)
{
  NS_ENSURE_ARG(aStartTime);
  *aStartTime = mStartTime;
  return NS_OK;
}

/* readonly attribute double speed; */
NS_IMETHODIMP
nsDownloadListener::GetSpeed(double* aSpeed)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIMIMEInfo MIMEInfo; */
NS_IMETHODIMP
nsDownloadListener::GetMIMEInfo(nsIMIMEInfo * *aMIMEInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDownloadListener::GetTargetFile(nsILocalFile ** aTargetFile)
{
  NS_ENSURE_ARG_POINTER(aTargetFile);
  NS_IF_ADDREF(*aTargetFile = mDestinationFile);
  return NS_OK;
}

#pragma mark -

/* void onProgressChange64 (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long long aCurSelfProgress, in long long aMaxSelfProgress, in long long a
CurTotalProgress, in long long aMaxTotalProgress); */
NS_IMETHODIMP 
nsDownloadListener::OnProgressChange64(nsIWebProgress *aWebProgress, 
                                       nsIRequest *aRequest, 
                                       PRInt64 aCurSelfProgress, 
                                       PRInt64 aMaxSelfProgress, 
                                       PRInt64 aCurTotalProgress, 
                                       PRInt64 aMaxTotalProgress)
{
  if (!mRequest)
    mRequest = aRequest; // for pause/resume downloading
	
  [mDownloadDisplay setProgressTo:aCurTotalProgress ofMax:aMaxTotalProgress];
  return NS_OK;
}

/* boolean onRefreshAttempted (in nsIWebProgress aWebProgress, in nsIURI aRefreshURI, in long aDelay, in boolean aSameURI); */
NS_IMETHODIMP
nsDownloadListener::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                                       nsIURI *aUri,
                                       PRInt32 aDelay,
                                       PRBool aSameUri,
                                       PRBool *allowRefresh)
{
    *allowRefresh = PR_TRUE;
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsDownloadListener::OnProgressChange(nsIWebProgress *aWebProgress, 
                                     nsIRequest *aRequest, 
                                     PRInt32 aCurSelfProgress, 
                                     PRInt32 aMaxSelfProgress, 
                                     PRInt32 aCurTotalProgress, 
                                     PRInt32 aMaxTotalProgress)
{
  return OnProgressChange64(aWebProgress, aRequest,
                            aCurSelfProgress, aMaxSelfProgress,
                            aCurTotalProgress, aMaxTotalProgress);
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsDownloadListener::OnLocationChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           nsIURI *location)
{
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsDownloadListener::OnStatusChange(nsIWebProgress *aWebProgress, 
               nsIRequest *aRequest, 
               nsresult aStatus, 
               const PRUnichar *aMessage)
{
  // aMessage contains an error string, but it's so crappy that we don't want to use it.
  if (NS_FAILED(aStatus))
    DownloadDone(aStatus);

  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
nsDownloadListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  return NS_OK;
}

// Implementation of nsIWebProgressListener
/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
nsDownloadListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, 
                                    PRUint32 aStatus)
{
  // NSLog(@"State changed: state %u, status %u", aStateFlags, aStatus);  

  if (!mGotFirstStateChange) {
    mNetworkTransfer = ((aStateFlags & STATE_IS_NETWORK) != 0);
    mGotFirstStateChange = PR_TRUE;
  }
  
  // when the entire download finishes, stop the progress timer and clean up
  // the window and controller. We will get this even in the event of a cancel,
  // so this is the only place in the listener where we should kill the download.
  if ((aStateFlags & STATE_STOP) && (!mNetworkTransfer || (aStateFlags & STATE_IS_NETWORK))) {
    DownloadDone(aStatus);
  }
  return NS_OK; 
}

#pragma mark -

void
nsDownloadListener::InitDialog()
{
  // dialog has to be shown before the outlets get hooked up
  if (mURI)
  {
    nsCAutoString spec;

    // we need to be careful not to show a password in the url
    nsCAutoString userPassword;
    mURI->GetUserPass(userPassword);
    if (!userPassword.IsEmpty())
    {
      // ugh, build it by hand
      nsCAutoString hostport, path;
      mURI->GetScheme(spec);
      mURI->GetHostPort(hostport);
      mURI->GetPath(path);
      
      spec.Append("://");
      spec.Append(hostport);
      spec.Append(path);
    }
    else {
      nsCOMPtr<nsIURI> exposableURI;
      nsCOMPtr<nsIURIFixup> fixup(do_GetService("@mozilla.org/docshell/urifixup;1"));
      if (fixup && NS_SUCCEEDED(fixup->CreateExposableURI(mURI, getter_AddRefs(exposableURI))) && exposableURI)
        exposableURI->GetSpec(spec);
      else
        mURI->GetSpec(spec);
    }

    [mDownloadDisplay setSourceURL: [NSString stringWithUTF8String:spec.get()]];
  }

  nsAutoString pathStr;
  mDestinationFile->GetPath(pathStr);
  [mDownloadDisplay setDestinationPath: [NSString stringWith_nsAString:pathStr]];

  [mDownloadDisplay onStartDownload:IsFileSave()];
}

void
nsDownloadListener::PauseDownload()
{
  if (!mDownloadPaused && mRequest) 
  {
    mRequest->Suspend();
    mDownloadPaused = PR_TRUE;
  }
}

void
nsDownloadListener::ResumeDownload()
{
  if (mDownloadPaused && mRequest)
  {
    mRequest->Resume();
    mDownloadPaused = PR_FALSE;
  }
}

void
nsDownloadListener::CancelDownload()
{
  mUserCanceled = PR_TRUE;

  if (!mSentCancel)
  {
    if (mCancelable)
      mCancelable->Cancel(NS_BINDING_ABORTED);

    mSentCancel = PR_TRUE;
  }
    
  // when we cancel the download, we don't get any more notifications
  // from the backend (unlike for other transfer errors. So we have
  // to call DownloadDone ourselves.
  DownloadDone(NS_BINDING_ABORTED);
}

void
nsDownloadListener::DownloadDone(nsresult aStatus)
{
  // break the reference cycle
  mCancelable = nsnull;
  
  mDownloadStatus = aStatus;
  
  if (NS_FAILED(aStatus))
  {
    // delete the file we created in CHBrowserService::Show
    if (mDestinationFile)
    {
      mDestinationFile->Remove(PR_FALSE);
      mDestinationFile = nsnull;
    }
    mDestination = nsnull;
  }
  
  [mDownloadDisplay onEndDownload:(NS_SUCCEEDED(aStatus) && !mUserCanceled) statusCode:aStatus];
}

//
// DetachDownloadDisplay
//
// there are times when the download dislpay UI goes away before the
// listener (quit, for example). This alerts us that we should forget all
// about having any UI.
//
void
nsDownloadListener::DetachDownloadDisplay()
{
  mDownloadDisplay = nil;
}

PRBool
nsDownloadListener::IsDownloadPaused()
{
  return mDownloadPaused;
}

#pragma mark -
