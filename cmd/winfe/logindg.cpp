/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// LoginDlg.cpp : implementation file
//

#include "stdafx.h"
#include "LoginDg.h" 
#include "prefapi.h"
#include "hk_funcs.h"
#include "setupwiz.h"
#include "mucwiz.h"
#include "mucproc.h"
#include "profile.h"
#include "dialog.h"
#include "helper.h"
#ifdef MOZ_LOC_INDEP
#include "winli.h"
#endif /* MOZ_LOC_INDEP */


#ifdef XP_WIN16
#include "winfile.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int login_SetUserKeyData(const char *profileName);

/////////////////////////////////////////////////////////////////////
// CLoginList

class CLoginList: public CListBox {
private:
	int m_iPosIndex, m_iPosName, m_iPosStatus;

protected:
	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

	afx_msg void OnLButtonDown( UINT nFlags, CPoint point ); 
	afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct );

	DECLARE_MESSAGE_MAP()
 
public:
	CLoginList();
	~CLoginList();

	void SetColumnPositions(int, int, int);
};
/////////////////////////////////////////////////////////////////////////////
// CLoginDlg dialog

class CLoginDlg : public CDialog
{
// Construction
public:
	CLoginDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CLoginDlg)
	enum { IDD = IDD_LOGIN_DIALOG };
	CLoginList	m_LoginList;
	CString	m_ProfileSelected;
	CStringList m_Profiles;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLoginDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLoginDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkLoginList();
	afx_msg void OnSelChangeLoginList();
	afx_msg void OnNewProfile();
	afx_msg void OnDeleteProfile();
	afx_msg void OnOption();
	afx_msg void OnAdvance();
	afx_msg void OnCheck();
	afx_msg void OnEdit();
	afx_msg void OnRemote();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void CheckPEConfig();
	void DialogDisplay(BOOL);

private:
	// PE
	RECT	rect;		
	int		m_range;	
	BOOL	m_bRestoreWindow;
	BOOL	m_bDialOnDemand;
	CString	m_strAcctName;
	CString m_strModemName;
	CString m_strProfileName;
};

#define PREF_CHAR 0
#define PREF_INT 1
#define PREF_BOOL 2

struct update_PrefTable { char *xp_name; char *section; char *name; int type;};

/////////////////////////////////////////////////////////////////////////////
// CUpdateFileDlg dialog
//
class CUpdateFileDlg : public CDialog
{
// Construction
public:
	CUpdateFileDlg(CWnd *pParent, BOOL bCopyDontMove);

    // Called at the start of each image saved
    void StartFileUpdate(char *category,char * pFilename);

	BOOL m_bCopyDontMove;
// Dialog Data
	//{{AFX_DATA(CSaveFileDlg)
	enum { IDD = IDD_UPDATE_STATUS };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA
    
private:
    CWnd      *m_pParent;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSaveFileDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:
	
    // Generated message map functions
	//{{AFX_MSG(CSaveFileDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CLoginDlg dialog


CLoginDlg::CLoginDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CLoginDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLoginDlg)
	m_ProfileSelected = _T("");
	//}}AFX_DATA_INIT

	if(theApp.m_bPEEnabled)
	{
		m_bRestoreWindow = FALSE;
		m_bDialOnDemand = TRUE;
		m_strAcctName = "";
		m_strModemName = "";
	}
}


void CLoginDlg::DoDataExchange(CDataExchange* pDX) 
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLoginDlg)
	//DDX_Control(pDX, IDC_LOGIN_LIST, m_LoginList);
	DDX_LBString(pDX, IDC_LOGIN_LIST, m_ProfileSelected);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLoginDlg, CDialog)
	//{{AFX_MSG_MAP(CLoginDlg)
	ON_LBN_DBLCLK(IDC_LOGIN_LIST, OnDblclkLoginList)
	ON_LBN_SELCHANGE(IDC_LOGIN_LIST, OnSelChangeLoginList)
	ON_COMMAND(IDC_CREATENEWPROFILE, OnNewProfile)
	ON_COMMAND(IDC_DELETEPROFILE,OnDeleteProfile)
	ON_COMMAND(IDC_OPTION,OnOption)
	ON_COMMAND(IDC_ADVANCED,OnAdvance)
	ON_COMMAND(IDC_PROFILE1,OnRemote)
	ON_COMMAND(IDC_CHECK,OnCheck)
	ON_COMMAND(IDC_EDITPROFILE,OnEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLoginDlg message handlers

void CLoginDlg::OnEdit() 
{
	UpdateData();
    char szMsg[_MAX_PATH+128];
    sprintf(szMsg,szLoadString(IDS_PROFILE_RENAME), m_ProfileSelected);

	CDialogPRMT dlgPrompt(this);
	
	char * newName = dlgPrompt.DoModal(szLoadString(IDS_PROFILE_RENAME2), m_ProfileSelected,szMsg);

	if (newName) {
		login_RenameUserKey(m_ProfileSelected,newName);
		m_LoginList.DeleteString(m_LoginList.FindStringExact(-1,m_ProfileSelected));
		int idx = m_LoginList.AddString(newName);
		if (idx != LB_ERR) {
			m_LoginList.SetItemDataPtr(idx,XP_STRDUP(newName));
			m_LoginList.UpdateWindow();
			m_LoginList.SetCurSel(m_LoginList.FindStringExact(-1,newName));
		}
	}
}

void CLoginDlg::OnOK() 
{
	// PE; disable dial-on-demand for all the profiles
	if(theApp.m_bPEEnabled && !theApp.m_bProfileManager)
	{
		CMucProc	m_mucProc;
		CString		temp;

		if(!m_bDialOnDemand)
			temp = "";
		else
			temp = m_strAcctName;
		m_mucProc.SetDialOnDemand(temp,m_bDialOnDemand);
	}
	CDialog::OnOK();
}

void CLoginDlg::OnNewProfile() 
{
	char * profile = login_QueryNewProfile(FALSE,this);
	if (profile) {
		int idx = m_LoginList.AddString(profile);
		if (idx != LB_ERR) {
			m_LoginList.SetItemDataPtr(idx,XP_STRDUP(profile));
			m_LoginList.UpdateWindow();
			m_LoginList.SetCurSel(m_LoginList.FindStringExact(-1,profile));
		}
		XP_FREE(profile);
		GetDlgItem(IDOK)->EnableWindow(TRUE);	
		GetDlgItem(IDC_DELETEPROFILE)->EnableWindow(TRUE);	
		GetDlgItem(IDC_EDITPROFILE)->EnableWindow(TRUE);
		GetDlgItem(IDC_OPTION)->EnableWindow(TRUE);
		GetDlgItem(IDC_ADVANCED)->EnableWindow(TRUE);
		OnOK();
	}
}

void CLoginDlg::OnDeleteProfile()
{
	UpdateData();
    char szMsg[256];
    sprintf(szMsg, szLoadString(IDS_CONFIRM_DELETE), m_ProfileSelected);

    if (AfxMessageBox(szMsg,MB_YESNO) == IDYES) {
		login_DeleteUserKey(m_ProfileSelected);
		m_LoginList.DeleteString(m_LoginList.FindStringExact(-1,m_ProfileSelected));
		m_LoginList.SetCurSel(0);
		AfxMessageBox(szLoadString(IDS_PROFILE_DELETED), MB_OK);
	}

	if (m_LoginList.GetCount() <= 0) {
		GetDlgItem(IDOK)->EnableWindow(FALSE);
		GetDlgItem(IDC_DELETEPROFILE)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDITPROFILE)->EnableWindow(FALSE);
		GetDlgItem(IDC_OPTION)->EnableWindow(FALSE);
		GetDlgItem(IDC_ADVANCED)->EnableWindow(FALSE);
	}

}

BOOL CLoginDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// PE: profile selector/manager
	if(theApp.m_bPEEnabled)
	{
		if(!theApp.m_bProfileManager)  // selector    
		{
			GetDlgItem(IDC_OPTION)->ShowWindow(SW_HIDE);
			GetDlgItem(IDC_ADVANCED)->ShowWindow(SW_SHOW);
			((CButton*)GetDlgItem(IDC_CHECK))->SetCheck(!m_bDialOnDemand);
		}
		else
			GetDlgItem(IDC_ADVANCED)->ShowWindow(SW_HIDE);
	}
	else   // none PE
	{
		GetDlgItem(IDC_ADVANCED)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_OPTION)->ShowWindow(SW_HIDE);  
		/* we used to hide some of the buttons when the profile manager was used as a
		   selector, now with li, we don't do that anymore
		*/
	}

	// dialog display -- we always show the buttons now, so pass in false.
	DialogDisplay(FALSE);

	// profile list
	m_LoginList.SubclassDlgItem(IDC_LOGIN_LIST,this);
	
	int idx=0;
	while (!m_Profiles.IsEmpty()) {
		char * name = strdup(m_Profiles.RemoveHead());
		m_LoginList.AddString(name);    
		//m_LoginList.SetItemData(idx++,(ULONG)(const char *)m_Profiles.RemoveHead());
		m_LoginList.SetItemDataPtr(idx++,(void *)name);
	}
	m_LoginList.SetCurSel(0);
	
	char * name = login_GetCurrentUser();
	if (name) {
		m_LoginList.SelectString(-1,name);      
		free(name);
	}
	if (idx == 0) {
		// we have no profiles -- disable OK/etc
		GetDlgItem(IDOK)->EnableWindow(FALSE);	
		GetDlgItem(IDC_DELETEPROFILE)->EnableWindow(FALSE);	
		GetDlgItem(IDC_EDITPROFILE)->EnableWindow(FALSE);	
		GetDlgItem(IDC_OPTION)->EnableWindow(FALSE);
		GetDlgItem(IDC_ADVANCED)->EnableWindow(FALSE);
	}
	m_LoginList.SetFocus();

	if(theApp.m_bPEEnabled)
		CheckPEConfig();

	return FALSE;  // return TRUE unless you set the focus to a control
		      // EXCEPTION: OCX Property Pages should return FALSE
}

