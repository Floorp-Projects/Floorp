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
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
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

// File Overview....
// 
// Header file for nsiRequest interface test cases

#include "Tests.h"

/////////////////////////////////////////////////////////////////////////////
// CNsIRequest window

class CNsIRequest //: public CTests
{
// Construction
public:
/*	CNsIRequest(nsIWebBrowser* mWebBrowser,
			   nsIBaseWindow* mBaseWindow,
			   nsIWebNavigation* mWebNav,
			   CBrowserImpl *mpBrowserImpl);
*/	

	CNsIRequest(nsIWebBrowser* mWebBrowser,CBrowserImpl *mpBrowserImpl);

// Attributes
public:
	// Mozilla interfaces
	//
	nsCOMPtr<nsIWebBrowser> qaWebBrowser;
	CBrowserImpl *qaBrowserImpl;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNsIRequest)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNsIRequest();

public:
	void OnInterfacesNsirequest();
	// individual nsIRequest tests
	void static IsPendingReqTest(nsIRequest *);
	void static GetStatusReqTest(nsIRequest *);
	void static SuspendReqTest(nsIRequest *);
	void static ResumeReqTest(nsIRequest *);
	void static CancelReqTest(nsIRequest *);
	void static SetLoadGroupTest(nsIRequest *, nsILoadGroup *);
	void static GetLoadGroupTest(nsIRequest *);

	// Generated message map functions
protected:
/*	//{{AFX_MSG(CNsIRequest)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()*/
};


typedef struct
{
	char		theUrl[1024];
	bool		reqPend;
	bool		reqStatus;
	bool		reqSuspend;
	bool		reqResume;
	bool		reqCancel;
	bool		reqSetLoadGroup;
	bool		reqGetLoadGroup;	
} Element;

/////////////////////////////////////////////////////////////////////////////
