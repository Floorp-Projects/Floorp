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

// Gecko
#include "nsIPrintingPromptService.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManagerUtils.h"
#include "nsIWebBrowserPrint.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"


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
       
    nsresult rv = wbPrint->Print(settings, nsnull);
    if (rv != NS_ERROR_ABORT)
        ThrowIfError_(rv);
}


void CPrintAttachment::DoPageSetup()
{
    nsCOMPtr<nsIWebBrowser> wb;
    mBrowserShell->GetWebBrowser(getter_AddRefs(wb));
    ThrowIfNil_(wb);
    nsCOMPtr<nsIDOMWindow> domWindow;
    wb->GetContentDOMWindow(getter_AddRefs(domWindow));
    ThrowIfNil_(domWindow);
    nsCOMPtr<nsIPrintSettings> settings;
    mBrowserShell->GetPrintSettings(getter_AddRefs(settings));
    ThrowIfNil_(settings);
    
    nsCOMPtr<nsIPrintingPromptService> printingPromptService =
             do_GetService("@mozilla.org/embedcomp/printingprompt-service;1");
    ThrowIfNil_(printingPromptService);

    nsresult rv = printingPromptService->ShowPageSetup(domWindow, settings, nsnull);
    if (rv != NS_ERROR_ABORT)
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

