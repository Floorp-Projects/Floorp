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

// edtable.cpp : implementation file
//

#include "stdafx.h"
#include "property.h"
#include "edtable.h"
#include "styles.h"
#include "edt.h"
#include "edprops.h"
#include "nethelp.h"
#include "xp_help.h"
#include "prefinfo.h"
#include "prefapi.h"

// the dialog box & string resources are in edtrcdll DLL
#include "edtrcdll\src\resource.h"

// Global string in EDFRAME.CPP: "Don't change"
extern char * ed_pDontChange;

// Convert the front-end valign (0..2) to the back end alignment codes.

// FE Vertical:     Top, Center, Bottom, Baseline
// FE Horizontal:   Left, center, right
static const ED_Alignment kFEToXPVAlign[4] = {ED_ALIGN_ABSTOP, ED_ALIGN_ABSCENTER, ED_ALIGN_ABSBOTTOM, ED_ALIGN_BASELINE};
static const ED_Alignment kFEToXPAlign[3] = {ED_ALIGN_LEFT, ED_ALIGN_ABSCENTER, ED_ALIGN_RIGHT };
// Base -1 (default = -1)
// default center left right top bottom baseline abscenter absbotom abstop
static const int kXPToFEAlign[10] =  {0,  1,  0,  2, -1, -1, -1, 1, -1, -1};  // Default is shown as LEFT in listbox
static const int kXPToFEVAlign[10] = {1, -1, -1, -1,  0,  2,  3, 1,  2,  0};  // Default is shown as CENTER in listbox

static const ED_HitType kFEToXPSelType[3] = {ED_HIT_SEL_CELL, ED_HIT_SEL_ROW, ED_HIT_SEL_COL};
static const int kXPToFESelType[10] = {-1, -1, 2, 1, 0, 0, -1, -1, -1, -1};


void SetColorHelper(BOOL bOverride, COLORREF crColor, LO_Color** ppLoColor)
{
    if ( !bOverride || crColor == DEFAULT_COLORREF) {
        if ( *ppLoColor ) {
            XP_FREE(*ppLoColor);
        }
        *ppLoColor = NULL;     /* null in the default case */
    }
    else {
        // Allocates the pColorBackground pointer.
        WFE_SetLO_ColorPtr( crColor, ppLoColor );
    }
}

void GetColorHelper(LO_Color* pLoColor, BOOL& bOverride, COLORREF& crColor)
{
    bOverride = pLoColor != NULL;
    if( bOverride ){
        crColor = WFE_LO2COLORREF( pLoColor, LO_COLOR_BG );
    } else {
        crColor = DEFAULT_COLORREF;
    }
}

#define ED_PIXELS  0
#define ED_PERCENT 1

void UpdateWidthAndHeight(CDialog* pPage, int iWidth, int iHeight)
{
    char szBuf[16];
    wsprintf(szBuf, "%d", iWidth);
    pPage->GetDlgItem(IDC_WIDTH)->SetWindowText(szBuf);

    wsprintf(szBuf, "%d", iHeight);
    pPage->GetDlgItem(IDC_HEIGHT)->SetWindowText(szBuf);
}

BOOL ValidateWidthAndHeight(CDialog* pPage, 
                            int* iWidth, int* iHeight,
                            int iWidthType, int iHeightType)
{
    char szMessage[256];
    CEdit* pWidthControl = (CEdit*)(pPage->GetDlgItem(IDC_WIDTH));
    CEdit* pHeightControl = (CEdit*)(pPage->GetDlgItem(IDC_HEIGHT));
    ASSERT (pWidthControl);
    ASSERT (pHeightControl);
    char szWidth[16] = "";
    char szHeight[16] = "";
    char *szEndWidth;
    char *szEndHeight;
    pWidthControl->GetWindowText(szWidth, 15);
    pHeightControl->GetWindowText(szHeight, 15);
    
    int width = (int)strtol( szWidth, &szEndWidth, 10 );
    int height = (int)strtol( szHeight, &szEndHeight, 10 );

    int iMaxWidth = (iWidthType == ED_PERCENT) ? 100 : MAX_TABLE_PIXELS;
    int iMaxHeight = (iHeightType == ED_PERCENT) ? 100 : MAX_TABLE_PIXELS;;
    
    // If we have a checkbox to use width/height, validate range only if it is checked
    // Bad conversion if end pointer isn't at terminal null;
    BOOL bHaveWidthCheck = pPage->GetDlgItem(IDC_OVERRIDE_WIDTH) && 
                           ((CButton*)pPage->GetDlgItem(IDC_OVERRIDE_WIDTH))->GetCheck();
    BOOL bHaveHeightCheck = pPage->GetDlgItem(IDC_OVERRIDE_HEIGHT) && 
                            ((CButton*)pPage->GetDlgItem(IDC_OVERRIDE_HEIGHT))->GetCheck();
    
    // Minimum value is always 1
    BOOL bBadWidth = bHaveWidthCheck && 
                     ( (width < 1  || width > iMaxWidth) || *szEndWidth != '\0');
    BOOL bBadHeight = bHaveHeightCheck && 
                      ( (height< 1  || height > iMaxHeight) || *szEndHeight != '\0');
    
    if( bBadWidth || bBadHeight ){
        // Note: We only show 1 error at a time, so allways check Width error first

        // Construct a string showing correct range
        wsprintf( szMessage, szLoadString(IDS_INTEGER_RANGE_ERROR), 1,
                  bBadWidth ? iMaxWidth : iMaxHeight );

        // Notify user with similar message to the DDV_ validation system
        pPage->MessageBox(szMessage, szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);
        
        // Put focus in the offending control
        // And select all text, just like DDV functions
        if( bBadWidth ){
            pWidthControl->SetFocus();
            pWidthControl->SetSel(0, -1, TRUE);
        } else {
            pHeightControl->SetFocus();
            pHeightControl->SetSel(0, -1, TRUE);
        }
        return FALSE;
    }
    // Save values if they are good
    if( width > 0 ){
        *iWidth = width;
    }
    if( height > 0 ){
        *iHeight = height;
    }
    return TRUE;
}

void SetTableCaption(MWContext * pMWContext, int iCaptionIndex)
{
    // Try to get existing caption data
    EDT_TableCaptionData* pTableCaptionData = EDT_GetTableCaptionData(pMWContext);
    // NOTE: We shouldn't set insert point to the caption when it is inserted!
    //  so don't use that as test for existing caption
    BOOL bAlreadyHaveCaption = pTableCaptionData != NULL;
    
    if ( iCaptionIndex > 0 ) {
        if ( !bAlreadyHaveCaption ) {
            // Create new caption
            pTableCaptionData = EDT_NewTableCaptionData();
        }
        if ( pTableCaptionData ) {
            pTableCaptionData->align = (iCaptionIndex == 1 ? ED_ALIGN_ABSTOP : ED_ALIGN_ABSBOTTOM);

            if( bAlreadyHaveCaption ){
                // Change existing caption data
                EDT_SetTableCaptionData(pMWContext, pTableCaptionData);
            } else {
                // Insert a new caption
                EDT_InsertTableCaption(pMWContext, pTableCaptionData);
            }
            EDT_FreeTableCaptionData(pTableCaptionData);
        }
    } else if ( bAlreadyHaveCaption ) {
        // We have a caption but user changed to "none" so remove it
        EDT_DeleteTableCaption(pMWContext);
    }
}

