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

// nsIChannelTests.h : header file for nsIChannel test cases

#include "Tests.h"

#if !defined(AFX_NSICHANNELTESTS_H__33C5EBD3_0178_11D7_9C13_00C04FA02BE6__INCLUDED_)
#define AFX_NSICHANNELTESTS_H__33C5EBD3_0178_11D7_9C13_00C04FA02BE6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// nsIChannelTests.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// nsIChannelTests window

class CnsIChannelTests
{
// Construction
public:
	CnsIChannelTests(nsIWebBrowser* mWebBrowser, CBrowserImpl *mpBrowserImpl);

// Attributes
public:

// Operations
public:

// Implementation
	nsIChannel * GetChannelObject(nsCAutoString);
	nsIURI * GetURIObject(nsCAutoString);
	void SetOriginalURITest(nsIChannel *, nsCAutoString, PRInt16);
	void GetOriginalURITest(nsIChannel *, PRInt16);
	void GetURITest(nsIChannel *, PRInt16);
	void SetOwnerTest(nsIChannel *, PRInt16);
	void GetOwnerTest(nsIChannel *, PRInt16);
	void SetNotificationsTest(nsIChannel *, PRInt16);
	void GetNotificationsTest(nsIChannel *, PRInt16);
	void GetSecurityInfoTest(nsIChannel *, PRInt16);
	void SetContentTypeTest(nsIChannel *, PRInt16);
	void GetContentTypeTest(nsIChannel *, PRInt16);
	void SetContentCharsetTest(nsIChannel *, PRInt16);
	void GetContentCharsetTest(nsIChannel *, PRInt16);
	void SetContentLengthTest(nsIChannel *, PRInt16);
	void GetContentLengthTest(nsIChannel *, PRInt16);
	void OpenTest(nsIChannel *, PRInt16);
	void AsyncOpenTest(nsIChannel *, PRInt16);
	void PostAsyncTests(nsIChannel *, PRInt16);
	void OnStartTests(UINT nMenuID);
	void RunAllTests();
public:
	virtual ~CnsIChannelTests();

	// Generated message map functions
protected:

private:
	CBrowserImpl *qaBrowserImpl;
	nsCOMPtr<nsIWebBrowser> qaWebBrowser;
	nsCOMPtr<nsIChannel> theChannel;
	nsCOMPtr<nsIURI> theURI;
	nsCOMPtr<nsISupports> theSupports;
	nsCOMPtr<nsIInterfaceRequestor> theIRequestor;
	nsCOMPtr<nsIInputStream> theInputStream;
	nsCAutoString theSpec;
};

typedef struct
{
	char		theURL[1024];
	char		contentType[1024];	
} ChannelRow;

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NSICHANNELTESTS_H__33C5EBD3_0178_11D7_9C13_00C04FA02BE6__INCLUDED_)
