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

// nsIWebBrow.h : header file for nsIWebBrowser test cases
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _NSIWEBBROW_H
#define _NSIWEBBROW_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "BrowserView.h"
#include "BrowserImpl.h"
#include "StdAfx.h"


/////////////////////////////////////////////////////////////////////////////
// CTESTS class

class CBrowserImpl;

class CNsIWebBrowser
{
public:
	CNsIWebBrowser(nsIWebBrowser *mWebBrowser,
				   CBrowserImpl *mpBrowserImpl);
	virtual ~CNsIWebBrowser();

	// Mozilla interfaces
	//
	nsCOMPtr<nsIWebBrowser> qaWebBrowser;
	CBrowserImpl	*qaBrowserImpl;

	// local test methods for nsIWebBrowser
	void WBAddListener(PRInt16);
	void WBRemoveListener(PRInt16);
	void WBGetContainerWindow(PRInt16);
	void WBSetContainerWindow(PRInt16);
	void WBGetURIContentListener(PRInt16);
	void WBSetURIContentListener(PRInt16);
	void WBGetDOMWindow(PRInt16);
	void OnStartTests(UINT nMenuID);
	void RunAllTests();

	// local test methods for nsIWebBrowserSetup
	void WBSSetupProperty(PRInt16);

	// local test variables

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTests)
	public:
	//}}AFX_VIRTUAL

private:
	// Individual interface Objects
	nsCOMPtr<nsIWebBrowserChrome> qaWebBrowserChrome;
	nsCOMPtr<nsIURIContentListener> qaURIContentListener;
	// Generated message map functions
protected:
	//{{AFX_MSG(CTests)

	//}}AFX_MSG
};

#endif //_NSIWEBBROW_H
