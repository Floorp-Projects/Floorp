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

// edframe.cpp : implementation file
//
#include "stdafx.h"
#ifdef EDITOR
#include "edt.h"
#include "netsvw.h"
#include "edview.h"
#include "edprops.h"
#include "mainfrm.h"
#include "edframe.h"
#include "edres2.h"
#include "compfrm.h"
#include "resource.h"
#include "prefapi.h"
#include "prefapi.h"
#include "eddialog.h"
#include "template.h"
#include "edtplug.h"
// For XP Strings
extern "C" {
#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
}


#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

// Globals in TEMPLATE.CPP - 
//     saves last frame location for new window postioning
//     and flag to overlay current window (default = cascaded windows)
extern BOOL wfe_bUseLastFrameLocation;
extern CGenericFrame *wfe_pLastFrame;

char *EDT_NEW_DOC_URL = NULL;
char *EDT_NEW_DOC_NAME = NULL;

extern TagType FEED_nParagraphTags[];

void FED_SetupStrings(void)
{
    // Get strings from XP_MSG.I for new document stuff
    // These are the same strings used in \LIB\LIBNET\MKGETURL.C
    //  to do most of the new doc stuff at cross-platform level
    if (EDT_NEW_DOC_URL == NULL) {
        StrAllocCopy(EDT_NEW_DOC_URL, XP_GetString(XP_EDIT_NEW_DOC_URL) );
    }
    if (EDT_NEW_DOC_NAME == NULL) {
        StrAllocCopy(EDT_NEW_DOC_NAME, XP_GetString(XP_EDIT_NEW_DOC_NAME) );
    }
}

static UINT nTraceCount = 0;

// toolbar buttons - IDs are command buttons except for ID_SEPARATOR and 
//   ID_COMBOBOX, the placeholder for comboboxes.built with CComboToolBar  _WIN32
// Main edit toolbar:
static UINT BASED_CODE nIDEditBarArray[] =
{
	// same order as in the bitmap for toolbar
    ID_EDT_NEW_DOC_BLANK,
    ID_FILE_OPEN,
    ID_EDT_FILE_SAVE,
    ID_FILE_PUBLISH,
    ID_OPEN_NAV_WINDOW,
    ID_FILE_PRINT,
    ID_CHECK_SPELLING,
    ID_MAKE_LINK,
    ID_INSERT_TARGET,
    ID_INSERT_IMAGE,
    ID_INSERT_HRULE,
	ID_INSERT_TABLE_OR_PROPS,
};
#define EDITBAR_ID_COUNT ((sizeof(nIDEditBarArray))/sizeof(UINT))

// Character Format Toolbar:
// NOTE: We now use a NSToolbar2 for the button array
static UINT BASED_CODE nIDCharacterBarArray[] =
{
	// same order as in the bitmap for toolbar
    ID_COMBOBOX,        // Paragraph style
    ID_COMBOBOX,        // Font Face
    ID_COMBOBOX,        // Font Size
    ID_COMBOBOX         // Font Color combobox in Win4+ versions - define COLOR_COMBO_INDEX as this location
};
#define CHARBAR_ID_COUNT ((sizeof(nIDCharacterBarArray))/sizeof(UINT))

static UINT BASED_CODE nIDCharButtonBarArray[] =
{
	// same order as in the bitmap for toolbar
    ID_FORMAT_CHAR_BOLD,
    ID_FORMAT_CHAR_ITALIC,
    ID_FORMAT_CHAR_UNDERLINE,
    ID_FORMAT_CHAR_NONE,
    ID_UNUM_LIST,
    ID_NUM_LIST,
    ID_FORMAT_OUTDENT,
    ID_FORMAT_INDENT,
    ID_ALIGN_POPUP,
    ID_INSERT_POPUP
};
#define CHARBUTTONBAR_ID_COUNT ((sizeof(nIDCharButtonBarArray))/sizeof(UINT))

#define COLOR_COMBO_INDEX 3 // Location of color combo we hide in Win16/NT3.51

// Global strings for dropdown property combos
// Filled in once in InitComposer()
char * ed_pOther = NULL;
char * ed_pMixedFonts = NULL;
char * ed_pDontChange = NULL;
CBitmap ed_DragCaretBitmap;

//////////////////////////////////////////////////////////////////////////
// The height of an item in the dropdown listbox part of a comobbox
extern int wfe_iListItemHeight;
extern void SetCurrentDefaultFontColor();

// Callback notification when preferences are changed
int PR_CALLBACK ed_prefWatcher(const char *pPrefName, void *pData)
{
	switch ((int)pData) {
		case 1:
        {
            int32 iDelay;
	        PREF_GetIntPref("editor.auto_save_delay", &iDelay);

            CGenericFrame * f;
            // We must set the autosave values for ALL edit contexts
            for(f = theApp.m_pFrameList; f; f = f->m_pNext) {
                MWContext *pMWContext = f->GetMainContext()->GetContext();
    
                if ( pMWContext && EDT_IS_EDITOR(pMWContext) && !pMWContext->bIsComposeWindow ){
                    EDT_SetAutoSavePeriod(pMWContext, iDelay);
                }
            }
            break;
        }
        case 2:
        {
	        PREF_GetIntPref("editor.fontsize_mode", &wfe_iFontSizeMode);

            CGenericFrame * f;
            // We update font size combobox for ALL edit contexts
            for(f = theApp.m_pFrameList; f; f = f->m_pNext) {
                MWContext *pMWContext = f->GetMainContext()->GetContext();
                if ( pMWContext && EDT_IS_EDITOR(pMWContext) ){
                    CNetscapeEditView * pView = (CNetscapeEditView*)f->GetActiveView();
                    if( pView ){
	                    pView->UpdateFontSizeCombo();
                    }
                }
            }
            break;        
        }
        case 3:
        {
            // Find all composer windows and change Author name only if it
            //   isn't already set in the page
            CGenericFrame * f;
            for(f = theApp.m_pFrameList; f; f = f->m_pNext) {
                MWContext *pMWContext = f->GetMainContext()->GetContext();
    
                if ( pMWContext && EDT_IS_EDITOR(pMWContext) && !pMWContext->bIsComposeWindow ){
                    int count = EDT_MetaDataCount(pMWContext);
                    BOOL bFound = FALSE;
                    EDT_MetaData* pData = NULL;
                    for ( int i = 0; i < count; i++ ) {
                        pData = EDT_GetMetaData(pMWContext, i);
                        if( pData ){
                            if( 0 != _stricmp(pData->pName, "Author") ){
                                bFound = TRUE;
                                EDT_FreeMetaData(pData);
                                break;
                            }
                            EDT_FreeMetaData(pData);
                        }
                    }                        
                    // Use preference only if Author tag not already found
                    if(!bFound){
                        pData->pName = XP_STRDUP("Author");
                        PREF_CopyCharPref("editor.author", &pData->pContent);
                        EDT_SetMetaData(pMWContext, pData);
                        EDT_FreeMetaData(pData);
                    }
                }
            }
            break;
        }
    }

	return PREF_NOERROR;
}

static void wfe_EditURLCallback(const char* urlToOpen){
    FE_LoadUrl((char*) urlToOpen, LOAD_URL_COMPOSER);
}

// Initialize global data (Note: we don't have a MWContext/Frame/View yet)
void WFE_InitComposer()
{     
    // Check to make sure range of IDs in EDRES1.H (which start at 36000)
    //  don't collide with those in RESOURCE.H
    ASSERT(ID_EDIT_LAST_ID < 42000);
    int i;

    UINT   n = ID_FORMAT_FONTFACE_BASE;

    char   pPref[32];
    char * pLocation = NULL;
    // Count the number of most-recently-used template locations
    theApp.m_iTemplateLocationCount = 0;
    for ( i = 0; i < MAX_TEMPLATE_LOCATIONS; i++ ){
        sprintf( pPref, "editor.template_history_%d", i);
        PREF_CopyCharPref(pPref, &pLocation);
        if( pLocation && *pLocation ){
            // We have a non-empty string - count it
            theApp.m_iTemplateLocationCount++;
            XP_FREEIF(pLocation);
        } else {
            break;
        }
    }
    XP_FREEIF(pLocation);

    // Initialize the static string "Other" we use in comboboxes
    if( !ed_pOther ){
        ed_pOther = XP_STRDUP(szLoadString(IDS_OTHER));
    }
    if( !ed_pMixedFonts ){
        ed_pMixedFonts = XP_STRDUP(szLoadString(IDS_MIXED_FONTS));
    }
    if( !ed_pDontChange ){
        ed_pDontChange = XP_STRDUP(szLoadString(IDS_DONT_CHANGE));
    }
    // This will be updated in CGenericFrame::OnDisplayPreferences() 
    PREF_GetIntPref("editor.fontsize_mode", &wfe_iFontSizeMode);

    // Register callbacks for items we need to know immediately when changed
	PREF_RegisterCallback("editor.auto_save_delay", ed_prefWatcher, (void*)1);
	PREF_RegisterCallback("editor.fontsize_mode", ed_prefWatcher, (void*)2);
	PREF_RegisterCallback("editor.author", ed_prefWatcher, (void*)3);

    wfe_pFont = new CFont();
    wfe_pBoldFont = new CFont();

    if( wfe_pFont && wfe_pBoldFont ){
        if( GetSystemMetrics(SM_DBCSENABLED) ){
            HFONT	hFont = NULL;

#ifdef _WIN32
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
#endif
            if (!hFont){
	            hFont = (HFONT)GetStockObject(SYSTEM_FONT);
            }
            wfe_pFont->Attach(hFont);
            wfe_pBoldFont->Attach(hFont);
        } else {
            //  Get a 1-pixel font
            LOGFONT logFont;
            memset(&logFont, 0, sizeof(logFont));
            //logFont.lfHeight = -MulDiv(9, ::GetDeviceCaps(hDC, LOGPIXELSY), 72);
            logFont.lfHeight = -10; // This maps to "8 pts"
            logFont.lfWeight = FW_NORMAL;
            logFont.lfCharSet = IntlGetLfCharset(CIntlWin::GetSystemLocaleCsid());
            logFont.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
            if (CIntlWin::GetSystemLocaleCsid() == CS_LATIN1)
	            _tcscpy(logFont.lfFaceName, "MS Sans Serif");
            else
	            _tcscpy(logFont.lfFaceName, IntlGetUIPropFaceName(CIntlWin::GetSystemLocaleCsid()));

            if( !wfe_pFont->CreateFontIndirect(&logFont) ){
	            delete wfe_pFont;
                wfe_pFont = NULL;
            }
            
            logFont.lfWeight = FW_BOLD;
            if( !wfe_pBoldFont->CreateFontIndirect(&logFont) ){
	            delete wfe_pBoldFont;
                wfe_pBoldFont = NULL;
            }
        }
    }

    // Set default Author name to the same as mail username if not already set    
    char * pAuthor = NULL;
    if( PREF_CopyCharPref("editor.author", &pAuthor) != PREF_NOERROR || !pAuthor || !*pAuthor ){
        // No Author field - copy the mail username
        char * pUserName = NULL;
        if( PREF_CopyCharPref("mail.identity.username", &pUserName) == PREF_NOERROR &&
            pUserName && *pUserName ){
            PREF_SetCharPref("editor.author", pUserName);
        }
        XP_FREEIF(pUserName);
    }
    XP_FREEIF(pAuthor);

    // Register edtplug java-to-C++ Open-the-editor callback.
    EDTPLUG_RegisterEditURLCallback(&wfe_EditURLCallback);

    ed_DragCaretBitmap.LoadBitmap(IDB_ED_DRAG);

    // Get global last-color-picked from prefs
    char * pCustomColor = NULL;
    LO_Color LoColor;

    PREF_CopyCharPref("editor.last_color_picked", &pCustomColor);    
    if( pCustomColor )
    {        
        EDT_ParseColorString(&LoColor, pCustomColor);
        wfe_crLastColorPicked =  RGB(LoColor.red, LoColor.green, LoColor.blue);
        XP_FREEIF(pCustomColor);
    } else {
        wfe_crLastColorPicked = 0;
    }
	
    PREF_CopyCharPref("editor.last_background_color_picked", &pCustomColor);    
    if( pCustomColor )
    {        
        EDT_ParseColorString(&LoColor, pCustomColor);
        wfe_crLastBkgrndColorPicked =  RGB(LoColor.red, LoColor.green, LoColor.blue);
        XP_FREEIF(pCustomColor);
    } else {
        wfe_crLastBkgrndColorPicked = RGB(255, 255, 255);
    }
	
    //  Fill color table with 0s
    memset((void*)wfe_CustomPalette, 0, 16*sizeof(COLORREF));
    
    // We should make this an expandable list so we can 
    //   append document colors to the list
    for( i = 1; i <= MAX_CUSTOM_COLORS; i++ )
    {
        wsprintf(pPref, "editor.custom_color_%d", (i < MAX_CUSTOM_COLORS) ? i : 0);
        PREF_CopyCharPref(pPref, &pCustomColor);    
		if (pCustomColor)
        	EDT_ParseColorString(&LoColor, pCustomColor);
        XP_FREEIF(pCustomColor);

        // Save color to test if it changed
        wfe_CustomPalette[i] = RGB(LoColor.red, LoColor.green, LoColor.blue);
    }
}

