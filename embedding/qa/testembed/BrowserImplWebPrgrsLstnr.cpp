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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *   David Epstein <depstein@netscape.com>
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

#ifdef _WINDOWS
  #include "stdafx.h"
#endif
#include "BrowserImpl.h"
#include "IBrowserFrameGlue.h"

#include "TestEmbed.h"
#include "BrowserView.h"
#include "BrowserFrm.h"

#include "QaUtils.h"
#include "Tests.h"

//*****************************************************************************
// CBrowserImpl::nsIWebProgressListener Implementation
//*****************************************************************************   
//
// - Implements browser progress update functionality 
//   while loading a page into the embedded browser
//
//	- Calls methods via the IBrowserFrameGlue interace 
//	  (available thru' the m_pBrowserFrameGlue member var)
//	  to do the actual statusbar/progress bar updates. 
//

class CBrowserView;

NS_IMETHODIMP CBrowserImpl::OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                                  PRInt32 curSelfProgress, PRInt32 maxSelfProgress,
                                                  PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
	// nsCString
	nsCString stringMsg;

	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	QAOutput("Entering nsIWebProgLstnr::OnProgressChange().");

	PRInt32 nProgress = curTotalProgress;
	PRInt32 nProgressMax = maxTotalProgress;

	RequestName(request, stringMsg);

	if (nProgressMax == 0)
		nProgressMax = LONG_MAX;

	FormatAndPrintOutput("OnProgressChange(): curSelfProgress value = ", curSelfProgress, 1); 
	FormatAndPrintOutput("OnProgressChange(): maxSelfProgress value = ", maxSelfProgress, 1);
	FormatAndPrintOutput("OnProgressChange(): curTotalProgress value = ", nProgress, 1); 
	FormatAndPrintOutput("OnProgressChange(): maxTotalProgress value = ", nProgressMax, 1);

	if (curSelfProgress == maxSelfProgress && maxSelfProgress != -1)
	{
		QAOutput("nsIWebProgLstnr::OnProgressChange(): Self progress complete!", 1);

		// web progress DOMWindow test
		WebProgDOMWindowTest(progress, "OnProgressChange()", 1);
	}

	if (nProgress > nProgressMax && nProgressMax != -1)
	{
		nProgress = nProgressMax; // Progress complete

		QAOutput("nsIWebProgLstnr::OnProgressChange(): Progress Update complete!", 1);
	}
	WebProgIsDocLoadingTest(progress, "OnProgressChange()", 1);
	m_pBrowserFrameGlue->UpdateProgress(nProgress, nProgressMax);

	QAOutput("Exiting nsIWebProgLstnr::OnProgressChange().\r\n");
  
	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                          PRUint32 progressStateFlags, nsresult status)
{
	char theDocType[100];
	char theStateType[100];
	int displayMode = 1;
	nsCString stringMsg;

	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	QAOutput("Entering nsIWebProgLstnr::OnStateChange().");

	RequestName(request, stringMsg);	// nsIRequest::GetName() test

	if (progressStateFlags & STATE_IS_DOCUMENT)		// DOCUMENT
	{
		displayMode = 1;
		strcpy(theDocType, "DOCUMENT");
		if (progressStateFlags & STATE_START)
		{ 
			// Navigation has begun
			strcpy(theStateType, "STATE_START");
			displayMode = 2;

			if(m_pBrowserFrameGlue)
				m_pBrowserFrameGlue->UpdateBusyState(PR_TRUE);
		}

		else if (progressStateFlags & STATE_REDIRECTING)
			strcpy(theStateType, "STATE_REDIRECTING");

		else if (progressStateFlags & STATE_TRANSFERRING) {
			strcpy(theStateType, "STATE_TRANSFERRING");	
		}

		else if (progressStateFlags & STATE_NEGOTIATING)
			strcpy(theStateType, "STATE_NEGOTIATING");				

		else if (progressStateFlags & STATE_STOP)
		{
			// We've completed the navigation

			strcpy(theStateType, "STATE_STOP");
			displayMode = 2;

			m_pBrowserFrameGlue->UpdateBusyState(PR_FALSE);
			m_pBrowserFrameGlue->UpdateProgress(0, 100);       // Clear the prog bar
			m_pBrowserFrameGlue->UpdateStatusBarText(nsnull);  // Clear the status bar

		// web progress DOMWindow test
			WebProgDOMWindowTest(progress, "OnStateChange()", 1);
		}

		onStateChangeString(theStateType, theDocType, stringMsg, status, displayMode);

	}		// end STATE_IS_DOCUMENT

	if (progressStateFlags & STATE_IS_REQUEST)		// REQUEST
	{
		displayMode = 1;
		strcpy(theDocType, "REQUEST");
		if (progressStateFlags & STATE_START)
			strcpy(theStateType, "STATE_START");
		else if (progressStateFlags & STATE_REDIRECTING)
			strcpy(theStateType, "STATE_REDIRECTING");

		else if (progressStateFlags & STATE_TRANSFERRING)
			strcpy(theStateType, "STATE_TRANSFERRING");		

		else if (progressStateFlags & STATE_NEGOTIATING)
			strcpy(theStateType, "STATE_NEGOTIATING");				

		else if (progressStateFlags & STATE_STOP)
			strcpy(theStateType, "STATE_STOP");
		
		onStateChangeString(theStateType, theDocType, stringMsg, status, displayMode);
	}

	if (progressStateFlags & STATE_IS_NETWORK)		// NETWORK
	{
		displayMode = 1;
		strcpy(theDocType, "NETWORK");
		if (progressStateFlags & STATE_START)
			strcpy(theStateType, "STATE_START");
		else if (progressStateFlags & STATE_REDIRECTING)
			strcpy(theStateType, "STATE_REDIRECTING");

		else if (progressStateFlags & STATE_TRANSFERRING)
			strcpy(theStateType, "STATE_TRANSFERRING");		

		else if (progressStateFlags & STATE_NEGOTIATING)
			strcpy(theStateType, "STATE_NEGOTIATING");				

		else if (progressStateFlags & STATE_STOP)
			strcpy(theStateType, "STATE_STOP");
		
		onStateChangeString(theStateType, theDocType, stringMsg, status, displayMode);

	}
	if (progressStateFlags & STATE_IS_WINDOW)		// WINDOW
	{
		displayMode = 1;
		strcpy(theDocType, "WINDOW");
		if (progressStateFlags & STATE_START)
			strcpy(theStateType, "STATE_START");
		else if (progressStateFlags & STATE_REDIRECTING)
			strcpy(theStateType, "STATE_REDIRECTING");

		else if (progressStateFlags & STATE_TRANSFERRING)
			strcpy(theStateType, "STATE_TRANSFERRING");		

		else if (progressStateFlags & STATE_NEGOTIATING)
			strcpy(theStateType, "STATE_NEGOTIATING");				

		else if (progressStateFlags & STATE_STOP)
			strcpy(theStateType, "STATE_STOP");
		
		onStateChangeString(theStateType, theDocType, stringMsg, status, displayMode);

	}
	// web progress isDocumentLoading test
	WebProgIsDocLoadingTest(progress, "OnStateChange()", 1);

	QAOutput("Exiting nsIWebProgLstnr::OnStateChange().\r\n");

    return NS_OK;
}


