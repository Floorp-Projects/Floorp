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

// mozembed.h : main header file for the MOZEMBED application
//

#ifndef _TESTEMBED_H
#define _TESTEMBED_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CTestEmbedApp:
// See mozembed.cpp for the implementation of this class
//

class CBrowserFrame;
class CProfileMgr;

class CTestEmbedApp : public CWinApp,
                     public nsIObserver,
                     public nsIWindowCreator,
                     public nsSupportsWeakReference
{
public:
	CTestEmbedApp();
	
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIWINDOWCREATOR

	CBrowserFrame* CreateNewBrowserFrame(PRUint32 chromeMask = nsIWebBrowserChrome::CHROME_ALL, 
							PRInt32 x = -1, PRInt32 y = -1, 
							PRInt32 cx = -1, PRInt32 cy = -1,
							PRBool bShowWindow = PR_TRUE);
	void RemoveFrameFromList(CBrowserFrame* pFrm, BOOL bCloseAppOnLastFrame = TRUE);

    void ShowDebugConsole();
    BOOL IsCmdLineSwitch(const char *pSwitch, BOOL bRemove = TRUE);
    void ParseCmdLine();
    nsresult OverrideComponents();

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTestEmbedApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual BOOL OnIdle(LONG lCount);
	//}}AFX_VIRTUAL

	CObList m_FrameWndLst;

    CString m_strHomePage;
    inline BOOL GetHomePage(CString& strHomePage) {
        strHomePage = m_strHomePage;
        return TRUE;
    }

    int m_iStartupPage; //0 = BlankPage, 1 = HomePage
    inline int GetStartupPageMode() {
        return m_iStartupPage;
    }

// Implementation

public:
	//{{AFX_MSG(CTestEmbedApp)
	afx_msg void OnAppAbout();
	afx_msg void OnNewBrowser();
    afx_msg void OnManageProfiles();
    afx_msg void OnEditPreferences();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	BOOL			InitializeProfiles();
	BOOL			CreateHiddenWindow();
    nsresult        InitializePrefs();
    nsresult        InitializeWindowCreator();
    
private:
    CProfileMgr     *m_ProfileMgr;
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // _TESTEMBED_H