void WFE_ExitComposer()
{
    if( wfe_pFont ){
        delete wfe_pFont;
    }
    if( wfe_pBoldFont ){
        delete wfe_pBoldFont;
    }
    XP_FREEIF(ed_pOther);
    XP_FREEIF(ed_pMixedFonts);
    XP_FREEIF(ed_pDontChange);
    ed_DragCaretBitmap.DeleteObject();
}

/////////////////////////////////////////////////////////////////////////////
// Toolbar controller class used by Edit and Mail compose frames
//
CEditToolBarController::CEditToolBarController(CWnd * pParent) :
    m_pWnd(pParent),
    m_iFontColorOtherIndex(0),
    m_pCharacterToolbar(0)
{
}

CEditToolBarController::~CEditToolBarController()
{
    if( m_pCharacterToolbar )
	    delete m_pCharacterToolbar;
}

BOOL CEditToolBarController::CreateEditBars(MWContext *pMWContext, unsigned ett)
{
    // Initialize things needed by both CNetscapeEditFrame and CComposeFrame
	CGenericFrame *pParent = (CGenericFrame*)GetParent();

	if (ett & DISPLAY_EDIT_TOOLBAR) {
		CButtonToolbarWindow *pWindow;
		BOOL bOpen, bShowing;
		int32 nPos;

		//I'm hardcoding because I don't want this translated
		pParent->GetChrome()->LoadToolbarConfiguration(IDS_EDIT_TOOLBAR_CAPTION, CString("Composition_Toolbar"),nPos, bOpen, bShowing);

        // Add a customizable toolbar
		LPNSTOOLBAR pIToolBar;
		pParent->GetChrome()->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
		if ( pIToolBar ) {
			pIToolBar->Create( pParent, WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE|CBRS_TOP );
			pIToolBar->SetButtons( nIDEditBarArray, EDITBAR_ID_COUNT );
			// Set menu/toolbar popup styles for specific buttons:
            pIToolBar->SetButtonStyle(ID_EDT_NEW_DOC_BLANK, TB_HAS_TIMED_MENU);
            pIToolBar->SetButtonStyle(ID_FILE_OPEN, TB_HAS_TIMED_MENU);
            pIToolBar->SetButtonStyle(ID_FILE_PRINT, TB_HAS_TIMED_MENU);
            
            // First pair are actually ignored! Second is bitmap size
            pIToolBar->SetSizes( CSize( 29, 27 ), CSize( 23, 21 ) );
			pIToolBar->LoadBitmap( MAKEINTRESOURCE( IDB_FILE_PICT_TOOLBAR ) );
			pIToolBar->SetToolbarStyle( theApp.m_pToolbarStyle );
			pWindow = new CButtonToolbarWindow(CWnd::FromHandlePermanent(pIToolBar->GetHWnd()), theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);
			pParent->GetChrome()->GetCustomizableToolbar()->AddNewWindow(IDS_EDIT_TOOLBAR_CAPTION, pWindow,nPos, 50, 37, 0, CString(szLoadString(IDS_EDIT_TOOLBAR_CAPTION)),theApp.m_pToolbarStyle, bOpen, FALSE);
			pParent->GetChrome()->ShowToolbar(IDS_EDIT_TOOLBAR_CAPTION, bShowing);
   			pIToolBar->Release();
		}
    }

	// Character Format toolbar
	if( ett & DISPLAY_CHARACTER_TOOLBAR ){
        // We don't use the "Insert Object" last item if we are displaying the edit toolbar,
        //  which has these items
        if (!m_wndCharacterBar.Create(ett & DISPLAY_EDIT_TOOLBAR, GetParent(), IDW_PARA_TOOLBAR, IDS_CHAR_TOOLBAR_CAPTION,
								   nIDCharacterBarArray, CHARBAR_ID_COUNT,
								   IDB_NEW_FORMAT_TOOLBAR,
								   CSize(8, ED_TB_BUTTON_HEIGHT),
								   CSize(1,ED_TB_BITMAP_HEIGHT) ) )
            return FALSE;
		m_pCharacterToolbar = CreateCharacterToolbar((ett & DISPLAY_EDIT_TOOLBAR) ?  CHARBUTTONBAR_ID_COUNT-1 : CHARBUTTONBAR_ID_COUNT);

		m_wndCharacterBar.SetCNSToolbar(m_pCharacterToolbar);


	    // Paragraph styles Combo
	    CRect rect;
        rect.SetRectEmpty();  //Don't need size to create it

	    if (!m_ParagraphCombo.Create(CBS_DROPDOWNLIST|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL,
                                     rect, &m_wndCharacterBar, ID_COMBO_PARA))
	    {
		    TRACE0("Failed to create paragraph format combo-box\n");
		    return FALSE;
	    }

        // Font Face Combo
	    if (!m_FontFaceCombo.Create(rect, &m_wndCharacterBar, ID_COMBO_FONTFACE))
	    {
		    return FALSE;
	    }

        // Font Size Combo      
	    if (!m_FontSizeCombo.Create(rect, &m_wndCharacterBar, ID_COMBO_FONTSIZE))
	    {
		    TRACE0("Failed to create Font Size combo-box\n");
		    return FALSE;
	    }

        if ( wfe_pFont ){
		    m_ParagraphCombo.SetFont(wfe_pFont);
            //Other combos are CNSComboBox and have font set in Create or Subclass calls
        }

        if( !m_FontColorCombo.CreateAndFill(rect, &m_wndCharacterBar,
                                            ID_COMBO_FONTCOLOR, DEFAULT_COLORREF) ){
            return FALSE;
        }

	    //  Fill the other combo boxes
	    CString csTemp;
        // Paragraph - Get maximum width while loading strings
        int iMaxParaWidth = 0;
        CDC *pDC = m_ParagraphCombo.GetDC();
        CSize cSize;
        int wincsid = INTL_CharSetNameToID(INTL_ResourceCharSet());
	    for ( int i = 0; FEED_nParagraphTags[i] != P_UNKNOWN; i++ ){
		    if (csTemp.LoadString(CASTUINT(ID_LIST_TEXT_PARAGRAPH_BASE+FEED_nParagraphTags[i]))){
			    m_ParagraphCombo.AddString((LPCTSTR)csTemp);
                if ( pDC ){            
    		        cSize = CIntlWin::GetTextExtent(wincsid, pDC->GetSafeHdc(), csTemp, csTemp.GetLength());
	        	    pDC->LPtoDP(&cSize);
	    	        if ( cSize.cx > iMaxParaWidth ){
    			        iMaxParaWidth = cSize.cx;
		            }
    		    }
	        }
	    }
        // String used to get width of FontSize combobox
        csTemp = "55";
        cSize = CIntlWin::GetTextExtent(wincsid, pDC->GetSafeHdc(), csTemp, csTemp.GetLength());
        pDC->LPtoDP(&cSize);
        // Save the full height of this font
        wfe_iFontHeight = cSize.cy;
        
        // Calculate list item height from font size and store in global variable
        // Trim 1 pixel off height to fit more items in the list
        wfe_iListItemHeight = wfe_iFontHeight - 1;
        int iFontSizeWidth = cSize.cx + sysInfo.m_iScrollWidth + 6;

        m_ParagraphCombo.ReleaseDC(pDC);

        // Initialize app-global list of TrueType fonts
        //  and fill the toolbar combo with all font strings
        // WARNING: This is a user-draw listbox and pointers to font-name strings
        //          must be STATIC 
        pDC = m_FontFaceCombo.GetDC();
        wfe_InitTrueTypeArray(pDC);
        int iMaxFontWidth = 0;
        wfe_iFontFaceOtherIndex = wfe_FillFontComboBox(&m_FontFaceCombo, &iMaxFontWidth);
        // ??? "NEEDED ONLY IF NOT A WINDOW"
        m_FontFaceCombo.ReleaseDC(pDC);

        // Font Size
        // This gets filled on every call to open the combobox
        wfe_FillFontSizeCombo(pMWContext, &m_FontSizeCombo);

        m_FontFaceCombo.SetCurSel(0);         // show the first entry 

        // Now that we have a font, we can set the full size of comboboxes
        // The order of these calls determine order of appearence in toolbar
        // Use 0 for height (param 5) to let class figure out optimal height
        // Note that we use Scroll Width to get size of combobox since users
        //  can change that in Win95 and NT4.0 and that size determines width
        //  of combobox dropdown button
	    m_wndCharacterBar.SetComboBox( ID_COMBO_PARA, &m_ParagraphCombo, 
								       sysInfo.m_iScrollWidth+61, iMaxParaWidth + 4, 0 );
        m_ParagraphCombo.SetCurSel(0);

        int iWidth = sysInfo.m_iScrollWidth;
        // Allow wider closed-combo-size when screen width is > 640
        // (We can't test pMWContext->bIsComposeWindow, it is not set at this stage)
        if( sysInfo.m_iScreenWidth <= 640 || !(ett & DISPLAY_EDIT_TOOLBAR) ){
            // We are in Message Composer - use fixed width
            iWidth += 80;
        } else {
            // Add width of 1st string so it displays fully in closed combobox
            pDC = m_FontFaceCombo.GetDC();
            if ( pDC ){
                csTemp = XP_GetString(XP_NSFONT_DEFAULT);            
    		    cSize = CIntlWin::GetTextExtent(wincsid, pDC->GetSafeHdc(), csTemp, csTemp.GetLength());
	        	pDC->LPtoDP(&cSize);
                // but never smaller than 90 so other long names show better 
                // in Win16/NT3.51, which can't expand width of dropdown
                iWidth += max(90, cSize.cy);
                m_FontFaceCombo.ReleaseDC(pDC);
            } else {
                iWidth += 90;
            }
        }
	    m_wndCharacterBar.SetComboBox( ID_COMBO_FONTFACE, &m_FontFaceCombo, 
									   iWidth, iMaxFontWidth, 0 );


        m_wndCharacterBar.SetComboBox( ID_COMBO_FONTSIZE, &m_FontSizeCombo, 
									   iFontSizeWidth /*sysInfo.m_iScrollWidth+20*/, 0, 0 );
        m_FontSizeCombo.SetCurSel(2); // Initialize with the "default" size - 3rd in list

        // Set the color combobox data
        m_wndCharacterBar.SetComboBox( ID_COMBO_FONTCOLOR, &m_FontColorCombo, 
                                       sysInfo.m_iScrollWidth+16, 0, 0);
        m_FontColorCombo.SetCurSel(0); // Initialize with default color

        // Make the Alignment and "Insert Object" buttons do their actions on button down
        //  Used to popup a CDropDownToolbar
        m_wndCharacterBar.SetDoOnButtonDown(ID_ALIGN_POPUP, TRUE);
        m_wndCharacterBar.SetDoOnButtonDown(ID_INSERT_POPUP, TRUE);


#ifdef XP_WIN16
        // This will add tooltips for Win16
        m_wndCharacterBar.RecalcLayout();
#endif
    }
    
    int nLastID = ID_EDIT_LAST_ID;
    ASSERT(nLastID < 42000);
	return TRUE;
}