NS_IMETHODIMP CBrowserImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
	nsCString stringMsg;

	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	QAOutput("Entering nsIWebProgLstnr::OnLocationChange().");

	// test nsIURI parameter
	GetTheURI(location, 1);
 
	RequestName(aRequest, stringMsg);  

	PRBool isSubFrameLoad = PR_FALSE; // Is this a subframe load
	if (aWebProgress) {
		nsCOMPtr<nsIDOMWindow>  domWindow;
		nsCOMPtr<nsIDOMWindow>  topDomWindow;
		aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
		if (domWindow) { // Get root domWindow
			domWindow->GetTop(getter_AddRefs(topDomWindow));
		}
		if (domWindow != topDomWindow)
			isSubFrameLoad = PR_TRUE;
		WebProgIsDocLoadingTest(aWebProgress, "OnLocationChange()", 1);
	}

	if (!isSubFrameLoad) // Update urlbar only if it is not a subframe load
	  m_pBrowserFrameGlue->UpdateCurrentURI(location);

  	QAOutput("Exiting nsIWebProgLstnr::OnLocationChange().\r\n");
    return NS_OK;
}

NS_IMETHODIMP 
CBrowserImpl::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
	nsCString stringMsg;

	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	QAOutput("Entering nsIWebProgLstnr::OnStatusChange().");

	RequestName(aRequest, stringMsg);

			// status result test (id number)
	FormatAndPrintOutput("OnStatusChange(): Status id = ", aStatus, 1);

			// web progress DOMWindow test (typically the host name)
	WebProgDOMWindowTest(aWebProgress, "OnStatusChange(): web prog DOM window test", 1);
			// web progress isDocumentLoading test
	WebProgIsDocLoadingTest(aWebProgress, "OnStatusChange()", 1);

	m_pBrowserFrameGlue->UpdateStatusBarText(aMessage);

	QAOutput("Exiting nsIWebProgLstnr::OnStatusChange().\r\n");

    return NS_OK;
}


NS_IMETHODIMP 
CBrowserImpl::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
	nsCString stringMsg;

	QAOutput("Entering nsIWebProgLstnr::OnSecurityChange().");

	RequestName(aRequest, stringMsg);

	if (state & STATE_IS_SECURE)
	{
		QAOutput("OnSecurityChange():STATE_IS_SECURE. All docs & subdocs are https.");
		if ((state & 0xFFFF0000) == STATE_SECURE_HIGH)
			QAOutput("OnSecurityChange(): STATE_SECURE_HIGH state");
		else if (state & STATE_SECURE_MED)
			QAOutput("OnSecurityChange(): STATE_SECURE_MED state");
		else if (state & STATE_SECURE_LOW)
			QAOutput("OnSecurityChange(): STATE_SECURE_LOW state");
	}
	else if (state & STATE_IS_BROKEN)
		QAOutput("OnSecurityChange():STATE_IS_BROKEN. Mixed: some docs are https.");
	else if ((state & 0xFFFF) ==  STATE_IS_INSECURE)
		QAOutput("OnSecurityChange():STATE_IS_INSECURE. Nothing is https.");

				// web progress DOMWindow test
	WebProgDOMWindowTest(aWebProgress, "OnSecurityChange()", 1);
				// web progress isDocumentLoading test
	WebProgIsDocLoadingTest(aWebProgress, "OnSecurityChange()", 1);

	QAOutput("Exiting nsIWebProgLstnr::OnSecurityChange().\r\n");

    return NS_OK;
}