int GetTableAlign(EDT_TableData * pTableData)
{
    int iAlign = 0; // Default = Left align
    if( pTableData ){
        switch ( pTableData->align){
            case ED_ALIGN_CENTER:
            case ED_ALIGN_ABSCENTER:
                iAlign = 1;
                break;
            case ED_ALIGN_RIGHT:
                iAlign = 2;
                break;
        }
    }
    return iAlign;
}

void SetTableAlign(EDT_TableData * pTableData, int iAlign)
{
    if( pTableData ){
        switch ( iAlign ){
            case 1:
                pTableData->align = ED_ALIGN_ABSCENTER;
                break;
            case 2:
                pTableData->align = ED_ALIGN_RIGHT;
                break;
            default:       // 0 or -1
                pTableData->align = ED_ALIGN_LEFT;
        }
    }
}

//////////////////////////////////////////////////////
//
// Property pages for Table and Cell Table properties
//
/////////////////////////////////////////////////////////////////////
CTablePage::CTablePage(CWnd *pParent, MWContext * pMWContext,
                       CEditorResourceSwitcher * pResourceSwitcher,
                       EDT_TableData * pTableData)
           : CNetscapePropertyPage(CTablePage::IDD),
             m_bActivated(0),
             m_pMWContext(pMWContext),
             m_pResourceSwitcher(pResourceSwitcher),
             m_pTableData(pTableData),
             m_bCustomColor(0),
             m_crColor(DEFAULT_COLORREF),
             m_iParentWidth(0),
             m_iParentHeight(0),
             m_bInternalChangeEditbox(0)
{
    ASSERT(pMWContext);
    ASSERT(pTableData);
    //{{AFX_DATA_INIT(CPage)
	m_iRows = 0;
    m_iColumns = 0;
	m_iAlign = -1;
	m_iCaption = 0;
	m_iBorderWidth = 0;
	m_iCellPadding = 0;
	m_iCellSpacing = 0;
	m_bColumnHeader = FALSE;
	m_bUseColor = FALSE;
	m_bUseHeight = FALSE;
	m_bUseWidth = FALSE;
	m_iHeight = 1;
	m_iHeightType = 0;
	m_iWidthType = 0;
	m_bRowHeader = FALSE;
	m_iWidth = 1;
    m_bUseCols = TRUE;
	m_csBackgroundImage = _T("");
    m_bNoSave = 0;
    m_bBorderWidthDefined = FALSE;
	//}}AFX_DATA_INIT

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}