void CLoginDlg::DialogDisplay(BOOL m_bSelector)
{
	// display window
	RECT	tagRectTop, tagRectBottom;
	RECT	helpTextRect, frameClientRect, listboxRect;
	RECT	frameScreenRect, helpTextScreenRect;

	GetWindowRect(&rect);
	GetDlgItem(IDC_TOP_TAG)->GetWindowRect(&tagRectTop);
	GetDlgItem(IDC_BOTTOM_TAG)->GetWindowRect(&tagRectBottom);
	m_range = tagRectBottom.bottom - tagRectTop.bottom;

	SetWindowPos(&wndTop, rect.left, rect.top, 
				rect.right-rect.left, rect.bottom-rect.top-m_range, SWP_SHOWWINDOW);

	// display list box
	if(! m_bSelector)
		return;

	GetDlgItem(IDC_FRAME_BOX)->GetClientRect(&frameClientRect);
	GetDlgItem(IDC_LOGIN_LIST)->GetClientRect(&listboxRect);
	GetDlgItem(IDC_HELP_TEXT)->GetClientRect(&helpTextRect);
	GetDlgItem(IDC_FRAME_BOX)->GetWindowRect(&frameScreenRect);
	GetDlgItem(IDC_HELP_TEXT)->GetWindowRect(&helpTextScreenRect);
	
	int m_listBoxWidth = listboxRect.right - listboxRect.left;
	listboxRect.left = frameScreenRect.left + 
					((frameClientRect.right - frameClientRect.left) - m_listBoxWidth)/2;
	listboxRect.right = listboxRect.left + m_listBoxWidth;

	int m_listBoxHeight = listboxRect.bottom - listboxRect.top;
	listboxRect.top = helpTextScreenRect.bottom + 
					((frameScreenRect.bottom - helpTextScreenRect.bottom) - m_listBoxHeight)/2;
	listboxRect.bottom = listboxRect.top + m_listBoxHeight;

	ScreenToClient(&listboxRect);

	GetDlgItem(IDC_LOGIN_LIST)->SetWindowPos(&wndTop, 
				listboxRect.left, listboxRect.top,
				m_listBoxWidth, m_listBoxHeight,
				SWP_SHOWWINDOW);

	// change the dialog title
	CString		m_str;
	m_str.LoadString(IDS_PROFILE_SELECTOR_TITLE);
	SetWindowText(m_str);
}

void CLoginDlg::OnDblclkLoginList() 
{
	// TODO: Add your control notification handler code here
	OnOK(); 
}

void CLoginDlg::OnSelChangeLoginList() 
{
	if(theApp.m_bPEEnabled && !theApp.m_bProfileManager)
	{
		CheckPEConfig();
		GetDlgItem(IDC_ADVANCED_ACCTNAME)->SetWindowText(m_strAcctName);
		UpdateData(FALSE);
	}
}

void CLoginDlg::OnOption() 
{
	CheckPEConfig();
	CMucViewWizard mucViewWizard(this, m_strProfileName, m_strAcctName, m_strModemName);
	int ret = mucViewWizard.DoModal();
}

/* handles the remote profile case */
void CLoginDlg::OnRemote() 
{
	/* if they hit ok - go for it
	   if they don't, let them choose a regular one.
	*/
#ifdef MOZ_LOC_INDEP
	if (winli_QueryNetworkProfile()) {
		 /* act like they hit ok to close the profile manager.*/
		OnOK();
		/* set profile selected to null to stop us from getting the profile from 
		   the list. Must be done after the ok/dataExchange stuff.
		*/
		m_ProfileSelected = "";
	}
#endif // MOZ_LOC_INDEP
}

void CLoginDlg::OnAdvance() 
{
	m_bRestoreWindow = (m_bRestoreWindow + 1) %2; 

	if(m_bRestoreWindow)
	{
		GetWindowRect(&rect);
		SetWindowPos(&wndTop, rect.left, rect.top, 
					rect.right-rect.left, rect.bottom-rect.top+m_range, SWP_SHOWWINDOW);

		SetDlgItemText(IDC_ADVANCED, "Advanced<<");
	
		GetDlgItem(IDC_ADVANCED_ACCTNAME)->SetWindowText(m_strAcctName);
		UpdateData(FALSE);
	}
	else
	{
		GetWindowRect(&rect);
		SetWindowPos(&wndTop, rect.left, rect.top, 
					rect.right-rect.left, rect.bottom-rect.top-m_range, SWP_SHOWWINDOW);
				
		SetDlgItemText(IDC_ADVANCED, "Advanced>>");
	}
}

void CLoginDlg::OnCheck() 
{
	m_bDialOnDemand = (m_bDialOnDemand + 1) %2;
}

void CLoginDlg::CheckPEConfig() 
{
	XP_StatStruct statinfo; 
	int     ret;
	char    strAcct[MAX_PATH];
	char    strModem[MAX_PATH];
	char    path[MAX_PATH+1];
	HKEY    hKeyRet;
	char    *pString = NULL;
	long    result;
	DWORD   type, size;
	char    name[MAX_PATH];
	int             curSelect;

	curSelect = m_LoginList.GetCurSel();  
	if(curSelect < 0)
	{
		m_strAcctName = "";
		m_strModemName = "";
		m_strProfileName = "";   
		return;
	}
	strcpy(name, (char*)m_LoginList.GetItemDataPtr(curSelect));

	// get profile path
#ifdef XP_WIN32
	strcpy(path, "SOFTWARE\\Netscape\\Netscape Navigator\\Users");
	strcat(path, "\\");
	strcat(path, name);

	if (ERROR_SUCCESS == RegOpenKey(HKEY_LOCAL_MACHINE, path, &hKeyRet)) 
	{
		// see how much space we need
		size = 0;
		result = RegQueryValueEx(hKeyRet, "DirRoot", NULL, &type, NULL, &size);

		// if we didn't find it just use the default
		if((result == ERROR_SUCCESS) && (size != 0)) 
		{
			// allocate space to hold the string
			pString = (char *) XP_ALLOC(size * sizeof(char));

			// actually load the string now that we have the space
			result = RegQueryValueEx(hKeyRet, "DirRoot", NULL, &type, (LPBYTE) pString, &size);
	
			if (hKeyRet) RegCloseKey(hKeyRet);
		}
	}
#else
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	pString = (char *) XP_ALLOC(_MAX_PATH * sizeof(char));
	::GetPrivateProfileString("Users", name,"",pString,_MAX_PATH,csNSCPini);
#endif

	if(pString == NULL)
		return;
	ret = _stat(pString, &statinfo);
	if(ret == 0) 
	{
		// construct the profile full name
		strcpy(path, pString);
		strcat(path, "\\config.ini");
        
        memset(strAcct, 0x00, MAX_PATH);
		::GetPrivateProfileString("Account", "Account", "", strAcct, MAX_PATH, path);
//		::GetPrivateProfileString("Modem", "Modem", "", strModem, MAX_PATH, path);

		m_strAcctName = strAcct;
//		m_strModemName = strModem;
	}   
	m_strProfileName = path;
	
	XP_FREE (pString);
}                       

/////////////////////////////////////////////////////////////////////////////
// CLoginList

CLoginList::CLoginList(): CListBox()
{
	m_iPosIndex = 0;
	m_iPosName = 50;
	m_iPosStatus = 100;
}

CLoginList::~CLoginList()
{
}

void CLoginList::SetColumnPositions(int iPosIndex, int iPosName, int iPosStatus)
{
	m_iPosIndex = iPosIndex;
	m_iPosName = iPosName;
	m_iPosStatus = iPosStatus;
}

BEGIN_MESSAGE_MAP( CLoginList, CListBox )
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP( )

void CLoginList::MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{
	lpMeasureItemStruct->itemHeight = 32;
}
 
void CLoginList::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	HDC hDC = lpDrawItemStruct->hDC;
	RECT rcItem = lpDrawItemStruct->rcItem;
	RECT rcTemp = rcItem;

	if (!lpDrawItemStruct) return;
	if (!lpDrawItemStruct->itemData) return;

	const char * itemData = (const char *)GetItemDataPtr(lpDrawItemStruct->itemID); 
						
	HBRUSH hBrushFill;

