/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Epstein <depstein@netscape.com>
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

//
// test cases for nsIWebBrowser and nsIWebBrowserSetup
//

#include "Stdafx.h"
#include "TestEmbed.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "Tests.h"
#include "domwindow.h"
#include "QaUtils.h"
#include <stdio.h>
#include "nsIWebBrow.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// constructor for CNsIWebBrowser
CNsIWebBrowser::CNsIWebBrowser(nsIWebBrowser *mWebBrowser, CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser;
	qaBrowserImpl = mpBrowserImpl;
}

// destructor for CNsIWebBrowser
CNsIWebBrowser::~CNsIWebBrowser()
{

}

void CNsIWebBrowser::WBAddListener(PRInt16 displayMode)
{
	// AddWebBrowserListener
	nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsIContextMenuListener*, qaBrowserImpl)));
    rv = qaWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIContextMenuListener));
	RvTestResult(rv, "AddWebBrowserListener(). nsIContextMenuListener test", displayMode);
	RvTestResultDlg(rv, "AddWebBrowserListener(). nsIContextMenuListener test", true);
}

void CNsIWebBrowser::WBRemoveListener(PRInt16 displayMode)
{
	// RemoveWebBrowserListener
	nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsIContextMenuListener*, qaBrowserImpl)));

	rv = qaWebBrowser->RemoveWebBrowserListener(weakling, NS_GET_IID(nsIContextMenuListener));
	RvTestResult(rv, "RemoveWebBrowserListener(). nsIContextMenuListener test", displayMode);
	RvTestResultDlg(rv, "RemoveWebBrowserListener(). nsIContextMenuListener test");
}

void CNsIWebBrowser::WBGetContainerWindow(PRInt16 displayMode)
{
	// GetContainerWindow
	nsCOMPtr<nsIWebBrowserChrome> qaWebBrowserChrome;
	rv = qaWebBrowser->GetContainerWindow(getter_AddRefs(qaWebBrowserChrome));
	RvTestResult(rv, "nsIWebBrowser::GetContainerWindow() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowser::GetContainerWindow() test");
	if (!qaWebBrowserChrome)
		QAOutput("Didn't get web browser chrome object.", displayMode);
	else {
		rv = qaWebBrowserChrome->ShowAsModal();
		RvTestResult(rv, "nsIWebBrowserChrome::ShowAsModal() test", displayMode);
		RvTestResultDlg(rv, "nsIWebBrowserChrome::ShowAsModal() test");
	}
}

void CNsIWebBrowser::WBSetContainerWindow(PRInt16 displayMode)
{
	// SetContainerWindow

	rv = qaWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, qaBrowserImpl));
	RvTestResult(rv, "nsIWebBrowser::SetContainerWindow() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowser::SetContainerWindow() test");
}

void CNsIWebBrowser::WBGetURIContentListener(PRInt16 displayMode)
{
	// GetParentURIContentListener

	rv = qaWebBrowser->GetParentURIContentListener(getter_AddRefs(qaURIContentListener));
	RvTestResult(rv, "nsIWebBrowser::GetParentURIContentListener() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowser::GetParentURIContentListener() test");
	if (!qaURIContentListener)
		QAOutput("Didn't get uri content listener object.", displayMode);
	else {
		nsCOMPtr<nsISupports> qaSupports;
		rv = qaURIContentListener->GetLoadCookie(getter_AddRefs(qaSupports));
		RvTestResult(rv, "nsIURIContentListener::GetLoadCookie() test", displayMode);
		RvTestResultDlg(rv, "nsIURIContentListener::GetLoadCookie() test");
	}
}

void CNsIWebBrowser::WBSetURIContentListener(PRInt16 displayMode)
{
	// SetParentURIContentListener

	rv = qaWebBrowser->SetParentURIContentListener(NS_STATIC_CAST(nsIURIContentListener*, qaBrowserImpl));
	RvTestResult(rv, "nsIWebBrowser::SetParentURIContentListener() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowser::SetParentURIContentListener() test");
}