void CTablePage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTablePage)
	DDX_Text(pDX, IDC_ROWS, m_iRows);
	DDV_MinMaxInt(pDX, m_iRows, 1, MAX_TABLE_ROWS);
    DDX_Text(pDX, IDC_COLUMNS, m_iColumns);
	DDV_MinMaxInt(pDX, m_iColumns, 1, MAX_TABLE_COLUMNS);
	DDX_CBIndex(pDX, IDC_TABLE_ALIGN, m_iAlign);
	DDX_CBIndex(pDX, IDC_TABLE_CAPTION, m_iCaption);
	DDX_Text(pDX, IDC_BORDER, m_iBorderWidth);
	DDV_MinMaxInt(pDX, m_iBorderWidth, 0, MAX_TABLE_PIXELS);
	DDX_Text(pDX, IDC_CELL_PADDING, m_iCellPadding);
	DDV_MinMaxInt(pDX, m_iCellPadding, 0, MAX_TABLE_PIXELS);
	DDX_Text(pDX, IDC_CELL_SPACING, m_iCellSpacing);
	DDV_MinMaxInt(pDX, m_iCellSpacing, 0, MAX_TABLE_PIXELS);
	DDX_Check(pDX, IDC_OVERRIDE_COLOR, m_bUseColor);
	DDX_Check(pDX, IDC_OVERRIDE_HEIGHT, m_bUseHeight);
	DDX_Check(pDX, IDC_OVERRIDE_WIDTH, m_bUseWidth);
	DDX_CBIndex(pDX, IDC_WIDTH_PIX_OR_PERCENT, m_iWidthType);
	DDX_CBIndex(pDX, IDC_HEIGHT_PIX_OR_PERCENT, m_iHeightType);
	DDX_Check(pDX, IDC_USE_COLS, m_bUseCols);
	DDX_Text(pDX, IDC_BKGRND_IMAGE, m_csBackgroundImage);
	DDX_Check(pDX, IDC_NO_SAVE_IMAGE, m_bNoSave);
	DDX_Check(pDX, IDC_USE_BORDER, m_bBorderWidthDefined);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTablePage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CTablePage)
    ON_EN_CHANGE(IDC_ROWS, EnableApplyButton)
    ON_EN_CHANGE(IDC_COLUMNS, EnableApplyButton)
    ON_EN_CHANGE(IDC_ROWS, EnableApplyButton)
    ON_EN_CHANGE(IDC_COLUMNS, EnableApplyButton)
    ON_EN_CHANGE(IDC_CELL_SPACING, EnableApplyButton)
    ON_EN_CHANGE(IDC_CELL_PADDING, EnableApplyButton)
    ON_CBN_SELCHANGE(IDC_TABLE_ALIGN, EnableApplyButton)
    ON_EN_CHANGE(IDC_TABLE_CAPTION, EnableApplyButton)
	ON_BN_CLICKED(IDC_OVERRIDE_WIDTH, EnableApplyButton)
	ON_BN_CLICKED(IDC_USE_BORDER, EnableApplyButton)
    ON_EN_CHANGE(IDC_BORDER, OnChangeBorder)
    ON_EN_CHANGE(IDC_WIDTH, OnChangeWidth)
    ON_CBN_SELCHANGE(IDC_WIDTH_PIX_OR_PERCENT, OnChangeWidthType)
	ON_BN_CLICKED(IDC_OVERRIDE_HEIGHT, EnableApplyButton)
    ON_EN_CHANGE(IDC_HEIGHT, OnChangeHeight)
    ON_CBN_SELCHANGE(IDC_HEIGHT_PIX_OR_PERCENT, OnChangeHeightType)
	ON_BN_CLICKED(IDC_OVERRIDE_COLOR, OnOverrideColor)
	ON_BN_CLICKED(IDC_CHOOSE_COLOR, OnChooseColor)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_EXTRA_HTML, OnExtraHTML)
	ON_BN_CLICKED(IDC_USE_COLS, EnableApplyButton)
	ON_BN_CLICKED(IDC_BKGRND_USE_IMAGE, OnUseBkgrndImage)
	ON_EN_CHANGE(IDC_BKGRND_IMAGE, OnChangeBkgrndImage)
	ON_BN_CLICKED(IDC_CHOOSE_BACKGROUND, OnChooseBkgrndImage)
	ON_BN_CLICKED(IDC_NO_SAVE_IMAGE, OnNoSave)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CTablePage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated)
    {
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);

	m_bActivated = TRUE;

    // Switch back to EXE's resources
    if( m_pResourceSwitcher )
        m_pResourceSwitcher->Reset();

    if( !m_ColorButton.Subclass(this, IDC_CHOOSE_COLOR, &m_crColor) )
        return FALSE;

    ((CEdit*)GetDlgItem(IDC_COLUMNS))->LimitText(3);
    ((CEdit*)GetDlgItem(IDC_ROWS))->LimitText(3);
    ((CEdit*)GetDlgItem(IDC_BORDER))->LimitText(5);
	((CEdit*)GetDlgItem(IDC_CELL_PADDING))->LimitText(5);
	((CEdit*)GetDlgItem(IDC_CELL_SPACING))->LimitText(5);
	((CEdit*)GetDlgItem(IDC_HEIGHT))->LimitText(5);
	((CEdit*)GetDlgItem(IDC_WIDTH))->LimitText(5);

    m_iRows = m_pTableData->iRows;
    m_iColumns =  m_pTableData->iColumns;
    m_bBorderWidthDefined = m_pTableData->bBorderWidthDefined;
    m_iBorderWidth = CASTINT(m_pTableData->iBorderWidth);
    m_iCellSpacing = CASTINT(m_pTableData->iCellSpacing);
    m_iCellPadding = CASTINT(m_pTableData->iCellPadding);
    m_iWidth = CASTINT(m_pTableData->iWidth);
    m_iHeight = CASTINT(m_pTableData->iHeight);
    m_iWidthType = m_pTableData->bWidthPercent ? ED_PERCENT : ED_PIXELS;
    m_iHeightType = m_pTableData->bHeightPercent ? ED_PERCENT : ED_PIXELS;
    m_iAlign = GetTableAlign(m_pTableData);
    m_bUseCols = m_pTableData->bUseCols;

    GetColorHelper(m_pTableData->pColorBackground, m_bUseColor, m_crColor);

    // Fill comboboxes
    CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_ALIGN);
    pCombo->AddString(szLoadString(IDS_LEFT));
    pCombo->AddString(szLoadString(IDS_CENTER));
    pCombo->AddString(szLoadString(IDS_RIGHT));

    pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_CAPTION);
    pCombo->AddString(szLoadString(IDS_CAPTION_NONE));
    pCombo->AddString(szLoadString(IDS_CAPTION_ABOVE));
    pCombo->AddString(szLoadString(IDS_CAPTION_BELOW));

    EDT_TableCaptionData* pTableCaptionData = EDT_GetTableCaptionData(m_pMWContext);
    if ( pTableCaptionData )
    {
        m_iCaption = pTableCaptionData->align == ED_ALIGN_ABSTOP ? 1 : 2;
        EDT_FreeTableCaptionData(pTableCaptionData);
    }
    
    // Initialize string: use "% of window" if not in table, 
    // or "% of parent cell" if we're in a cell
    pCombo = (CComboBox*)GetDlgItem(IDC_HEIGHT_PIX_OR_PERCENT);
    char *pPercent = szLoadString(CASTUINT(EDT_IsInsertPointInNestedTable(m_pMWContext) ? IDS_PERCENT_PARENT_CELL : IDS_PERCENT_WINDOW));
    pCombo->AddString(szLoadString(IDS_PIXELS));
    pCombo->AddString(pPercent);

    pCombo = (CComboBox*)GetDlgItem(IDC_WIDTH_PIX_OR_PERCENT);
    pCombo->AddString(szLoadString(IDS_PIXELS));
    pCombo->AddString(pPercent);
        
    // Get background image and set checkbox
    m_csBackgroundImage = m_pTableData->pBackgroundImage;
    if ( !m_csBackgroundImage.IsEmpty() )
    {
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
        m_bUseColor = FALSE;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->SetCheck(0);
    }
    // Disable NoSaveImage checkbox if using color
    if( m_bUseColor )
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(FALSE);
 
    // Flag to leave image at original location and not save with page
    m_bNoSave = m_pTableData->bBackgroundNoSave;

    // Get size of parent to convert % values to pixels and vice versa
    // FALSE = get current table's "parent" data (current view or cell if nested table)
    // Use to convert pixels to "% of parent" 
    EDT_GetTableParentSize(m_pMWContext, FALSE, &m_iParentWidth, &m_iParentHeight);

    // We don't use DDX with these so we don't get "not-integer" error messages
    // Set flag to prevent changing the associated checkbox
    m_bInternalChangeEditbox = TRUE;
    UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
    m_bInternalChangeEditbox = FALSE;

    // Send data to controls
    UpdateData(FALSE);

    // Clear data-modified flag
    SetModified(FALSE);

	return(TRUE);
}

BOOL CTablePage::OnKillActive()
{
    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this))
    {
        return TRUE;
    }
    if ( !UpdateData(TRUE) ||
         !ValidateWidthAndHeight(this, &m_iWidth, &m_iHeight, m_iWidthType, m_iHeightType) )
    {
        return FALSE;
    }
    if ( !m_csBackgroundImage.IsEmpty() )
    {
        if ( m_bImageChanged && !m_bValidImage )
        {
            if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() &&
                !wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/  ) )
            {
                ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
                GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(FALSE);
                return FALSE;
            }
            GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(TRUE);
            // Send changed image back to editbox in case this is Apply usage
            UpdateData(FALSE);
        }
    }
    if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() &&
        !m_csBackgroundImage.IsEmpty() )
    {
        wfe_ValidateImage( m_pMWContext, m_csBackgroundImage );
        int iLastDot = m_csBackgroundImage.ReverseFind('.');
        CString csExt;
        if(iLastDot > 0)
            csExt= m_csBackgroundImage.Mid(iLastDot);

        //we must check to see if file is a bmp!
        if (0 == csExt.CompareNoCase(".bmp"))
        {
            char *t_outputfilename=wfe_ConvertImage(m_csBackgroundImage.GetBuffer(0),(void *)this,m_pMWContext);
            if (t_outputfilename)
            {
                m_csBackgroundImage=t_outputfilename;
                wfe_ValidateImage( m_pMWContext, m_csBackgroundImage );
                XP_FREE(t_outputfilename);
                UpdateData(FALSE);//we need to update m_csImage!
            }
            else 
                return FALSE;
        }
    }

    return TRUE;
}

void CTablePage::OnHelp() 
{
    NetHelp(HELP_TABLE_PROPS_TABLE);
}