#ifndef XP_WIN16
	if ( lpDrawItemStruct->itemState & ODS_SELECTED ) {
		hBrushFill = GetSysColorBrush( COLOR_HIGHLIGHT );
		::SetBkColor( hDC, GetSysColor( COLOR_HIGHLIGHT ) );
		::SetTextColor( hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
	} else {
		hBrushFill = GetSysColorBrush( COLOR_WINDOW );
		::SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
		::SetTextColor( hDC, GetSysColor( COLOR_WINDOWTEXT ) );
	}
#else
	if ( lpDrawItemStruct->itemState & ODS_SELECTED ) {
		hBrushFill = CreateSolidBrush( GetSysColor(COLOR_HIGHLIGHT));
		::SetBkColor( hDC, GetSysColor( COLOR_HIGHLIGHT ) );
		::SetTextColor( hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
	} else {
		hBrushFill = CreateSolidBrush( GetSysColor(COLOR_WINDOW) );
		::SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
		::SetTextColor( hDC, GetSysColor( COLOR_WINDOWTEXT ) );
	}
#endif

	if ( itemData ) {
		CClientDC dc(this);

		::FillRect( hDC, &rcItem, hBrushFill );

		rcTemp.left = m_iPosIndex;
		rcTemp.right = m_iPosName;
		DrawIcon(hDC,rcTemp.left,rcTemp.top,theApp.LoadIcon(IDR_DOCUMENT));

		rcTemp.left = rcTemp.right;
		rcTemp.right = rcItem.right;
		::DrawText( hDC, itemData, -1,  &rcTemp, DT_VCENTER|DT_LEFT|DT_SINGLELINE );
	}
#ifdef XP_WIN16
	if (hBrushFill) DeleteObject(hBrushFill);
#endif
}

char szProfileName[] = "Profile Name:";
char szUserDirectory[] = "UserDirectory";

void CLoginList::OnLButtonDown( UINT nFlags, CPoint point )
{
	CListBox::OnLButtonDown( nFlags, point );
}
  
#include "dialog.h"
#include "setupwiz.h"

int PR_CALLBACK ProfilePrefChangedFunc(const char *pref, void *data) 
{
	if ((!XP_STRCASECMP(pref,"mail.identity.username")) ||
		(!XP_STRCASECMP(pref,"mail.identity.useremail"))) { 
		char * name = login_GetCurrentUser();

		if (name) {
			login_SetUserKeyData(name);
		}
	}
	
	return 0;
}

// The magical prefix of $ROOT in a URL will take us to the directory
// that the program was run from.  If that feature should die, this will
// have to contain some real code...
char * GetASWURL()
{
	char *pString = "$ROOT\\ASW\\start.htm";
	return XP_STRDUP(pString);
}

// This function will attempt to create a new profile.  This
// includes setting up the registry key as well as creating
// the corresponding user directory
char * login_QueryNewProfile(BOOL bUpgrade,CWnd *parent) {
	CNewProfileWizard setupWizard(parent,bUpgrade);
	int ret = setupWizard.DoModal();
	if (!setupWizard.m_pProfileName.IsEmpty()) {
		char * ret = XP_STRDUP(setupWizard.m_pProfileName);
		return ret;
	}
	else return NULL;
}

int login_RenameUserKey(const char *oldName, const char* newName) {
	long    result;
	int             fieldLen = 0;

	if (!strcmpi(oldName,newName)) return FALSE;

#ifdef XP_WIN32
	HKEY hKeyRet;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";
	CString newPath,oldPath;
	HKEY hKeyNewName;
	HKEY hKeyOldName;
    DWORD dwDisposition;

	newPath = csSub+newName;
	oldPath = csSub+oldName;

	// create a new key for our new profile name
	if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, newPath, NULL, NULL, NULL, KEY_WRITE, NULL, &hKeyNewName, &dwDisposition)) {

		// now... we enumerate old profile key contents and copy everything into new profile key
		// first, gets the hkey for the old profile path
		if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, oldPath, NULL, KEY_WRITE | KEY_READ, &hKeyOldName)) {

			DWORD subKeys;
			DWORD maxSubKeyLen;
			DWORD maxClassLen;
			DWORD values;
			DWORD maxValueNameLen;
			DWORD maxValueLen;
			DWORD securityDescriptor;
			FILETIME lastWriteTime;

			// get some information about this old profile key 
			if (ERROR_SUCCESS == RegQueryInfoKey(hKeyOldName, NULL, NULL, NULL, &subKeys, &maxSubKeyLen, &maxClassLen, &values, &maxValueNameLen, &maxValueLen, &securityDescriptor, &lastWriteTime)) {

				// copy the values
				BOOL NoErr = CopyRegKeys(hKeyOldName, hKeyNewName, subKeys, maxSubKeyLen, maxClassLen, values, maxValueNameLen, maxValueLen, oldPath, newPath);

				// delete the old Key, if everything went well
				if (NoErr) {
					login_DeleteUserKey(oldName);					
				}
			}
		}
		RegCloseKey(hKeyNewName);
		RegCloseKey(hKeyOldName);
		return TRUE;
	}

#else
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	// get old dir
	char *pString = (char *) XP_ALLOC(_MAX_PATH * sizeof(char));
	::GetPrivateProfileString("Users", oldName,"",pString,_MAX_PATH,csNSCPini);

	// blank out old name now
	::WritePrivateProfileString("Users", oldName, NULL,csNSCPini);
	result = ::WritePrivateProfileString("Users", newName, pString,csNSCPini);
	if (pString) XP_FREE(pString);
#endif
	return TRUE;
}

int login_CreateNewUserKey(const char *profileName, const char* directory) {
	long    result;
	char    *nullString = "";
	char    *email, *username;
	int             fieldLen = 0;

	if (PREF_GetCharPref("mail.identity.useremail", NULL, &fieldLen) == PREF_NOERROR) {
		email = (char *) XP_ALLOC(fieldLen);
		if (email) {
			PREF_GetCharPref("mail.identity.useremail", email, &fieldLen);
		} else {
			email = nullString;
		}
	}

	fieldLen = 0;
	if (PREF_GetCharPref("mail.identity.username", NULL, &fieldLen) == PREF_NOERROR) {
		username = (char *) XP_ALLOC(fieldLen);
		if (username) {
			PREF_GetCharPref("mail.identity.username", username, &fieldLen);
		} else {
			username = nullString;
		}
	}

#ifdef XP_WIN32
	HKEY hKeyRet;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";

	csSub += profileName;
	result = RegCreateKey(HKEY_LOCAL_MACHINE,
						csSub,
						&hKeyRet);


	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_SET_VALUE,
						&hKeyRet);

	if (result == ERROR_SUCCESS) {
		RegSetValueEx(hKeyRet,"DirRoot",NULL,REG_SZ,(const BYTE *)directory,strlen(directory)+1);
		RegSetValueEx(hKeyRet,"UserName",NULL,REG_SZ,(const BYTE *)username,strlen(username)+1);
		RegSetValueEx(hKeyRet,"EmailAddr",NULL,REG_SZ,(const BYTE *)email,strlen(email)+1);
		RegCloseKey(hKeyRet);
	} else {
		AfxMessageBox(szLoadString(IDS_UNABLE_WRITE_REG));
	}
#else
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	result = ::WritePrivateProfileString("Users", profileName, directory,csNSCPini);        
#endif

	return CASTINT(result);
}

#ifdef XP_WIN32
// this wasn't exported anywhere!
extern LONG RegDeleteKeyNT(HKEY hStartKey, LPCSTR lpszKeyName);
#endif

int login_DeleteUserKey(const char *profileName) {
	long result;
#ifdef XP_WIN32
	HKEY hKeyRet;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";

	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_WRITE,
						&hKeyRet);

	if (result == ERROR_SUCCESS) {
		RegDeleteKeyNT(hKeyRet,profileName);
	}
	if (hKeyRet) RegCloseKey(hKeyRet);
#else
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	result = ::WritePrivateProfileString("Users", profileName, NULL,csNSCPini);     
#endif

	return CASTINT(result);
}

int login_SetUserKeyData(const char *profileName) {
	long    result;
	char    *nullString = "";
	char    *email, *username;
	int             fieldLen = 0;

	if (PREF_GetCharPref("mail.identity.useremail", NULL, &fieldLen) == PREF_NOERROR) {
		email = (char *) XP_ALLOC(fieldLen);
		if (email) {
			PREF_GetCharPref("mail.identity.useremail", email, &fieldLen);
		} else {
			email = nullString;
		}
	}

	fieldLen = 0;
	if (PREF_GetCharPref("mail.identity.username", NULL, &fieldLen) == PREF_NOERROR) {
		username = (char *) XP_ALLOC(fieldLen);
		if (username) {
			PREF_GetCharPref("mail.identity.username", username, &fieldLen);
		} else {
			username = nullString;
		}
	}
	
#ifdef XP_WIN32
	HKEY hKeyRet;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";

	csSub += profileName;

	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_SET_VALUE,
						&hKeyRet);

	if (result == ERROR_SUCCESS) {
		RegSetValueEx(hKeyRet,"UserName",NULL,REG_SZ,(const BYTE *)username,strlen(username)+1);
		RegSetValueEx(hKeyRet,"EmailAddr",NULL,REG_SZ,(const BYTE *)email,strlen(email)+1);
		RegCloseKey(hKeyRet);
	}
#else
#endif
	return CASTINT(result);
}


int login_SetCurrentUser(const char * user) {
	long result;
#ifdef XP_WIN32
	HKEY hKeyRet = NULL;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";

	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_SET_VALUE,
						&hKeyRet);

	if (result == ERROR_SUCCESS) {
		RegSetValueEx(hKeyRet,"CurrentUser",NULL,REG_SZ,(const BYTE *)user,strlen(user)+1);
		RegCloseKey(hKeyRet);
	}
#else
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	result = ::WritePrivateProfileString("Users Additional Info", "CurrentUser", user,csNSCPini);   
#endif
	return CASTINT(result);
}

// checks to see if we think somebody has already used Navigator on this machine
Bool login_UpgradeNeeded(void) {
	// lets just try to load the homepage
	CString csPref= theApp.GetProfileString("Main","Home Page","");
	if (!csPref.IsEmpty()) return TRUE;

	csPref= theApp.GetProfileString("User","User_Addr","");
	if (!csPref.IsEmpty()) return TRUE;

	csPref= theApp.GetProfileString("User","User_Name","");
	if (!csPref.IsEmpty()) return TRUE;

	csPref= theApp.GetProfileString("Bookmark List","File Location","");
	if (!csPref.IsEmpty()) return TRUE;

	return FALSE;
}

int PR_CALLBACK
ProfileNameChangedFunc(const char *pref, void *data)
{
	if (!pref) return FALSE;
	if (!data) return FALSE;

	if (!strcmpi(pref,"profile.name") ) {
		LPSTR	lpszNewName = NULL;

		PREF_CopyCharPref("profile.name", &lpszNewName);
		login_RenameUserKey((char *)data,lpszNewName);
		if (lpszNewName)
			XP_FREE(lpszNewName);
		return TRUE;
	}
	return FALSE;
}

