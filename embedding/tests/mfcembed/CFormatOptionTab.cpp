// FormatOptionTab.cpp : implementation file
//

#include "stdafx.h"
#include "mfcembed.h"
#include "CFormatOptionTab.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int kMinScaleRange = 30;

/////////////////////////////////////////////////////////////////////////////
typedef struct {
  TCHAR*   mDesc;
  short   mUnit;
  double  mWidth;
  double  mHeight;
  BOOL    mIsUserDefined;
} PaperSizes;

static const PaperSizes gPaperSize[] = {
  {_T("Letter (8.5 x 11.0)"), nsIPrintSettings::kPaperSizeInches, 8.5, 11.0, FALSE},
  {_T("Legal (8.5 x 14.0)"),  nsIPrintSettings::kPaperSizeInches, 8.5, 14.0, FALSE},
  {_T("A4 (210 x 297mm)"),    nsIPrintSettings::kPaperSizeMillimeters, 210.0, 297.0, FALSE},
  {_T("User Defined"),        nsIPrintSettings::kPaperSizeInches, 8.5, 11.0, TRUE}
};
static const int gNumPaperSizes = 4;


/////////////////////////////////////////////////////////////////////////////
// CFormatOptionTab property page

IMPLEMENT_DYNCREATE(CFormatOptionTab, CPropertyPage)

CFormatOptionTab::CFormatOptionTab() : CPropertyPage(CFormatOptionTab::IDD)
{
	  //{{AFX_DATA_INIT(CFormatOptionTab)
	  m_PaperSize = -1;
	  m_BGColors = FALSE;
	  m_Scaling = 0;
	  m_ScalingText = _T("");
	  m_PaperHeight = 0.0;
	  m_PaperWidth = 0.0;
	  m_DoInches = -1;
	  m_BGImages = FALSE;
	  m_IsLandScape = FALSE;
	  //}}AFX_DATA_INIT
}

CFormatOptionTab::~CFormatOptionTab()
{
}

void CFormatOptionTab::DoDataExchange(CDataExchange* pDX)
{
	  CPropertyPage::DoDataExchange(pDX);
	  //{{AFX_DATA_MAP(CFormatOptionTab)
	  DDX_CBIndex(pDX, IDC_PAPER_SIZE_CBX, m_PaperSize);
	  DDX_Check(pDX, IDC_PRT_BGCOLORS, m_BGColors);
	  DDX_Slider(pDX, IDC_SCALE, m_Scaling);
	  DDX_Text(pDX, IDC_SCALE_TXT, m_ScalingText);
	  DDX_Text(pDX, IDC_UD_PAPER_HGT, m_PaperHeight);
	  DDX_Text(pDX, IDC_UD_PAPER_WDTH, m_PaperWidth);
	  DDX_Radio(pDX, IDC_INCHES_RD, m_DoInches);
	  DDX_Check(pDX, IDC_PRT_BGIMAGES, m_BGImages);
	  DDX_Radio(pDX, IDC_PRT_PORTRAIT_RD, m_IsLandScape);
	  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFormatOptionTab, CPropertyPage)
	//{{AFX_MSG_MAP(CFormatOptionTab)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SCALE, OnCustomdrawScale)
	ON_EN_KILLFOCUS(IDC_SCALE_TXT, OnKillfocusScaleTxt)
	ON_CBN_SELCHANGE(IDC_PAPER_SIZE_CBX, OnSelchangePaperSizeCbx)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFormatOptionTab message handlers

BOOL CFormatOptionTab::OnInitDialog() 
{
	  CDialog::OnInitDialog();
	  
    CSliderCtrl* scale = (CSliderCtrl*)GetDlgItem(IDC_SCALE);
    CWnd* scaleTxt     = GetDlgItem(IDC_SCALE_TXT);
    if (scale != NULL) 
    {
        scale->SetRange(kMinScaleRange, 100);
        scale->SetBuddy(scaleTxt, FALSE);
        scale->SetTicFreq(10);
    }

	  CComboBox* cbx = (CComboBox*)GetDlgItem(IDC_PAPER_SIZE_CBX);
    if (cbx != NULL) 
    {
        // First Initialize the Combobox
        for (int i=0;i<gNumPaperSizes;i++) 
        {
            cbx->AddString(gPaperSize[i].mDesc);
        }
        short  unit;
        double paperWidth  = 0.0;
        double paperHeight = 0.0;
        m_PrintSettings->GetPaperSizeType(&unit);
        m_PrintSettings->GetPaperWidth(&paperWidth);
        m_PrintSettings->GetPaperHeight(&paperHeight);

        m_PaperSizeInx = GetPaperSizeIndexFromData(unit, paperWidth, paperHeight);
        if (m_PaperSizeInx == -1) 
        { // couldn't find a match
            m_PaperSizeInx = 0;
            unit        = gPaperSize[m_PaperSizeInx].mUnit;
            paperWidth  = gPaperSize[m_PaperSizeInx].mWidth;
            paperHeight = gPaperSize[m_PaperSizeInx].mHeight;
        }

        cbx->SetCurSel(m_PaperSizeInx);

        EnableUserDefineControls(gPaperSize[m_PaperSizeInx].mIsUserDefined);

        if (gPaperSize[m_PaperSizeInx].mIsUserDefined) 
        {
            CString wStr;
            CString hStr;
            if (unit == nsIPrintSettings::kPaperSizeInches) 
            {
                wStr.Format(_T("%6.2f"), paperWidth);
                hStr.Format(_T("%6.2f"), paperHeight);
                CheckRadioButton(IDC_INCHES_RD, IDC_MILLI_RD, IDC_INCHES_RD);
            } 
            else 
            {
                wStr.Format(_T("%d"), int(paperWidth));
                hStr.Format(_T("%d"), int(paperHeight));
                CheckRadioButton(IDC_INCHES_RD, IDC_MILLI_RD, IDC_MILLI_RD);
            }
	          CWnd* widthTxt  = GetDlgItem(IDC_UD_PAPER_WDTH);
	          CWnd* heightTxt = GetDlgItem(IDC_UD_PAPER_HGT);
            widthTxt->SetWindowText(wStr);
            heightTxt->SetWindowText(hStr);
        } 
        else 
        {
            CheckRadioButton(IDC_INCHES_RD, IDC_MILLI_RD, IDC_INCHES_RD);
        }
    }

	  return TRUE;  // return TRUE unless you set the focus to a control
	                // EXCEPTION: OCX Property Pages should return FALSE
}