void CTablePage::OnOK() 
{
    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this)){
        return;
    }

    EDT_BeginBatchChanges(m_pMWContext);

    // Save background image
    if ( m_pTableData->pBackgroundImage )
    {
        XP_FREE( m_pTableData->pBackgroundImage );
        m_pTableData->pBackgroundImage = NULL;
    }

    if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() &&
        !m_csBackgroundImage.IsEmpty() ){
        m_pTableData->pBackgroundImage = XP_STRDUP(m_csBackgroundImage);
        // Save state of flag from checkbox
        m_pTableData->bBackgroundNoSave = m_bNoSave;
    }

    m_pTableData->iRows = m_iRows;
    m_pTableData->iColumns = m_iColumns;
    m_pTableData->bBorderWidthDefined = m_bBorderWidthDefined;
    m_pTableData->iBorderWidth = m_iBorderWidth;
    m_pTableData->iCellSpacing = m_iCellSpacing;
    m_pTableData->iCellPadding = m_iCellPadding;
    m_pTableData->bWidthDefined = m_bUseWidth;
    m_pTableData->bWidthPercent = m_iWidthType == ED_PERCENT;
    m_pTableData->iWidth = m_iWidth;
    m_pTableData->bHeightDefined = m_bUseHeight;
    m_pTableData->bHeightPercent = m_iHeightType == ED_PERCENT;
    m_pTableData->iHeight = m_iHeight;
    m_pTableData->bUseCols = m_bUseCols;

    SetTableAlign(m_pTableData, m_iAlign);
    SetColorHelper(m_bUseColor, m_crColor, &m_pTableData->pColorBackground);

    EDT_SetTableData(m_pMWContext, m_pTableData);
    // Note: Caller must free m_pTableData

    // OK, we need to insert or remove a caption, as nescessary.
    SetTableCaption(m_pMWContext, m_iCaption);

    OkToClose(); // CancelToClose() doesn't work!
    EDT_EndBatchChanges(m_pMWContext);

    CPropertyPage::OnOK();

    // These may be different as a result of Relayout during EDT_SetTableData()
    //  (Backend fixes up size data to reflect layout algorithms)
    m_iWidth = m_pTableData->iWidth;
    m_iHeight = m_pTableData->iHeight;

    // Get possibly new size of parent because of Relayout
    EDT_GetTableParentSize(m_pMWContext, FALSE, &m_iParentWidth, &m_iParentHeight);

    // We don't use DDX with these so we don't get "not-integer" error messages
    // Set flag to prevent changing the associated checkbox
    m_bInternalChangeEditbox = TRUE;
    UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
    m_bInternalChangeEditbox = FALSE;
}

void CTablePage::OnExtraHTML()
{
    CExtraHTMLDlg dlg(this, &m_pTableData->pExtra, IDS_TABLE_TAG);
    if( IDOK == dlg.DoModal() )
        SetModified(TRUE);
}

// Shared message handler for controls that
//  only need to change Apply state
void CTablePage::EnableApplyButton()
{
    SetModified(TRUE);
}

void CTablePage::OnChangeBorder()
{
    ((CButton*)GetDlgItem(IDC_USE_BORDER))->SetCheck(1);
    m_bBorderWidthDefined = TRUE;
    SetModified(TRUE);
}

void CTablePage::OnChangeWidth()
{
    if( !m_bInternalChangeEditbox )
    {
        SetModified(TRUE);
        m_bUseWidth = TRUE;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_WIDTH))->SetCheck(1);
    }
}

void CTablePage::OnChangeWidthType()
{
    // See if type has changed and we need to convert
    //   to opposite type (pixels <-> percent of parent)
    int iWidthType = ((CComboBox*)GetDlgItem(IDC_WIDTH_PIX_OR_PERCENT))->GetCurSel();
    if( iWidthType != m_iWidthType )
    {
        // Get current data from edit box before converting
        // Note that validation uses NEW width type 
        if( ValidateWidthAndHeight(this, &m_iWidth, &m_iHeight, iWidthType, m_iHeightType) )
        {
            if( iWidthType )
                // Convert previous from % to pixels
                m_iWidth = (m_iWidth * m_iParentWidth ) / 100;
            else 
                // Convert previous value from pixels to %
                m_iWidth = (m_iWidth * 100) / m_iParentWidth;

            // This will trigger OnChange for width,
            //  which will set the IDC_OVERRIDE_WIDTH checkbox
            UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
            SetModified(TRUE);
        }
        m_iWidthType = iWidthType;
    }
}


void CTablePage::OnChangeHeight()
{
    if( !m_bInternalChangeEditbox )
    {
        SetModified(TRUE);
        m_bUseHeight = TRUE;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_HEIGHT))->SetCheck(1);
    }
}

void CTablePage::OnChangeHeightType()
{
    // See if type has changed and we need to convert
    //   to opposite type (pixels <-> percent of parent)
    int iHeightType = ((CComboBox*)GetDlgItem(IDC_HEIGHT_PIX_OR_PERCENT))->GetCurSel();
    if( iHeightType != m_iHeightType )
    {
        // Get current data from edit box before converting
        // Note that validation uses NEW height type 
        if( ValidateWidthAndHeight(this, &m_iWidth, &m_iHeight, m_iWidthType, iHeightType) )
        {
            if( iHeightType )
                // Convert previous from % to pixels
                m_iHeight = (m_iHeight * m_iParentHeight ) / 100;
            else 
                // Convert previous value from pixels to %
                m_iHeight = (m_iHeight * 100) / m_iParentHeight;

            // This will trigger OnChange for height,
            //  which will set the IDC_OVERRIDE_HEIGHT checkbox
            UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
            SetModified(TRUE);
        }
        m_iHeightType = iHeightType;
    }
}

void CTablePage::OnOverrideColor() 
{
    m_bUseColor = ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->GetCheck();
    SetModified(TRUE);
}

void CTablePage::OnChooseColor() 
{
    // Get the combobox location so we popup new dialog just under it
    RECT rect;
    GetDlgItem(IDC_CHOOSE_COLOR)->GetWindowRect(&rect);

    CColorPicker dlg(this, m_pMWContext, m_crColor, BACKGROUND_COLORREF, 0, &rect);

    COLORREF crNew = dlg.GetColor();
    if( crNew != CANCEL_COLORREF )
    {
        m_crColor = crNew;
        if(m_crColor == DEFAULT_COLORREF){
            m_crColor = prefInfo.m_rgbBackgroundColor;
        }
        m_bUseColor = TRUE;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->SetCheck(m_bUseColor);
        m_ColorButton.Update();
        SetModified(TRUE);
    }
}

void CTablePage::OnUseBkgrndImage() 
{
    int iUseImageBackground = ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck();;
    SetModified(TRUE);
    if( iUseImageBackground )
    {
        // User decided to use the image, so do trigger validation
        if( !m_csBackgroundImage.IsEmpty() && 
            wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/  ) )
        {
            ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
        } else {
            GetDlgItem(IDC_BKGRND_IMAGE)->SetFocus();
        }
        m_bValidImage = TRUE;
    }
    GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(iUseImageBackground);
}

void CTablePage::OnChooseBkgrndImage() 
{
    UpdateData(TRUE);
    char * szFilename = wfe_GetExistingImageFileName(this->m_hWnd, 
                                         szLoadString(IDS_SELECT_IMAGE), TRUE);
    if ( szFilename == NULL )
        return;

    m_csBackgroundImage = szFilename;
    if( wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/  ) )
    {
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(TRUE);
    } else {
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(FALSE);
    }

    // Send new name to editbox
    UpdateData(FALSE);

    XP_FREE( szFilename );
    m_bValidImage = TRUE;
    m_bImageChanged = FALSE;
    SetModified(TRUE);
}

void CTablePage::OnNoSave() 
{
    SetModified(TRUE);
}