int login_QueryForCurrentProfile() {
	CLoginDlg dlg;
	Bool bGotoASW = FALSE;
	Bool bFirstTimeUser;
	Bool bUpgradeNeeded;
	Bool bRet;
	int  numProfiles;
	char * pString = NULL;
	CString csProfileSelected;

	char aPath[_MAX_PATH];
	::GetModuleFileName(theApp.m_hInstance, aPath, _MAX_PATH);
	//      Kill the last slash and everything after it
	char *pSlash = ::strrchr(aPath, '\\');
	if(pSlash)
		*pSlash = '\0';
	CString csLockFile = aPath;
	csLockFile += "\\netscape.cfg";

	// we need to read this early so that any defaults get set for the setup wizard's use.
	// we will read it again at the end to overwrite any user settings
	PREF_ReadLockFile(csLockFile);

	csProfileSelected.Empty();

	if (theApp.m_bNetworkProfile) {
#ifdef MOZ_LOC_INDEP
		return winli_QueryNetworkProfile();
#else
    return FALSE;
#endif // MOZ_LOC_INDEP
	}
	//------------------------------------------------------------------------------------------------
	//  KLUDGE!
	//    The Account Setup Wizard is now a Java/Javascript application.  Since they have no control
	//    over the profile that will contain the preference changes, we use a hidden magic profile to
	//    hold their changes until they're done MUCKing about.  When they've finished, they will 
	//    rename the profile based on user inputs.  If all goes well, the magic profile never outlives
	//    any given instance of the navigator/ASW.  However, if anything fails the magic profile could
	//    be left lying about.  This would be bad.
	//
	//    The most unfortunate occurance of this problem would be while creating the first profile.
	//    In this case, we don't know the user hasn't successfully created a profile unless we check
	//    to see if the magic profile is the only one.  The simplest way to handle this is to just
	//    expunge the magic profile whenever we get here and find that it exists.
	//------------------------------------------------------------------------------------------------
	//
	//  Delete the magic profile if it exists
	CUserProfileDB profileDB;
	profileDB.DeleteUserProfile( ASW_MAGIC_PROFILE_NAME );

	profileDB.GetUserProfilesList( &dlg.m_Profiles );
	bFirstTimeUser = dlg.m_Profiles.IsEmpty();
	numProfiles    = dlg.m_Profiles.GetCount();
	if (bFirstTimeUser)
		bUpgradeNeeded = login_UpgradeNeeded();
	else
		bUpgradeNeeded = FALSE;

	/////////////////////////////////////////////////////////////////////////////////////
	// The account setup flag and the new profile flag are basically mutually exclusive,
	// but check the account setup flag first just in case.  The account setup flag is
	// an override that causes the account setup wizard to run instead of the profile
	// wizard
	//
	//-----------------------------------------------------------
	// Decide what to do between MUP/MUC/ASW
	//-----------------------------------------------------------
	if (bFirstTimeUser && theApp.m_bPEEnabled)
	{
		// PE!

		// Disable -profile_manager in this case since we don't
		// want first-time users to see it for PE
		theApp.m_bProfileManager = FALSE;

		if (bUpgradeNeeded)
		{
			// Upgrades do upgrade dialog and then to ASW
			if (!login_QueryNewProfile(bUpgradeNeeded,NULL))
				return FALSE;

			// Tell ASW to go to Upgrade screen
			PREF_SetBoolPref("account_setup.upgraded", TRUE);
		}
		else
		{
			// Very first _account_ (this _is_ PE after all)

			// Get a directory name that'll work on the current system
			CString theDirectory;
			if (!profileDB.AssignProfileDirectoryName(ASW_MAGIC_PROFILE_NAME, theDirectory))
				return FALSE;

			// Create the Magic Profile for ASW
			profileDB.AddNewProfile(ASW_MAGIC_PROFILE_NAME, 
						 theDirectory,
						 UPGRADE_IGNORE );

			bGotoASW = TRUE;
		}

		// We now have exactly one profile
	} 
	else if (theApp.m_bPEEnabled && theApp.m_bAccountSetup)
	{
		// PE, Some normal profile(s) exists and ASW requested
		bGotoASW = TRUE;
	}
	else if ((bFirstTimeUser || theApp.m_bCreateNewProfile) && !theApp.m_bProfileManager)
	{
		// No profiles or -new_profile requested
		char * profile = login_QueryNewProfile(bUpgradeNeeded,NULL);
		if (!profile)
			return FALSE;
		else {
			login_SetCurrentUser(profile);
		}
	}

	// This information may have changed
	profileDB.GetUserProfilesList( &dlg.m_Profiles );
	numProfiles    = dlg.m_Profiles.GetCount();
	PREF_SetDefaultIntPref( "profile.numprofiles", numProfiles );

	// See if we need to do wizardry to get to ASW
	if (bGotoASW)
	{
		char *asw = GetASWURL();
		theApp.m_csPEPage = asw;
		theApp.m_bAlwaysDockTaskBar = TRUE;
		XP_FREEIF(asw);
		theApp.m_bAccountSetup     = TRUE;
		theApp.m_bAccountSetupStartupJava = TRUE;  // Need this here too...
	}
	else
		theApp.m_bAccountSetup     = FALSE;


	theApp.m_bCreateNewProfile = FALSE; // already did it


	if (theApp.m_bProfileManager) {
		if (dlg.DoModal()==IDCANCEL) {
			return FALSE;
		};
		csProfileSelected =(const char *)dlg.m_ProfileSelected;
	} else {

		if (numProfiles == 0) {
			assert(0);
			theApp.m_bCreateNewProfile = TRUE; // Do it again!
			return login_QueryForCurrentProfile();  // run us again
		}

		if (numProfiles == 1) {                           
			// we only have one profile -- just use it!
			csProfileSelected = dlg.m_Profiles.GetHead();

		}
		else if (theApp.m_CmdLineProfile) {
			csProfileSelected = theApp.m_CmdLineProfile;
		}
		else {
			// ask the user to choose!
			if (dlg.DoModal()==IDCANCEL) {
				return FALSE;
			} else if (dlg.m_ProfileSelected == "")
				return TRUE; // LI NetProfile selected.
			csProfileSelected =(const char *)dlg.m_ProfileSelected;
		}
	}
	
	/* we have the profile, publish the name as a pref */
	PREF_SetDefaultCharPref( "profile.name", csProfileSelected );

	pString = profileDB.GetUserProfileValue( csProfileSelected, "DirRoot" );

	// read in the preference file (will assume valid dir here...we need to know if LI)
	// REMIND:  JEM:  we now read prefs twice...just to find this one pref...we should parse the file
	CString csTmp = pString;
	csTmp += "\\prefs.js";
	PREF_Init((char *)(const char *)csTmp);

	XP_Bool prefBool=FALSE;
	PREF_GetBoolPref("li.enabled",&prefBool);

	theApp.m_UserDirectory = pString;
	if (pString)
		PREF_SetDefaultCharPref( "profile.directory", pString );

// I figure this is safe since "li.enabled" should always be false if not MOZ_LOC_INDEP
#ifdef MOZ_LOC_INDEP
	if (prefBool) 
		bRet = FEU_StartGetCriticalFiles(csProfileSelected,pString);
	else
#endif // MOZ_LOC_INDEP
		bRet = login_ProfileSelectedCompleteTheLogin(csProfileSelected,pString);
  // Careful, the above line is actually inside an if statement.

	XP_FREEIF(pString);
	return bRet;
}

Bool login_ProfileSelectedCompleteTheLogin(const char * szProfileName, const char * szProfileDir) {


	CString csTmp;

	// verify directory exists
	XP_StatStruct statinfo; 
	int ret = _stat((char *)(const char *)theApp.m_UserDirectory, &statinfo);
	if(ret == -1) {
		char szMsg[256];
		sprintf(szMsg, szLoadString(IDS_BAD_DIRECTORY2), theApp.m_UserDirectory);

		int choice = AfxMessageBox(szMsg,MB_OKCANCEL);
		if (choice == IDOK) {
			theApp.m_bCreateNewProfile = TRUE;  // force wizard to run
			theApp.m_bProfileManager = FALSE;
			return login_QueryForCurrentProfile();  // run us again
		} else return FALSE;
	}

	// this call needs to be done after the last chance that profile.name would inadvertantly change
	// in other words, it needs to be below the above recursive call to login_QueryForCurrentProfile()
	PREF_RegisterCallback("profile.name",ProfileNameChangedFunc,(void *)(const char *)szProfileName);

	// read in the preference file which we read-write
	csTmp = theApp.m_UserDirectory;
	csTmp += "\\prefs.js";

	PREF_Init((char *)(const char *)csTmp);

	// read in the user's LI prefs file
	char * prefName = WH_FileName(NULL, xpLIPrefs);
	PREF_ReadLIJSFile(prefName);
	XP_FREEIF (prefName);

	// read in the users optional JS file
	csTmp = theApp.m_UserDirectory;
	csTmp += "\\user.js";
	PREF_ReadUserJSFile((char *)(const char *)csTmp);

#ifndef MOZ_NGLAYOUT
  	// read in the users optional JS HTML Hook file
  	csTmp = theApp.m_UserDirectory;
  	csTmp += "\\hook.js";
  	HK_ReadHookFile((char *)(const char *)csTmp);
#endif /* MOZ_NGLAYOUT */

	char aPath[_MAX_PATH];
	::GetModuleFileName(theApp.m_hInstance, aPath, _MAX_PATH);
	//      Kill the last slash and everything after it
	char *pSlash = ::strrchr(aPath, '\\');
	if(pSlash)
		*pSlash = '\0';
	CString csLockFile = aPath;
	csLockFile += "\\netscape.cfg";

	// check for required file: netscape.cfg
	if (!FEU_SanityCheckFile(csLockFile)) {
		AfxMessageBox(szLoadString(IDS_MISSING_CFG));
		return FALSE;
	}

	// read the optional profile.cfg file
	CString csProfileCfg;
	csProfileCfg = theApp.m_UserDirectory;
	csProfileCfg += "\\profile.cfg";
	PREF_ReadLockFile(csProfileCfg);

	if (PREF_ReadLockFile(csLockFile) == PREF_BAD_LOCKFILE)	{
		AfxMessageBox(szLoadString(IDS_INVALID_CFG));
		return FALSE;
	}


	PREF_SavePrefFile(); // explicit save to get any prefs set before filename was set
	login_SetCurrentUser(szProfileName);
		
	PREF_RegisterCallback("mail.identity",ProfilePrefChangedFunc, NULL);
	return TRUE;
}


