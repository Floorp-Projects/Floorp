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
<<<<<<< BrowserImplHistoryLstnr.cpp
	QAOutput("nsIHistoryListener::OnHistoryNewEntry()", 2);
    GetTheUri(theUri, 1);
=======
	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryNewEntry()", 2);

    CQaUtils::GetTheUri(theUri, 1);

>>>>>>> 1.6
	return NS_OK;
}


NS_IMETHODIMP CBrowserImpl::OnHistoryGoBack(nsIURI *theUri, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryGoBack()", 2);

<<<<<<< BrowserImplHistoryLstnr.cpp
    GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryGoBack() notification = ", *notify, 1);

=======
    CQaUtils::GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	CQaUtils::FormatAndPrintOutput("OnHistoryGoBack() notification = ", *notify, 1);

>>>>>>> 1.6
	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryGoForward(nsIURI *theUri, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryGoForward()", 2);

<<<<<<< BrowserImplHistoryLstnr.cpp
	GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryGoForward() notification = ", *notify, 1);

=======
	CQaUtils::GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	CQaUtils::FormatAndPrintOutput("OnHistoryGoForward() notification = ", *notify, 1);

>>>>>>> 1.6
	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryReload(nsIURI *theUri, PRUint32 reloadFlags, PRBool *notify)
{
<<<<<<< BrowserImplHistoryLstnr.cpp
	char flagString[100];

	QAOutput("nsIHistoryListener::OnHistoryReload()", 2);
=======
	char flagString[100];

	CQaUtils::QAOutput("nsIHistoryListener::OnHistoryReload()", 2);
>>>>>>> 1.6

<<<<<<< BrowserImplHistoryLstnr.cpp
	GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryReload() notification = ", *notify, 1);

	if (reloadFlags == 0x0000)
		strcpy(flagString, "LOAD_FLAGS_NONE");
	else if (reloadFlags == 0x0100)
		strcpy(flagString, "LOAD_FLAGS_BYPASS_CACHE");
	else if (reloadFlags == 0x0200)
		strcpy(flagString, "LOAD_FLAGS_BYPASS_PROXY");
	else if (reloadFlags == 0x0400)
		strcpy(flagString, "LOAD_FLAGS_CHARSET_CHANGE");
	else if (reloadFlags == (0x0200 | 0x0400))
		strcpy(flagString, "LOAD_RELOAD_BYPASS_PROXY_AND_CACHE");

	FormatAndPrintOutput("OnHistoryReload() flag = ", flagString, 2);

=======
	CQaUtils::GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	CQaUtils::FormatAndPrintOutput("OnHistoryReload() notification = ", *notify, 1);

	if (reloadFlags == 0x0000)
		strcpy(flagString, "LOAD_FLAGS_NONE");
	else if (reloadFlags == 0x0100)
		strcpy(flagString, "LOAD_FLAGS_BYPASS_CACHE");
	else if (reloadFlags == 0x0200)
		strcpy(flagString, "LOAD_FLAGS_BYPASS_PROXY");
	else if (reloadFlags == 0x0400)
		strcpy(flagString, "LOAD_FLAGS_CHARSET_CHANGE");
	else if (reloadFlags == (0x0200 | 0x0400))
		strcpy(flagString, "LOAD_RELOAD_BYPASS_PROXY_AND_CACHE");

	CQaUtils::FormatAndPrintOutput("OnHistoryReload() flag = ", flagString, 2);

>>>>>>> 1.6
	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryGotoIndex(PRInt32 theIndex, nsIURI *theUri, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryGotoIndex()", 2);

<<<<<<< BrowserImplHistoryLstnr.cpp
    GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryGotoIndex() notification = ", *notify, 1);

	FormatAndPrintOutput("OnHistoryGotoIndex() index = ", theIndex, 2);

=======
    CQaUtils::GetTheUri(theUri, 1);
	*notify = PR_TRUE;
	CQaUtils::FormatAndPrintOutput("OnHistoryGotoIndex() notification = ", *notify, 1);

	CQaUtils::FormatAndPrintOutput("OnHistoryGotoIndex() index = ", theIndex, 2);

>>>>>>> 1.6
	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryPurge(PRInt32 theNumEntries, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryPurge()", 2);

<<<<<<< BrowserImplHistoryLstnr.cpp
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryPurge() notification = ", *notify, 1);

	FormatAndPrintOutput("OnHistoryGotoIndex() theNumEntries = ", theNumEntries, 2);

=======
	*notify = PR_TRUE;
	CQaUtils::FormatAndPrintOutput("OnHistoryPurge() notification = ", *notify, 1);

	CQaUtils::FormatAndPrintOutput("OnHistoryGotoIndex() theNumEntries = ", theNumEntries, 2);

>>>>>>> 1.6
	return NS_OK;
}