void CTablePage::OnChangeBkgrndImage() 
{
    // Set flags to trigger validation
    m_bValidImage = FALSE;
    m_bImageChanged = TRUE;
    
    UpdateData(TRUE);
    CString csTemp = m_csBackgroundImage;
    csTemp.TrimLeft();
    csTemp.TrimRight();

    // Set checkbox if there are any characters, clear if empty
    ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(!csTemp.IsEmpty());
    if(!csTemp.IsEmpty())
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(TRUE);

    SetModified(TRUE);
}

/////////////////////////////////////////////////////////////////
CTableCellPage::CTableCellPage(CWnd *pParent, MWContext * pMWContext,
                               CEditorResourceSwitcher * pResourceSwitcher,
                               EDT_TableCellData * pCellData,
                               UINT nIDCaption )
    : CNetscapePropertyPage(CTableCellPage::IDD, nIDCaption),
      m_bActivated(0),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher),
      m_pCellData(pCellData),
      m_bCustomColor(0),
      m_crColor(DEFAULT_COLORREF),
      m_iParentWidth(0),
      m_iParentHeight(0),
      m_bInternalChangeEditbox(0)
{
    ASSERT(pMWContext);
    ASSERT(pCellData);
    //{{AFX_DATA_INIT(CPage)
	m_iAlign = -1;
	m_iVAlign = -1;
	m_iColumnSpan = 0;
	m_iUseColor = 0;
	m_iUseHeight = 0;
	m_iUseWidth = 0;
	m_iHeader = FALSE;
	m_iHeight = 1;
	m_iHeightType = 0;
	m_iWidthType = 0;
	m_iRowSpan = 0;
	m_iWidth = 1;
	m_iNoWrap = 1;
	m_csBackgroundImage = _T("");
    m_bNoSave = 0;
    //}}AFX_DATA_INIT

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}

void CTableCellPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTableCellPage)
	DDX_Text(pDX, IDC_ROWSPAN, m_iRowSpan);
	DDV_MinMaxInt(pDX, m_iRowSpan, 1, 100);
	DDX_Text(pDX, IDC_COLSPAN, m_iColumnSpan);
	DDV_MinMaxInt(pDX, m_iColumnSpan, 1, 100);
	DDX_CBIndex(pDX, IDC_TABLE_CELL_ALIGN, m_iAlign);
	DDX_CBIndex(pDX, IDC_TABLE_CELL_VALIGN, m_iVAlign);
	DDX_Check(pDX, IDC_OVERRIDE_COLOR, m_iUseColor);
	DDX_Check(pDX, IDC_OVERRIDE_HEIGHT, m_iUseHeight);
	DDX_Check(pDX, IDC_OVERRIDE_WIDTH, m_iUseWidth);
	DDX_Check(pDX, IDC_HEADER, m_iHeader);
	DDX_Check(pDX, IDC_WRAP, m_iNoWrap);
	DDX_CBIndex(pDX, IDC_WIDTH_PIX_OR_PERCENT, m_iWidthType);
	DDX_CBIndex(pDX, IDC_HEIGHT_PIX_OR_PERCENT, m_iHeightType);
    DDX_Text(pDX, IDC_BKGRND_IMAGE, m_csBackgroundImage);
    DDX_Check(pDX, IDC_NO_SAVE_IMAGE, m_bNoSave);
//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTableCellPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CTableCellPage)
    ON_EN_CHANGE(IDC_ROWSPAN, EnableApplyButton)
    ON_EN_CHANGE(IDC_COLSPAN, EnableApplyButton)
	ON_BN_CLICKED(IDC_HEADER, EnableApplyButton)
	ON_BN_CLICKED(IDC_WRAP, EnableApplyButton)
    ON_CBN_SELCHANGE(IDC_TABLE_CELL_ALIGN, OnChangeHAlign)
    ON_CBN_SELCHANGE(IDC_TABLE_CELL_VALIGN, OnChangeVAlign)
	ON_BN_CLICKED(IDC_OVERRIDE_WIDTH, EnableApplyButton)
    ON_EN_CHANGE(IDC_WIDTH, OnChangeWidth)
    ON_CBN_SELCHANGE(IDC_WIDTH_PIX_OR_PERCENT, OnChangeWidthType)
	ON_BN_CLICKED(IDC_OVERRIDE_HEIGHT, EnableApplyButton)
    ON_EN_CHANGE(IDC_HEIGHT, OnChangeHeight)
    ON_CBN_SELCHANGE(IDC_HEIGHT_PIX_OR_PERCENT, OnChangeHeightType)
	ON_BN_CLICKED(IDC_OVERRIDE_COLOR, OnOverrideColor)
	ON_BN_CLICKED(IDC_CHOOSE_COLOR, OnChooseColor)
	ON_BN_CLICKED(IDC_EXTRA_HTML, OnExtraHTML)
	ON_BN_CLICKED(IDC_BKGRND_USE_IMAGE, OnUseBkgrndImage)
	ON_EN_CHANGE(IDC_BKGRND_IMAGE, OnChangeBkgrndImage)
	ON_BN_CLICKED(IDC_CHOOSE_BACKGROUND, OnChooseBkgrndImage)
	ON_BN_CLICKED(IDC_NO_SAVE_IMAGE, EnableApplyButton)
	ON_BN_CLICKED(IDC_PREVIOUS, OnPrevious)
	ON_BN_CLICKED(IDC_NEXT, OnNext)
	ON_CBN_SELCHANGE(IDC_TABLE_SELECTION_TYPE, OnChangeSelectionType)
	ON_BN_CLICKED(IDC_INSERT, OnInsert)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CTableCellPage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated)
    {
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);

	m_bActivated = TRUE;

    // Get these strings from the editor resource dll before we switch back
    m_csSingleCell.LoadString(IDS_SINGLE_CELL_CAPTION);
    m_csSelectedCells.LoadString(IDS_SELECTED_CELLS_CAPTION); 
    m_csSelectedRow.LoadString(IDS_SELECTED_ROW_CAPTION);
    m_csSelectedCol.LoadString(IDS_SELECTED_COLUMN_CAPTION);

    // Switch back to EXE's resources
    if( m_pResourceSwitcher )
        m_pResourceSwitcher->Reset();

    if( !m_ColorButton.Subclass(this, IDC_CHOOSE_COLOR, &m_crColor) )
        return FALSE;

    ((CEdit*)GetDlgItem(IDC_ROWSPAN))->LimitText(3);
	((CEdit*)GetDlgItem(IDC_COLSPAN))->LimitText(3);

    // Initialize Horizontal and Vertical Alignment listboxes
    CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_CELL_ALIGN);
    pCombo->AddString(szLoadString(IDS_LEFT));
    pCombo->AddString(szLoadString(IDS_CENTER));
    pCombo->AddString(szLoadString(IDS_RIGHT));

    pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_CELL_VALIGN);
    pCombo->AddString(szLoadString(IDS_TOP));
    pCombo->AddString(szLoadString(IDS_CENTER));
    pCombo->AddString(szLoadString(IDS_BOTTOM));
    pCombo->AddString(szLoadString(IDS_BASELINE));
    
    // Initialize width and height Units comboboxex
    pCombo = (CComboBox*)GetDlgItem(IDC_HEIGHT_PIX_OR_PERCENT);
    char *pPercent = szLoadString(CASTUINT(EDT_IsInsertPointInNestedTable(m_pMWContext) ? IDS_PERCENT_PARENT_CELL : IDS_PERCENT_TABLE));
    pCombo->AddString(szLoadString(IDS_PIXELS));
    pCombo->AddString(pPercent);

    pCombo = (CComboBox*)GetDlgItem(IDC_WIDTH_PIX_OR_PERCENT);
    pCombo->AddString(szLoadString(IDS_PIXELS));
    pCombo->AddString(pPercent);

    pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_SELECTION_TYPE);
    pCombo->AddString(szLoadString(IDS_CELL));
    pCombo->AddString(szLoadString(IDS_ROW));
    pCombo->AddString(szLoadString(IDS_COLUMN));

    // Get size of parent to convert % values to pixels and vice versa
    // TRUE = get size of current cell's parent, table, minus border and inter-cell space
    // Use to convert pixels to "% of parent" 
    EDT_GetTableParentSize(m_pMWContext, TRUE, &m_iParentWidth, &m_iParentHeight);

    // Initialize all controls based on cell data
    InitPageData();

    return(TRUE);
}

