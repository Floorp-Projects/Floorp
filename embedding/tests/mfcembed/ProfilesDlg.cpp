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

// ProfilesDlg.cpp : implementation file
//

#include "stdafx.h"
#include <afxpriv.h>
#include "mfcembed.h"
#include "ProfilesDlg.h"

// Mozilla
#include "nsIProfile.h"
#include "nsIServiceManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Static Routines
static void ValidateProfileName(const CString& profileName, CDataExchange* pDX)
{
    USES_CONVERSION;

    nsresult rv;
    PRBool exists = FALSE;

    {
        nsCOMPtr<nsIProfile> profileService = 
                 do_GetService(NS_PROFILE_CONTRACTID, &rv);
        rv = profileService->ProfileExists(T2CW(profileName), &exists);
    }

    if (NS_SUCCEEDED(rv) && exists)
    {
        CString errMsg;
        errMsg.Format(_T("Error: A profile named \"%s\" already exists."), profileName);
        AfxMessageBox( errMsg, MB_ICONEXCLAMATION );
        errMsg.Empty();
        pDX->Fail();
    }

    if (profileName.FindOneOf(_T("\\/")) != -1)
    {
        AfxMessageBox( _T("Error: A profile name cannot contain the characters \"\\\" or \"/\"."), MB_ICONEXCLAMATION );
        pDX->Fail();
    }
}

/////////////////////////////////////////////////////////////////////////////
// CNewProfileDlg dialog


CNewProfileDlg::CNewProfileDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CNewProfileDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CNewProfileDlg)
    m_LocaleIndex = -1;
    m_Name = _T("");
    //}}AFX_DATA_INIT
}


void CNewProfileDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CNewProfileDlg)
    DDX_CBIndex(pDX, IDC_LOCALE_COMBO, m_LocaleIndex);
    DDX_Text(pDX, IDC_NEW_PROF_NAME, m_Name);
    //}}AFX_DATA_MAP

    pDX->PrepareEditCtrl(IDC_NEW_PROF_NAME);
    if (pDX->m_bSaveAndValidate)
    {
        ValidateProfileName(m_Name, pDX);
    }
}


BEGIN_MESSAGE_MAP(CNewProfileDlg, CDialog)
    //{{AFX_MSG_MAP(CNewProfileDlg)
        // NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewProfileDlg message handlers


/////////////////////////////////////////////////////////////////////////////
// CRenameProfileDlg dialog


CRenameProfileDlg::CRenameProfileDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CRenameProfileDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CRenameProfileDlg)
    m_NewName = _T("");
    //}}AFX_DATA_INIT
}


void CRenameProfileDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CRenameProfileDlg)
    DDX_Text(pDX, IDC_NEW_NAME, m_NewName);
    //}}AFX_DATA_MAP

    pDX->PrepareEditCtrl(IDC_NEW_NAME);
    if (pDX->m_bSaveAndValidate)
    {
        ValidateProfileName(m_NewName, pDX);
    }
}


BEGIN_MESSAGE_MAP(CRenameProfileDlg, CDialog)
    //{{AFX_MSG_MAP(CRenameProfileDlg)
        // NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRenameProfileDlg message handlers


/////////////////////////////////////////////////////////////////////////////
// CProfilesDlg dialog


CProfilesDlg::CProfilesDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CProfilesDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CProfilesDlg)
    m_bAtStartUp = FALSE;
    m_bAskAtStartUp = FALSE;
    //}}AFX_DATA_INIT
}


void CProfilesDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CProfilesDlg)
    DDX_Control(pDX, IDC_LIST1, m_ProfileList);
    DDX_Check(pDX, IDC_CHECK_ASK_AT_START, m_bAskAtStartUp);
    //}}AFX_DATA_MAP

    if (pDX->m_bSaveAndValidate)
    {
        USES_CONVERSION;

        int itemIndex = m_ProfileList.GetCurSel();
        if (itemIndex != LB_ERR)
        {
            CString itemText;
            m_ProfileList.GetText(itemIndex, itemText);
            m_SelectedProfile.Assign(T2CW(itemText));
        }
    }
}