CCommandToolbar* CEditToolBarController::CreateCharacterToolbar(int nCount)
{
	CCommandToolbar *pCharToolbar =new CCommandToolbar(15, theApp.m_pToolbarStyle, 43, 25 /*27*/, 25 /*27*/);

	if (!pCharToolbar->Create(GetParent()))
	{
			return FALSE;
	}

	pCharToolbar->SetBitmap(IDB_CHAR_FORMAT_TOOLBAR);

	CString statusStr, toolTipStr, textStr;
	int nBitmapIndex = 0;

	for(int i = 0; i < nCount; i++)
	{
		CCommandToolbarButton *pCommandButton = new CCommandToolbarButton;

		WFE_ParseButtonString(nIDCharButtonBarArray[i], statusStr, toolTipStr, textStr);

		pCommandButton->Create(pCharToolbar, TB_PICTURES, 
                        CSize(44, 37)/*novice size*/, CSize(25, 25) /*advanced size*/,
						"",(const char*) toolTipStr, (const char*) statusStr,
						IDB_CHAR_FORMAT_TOOLBAR, i, CSize(20,18)/*Bitmap size*/, 
                        nIDCharButtonBarArray[i], -1, (DWORD)0/*button style*/);

		pCommandButton->SetPicturesOnly(TRUE);
		pCommandButton->SetBitmap(pCharToolbar->GetBitmap(), TRUE);

		pCharToolbar->AddButton(pCommandButton, i);
	}


	return pCharToolbar;
}

int CEditToolBarController::GetSelectedFontFaceIndex()
{
    int iSel = m_FontFaceCombo.GetCurSel();
    if( wfe_iFontFaceOtherIndex && iSel == wfe_iFontFaceOtherIndex ){
        return INDEX_OTHER;
    }
    return iSel;
}

int CEditToolBarController::GetSelectedFontColorIndex()
{
    int iSel = m_FontColorCombo.GetCurSel();
    if( m_iFontColorOtherIndex && iSel == m_iFontColorOtherIndex ){
        return INDEX_OTHER;
    }
    return iSel;
}

void CEditToolBarController::ShowToolBar( BOOL bShow, CComboToolBar * pToolBar)
{
	ASSERT(m_pWnd);
	ASSERT(pToolBar);
    CFrameWnd * pFrame = pToolBar->GetParentFrame();

    if ( pFrame != m_pWnd ) {
        // Parent frame isn't toolbar Controller - must be a floating "MiniFrame"
        pFrame->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
    }
	// Do this even if we have mini-frame because we use
	//  the toolbar's show state to tell us current visibility
    pToolBar->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
    pFrame->RecalcLayout();
}

/////////////////////////////////////////////////////////////////////////////
// CEditFrame

#undef new
IMPLEMENT_DYNCREATE(CEditFrame, CMainFrame)
#define new DEBUG_NEW

BEGIN_MESSAGE_MAP(CEditFrame, CMainFrame)
    //{{AFX_MSG_MAP(CEditFrame)
	ON_MESSAGE(WM_TOOLCONTROLLER,OnToolController)
	ON_WM_CREATE()
    ON_WM_INITMENUPOPUP()
	ON_WM_CLOSE()
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_WM_QUERYENDSESSION()
    ON_COMMAND(ID_EDIT_WINDOW_BOOKMARKS, OnShowBookmarkWindow) // Override CMainFrame
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_SETMESSAGESTRING, OnSetMessageString)
    ON_MESSAGE(NSBUTTONMENUOPEN, OnButtonMenuOpen)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
CEditFrame::CEditFrame()
{
    m_pToolBarController = new CEditToolBarController( this );
	m_hPal = NULL;
    m_pTemplateContext = NULL;
}

CEditFrame::~CEditFrame()
{
	delete m_pToolBarController;
}

/////////////////////////////////////////////////////////////////////////////
// CEditFrame diagnostics
 
#ifdef _DEBUG
void CEditFrame::AssertValid() const
{
    CMainFrame::AssertValid();
}

void CEditFrame::Dump(CDumpContext& dc) const
{
    CMainFrame::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CEditFrame member functions
BOOL CEditFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext)
{
	// Call the base - it creates our CWinCX context
	return CMainFrame::OnCreateClient(lpcs, pContext);

    //  We don't need to set the context flag since it is now
    //  set in CMainFrame::OnCreateClient() [framinit.cpp]
    //  because of a SetFocus() call done there, which needs to have
    //  is_editor already set to avoid JavaScript message problems
#if 0
	if(bRetval == TRUE)     {
        CWinCX * pContext = GetMainWinContext();
        ASSERT( pContext );
        // Tell net and stream processors that we are an editor
        MWContext * pMWContext = pContext->GetContext();
        ASSERT( pMWContext );
        pMWContext->is_editor = TRUE;
    }

    return bRetval;
#endif
}

void CEditFrame::DockControlBarLeftOf(CToolBar* Bar,CToolBar* LeftOf)
{
#ifdef XP_WIN32
	CRect rect;
	DWORD dw;
	UINT n;

	// get MFC to adjust the dimensions of all docked ToolBars
	// so that GetWindowRect will be accurate
	RecalcLayout();

	LeftOf->GetWindowRect(&rect);
	rect.OffsetRect(10,10);
	dw=LeftOf->GetBarStyle();
	n = 0;
	n = (dw&CBRS_ALIGN_TOP) ? AFX_IDW_DOCKBAR_TOP : n;
	n = (dw&CBRS_ALIGN_BOTTOM && n==0) ? AFX_IDW_DOCKBAR_BOTTOM : n;
	n = (dw&CBRS_ALIGN_LEFT && n==0) ? AFX_IDW_DOCKBAR_LEFT : n;
	n = (dw&CBRS_ALIGN_RIGHT && n==0) ? AFX_IDW_DOCKBAR_RIGHT : n;

	// When we take the default parameters on rect, DockControlBar will dock
	// each Toolbar on a seperate line.  By calculating a rectangle, we in effect
	// are simulating a Toolbar being dragged to that location and docked.
	DockControlBar(Bar,n,&rect);
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CEditFrame message handlers

int CEditFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	// Most of the work is done in base class
	if (CMainFrame::OnCreate(lpCreateStruct) == -1)
		return -1;

	ASSERT(m_pToolBarController);

	GetChrome()->CreateCustomizableToolbar("Composer"/*ID_COMPOSER*/, 2, FALSE);

    if ( !m_pToolBarController->CreateEditBars(GetMainContext()->GetContext()) )
        return -1;      // fail to create

    // Need to do this to get accurate toolbar size info
	RecalcLayout();

	CControlBarToolbarWindow *pWindow;
	BOOL bOpen, bShowing;
	int32 nPos;

	//putterman: I'm hardcoding because I don't want this translated
	GetChrome()->LoadToolbarConfiguration(IDS_CHAR_TOOLBAR_CAPTION, CString("Formatting_Toolbar"), nPos, bOpen, bShowing);

	pWindow = new CControlBarToolbarWindow(m_pToolBarController->GetCharacterBar(), theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);
	GetChrome()->GetCustomizableToolbar()->AddNewWindow(IDS_CHAR_TOOLBAR_CAPTION, pWindow,nPos, 50, 37, 0, CString(szLoadString(IDS_CHAR_TOOLBAR_CAPTION)),theApp.m_pToolbarStyle, bOpen, FALSE);
	GetChrome()->ShowToolbar(IDS_CHAR_TOOLBAR_CAPTION, bShowing);

    GetChrome()->SetWindowTitle(szLoadString(IDS_NETSCAPE_COMPOSER));
    SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
    
    // Attach the submenus shared by Composer, Message Composer, and right-button popups
    HMENU hMenu = ::GetMenu(m_hWnd);
    if( hMenu )
    {
        HMENU hTableMenu = ::GetSubMenu(hMenu, ED_MENU_TABLE);
        if( hTableMenu )
        {
            ::ModifyMenu( hTableMenu, ID_SELECT_TABLE, MF_BYCOMMAND | MF_POPUP | MF_STRING,
                          (UINT)::LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(IDM_COMPOSER_TABLE_SELECTMENU)),
                          szLoadString(IDS_SUBMENU_SELECT_TABLE) );
            ::ModifyMenu( hTableMenu, ID_INSERT_TABLE, MF_BYCOMMAND | MF_POPUP | MF_STRING,
                          (UINT)::LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(IDM_COMPOSER_TABLE_INSERTMENU)),
                          szLoadString(IDS_SUBMENU_INSERT_TABLE) );
            ::ModifyMenu( hTableMenu, ID_DELETE_TABLE, MF_BYCOMMAND | MF_POPUP | MF_STRING,
                          (UINT)::LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(IDM_COMPOSER_TABLE_DELETEMENU)),
                          szLoadString(IDS_SUBMENU_DELETE_TABLE) );
        }
    }
	return 0;
}

// Global function so we can use it in CComposeFrame as well
// Returns TRUE only if message was supplied
BOOL edt_GetMessageString(CView * pView, UINT MenuID, CString& Message)
{
	if (MenuID >= ID_EDITOR_PLUGINS_BASE && MenuID < (ID_EDITOR_PLUGINS_BASE + MAX_EDITOR_PLUGINS))
	{
		uint32 CategoryId, PluginId;
		CNetscapeEditView *pEditView = (CNetscapeEditView *)pView;
		ASSERT(pView != NULL && pView->IsKindOf(RUNTIME_CLASS(CNetscapeEditView)));

		if (pEditView->GetPluginInfo(MenuID, &CategoryId, &PluginId))
			Message = EDT_GetPluginMenuHelp(CategoryId, PluginId);
        return TRUE;
	}
	else if (MenuID >= ID_EDIT_HISTORY_BASE && MenuID < (ID_EDIT_HISTORY_BASE + MAX_EDIT_HISTORY_LOCATIONS) )
    {
        char * pStatusTitle = NULL;
        // Get the string for Page Title associated with menu item (which shows the URL)
        //  (NULL means we don't need to get the URL string)
        if( EDT_GetEditHistory( ((CGenericView*)pView)->GetContext()->GetContext(), 
                                MenuID-ID_EDIT_HISTORY_BASE, NULL, &pStatusTitle) )
        {
            Message = pStatusTitle;
            return TRUE;
        }
    }
    return FALSE;
}

