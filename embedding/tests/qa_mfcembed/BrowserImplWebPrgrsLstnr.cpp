/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

#ifdef _WINDOWS
  #include "stdafx.h"
#endif
#include "BrowserImpl.h"
#include "IBrowserFrameGlue.h"

#include "MfcEmbed.h"
#include "BrowserView.h"
#include "BrowserFrm.h"

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
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	PRInt32 nProgress = curTotalProgress;
	PRInt32 nProgressMax = maxTotalProgress;

	if (nProgressMax == 0)
		nProgressMax = LONG_MAX;

	if (curSelfProgress > maxSelfProgress)
		CBrowserView::QAOutput("nsIWebProgressListener::OnProgressChange(): Self progress complete!", 1);

	if (nProgress > nProgressMax)
	{
		nProgress = nProgressMax; // Progress complete
		CBrowserView::QAOutput("nsIWebProgressListener::OnProgressChange(): Progress Update complete!");
	}

	m_pBrowserFrameGlue->UpdateProgress(nProgress, nProgressMax);

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRInt32 progressStateFlags, PRUint32 status)
{
	char theDocType[100];
	char theStateType[100];
	char theTotalString[100];
	int displayMode = 1;
	CString strMsg;

	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

/*
    nsXPIDLString theReqName;
	nsresult rv;
	const char *CReqName;

	rv = request->GetName(getter_Copies(theReqName));
	if(NS_SUCCEEDED(rv))
	{
		CBrowserView::QAOutput("We got the request name.\r\n");
		CReqName = NS_ConvertUCS2toUTF8(theReqName).get();
		strMsg.Format("The request name = %s", CReqName); 
//		AfxMessageBox(strMsg);
		CBrowserView::WriteToOutputFile((char *)CReqName);
	}
	else
		CBrowserView::QAOutput("We didn't get the request name.\r\n");
*/
	CBrowserView::QAOutput("Entering nsIWebProgressListener::OnStateChange().");

	if (progressStateFlags & STATE_IS_DOCUMENT)		// DOCUMENT
	{
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

		else if (progressStateFlags & STATE_TRANSFERRING)
			strcpy(theStateType, "STATE_TRANSFERRING");		

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

//			CBrowserView::QAOutput("OnStateChange(): STATE_STOP, STATE_IS_DOCUMENT", 2);

	/*
			char *uriSpec;
			nsresult rv;
			nsCOMPtr<nsIURI> pURI;

			rv = (CBrowserView)mWebNav->GetCurrentURI( getter_AddRefs( pURI ) );
			rv = pURI->GetSpec(&uriSpec);
			CBrowserView::WriteToOutputFile("The new URL = ");
			CBrowserView::WriteToOutputFile(uriSpec);

			nsXPIDLCString uriString;
			nsCOMPtr<nsIURI> uri;
			nsCOMPtr<nsIChannel> channel(do_QueryInterface(request), &rv);
			channel->getURI(getter_AddRefs(uri));
			uri->GetSpec(getter_Copies(uriString));


  			CBrowserView::QAOutput("Start URL validation test().", 0);
			if (strcmp(uriSpec, CBrowserView.theUrl) == 0)
				CBrowserView::QAOutput("Url loaded successfully. Test Passed.", 2);	
			else
				CBrowserView::QAOutput("Url didn't load successfully. Test Failed.", 2);

	*/
		}
	}		// end STATE_IS_DOCUMENT
	else if (progressStateFlags & STATE_IS_REQUEST)		// REQUEST
	{
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
	}

	else if (progressStateFlags & STATE_IS_NETWORK)		// NETWORK
	{
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
	}
	else if (progressStateFlags & STATE_IS_WINDOW)		// WINDOW
	{
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
	}

//	sprintf(theTotalString, "%s %s%c %s%c %s", "OnStateChange():", theStateType, ',', 
//			theDocType, ',', CReqName);
	sprintf(theTotalString, "%s %s%c %s", "OnStateChange():", theStateType, ',', 
			theDocType);

	CBrowserView::QAOutput(theTotalString, displayMode);
	CBrowserView::QAOutput("Exiting nsIWebProgressListener::OnStateChange().\r\n");

    return NS_OK;
}


NS_IMETHODIMP CBrowserImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->UpdateCurrentURI(location);

    return NS_OK;
}

NS_IMETHODIMP 
CBrowserImpl::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->UpdateStatusBarText(aMessage);

    return NS_OK;
}



NS_IMETHODIMP 
CBrowserImpl::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
