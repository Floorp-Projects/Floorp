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

#ifndef UDownload_h__
#define UDownload_h__
#pragma once

#include "nsIDownload.h"
#include "nsIWebProgressListener.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIExternalHelperAppService.h"

#include "nsIURI.h"
#include "nsILocalFile.h"
#include "nsIWebBrowserPersist.h"

class ADownloadProgressView;

//*****************************************************************************
// CDownload
//
// Holds information used to display a single download in the UI. This object is
// created in one of two ways:
// (1) By nsExternalHelperAppHandler when Gecko encounters a  MIME type which
//     it doesn't itself handle. In this case, the notifications sent to
//     nsIDownload are controlled by nsExternalHelperAppHandler.
// (2) By the embedding app's file saving code when saving a web page or a link
//     target. See CHeaderSniffer.cpp. In this case, the notifications sent to
//     nsIDownload are controlled by the implementation of nsIWebBrowserPersist.
//*****************************************************************************   

class CDownload : public nsIDownload,
                  public nsIWebProgressListener,
                  public LBroadcaster
{
public:
    
    // Messages we broadcast to listeners.
    enum {
        msg_OnDLStart           = 57723,    // param is CDownload*
        msg_OnDLComplete,                   // param is CDownload*
        msg_OnDLProgressChange              // param is MsgOnDLProgressChangeInfo*
    };
       
    struct MsgOnDLProgressChangeInfo
    {
        MsgOnDLProgressChangeInfo(CDownload* broadcaster, PRInt32 curProgress, PRInt32 maxProgress) :
            mBroadcaster(broadcaster), mCurProgress(curProgress), mMaxProgress(maxProgress)
            { }
        
        CDownload *mBroadcaster;      
        PRInt32 mCurProgress, mMaxProgress;
    };

                            CDownload();
    virtual                 ~CDownload();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOWNLOAD
    NS_DECL_NSIWEBPROGRESSLISTENER
    
    virtual void            Cancel();
    virtual void            GetStatus(nsresult& aStatus)
                            { aStatus = mStatus; }

protected:
    void                    EnsureProgressView()
                            {
                                if (!sProgressView)
                                    CreateProgressView();
                            }
    virtual void            CreateProgressView();
    // sProgressView is a singleton. This will only be called once.
    
protected:
    nsCOMPtr<nsIURI>        mSource;
    nsCOMPtr<nsILocalFile>  mDestination;
    PRInt64                 mStartTime;
    PRInt32                 mPercentComplete;
    
    bool                    mGotFirstStateChange, mIsNetworkTransfer;
    bool                    mUserCanceled;
    nsresult                mStatus;
    
    // These two are mutually exclusive.
    nsCOMPtr<nsIWebBrowserPersist> mWebPersist;
    nsCOMPtr<nsIHelperAppLauncher> mHelperAppLauncher;
    
    static ADownloadProgressView *sProgressView;
};

//*****************************************************************************
// CHelperAppLauncherDialog
//
// The implementation of nsIExternalHelperAppService in Gecko creates one of
// these at the beginning of the download and calls its Show() method. Typically,
// this will create a non-modal dialog in which the user can decide whether to
// save the file to disk or open it with an application. This implementation
// just saves the file to disk unconditionally. The user can decide what they
// wish to do with the download from the progress dialog.
//*****************************************************************************   

class CHelperAppLauncherDialog : public nsIHelperAppLauncherDialog
{
public:
                            CHelperAppLauncherDialog();
    virtual                 ~CHelperAppLauncherDialog();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

protected:

};


//*****************************************************************************
// ADownloadProgressView
//
// An abstract class which handles the display and interaction with a download.
// Typically, it presents a progress dialog.
//*****************************************************************************

class ADownloadProgressView
{
    friend class CDownload;
    
    virtual void AddDownloadItem(CDownload *aDownloadItem) = 0;
    // A download is beginning. Initialize the UI for this download.
    // Throughout the download process, the CDownload will broadcast
    // status messages. The UI needs to call LBroadcaster::AddListener()
    // on the CDownload at this point in order to get the messages.
    
};


#endif // UDownload_h__
