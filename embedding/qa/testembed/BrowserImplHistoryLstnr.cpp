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

NS_IMETHODIMP CBrowserImpl::OnHistoryNewEntry(nsIURI *theURI)
{
	QAOutput("nsIHistoryListener::OnHistoryNewEntry()", 2);
    GetTheURI(theURI, 1);
	return NS_OK;
}


NS_IMETHODIMP CBrowserImpl::OnHistoryGoBack(nsIURI *theURI, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryGoBack()", 2);

    GetTheURI(theURI, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryGoBack() notification = ", *notify, 1);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryGoForward(nsIURI *theURI, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryGoForward()", 2);

	GetTheURI(theURI, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryGoForward() notification = ", *notify, 1);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryReload(nsIURI *theURI, PRUint32 reloadFlags, PRBool *notify)
{
	char flagString[200];

	QAOutput("nsIHistoryListener::OnHistoryReload()", 2);

	GetTheURI(theURI, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryReload() notification = ", *notify, 1);

	if (reloadFlags == 0x0000)
		strcpy(flagString, "LOAD_FLAGS_NONE");
	else if (reloadFlags == 0x0010)
		strcpy(flagString, "LOAD_FLAGS_IS_REFRESH");
	else if (reloadFlags == 0x0020)
		strcpy(flagString, "LOAD_FLAGS_IS_LINK");
	else if (reloadFlags == 0x0040)
		strcpy(flagString, "LOAD_FLAGS_BYPASS_HISTORY");
	else if (reloadFlags == 0x0080)
		strcpy(flagString, "LOAD_FLAGS_REPLACE_HISTORY");
	else if (reloadFlags == 0x0100)
		strcpy(flagString, "LOAD_FLAGS_BYPASS_CACHE");
	else if (reloadFlags == 0x0200)
		strcpy(flagString, "LOAD_FLAGS_BYPASS_PROXY");
	else if (reloadFlags == 0x0400)
		strcpy(flagString, "LOAD_FLAGS_CHARSET_CHANGE");
	else if (reloadFlags == (0x0100 | 0x0200))
		strcpy(flagString, "LOAD_FLAGS_BYPASS_CACHE | LOAD_FLAGS_BYPASS_PROXY");

	FormatAndPrintOutput("OnHistoryReload() flag value = ", reloadFlags, 1);
	FormatAndPrintOutput("OnHistoryReload() flag string = ", flagString, 2);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryGotoIndex(PRInt32 theIndex, nsIURI *theURI, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryGotoIndex()", 2);

    GetTheURI(theURI, 1);
	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryGotoIndex() notification = ", *notify, 1);

	FormatAndPrintOutput("OnHistoryGotoIndex() index = ", theIndex, 2);

	return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::OnHistoryPurge(PRInt32 theNumEntries, PRBool *notify)
{
	QAOutput("nsIHistoryListener::OnHistoryPurge()", 2);

	*notify = PR_TRUE;
	FormatAndPrintOutput("OnHistoryPurge() notification = ", *notify, 1);

	FormatAndPrintOutput("OnHistoryPurge() theNumEntries = ", theNumEntries, 2);

	return NS_OK;
}