void CTableCellPage::InitPageData()
{

    // Set currently-selected align items in comboboxes
    CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_CELL_ALIGN);
    
    if( m_pCellData->mask & CF_ALIGN )
    {
        m_iAlign = kXPToFEAlign[m_pCellData->align+1];
        // Remove "Don't change" item in combo if already there
        if( pCombo->GetCount() == 4 )
            pCombo->DeleteString(3);
    }
    else
    {
        m_iAlign = -1;   // Empty combox = mixed styles
        // Add "Dont change" item to combo if not already there
        if( pCombo->GetCount() == 3 )
            pCombo->AddString(ed_pDontChange);
    }

    // Vertical alignment
    pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_CELL_VALIGN);
    
    if( m_pCellData->mask & CF_VALIGN )
    {
        m_iVAlign = kXPToFEVAlign[m_pCellData->valign+1];
        // Remove "Dont change" item in combo if already there
        if( pCombo->GetCount() == 5 )
            pCombo->DeleteString(4);
    }
    else
    {
        m_iVAlign = -1;
        // Add "Dont change" item to combo if not already there
        if( pCombo->GetCount() == 4 )
            pCombo->AddString(ed_pDontChange);
    }

	m_iColumnSpan = CASTINT(m_pCellData->iColSpan);
	m_iRowSpan = CASTINT(m_pCellData->iRowSpan);

    // Disable ColSpan and RowSpan if multiple cells selected
    GetDlgItem(IDC_COLSPAN)->EnableWindow(m_pCellData->mask & CF_COLSPAN);
    GetDlgItem(IDC_ROWSPAN)->EnableWindow(m_pCellData->mask & CF_ROWSPAN);

	m_iHeader = InitCheckbox(IDC_HEADER, CF_HEADER, m_pCellData->bHeader);
	m_iNoWrap = InitCheckbox(IDC_WRAP, CF_NOWRAP, m_pCellData->bNoWrap);

	m_iWidth = CASTINT(m_pCellData->iWidth);
	m_iWidthType = m_pCellData->bWidthPercent ? ED_PERCENT : ED_PIXELS;
	m_iHeight = CASTINT(m_pCellData->iHeight);
	m_iHeightType = m_pCellData->bHeightPercent ? ED_PERCENT : ED_PIXELS;

    // We don't use DDX with these so we don't get "not-integer" error messages
    // Set flag to prevent changing the associated checkbox
    m_bInternalChangeEditbox = TRUE;
    UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
    m_bInternalChangeEditbox = FALSE;

    m_iUseWidth = InitCheckbox(IDC_OVERRIDE_WIDTH, CF_WIDTH, m_pCellData->bWidthDefined);
    m_iUseHeight = InitCheckbox(IDC_OVERRIDE_HEIGHT, CF_HEIGHT, m_pCellData->bHeightDefined);

    BOOL bUseColor;
    GetColorHelper(m_pCellData->pColorBackground, bUseColor, m_crColor);
    m_iUseColor = InitCheckbox(IDC_OVERRIDE_COLOR, CF_BACK_COLOR, bUseColor);
    m_csBackgroundImage = m_pCellData->pBackgroundImage;

    BOOL bHaveImage = !m_csBackgroundImage.IsEmpty();
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
    
    int iUseImage = InitCheckbox(IDC_BKGRND_USE_IMAGE, CF_BACK_IMAGE, bHaveImage);
    ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(iUseImage);

    if( iUseImage == 1 && m_iUseColor == 1 )
    {
         // Clear the current color checkbox ONLY 
         //   if we were sure about using image
         //   we were going to use color
         m_iUseColor = 0;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->SetCheck(0);
    }

    // Disable original location checkbox if using color
    if( m_iUseColor )
    {
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(FALSE);
    }

    // Flag to leave image at original location and not save with page
    m_bNoSave = m_pCellData->bBackgroundNoSave;

    // Get type of cell set selected and set combobox
    m_iSelectionType = m_pCellData->iSelectionType;
    // Not sure if this is needed????
    if( m_iSelectionType != ED_HIT_SEL_ROW && m_iSelectionType != ED_HIT_SEL_COL )
    {
        m_iSelectionType = ED_HIT_SEL_CELL;
    }
    pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_SELECTION_TYPE);
    pCombo->SetCurSel(kXPToFESelType[m_iSelectionType]);

    // SetPageTitle only works in Win32
    // How do we set tab text in Win16?
    // Maybe set m_strCaption directly?
    CNetscapePropertySheet *pSheet = (CNetscapePropertySheet*)GetParent();

    if( m_pCellData->iSelectedCount > 1 )
    {
        switch( m_iSelectionType )
        {
            case ED_HIT_SEL_COL:
                
                pSheet->SetPageTitle(1, m_csSelectedCol);
                break;
            case ED_HIT_SEL_ROW:
                pSheet->SetPageTitle(1, m_csSelectedRow);
                break;
            default:
                pSheet->SetPageTitle(1, m_csSelectedCells);
                break;
        }
    } else {
        pSheet->SetPageTitle(1, m_csSingleCell);
    }

    // Send rest of the data to controls
    UpdateData(FALSE);
    
    // Gray out the APPLY button
    SetModified(FALSE);

    // Force redrawing entire page
    Invalidate(FALSE);
}

int CTableCellPage::InitCheckbox(UINT nIDCheckbox, ED_CellFormat cf, BOOL bSetState)
{
    int iState;
    CButton *pButton = ((CButton*)GetDlgItem(nIDCheckbox));

    // Get current style but clear all relevant button-style bits
    UINT nStyle = CASTUINT(pButton->GetButtonStyle() & 0xFFFFFFF0L);

    if( m_pCellData->mask & cf )
    {
        // We know the style, so we can use 2-state boxes
        nStyle |= BS_AUTOCHECKBOX;
        iState = bSetState ? 1 : 0;
    } else 
    {
        // We don't know the style, so we use 3-state boxes
        //  so user can return to the "don't change" state
        nStyle |= BS_AUTO3STATE;
        // Set initial value to the indeterminate state
        iState = 2;
    }
    pButton->SetButtonStyle(nStyle);

    return iState;
}

