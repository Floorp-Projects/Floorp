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

// Local
#include "CPrintAttachment.h"
#include "CBrowserShell.h"
#include "UMacUnicode.h"
#include "ApplIDs.h"

// Gecko
#include "nsIPrintOptions.h"
#include "nsIServiceManagerUtils.h"
#include "nsIWebBrowserPrint.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"

// PowerPlant
#include <LProgressBar.h>
#include <LStaticText.h>

// Std Lib
#include <algorithm>
using namespace std;

// Constants
enum {
    paneID_ProgressBar      = 'Prog',
    paneID_StatusText       = 'Stat'
};

//*****************************************************************************
//***    CPrintProgressListener
//*****************************************************************************

class CPrintProgressListener : public nsIWebProgressListener,
                               public LListener
{
public:
                        CPrintProgressListener(nsIWebBrowserPrint *wbPrint,
                                               const nsAString& jobTitle);
    virtual             ~CPrintProgressListener();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER

	void	            ListenToMessage(MessageT inMessage,
                                        void* ioParam);
    
protected:
    nsIWebBrowserPrint  *mWBPrint;
    nsString            mJobTitle;
    
    LWindow             *mDialog;
    LProgressBar        *mDialogProgressBar;
    LStaticText         *mDialogStatusText;
};

CPrintProgressListener::CPrintProgressListener(nsIWebBrowserPrint* wbPrint,
                                               const nsAString& jobTitle) :
    mWBPrint(wbPrint), mJobTitle(jobTitle),
    mDialog(nsnull), mDialogProgressBar(nsnull), mDialogStatusText(nsnull)
{
    NS_INIT_ISUPPORTS();
    ThrowIfNil_(mWBPrint);
}

CPrintProgressListener::~CPrintProgressListener()
{
}

NS_IMPL_ISUPPORTS1(CPrintProgressListener,
                   nsIWebProgressListener);
                   
NS_IMETHODIMP CPrintProgressListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aStateFlags, PRUint32 aStatus)
{    
    if ((aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT) &&
        (aStateFlags & nsIWebProgressListener::STATE_START))
    {
        if (!mDialog) {
            try {
                mDialog = LWindow::CreateWindow(dlog_OS9PrintProgress, LCommander::GetTopCommander());
        	    UReanimator::LinkListenerToBroadcasters(this, mDialog, dlog_OS9PrintProgress);
        	    mDialogProgressBar = dynamic_cast<LProgressBar*>(mDialog->FindPaneByID(paneID_ProgressBar));
        	    mDialogStatusText = dynamic_cast<LStaticText*>(mDialog->FindPaneByID(paneID_StatusText));
        	    if (mDialogStatusText && mJobTitle.Length()) {
                    // The status text in the resource is something like: "Document: ^0" or "Printing: ^0"
                    // Get that text and replace "^0" with the job title. 
                    char buf[256];
                    Size bufLen;
                    mDialogStatusText->GetText(buf, sizeof(buf) - 1, &bufLen);
                    buf[bufLen] = '\0';
                    
                    nsCAutoString statusCString(buf);
                    nsCAutoString jobTitleCString;
                    CPlatformUCSConversion::GetInstance()->UCSToPlatform(mJobTitle, jobTitleCString);
                    statusCString.ReplaceSubstring(nsCAutoString("^0"), jobTitleCString);
                    
                    mDialogStatusText->SetText(const_cast<char *>(statusCString.get()), statusCString.Length());        	    
        	    }        	    
        	    mDialog->Show();
    	    }
    	    catch (...) {
    	        NS_ERROR("Failed to make print progress dialog - missing a resource?");
    	    }
        }
    }
    else if ((aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT) &&
             (aStateFlags & nsIWebProgressListener::STATE_STOP))
    {
        delete mDialog;
        mDialog = nsnull;
    }
    
    return NS_OK;
}

NS_IMETHODIMP CPrintProgressListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{    
    if (mDialogProgressBar) {
        if (aMaxSelfProgress != -1 && mDialogProgressBar->IsIndeterminate())
            mDialogProgressBar->SetIndeterminateFlag(false, false);
        else if (aMaxSelfProgress == -1 && !mDialogProgressBar->IsIndeterminate())
            mDialogProgressBar->SetIndeterminateFlag(true, true);

        if (!mDialogProgressBar->IsIndeterminate()) {
            PRInt32 aMax = max(0, aMaxSelfProgress);
            PRInt32 aVal = min(aMax, max(0, aCurSelfProgress));
            mDialogProgressBar->SetMaxValue(aMax);
            mDialogProgressBar->SetValue(aVal);
        }
    }
    
    return NS_OK;
}

NS_IMETHODIMP CPrintProgressListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPrintProgressListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    nsCAutoString cString; cString.AssignWithConversion(aMessage);
    printf("CPrintProgressListener::OnStatusChange: aStatus = %s\n", cString.get());
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPrintProgressListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void CPrintProgressListener::ListenToMessage(MessageT inMessage,
                                             void* ioParam)
{
    if (inMessage == msg_Cancel)
        mWBPrint->Cancel();
}


//*****************************************************************************
//***    CPrintAttachment
//*****************************************************************************

CPrintAttachment::CPrintAttachment(PaneIDT inBrowserPaneID,
                                   MessageT inMessage,
                                   Boolean inExecuteHost) :
    LAttachment(inMessage,inExecuteHost),
    mBrowserShell(nil), mBrowserShellPaneID(inBrowserPaneID)
{
}