// Helper to test for dynamic menus shared by Page and Message Composer frame windows
BOOL edt_IsEditorDynamicMenu(WPARAM wParam)
{
    return( (wParam >= ID_EDITOR_PLUGINS_BASE && wParam < (ID_EDITOR_PLUGINS_BASE + MAX_EDITOR_PLUGINS)) ||
            (wParam >= ID_EDIT_HISTORY_BASE && wParam < (ID_EDIT_HISTORY_BASE + MAX_EDIT_HISTORY_LOCATIONS)) );
}

// GetMessageString - Override of CFrameWnd virtual. It sets the menu help strings for the 
// dynamically loaded editor plugins.
void CEditFrame::GetMessageString(UINT MenuID, CString& Message) const
{
    if( edt_GetMessageString(GetActiveView(), MenuID, Message) )
        return;

    CMainFrame::GetMessageString(MenuID, Message);
}

// OnSetMessageString - Override of CMainFrame's WM_SETMESSAGESTRING message handler. We need this 
// because the base class (actually CGenericFrame) is supressing the dynamically created menu help
// strings for Composer
LRESULT CEditFrame::OnSetMessageString(WPARAM wParam, LPARAM lParam)
{
	if( edt_IsEditorDynamicMenu(wParam) )
		return CFrameWnd::OnSetMessageString(wParam, lParam);
    else
		return CMainFrame::OnSetMessageString(wParam, lParam);
}

LONG CEditFrame::OnToolController(UINT,LONG)
{
	return (LONG)m_pToolBarController;
}

void CEditFrame::OnShowBookmarkWindow()
{
	XP_Bool prefBool;
	PREF_GetBoolPref("editor.hints.bookmark",&prefBool);

    if ( prefBool ){
        // Show a "Hint" dialog about drag and drop until
        //   user checks "Don't show this" in dialog
        CEditHintDlg dlg(this, IDS_DRAG_BOOKMARK_HINT);

        dlg.DoModal();
        if ( dlg.m_bDontShowAgain ) {
			PREF_SetBoolPref("editor.hints.bookmark",FALSE);
        }
    }
    CGenericFrame::OnShowBookmarkWindow();
}


// Handy dandy routine to get a preference and prompt
//  user to set it in Editor Preferences if it does
//  not already exist
char* CEditFrame::GetLocationFromPreferences(const char *pPrefName, UINT nID_Msg, UINT nID_Caption, UINT nID_FileCaption)
{
    if( pPrefName == NULL ){
        return NULL;
    }

    char *pLocation = NULL;
	PREF_CopyCharPref(pPrefName, &pLocation);

    if( pLocation == NULL || pLocation[0] == '\0' ){
        // Popup dialog to allow user to set location now
        CGetLocationDlg dlg(this, nID_Msg, nID_Caption, nID_FileCaption);
        if(dlg.DoModal() == IDOK){
            if( !dlg.m_csLocation.IsEmpty() ){
                pLocation = XP_STRDUP((char*)LPCSTR(dlg.m_csLocation));
                PREF_SetCharPref(pPrefName, pLocation);
            }
        }
    }

    if( pLocation && pLocation[0] == '\0' ){
        XP_FREE(pLocation);
        return NULL;
    }

    return pLocation;
}

void CEditFrame::OnClose() 
{
    MWContext *pMWContext;
    if( GetMainContext() == NULL ||
        (pMWContext = GetMainContext()->GetContext()) == NULL )
        return;

    // Stop any active plugin
    if (!CheckAndCloseEditorPlugin(pMWContext)) 
        return;

     // Ignore close if we are doing something
    if(pMWContext->edit_saving_url ||
       pMWContext->waitingMode ||
       (EDT_IsBlocked(pMWContext) &&LO_GetEDBuffer(pMWContext))//if we dont have an edit buffer, we should not be blocked from closing!
        ) {
        return;
    }
    
    // The CGenericFrame flag should be respected only for
    //  one attempt to close, so clear it here
    BOOL bPromptForSaving = !m_bSkipSaveEditChanges;
    m_bSkipSaveEditChanges = FALSE;
    
    // Check for changes to doc and prompt to save:    
    // except if we are a new document AND nothing was ever edited,
    //   don't prompt to save it
    // Don't close if user cancels when prompted to save
    if( LO_GetEDBuffer( ABSTRACTCX(pMWContext)->GetDocumentContext() ) ){
        if( bPromptForSaving && 
	        !FE_CheckAndSaveDocument(pMWContext) ){
	        return;
        }
    		History_entry * hist_entry = SHIST_GetCurrent(&(pMWContext ->hist));
        EDT_PreClose(pMWContext,hist_entry ? hist_entry->address: NULL,
                         RealCloseS,(void *)this);
        return;
    }
  RealClose();
}

void CEditFrame::RealCloseS(void* hook) {
  CEditFrame *pEdFrame = (CEditFrame *)hook;
  if (!pEdFrame) {
    ASSERT(0);
    return;
  }
  pEdFrame->RealClose();  
}

void CEditFrame::RealClose() {
  MWContext *pMWContext;
  if( GetMainContext() == NULL ||
      (pMWContext = GetMainContext()->GetContext()) == NULL )
    return;

  if( LO_GetEDBuffer( ABSTRACTCX(pMWContext)->GetDocumentContext() ) ){
    EDT_DestroyEditBuffer(pMWContext);
  }

  //I'm hardcoding because I don't want this translated
  GetChrome()->SaveToolbarConfiguration(IDS_EDIT_TOOLBAR_CAPTION, CString("Composition_Toolbar"));
  GetChrome()->SaveToolbarConfiguration(IDS_CHAR_TOOLBAR_CAPTION, CString("Formatting_Toolbar"));

  CMainFrame::OnClose();  
}

void CEditFrame::OnFileClose() 
{
    MWContext *pMWContext;
    if( GetMainContext() == NULL ||
	(pMWContext = GetMainContext()->GetContext()) == NULL )
	return;

    // Stop any active plugin
    if (!CheckAndCloseEditorPlugin(pMWContext)) 
        return;

    if ( FE_CheckAndSaveDocument(pMWContext) ) {
        m_bSkipSaveEditChanges = TRUE;
	    CMainFrame::OnFrameExit();
    }
}

BOOL CEditFrame::OnQueryEndSession() 
{
	if (!CMainFrame::OnQueryEndSession())
		return FALSE;
	
    MWContext *pMWContext;
    if( GetMainContext() == NULL ||
	(pMWContext = GetMainContext()->GetContext()) == NULL )
	return TRUE;

    // Returns FALSE if user cancels from dialog - stops shutdown
    return FE_CheckAndSaveDocument(pMWContext);
}

// These are Frame load/creation routines for editor, formerly in GENFRAME.CPP

// Get pointer to URL passed from 2nd instance
char* GetGlobalUrlPointer(HANDLE handle) {
    char * pUrl = NULL;
#ifdef WIN32
    // Read URL from memory-mapped file and load it
    handle = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, TRUE, szNGMemoryMapFilename);

    if (NULL != handle) {
        char * szFile = (char*)MapViewOfFile(handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
        if (szFile ) {
	        pUrl = XP_STRDUP(szFile);
        } else {
	        TRACE1( "OpenFileMapping error: %u\n", GetLastError() );
        }
        CloseHandle(handle);
    }

#else
    pUrl = (char*)GlobalLock(handle);
#endif
    return pUrl;
}

// Save the last-use template URL
//  to our list of recently-accessed templates
//  Call from FE_EditorGetUrlExitRoutine after template successfully loaded
void  CEditFrame::SaveTemplateLocation(char *pLastLoc)
{
	int iToBeMoved = -1;
	char szLocName[32];
	char szLocation[256];
	int iLen = 256;
    char * pLastLocation = NULL;
    int i;

    if( pLastLoc ){
        pLastLocation = XP_STRDUP(pLastLoc);
    } else {
	    PREF_CopyCharPref("editor.template_last_loc",&pLastLocation);
    }
    if( !pLastLocation ){
        return;
    }

    // First scan the list to find pref that matches new item
    for( i = 0; i < theApp.m_iTemplateLocationCount; i++ ){
		wsprintf(szLocName,"editor.template_history_%d",i);
		PREF_GetCharPref(szLocName,szLocation,&iLen);
		
        if( 0 == strcmp(szLocation,pLastLocation) ){
            // URL is already in the list
			if (i ==0) 
                // It is already at the start of list,
                // so there's nothing more to do!
				return;

            iToBeMoved = i;
            break;
        }
    }

    // If our list was already at the limit, we will delete the oldest item
    if( theApp.m_iTemplateLocationCount >= MAX_TEMPLATE_LOCATIONS ){
        theApp.m_iTemplateLocationCount = MAX_TEMPLATE_LOCATIONS;
    } else if( iToBeMoved == -1 ){
        // We will be adding 1 new item        
        theApp.m_iTemplateLocationCount++;
    }

    // Start at 1 less than new count or item to-be-deleted
	// that is, start at the number to be replaced after
	// adjusting for the zero index
	int iStart;
    if (iToBeMoved > 0){
        iStart = iToBeMoved;
    } else {
        iStart = theApp.m_iTemplateLocationCount-1;
    }
	
    for( i = iStart; i > 0; i-- ){
		// iterate backwards and move everything up 1 position
		wsprintf(szLocName, "editor.template_history_%d", i-1);
		PREF_GetCharPref(szLocName, szLocation, &iLen);

		wsprintf(szLocName, "editor.template_history_%d", i);

		PREF_SetCharPref(szLocName,szLocation);
    }

    // Add the new item to list
	wsprintf(szLocName, "editor.template_history_%d", 0);

	PREF_SetCharPref(szLocName, pLastLocation);

	if (pLastLocation) XP_FREE(pLastLocation);
}

// Route toolbar menu messages to edit view
LRESULT CEditFrame::OnButtonMenuOpen(WPARAM wParam, LPARAM lParam)
{
    if( GetMainWinContext()){
        return ::SendMessage( GetMainWinContext()->GetPane(), NSBUTTONMENUOPEN, wParam, lParam);
    }
    return 0;
}


//////////////////////////////////////////////////////////////////////
// Frame load/creation methods
//      Prototypes of CGenericFrame functions are in GENFRAME.H
//
int FindCurrentUrlInHistory(MWContext * pMWcontext, char * szAddress)
{
	if(!pMWcontext)
        return 0;

	XP_List * list_ptr;
	History_entry *pEntry;
    int nIndex = 0;
	list_ptr = pMWcontext->hist.list_ptr;
	
	while((pEntry = (History_entry *) XP_ListNextObject(list_ptr))!=0) {
        nIndex++;
        if (0 == XP_STRCMP(pEntry->address, szAddress )) {
	        return nIndex;
        }
    }
    // If no match found:
    return 0;
}