// Called before OnOK to do validation
BOOL CTableCellPage::OnKillActive()
{
    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this))
    {
        return TRUE;
    }
    if ( !UpdateData(TRUE) ||
         !ValidateWidthAndHeight(this, &m_iWidth, &m_iHeight, m_iWidthType, m_iHeightType) )
    {
        return FALSE;
    }

    if ( !m_csBackgroundImage.IsEmpty() )
    {
        if ( m_bImageChanged && !m_bValidImage )
        {
            if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() == 1 &&
                !wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE ) )
            {
                ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
                GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(FALSE);
                return FALSE;
            }
            // Send changed image back to editbox in case this is Apply usage
            UpdateData(FALSE);
        }
    }

    if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() == 1 &&
        !m_csBackgroundImage.IsEmpty() )
    {
        wfe_ValidateImage( m_pMWContext, m_csBackgroundImage );
        int iLastDot = m_csBackgroundImage.ReverseFind('.');
        CString csExt;
        if(iLastDot > 0)
            csExt= m_csBackgroundImage.Mid(iLastDot);

        //we must check to see if file is a bmp!
        if (0 == csExt.CompareNoCase(".bmp"))
        {
            char *t_outputfilename=wfe_ConvertImage(m_csBackgroundImage.GetBuffer(0),(void *)this,m_pMWContext);
            if (t_outputfilename)
            {
                m_csBackgroundImage=t_outputfilename;
                wfe_ValidateImage( m_pMWContext, m_csBackgroundImage );
                XP_FREE(t_outputfilename);
                UpdateData(FALSE);//we need to update m_csImage!
            }
            else 
                return FALSE;
        }
    }
    return TRUE;
}

void CTableCellPage::OnOK() 
{
    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this))
    {
        return;
    }

    if ( !UpdateData(TRUE) ||
         !ValidateWidthAndHeight(this, &m_iWidth, &m_iHeight, m_iWidthType, m_iHeightType) )
    {
        return;
    }

    // Use checkbox states and set/clear mask bits and data appropriately

    // This checkbox is not hooked up to DDX    
    int iCheckState = ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck();
    
    // Clear background image unless we shouldn't change it
    if( iCheckState != 2 )
        XP_FREEIF( m_pCellData->pBackgroundImage );

    if( iCheckState == 1 &&
        !m_csBackgroundImage.IsEmpty() )
    {
        m_pCellData->pBackgroundImage = XP_STRDUP(m_csBackgroundImage);

        // Save state of flag from checkbox
        m_pCellData->bBackgroundNoSave = m_bNoSave;
    }

    if( m_iAlign == -1 )
    {
        // Don't change this attribute
        FE_CLEAR_BIT(m_pCellData->mask, CF_ALIGN);
    } else {
        // We are sure user wants to set this
        FE_SET_BIT(m_pCellData->mask, CF_ALIGN);
    }
    m_pCellData->align = kFEToXPAlign[m_iAlign];

    if( m_iVAlign == -1 )
    {
        FE_CLEAR_BIT(m_pCellData->mask, CF_VALIGN);
    } else {
        FE_SET_BIT(m_pCellData->mask, CF_VALIGN);
    }
    m_pCellData->valign = kFEToXPVAlign[m_iVAlign];

    // These can't change if disabled, so it doesn't 
    //  hurt to get values no matter what
	m_pCellData->iColSpan = m_iColumnSpan;
	m_pCellData->iRowSpan = m_iRowSpan;

    if( m_iUseColor == 2 )
    {
        FE_CLEAR_BIT(m_pCellData->mask, CF_BACK_COLOR);
    } else {
        FE_SET_BIT(m_pCellData->mask, CF_BACK_COLOR);
        SetColorHelper(m_iUseColor == 1, m_crColor, &m_pCellData->pColorBackground);
    }

    if( m_iHeader == 2 )
    {
        FE_CLEAR_BIT(m_pCellData->mask, CF_HEADER);
    } else {
        FE_SET_BIT(m_pCellData->mask, CF_HEADER);
	    m_pCellData->bHeader = m_iHeader > 0;
    }

    if( m_iNoWrap == 2 )
    {
        FE_CLEAR_BIT(m_pCellData->mask, CF_NOWRAP);
    } else {
        FE_SET_BIT(m_pCellData->mask, CF_NOWRAP);
    	m_pCellData->bNoWrap = m_iNoWrap > 0;
    }

    if( m_iUseHeight == 2 )
    {
        FE_CLEAR_BIT(m_pCellData->mask, CF_HEIGHT);
    } else {
        FE_SET_BIT(m_pCellData->mask, CF_HEIGHT);
	    m_pCellData->iHeight = m_iHeight;
	    m_pCellData->bHeightPercent = (m_iHeightType == ED_PERCENT);
    }
    m_pCellData->bHeightDefined = (m_iUseHeight == 1);
    
    if( m_iUseWidth == 2 )
    {
        FE_CLEAR_BIT(m_pCellData->mask, CF_WIDTH);
    } else {
        FE_SET_BIT(m_pCellData->mask, CF_WIDTH);
	    m_pCellData->iWidth = m_iWidth;
	    m_pCellData->bWidthPercent = (m_iWidthType == ED_PERCENT);
    }
    m_pCellData->bWidthDefined = (m_iUseWidth == 1);
    
    EDT_SetTableCellData(m_pMWContext, m_pCellData);

    // TODO: WE MAY NOT NEED THIS AFTER NEW LAYOUT WORK
    // We must call this here else special selection is lost
    EDT_StartSpecialCellSelection(m_pMWContext, m_pCellData);

    OkToClose(); // CancelToClose() doesn't work!
    CPropertyPage::OnOK();
    
    // These may be different as a result of Relayout during EDT_SetTableData()
    //  (Backend fixes up size data to reflect layout algorithms)
    m_iWidth = m_pCellData->iWidth;
    m_iHeight = m_pCellData->iHeight;

    // Get possibly new size of parent because of Relayout
    EDT_GetTableParentSize(m_pMWContext, TRUE, &m_iParentWidth, &m_iParentHeight);

    // We don't use DDX with these so we don't get "not-integer" error messages
    // Set flag to prevent changing the associated checkbox
    m_bInternalChangeEditbox = TRUE;
    UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
    m_bInternalChangeEditbox = FALSE;
}

void CTableCellPage::OnChangeSelectionType()
{
    CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_SELECTION_TYPE);

    // Get selection type from combobox    
    m_iSelectionType = kFEToXPSelType[pCombo->GetCurSel()];
    
    // Change selection, but don't move focus cell
    ChangeSelection(ED_MOVE_NONE);
}

