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
 *   David Epstein <depstein@netscape.com> 
 *
 */

#ifdef _WINDOWS
  #include "stdafx.h"
#endif
#include "BrowserImpl.h"
#include "IBrowserFrameGlue.h"

#include "TestEmbed.h"
#include "BrowserView.h"
#include "BrowserFrm.h"

#include "QaUtils.h"

//*****************************************************************************
// CBrowserImpl::nsIHistoryListener Implementation
//*****************************************************************************   
//
// - Implements browser history listener methods. Activated through various events
//	 (i.e. forward, back, reload)
//
//	- Calls methods via the IBrowserFrameGlue interace 
//	  (available thru' the m_pBrowserFrameGlue member var)
//	  to do the actual statusbar/progress bar updates. 
//

class CBrowserView;


//*****************************************************************************
// CBrowserImpl::nsISHistoryListener methods
//*****************************************************************************   

NS_IMETHODIMP CBrowserImpl::OnHistoryNewEntry(nsIURI *theUri)
{
	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryNewEntry()", 2);

    CQaUtils::GetTheUri(theUri, 2);

	return NS_OK;
}


NS_IMETHODIMP CBrowserImpl::OnHistoryGoBack(nsIURI *theUri, PRBool *notify)
{
	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryGoBack()", 2);

    CQaUtils::GetTheUri(theUri, 2);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryGoForward(nsIURI *theUri, PRBool *notify)
{
	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryGoForward()", 2);

	CQaUtils::GetTheUri(theUri, 2);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryReload(nsIURI *theUri, PRUint32 reloadFlags, PRBool *notify)
{
	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryReload()", 2);

	CQaUtils::GetTheUri(theUri, 2);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryGotoIndex(PRInt32 theIndex, nsIURI *theUri, PRBool *notify)
{
	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryGotoIndex()", 2);

    CQaUtils::GetTheUri(theUri, 2);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryPurge(PRInt32 theNumEntries, PRBool *notify)
{
	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryPurge()", 2);

	return NS_OK;
}