// Use this to activate a Navigator or Editor frame instead 
//   of creating a new one.
MWContext * FE_ActivateFrameWithURL(char * pUrl, BOOL bFindEditor)
{
    if ( pUrl == NULL ) {
        return NULL;
    }

    for(CGenericFrame * f = theApp.m_pFrameList; f; f = f->m_pNext) {
        MWContext *pMWContext = f->GetMainContext()->GetContext();

        if ( pMWContext && 
	         (bFindEditor && EDT_IS_EDITOR(pMWContext)) ||
	         (!bFindEditor && !EDT_IS_EDITOR(pMWContext)) ) {
	        History_entry * pEntry = SHIST_GetCurrent(&pMWContext->hist);
	        if( pEntry && pEntry->address && EDT_IsSameURL(pEntry->address, pUrl,0,0) )
            {
                if ( f->IsIconic() ){
	                f->ShowWindow(SW_RESTORE);
                }
                f->SetActiveWindow();
                // Don't close the frame
                f->m_bCloseFrame = FALSE;
                return pMWContext;
	        }
        }
    }
    return NULL;
}

// Use this to activate a Navigator frame with supplied URL,
//   or the frame that is previous in app's frame list,
//   or create a new frame if no other frames exist
//   Use when we can't edit a file -- not found or not editable type
void FE_RevertToPreviousFrame(char * pUrl, MWContext *pMWContext)
{
    CGenericFrame *pFrame = (CGenericFrame*)GetFrame(pMWContext);
    if( !pFrame ){
        return;
    }    
    
    // If we were loading into an untouched new doc,
    //   the only case when we DON'T create a new frame first,
    //   then stay in that doc -- don't close it
    if( EDT_IS_EDITOR(pMWContext) && EDT_IS_NEW_DOCUMENT(pMWContext) && !EDT_DirtyFlag(pMWContext) ){
        // If ESC key is used to cancel load URL, focus needs to be set
        pFrame->GetActiveView()->SetFocus();
        return;
    }

    // Find the browser with the same URL    
    if( FE_ActivateFrameWithURL(pUrl, FALSE) ){
        // Close caller's frame
        pFrame->PostMessage(WM_CLOSE);
        return;
    }

    if( wfe_pLastFrame ){
        if ( wfe_pLastFrame->IsIconic() ){
	        // Bloody unlikely!
	        wfe_pLastFrame->ShowWindow(SW_RESTORE);
        }
        wfe_pLastFrame->m_bCloseFrame = FALSE;
        wfe_pLastFrame->SetActiveWindow();
    } else {
        // No existing frame,
        //  create a new browser at same location
        //  and load home page
        wfe_bUseLastFrameLocation = TRUE;
        MWContext *pNavContext = CFE_CreateNewDocWindow(NULL, NULL);
        if( pNavContext ){
	        ((CMainFrame*)GetFrame(pNavContext))->OnLoadHomePage();
        }
    }
    // Be sure this is clear so we don't get prompted to save bogus empty document
    EDT_SetDirtyFlag(pMWContext, FALSE);

    // Close caller's frame
    pFrame->PostMessage(WM_CLOSE);
}

void FE_EditorGetUrlExitRoutine(URL_Struct *pUrlStruct, int iStatus, MWContext *pMWContext)
{
    // Error status is negative, but don't close
    //  if error is from publishing upload
    if( EDT_IS_EDITOR(pMWContext) && !pMWContext->bIsComposeWindow ) {
        CEditFrame * pFrame = (CEditFrame*)GetFrame(pMWContext);
        if(iStatus < 0 ){
	        //Enable window if we have an error -- it is disabled in CEditFrame::OnCreate()
            // WE DON'T DISABLE WINDOW ANY MORE
            //pFrame->GetActiveView()->EnableWindow(TRUE);
            
            // Be sure this is cleared, else next load will think
            //   we want a template. This is used by FE_EditorDocumentLoaded
            //   to convert a template doc into a new doc
            pFrame->m_pTemplateContext = NULL;

	        // Default is to try to load the failed address
	        char *pAddress = pUrlStruct->address;
	        
	        if( pUrlStruct->files_to_post == NULL ){
                // we failed to to load a document into an editor context
                // Try to find frame with our URL,
                //   or the previous frame in list,
                //   or create a new browser frame
                // (also closes current edit frame)
                //
                // But first check if we failed to load the URL
                // Save preference
                int iTemp = theApp.m_nChangeHomePage;
                // These are (dynamically?) defined. See allxpstr.h
                //  TODO: We should probably check a whole range of values
                if( iStatus == MK_UNABLE_TO_LOCATE_FILE
	                || iStatus == MK_MALFORMED_URL_ERROR ){
	                // This will prevent trying to load 
	                //   a bad startup URL on the command line
	                theApp.m_nChangeHomePage = FALSE;
	                // If we didn't find URL at startup,
	                //  then don't try to load it in Browser either
	                if( iStatus == -209 ){
                        pAddress = NULL;
	                }
                }
                // Close current window and go back to last active window
                FE_RevertToPreviousFrame(pAddress, pMWContext);
                theApp.m_nChangeHomePage = iTemp;
	        }
        }
    }

#ifdef XP_WIN32
    // Do this even if not an edit context
    if ( iStatus == MK_DATA_LOADED && pMWContext->type == MWContextSaveToDisk &&
	 pMWContext->save_as_name ){
        // Notify site manager that a file was saved
        // CString csUrlFileName; 
        // Convert local file to URL format
        // WFE_ConvertFile2Url(csUrlFileName, (const char*)pMWContext->save_as_name);
        // For now, give local name to SiteManager:
        if ( bSiteMgrIsActive ) {
            pITalkSMClient->SavedURL(pMWContext->save_as_name);
        }
    }
#endif // XP_WIN32
}

// Editor calls us when we are finished loading
void FE_EditorDocumentLoaded(MWContext* pMWContext)
{
    if ( ! pMWContext ) {
        return;
    }
    CEditFrame* pFrame = (CEditFrame*)GetFrame(pMWContext);

    // Clear cached pointers to old elements
    CWinCX * pContext = WINCX(pMWContext);
    if(pContext){
        pContext->ClearLastElement();
    }

	History_entry * hist_ent = SHIST_GetCurrent(&pMWContext->hist);
    ASSERT(hist_ent);
    ASSERT(hist_ent->address);
    if( !hist_ent || !hist_ent->address ){
        return;
    }

    // Tell edit core the file Auto Save preference
	int32 iSave;
	PREF_GetIntPref("editor.auto_save_delay",&iSave);
    EDT_SetAutoSavePeriod(pMWContext, iSave );

    // Close the caller's window if marked to do so
    if( wfe_pLastFrame && wfe_pLastFrame->m_bCloseFrame){
        wfe_pLastFrame->PostMessage(WM_CLOSE);
    }

    if( pMWContext == pFrame->m_pTemplateContext )
    {
        // We loaded a URL (template or text file)
        //  that we want to convert to a new doc

        // Save in history
        pFrame->SaveTemplateLocation(hist_ent->address);

        EDT_ConvertCurrentDocToNewDoc(pMWContext);
        pFrame->m_pTemplateContext = NULL;
    }

    // This is really needed only for Win16 -- to set initial focus,
    //   but it shouldn't hurt to do this all the time
    if( pFrame->IsKindOf(RUNTIME_CLASS(CEditFrame)) ) {
        pFrame->PostMessage(WM_SETFOCUS, 0, 0);
    }

#ifdef MOZ_MAIL_NEWS
    if (pMWContext->type == MWContextMessageComposition)
    {
        CGenericFrame * pFrame = wfe_FrameFromXPContext(pMWContext);
        if (pFrame)
        {
	        CComposeFrame * pCompose = (CComposeFrame*)pFrame;
	        if (pCompose->UseHtml())
			{
				if (!pCompose->Initialized())
					pCompose->InsertInitialText();
				else {
					int32 startOffset = pCompose->GetQuoteSel();
					if (startOffset != -1) {
						int32 endOffset = startOffset;
						int32 eReplyOnTop = 0;
						if (PREF_NOERROR ==
								PREF_GetIntPref("mailnews.reply_on_top", &eReplyOnTop))
							{
								switch (eReplyOnTop) {
								case 1:
								default:
									EDT_SetInsertPointToOffset(pMWContext, startOffset, 0);
									break;
								case 2:
								case 3:
									endOffset = EDT_GetInsertPointOffset(pMWContext);
									EDT_SetInsertPointToOffset( pMWContext, startOffset, 
																endOffset - startOffset);
									break;
								}
							}
						pCompose->SetQuoteSel(-1);
					}
				}
	
			} // UseHtml()
        } // pFrame
    } // MWContextMessageComposition
#endif // MOZ_MAIL_NEWS
}

void FE_FinishedRelayout(MWContext * pMWContext)
{
    // Clear cached pointers to old elements
    if( pMWContext ){
        CWinCX * pContext = WINCX(pMWContext);
        if(pContext){
            pContext->ClearLastElement();
        }
    }
}


//////////////////////////////////////////////////////////////////////////
enum {
    ED_LOAD_CURRENT_PAGE,
    ED_LOAD_CURRENT_FRAME,
    ED_LOAD_NEW_PAGE,
    ED_LOAD_TEMPLATE
};

typedef struct _EDT_LoadUrlData {
    int             iStyle;
    BOOL            bNewWindow;
    CGenericFrame * pFrame;
    MWContext     * pCopyHistoryContext;
} EDT_LoadUrlData;


