// ProfilesDlg.cpp : implementation file
//

#include "stdafx.h"
#include <afxpriv.h>
#include "Testembed.h"
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
        //NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
	    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID));
        rv = profileService->ProfileExists(T2W(profileName), &exists);
    }

    if (NS_SUCCEEDED(rv) && exists)
    {
        CString errMsg;

        errMsg.Format(_T("Error: A profile named \"%s\" already exists."), (const char *)profileName);
        AfxMessageBox( errMsg, MB_ICONEXCLAMATION );
        errMsg.Empty();
        pDX->Fail();
    }

    if (profileName.FindOneOf("\\/") != -1)
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
            m_SelectedProfile.Assign(T2W(itemText));
        }
    }
}


BEGIN_MESSAGE_MAP(CProfilesDlg, CDialog)
	//{{AFX_MSG_MAP(CProfilesDlg)
	ON_BN_CLICKED(IDC_PROF_NEW, OnNewProfile)
	ON_BN_CLICKED(IDC_PROF_RENAME, OnRenameProfile)
	ON_BN_CLICKED(IDC_PROF_DELETE, OnDeleteProfile)
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
    //NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID));
    profileService->GetCurrentProfile(getter_Copies(curProfileName));

    PRInt32     selectedRow = 0;
    PRUint32    listLen;
    PRUnichar   **profileList;
    rv = profileService->GetProfileList(&listLen, &profileList);

    for (PRUint32 index = 0; index < listLen; index++)
    {
        CString tmpStr(W2T(profileList[index]));
        m_ProfileList.AddString(tmpStr);
        if (nsCRT::strcmp(profileList[index], curProfileName.get()) == 0)
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

        //NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
	    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID,&rv));
        ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv))
        {
            USES_CONVERSION;

   		    rv = profileService->CreateNewProfile(T2W(dialog.m_Name), nsnull, nsnull, PR_FALSE);
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

        //NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
	    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID,&rv));
        ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv))
        {
            rv = profileService->RenameProfile(T2W(dialog.m_CurrentName), T2W(dialog.m_NewName));
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
    //NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID,&rv));

    ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv))
    {
        USES_CONVERSION;

        rv = profileService->DeleteProfile(T2W(selectedProfile), PR_TRUE);
        ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv))
        {
            int itemCount = m_ProfileList.DeleteString(itemIndex);
            if (itemCount == 0)
                GetDlgItem(IDOK)->EnableWindow(FALSE);
        }
    }	
}