CPrintAttachment::CPrintAttachment(LStream* inStream) :
    LAttachment(inStream),
    mBrowserShell(nil), mBrowserShellPaneID(PaneIDT_Unspecified)
{
	*inStream >> mBrowserShellPaneID;
}


CPrintAttachment::~CPrintAttachment()
{
}

//*****************************************************************************
//***    CPrintAttachment::LAttachment
//*****************************************************************************

void CPrintAttachment::SetOwnerHost(LAttachable* inOwnerHost)
{
    LAttachment::SetOwnerHost(inOwnerHost);
    
	if (mBrowserShell == nil) {
		if (mBrowserShellPaneID != PaneIDT_Unspecified) {
			LView*	container = GetTopmostView(dynamic_cast<LPane*>(mOwnerHost));
			if (container != nil) {
				LPane* targetPane = container->FindPaneByID(mBrowserShellPaneID);
				if (targetPane != nil)
					mBrowserShell = dynamic_cast<CBrowserShell*>(targetPane);
			}
		}
		else
			mBrowserShell = dynamic_cast<CBrowserShell*>(mOwnerHost);

	    Assert_(mBrowserShell != nil);		// Programmer error
	}
}

void CPrintAttachment::ExecuteSelf(MessageT inMessage, void *ioParam)
{
	mExecuteHost = true;

	if (inMessage == msg_CommandStatus) {
		SCommandStatus	*status = (SCommandStatus *)ioParam;
		if (status->command == cmd_Print) {
			*status->enabled = true;
			*status->usesMark = false;
			mExecuteHost = false; // we handled it
		}
		else if (status->command == cmd_PageSetup) {
			*status->enabled = true;
			*status->usesMark = false;
			mExecuteHost = false; // we handled it
		}
	}
	else if (inMessage == cmd_Print) {
	    DoPrint();
	    mExecuteHost = false; // we handled it
	}
	else if (inMessage == cmd_PageSetup) {
	    DoPageSetup();
	    mExecuteHost = false; // we handled it
	}
}

//*****************************************************************************
//***    CPrintAttachment::CPrintAttachment
//*****************************************************************************

void CPrintAttachment::DoPrint()
{
    nsCOMPtr<nsIWebBrowser> wb;
    mBrowserShell->GetWebBrowser(getter_AddRefs(wb));
    ThrowIfNil_(wb);
    nsCOMPtr<nsIWebBrowserPrint> wbPrint(do_GetInterface(wb));
    ThrowIfNil_(wbPrint);
    nsCOMPtr<nsIPrintSettings> settings;
    mBrowserShell->GetPrintSettings(getter_AddRefs(settings));
    ThrowIfNil_(settings);
    
    // The progress listener handles a print progress dialog. Don't do this on OS X because
    // the OS puts up a perfectly lovely one automatically. We're not at the mercy of the
    // printer driver for this as we are on Classic.
    nsCOMPtr<nsIWebProgressListener> listener;

    long version;
    PRBool runningOSX = (::Gestalt(gestaltSystemVersion, &version) == noErr && version >= 0x00001000);
    
    if (!runningOSX) {    
        // Get the title for the job and make a listener.
        nsXPIDLString jobTitle;
        nsCOMPtr<nsIWebBrowserChrome> chrome;
        mBrowserShell->GetWebBrowserChrome(getter_AddRefs(chrome));
        ThrowIfNil_(chrome);
        nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow(do_QueryInterface(chrome));
        ThrowIfNil_(siteWindow);
        siteWindow->GetTitle(getter_Copies(jobTitle));

        // Don't AddRef the listener. The nsIWebBrowserPrint holds the only ref to it.
        listener = new CPrintProgressListener(wbPrint, jobTitle);
    }

    // In any case, we don't want Gecko to display its XUL progress dialog.
    // Unfortunately, there is nothing in the printing API to control this -
    // it's done through a pref :-( If you are distributing your own default
    // prefs, this could be done there instead of programatically.
    nsCOMPtr<nsIPrefService> prefsService(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefsService) {
        nsCOMPtr<nsIPrefBranch> printBranch;
        prefsService->GetBranch("print.", getter_AddRefs(printBranch));
        if (printBranch) {
            printBranch->SetBoolPref("show_print_progress", PR_FALSE);
        }
    }
   
    nsresult rv = wbPrint->Print(settings, listener);
    ThrowIfError_(rv);
}


void CPrintAttachment::DoPageSetup()
{
    nsCOMPtr<nsIPrintOptions> printOptionsService = do_GetService("@mozilla.org/gfx/printoptions;1");
    ThrowIfNil_(printOptionsService);
    nsCOMPtr<nsIPrintSettings> printSettings;
    mBrowserShell->GetPrintSettings(getter_AddRefs(printSettings));
    ThrowIfNil_(printSettings);
    
    nsresult rv = printOptionsService->ShowPrintSetupDialog(printSettings);
    ThrowIfError_(rv);
}


LView* CPrintAttachment::GetTopmostView(LPane*	inStartPane)
{
										// Begin with the start Pane as a
										//   View. Will be nil if start Pane
										//   is nil or is not an LView.
	LView*	theView = dynamic_cast<LView*>(inStartPane);
	
	if (inStartPane != nil) {
										// Look at SuperView of start Pane
		LView*	superView = inStartPane->GetSuperView();
		
		while (superView != nil) {		// Move up view hierarchy until
			theView = superView;		//   reaching a nil SuperView
			superView = theView->GetSuperView();
		}
	}
	
	return theView;
}