char * login_GetCurrentUser() 
{
	long result;
#ifdef XP_WIN32
	DWORD type, size;
	HKEY hKeyRet;

	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";

	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_QUERY_VALUE,
						&hKeyRet);

	if (result != ERROR_SUCCESS) {
		return NULL;
	}

	// see how much space we need
	result = RegQueryValueEx(hKeyRet,
							  (char *) "CurrentUser",
							  NULL,
							  &type,
							  NULL,
							  &size);

	// if we didn't find it error!
	if((result != ERROR_SUCCESS) || (size == 0))
		//AfxMessageBox("Unable to determine last user");
		return NULL;

	// allocate space to hold the string
	char * pString = (char *) XP_ALLOC(size * sizeof(char));

	// actually load the string now that we have the space
	result = RegQueryValueEx(hKeyRet,
							  (char *) "CurrentUser",
							  NULL,
							  &type,
							  (LPBYTE) pString,
							  &size);

	if((result != ERROR_SUCCESS) || (size == 0))
		return NULL;
	if (hKeyRet) RegCloseKey(hKeyRet);
	return pString;
#else
	auto char ca_iniBuff[_MAX_PATH];
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	if (::GetPrivateProfileString("Users Additional Info", "CurrentUser","",ca_iniBuff,_MAX_PATH,csNSCPini))
		return XP_STRDUP(ca_iniBuff);
	else return NULL;
#endif
}

char *login_GetUserByName(const char *userName) {
	long result;
	char            *matchedProfile = NULL;

#ifdef XP_WIN32
	HKEY            hKeyRet;
	CString         csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";
	DWORD           keyIndex = 0, usernameLen;
	auto char       subkeyName[MAX_PATH+1];
	char            *pUserString;
	
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_READ,
						&hKeyRet);

	while ((result == ERROR_SUCCESS) && (!matchedProfile)) {
		if ((result = RegEnumKey(hKeyRet,
					keyIndex++,
					subkeyName,
					MAX_PATH+1)) == ERROR_SUCCESS) {
			CString         csKeyName = csSub + subkeyName;
			HKEY            hSubkeyRet;
			
			if ((result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
								csKeyName,
								NULL,
								KEY_READ,
								&hSubkeyRet)) == ERROR_SUCCESS) {

				// see how much space we need
				usernameLen = 0;
				result = RegQueryValueEx(hSubkeyRet,
										  (char *) "UserName",
										  NULL,
										  NULL,
										  NULL,
										  &usernameLen);

				if (result == ERROR_SUCCESS) {
					// allocate space to hold the string
					pUserString = (char *) XP_ALLOC(usernameLen * sizeof(char));

					if (pUserString) {
						// actually load the string now that we have the space
						result = RegQueryValueEx(hSubkeyRet,
												  (char *) "UserName",
												  NULL,
												  NULL,
												  (LPBYTE) pUserString,
												  &usernameLen);

						if ((result == ERROR_SUCCESS) && (XP_STRCMP(pUserString, userName) == 0)) {
							matchedProfile = XP_STRDUP(subkeyName);
						}

						XP_FREE(pUserString);
					}
				}

			RegCloseKey(hSubkeyRet);
			}
			// if we have a problem reading this entry, still go on to the next
			result = ERROR_SUCCESS;
		}

	}

	RegCloseKey(hKeyRet);

#else
/*	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	result = ::WritePrivateProfileString("Users", profileName, NULL,csNSCPini);     */
#endif
	return matchedProfile;
}


char *login_GetUserByEmail(const char *email) {
	long result;
	char            *matchedProfile = NULL;

#ifdef XP_WIN32
	HKEY            hKeyRet;
	CString         csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";
	DWORD           keyIndex = 0, emailLen;
	auto char       subkeyName[MAX_PATH+1];
	char            *pEmailString;
	
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_READ,
						&hKeyRet);

	while ((result == ERROR_SUCCESS) && (!matchedProfile)) {
		if ((result = RegEnumKey(hKeyRet,
					keyIndex++,
					subkeyName,
					MAX_PATH+1)) == ERROR_SUCCESS) {
			CString         csKeyName = csSub + subkeyName;
			HKEY            hSubkeyRet;
			
			if ((result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
								csKeyName,
								NULL,
								KEY_READ,
								&hSubkeyRet)) == ERROR_SUCCESS) {

				// see how much space we need
				emailLen = 0;
				result = RegQueryValueEx(hSubkeyRet,
										  (char *) "EmailAddr",
										  NULL,
										  NULL,
										  NULL,
										  &emailLen);

				if (result == ERROR_SUCCESS) {
					// allocate space to hold the string
					pEmailString = (char *) XP_ALLOC(emailLen * sizeof(char));

					if (pEmailString) {
						// actually load the string now that we have the space
						result = RegQueryValueEx(hSubkeyRet,
												  (char *) "EmailAddr",
												  NULL,
												  NULL,
												  (LPBYTE) pEmailString,
												  &emailLen);

						if ((result == ERROR_SUCCESS) && (XP_STRCMP(pEmailString, email) == 0)) {
							matchedProfile = XP_STRDUP(subkeyName);
						}

						XP_FREE(pEmailString);
					}
				}

			RegCloseKey(hSubkeyRet);
			}
			// if we have a problem reading this entry, still go on to the next
			result = ERROR_SUCCESS;
		}

	}

	RegCloseKey(hKeyRet);

#else
/*	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	result = ::WritePrivateProfileString("Users", profileName, NULL,csNSCPini);     */
#endif
	return matchedProfile;
}


#ifdef XP_WIN32
#define MY_FINDFIRST(a,b) FindFirstFile(a,b) 
#define MY_FINDNEXT(a,b) FindNextFile(a,b) 
#define ISDIR(a) (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
#define MY_FINDCLOSE(a) FindClose(a) 
#define MY_FILENAME(a) a.cFileName
#define MY_FILESIZE(a) (a.nFileSizeHigh * MAXDWORD) + a.nFileSizeLow
#else
static unsigned u_finding = _A_NORMAL | _A_RDONLY | _A_ARCH | _A_SUBDIR;

#define MY_FINDFIRST(a,b) (_dos_findfirst(a,u_finding,b)==0)
#define MY_FINDNEXT(a,b) (_dos_findnext(b)==0) 
#define ISDIR(a) (a.attrib & _A_SUBDIR)  
#define MY_FINDCLOSE(a) a=INVALID_HANDLE_VALUE 
#define MY_FILENAME(a) a.name
#define MY_FILESIZE(a) a.size
#endif

int WFEU_CheckDiskSpaceForMove(CString src,CString dst,uint32 * iSpaceNeeded)
{
#ifdef XP_WIN16
	struct _find_t data_ptr;
	unsigned find_handle;
#else
	WIN32_FIND_DATA data_ptr;
	HANDLE find_handle;
#endif

	// Append slash to the end of the directory names if not there
	if (dst.Right(1) != "\\")
		dst += "\\";

	if (src.Right(1) != "\\")
		src += "\\";

	find_handle = MY_FINDFIRST(src+"*.*", &data_ptr);

	if (find_handle != INVALID_HANDLE_VALUE) {
		do  {

			if (ISDIR(data_ptr)
				&& (strcmpi(MY_FILENAME(data_ptr),"."))
				&& (strcmpi(MY_FILENAME(data_ptr),".."))) {
					WFEU_CheckDiskSpaceForMove(
						src + MY_FILENAME(data_ptr),
						dst + MY_FILENAME(data_ptr),
						iSpaceNeeded);
			}
			else if (!ISDIR(data_ptr)) {
				*iSpaceNeeded+= MY_FILESIZE(data_ptr);
			}
		} while(MY_FINDNEXT(find_handle,&data_ptr));
		MY_FINDCLOSE(find_handle);
	}
	return TRUE;
}

#ifndef XP_WIN16
// doesn't work in win16...copy individual files
int WFEU_MoveDirectorySameDrive(CString src,CString dst,CUpdateFileDlg * pDlg,char *category) 
{
	// Append slash to the end of the directory names if not there
	if (dst.Right(1) != "\\")
		dst += "\\";

	if (src.Right(1) != "\\")
		src += "\\";

	// update status dialog
	if (pDlg) pDlg->StartFileUpdate(category,(char *)(const char *)src);

	BOOL bRet = WFE_MoveFile(src,dst);
	return (bRet);
}
#endif

// 
int WFEU_MoveDirectoryRecursiveDiffDrive(CString src,CString dst,CUpdateFileDlg * pDlg,char *category,XP_Bool bCopyDontMove) 
{
#ifdef XP_WIN16
	struct _find_t data_ptr;
	unsigned find_handle;
#else
	WIN32_FIND_DATA data_ptr;
	HANDLE find_handle;
#endif

#ifdef XP_WIN16
	// before the extra slash
	_mkdir(dst);
#endif
	// Append slash to the end of the directory names if not there
	if (dst.Right(1) != "\\")
		dst += "\\";

	if (src.Right(1) != "\\")
		src += "\\";

#ifdef XP_WIN32
	// this wants (or at least tolerates the extra slash)
	CreateDirectory(dst, NULL);
#endif
	find_handle = MY_FINDFIRST(src+"*.*", &data_ptr);
	if (find_handle != INVALID_HANDLE_VALUE) {
		do  {
			if (ISDIR(data_ptr) 
				&& (strcmpi(MY_FILENAME(data_ptr),"."))
				&& (strcmpi(MY_FILENAME(data_ptr),".."))) {
					WFEU_MoveDirectoryRecursiveDiffDrive(
						src + MY_FILENAME(data_ptr), 
						dst + MY_FILENAME(data_ptr),
						pDlg,
						category,
						bCopyDontMove);
			}
			else if (!ISDIR(data_ptr)) {
				if (pDlg) pDlg->StartFileUpdate(category,MY_FILENAME(data_ptr));
				if (bCopyDontMove)
					WFE_CopyFile(src + MY_FILENAME(data_ptr), dst + MY_FILENAME(data_ptr));
				else
					WFE_MoveFile(src + MY_FILENAME(data_ptr), dst + MY_FILENAME(data_ptr));
			}
		} while(MY_FINDNEXT(find_handle,&data_ptr));
		MY_FINDCLOSE(find_handle);
		return TRUE;
	}
	return FALSE;
}

// duplicate of line #1323 in mknewsgr.c -- Bad but don't wanna mess with header files
#define NEWSRC_MAP_FILE_COOKIE "netscape-newsrc-map-file"