void CTableCellPage::ChangeSelection(ED_MoveSelType iMoveType)
{
    if( iMoveType != ED_MOVE_NONE && IS_APPLY_ENABLED(this) &&
        IDYES == MessageBox(szLoadString(IDS_APPLY_CELL_MSG), szLoadString(IDS_CHANGE_SEL_CAPTION),
                            MB_YESNO | MB_ICONQUESTION) )
    {
        // Save current data
        if( OnKillActive() )
        {
            OnOK();
        } else {
            return;
        }
    }

    EDT_ChangeTableSelection(m_pMWContext, m_iSelectionType, iMoveType, m_pCellData);
    InitPageData();
}

void CTableCellPage::OnPrevious()
{
    ChangeSelection(ED_MOVE_PREV);
}

void CTableCellPage::OnNext()
{
    ChangeSelection(ED_MOVE_NEXT);
}

void CTableCellPage::OnHelp() 
{
    NetHelp(HELP_TABLE_PROPS_CELL);
}

void CTableCellPage::OnExtraHTML()
{
    CExtraHTMLDlg dlg(this, &m_pCellData->pExtra, IDS_TD_TAG);
    if( IDOK == dlg.DoModal() )
    {
        SetModified(TRUE);
    }
}

void CTableCellPage::OnChangeWidth()
{
    if( !m_bInternalChangeEditbox )
    {
        SetModified(TRUE);
        m_iUseWidth = TRUE;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_WIDTH))->SetCheck(1);
    }
}

void CTableCellPage::OnChangeWidthType()
{
    // See if type has changed and we need to convert
    //   to opposite type (pixels <-> percent of parent)
    int iWidthType = ((CComboBox*)GetDlgItem(IDC_WIDTH_PIX_OR_PERCENT))->GetCurSel();
    if( iWidthType != m_iWidthType )
    {
        // Get current data from edit box before converting
        // Note that validation uses NEW width type 
        if( ValidateWidthAndHeight(this, &m_iWidth, &m_iHeight, iWidthType, m_iHeightType) )
        {
            if( iWidthType ) 
            {
                // New type = %
                // Convert previous value from pixels to %
                m_iWidth = (m_iWidth * 100) / m_iParentWidth;
            } else {
                // New type = pixels
                // Convert previous from % to pixels
                m_iWidth = (m_iWidth * m_iParentWidth ) / 100;
            }

            UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
            SetModified(TRUE);
        }
        m_iWidthType = iWidthType;
    }
}

void CTableCellPage::OnChangeHeight()
{
    if( !m_bInternalChangeEditbox )
    {
        SetModified(TRUE);
        m_iUseHeight = 1;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_HEIGHT))->SetCheck(1);
    }
}

void CTableCellPage::OnChangeHeightType()
{
    // See if type has changed and we need to convert
    //   to opposite type (pixels <-> percent of parent)
    int iHeightType = ((CComboBox*)GetDlgItem(IDC_HEIGHT_PIX_OR_PERCENT))->GetCurSel();
    if( iHeightType != m_iHeightType )
    {
        // Get current data from edit box before converting
        // Note that validation uses NEW height type 
        if( ValidateWidthAndHeight(this, &m_iWidth, &m_iHeight, m_iWidthType, iHeightType) )
        {
            if( iHeightType )
                // Convert previous from % to pixels
                m_iHeight = (m_iHeight * m_iParentHeight ) / 100;
            else 
                // Convert previous value from pixels to %
                m_iHeight = (m_iHeight * 100) / m_iParentHeight;

            UpdateWidthAndHeight(this, m_iWidth, m_iHeight);
            SetModified(TRUE);
        }
        m_iHeightType = iHeightType;
    }
}

void CTableCellPage::OnOverrideColor() 
{
    m_iUseColor = ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->GetCheck();
    SetModified(TRUE);
}

void CTableCellPage::OnChooseColor() 
{
    // Get the combobox location so we popup new dialog just under it
    RECT rect;
    GetDlgItem(IDC_CHOOSE_COLOR)->GetWindowRect(&rect);

    CColorPicker dlg(this, m_pMWContext, m_crColor, BACKGROUND_COLORREF, 0, &rect);

    COLORREF crNew = dlg.GetColor();
    if( crNew != CANCEL_COLORREF )
    {
        m_crColor = crNew;
        if(m_crColor == DEFAULT_COLORREF)
        {
            m_crColor = prefInfo.m_rgbBackgroundColor;
        }
        m_iUseColor = 1;
        ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->SetCheck(m_iUseColor);
        m_ColorButton.Update();
        SetModified(TRUE);
    }
}

void CTableCellPage::OnUseBkgrndImage() 
{
    int iUseImageBackground = ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck();;
    SetModified(TRUE);
    if( iUseImageBackground )
    {
        // User decided to use the image, so do trigger validation
        if( !m_csBackgroundImage.IsEmpty() && 
            wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/ ) )
        {
            ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
        } else {
            GetDlgItem(IDC_BKGRND_IMAGE)->SetFocus();
        }
        m_bValidImage = TRUE;
    }
    GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(iUseImageBackground);
}

void CTableCellPage::OnChooseBkgrndImage() 
{
    UpdateData(TRUE);
    char * szFilename = wfe_GetExistingImageFileName(this->m_hWnd, 
                                         szLoadString(IDS_SELECT_IMAGE), TRUE);
    if ( szFilename == NULL )
        return;

    m_csBackgroundImage = szFilename;
    if( wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/ ) )
    {
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(TRUE);
    } else {
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(FALSE);
    }

    // Send new name to editbox
    UpdateData(FALSE);

    XP_FREE( szFilename );
    m_bValidImage = TRUE;
    m_bImageChanged = FALSE;
    SetModified(TRUE);
}

void CTableCellPage::OnChangeBkgrndImage() 
{
    // Set flags to trigger validation
    m_bValidImage = FALSE;
    m_bImageChanged = TRUE;
    
    UpdateData(TRUE);
    CString csTemp = m_csBackgroundImage;
    csTemp.TrimLeft();
    csTemp.TrimRight();

    // Set checkbox if there are any characters, clear if empty
    ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(!csTemp.IsEmpty());
    if(!csTemp.IsEmpty())
    {
        GetDlgItem(IDC_NO_SAVE_IMAGE)->EnableWindow(TRUE);
    }
    SetModified(TRUE);
}

void CTableCellPage::OnChangeHAlign()
{
    SetModified(TRUE);
    // When user selects last item ("Don't change"),
    //  change selected item to empty combobox
    CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_CELL_ALIGN);
    m_iAlign = pCombo->GetCurSel();
    if( m_iAlign == 3 )
    {
        m_iAlign = -1;
        pCombo->SetCurSel(-1);
    }
}

void CTableCellPage::OnChangeVAlign()
{
    SetModified(TRUE);
    CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TABLE_CELL_VALIGN);
    m_iVAlign = pCombo->GetCurSel();
    if( m_iVAlign == 4 )
    {
        m_iVAlign = -1;
        pCombo->SetCurSel(-1);
    }
}

void CTableCellPage::OnInsert()
{
    //TODO: ADD INSERT DIALOG
}


void CTableCellPage::OnDelete()
{
    //TODO: ADD DELETE DIALOG
}

// Shared message handler for controls that
//  only need to change Apply state
void CTableCellPage::EnableApplyButton()
{
    SetModified(TRUE);
}

/////////////////////////////////////////////////////////////////