void CNsIWebBrowser::WBGetDOMWindow(PRInt16 displayMode)
{
	// GetContentDOMWindow
	nsCOMPtr<nsIDOMWindow> qaDOMWindow;
	rv = qaWebBrowser->GetContentDOMWindow(getter_AddRefs(qaDOMWindow));
	RvTestResult(rv, "nsIWebBrowser::GetContentDOMWindow() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowser::GetContentDOMWindow() test");
	if (!qaDOMWindow)
		QAOutput("Didn't get dom window object.", displayMode);
	else {
		rv = qaDOMWindow->ScrollTo(50,50);
		RvTestResult(rv, "nsIDOMWindow::ScrollTo() test", displayMode);
		RvTestResultDlg(rv, "nsIDOMWindow::ScrollTo() test");
	}
}

void CNsIWebBrowser::OnStartTests(UINT nMenuID)
{
	switch(nMenuID)
	{
		case ID_INTERFACES_NSIWEBBROWSER_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_ADDWEBBROWSERLISTENER :
			WBAddListener(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_REMOVEWEBBROWSERLISTENER :
			WBRemoveListener(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_GETCONTAINERWINDOW  :
			WBGetContainerWindow(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_SETCONTAINERWINDOW :
			WBSetContainerWindow(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_GETPARENTURICONTENTLISTENER :
			WBGetURIContentListener(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_SETPARENTURICONTENTLISTENER :
			WBSetURIContentListener(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_GETCONTENTDOMWINDOW  :
			WBGetDOMWindow(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSER_NSIWBSETUPSETPROPERTY  :
			WBSSetupProperty(2);
			break ;
	}
}

void CNsIWebBrowser::RunAllTests()
{
	WBAddListener(1);
	WBRemoveListener(1);
	WBSetContainerWindow(1);
	WBGetContainerWindow(1);
	WBSetURIContentListener(1);
	WBGetURIContentListener(1);
	WBGetDOMWindow(1);
	WBSSetupProperty(1);
}

void CNsIWebBrowser::WBSSetupProperty(PRInt16 displayMode)
{
	// nsIWebBrowserSetup methods

	nsCOMPtr <nsIWebBrowserSetup> qaWBSetup(do_QueryInterface(qaWebBrowser, &rv));
	RvTestResult(rv, "nsIWebBrowserSetup object test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserSetup object test");

	if (!qaWBSetup) {
		QAOutput("Didn't get WebBrowser Setup object.", displayMode);
		return;
	}

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_PLUGINS, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_PLUGINS, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_PLUGINS, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_PLUGINS, PR_FALSE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_JAVASCRIPT, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_JAVASCRIPT, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_JAVASCRIPT, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_JAVASCRIPT, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_JAVASCRIPT, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_JAVASCRIPT, PR_FALSE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_META_REDIRECTS, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_META_REDIRECTS, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_META_REDIRECTS, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_META_REDIRECTS, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_META_REDIRECTS, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_META_REDIRECTS, PR_FALSE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_SUBFRAMES, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_SUBFRAMES, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_SUBFRAMES, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_SUBFRAMES, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_SUBFRAMES, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_SUBFRAMES, PR_FALSE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_IMAGES, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_IMAGES, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_IMAGES, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_IMAGES, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_IMAGES, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_ALLOW_IMAGES, PR_FALSE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_FOCUS_DOC_BEFORE_CONTENT, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_FOCUS_DOC_BEFORE_CONTENT, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_FOCUS_DOC_BEFORE_CONTENT, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_FOCUS_DOC_BEFORE_CONTENT, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_FOCUS_DOC_BEFORE_CONTENT, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_FOCUS_DOC_BEFORE_CONTENT, PR_FALSE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_USE_GLOBAL_HISTORY, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_USE_GLOBAL_HISTORY, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_USE_GLOBAL_HISTORY, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_USE_GLOBAL_HISTORY, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_USE_GLOBAL_HISTORY, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_USE_GLOBAL_HISTORY, PR_FALSE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_IS_CHROME_WRAPPER, PR_TRUE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_IS_CHROME_WRAPPER, PR_TRUE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_IS_CHROME_WRAPPER, PR_TRUE)");

	rv = qaWBSetup->SetProperty(nsIWebBrowserSetup::SETUP_IS_CHROME_WRAPPER, PR_FALSE);
	RvTestResult(rv, "nsIWebBrowserSetup:SetProperty(SETUP_IS_CHROME_WRAPPER, PR_FALSE)", 1);
	RvTestResultDlg(rv, "nsIWebBrowserSetup:SetProperty(SETUP_IS_CHROME_WRAPPER, PR_FALSE)");
}