// Access function for calling from outside to Edit or Navigate
// Used by LiveWire SiteManager, DDE, OLE, and JAVA interfaces
void FE_LoadUrl( char *pUrl, BOOL bEdit )
{
    if ( *pUrl == '\0' ){
        // An empty string is same as none
        // Will load new blank page if bEdit is TRUE,
        //   else load HomePage into browser
        pUrl = NULL;
    }

    CString csUrlFileName;
    if ( pUrl && NET_URL_Type(pUrl) == 0 ) {
        // Assume "file:///" if no URL type supplied
        WFE_ConvertFile2Url(csUrlFileName, (const char*)pUrl);
        pUrl = (char*)LPCSTR(csUrlFileName);
    }

    // Get the last active browser or editor frame
    // It doesn't matter which frame's function we call
    //  since we are assured of having a URL and don't need history or other data. 
    CGenericFrame* pFrame = (CGenericFrame*)FEU_GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
    
    // Default is to start a new editor frame
    BOOL bNewEditor = TRUE;
    
    MWContext * pMWContext = NULL;

    if( pFrame ){
        if( pFrame->GetMainContext() ){
            pMWContext = pFrame->GetMainContext()->GetContext();
        }

        // If any frame already exists, then create a new editor in LoadUrlEditor
        //   but if we are loading from an empty/untouched new document, then use that frame instead
        if( pMWContext && EDT_IS_NEW_DOCUMENT(pMWContext) && !EDT_DirtyFlag(pMWContext) ){
            bNewEditor = FALSE;
        }
    } else {
        // We end up here if launched from OLE automation, e.g., Sitemanager
        if( bEdit ){
            theApp.m_EditTmplate->OpenDocumentFile(NULL);
            pFrame = (CGenericFrame*)FEU_GetLastActiveFrame(MWContextBrowser, FEU_FINDEDITORONLY);
            // We don't need a new frame -- load the URL into the frame just created
            bNewEditor = FALSE;
        } else {
            theApp.m_ViewTmplate->OpenDocumentFile(NULL);
			pFrame = (CGenericFrame*)FEU_GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERONLY);
        }
    }
    if( pFrame ){
        if( !pMWContext && pFrame->GetMainContext() ){
            pMWContext = pFrame->GetMainContext()->GetContext();
        }
        if ( bEdit ) {
            // This will look for exisiting window AFTER
            //   passing through EDT_PreOpen() plugin hook
            pFrame->LoadUrlEditor( pUrl, bNewEditor, pUrl ? ED_LOAD_CURRENT_PAGE : ED_LOAD_NEW_PAGE );
        } else if( !FE_ActivateFrameWithURL(pUrl, FALSE) ) {
            // We didn't find a Browser with that URL - find or create a Browser
            if( !pFrame->GetMainContext() || EDT_IS_EDITOR(pMWContext) ){
                // The last active frame was an editor - we need a Browser
                pFrame = (CGenericFrame*)FEU_GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERONLY);
            }
            if( pFrame ){
                // Bring this page to the foreground
                if ( pFrame->IsIconic() ){
	                pFrame->ShowWindow(SW_RESTORE);
                }
                pFrame->SetActiveWindow();
                // Load into existing Browser
                pFrame->GetMainContext()->NormalGetUrl(pUrl);
            } else {
                // Start a new Browser frame with supplied URL
                wfe_CreateNavigator(pUrl);
            }
        }
    } else {
        //VERY BAD TO BE HERE - EXIT APP!
        ::MessageBox(0, "Failed to create new window\nfrom OLE Automation launch", 0, 0);
        // We know there are no visible windows to ask user about,
        //  so just exit
        theApp.CommonAppExit();
    }
}

void FE_SetNewDocumentProperties(MWContext * pMWContext)
{
    EDT_PageData *pPageData = EDT_GetPageData(pMWContext);
    if ( pPageData == NULL ){
        return;
    }

    // Copy background image if not in same directory as document
	int bKeepImages;
	PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);

    pPageData->bKeepImagesWithDoc = bKeepImages;

    // Get editor colors if using custom colors
	XP_Bool prefBool;
	PREF_GetBoolPref("editor.use_custom_colors",&prefBool);
    if( prefBool ) {
		COLORREF clr;
		
		PREF_GetColorPrefDWord("editor.text_color",&clr);
        WFE_SetLO_ColorPtr(clr,&pPageData->pColorText);
        
		PREF_GetColorPrefDWord("editor.link_color",&clr);
		WFE_SetLO_ColorPtr(clr,&pPageData->pColorLink);

		PREF_GetColorPrefDWord("editor.active_link_color",&clr);
        WFE_SetLO_ColorPtr(clr,&pPageData->pColorActiveLink);

		PREF_GetColorPrefDWord("editor.followed_link_color",&clr);
        WFE_SetLO_ColorPtr(clr,&pPageData->pColorFollowedLink);

        // Set the background color even if we use an image TODO: CHECK IF THIS IS SOURCE OF BUG
		PREF_GetColorPrefDWord("editor.background_color",&clr);
        WFE_SetLO_ColorPtr(clr, &pPageData->pColorBackground);
    }
	char * szBack = NULL;
	PREF_CopyCharPref("editor.background_image",&szBack);
	XP_Bool bBack;
	PREF_GetBoolPref("editor.use_background_image",&bBack);
    // Background image preference is independent from color preferences
    if( bBack && szBack ){
        pPageData->pBackgroundImage = XP_STRDUP(szBack);
		XP_FREE(szBack);
    }
  
    EDT_SetPageData(pMWContext, pPageData);
    EDT_FreePageData(pPageData);

    // Add fixed MetaData items:
    EDT_MetaData *pMetaData = EDT_NewMetaData();
    if ( pMetaData == NULL ){
        return;
    }
    
    //What generated this document:
    pMetaData->bHttpEquiv = FALSE;

    pMetaData->pName = XP_STRDUP("Author");

	PREF_CopyCharPref("editor.author",&(pMetaData->pContent));    

    EDT_SetMetaData(pMWContext, pMetaData);     
    EDT_FreeMetaData(pMetaData);

    // Force entire view to be painted 
    // (if not, a browser background image leaks through at top, 
    //  above caret on first line)
    ::InvalidateRect( (PANECX(pMWContext))->GetPane(), NULL, TRUE);
} 

// From the Composer button on the Taskbar:
void CGenericFrame::OnOpenComposerWindow()
{
	CAbstractCX *pCX = GetMainContext();
	if(pCX != NULL)
	{
		int nCount = FEU_GetNumActiveFrames(MWContextBrowser, FEU_FINDEDITORONLY);

		if((nCount == 1 && (pCX->GetContext()->type == MWContextBrowser && EDT_IS_EDITOR(pCX->GetContext()))) ||
			nCount == 0){
			// If we didn't find an editor, start a new one
			OnEditNewBlankDocument();
		}
		else if(nCount > 1 || !(pCX->GetContext()->type == MWContextBrowser && EDT_IS_EDITOR(pCX->GetContext())))
		{
			CFrameWnd *pFrame;

			//if we are an editor then get the bottom most editor
			if(pCX->GetContext()->type == MWContextBrowser && EDT_IS_EDITOR(pCX->GetContext()))
			{
				pFrame = FEU_GetBottomFrame(MWContextBrowser, FEU_FINDEDITORONLY);
			}
			else
			// if we are not an editor, then get the last active editor
				pFrame = FEU_GetLastActiveFrame(MWContextBrowser, FEU_FINDEDITORONLY);

			if(pFrame){

				if(pFrame->IsIconic())
		            pFrame->ShowWindow(SW_RESTORE);

#ifdef _WIN32
				pFrame->SetForegroundWindow();
#else	
				pFrame->SetActiveWindow();
#endif
			}
		}

	}
}

static void FinishLoadUrlEditor(XP_Bool bUserCanceled, char* pUrl, void* user_data)
{
    ASSERT(user_data);
    if( !user_data){
        return;
    }

    EDT_LoadUrlData * pData = (EDT_LoadUrlData*)user_data;
    if( bUserCanceled ){
        // Note: We usually create a new Compose window below,
        //       except when we need to create one in FE_LoadUrl
        //         because no Browser or Composer windows existed
        //         before trying to start Composer.
        //       If this window was already created,
        //         create a Navigator with the URL instead
        //         and close the Composer window
        if( !pData->bNewWindow && pData->pFrame ){
            wfe_CreateNavigator(pUrl);
            pData->pFrame->PostMessage(WM_CLOSE,0,0);
        }
        return;
    }


    // Use the MAIN context for frameset URL or if active context is not found
    CWinCX *pWinContext = NULL;
    if( pData->iStyle == ED_LOAD_CURRENT_FRAME ||
        !(pWinContext = pData->pFrame->GetActiveWinContext()) ){
        pWinContext = pData->pFrame->GetMainWinContext();
    }

	URL_Struct *pUrlStruct = NULL;
    History_entry * pEntry = NULL;
    if( pWinContext ){
        pEntry = SHIST_GetCurrent(&(pWinContext->GetContext()->hist));
    }

    // An empty string is same as blank
    if( pUrl && *pUrl == '\0' ){
        pUrl = NULL;
    }
    BOOL bNewDocument = (pData->iStyle == ED_LOAD_NEW_PAGE) || (pUrl == NULL);

    if( !bNewDocument ){
        // Try to activate an existing window EXCEPT if loading a template
        if( pUrl && pData->iStyle != ED_LOAD_TEMPLATE &&
            FE_ActivateFrameWithURL( pUrl, TRUE ) ){
            // We found existing window - we're done
            delete pData;
            return;
        }
        // Important! We need to detect when URL to load is same as current history's URL
        //   so we create the URL Struct from the appropriate context
        if ( pUrl == NULL || 
             (pEntry && pEntry->address && !strcmp(pEntry->address, pUrl)) )
        {
	        // Create a new URL struct using the data in current page's context
	        if(pEntry){
	            pUrlStruct = SHIST_CreateURLStructFromHistoryEntry(pWinContext->GetContext(), pEntry);
                // Above defaults to NET_DONT_RELOAD. 
                // We need NET_NORMAL_RELOAD to be sure we do normal server checks 
                //   to see if doc changed at original source (fix for bug 80606)
                pUrlStruct->force_reload = NET_NORMAL_RELOAD;
	        } else {
                // Unlikely event - no current history entry
	            bNewDocument = TRUE;
	        }
        } else {
	        // Create a new URL structures and copy URL string to it
	        pUrlStruct = NET_CreateURLStruct(pUrl, NET_NORMAL_RELOAD);
        }
    }

    if ( pUrlStruct ) {
        // Must clear this to correctly load URL
	    pUrlStruct->fe_data = NULL;
		//      If this structure changes in the future to hold data which can be carried
		//              across contexts, then we lose.
		memset((void *)&(pUrlStruct->savedData), 0, sizeof(SHIST_SavedData));
    }

    MWContext * pEditContext = NULL;
    if( pData->bNewWindow ){
        // Most uses create a new context, frame, view set and load the URL if supplied
        pEditContext = FE_CreateNewEditWindow(NULL, pUrlStruct );
    } else {
        // Or load URL into an existing window only if its already an editor
        if( EDT_IS_EDITOR(pWinContext->GetContext()) ){
            pEditContext = pWinContext->GetContext();
        }
        if( !bNewDocument ){
            // If Composer frame was created at startup, 
            //  we end up here to load URL from the command line
            // Suppress the "-embedding" string we get from OLE launch
            if( 0 == strcmpi(pUrl, "-embedding") ){
                bNewDocument = TRUE;
            } else {
                pWinContext->NormalGetUrl(pUrl);
            }
        }
    }

    if( pEditContext ){
        if ( bNewDocument ) {
            // Start a new document - Get the view from the newly-created edit context
            // Start new doc with URL = "about:editfilenew"
            WINCX(pEditContext)->NormalGetUrl(EDT_NEW_DOC_URL);
        } else {
            if( pData->iStyle == ED_LOAD_TEMPLATE ){
    
                // This will trigger changing to new doc name and URL
                //   in Edit FE_EditorGetUrlExitRoutine.
                ((CEditFrame*)GetFrame(pEditContext))->m_pTemplateContext = pEditContext;
            }

            // Copy the history of the "caller" browser window if it was set
            // Note, after LoadUrlEditor, if user closes the Navigator window
            //  before canceling out of a "Can't Edit URL" prompt, 
            //  pNavContext will be BAD, so check if context  is in the list
            if ( pData->pCopyHistoryContext && 
                 XP_IsContextInList(pData->pCopyHistoryContext) &&
                 pData->pCopyHistoryContext != pEditContext ) {
                // Copy History from current window to Edit context
                // (This sets current history index to 1)
                SHIST_CopySession(pEditContext, pData->pCopyHistoryContext);

                //      Set the current session history index for the new context
                //   by finding same location of old address in new context
                //  If there is not pCurrentEntry, then browser had no history
                //   and we got a new doc when editor was created: index = 1
                int nIndex = 1;
                if ( pEntry && pEntry->address != NULL ) {
	                nIndex = FindCurrentUrlInHistory(pEditContext, pEntry->address );
                }
                SHIST_SetCurrent(&(pEditContext->hist), nIndex );
            }
        }
    }
    delete pData;
}

