// PrintSetupDialog.cpp : implementation file
//

#include "stdafx.h"
#include "mfcembed.h"
#include "PrintSetupDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "nsPrintSettingsImpl.h"

/////////////////////////////////////////////////////////////////////////////
// CPrintSetupDialog dialog


CPrintSetupDialog::CPrintSetupDialog(nsIPrintSettings* aPrintSettings, CWnd* pParent /*=NULL*/)
	: CDialog(CPrintSetupDialog::IDD, pParent),
  m_PrintSettings(aPrintSettings)
{
	//{{AFX_DATA_INIT(CPrintSetupDialog)
	m_BottomMargin = _T("");
	m_LeftMargin = _T("");
	m_RightMargin = _T("");
	m_TopMargin = _T("");
	m_Scaling = 0;
	m_PrintBGImages = FALSE;
	m_PrintBGColors = FALSE;
	//}}AFX_DATA_INIT

  SetPrintSettings(m_PrintSettings);
	
}


void CPrintSetupDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrintSetupDialog)
	DDX_Text(pDX, IDC_BOTTOM_MARGIN_TXT, m_BottomMargin);
	DDX_Text(pDX, IDC_LEFT_MARGIN_TXT, m_LeftMargin);
	DDX_Text(pDX, IDC_RIGHT_MARGIN_TXT, m_RightMargin);
	DDX_Text(pDX, IDC_TOP_MARGIN_TXT, m_TopMargin);
	DDX_Slider(pDX, IDC_SCALE, m_Scaling);
	DDX_Check(pDX, IDC_PRT_BGIMAGES, m_PrintBGImages);
	DDX_Check(pDX, IDC_PRT_BGCOLORS, m_PrintBGColors);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPrintSetupDialog, CDialog)
	//{{AFX_MSG_MAP(CPrintSetupDialog)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SCALE, OnCustomdrawScale)
	ON_EN_KILLFOCUS(IDC_SCALE_TXT, OnKillfocusScaleTxt)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPrintSetupDialog message handlers
void CPrintSetupDialog::SetPrintSettings(nsIPrintSettings* aPrintSettings) 
{ 
  if (m_PrintSettings != NULL) {
    m_PrintSettings = aPrintSettings;
    double top, left, right, bottom;
    m_PrintSettings->GetMarginTop(&top);
    m_PrintSettings->GetMarginLeft(&left);
    m_PrintSettings->GetMarginRight(&right);
    m_PrintSettings->GetMarginBottom(&bottom);

    char buf[16];
    sprintf(buf, "%5.2f", top);
    m_TopMargin = buf;
    sprintf(buf, "%5.2f", left);
    m_LeftMargin = buf;
    sprintf(buf, "%5.2f", right);
    m_RightMargin = buf;
    sprintf(buf, "%5.2f", bottom);
    m_BottomMargin = buf;

    double scaling;
    m_PrintSettings->GetScaling(&scaling);
	  m_Scaling = int(scaling * 100.0);

    PRBool boolVal;
    m_PrintSettings->GetPrintBGColors(&boolVal);
    m_PrintBGColors = boolVal == PR_TRUE;
    m_PrintSettings->GetPrintBGImages(&boolVal);
    m_PrintBGImages = boolVal == PR_TRUE;

  }
}

void CPrintSetupDialog::OnOK() 
{
	CDialog::OnOK();
}

BOOL CPrintSetupDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  CSliderCtrl* scale = (CSliderCtrl*)GetDlgItem(IDC_SCALE);
  CWnd* scaleTxt     = GetDlgItem(IDC_SCALE_TXT);
  if (scale != NULL) {
    scale->SetRange(50, 100);
    scale->SetBuddy(scaleTxt, FALSE);
    scale->SetTicFreq(10);
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPrintSetupDialog::OnCustomdrawScale(NMHDR* pNMHDR, LRESULT* pResult) 
{
  CSliderCtrl* scale = (CSliderCtrl*)GetDlgItem(IDC_SCALE);
  CWnd* scaleTxt     = GetDlgItem(IDC_SCALE_TXT);
  if (scale != NULL && scaleTxt != NULL) {
    CString str;
    str.Format("%d", scale->GetPos());
    scaleTxt->SetWindowText(str);
  }
	
	*pResult = 0;
}

static int GetIntFromStr(const char* aStr, int aMinVal = 50, int aMaxVal = 100)
{
  int val = aMinVal;
  sscanf(aStr, "%d", &val);
  if (val < aMinVal) {
    return aMinVal;

  } else if (val > aMaxVal) {
    return aMaxVal;
  }
  return val;
}

void CPrintSetupDialog::OnKillfocusScaleTxt() 
{
  CSliderCtrl* scale = (CSliderCtrl*)GetDlgItem(IDC_SCALE);
  CWnd* scaleTxt     = GetDlgItem(IDC_SCALE_TXT);
  if (scale != NULL && scaleTxt != NULL) {
    CString str;
    scaleTxt->GetWindowText(str);
    scale->SetPos(GetIntFromStr(str));
  }	
}