int     WFEU_UpdateNewsFatFile(CString src,CString dst)
{
	XP_File src_fp,dst_fp;
	XP_StatStruct stats;
	long fileLength;
	CString csSrcFat,csDstFat;

		// Append slash to the end of the directory names if not there
	if (dst.Right(1) != "\\")
		dst += "\\";

	if (src.Right(1) != "\\")
		src += "\\";

	csSrcFat = src + "fat";
	csDstFat = dst + "fat.new";

    if (_stat(csSrcFat, &stats) == -1)
		return FALSE;

	fileLength = stats.st_size;
	if (fileLength <= 1)
		return FALSE;
	
	src_fp = fopen(csSrcFat, "r");
	dst_fp = fopen(csDstFat, "wb");

	if (src_fp && dst_fp) { 
		char buffer[512];
		char psuedo_name[512];
		char filename[512];
		char is_newsgroup[512];

		// This code is all stolen from mknewsgr.c -- written by JRE

		/* get the cookie and ignore */
		XP_FileReadLine(buffer, sizeof(buffer), src_fp);

		XP_FileWrite(NEWSRC_MAP_FILE_COOKIE, XP_STRLEN(NEWSRC_MAP_FILE_COOKIE), dst_fp);
		XP_FileWrite(LINEBREAK, XP_STRLEN(LINEBREAK), dst_fp);

		while(XP_FileReadLine(buffer, sizeof(buffer), src_fp))
		  {
			char * p;
			int i;
			
			filename[0] = '\0';
			is_newsgroup[0]='\0';

			for (i = 0, p = buffer; *p && *p != '\t' && i < 500; p++, i++)
				psuedo_name[i] = *p;
			psuedo_name[i] = '\0';
			if (*p) 
			  {
				for (i = 0, p++; *p && *p != '\t' && i < 500; p++, i++)
					filename[i] = *p;
				filename[i]='\0';
				if (*p) 
				  {
					for (i = 0, p++; *p && *p != '\r' && *p != '\n' && i < 500; p++, i++)
						is_newsgroup[i] = *p;
					is_newsgroup[i]='\0';
				  }
			  }

			CString csFilename = filename;
			CString csDestFilename = dst;
			int iLastSlash = csFilename.ReverseFind('\\');

			if (iLastSlash != -1) {
				csDestFilename += csFilename.Right(csFilename.GetLength()-iLastSlash-1);
			} else 
				csDestFilename += filename;

			// write routines stolen from mknewsgr.c -- line #1348
			XP_FileWrite(psuedo_name, XP_STRLEN(psuedo_name),dst_fp);
			
			XP_FileWrite("\t", 1, dst_fp);

			XP_FileWrite((const char *)csDestFilename, csDestFilename.GetLength() , dst_fp);
			
			XP_FileWrite("\t", 1, dst_fp);

			XP_FileWrite(is_newsgroup, XP_STRLEN(is_newsgroup), dst_fp);

			XP_FileWrite(LINEBREAK, XP_STRLEN(LINEBREAK), dst_fp);

		  }
		XP_FileClose(src_fp);
		XP_FileClose(dst_fp);
	} else
		return FALSE;
	
	return TRUE;
}

int WFEU_UpdaterCopyDirectory(CString src,CString dst,CUpdateFileDlg * pDlg,char *category) 
{
	uint32 iSpaceNeeded = 0;
	uint32 iSpaceAvailable = 0;
	DWORD dwSectorsPerCluster, dwBytesPerSector, dwNumFreeClusters, dwTotalClusters;
	
	// assumes properly formated destination string
	GetDiskFreeSpace(dst.Left(3),&dwSectorsPerCluster,&dwBytesPerSector,
		&dwNumFreeClusters,&dwTotalClusters);

	iSpaceAvailable = (dwNumFreeClusters * dwSectorsPerCluster * dwBytesPerSector);
	WFEU_CheckDiskSpaceForMove(src,dst,&iSpaceNeeded);
	while (iSpaceNeeded > iSpaceAvailable) {
		char szLen[64];
		_ltoa(((iSpaceNeeded-iSpaceAvailable)/1024)/1024,szLen,10);

		char szMsg[1024];
		sprintf(szMsg, szLoadString(IDS_INSUFFICIENT_DISKSPACE_COPY), category, src, dst, szLen);

		if (AfxMessageBox(szMsg,MB_OKCANCEL) == IDCANCEL)
			return FALSE;
		GetDiskFreeSpace(dst.Left(3),&dwSectorsPerCluster,&dwBytesPerSector,
			&dwNumFreeClusters,&dwTotalClusters);
		iSpaceAvailable = (dwNumFreeClusters * dwSectorsPerCluster * dwBytesPerSector);
	}
	return WFEU_MoveDirectoryRecursiveDiffDrive(src,dst,pDlg,category,TRUE);
}

int WFEU_UpdaterMoveDirectory(CString src,CString dst,CUpdateFileDlg * pDlg,char *category) 
{
#ifndef XP_WIN16
	// directory rename doesn't work under win16
	int iDiffDrive = src.Left(2).CompareNoCase(dst.Left(2));

	if (!iDiffDrive) {
		int iRet = WFEU_MoveDirectorySameDrive(src,dst,pDlg,category);
		if (iRet)
			return iRet;
	}
#else
	int iDiffDrive = TRUE;
#endif
	// else if DiffDrive or if move same drive failed...

	if (iDiffDrive) {
		uint32 iSpaceNeeded = 0;
		uint32 iSpaceAvailable = 0;
		DWORD dwSectorsPerCluster, dwBytesPerSector, dwNumFreeClusters, dwTotalClusters;
		
		// assumes properly formated destination string
		GetDiskFreeSpace(dst.Left(3),&dwSectorsPerCluster,&dwBytesPerSector,
			&dwNumFreeClusters,&dwTotalClusters);

		iSpaceAvailable = (dwNumFreeClusters * dwSectorsPerCluster * dwBytesPerSector);
		WFEU_CheckDiskSpaceForMove(src,dst,&iSpaceNeeded);
		while (iSpaceNeeded > iSpaceAvailable) {
			char szLen[64];
			_ltoa(((iSpaceNeeded-iSpaceAvailable)/1024)/1024,szLen,10);

			char szMsg[1024];
			sprintf(szMsg, szLoadString(IDS_INSUFFICIENT_DISKSPACE_MOVE), category, src, dst, szLen);

			if (AfxMessageBox(szMsg,MB_OKCANCEL) == IDCANCEL)
				return FALSE;
			GetDiskFreeSpace(dst.Left(3),&dwSectorsPerCluster,&dwBytesPerSector,
				&dwNumFreeClusters,&dwTotalClusters);
			iSpaceAvailable = (dwNumFreeClusters * dwSectorsPerCluster * dwBytesPerSector);
		}
	}
	return WFEU_MoveDirectoryRecursiveDiffDrive(src,dst,pDlg,category,FALSE);
}


static struct update_PrefTable prefUpdater[] =
{
  {"browser.cache.disk_cache_ssl","Cache","Disk Cache SSL",PREF_BOOL},
  {"browser.cache.disk_cache_size","Cache","Disk Cache Size",PREF_INT},
  {"browser.cache.memory_cache_size","Cache","Memory Cache Size",PREF_INT},
//
  {"security.enable_java","Java","Enable Java",PREF_BOOL},
  {"security.enable_javascript","Java","Enable JavaScript",PREF_BOOL},
//  
  {"mail.check_time","Mail","Check Time",PREF_INT},
  {"mail.max_size","Mail","Max Size",PREF_INT},
  {"mail.pop_password","Mail","POP Password",PREF_CHAR},
  {"mail.pop_name","Mail","POP Name",PREF_CHAR},
  {"mail.leave_on_server","Mail","Leave on server",PREF_BOOL},
  {"mail.auto_quote","Mail","Auto quote",PREF_BOOL},
  {"mail.remember_password","Mail","Remember Password",PREF_BOOL},
//
  {"browser.underline_anchors","Main","Anchor Underline",PREF_BOOL},
  {"browser.cache.check_doc_frequency","Main","Check Server",PREF_INT},
  {"browser.download_directory","Main","Default Save Dir",PREF_CHAR},
  {"browser.wfe.ignore_def_check","Main","Ignore DefCheck",PREF_BOOL},
  {"browser.startup.homepage","Main","Home Page",PREF_CHAR},
//
  {"security.submit_email_forms","Network","Warn Submit Email Form",PREF_BOOL},
  {"security.email_as_ftp_password","Network","Use Email For FTP",PREF_BOOL},
  {"network.cookie.warnAboutCookies","Network","Warn Accepting Cookie",PREF_BOOL},
#if defined(SingleSignon)
  {"network.signon.rememberSignons","Network","Remember Signons",PREF_BOOL},
#endif