void CGenericFrame::LoadUrlEditor(char * pUrl,
                                  BOOL bNewWindow,
                                  int iLoadStyle, 
                                  MWContext * pCopyHistoryContext)
{
    EDT_LoadUrlData * pData = new EDT_LoadUrlData;
    if( pData ){
        pData->pFrame = this;
        pData->iStyle = iLoadStyle;
        // We MUST create a new window if current window is not an editor
        pData->bNewWindow = EDT_IS_EDITOR(GetMainContext()->GetContext()) ? bNewWindow : TRUE;
        
        // We will copy history from a source browser context ONLY if creating a new window
        pData->pCopyHistoryContext = pData->bNewWindow ? pCopyHistoryContext : NULL;
        
        if( pUrl && *pUrl){
            // Be sure we have URL format by now
            CString csURL;
            WFE_ConvertFile2Url(csURL, pUrl);

            // Call Editor Plugin to possibly replace the supplied URL
            //   or do source file locking
            
            //EDT_PreOpen(GetMainContext()->GetContext(), (char*)(LPCSTR(csURL)), &FinishLoadUrlEditor, pData);
            //TODO: REMOVE THIS - TEMP - SKIP PLUGIN TO AVOID NSPR20 PROBLEMS????
            FinishLoadUrlEditor(FALSE, (char*)(LPCSTR(csURL)), pData);
        } else {
            // No URL address given - Don't go through plugin
            FinishLoadUrlEditor(FALSE, NULL, pData);
        }
    }
}

// Start New document in a new window
// This is used by both Browser and Editor
void CGenericFrame::OnEditNewBlankDocument()
{
    if ( GetMainContext() == NULL || GetMainContext()->GetContext() == NULL ) {
        return;
    }
    // Create a new window with a new blank page
    LoadUrlEditor(NULL, TRUE, ED_LOAD_NEW_PAGE);
}

// Open a Editor window with current URL from a browser window
void CGenericFrame::OnNavigateToEdit()
{
    CWinCX *pContext = GetMainWinContext();

    if( !pContext || !pContext->GetContext())
        return;

    OpenEditorWindow(ED_LOAD_CURRENT_PAGE);
}

void CGenericFrame::OnEditFrame()
{
    CWinCX *pContext = GetActiveWinContext();

    if( !pContext || !pContext->GetContext())
        return;
    // This style really doesn't do anything --
    //   we will look for active frame no matter
    //   what style is if not ED_LOAD_CURRENT_PAGE
    OpenEditorWindow(ED_LOAD_CURRENT_FRAME);
}

void CGenericFrame::OpenEditorWindow(int iLoadStyle)
{
    CWinCX *pWinContext = NULL;
    
    // If editing a frameset, or no active window, use top-level (frameset) URL
    if( iLoadStyle == ED_LOAD_CURRENT_PAGE ||
        !(pWinContext = GetActiveWinContext()) ){
        pWinContext = GetMainWinContext();
    }
    if( !pWinContext ){
        return;
    }
    MWContext * pCurrentContext = pWinContext->GetContext();
    if(!pCurrentContext){
        return;
    }

    History_entry * pEntry = SHIST_GetCurrent(&(pWinContext->GetContext()->hist));
    // Get current address from history and load that into editor
    // Pass pCurrentContext to signal FinishLoadUrlEditor to copy history from this context
    LoadUrlEditor((pEntry && pEntry->address) ? pEntry->address : NULL,
                  TRUE, iLoadStyle, pCurrentContext);
}

// Called by other apps via registered message
// TODO: TEST IF THESE ARE USED ANY MORE -- LIVEWIRE???
LRESULT CGenericFrame::OnOpenEditor(WPARAM wParam, LPARAM lParam )
{
    HANDLE handle = NULL;
#ifndef WIN32
    // We get data passed to us if Win16
    handle = (HANDLE)wParam;
#endif
    char *pUrl = GetGlobalUrlPointer(handle);
    
	// In Win32, handle wil be obtained from memory-mapped file
	// This will load an empty editor window if no data is found
    // This will try to locate an existing window AFTER passing 
    //    through EDT_PreOpen() plugin hook function
    LoadUrlEditor(pUrl);

#ifndef WIN32
    if ( NULL != handle) {
        GlobalUnlock(handle);
    }
#else   // Win16
    if ( pUrl ) {
        XP_FREE(pUrl );
    }
#endif
    return 0;
}

// Called by other apps via registered message
LRESULT CGenericFrame::OnOpenNavigator(WPARAM wParam, LPARAM lParam )
{
    HANDLE handle = NULL;
#ifndef WIN32
    // We get data passed to us if Win16
    handle = (HANDLE)wParam;
#endif
    char *pUrl = GetGlobalUrlPointer(handle);

    // First try to find existing frame
    if ( ! FE_ActivateFrameWithURL( pUrl, FALSE ) ) {
        wfe_CreateNavigator( pUrl );
    }

#ifndef WIN32
    if ( NULL != handle) {
        GlobalUnlock(handle);
    }
#else   // Win16
    if ( pUrl ) {
        XP_FREE(pUrl );
    }
#endif
    return 0;
}

// Should be called only from Edit view
//   or from EditToNavigate (which is called from view)
//   after FE_CheckAndSaveDocument is called

void CGenericFrame::OpenNavigatorWindow(MWContext * pMWContext)
{
    if ( !pMWContext ){
        return;
    }
	BOOL bNewDocument = FALSE;
	MWContext * pNavContext = NULL;
    BOOL bNewWindow = TRUE;
	
	URL_Struct *pUrlStruct = NULL;
    History_entry * pCurrentEntry = SHIST_GetCurrent(&(pMWContext->hist));
	if ( pCurrentEntry != NULL && pCurrentEntry->address ) {
        if ( 0 == XP_STRCMP(pCurrentEntry->address, EDT_NEW_DOC_NAME) ) {
	        // Suppress trying to load "Untitled" document, load home page instead
	        bNewDocument = TRUE;
	        pCurrentEntry = NULL;
        }
        else {
	        // First try to find existing frame
	        pNavContext = FE_ActivateFrameWithURL( pCurrentEntry->address, FALSE );
	        if ( pNavContext ) {
                bNewWindow = FALSE;
                // Do reload in case the contents of the found browser window 
                //  are older than the editor we are switching from
                // (Note: Must get new CWinCX from MWcontext since
                //        this will get sent before view is activated,
                //        i.e., Main and Active contexts not set yet)
                WINCX(pNavContext)->Reload(NET_SUPER_RELOAD);
            } else {
                pUrlStruct = SHIST_CreateURLStructFromHistoryEntry(GetMainContext()->GetContext(), 
						                 pCurrentEntry);
                if ( pUrlStruct ) {
	                // Must clear this to correctly load URL into new context
	                pUrlStruct->fe_data = NULL;

                    // Always check the server to get most up-to-date version
                    pUrlStruct->force_reload = NET_NORMAL_RELOAD;

	                // This prevents URL from being added twice 
	                //  to history list -- we copy from old to new context
	                //  and reposition "current" to that history item
	                pUrlStruct->history_num = 0;
	                //      If this structure changes in the future to hold data which can be carried
	                //              across contexts, then we lose.
	                memset((void *)&(pUrlStruct->savedData), 0, sizeof(SHIST_SavedData));
                }
	        }
        }
    } else {
        // If no current history, this will cause loading the Home Page
        bNewDocument = TRUE;
    }

    if ( !pNavContext ) {
	// Create new Frame+View+Context
	    pNavContext = CFE_CreateNewDocWindow(NULL, pUrlStruct );
    }


    if ( pNavContext ) {
        if ( bNewWindow ) {
	        // Copy History from Editor to Navigator context
	        // Note: This function was modified to not copy any "Untitled" new doc entries
	        SHIST_CopySession(pNavContext, pMWContext);

	        //      Set the current session history index for the new context
	        //   by finding same location of old address in new context
	        //  (Note: pCurrentEntry = NULL in browser when changing to browser
	        //         from an empty document. Set default Index to 0 to handle this)
	        int nIndex = 0;
	        if ( pCurrentEntry && pCurrentEntry->address != NULL ) {
	        nIndex = FindCurrentUrlInHistory(pNavContext, pCurrentEntry->address );
	        }
	        SHIST_SetCurrent(&(pNavContext->hist), nIndex );
        }
        // Do this AFTER copying history so homepage appears in new history list
        if ( bNewDocument ) {
	        // Load home page if no URL or "Untitled" was supplied
	        // TODO: Find existing instance of current home page? (very tricky!)
	        ((CMainFrame*)GetFrame(pNavContext))->OnLoadHomePage();
        }
    }
}

// Replace Editor with a Navigator window with current Editor URL
// This and OpenEditorWindow should be called
//  only after FE_CheckAndSaveDocument
void CGenericFrame::EditToNavigate(MWContext * pEditContext, BOOL bNewDocument )
{
	if ( pEditContext == NULL ) {
		return;
	}
    // Place Browser at same place
    wfe_bUseLastFrameLocation = TRUE;

    OpenNavigatorWindow(pEditContext);

    // Close the underlying editor
    ((CMainFrame*)GetFrame(pEditContext))->PostMessage(WM_CLOSE);
}

LRESULT CGenericFrame::OnNetscapeGoldIsActive(WPARAM wParam, LPARAM lParam)
{
    // Return handle to Frame window as response to
    //  our special message - another instance or app
    //  is asking if we are here
    return (LRESULT)(this->m_hWnd);
}

