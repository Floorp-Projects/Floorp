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
 *   Chak Nanga <chak@netscape.com> 
 *
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

#ifdef _WINDOWS
  #include "stdafx.h"
#endif
#include "BrowserImpl.h"
#include "IBrowserFrameGlue.h"

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

	if (nProgress > nProgressMax)
		nProgress = nProgressMax; // Progress complete

	m_pBrowserFrameGlue->UpdateProgress(nProgress, nProgressMax);

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRInt32 progressStateFlags, PRUint32 status)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

    if ((progressStateFlags & STATE_START) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
		// Navigation has begun

		if(m_pBrowserFrameGlue)
			m_pBrowserFrameGlue->UpdateBusyState(PR_TRUE);
    }

    if ((progressStateFlags & STATE_STOP) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
		// We've completed the navigation

		m_pBrowserFrameGlue->UpdateBusyState(PR_FALSE);
		m_pBrowserFrameGlue->UpdateProgress(0, 100);       // Clear the prog bar
		m_pBrowserFrameGlue->UpdateStatusBarText(nsnull);  // Clear the status bar
    }

    return NS_OK;
}


NS_IMETHODIMP CBrowserImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

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

  }

  if (!isSubFrameLoad) // Update urlbar only if it is not a subframe load
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
    m_pBrowserFrameGlue->UpdateSecurityStatus(state);

    return NS_OK;
}
