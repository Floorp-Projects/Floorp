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
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined(AFX_PROFILESDLG_H__48358887_EBFA_11D4_9905_00B0D0235410__INCLUDED_)
#define AFX_PROFILESDLG_H__48358887_EBFA_11D4_9905_00B0D0235410__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProfilesDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CNewProfileDlg dialog

class CNewProfileDlg : public CDialog
{
// Construction
public:
    CNewProfileDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(CNewProfileDlg)
    enum { IDD = IDD_PROFILE_NEW };
    int        m_LocaleIndex;
    CString    m_Name;
    //}}AFX_DATA


// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CNewProfileDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:

    // Generated message map functions
    //{{AFX_MSG(CNewProfileDlg)
        // NOTE: the ClassWizard will add member functions here
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CRenameProfileDlg dialog

class CRenameProfileDlg : public CDialog
{
// Construction
public:
    CRenameProfileDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(CRenameProfileDlg)
    enum { IDD = IDD_PROFILE_RENAME };
    CString    m_NewName;
    //}}AFX_DATA

    CString     m_CurrentName;

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CRenameProfileDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:

    // Generated message map functions
    //{{AFX_MSG(CRenameProfileDlg)
        // NOTE: the ClassWizard will add member functions here
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CProfilesDlg dialog

class CProfilesDlg : public CDialog
{
// Construction
public:
    CProfilesDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(CProfilesDlg)
    enum { IDD = IDD_PROFILES };
    CListBox    m_ProfileList;
    BOOL        m_bAtStartUp;
    BOOL        m_bAskAtStartUp;
    //}}AFX_DATA

    nsAutoString m_SelectedProfile;

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CProfilesDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:

    // Generated message map functions
    //{{AFX_MSG(CProfilesDlg)
    virtual BOOL OnInitDialog();
    afx_msg void OnNewProfile();
    afx_msg void OnRenameProfile();
    afx_msg void OnDeleteProfile();
    afx_msg void OnDblclkProfile();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROFILESDLG_H__48358887_EBFA_11D4_9905_00B0D0235410__INCLUDED_)
