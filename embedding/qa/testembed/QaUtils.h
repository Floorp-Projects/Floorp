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
 */

// QAUtils.h : utilities for CQAUtils class. Includes writing to output log.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _QAUTILS_H
#define _QAUTILS_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "BrowserView.h"
#include "stdafx.h"

/////////////////////////////////////////////////////////////////////////////
// CQaUtils window

class CQaUtils : public CWnd
{
public:
	CQaUtils();
    virtual ~CQaUtils();

	void static RvTestResult(nsresult, const char *, int displayMethod=1);
	void static WriteToOutputFile(const char *);
	void static QAOutput(const char *pLine, int displayMethod=1);
	void static FormatAndPrintOutput(const char *, const char *, int);
	void static FormatAndPrintOutput(const char *, int, int);
	void static RequestName(nsIRequest *, nsCString &, int displayMethod=1);
	void static WebProgDOMWindowTest(nsIWebProgress *, const char *, 
									 int displayMethod=1);
	void static GetTheUri(nsIURI *, int displayMethod=1);

	nsresult rv;

	// Some helper methods

	// Generated message map functions
protected:
	//{{AFX_MSG(CQaUtils)
	DECLARE_MESSAGE_MAP()
};

#endif //_QAUTILS_H