BEGIN_MESSAGE_MAP(CProfilesDlg, CDialog)
    //{{AFX_MSG_MAP(CProfilesDlg)
    ON_BN_CLICKED(IDC_PROF_NEW, OnNewProfile)
    ON_BN_CLICKED(IDC_PROF_RENAME, OnRenameProfile)
    ON_BN_CLICKED(IDC_PROF_DELETE, OnDeleteProfile)
    ON_LBN_DBLCLK(IDC_LIST1, OnDblclkProfile)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProfilesDlg message handlers

BOOL CProfilesDlg::OnInitDialog() 
{
    USES_CONVERSION;

    CDialog::OnInitDialog();
    
    nsCAutoString   cStr;        
    nsXPIDLString   curProfileName;

    // Fill the list of profiles
    nsresult rv;
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    profileService->GetCurrentProfile(getter_Copies(curProfileName));

    PRInt32     selectedRow = 0;
    PRUint32    listLen;
    PRUnichar   **profileList;
    rv = profileService->GetProfileList(&listLen, &profileList);

    for (PRUint32 index = 0; index < listLen; index++)
    {
        CString tmpStr(W2T(profileList[index]));
        m_ProfileList.AddString(tmpStr);
        if (wcscmp(profileList[index], curProfileName.get()) == 0)
            selectedRow = index;
    }

    m_ProfileList.SetCurSel(selectedRow);

    if (m_bAtStartUp)
    {
        GetDlgItem(IDCANCEL)->EnableWindow(FALSE);
    }
    
    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}

void CProfilesDlg::OnNewProfile() 
{
    CNewProfileDlg dialog;

    if (dialog.DoModal() == IDOK)
    {
        nsresult rv;

        nsCOMPtr<nsIProfile> profileService = 
                 do_GetService(NS_PROFILE_CONTRACTID, &rv);
        ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv))
        {
            USES_CONVERSION;

            rv = profileService->CreateNewProfile(T2CW(dialog.m_Name), nsnull, nsnull, PR_FALSE);
            ASSERT(NS_SUCCEEDED(rv));
            if (NS_SUCCEEDED(rv))
            {
                int item = m_ProfileList.AddString(dialog.m_Name);
                m_ProfileList.SetCurSel(item);
                GetDlgItem(IDOK)->EnableWindow(TRUE);
            }
        }
    }
}

void CProfilesDlg::OnRenameProfile() 
{
    CRenameProfileDlg dialog;

    int itemIndex = m_ProfileList.GetCurSel();
    ASSERT(itemIndex != LB_ERR);
    if (itemIndex == LB_ERR)
        return;

    m_ProfileList.GetText(itemIndex, dialog.m_CurrentName);

    if (dialog.DoModal() == IDOK)
    {
        USES_CONVERSION;

        nsresult rv;

        nsCOMPtr<nsIProfile> profileService = 
                 do_GetService(NS_PROFILE_CONTRACTID, &rv);
        ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv))
        {
            rv = profileService->RenameProfile(T2CW(dialog.m_CurrentName), T2CW(dialog.m_NewName));
            ASSERT(NS_SUCCEEDED(rv));
        }
    }    
}

void CProfilesDlg::OnDeleteProfile() 
{
    int itemIndex = m_ProfileList.GetCurSel();
    ASSERT(itemIndex != LB_ERR);
    if (itemIndex == LB_ERR)
        return;

    CString selectedProfile;
    m_ProfileList.GetText(itemIndex, selectedProfile);
    
    nsresult rv;
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv))
    {
        USES_CONVERSION;

        rv = profileService->DeleteProfile(T2CW(selectedProfile), PR_TRUE);
        ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv))
        {
            int itemCount = m_ProfileList.DeleteString(itemIndex);
            if (itemCount == 0)
                GetDlgItem(IDOK)->EnableWindow(FALSE);
        }
    }    
}

void CProfilesDlg::OnDblclkProfile() 
{
    OnOK();
}