void CFormatOptionTab::OnCustomdrawScale(NMHDR* pNMHDR, LRESULT* pResult) 
{
    CSliderCtrl* scale = (CSliderCtrl*)GetDlgItem(IDC_SCALE);
    CWnd* scaleTxt     = GetDlgItem(IDC_SCALE_TXT);
    if (scale != NULL && scaleTxt != NULL) 
    {
        CString str;
        str.Format(_T("%d"), scale->GetPos());
        scaleTxt->SetWindowText(str);
    }
	  
	  *pResult = 0;
}

static int GetIntFromStr(const TCHAR * aStr, int aMinVal = kMinScaleRange, int aMaxVal = 100)
{
    int val = aMinVal;
    _stscanf(aStr, _T("%d"), &val);
    if (val < aMinVal) 
    {
        return aMinVal;
    } 
    else if (val > aMaxVal) 
    {
        return aMaxVal;
    }
    return val;
}

void CFormatOptionTab::OnKillfocusScaleTxt() 
{
    CSliderCtrl* scale = (CSliderCtrl*)GetDlgItem(IDC_SCALE);
    CWnd* scaleTxt     = GetDlgItem(IDC_SCALE_TXT);
    if (scale != NULL && scaleTxt != NULL) 
    {
        CString str;
        scaleTxt->GetWindowText(str);
        scale->SetPos(GetIntFromStr(str));
    }	
}


void CFormatOptionTab::EnableUserDefineControls(BOOL aEnable) 
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

void CFormatOptionTab::OnSelchangePaperSizeCbx() 
{
	  CComboBox* cbx = (CComboBox*)GetDlgItem(IDC_PAPER_SIZE_CBX);
    if (cbx) 
    {
        CString text;
        cbx->GetWindowText(text);
        m_PaperSizeInx = GetPaperSizeIndex(text);
        EnableUserDefineControls(gPaperSize[m_PaperSizeInx].mIsUserDefined);
    }
}

void CFormatOptionTab::GetPaperSizeInfo(short& aUnit, double& aWidth, double& aHeight)
{
    if (gPaperSize[m_PaperSizeInx].mIsUserDefined) 
    {
        aUnit   = m_DoInches == 0?nsIPrintSettings::kPaperSizeInches:nsIPrintSettings::kPaperSizeMillimeters;
        aWidth  = m_PaperWidth;
        aHeight = m_PaperHeight;
    } 
    else 
    {
        aUnit   = gPaperSize[m_PaperSizeInx].mUnit;
        aWidth  = gPaperSize[m_PaperSizeInx].mWidth;
        aHeight = gPaperSize[m_PaperSizeInx].mHeight;
    }
}

// Search for Sizes in Pape Size Data
int CFormatOptionTab::GetPaperSizeIndexFromData(short aUnit, double aW, double aH) 
{
    for (int i=0;i<gNumPaperSizes;i++) 
    {
        if (gPaperSize[i].mUnit == aUnit && 
            gPaperSize[i].mWidth == aW &&
            gPaperSize[i].mHeight == aH) 
        {
            return i;
        }
    }

    // find the first user defined
    for ( i=0;i<gNumPaperSizes;i++) 
    {
        if (gPaperSize[i].mIsUserDefined) 
        {
            return i;
        }
    }

    return -1;
}

int CFormatOptionTab::GetPaperSizeIndex(const CString& aStr) 
{
    for (int i=0;i<gNumPaperSizes;i++) 
    {
        if (!aStr.Compare(gPaperSize[i].mDesc)) 
        {
            return i;
        }
    }
    return -1;
}

