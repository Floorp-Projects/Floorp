// DlgEditPrefStr.cpp : implementation file
//

#include "stdafx.h"
#include "DlgEditPrefStr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgEditPrefStr dialog


CDlgEditPrefStr::CDlgEditPrefStr(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgEditPrefStr::IDD, pParent), m_bChoose(FALSE)
{
	//{{AFX_DATA_INIT(CDlgEditPrefStr)
	m_strDescription = _T("");
	m_strPrefName = _T("");
	m_strValue = _T("");
	m_bLocked = FALSE;
	m_bValue = FALSE;
	m_bRemoteAdmin = FALSE;
	m_bLockable = TRUE;
	//}}AFX_DATA_INIT
}


void CDlgEditPrefStr::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgEditPrefStr)
	DDX_Control(pDX, IDC_VALLIST, m_listValue);
	DDX_Control(pDX, IDC_VALCHECK, m_checkValue);
	DDX_Control(pDX, IDC_VALUE, m_editValue);
	DDX_Control(pDX, IDC_LOCKED, m_checkLocked);
	DDX_Control(pDX, IDC_REMOTEADMIN, m_checkRemoteAdmin);
	DDX_Check(pDX, IDC_REMOTEADMIN, m_bRemoteAdmin);
	DDX_Text(pDX, IDC_DESCRIPTION, m_strDescription);
	DDX_Text(pDX, IDC_PREFNAME, m_strPrefName);
	DDX_Text(pDX, IDC_VALUE, m_strValue);
	DDX_Check(pDX, IDC_LOCKED, m_bLocked);
	DDX_Check(pDX, IDC_VALCHECK, m_bValue);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgEditPrefStr, CDialog)
	//{{AFX_MSG_MAP(CDlgEditPrefStr)
	ON_WM_CREATE()
	ON_WM_CANCELMODE()
	ON_BN_CLICKED(IDC_DEFAULT, OnResetDefault)
	ON_BN_CLICKED(IDC_REMOTEADMIN, OnRemoteAdmin)
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgEditPrefStr message handlers

int CDlgEditPrefStr::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	SetWindowText(m_strTitle);
	return 0;
}


BOOL CDlgEditPrefStr::PreCreateWindow(CREATESTRUCT& cs) 
{
  BOOL retval = CDialog::PreCreateWindow(cs);

	return retval;
}

BOOL CDlgEditPrefStr::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  if (m_bChoose)
  {
    m_editValue.ShowWindow(SW_HIDE);
    m_listValue.ShowWindow(SW_SHOW);
    m_checkValue.ShowWindow(SW_HIDE);

    // The list of choices comes in as an array of CStrings, 
    // terminated with an empty one.
    CString *pstrChoice = m_pstrChoices;
    while(pstrChoice->GetLength() > 0)
    {
      m_listValue.AddString(*pstrChoice);
      pstrChoice++;
    }

    m_listValue.SelectString(0, m_strSelectedChoice);
  }

  else if (m_strType.CompareNoCase("bool") == 0)
  {
    m_editValue.ShowWindow(SW_HIDE);
    m_listValue.ShowWindow(SW_HIDE);
    m_checkValue.ShowWindow(SW_SHOW);
    m_checkValue.SetWindowText(m_strTitle);
    
    if (m_strValue.CompareNoCase("true") == 0)
      m_checkValue.SetCheck(TRUE);
  }	

  else // string or int type
  {
    m_editValue.ShowWindow(SW_SHOW);
    m_listValue.ShowWindow(SW_HIDE);
    m_checkValue.ShowWindow(SW_HIDE);
  }

  // disable lock checkbox if pref is not lockable
  
  if (!m_bLockable)
  {
    m_checkLocked.EnableWindow(FALSE);
  }
    

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// Caller only has to get value from m_strValue for all types.
void CDlgEditPrefStr::OnOK() 
{
  // For bool types, convert back to the data string.
  if (m_strType.CompareNoCase("bool") == 0)
  {
    if (m_checkValue.GetCheck())
      m_editValue.SetWindowText("true");
    else
      m_editValue.SetWindowText("false");
  }	  
  // For choose, return the selected list string.
  else if (m_bChoose)
  {
    CString strSelectString;
    m_listValue.GetLBText(m_listValue.GetCurSel(), strSelectString);
    m_editValue.SetWindowText(strSelectString);
  }
	
  else // string, int type
  {
    
  }

	CDialog::OnOK();
}

void CDlgEditPrefStr::OnResetDefault() 
{
  if (m_bChoose)
  {
    m_listValue.SelectString(0, m_strDefault);
  }

  else if (m_strType.CompareNoCase("bool") == 0)
  {
    if (m_strDefault.CompareNoCase("true") == 0)
      m_checkValue.SetCheck(TRUE);
  }	

  else // string or int type
  {
    m_editValue.SetWindowText(m_strDefault);
  }
}


void CDlgEditPrefStr::OnRemoteAdmin()
{
  if (m_bLockable)
  {
    if (m_checkRemoteAdmin.GetCheck())
    {
      m_checkLocked.SetCheck(1);
      m_checkLocked.EnableWindow(FALSE);
    }
    else if (m_bLockable)
    {
      m_checkLocked.SetCheck(0);
      m_checkLocked.EnableWindow(TRUE);
    }
  }
}
