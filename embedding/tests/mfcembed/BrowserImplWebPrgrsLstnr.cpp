/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
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
// - Calls methods via the IBrowserFrameGlue interace 
//    (available thru' the m_pBrowserFrameGlue member var)
//    to do the actual statusbar/progress bar updates. 
//

NS_IMETHODIMP CBrowserImpl::OnProgressChange(nsIWebProgress *progress,
                                             nsIRequest *request,
                                             PRInt32 curSelfProgress,
                                             PRInt32 maxSelfProgress,
                                             PRInt32 curTotalProgress,
                                             PRInt32 maxTotalProgress)
{
  if(! m_pBrowserFrameGlue) {
    // always return NS_OK
    return NS_OK;
  }

  PRInt32 nProgress = curTotalProgress;
  PRInt32 nProgressMax = maxTotalProgress;

  if (nProgressMax == 0)
    nProgressMax = LONG_MAX;

  if (nProgress > nProgressMax)
    nProgress = nProgressMax; // Progress complete

  m_pBrowserFrameGlue->UpdateProgress(nProgress, nProgressMax);

  return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnStateChange(nsIWebProgress *progress,
                                          nsIRequest *request,
                                          PRUint32 progressStateFlags,
                                          nsresult status)
{
  if(! m_pBrowserFrameGlue) {
    // always return NS_OK
    return NS_OK;
  }

  if ((progressStateFlags & STATE_START) && (progressStateFlags & STATE_IS_DOCUMENT))
  {
    // Navigation has begun
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
  if(! m_pBrowserFrameGlue) {
    // always return NS_OK
    return NS_OK;
  }

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
  if(m_pBrowserFrameGlue)
    m_pBrowserFrameGlue->UpdateStatusBarText(aMessage);

  return NS_OK;
}



NS_IMETHODIMP 
CBrowserImpl::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
  m_pBrowserFrameGlue->UpdateSecurityStatus(state);

  return NS_OK;
}
