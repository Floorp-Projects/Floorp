// MarginHeaderFooter.cpp : implementation file
//

#include "stdafx.h"
#include "mfcembed.h"
#include "CMarginHeaderFooter.h"
#include "CCustomPromptDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static TCHAR* sCBXTitles[] = {
    _T("--Blank--"), _T("Title"), _T("URL"), _T("Date/Time"), _T("Page #"),
    _T("Page # of #"), _T("Custom..."), NULL};
static TCHAR* sCBXValues[] = {
    _T(""), _T("&T"), _T("&U"), _T("&D"), _T("&P"), _T("&PT"), _T(""), NULL};

/////////////////////////////////////////////////////////////////////////////
// CMarginHeaderFooter property page

IMPLEMENT_DYNCREATE(CMarginHeaderFooter, CPropertyPage)

CMarginHeaderFooter::CMarginHeaderFooter() : CPropertyPage(CMarginHeaderFooter::IDD)
{
	  //{{AFX_DATA_INIT(CMarginHeaderFooter)
	  m_BottomMarginText = _T("");
	  m_LeftMarginText = _T("");
	  m_RightMarginText = _T("");
	  m_TopMarginText = _T("");
	  //}}AFX_DATA_INIT

	  m_FooterLeftText = _T("");
	  m_FooterCenterText = _T("");
	  m_FooterRightText = _T("");
	  m_HeaderLeftText = _T("");
	  m_HeaderCenterText = _T("");
	  m_HeaderRightText = _T("");
}

CMarginHeaderFooter::~CMarginHeaderFooter()
{
}

void CMarginHeaderFooter::DoDataExchange(CDataExchange* pDX)
{
	  CPropertyPage::DoDataExchange(pDX);
	  //{{AFX_DATA_MAP(CMarginHeaderFooter)
	  DDX_Text(pDX, IDC_BOTTOM_MARGIN_TXT, m_BottomMarginText);
	  DDX_Text(pDX, IDC_LEFT_MARGIN_TXT, m_LeftMarginText);
	  DDX_Text(pDX, IDC_RIGHT_MARGIN_TXT, m_RightMarginText);
	  DDX_Text(pDX, IDC_TOP_MARGIN_TXT, m_TopMarginText);
	  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMarginHeaderFooter, CPropertyPage)
	//{{AFX_MSG_MAP(CMarginHeaderFooter)
	ON_CBN_SELCHANGE(IDC_FTR_LEFT_CMBX, OnEditchangeFTRLeft)
	ON_CBN_SELCHANGE(IDC_FTR_CENTER_CMBX, OnEditchangeFTRCenter)
	ON_CBN_SELCHANGE(IDC_FTR_RIGHT_CMBX, OnEditchangeFTRRight)
	ON_CBN_SELCHANGE(IDC_HDR_LEFT_CMBX, OnEditchangeHDRLeft)
	ON_CBN_SELCHANGE(IDC_HDR_CENTER_CMBX, OnEditchangeHDRCenter)
	ON_CBN_SELCHANGE(IDC_HDR_RIGHT_CMBX, OnEditchangeHDRRight)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMarginHeaderFooter message handlers

void CMarginHeaderFooter::SetComboboxValue(int aId, const TCHAR *aItem)
{
    CComboBox* cbx = (CComboBox*)GetDlgItem(aId);
    if (cbx) 
    {
        int inx = 0;
        while (sCBXValues[inx] != NULL) 
        {
            if (!_tcscmp(sCBXValues[inx], aItem)) 
            {
                cbx->SetCurSel(inx);
                return;
            }
            inx++;
        }
        cbx->SetCurSel(inx-1);
    }
}

void CMarginHeaderFooter::AddCBXItem(int aId, const TCHAR * aItem)
{
    CComboBox* cbx = (CComboBox*)GetDlgItem(aId);
    if (cbx) 
    {
        cbx->AddString(aItem);
    }
}

BOOL CMarginHeaderFooter::OnInitDialog() 
{
	CDialog::OnInitDialog();

    int inx = 0;
    while (sCBXTitles[inx] != NULL) 
    {
        AddCBXItem(IDC_HDR_LEFT_CMBX,   sCBXTitles[inx]);
        AddCBXItem(IDC_HDR_CENTER_CMBX, sCBXTitles[inx]);
        AddCBXItem(IDC_HDR_RIGHT_CMBX,  sCBXTitles[inx]);
        AddCBXItem(IDC_FTR_LEFT_CMBX,   sCBXTitles[inx]);
        AddCBXItem(IDC_FTR_CENTER_CMBX, sCBXTitles[inx]);
        AddCBXItem(IDC_FTR_RIGHT_CMBX,  sCBXTitles[inx]);
        inx++;
    }
	  SetComboboxValue(IDC_HDR_LEFT_CMBX, m_HeaderLeftText);
	  SetComboboxValue(IDC_HDR_CENTER_CMBX, m_HeaderCenterText);
	  SetComboboxValue(IDC_HDR_RIGHT_CMBX, m_HeaderRightText);
	  SetComboboxValue(IDC_FTR_LEFT_CMBX, m_FooterLeftText);
	  SetComboboxValue(IDC_FTR_CENTER_CMBX, m_FooterCenterText);
	  SetComboboxValue(IDC_FTR_RIGHT_CMBX, m_FooterRightText);

	  return TRUE;  // return TRUE unless you set the focus to a control
	                // EXCEPTION: OCX Property Pages should return FALSE
}

void CMarginHeaderFooter::SetCombobox(int aId, CString& aText) 
{
    CComboBox* cbx = (CComboBox*)GetDlgItem(aId);
	  int inx = cbx->GetCurSel();
    if (inx == 6) 
    {
        CCustomPromptDialog prompt(this);
        prompt.m_CustomText = aText;
        if(prompt.DoModal() == IDOK)
        {
            aText = prompt.m_CustomText;
        }
    } else {
      aText = sCBXValues[inx];
    }
}

void CMarginHeaderFooter::OnEditchangeFTRLeft() 
{
    SetCombobox(IDC_FTR_LEFT_CMBX, m_FooterLeftText);
}

void CMarginHeaderFooter::OnEditchangeFTRCenter() 
{
    SetCombobox(IDC_FTR_CENTER_CMBX, m_FooterCenterText);
}

void CMarginHeaderFooter::OnEditchangeFTRRight() 
{
    SetCombobox(IDC_FTR_CENTER_CMBX, m_FooterCenterText);
}

void CMarginHeaderFooter::OnEditchangeHDRLeft() 
{
    SetCombobox(IDC_HDR_LEFT_CMBX, m_HeaderLeftText);
}

void CMarginHeaderFooter::OnEditchangeHDRCenter() 
{
    SetCombobox(IDC_HDR_CENTER_CMBX, m_HeaderCenterText);
}

void CMarginHeaderFooter::OnEditchangeHDRRight() 
{
    SetCombobox(IDC_HDR_CENTER_CMBX, m_HeaderCenterText);
}