  {"network.max_connections","Network","Max Connections",PREF_INT},
  {"network.tcpbufsize","Network","TCP Buffer Size",PREF_INT},
//
  {"news.max_articles","News","News Chunk Size",PREF_INT},
  {"news.show_headers","News","Show Headers",PREF_INT},
  {"news.sort_by","News","Sort News",PREF_INT},
  {"news.thread_news","News","Thread News",PREF_BOOL},
//
  {"network.proxy.autoconfig_url","Proxy Information","Auto Config Url",PREF_CHAR},
  {"network.proxy.type","Proxy Information","Proxy Type",PREF_INT},
  {"network.proxy.wais_port","Proxy Information","Wais_ProxyPort",PREF_INT},
  {"network.proxy.ftp_port","Proxy Information","Ftp_ProxyPort",PREF_INT},
  {"network.proxy.ssl_port","Proxy Information","HTTPS_ProxyPort",PREF_INT},
  {"network.proxy.gopher_port","Proxy Information","Gopher_ProxyPort",PREF_INT},
  {"network.proxy.http_port","Proxy Information","Http_ProxyPort",PREF_INT},
  {"network.proxy.no_proxies_on","Proxy Information","No_Proxy",PREF_CHAR},
  {"network.proxy.wais","Proxy Information","Wais_Proxy",PREF_CHAR},
  {"network.proxy.ssl","Proxy Information","HTTPS_Proxy",PREF_CHAR},
  {"network.proxy.gopher","Proxy Information","Gopher_Proxy",PREF_CHAR},
  {"network.proxy.ftp","Proxy Information","FTP_Proxy",PREF_CHAR},
  {"network.proxy.http","Proxy Information","HTTP_Proxy",PREF_CHAR},
//
  {"security.warn_submit_insecure","Security","Warn Insecure Forms",PREF_BOOL},
  {"security.default_personal_cert","Security","Default User Cert",PREF_CHAR},
  {"security.enable_ssl3","Security","Enable SSL v3",PREF_BOOL},
  {"security.enable_ssl2","Security","Enable SSL v2",PREF_BOOL},
  {"security.ask_for_password","Security","Ask for password",PREF_INT},
  {"security.password_lifetime","Security","Password Lifetime",PREF_INT},
  {"security.warn_entering_secure","Security","Warn Entering",PREF_BOOL},
  {"security.warn_leaving_secure","Security","Warn Leaving",PREF_BOOL},
  {"security.warn_viewing_mixed","Security","Warn Mixed",PREF_BOOL},
  {"security.ciphers","Security","Cipher Prefs",PREF_CHAR},
//
  {"mail.use_exchange","Services","Mapi",PREF_BOOL},
  {"network.hosts.socks_serverport","Services","SOCKS_ServerPort",PREF_INT},
  {"network.hosts.socks_conf","Services","Socks Conf",PREF_CHAR},
  {"network.hosts.pop_server","Services","POP_Server",PREF_CHAR},
  {"network.hosts.nntp_server","Services","NNTP_Server",PREF_CHAR},
  {"network.hosts.smtp_server","Services","SMTP_Server",PREF_CHAR},
  {"network.hosts.socks_server","Services","SOCKS_Server",PREF_CHAR},
//
  {"mail.fixed_width_messages","Settings","Fixed Width Messages",PREF_BOOL},
  {"mail.quoted_style","Settings","Quoted Style",PREF_INT},
  {"mail.quoted_size","Settings","Quoted Size",PREF_INT},
  {"browser.blink_allowed","Settings","Blinking",PREF_BOOL},
  {"browser.link_expiration","Settings","History Expiration",PREF_INT},
//
  {"mail.identity.reply_to","User","Reply_To",PREF_CHAR},
  {"mail.identity.organization","User","User_Organization",PREF_CHAR},
  {"mail.signature_file","User","Sig_File",PREF_CHAR},
//
  {"editor.publish_location", "Publish", "Default Location", PREF_CHAR},
  {"editor.publish_browse_location", "Publish", "Default Browse Location", PREF_CHAR},
//
  {NULL,NULL,NULL,NULL}
};

int     login_UpdatePreferencesToJavaScript(const char * path)
{
	int idx=0;
	while (prefUpdater[idx].xp_name) {
		if (prefUpdater[idx].type == PREF_CHAR) {// char pref 
			CString csPref= theApp.GetProfileString(prefUpdater[idx].section,prefUpdater[idx].name,"");
			if (!csPref.IsEmpty())
				PREF_SetCharPref(prefUpdater[idx].xp_name,(char *)(const char *)csPref);
		} else if (prefUpdater[idx].type == PREF_INT) {// char pref 
			int iPref= theApp.GetProfileInt(prefUpdater[idx].section,prefUpdater[idx].name,-1);
			if (iPref != -1)
				PREF_SetIntPref(prefUpdater[idx].xp_name,iPref);
		} else if (prefUpdater[idx].type == PREF_BOOL) {// char pref 
			CString csPref= theApp.GetProfileString(prefUpdater[idx].section,prefUpdater[idx].name,"");
			if (!csPref.IsEmpty()) {
				int iRet = csPref.CompareNoCase("yes");
				if (iRet ==0)
					PREF_SetBoolPref(prefUpdater[idx].xp_name,TRUE);
				else
					PREF_SetBoolPref(prefUpdater[idx].xp_name,FALSE);
			}
		}
		idx++;
	}

	// Do some special hacking on "browser.cache.check_doc_frequency" since the values changed
	// between 3.0 and 4.0
	int32 iExpr;

	PREF_GetIntPref("browser.cache.check_doc_frequency",&iExpr);
	if (iExpr == 1)
		PREF_SetIntPref("browser.cache.check_doc_frequency",0);
	else if (iExpr == 0)
		PREF_SetIntPref("browser.cache.check_doc_frequency",1);

	// Do some special processing for history link expiration. It used to be that there
	// was an option that history never expired. We no longer support that, in the UI
	// anyway
	int32   iExp;

	PREF_GetIntPref("browser.link_expiration", &iExp);
	if (iExp == -1) {
		// Set the date to something safe like 6 months
		PREF_SetIntPref("browser.link_expiration", 180);
	}

	CString csPref= theApp.GetProfileString("Main","Autoload Home Page","");
	if (!csPref.IsEmpty()) {
		if (!csPref.CompareNoCase("yes"))
			PREF_SetIntPref("browser.startup.page", 1);  // set to load homepage
		else
			PREF_SetIntPref("browser.startup.page", 0);  // set to blank
	}
	return TRUE;
}

void login_CreateEmptyProfileDir(const char * dst, CWnd * pParent, BOOL bExistingDir) 
{
		CString csTmp;
		
		csTmp = dst;
		csTmp += "\\News";
		_mkdir(csTmp);

		csTmp = dst;
		csTmp += "\\Mail";
		_mkdir(csTmp);
		
		csTmp = dst;
		csTmp += "\\Cache";
		_mkdir(csTmp);

		csTmp = dst;
		csTmp += "\\NavCntr";
		_mkdir(csTmp);

		if (!bExistingDir) login_CopyStarterFiles(dst,pParent);
}

/* The following has to be declared extern "C" because it's called from some Java
   native code written in C. */
extern "C"
char * login_GetUserProfileDir()
{
	char    *pString = NULL;

	/* First, get the user's profile name */
	 char	*name = login_GetCurrentUser();

#ifdef XP_WIN32
	char    path[MAX_PATH+1];
	long    result;
	DWORD   type, size;
	HKEY    hKeyRet;

	 /* get profile path */
	strcpy(path, "SOFTWARE\\Netscape\\Netscape Navigator\\Users");
	strcat(path, "\\");
	strcat(path, name);

	if (ERROR_SUCCESS == RegOpenKey(HKEY_LOCAL_MACHINE, path, &hKeyRet)) 
	{
		// see how much space we need
		size = 0;
		result = RegQueryValueEx(hKeyRet, "DirRoot", NULL, &type, NULL, &size);

		// if we didn't find it just use the default
		if((result == ERROR_SUCCESS) && (size != 0)) 
		{
			// allocate space to hold the string
			pString = (char *) XP_ALLOC(size * sizeof(char));

			// actually load the string now that we have the space
			result = RegQueryValueEx(hKeyRet, "DirRoot", NULL, &type, (LPBYTE) pString, &size);
	
			if (hKeyRet) RegCloseKey(hKeyRet);
		}
	}

#else
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	pString = (char *) XP_ALLOC(_MAX_PATH * sizeof(char));
	::GetPrivateProfileString("Users", name,"",pString,_MAX_PATH,csNSCPini);
#endif

	XP_FREE(name);

	// The string has to be freed by the caller
	return pString;
}

void login_CopyStarterFiles(const char * dst, CWnd *pParent)
{
	CString csDst = dst;
	CString csSrc;
	char aPath[_MAX_PATH];
	CUpdateFileDlg * pDlg = new CUpdateFileDlg(pParent,TRUE);

	::GetModuleFileName(theApp.m_hInstance, aPath, _MAX_PATH);

	//	Find the trailing slash.
	char *pSlash = ::strrchr(aPath, '\\');
	if(pSlash)	{
		*pSlash = '\0';
	}
	else	{
		aPath[0] = '\0';
	}
	if (!(aPath && aPath[0])) return;
	if (!dst) return;
	if (!pDlg) return;

	csSrc = aPath;
	csSrc += "\\defaults";
	csDst = dst;
	WFEU_UpdaterCopyDirectory(csSrc,csDst,pDlg,"Default Files");

	// PE doesn't have a parent window to attach pDlg to.
	if (!pParent)         // For now, this will be PE specific.  JonM will decide if it's always...
		delete pDlg;
}

