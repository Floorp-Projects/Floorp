/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Simon Fraser <sfraser@netscape.com>
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

 
 
#import <Foundation/Foundation.h>
#import <Appkit/Appkit.h>

#import "CHDownloadProgressDisplay.h"

#include "nsString.h"
#include "nsIDownload.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserPersist.h"
#include "nsIURI.h"
#include "nsILocalFile.h"
#include "nsExternalHelperAppService.h"


// maybe this should replace nsHeaderSniffer too?

class nsDownloadListener :  public CHDownloader,
                            public nsIDownload,
                            public nsIWebProgressListener
{
public:
            nsDownloadListener(DownloadControllerFactory* inDownloadControllerFactory);
    virtual ~nsDownloadListener();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIDOWNLOAD
    NS_DECL_NSIWEBPROGRESSLISTENER
  
public:
    //void BeginDownload();
    void InitDialog();
    
    virtual void PauseDownload();
    virtual void ResumeDownload();
    virtual void CancelDownload();
    virtual void DownloadDone();
    virtual void DetachDownloadDisplay();
    
private:

    // These two are mutually exclusive
    nsCOMPtr<nsIWebBrowserPersist>  mWebPersist;        // Our web persist object.
    nsCOMPtr<nsIHelperAppLauncher>  mHelperAppLauncher; // If we're talking to uriloader
    
    nsCOMPtr<nsIURI>                mURI;               // The URI of our source file. Null if we're saving a complete document.
    nsCOMPtr<nsILocalFile>          mDestination;       // Our destination URL.
    PRInt64                         mStartTime;         // When the download started
    PRPackedBool                    mBypassCache;       // Whether we should bypass the cache or not.
    PRPackedBool                    mNetworkTransfer;     // true if the first OnStateChange has the NETWORK bit set
    PRPackedBool                    mGotFirstStateChange; // true after we've seen the first OnStateChange
    PRPackedBool                    mUserCanceled;        // true if the user canceled the download
};

