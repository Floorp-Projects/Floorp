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

typedef struct {
  char*   mDesc;
  short   mUnit;
  double  mWidth;
  double  mHeight;
  BOOL    mIsUserDefined;
} PaperSizes;

static const PaperSizes gPaperSize[] = {
  {"Letter (8.5 x 11.0)", nsIPrintSettings::kPaperSizeInches, 8.5, 11.0, FALSE},
  {"Legal (8.5 x 14.0)", nsIPrintSettings::kPaperSizeInches, 8.5, 14.0, FALSE},
  {"A4 (210 x 297mm)", nsIPrintSettings::kPaperSizeMillimeters, 210.0, 297.0, FALSE},
  {"User Defined", nsIPrintSettings::kPaperSizeInches, 8.5, 11.0, TRUE}
};
static const int gNumPaperSizes = 4;


/////////////////////////////////////////////////////////////////////////////
// CPrintSetupDialog dialog


CPrintSetupDialog::CPrintSetupDialog(nsIPrintSettings* aPrintSettings, CWnd* pParent /*=NULL*/)
	: CDialog(CPrintSetupDialog::IDD, pParent),
  m_PrintSettings(aPrintSettings),
  m_PaperSizeInx(0)
{
	//{{AFX_DATA_INIT(CPrintSetupDialog)
	m_BottomMargin = _T("");
	m_LeftMargin = _T("");
	m_RightMargin = _T("");
	m_TopMargin = _T("");
	m_Scaling = 0;
	m_PrintBGImages = FALSE;
	m_PrintBGColors = FALSE;
	m_PaperSize = _T("");
	m_PaperHeight = 0.0;
	m_PaperWidth = 0.0;
	m_IsInches = -1;
	m_FooterLeft = _T("");
	m_FooterMiddle = _T("");
	m_FooterRight = _T("");
	m_HeaderLeft = _T("");
	m_HeaderMiddle = _T("");
	m_HeaderRight = _T("");
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
	DDX_CBString(pDX, IDC_PAPER_SIZE_CBX, m_PaperSize);
	DDX_Text(pDX, IDC_UD_PAPER_HGT, m_PaperHeight);
	DDX_Text(pDX, IDC_UD_PAPER_WDTH, m_PaperWidth);
	DDX_Radio(pDX, IDC_INCHES_RD, m_IsInches);
	DDX_Text(pDX, IDC_FTR_LEFT_TXT, m_FooterLeft);
	DDX_Text(pDX, IDC_FTR_MID_TXT, m_FooterMiddle);
	DDX_Text(pDX, IDC_FTR_RIGHT_TXT, m_FooterRight);
	DDX_Text(pDX, IDC_HDR_LEFT_TXT, m_HeaderLeft);
	DDX_Text(pDX, IDC_HDR_MID_TXT, m_HeaderMiddle);
	DDX_Text(pDX, IDC_HDR_RIGHT_TXT, m_HeaderRight);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPrintSetupDialog, CDialog)
	//{{AFX_MSG_MAP(CPrintSetupDialog)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SCALE, OnCustomdrawScale)
	ON_EN_KILLFOCUS(IDC_SCALE_TXT, OnKillfocusScaleTxt)
	ON_CBN_SELCHANGE(IDC_PAPER_SIZE_CBX, OnSelchangePaperSizeCbx)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPrintSetupDialog message handlers
void CPrintSetupDialog::SetPrintSettings(nsIPrintSettings* aPrintSettings) 
{ 
  if (aPrintSettings != NULL) {
    double top, left, right, bottom;
    aPrintSettings->GetMarginTop(&top);
    aPrintSettings->GetMarginLeft(&left);
    aPrintSettings->GetMarginRight(&right);
    aPrintSettings->GetMarginBottom(&bottom);

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
    aPrintSettings->GetScaling(&scaling);
	  m_Scaling = int(scaling * 100.0);

    PRBool boolVal;
    aPrintSettings->GetPrintBGColors(&boolVal);
    m_PrintBGColors = boolVal == PR_TRUE;
    aPrintSettings->GetPrintBGImages(&boolVal);
    m_PrintBGImages = boolVal == PR_TRUE;

    PRUnichar* uStr;
    aPrintSettings->GetHeaderStrLeft(&uStr);
		m_HeaderLeft = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetHeaderStrCenter(&uStr);
		m_HeaderMiddle = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetHeaderStrRight(&uStr);
		m_HeaderRight = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrLeft(&uStr);
		m_FooterLeft = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrCenter(&uStr);
		m_FooterMiddle = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrRight(&uStr);
		m_FooterRight = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

  }
}

void CPrintSetupDialog::OnOK() 
{
	CDialog::OnOK();
}

