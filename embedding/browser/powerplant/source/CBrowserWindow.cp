/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
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

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"
#include "nsIWindowCreator.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsRect.h"
#include "nsIWebProgressListener.h"

#include "CBrowserWindow.h"
#include "CBrowserShell.h"
#include "CBrowserMsgDefs.h"
#include "CThrobber.h"
#include "ApplIDs.h"
#include "UMacUnicode.h"

#include <LEditText.h>
#include <LStaticText.h>
#include <LWindowHeader.h>
#include <LBevelButton.h>
#include <LProgressBar.h>
#include <LIconControl.h>

#if PP_Target_Carbon
#include <UEventMgr.h>
#endif

#include <algorithm>
using namespace std;

#include <InternetConfig.h>

// CBrowserWindow:
// A simple browser window that hooks up a CWebShell to a minimal set of controls
// (Back, Forward and Stop buttons + URL field + status bar).


enum
{
    paneID_BackButton       = 'Back',
    paneID_ForwardButton    = 'Forw',
    paneID_ReloadButton     = 'RLoa',
    paneID_StopButton       = 'Stop',
    paneID_URLField         = 'gUrl',
    paneID_StatusBar        = 'Stat',
    paneID_Throbber         = 'THRB',
    paneID_ProgressBar      = 'Prog',
    paneID_LockIcon         = 'Lock'
};


// ---------------------------------------------------------------------------
//  ¥ CBrowserWindow                        Default Constructor       [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow() :
    mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
    mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL),
    mProgressBar(NULL), mLockIcon(NULL)
{
}


// ---------------------------------------------------------------------------
//  ¥ CBrowserWindow                        Parameterized Constructor [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow(LCommander*      inSuperCommander,
                               const Rect&      inGlobalBounds,
                               ConstStringPtr   inTitle,
                               SInt16           inProcID,
                               UInt32           inAttributes,
                               WindowPtr        inBehind) :
    LWindow(inSuperCommander, inGlobalBounds, inTitle, inProcID, inAttributes, inBehind),
    mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
    mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL),
    mProgressBar(NULL), mLockIcon(NULL)
{
}


// ---------------------------------------------------------------------------
//  ¥ CBrowserWindow                        Stream Constructor        [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow(LStream* inStream) :
    LWindow(inStream),
    mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
    mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL),
    mProgressBar(NULL), mLockIcon(NULL)
{
}


// ---------------------------------------------------------------------------
//  ¥ ~CBrowserWindow                       Destructor                [public]
// ---------------------------------------------------------------------------

CBrowserWindow::~CBrowserWindow()
{
    if (mBrowserShell)
        mBrowserShell->RemoveListener(this);
}


// ---------------------------------------------------------------------------
//  ¥ FinishCreateSelf
// ---------------------------------------------------------------------------

void CBrowserWindow::FinishCreateSelf()
{
    mBrowserShell = dynamic_cast<CBrowserShell *>(FindPaneByID(CBrowserShell::paneID_MainBrowser));
    ThrowIfNil_(mBrowserShell);
    mBrowserShell->AddListener(this);
    SetLatentSub(mBrowserShell);
    
    // Find our subviews - Depending on our chrome flags, we may or may
    // not have any of these subviews so don't fail if they don't exist
    mURLField = dynamic_cast<LEditText*>(FindPaneByID(paneID_URLField));
    mStatusBar = dynamic_cast<LStaticText*>(FindPaneByID(paneID_StatusBar));
    mThrobber = dynamic_cast<CThrobber*>(FindPaneByID(paneID_Throbber));
    mLockIcon = dynamic_cast<LIconControl*>(FindPaneByID(paneID_LockIcon));
    mProgressBar = dynamic_cast<LProgressBar*>(FindPaneByID(paneID_ProgressBar));
    if (mProgressBar)
       mProgressBar->Hide();
    
    mBackButton = dynamic_cast<LControl*>(FindPaneByID(paneID_BackButton));
    if (mBackButton)
        mBackButton->Disable();
    mForwardButton = dynamic_cast<LControl*>(FindPaneByID(paneID_ForwardButton));
    if (mForwardButton)
        mForwardButton->Disable();
    mReloadButton = dynamic_cast<LControl*>(FindPaneByID(paneID_ReloadButton));
    if (mReloadButton)
        mReloadButton->Disable();   
    mStopButton = dynamic_cast<LControl*>(FindPaneByID(paneID_StopButton));
    if (mStopButton)
        mStopButton->Disable();

    UReanimator::LinkListenerToControls(this, this, view_BrowserToolBar);
    StartListening();
    StartBroadcasting();    
}


// ---------------------------------------------------------------------------
//  ¥ ListenToMessage
// ---------------------------------------------------------------------------