void CGenericFrame::OnEditNewDocFromTemplate()
{
    if ( GetMainContext() == NULL || GetMainContext()->GetContext() == NULL ) {
        return;
    }
    
    // Popup dialog to allow user to set location, choose from 20 most recent, or choose new file
    COpenTemplateDlg dlg(this);
    if(dlg.DoModal() == IDOK){
        char *pTemplate = NULL;
        if( !dlg.m_csLocation.IsEmpty() ){
            pTemplate = XP_STRDUP((char*)LPCSTR(dlg.m_csLocation));
            if( pTemplate ) {
                // Save the location chosen; if it is loaded successfully,
                //   it will be added to template history list
                PREF_SetCharPref("editor.template_last_loc", pTemplate);

                // Load a browser with this template URL
                char * pDefault = NULL;
    	        PREF_CopyCharPref("editor.default_template_location", &pDefault);
                if( pDefault &&  0 == strcmp(pDefault, pTemplate) ){
                    // Current location is Netscape's Template page,
                    // Load into Browser
                    wfe_CreateNavigator(pTemplate);
                } else {
                    // Check if user is already editing this template
                    MWContext *pEditContext = FE_ActivateFrameWithURL(pTemplate, TRUE);
                    if( pEditContext && EDT_DirtyFlag(pEditContext) ){
                        CGenericFrame *pFrame = (CGenericFrame*)GetFrame(pEditContext);
                        // Warn the user that template is being edited
                        //  and new page wont have unsaved changes
                        pFrame->MessageBox(szLoadString(IDS_EDITING_TEMPLATE),
                                           szLoadString(IDS_NEW_PAGE_TEMPLATE),
                                           MB_ICONEXCLAMATION | MB_OK);
                    }
                    // Load template page into a new edit window
                    LoadUrlEditor(pTemplate, TRUE, ED_LOAD_TEMPLATE);
                }
                XP_FREEIF(pDefault);
                XP_FREE(pTemplate);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
// SiteManager communications methods
//
#ifdef XP_WIN32
// Site Manager tells us it is being activated or destroyed
LRESULT CGenericFrame::OnSiteMgrMessage(WPARAM wParam, LPARAM lParam)
{
    if( wParam == SM_IS_ALIVE ){
        if( pITalkSMClient && !pITalkSMClient->IsRegistered() ){
	        // This should occur only if SiteManager was run the very first time
	        //  after our instance started (it wasn't registered)
	        // Recreating our class will reset registration flag
	        delete pITalkSMClient;
	        pITalkSMClient = new (ITalkSMClient);
        }
        if( pITalkSMClient && pITalkSMClient->IsRegistered() ){
	        bSiteMgrIsActive = TRUE;
	        pITalkSMClient->SetKnownSMState(TRUE);
        }
        return 1;
    } else if( wParam == SM_IS_DEAD ){
        bSiteMgrIsActive = FALSE;
        if( pITalkSMClient ){
	        pITalkSMClient->SetKnownSMState(FALSE);
        }
        return 1;
    }

    return 0;
}
#endif

void CGenericFrame::OnActivateSiteManager()
{
#ifdef XP_WIN32
	XP_Bool prefBool;
	PREF_GetBoolPref("editor.hints.sitemanager",&prefBool);

    if( bSiteMgrIsRegistered ) {
        if ( prefBool ) {
	        // Show a "Hint" dialog about drag and drop until
	        //   user checks "Don't show this" in dialog
	        CEditHintDlg dlg(this, IDS_DRAG_SITEMAN_HINT);
	        dlg.DoModal();
	        if ( dlg.m_bDontShowAgain ) {
		        PREF_SetBoolPref("editor.hints.sitemanager",FALSE);
	        }
        }
        // This will invoke SiteManager if it is not already running
        if( pITalkSMClient->BecomeActive() ){
	        bSiteMgrIsActive = TRUE;
        }
    }
#endif
}

void CGenericFrame::OnUpdateActivateSiteManager(CCmdUI* pCmdUI)
{
#ifdef XP_WIN32
    pCmdUI->Enable( bSiteMgrIsRegistered );    
#endif
}

static BOOL bInFontMenu = FALSE;
static BOOL bInSizeMenu = FALSE;
static BOOL bInHistoryMenu = FALSE;
static BOOL bInTableMenu = FALSE;

// Put all Composer-only code here - called only from CGenericFrame::OnMenuSelect()
void CGenericFrame::OnMenuSelectComposer(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
    if ( nFlags == 0xFFFF && hSysMenu == 0 )
    {
        // Menu is being destroyed - clear rebuild flags
        bInFontMenu = FALSE;
        bInSizeMenu = FALSE;
        bInHistoryMenu = FALSE;
        bInTableMenu = FALSE;
        return;
    }

    if ( nFlags & MF_POPUP )
    {
        MWContext * pMWContext = GetMainContext()->GetContext();
	    HMENU hMenu = ::GetMenu(m_hWnd);
        if( !hMenu || !pMWContext ){
            return;
        }
        int nCount = GetMenuItemCount(hMenu);
        HMENU hEditHistoryMenu = NULL;
        HMENU hFormatMenu = NULL;
        HMENU hFontMenu = NULL;
        HMENU hSizeMenu = NULL;
		HMENU hSubMenu = NULL;
        HMENU hSubSubMenu;
        int i;

	    for( i = 0; i < nCount; i++)
        {
    		hSubMenu = GetSubMenu(hMenu, i);
		    if( hSubMenu )
            {
                // Modify item(s) in the Table menu
                if( i == ED_MENU_TABLE )
                {
                    if( bInTableMenu )
                        return;
                    bInTableMenu = TRUE;
                    // Show "Convert table to text" when table is selected or
                    //  caret is inside table with nothing selected
                    BOOL bConvertToText = EDT_IsTableSelected(pMWContext) || 
                                           (EDT_IsInsertPointInTable(pMWContext) &&
                                            !EDT_IsSelected(pMWContext) &&
                                            EDT_GetSelectedCellCount(pMWContext) == 0);

                    ::ModifyMenu(hSubMenu, ID_TABLE_TEXT_CONVERT, MF_BYCOMMAND | MF_STRING, ID_TABLE_TEXT_CONVERT,
                                 szLoadString(bConvertToText ? IDS_CONVERT_TABLE_TO_TEXT : IDS_CONVERT_TEXT_TO_TABLE) );
                    // We can return here ONLY if we don't need to look at menus after "Table"
                    break;
                }
                // Search for the "Open Recent" subsubmenu under the File submenu,
                //  which is always the first top-level menu
                UINT nSubCount = GetMenuItemCount(hSubMenu);
                for(UINT j = 0; j < nSubCount; j++ )
                {
                    hSubSubMenu = GetSubMenu(hSubMenu, j);
                    UINT nID = GetMenuItemID(hSubMenu, j);

                    // Is the the currently-selected menu?
                    if( hSubSubMenu &&
#ifdef XP_WIN32
                        // In Win32, nItemID is INDEX to the submenu item...
                        nItemID == j && hSysMenu == hSubMenu )
#else
                        // ...in Win16, it is the handle to subsubmenu
                        nItemID == (UINT)hSubSubMenu )
#endif
                    {
                        // The first item in the SubSubMenu identifies that menu
                        UINT nID = GetMenuItemID(hSubSubMenu, 0);
                        switch( nID )
                        {
                            case ID_EDIT_HISTORY_BASE:
                                // Build a menu of recently-edited URL titles
                                ((CNetscapeEditView*)GetActiveView())->BuildEditHistoryMenu(hSubSubMenu, 0);
                                return;
                            case ID_FORMAT_FONTSIZE_BASE:
                                hFormatMenu = hSubMenu;
                                hSizeMenu = hSubSubMenu;
                                break;
                            case ID_FORMAT_FONTFACE_BASE:
                                hFormatMenu = hSubMenu;
                                hFontMenu = hSubSubMenu;
                                break;
                        }
                    }
                }
                // We are done when we found the format menu
                if( hFormatMenu )
                    break;
            }
	    }

        // The reest only pertain to Format menu
        if( !hFormatMenu )
        	return;

        if( hFontMenu )
        {
            // Prevent rebuilding menu when mouse moves away
            if( bInFontMenu )
                return;
            bInFontMenu = TRUE;
		    TRACE0("Recreating Font Menu\n");
            
            // Delete any existing items
            for( int i = ::GetMenuItemCount(hFontMenu) - 1; i >= 0; i--){
                DeleteMenu(hFontMenu, i, MF_BYPOSITION);
            }
            // Get list of FontFace fonts defined in XP_Strings
            char * pFontFaces = EDT_GetFontFaces();

            // Build the FontFace menu
            if( pFontFaces )
            {
                char * pFont = pFontFaces;
                ::AppendMenu(hFontMenu, MF_STRING, ID_FORMAT_FONTFACE_BASE, pFont);
                pFont += XP_STRLEN(pFont) + 1;
                if( *pFont )
                {
                    ::AppendMenu(hFontMenu, MF_STRING, ID_FORMAT_FONTFACE_BASE+1, pFont);
                }
                int nCount = 0;
                if( wfe_iTrueTypeFontCount && wfe_ppTrueTypeFonts )
                {
                    // Add separator
                    ::AppendMenu(hFontMenu, MF_SEPARATOR,0,0);
                
                    // Add TrueType fonts but don't overflow the screen height
                    // Number of items that will fit, minus some for items already
                    //  added, separators, and a bit extra
                    nCount = (sysInfo.m_iScreenHeight / GetSystemMetrics(SM_CYMENU)) - 6;
                    for( int i = 0; i < min(wfe_iTrueTypeFontCount, nCount); i++ ){
                        ::AppendMenu(hFontMenu, MF_STRING, ID_FORMAT_FONTFACE_BASE+i+2, 
                                     wfe_ppTrueTypeFonts[i]);
                    }
                }                    
                if( nCount < wfe_iTrueTypeFontCount )
                {
                    ::AppendMenu(hFontMenu, MF_SEPARATOR,0,0);
                    // Last item is "Other..." to launch Window's Font Face dialog
                    ::AppendMenu(hFontMenu, MF_STRING, ID_OTHER_FONTFACE, ed_pOther);
                }
            }
        }  
        else if( hSizeMenu )
        {
            if( bInSizeMenu )
                return;
            bInSizeMenu = TRUE;
            int iCount = ::GetMenuItemCount(hSizeMenu);
            if( iCount == 1 )
            {
                // Initial menu has 1 placeholder - modify that
                //   and append the rest
                // First 2 menu items are relative size change
                ::ModifyMenu( hSizeMenu, 0, MF_BYPOSITION | MF_STRING, ID_FORMAT_INCREASE_FONTSIZE,
                              szLoadString(IDS_INCREASE_FONTSIZE) );
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_DECREASE_FONTSIZE,
                              szLoadString(IDS_DECREASE_FONTSIZE) );
                ::AppendMenu( hSizeMenu, MF_SEPARATOR, 0, 0);
            }            
            // Delete any existing items except for 1st three
            for( i = iCount - 1; i > 2; i--)
            {
                DeleteMenu(hSizeMenu, i, MF_BYPOSITION);
            }

            // Check if current base font is fixed width
            BOOL bFixedWidth = (EDT_GetFontFaceIndex(pMWContext) == 1);
            // Change font size strings based on current font base
            for ( i = 0; i < MAX_FONT_SIZE; i++ ){
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_FONTSIZE_BASE + i,
                              wfe_GetFontSizeString(pMWContext, i+1, bFixedWidth,
                                                    TRUE) ); // Format for menu
            }
            if( wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED )
            {
                // The "Advanced" absolute point size strings
                ::AppendMenu( hSizeMenu, MF_SEPARATOR, 0, 0);
                // This string is "8 pts" so it must be translated
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE, szLoadString(IDS_8_PTS));
                // Others assume pure numbers don't have to be translated
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+1, "9");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+2, "10");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+3, "11");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+4, "12");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+5, "14");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+6, "16");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+7, "18");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+8, "20");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+9, "22");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+10, "24");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+11, "26");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+12, "28");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+13, "36");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+14, "48");
                ::AppendMenu( hSizeMenu, MF_STRING, ID_FORMAT_POINTSIZE_BASE+15, "72");
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

// Checks if there is any active editor plugin and lets the user stop it.
// Return: TRUE - no plugin was running or user stopped the active plugin
//         FALSE - user didn't want to stop the plugin

BOOL CheckAndCloseEditorPlugin(MWContext *pMWContext)
{
    if (EDT_IsPluginActive(pMWContext)) {
        CGenericView* pView = WINCX(pMWContext)->GetView();
        CWnd *pParentWnd = pView->GetFrame()->GetFrameWnd()->GetLastActivePopup();

        if (pParentWnd->MessageBox(szLoadString(IDS_CONFIRM_STOP_PLUGIN), NULL, MB_YESNO | MB_ICONQUESTION) == IDYES)
            EDT_StopPlugin(pMWContext);
        else
            return FALSE;
    }

    return TRUE;
}

#endif // EDITOR