int     login_UpdateFilesToNewLocation(const char * path,CWnd *pParent,BOOL bCopyDontMove)
{
	CString csTmp;
	
	if (!path) return FALSE;

	CUpdateFileDlg * pDlg = new CUpdateFileDlg(pParent,bCopyDontMove);

	CString csMain = theApp.GetProfileString("Main","Install Directory","");

	CString csBookmarks = theApp.GetProfileString("Bookmark List","File Location","");
	if (!csBookmarks.IsEmpty()) {
		csTmp = path;
		csTmp += "\\bookmark.htm";
		pDlg->StartFileUpdate("General Files","bookmark.htm");
		if (bCopyDontMove) {
			WFE_CopyFile(csBookmarks,csTmp);
		} else {
			if (WFE_MoveFile(csBookmarks,csTmp))
				theApp.WriteProfileString("Bookmark List","File Location",csTmp);
		}
	}

	CString csABook = theApp.GetProfileString("Address Book","File Location","");
	if (!csABook.IsEmpty()) {
		csTmp = path;
		csTmp += "\\address.htm";
		pDlg->StartFileUpdate("General Files","address.htm");
		if (bCopyDontMove) {
			WFE_CopyFile(csABook,csTmp);
		} else {
			if (WFE_MoveFile(csABook,csTmp))
				theApp.WriteProfileString("Address Book","File Location",csTmp);
		}
	} else {
		if (!csMain.IsEmpty()) {
			csTmp = path;
			csTmp += "\\address.htm";
			pDlg->StartFileUpdate("General Files","address.htm");
			WFE_CopyFile(csMain + "\\address.htm" ,csTmp);
		}
	}

	CString csHist = theApp.GetProfileString("History","History File","");
	CString csFilename = CString(XP_AppName) + ".hst";
	if (!csHist.IsEmpty()) {
		csTmp = path;
		csTmp += CString("\\") + csFilename;
		pDlg->StartFileUpdate("General Files", (char*)(const char*)csFilename);
		if (bCopyDontMove) {
			WFE_CopyFile(csHist,csTmp);
		} else {
			if (WFE_MoveFile(csHist,csTmp))
				theApp.WriteProfileString("History","History File",csTmp);
		}
	} else {
		if (!csMain.IsEmpty()) {
			csTmp = path;
			csTmp += CString("\\") + csFilename;
			pDlg->StartFileUpdate("General Files",(char*)(const char*)csFilename);
			WFE_CopyFile(csMain + "\\mozilla.hst" ,csTmp);
		}
	}

	CString csNewsRC = theApp.GetProfileString("Main","News RC","");
	if (!csNewsRC.IsEmpty()) {
		csTmp = path;
		csTmp += "\\Newsrc";
		pDlg->StartFileUpdate("General Files","Newsrc");
		if (bCopyDontMove) {
			WFE_CopyFile(csNewsRC,csTmp);
		} else {
			if (WFE_MoveFile(csNewsRC,csTmp))
				theApp.WriteProfileString("Main","News RC",csTmp);
		}
	} 

	if (!csMain.IsEmpty()) {
		// these files are always copied since we don't store pointers
		csTmp = path;
		csTmp += "\\socks.cnf";
		pDlg->StartFileUpdate("General Files","socks.cnf");
		WFE_CopyFile(csMain + "\\socks.cnf" ,csTmp);

		csTmp = path;
		csTmp += "\\cookies.txt";
		pDlg->StartFileUpdate("General Files","cookies.txt");
		WFE_CopyFile(csMain + "\\cookies.txt",csTmp);

#if defined(CookieManagement)
		csTmp = path;
		csTmp += "\\cookperm.txt";
		pDlg->StartFileUpdate("General Files","cookperm.txt");
		WFE_CopyFile(csMain + "\\cookperm.txt",csTmp);
#endif

#if defined(SingleSignon)
		csTmp = path;
		csTmp += "\\signons.txt";
		pDlg->StartFileUpdate("General Files","signons.txt");
		WFE_CopyFile(csMain + "\\signons.txt",csTmp);
#endif

		csTmp = path;
		csTmp += "\\key.db";
		pDlg->StartFileUpdate("Security Files","key.db");
		WFE_CopyFile(csMain + "\\key.db",csTmp);

		csTmp = path;
		csTmp += "\\cert5.db";
		pDlg->StartFileUpdate("Security Files","cert5.db");
		WFE_CopyFile(csMain + "\\cert5.db",csTmp);

		csTmp = path;
		csTmp += "\\certni.db";
		pDlg->StartFileUpdate("Security Files","certni.db");
		WFE_CopyFile(csMain + "\\certni.db",csTmp);

		csTmp = path;
		csTmp += "\\proxy.cfg";
		pDlg->StartFileUpdate("Network Files","proxy.cfg");
		WFE_CopyFile(csMain + "\\proxy.cfg",csTmp);

		csTmp = path;
		csTmp += "\\abook.nab";
		if (bCopyDontMove)
			WFE_CopyFile(csMain + "\\abook.nab",csTmp);
		else
			WFE_MoveFile(csMain + "\\abook.nab",csTmp);
	}
	csTmp = path;
	csTmp += "\\Mail";
	CString csMail = theApp.GetProfileString("Mail","Mail Directory","");

	if (!strnicmp(csMail,csTmp,strlen(csMail))) {
		char szMsg[256];
		_snprintf(szMsg, 256, szLoadString(IDS_UNABLETRANSFER_SUBDIR),"Mail Directory");
		AfxMessageBox(szMsg);
	}
	else {
		if (bCopyDontMove) {
			if (!csMail.IsEmpty()) {
				WFEU_UpdaterCopyDirectory(csMail,csTmp,pDlg,"Mail Directory");
			}
		} else {
			if ((!csMail.IsEmpty()) && WFEU_UpdaterMoveDirectory(csMail,csTmp,pDlg,"Mail Directory")) {
				theApp.WriteProfileString("Mail","Mail Directory",csTmp);
				CString csFCC = theApp.GetProfileString("Mail","Default Fcc","");
				int iSlash = csFCC.ReverseFind('\\');
				if (iSlash != -1) {
					CString csFolder = csFCC.Right(csFCC.GetLength()-iSlash);
					theApp.WriteProfileString("Mail","Default Fcc",csTmp+csFolder);
				}
			}
		}
	}

	csTmp = path;
	csTmp += "\\News";
	CString csNews = theApp.GetProfileString("News","News Directory","");

	if (!strnicmp(csNews,csTmp,strlen(csNews))) {
		char szMsg[256];
		_snprintf(szMsg, 256, szLoadString(IDS_UNABLETRANSFER_SUBDIR),"News Directory");
		AfxMessageBox(szMsg);
	}
	else {
		if (bCopyDontMove) {
			if (!csNews.IsEmpty()) {
				WFEU_UpdaterCopyDirectory(csNews,csTmp,pDlg,"News Directory");
			}
		} else {
			if ((!csNews.IsEmpty()) && WFEU_UpdaterMoveDirectory(csNews,csTmp,pDlg,"News Directory"))
				theApp.WriteProfileString("News","News Directory",csTmp);               
		}
		// update news rc file either way to new directory -- creates fat,new
		int iRet = WFEU_UpdateNewsFatFile(csTmp,csTmp);
		// now move fat.new over the old fat file
		if (iRet) {
			CString csOldFat = csTmp + "\\fat";
			CString csNewFat = csTmp + "\\fat.new";
			rename(csOldFat,csTmp + "\\fat.old");
			rename(csNewFat,csTmp + "\\fat");
		}
	}

	csTmp = path;
	csTmp += "\\cache";
	CString csCache = theApp.GetProfileString("Cache","Cache Dir","");

	if (!strnicmp(csCache,csTmp,strlen(csCache))) {
		char szMsg[256];
		_snprintf(szMsg, 256, szLoadString(IDS_UNABLETRANSFER_SUBDIR),"Cache Directory");
		AfxMessageBox(szMsg);
	}
	else {
		if (bCopyDontMove) {
			if (!csCache.IsEmpty()) {
				WFEU_UpdaterCopyDirectory(csCache,csTmp,pDlg,"Cache Directory");
			}
		} else {
			if ((!csCache.IsEmpty()) && WFEU_UpdaterMoveDirectory(csCache,csTmp,pDlg,"Cache Directory"))
				theApp.WriteProfileString("Cache","Cache Dir",csTmp);   
		}
	}
	return TRUE;
}

// CUpdateFileDlg dialog


CUpdateFileDlg::CUpdateFileDlg(CWnd *pParent, BOOL bCopyDontMove)
	: CDialog(CUpdateFileDlg::IDD, pParent),
      m_pParent(pParent)
{
	//{{AFX_DATA_INIT(CSaveFileDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
    
    // Suppress user interaction while we are active!
    if(pParent){ 
	pParent->EnableWindow(FALSE);
    }
    
    // For simplicity, create right here
    if (!CDialog::Create(CUpdateFileDlg::IDD, pParent))
    {
	TRACE0("Warning: creation of CUpdateFileDlg dialog failed\n");
	return;
    }

	m_bCopyDontMove = bCopyDontMove;

    // Clear our place-holder string
    GetDlgItem(IDC_CATEGORY)->SetWindowText("");
	GetDlgItem(IDC_TEXT1)->SetWindowText("Note:  If you have large mail or news folders, some of these operations may take a while.  Please be patient.");
    // Why doesn't this center on parent window automatically!
    CRect cRectParent;
    CRect cRectDlg;
    if( pParent ){
	pParent->GetWindowRect(&cRectParent);
	GetWindowRect(&cRectDlg);
	int iTop = 200;
	int iLeft = 200;
	if ( cRectParent.Height() > cRectDlg.Height() ){
	    iTop = cRectParent.top + ( (cRectParent.Height() - cRectDlg.Height()) / 2 );
	}
	if ( cRectParent.Width() > cRectDlg.Width() ){
	    iLeft = cRectParent.left + ( (cRectParent.Width() - cRectDlg.Width()) / 2 );
	}
	SetWindowPos(&wndTopMost, iLeft, iTop, 0, 0, SWP_NOSIZE);
    }
	ShowWindow(SW_SHOW);
}

void CUpdateFileDlg::StartFileUpdate(char *category,char * pFilename)
{
	GetDlgItem(IDC_CATEGORY)->SetWindowText(category);
	CString csTmp;
	
	if (m_bCopyDontMove)
		csTmp = "Copying File: ";
	else
		csTmp = "Moving File: ";
	csTmp+=pFilename;
    GetDlgItem(IDC_FILENAME_AREA)->SetWindowText(csTmp);
    // Report file number if we have more than 1 total
}

void CUpdateFileDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSaveFileDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CUpdateFileDlg, CDialog)
	//{{AFX_MSG_MAP(CSaveFileDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CUpdateFileDlg::PostNcDestroy() 
{
	CDialog::PostNcDestroy();
    if( m_pParent && ::IsWindow(m_pParent->m_hWnd) ){
	m_pParent->EnableWindow(TRUE);
	// Return focus to parent window
	m_pParent->SetActiveWindow();
	m_pParent->SetFocus();
    }
}

#ifdef XP_WIN16
void login_GetIniFilePath(CString &csNSCPini)
{
	// give a chance for win.ini to override the nscp.ini location
	auto char ca_iniBuff[_MAX_PATH];
    ::GetProfileString("netscape", "nscpINI", "", ca_iniBuff, _MAX_PATH);
	if (ca_iniBuff[0]) {
		csNSCPini = ca_iniBuff;
		return;
	}

	// otherwise fallback on the default
    GetWindowsDirectory(ca_iniBuff, _MAX_PATH);
	if (ca_iniBuff[strlen(ca_iniBuff)-1] == '\\')
		ca_iniBuff[strlen(ca_iniBuff)-1] = 0;

	csNSCPini = ca_iniBuff;
	csNSCPini += "\\nscp.ini";
}
#endif