// Search for Sizes in Pape Size Data
int CPrintSetupDialog::GetPaperSizeIndexFromData(short aUnit, double aW, double aH) 
{
  for (int i=0;i<gNumPaperSizes;i++) {
    if (gPaperSize[i].mUnit == aUnit && 
        gPaperSize[i].mWidth == aW &&
        gPaperSize[i].mHeight == aH) {
      return i;
    }
  }

  // find the first user defined
  for ( i=0;i<gNumPaperSizes;i++) {
    if (gPaperSize[i].mIsUserDefined) {
      return i;
    }
  }

  return -1;
}

int CPrintSetupDialog::GetPaperSizeIndex(const CString& aStr) 
{
  for (int i=0;i<gNumPaperSizes;i++) {
    if (!aStr.Compare(gPaperSize[i].mDesc)) {
      return i;
    }
  }
  return -1;
}

void CPrintSetupDialog::GetPaperSizeInfo(short& aUnit, double& aWidth, double& aHeight)
{
  if (gPaperSize[m_PaperSizeInx].mIsUserDefined) {
    aUnit   = m_IsInches == 0?nsIPrintSettings::kPaperSizeInches:nsIPrintSettings::kPaperSizeMillimeters;
    aWidth  = m_PaperWidth;
    aHeight = m_PaperHeight;
  } else {
    aUnit   = gPaperSize[m_PaperSizeInx].mUnit;
    aWidth  = gPaperSize[m_PaperSizeInx].mWidth;
    aHeight = gPaperSize[m_PaperSizeInx].mHeight;
  }

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

	CComboBox* cbx = (CComboBox*)GetDlgItem(IDC_PAPER_SIZE_CBX);
  if (cbx != NULL) {
    // First Initialize the Combobox
    for (int i=0;i<gNumPaperSizes;i++) {
      cbx->AddString(gPaperSize[i].mDesc);
    }
    short  unit;
    double paperWidth  = 0.0;
    double paperHeight = 0.0;
    m_PrintSettings->GetPaperSizeType(&unit);
    m_PrintSettings->GetPaperWidth(&paperWidth);
    m_PrintSettings->GetPaperHeight(&paperHeight);

    m_PaperSizeInx = GetPaperSizeIndexFromData(unit, paperWidth, paperHeight);
    if (m_PaperSizeInx == -1) { // couldn't find a match
      m_PaperSizeInx = 0;
      unit        = gPaperSize[m_PaperSizeInx].mUnit;
      paperWidth  = gPaperSize[m_PaperSizeInx].mWidth;
      paperHeight = gPaperSize[m_PaperSizeInx].mHeight;
    }

    cbx->SetCurSel(m_PaperSizeInx);

    EnableUserDefineControls(gPaperSize[m_PaperSizeInx].mIsUserDefined);

    if (gPaperSize[m_PaperSizeInx].mIsUserDefined) {
      CString wStr;
      CString hStr;
      if (unit == nsIPrintSettings::kPaperSizeInches) {
        wStr.Format("%6.2f", paperWidth);
        hStr.Format("%6.2f", paperHeight);
        CheckRadioButton(IDC_INCHES_RD, IDC_MILLI_RD, IDC_INCHES_RD);
      } else {
        wStr.Format("%d", int(paperWidth));
        hStr.Format("%d", int(paperHeight));
        CheckRadioButton(IDC_INCHES_RD, IDC_MILLI_RD, IDC_MILLI_RD);
      }
	    CWnd* widthTxt  = GetDlgItem(IDC_UD_PAPER_WDTH);
	    CWnd* heightTxt = GetDlgItem(IDC_UD_PAPER_HGT);
      widthTxt->SetWindowText(wStr);
      heightTxt->SetWindowText(hStr);
    } else {
      CheckRadioButton(IDC_INCHES_RD, IDC_MILLI_RD, IDC_INCHES_RD);
    }
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


void CPrintSetupDialog::EnableUserDefineControls(BOOL aEnable) 
{
	CWnd* cntrl = GetDlgItem(IDC_UD_WIDTH_LBL);
  cntrl->EnableWindow(aEnable);
	cntrl = GetDlgItem(IDC_UD_HEIGHT_LBL);
  cntrl->EnableWindow(aEnable);
	cntrl = GetDlgItem(IDC_UD_PAPER_WDTH);
  cntrl->EnableWindow(aEnable);
	cntrl = GetDlgItem(IDC_UD_PAPER_HGT);
  cntrl->EnableWindow(aEnable);
	cntrl = GetDlgItem(IDC_INCHES_RD);
  cntrl->EnableWindow(aEnable);
	cntrl = GetDlgItem(IDC_MILLI_RD);
  cntrl->EnableWindow(aEnable);
}

void CPrintSetupDialog::OnSelchangePaperSizeCbx() 
{
	CComboBox* cbx = (CComboBox*)GetDlgItem(IDC_PAPER_SIZE_CBX);
  if (cbx) {
    CString text;
    cbx->GetWindowText(text);
    m_PaperSizeInx = GetPaperSizeIndex(text);
    EnableUserDefineControls(gPaperSize[m_PaperSizeInx].mIsUserDefined);
  }
	
}
