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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