void CBrowserWindow::ListenToMessage(MessageT       inMessage,
                                     void*          ioParam)
{
    switch (inMessage)
    {
        case msg_OnNetStartChange:
            {
                if (mProgressBar) {
                    mProgressBar->Show();
                    mProgressBar->SetIndeterminateFlag(true, true);
                }
               
                if (mThrobber)
                    mThrobber->Start();

                if (mStopButton)
                    mStopButton->Enable();
            }
            break;
        
        case msg_OnNetStopChange:
            {
                if (mThrobber)
                    mThrobber->Stop();

                if (mProgressBar) {
                    if (mProgressBar->IsIndeterminate())
                        mProgressBar->Stop();
                    mProgressBar->Hide();
                }

                // Enable back, forward, reload, stop
                if (mBackButton)
                    mBrowserShell->CanGoBack() ? mBackButton->Enable() : mBackButton->Disable();
                if (mForwardButton)
                    mBrowserShell->CanGoForward() ? mForwardButton->Enable() : mForwardButton->Disable();
                if (mReloadButton)
                    mReloadButton->Enable();
                if (mStopButton)
                    mStopButton->Disable();
                    
                // Wipe the status clean
                if (mStatusBar)
                    mStatusBar->SetText(nsnull, 0);
            }
            break;
 
         case msg_OnProgressChange:
            {
                const MsgOnProgressChangeInfo *info = reinterpret_cast<MsgOnProgressChangeInfo*>(ioParam);
                
                if (mProgressBar) {
                    if (info->mMaxProgress != -1 && mProgressBar->IsIndeterminate())
                        mProgressBar->SetIndeterminateFlag(false, false);
                    else if (info->mMaxProgress == -1 && !mProgressBar->IsIndeterminate())
                        mProgressBar->SetIndeterminateFlag(true, true);

                    if (!mProgressBar->IsIndeterminate()) {
                        PRInt32 aMax = max(0, info->mMaxProgress);
                        PRInt32 aVal = min(aMax, max(0, info->mCurProgress));
                        mProgressBar->SetMaxValue(aMax);
                        mProgressBar->SetValue(aVal);
                    }
                }
            }
            break;
                
         case msg_OnLocationChange:
            {
                const MsgLocationChangeInfo *info = reinterpret_cast<MsgLocationChangeInfo*>(ioParam);
                
                if (mURLField)
                    mURLField->SetText(info->mURLSpec, strlen(info->mURLSpec));
            }
            break;

         case msg_OnStatusChange:
            {
                const MsgStatusChangeInfo *info = reinterpret_cast<MsgStatusChangeInfo*>(ioParam);
                
                if (mStatusBar) {
                    nsCAutoString cStr;
                    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(info->mMessage), cStr);
                    mStatusBar->SetText(const_cast<char *>(cStr.get()), cStr.Length());
                }
            }
            break;
            
        case msg_OnSecurityChange:
            {
                const MsgSecurityChangeInfo *info = reinterpret_cast<MsgSecurityChangeInfo*>(ioParam);

                if (mLockIcon) {
                    SInt16 iconID;
                    switch (info->mState & 0x0000FFFF)
                    {
                        case nsIWebProgressListener::STATE_IS_SECURE:
                            iconID = icon_LockSecure;
                            break;
                        case nsIWebProgressListener::STATE_IS_BROKEN:
                            iconID = icon_LockBroken;
                            break;
                        case nsIWebProgressListener::STATE_IS_INSECURE:
                            iconID = icon_LockInsecure;
                            break;
                        default:
                            NS_ERROR("Unknown security state!");
                            iconID = icon_LockInsecure;
                            break;
                    }
                    // The kControlIconResourceIDTag requires Appearance 1.1
                    // That's present on 8.5 and up and mozilla requires 8.6.
                    mLockIcon->SetDataTag(0, kControlIconResourceIDTag, sizeof(iconID), &iconID);
                }
            }
            break;

        case msg_OnChromeStatusChange:
            {
                const MsgChromeStatusChangeInfo *info = reinterpret_cast<MsgChromeStatusChangeInfo*>(ioParam);
                
                if (mStatusBar) {
                    nsCAutoString cStr;
                    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(info->mStatus), cStr);
                    mStatusBar->SetText(const_cast<char *>(cStr.get()), cStr.Length());
                }
            }
            break;
           
        default:
            ProcessCommand(inMessage, ioParam);
            break;
    }
}

// ---------------------------------------------------------------------------
//  ¥ ObeyCommand
// ---------------------------------------------------------------------------

Boolean CBrowserWindow::ObeyCommand(CommandT            inCommand,
                                    void                *ioParam)
{
#pragma unused(ioParam)

    Boolean cmdHandled = true;
    
    switch (inCommand)
    {            
        case paneID_BackButton:
            mBrowserShell->Back();
            break;

        case paneID_ForwardButton:
            mBrowserShell->Forward();
            break;
            
        case paneID_ReloadButton:
            mBrowserShell->Reload();
            break;

        case paneID_StopButton:
            mBrowserShell->Stop();
            break;
            
        case cmd_Reload:
            mBrowserShell->Reload();
            break;

        case paneID_URLField:
            {
                SInt32    urlTextLen;
                mURLField->GetText(nil, 0, &urlTextLen);
                StPointerBlock  urlTextPtr(urlTextLen, true, false);
                mURLField->GetText(urlTextPtr.Get(), urlTextLen, &urlTextLen);
                mBrowserShell->LoadURL(Substring(urlTextPtr.Get(), urlTextPtr.Get() + urlTextLen));
            }
            break;
            
        default:
            cmdHandled = false;
            break;
    }

    if (!cmdHandled)
        cmdHandled = LWindow::ObeyCommand(inCommand, ioParam);
    return cmdHandled;
}


#if 0
NS_METHOD CBrowserWindow::SetTitleFromDOMDocument()
{
    nsresult rv;
    
    nsCOMPtr<nsIDOMWindow> domWindow;
    rv = mBrowserChrome->GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(domWindow));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = domWindow->GetDocument(getter_AddRefs(domDoc));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIDOMElement> domDocElem;
    rv = domDoc->GetDocumentElement(getter_AddRefs(domDocElem));
    if (NS_FAILED(rv)) return rv;

    nsAutoString windowTitle;
    domDocElem->GetAttribute(NS_LITERAL_STRING("title"), windowTitle);
    if (!windowTitle.IsEmpty()) {
        Str255 pStr;
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(windowTitle, pStr);
        SetDescriptor(pStr);
    }
    else
        rv = NS_ERROR_FAILURE;
        
    return rv;
}
#endif
