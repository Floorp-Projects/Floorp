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

 
 
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#import "CHDownloadProgressDisplay.h"

#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDownload.h"
#include "nsIWebBrowserPersist.h"
#include "nsIURI.h"
#include "nsIRequest.h"
#include "nsILocalFile.h"

#include "nsIExternalHelperAppService.h"


// maybe this should replace nsHeaderSniffer too?
// NB. GetInterface() for nsIProgressEventSink is called on this class, if we wanted to implement it.
class nsDownloadListener :  public CHDownloader,
                            public nsIInterfaceRequestor,
                            public nsIDownload
{
public:
            nsDownloadListener();
    virtual ~nsDownloadListener();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIDOWNLOAD
    NS_DECL_NSITRANSFER
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIWEBPROGRESSLISTENER2
    
public:

    void InitDialog();
    
    virtual void PauseDownload();
    virtual void ResumeDownload();
    virtual void CancelDownload();
    virtual void DownloadDone(nsresult aStatus);
    virtual void DetachDownloadDisplay();
    virtual PRBool IsDownloadPaused();
    
private:

    nsCOMPtr<nsICancelable>         mCancelable;        // Object to cancel the download
    nsCOMPtr<nsIRequest>            mRequest;           // Request to hook on status change, allows pause/resume
    nsCOMPtr<nsIURI>                mURI;               // The URI of our source file. Null if we're saving a complete document.
    nsCOMPtr<nsIURI>                mDestination;       // Our destination URL.
    nsCOMPtr<nsILocalFile>          mDestinationFile;   // Our destination file.
    nsresult                        mDownloadStatus;		// status from last nofication
    PRInt64                         mStartTime;         // When the download started
    PRPackedBool                    mBypassCache;       // Whether we should bypass the cache or not.
    PRPackedBool                    mNetworkTransfer;     // true if the first OnStateChange has the NETWORK bit set
    PRPackedBool                    mGotFirstStateChange; // true after we've seen the first OnStateChange
    PRPackedBool                    mUserCanceled;        // true if the user canceled the download
    PRPackedBool                    mSentCancel;          // true when we've notified the backend of the cancel
    PRPackedBool                    mDownloadPaused;      // true when download is paused
};

