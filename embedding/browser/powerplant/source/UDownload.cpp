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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
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

#include "UDownload.h"

#include "nsIExternalHelperAppService.h"
#include "nsILocalFIleMac.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIRequest.h"
#include "netCore.h"
#include "nsIObserver.h"

#include "UDownloadDisplay.h"
#include "UMacUnicode.h"

#include "UNavServicesDialogs.h"


//*****************************************************************************
// CDownload
//*****************************************************************************
#pragma mark [CDownload]

ADownloadProgressView *CDownload::sProgressView;

CDownload::CDownload() :
    mGotFirstStateChange(false), mIsNetworkTransfer(false),
    mUserCanceled(false),
    mStatus(NS_OK)
{
}

CDownload::~CDownload()
{
}

NS_IMPL_ISUPPORTS2(CDownload, nsIDownload, nsIWebProgressListener)

#pragma mark -
#pragma mark [CDownload::nsIDownload]

/* void init (in nsIURI aSource, in nsILocalFile aTarget, in wstring aDisplayName, in nsIMIMEInfo aMIMEInfo, in long long startTime, in nsIWebBrowserPersist aPersist); */
NS_IMETHODIMP CDownload::Init(nsIURI *aSource, nsILocalFile *aTarget, const PRUnichar *aDisplayName, nsIMIMEInfo *aMIMEInfo, PRInt64 startTime, nsIWebBrowserPersist *aPersist)
{
    try {
        mSource = aSource;
        mDestination = aTarget;
        mStartTime = startTime;
        mPercentComplete = 0;
        if (aPersist) {
            mWebPersist = aPersist;
            // We have to break this circular ref when the download is done -
            // until nsIWebBrowserPersist supports weak refs - bug #163889.
            aPersist->SetProgressListener(this);
        }
        EnsureProgressView();
        sProgressView->AddDownloadItem(this);
    }
    catch (...) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

/* readonly attribute nsIURI source; */
NS_IMETHODIMP CDownload::GetSource(nsIURI * *aSource)
{
    NS_ENSURE_ARG_POINTER(aSource);
    NS_IF_ADDREF(*aSource = mSource);
    return NS_OK;
}

/* readonly attribute nsILocalFile target; */
NS_IMETHODIMP CDownload::GetTarget(nsILocalFile * *aTarget)
{
    NS_ENSURE_ARG_POINTER(aTarget);
    NS_IF_ADDREF(*aTarget = mDestination);
    return NS_OK;
}

/* readonly attribute nsIWebBrowserPersist persist; */
NS_IMETHODIMP CDownload::GetPersist(nsIWebBrowserPersist * *aPersist)
{
    NS_ENSURE_ARG_POINTER(aPersist);
    NS_IF_ADDREF(*aPersist = mWebPersist);
    return NS_OK;
}

/* readonly attribute PRInt32 percentComplete; */
NS_IMETHODIMP CDownload::GetPercentComplete(PRInt32 *aPercentComplete)
{
    NS_ENSURE_ARG_POINTER(aPercentComplete);
    *aPercentComplete = mPercentComplete;
    return NS_OK;
}

/* attribute wstring displayName; */
NS_IMETHODIMP CDownload::GetDisplayName(PRUnichar * *aDisplayName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CDownload::SetDisplayName(const PRUnichar * aDisplayName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long long startTime; */
NS_IMETHODIMP CDownload::GetStartTime(PRInt64 *aStartTime)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    *aStartTime = mStartTime;
    return NS_OK;
}

/* readonly attribute nsIMIMEInfo MIMEInfo; */
NS_IMETHODIMP CDownload::GetMIMEInfo(nsIMIMEInfo * *aMIMEInfo)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIWebProgressListener listener; */
NS_IMETHODIMP CDownload::GetListener(nsIWebProgressListener * *aListener)
{
    NS_ENSURE_ARG_POINTER(aListener);
    NS_IF_ADDREF(*aListener = (nsIWebProgressListener *)this);
    return NS_OK;
}

NS_IMETHODIMP CDownload::SetListener(nsIWebProgressListener * aListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIObserver observer; */
NS_IMETHODIMP CDownload::GetObserver(nsIObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CDownload::SetObserver(nsIObserver * aObserver)
{
    if (aObserver)
        aObserver->QueryInterface(NS_GET_IID(nsIHelperAppLauncher), getter_AddRefs(mHelperAppLauncher));
    return NS_OK;
}

#pragma mark -
#pragma mark [CDownload::nsIWebProgressListener]

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP CDownload::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    // For a file download via the external helper app service, we will never get a start
    // notification. The helper app service has gotten that notification before it created us.
    if (!mGotFirstStateChange) {
        mIsNetworkTransfer = ((aStateFlags & STATE_IS_NETWORK) != 0);
        mGotFirstStateChange = PR_TRUE;
        BroadcastMessage(msg_OnDLStart, this);
    }

    if (NS_FAILED(aStatus) && NS_SUCCEEDED(mStatus))
        mStatus = aStatus;
  
    // We will get this even in the event of a cancel,
    if ((aStateFlags & STATE_STOP) && (!mIsNetworkTransfer || (aStateFlags & STATE_IS_NETWORK))) {
        if (mWebPersist) {
            mWebPersist->SetProgressListener(nsnull);
            mWebPersist = nsnull;
        }
        mHelperAppLauncher = nsnull;
        BroadcastMessage(msg_OnDLComplete, this);
    }
        
    return NS_OK; 
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP CDownload::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    if (mUserCanceled) {
        if (mHelperAppLauncher)
            mHelperAppLauncher->Cancel();
        else if (aRequest)
            aRequest->Cancel(NS_BINDING_ABORTED);
        mUserCanceled = false;
    }
    if (aMaxTotalProgress == -1)
        mPercentComplete = -1;
    else
        mPercentComplete = ((float)aCurTotalProgress / (float)aMaxTotalProgress) * 100.0;
    
    MsgOnDLProgressChangeInfo info(this, aCurTotalProgress, aMaxTotalProgress);
    BroadcastMessage(msg_OnDLProgressChange, &info);

    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP CDownload::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP CDownload::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP CDownload::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

#pragma mark -
#pragma mark [CDownload Internal Methods]

void CDownload::Cancel()
{
    mUserCanceled = true;
    // nsWebBrowserPersist does the right thing: After canceling, next time through
    // OnStateChange(), aStatus != NS_OK. This isn't the case with nsExternalHelperAppService.
    if (!mWebPersist)
        mStatus = NS_ERROR_ABORT;
}

void CDownload::CreateProgressView()
{
    sProgressView = new CMultiDownloadProgress;
    ThrowIfNil_(sProgressView);
}


//*****************************************************************************
// CHelperAppLauncherDialog
//*****************************************************************************   
#pragma mark -
#pragma mark [CHelperAppLauncherDialog]

CHelperAppLauncherDialog::CHelperAppLauncherDialog()
{
}

CHelperAppLauncherDialog::~CHelperAppLauncherDialog()
{
}

NS_IMPL_ISUPPORTS1(CHelperAppLauncherDialog, nsIHelperAppLauncherDialog)

/* void show (in nsIHelperAppLauncher aLauncher, in nsISupports aContext); */
NS_IMETHODIMP CHelperAppLauncherDialog::Show(nsIHelperAppLauncher *aLauncher, nsISupports *aContext)
{
    return aLauncher->SaveToDisk(nsnull, PR_FALSE);
}

/* nsILocalFile promptForSaveToFile (in nsISupports aWindowContext, in wstring aDefaultFile, in wstring aSuggestedFileExtension); */
NS_IMETHODIMP CHelperAppLauncherDialog::PromptForSaveToFile(nsISupports *aWindowContext, const PRUnichar *aDefaultFile, const PRUnichar *aSuggestedFileExtension, nsILocalFile **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
 
    static bool sFirstTime = true;   
	UNavServicesDialogs::LFileDesignator	designator;

    if (sFirstTime) {
        // Get the default download folder and point Nav Sevices to it.
        nsCOMPtr<nsIFile> defaultDownloadDir;
        NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR, getter_AddRefs(defaultDownloadDir));
        if (defaultDownloadDir) {
            nsCOMPtr<nsILocalFileMac> macDir(do_QueryInterface(defaultDownloadDir));
            FSSpec defaultDownloadSpec;
            if (NS_SUCCEEDED(macDir->GetFSSpec(&defaultDownloadSpec)))
                designator.SetDefaultLocation(defaultDownloadSpec, true);
        }
        sFirstTime = false;
    }
	
	Str255  defaultName;
	CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(aDefaultFile), defaultName);
    bool result = designator.AskDesignateFile(defaultName);
    
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
        
    if (result) {
        FSSpec destSpec;
        designator.GetFileSpec(destSpec);
        nsCOMPtr<nsILocalFileMac> destFile;
        NS_NewLocalFileWithFSSpec(&destSpec, PR_TRUE, getter_AddRefs(destFile));
        if (!destFile)
            return NS_ERROR_FAILURE;
        *_retval = destFile;
        NS_ADDREF(*_retval);
        return NS_OK;
    }
    else
        return NS_ERROR_ABORT;
}

/* void showProgressDialog (in nsIHelperAppLauncher aLauncher, in nsISupports aContext); */
NS_IMETHODIMP CHelperAppLauncherDialog::ShowProgressDialog(nsIHelperAppLauncher *aLauncher, nsISupports *aContext)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
