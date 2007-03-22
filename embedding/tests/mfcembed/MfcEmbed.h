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

// mozembed.h : main header file for the MOZEMBED application
//

#ifndef _MFCEMBED_H
#define _MFCEMBED_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
    #error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CMfcEmbedApp:
// See mozembed.cpp for the implementation of this class
//

class CBrowserFrame;
class CProfileMgr;
class nsProfileDirServiceProvider;

class CMfcEmbedApp : public CWinApp,
                     public nsIObserver,
                     public nsIWindowCreator,
                     public nsSupportsWeakReference
{
public:
    CMfcEmbedApp();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIWINDOWCREATOR

    CBrowserFrame* CreateNewBrowserFrame(PRUint32 chromeMask = nsIWebBrowserChrome::CHROME_ALL, 
                            PRInt32 x = -1, PRInt32 y = -1, 
                            PRInt32 cx = -1, PRInt32 cy = -1, PRBool bShowWindow = PR_TRUE,
                            PRBool bIsEditor=PR_FALSE);
    void RemoveFrameFromList(CBrowserFrame* pFrm, BOOL bCloseAppOnLastFrame = TRUE);

    void ShowDebugConsole();
    nsresult OverrideComponents();

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CMfcEmbedApp)
    public:
    virtual BOOL InitInstance();
    virtual int ExitInstance();
    virtual BOOL OnIdle(LONG lCount);
    //}}AFX_VIRTUAL

    CObList m_FrameWndLst;

    BOOL m_bChrome;
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
    //{{AFX_MSG(CMfcEmbedApp)
    afx_msg void OnAppAbout();
    afx_msg void OnNewBrowser();
    afx_msg void OnNewEditor();
    afx_msg void OnManageProfiles();
    afx_msg void OnEditPreferences();
        // NOTE - the ClassWizard will add and remove member functions here.
        //    DO NOT EDIT what you see in these blocks of generated code !
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()

private:
    BOOL            InitializeProfiles();
    BOOL            CreateHiddenWindow();
    nsresult        InitializePrefs();
    nsresult        InitializeWindowCreator();
    
private:

#ifdef USE_PROFILES
    CProfileMgr     *m_ProfileMgr;
#else
    nsProfileDirServiceProvider *m_ProfileDirServiceProvider;
#endif
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // _MFCEMBED_H
