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

// edprops.cpp : implementation file
//
#include "stdafx.h"
#ifdef EDITOR
#include "edres2.h"
#include "property.h"
#include "edprops.h"
#include "edttypes.h"
#include "edt.h"
#include "pa_tags.h"
#include "netsvw.h"
#include "edview.h"
#include "styles.h" // For WFE_DrawSwatch()
#ifndef XP_WIN32
#include "tooltip.h"
#endif
#include "nethelp.h"
#include "xp_help.h"
#include "prefapi.h"
#include "prefinfo.h"
// the dialog box & string resources are in edtrcdll DLL
#include "edtrcdll\src\resource.h"

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

// Font shared by all comboboxes in dialogs and toolbars
// Should be 1-pixel, 8pt MS Sans Serif or default GUI or font on foreign systems
CFont * wfe_pFont = 0;
int     wfe_iFontHeight = 15; // Full Height of the this font

// Modification of wfe_iFontHeight for list boxes;
int     wfe_iListItemHeight = 15;


extern void wfe_Progress(MWContext *pMWContext, const char *pMessage);

extern BOOL FE_ResolveLinkURL( MWContext* pMWContext, 
                               CString& csURL, BOOL bAutoAdjustLinks );

BOOL bSaveDefaultHRule = TRUE;

int32 wfe_iFontSizeMode = ED_FONTSIZE_POINTS;

// Convert CString into non-const char*
#define CHAR_STR(csString)       (char*)LPCSTR(csString)

#define MAX_AUTO_SAVE_MINUTES  600

#define COLORREF_SIZE  sizeof(COLORREF)

// Array of colors to show in common color dialog
COLORREF wfe_CustomPalette[16];
COLORREF wfe_crLastColorPicked;

int wfe_iTrueTypeFontBase = 0;
int wfe_iTrueTypeFontCount = 0;

// Global array of True-Type fonts
// Neccesitates restarting program after new fonts installed
char **wfe_ppTrueTypeFonts = NULL;

// Global index within wfe_ppTrueTypeFonts if "Other..." was needed (too many fonts)
int wfe_iFontFaceOtherIndex = 0;

int CALLBACK EXPORT
EnumTrueTypeFonts(LPLOGFONT lpnlf, LPTEXTMETRIC lpntm, int nFontType, LPARAM lParam)
{
	if (lpntm->tmPitchAndFamily & (/*TMPF_VECTOR|TMPF_DEVICE|*/TMPF_TRUETYPE)){
        if( wfe_iTrueTypeFontCount < MAX_TRUETYPE_FONTS ){
		    wfe_ppTrueTypeFonts[wfe_iTrueTypeFontCount] = XP_STRDUP(lpnlf->lfFaceName);
            wfe_iTrueTypeFontCount++;
        } else {
            // Too many fonts -- add "Other..." to use common dialog
            *((BOOL*)lParam) = TRUE;
            return FALSE;
        }
    }
	return TRUE;
}

int CompareStrings( const void *arg1, const void *arg2 )
{
   // Compare all of both strings:
   // TODO: DO WIDE/MULTIBYTE VERSION
   return _stricmp( * ( char** ) arg1, * ( char** ) arg2 );
}

static BOOL bTooManyFonts = FALSE;

BOOL wfe_InitTrueTypeArray(CDC *pDC)
{
    ASSERT(pDC);
    HDC hdc = pDC->GetSafeHdc();
    if( wfe_ppTrueTypeFonts ){
        return bTooManyFonts;
    }

    wfe_iTrueTypeFontCount = 0;
    wfe_iFontFaceOtherIndex = 0;
    bTooManyFonts = FALSE;    
    wfe_ppTrueTypeFonts = (char**)XP_ALLOC(MAX_TRUETYPE_FONTS*sizeof(char*));
    if( !wfe_ppTrueTypeFonts ){
        ASSERT(wfe_ppTrueTypeFonts);
        bTooManyFonts = TRUE;
        return TRUE;
    }
    memset((void*)wfe_ppTrueTypeFonts, 0, MAX_TRUETYPE_FONTS*sizeof(char*));

	EnumFontFamilies(hdc, NULL, (FONTENUMPROC)EnumTrueTypeFonts, (LPARAM)&bTooManyFonts);

    if( wfe_iTrueTypeFontCount ){
        // Sort font names using Quicksort algorithm:
        qsort( (void *)wfe_ppTrueTypeFonts, (size_t)wfe_iTrueTypeFontCount, sizeof( char * ), CompareStrings );
    }
    return bTooManyFonts;
}

void wfe_FreeTrueTypeArray()
{
    int i;
    if( wfe_ppTrueTypeFonts ){
        for( i = 0; i < MAX_TRUETYPE_FONTS; i++ ){
            XP_FREEIF(wfe_ppTrueTypeFonts[i]);
        }
        XP_FREE(wfe_ppTrueTypeFonts);
        wfe_ppTrueTypeFonts = 0;
    }
    wfe_iTrueTypeFontCount = 0;
}

char pSepFont1[128] = "_";
char pSepFont2[128] = "_";

int wfe_FillFontComboBox(CComboBox * pCombo, int * pMaxWidth)
{
    char * pFontFaces = EDT_GetFontFaces();
    CDC * pDC = pCombo->GetDC();
    if( !pDC ) return 0;
    HDC hdc = 0;
    if( pMaxWidth ){
        // We need this only if returning iMaxWidth
        hdc = pDC->GetSafeHdc();
    }
    
    CSize cSize;
    int iMaxWidth = 0;
    int i = 0;

    if( pFontFaces ){
        char * pFont = pFontFaces;
        int iLen;    
        for ( i = 0; i < 2; i++){
        
    	    iLen = XP_STRLEN(pFont);
    		if( iLen ){
                // add to the end of the list
                // LIST MUST BE NOT SORTED for EDT_GetFontFaceIndex to work
                if( i == 1 ){
                    // Add signal to draw separator under this string
                    strcpy(pSepFont1+1,pFont);
    		        pCombo->AddString(pSepFont1);
                } else {
    		        pCombo->AddString(pFont);
                }
                if ( hdc ){            
    		        cSize = CIntlWin::GetTextExtent(0, hdc, pFont, strlen(pFont));
	        	    pDC->LPtoDP(&cSize);
	    	        if ( cSize.cx > iMaxWidth ){
    			        iMaxWidth = cSize.cx;
		            }
    		    }
                pFont += iLen+1;
            } else {
                break;
            }
        }
    }
    // Done with built-in fonts -- next index must be start of True Type
    wfe_iTrueTypeFontBase = i;
    
    // Get pointers to array of font names
    // (Global list should already be built)
    if( wfe_iTrueTypeFontCount && wfe_ppTrueTypeFonts){
        // Add sorted string list to combobox
        for( int i = 0; i < wfe_iTrueTypeFontCount; i++ ){
            char * pFontName = wfe_ppTrueTypeFonts[i];
            pCombo->AddString(pFontName);
            if ( hdc ){            
    		    cSize = CIntlWin::GetTextExtent(0, hdc, pFontName, strlen(pFontName));
	        	pDC->LPtoDP(&cSize);
	    	    if ( cSize.cx > iMaxWidth ){
    			    iMaxWidth = cSize.cx;
		        }
    		}
        } 
    } else {
        // Should never get here if wfe_IntTrueTypeArray was ever called
        bTooManyFonts = TRUE;;
    }
    if( pMaxWidth ){
        // Return max width so dropdown list can be expanded to fit
        *pMaxWidth = iMaxWidth;
    }
 
    if( bTooManyFonts ){
        // Add pointer to globalb "Other..." string
        pCombo->AddString(ed_pOther);
        // Calculage the index to the "Other..." item,
        wfe_iFontFaceOtherIndex = wfe_iTrueTypeFontBase + wfe_iTrueTypeFontCount;
    } else {
        wfe_iFontFaceOtherIndex = 0;
    }
	pCombo->ReleaseDC(pDC);
    return wfe_iFontFaceOtherIndex;
}

char wfe_pFontSizeString[64];
char wfe_pFontSizeList[MAX_FONT_SIZE+1][64];

int wfe_FillFontSizeCombo(MWContext *pMWContext, CNSComboBox * pCombo, BOOL bFixedWidth)
{
    int iFontSize = EDT_GetFontSize(pMWContext);
    if( pMWContext ) {
        // Clear current data
        pCombo->ResetContent();

        // Format a string like: "+2" for advanced mode, or convert to
        //  point size for regular mode
	    for ( int i = 1; i < MAX_FONT_SIZE; i++ ){
		    pCombo->AddString(wfe_GetFontSizeString(pMWContext, i, bFixedWidth));
	    }
        if( wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED ){
            // "_" will signal combobox to draw a separator under this
            pCombo->AddString("_+4");
            pCombo->AddString("8");
            pCombo->AddString("9");
            pCombo->AddString("10");
            pCombo->AddString("11");
            pCombo->AddString("12");
            pCombo->AddString("14");
            pCombo->AddString("16");
            pCombo->AddString("18");
            pCombo->AddString("20");
            pCombo->AddString("22");
            pCombo->AddString("24");
            pCombo->AddString("26");
            pCombo->AddString("28");
            pCombo->AddString("36");
            pCombo->AddString("48");
            pCombo->AddString("72");
        } else {
		    pCombo->AddString(wfe_GetFontSizeString(pMWContext, MAX_FONT_SIZE, bFixedWidth));
        }
    }
    // Index to current size is 1 less than our relative size (range = 1-7)
    // Note that this may be 0 (on startup)
    return (iFontSize - 1);
}

// iSize should be  1 to 7
// Returns pointer to static string - DON'T FREE RESULT!
char * wfe_GetFontSizeString(MWContext * pMWContext, int iSize, BOOL bFixedWidth, BOOL bMenu)
{
    ASSERT(pMWContext);
	int iRelative = iSize - 1 + MIN_FONT_SIZE_RELATIVE;

    CString csFormat;

    if( wfe_iFontSizeMode != ED_FONTSIZE_POINTS ){
        if ( iRelative < 0 ){
            // Minus is very narrow - include extra leading space
            csFormat = " -";
        } else if ( iRelative > 0 ){
            csFormat = '+';
        } else {
            // Use 2 spaces befor "0"; has same width as 1 "-" or "+" char
            csFormat = "  ";
        }

        if( bMenu && iSize > 2 ){
            // Add underline for "accelerator" only for "0" and above
            csFormat += '&';
        }
        csFormat += "%i";

        wsprintf(wfe_pFontSizeList[iSize-1], LPCSTR(csFormat), abs(iRelative));
    } else {
        // Contstruct strings for Point sizes
        CDCCX *pDC = VOID2CX(pMWContext->fe.cx, CDCCX);

	    EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(pMWContext);
        int iBaseSize = bFixedWidth ? pEncoding->iFixSize : pEncoding->iPropSize;
        double dPoints = pDC->CalcFontPointSize(iSize, iBaseSize);
        wsprintf(wfe_pFontSizeList[iSize-1], "%d", (int)dPoints);
    }
    return wfe_pFontSizeList[iSize-1];
}

////////////////////////////////////////////////////////////////////////
// Local string helper functions

// Strip out quotes and spaces at the ends
void CleanupString(CString& csString)
{
    csString.TrimLeft();
    csString.TrimRight();
    int iQuote;    
    while ( (iQuote = csString.Find('\"')) != -1 ){
        csString = csString.Left(iQuote) + csString.Mid(iQuote+1);
    }
}

// Extract separate Name and Value strings
// Return TRUE if Name string was found
BOOL GetNameAndValue(CString& csEntry, CString& csName, CString& csValue)
{
    int iEqual = csEntry.Find('=');
    if ( iEqual != -1 ) {
        csName = csEntry.Left(iEqual);
        CleanupString(csName);
        csValue = csEntry.Mid(iEqual+1);
        CleanupString(csValue);
        return ( !csName.IsEmpty() );
    }
    return FALSE;
}

// Local function used by Image and Document Properties dialogs
//
BOOL wfe_ValidateImage(MWContext * pMWContext, CString& csImageURL, BOOL bCheckIfFileExists)
{
    BOOL   bRetVal = TRUE;
    BOOL   bFileExists = FALSE;
    CleanupString(csImageURL);
    char * pAbsoluteImageURL = NULL;
    CString csURL;

    // Empty is OK
    if ( csImageURL.IsEmpty() ) {
        return TRUE;
    }
    //int   iUrlType = NET_URL_Type(csUrl);

    // We will trust HTTP: URLs to be absolute and valid
    if ( NET_IsHTTP_URL( csImageURL ) ) {
        return TRUE;
    }        

    // Convert to URL format -- just copies if already have URL syntax
    WFE_ConvertFile2Url( csURL, (char*)LPCSTR(csImageURL) );

    // Convert any user-enterred local strings to an absolute URL
    History_entry * pEntry = SHIST_GetCurrent(&pMWContext->hist);
    if( pEntry && pEntry->address ) {
        pAbsoluteImageURL = NET_MakeAbsoluteURL( 
                                pEntry->address, (char *)LPCSTR(csURL) );
    }

    if(bCheckIfFileExists && pAbsoluteImageURL) {

        if ( NET_IsLocalFileURL((char*)LPCSTR(csImageURL)) ) {
            // We have a local file URL -- test for existence
             char * pLocal = NULL;
             bFileExists = XP_ConvertUrlToLocalFile( csImageURL, &pLocal /*NULL*/);
             if( pLocal ) XP_FREE(pLocal);

        } else {
            // Assume we have a local file - find it
            XP_StatStruct statinfo;
            if ( -1 != XP_Stat((char*)LPCSTR(csImageURL), &statinfo, xpURL) &&
                statinfo.st_mode & _S_IFREG ) {
                bFileExists = TRUE;
            }
        }
        if ( !bFileExists ) {
            CString csMsg;
            AfxFormatString1(csMsg, IDS_ERR_SRC_NOT_FOUND, csImageURL);  
            CWnd *pParent = GetFrame(pMWContext)->GetFrameWnd()->GetLastActivePopup();
            ::MessageBox( pParent ? pParent->m_hWnd : NULL, 
                          csMsg, szLoadString(IDS_VALIDATE_IMAGE_FILE),
                          MB_OK | MB_ICONEXCLAMATION);
        }
    }

    if( bCheckIfFileExists ){
        bRetVal = bFileExists;
    }
    
    if ( pAbsoluteImageURL ) {
        if( bRetVal ){
            csImageURL = pAbsoluteImageURL;
        }
        XP_FREE(pAbsoluteImageURL);
    }

    return bRetVal;
}

// Initialize the strings for percent/pixels combobox
//   used with Width and Height input
void wfe_InitPixOrPercentCombos(CWnd* pParent)
{
    CComboBox *pCombo = 
        (CComboBox*)(pParent->GetDlgItem(IDC_HEIGHT_PIX_OR_PERCENT));
    if(pCombo){
        pCombo->AddString(szLoadString(IDS_PIXELS));
        pCombo->AddString(szLoadString(IDS_PERCENT_WINDOW));
    }
    pCombo = (CComboBox*)(pParent->GetDlgItem(IDC_WIDTH_PIX_OR_PERCENT));
    if(pCombo){
        pCombo->AddString(szLoadString(IDS_PIXELS));
        pCombo->AddString(szLoadString(IDS_PERCENT_WINDOW));
    }
}

// Get the "100%" Height and "100%" Width that layout uses:
//  this is the current view's dimensions minus margins
void wfe_GetLayoutViewSize(MWContext * pMWContext, int32 * pWidth, int32 * pHeight)
{
    if( pMWContext ){
        int32 iXOrigin;
        int32 iYOrigin;
        int32 iHeight;
        int32 iWidth;
    	int32 iMarginWidth;
	    int32 iMarginHeight;

        FE_GetDocAndWindowPosition(pMWContext, &iXOrigin, &iYOrigin, &iWidth, &iHeight);
    	LO_GetDocumentMargins(pMWContext, &iMarginWidth, &iMarginHeight);

        if( pWidth ){
            *pWidth = iWidth - (2 * iMarginWidth);
        }
        if( pHeight ){
            *pHeight = iHeight - (2 * iMarginHeight);
        }
    }    
}


char * wfe_ConvertImage(char *p_fileurl,void *p_parentwindow,MWContext *p_pMWContext)
{
    if( !wfe_ValidateImage(p_pMWContext, CString(p_fileurl), TRUE) ){
        return NULL;
    }

    CONVERT_IMGCONTEXT imageContext;
    CONVERT_IMG_INFO imageInfo;
    memset(&imageContext,0,sizeof(CONVERT_IMGCONTEXT));
    imageContext.m_stream.m_type=CONVERT_FILE;
    char *t_charp;
    XP_ConvertUrlToLocalFile(p_fileurl,&t_charp);
    if (!t_charp) //failed to localize file
    {
        // Show same error message as when wfe_ValidateImage fails:
        CString csMsg;
        AfxFormatString1(csMsg, IDS_ERR_SRC_NOT_FOUND, p_fileurl);  
        CWnd *pParent = GetFrame(p_pMWContext)->GetFrameWnd()->GetLastActivePopup();
        ::MessageBox( pParent ? pParent->m_hWnd : NULL, 
                      csMsg, szLoadString(IDS_VALIDATE_IMAGE_FILE),
                      MB_OK | MB_ICONEXCLAMATION);
        return NULL;
    }
    XP_STRCPY(imageContext.m_filename,t_charp);
    XP_FREE(t_charp);
    imageContext.m_stream.m_file=XP_FileOpen(imageContext.m_filename,xpTemporary,XP_FILE_READ_BIN);
    if (imageContext.m_stream.m_file)
    {
        imageContext.m_imagetype=conv_bmp;
        imageContext.m_callbacks.m_dialogimagecallback=FE_ImageConvertDialog;
        imageContext.m_callbacks.m_displaybuffercallback=FE_ImageConvertDisplayBuffer;
        imageContext.m_callbacks.m_completecallback=NULL;

        imageContext.m_pMWContext=p_pMWContext;
        imageContext.m_parentwindow=p_parentwindow;
        char *t_outputfilename[1];
        t_outputfilename[0]=NULL;
        CONVERT_IMAGERESULT t_result=convert_stream2image(imageContext,&imageInfo,1,t_outputfilename); //1 for 1 output
        //there is a callback when complete 
        XP_FileClose(imageContext.m_stream.m_file);
        if (t_outputfilename[0])
        {
            // If using GIF plugin, wait here until finished
            //   (edit buffer is not writeable during plugin use)
            char * pLastDot = strrchr(t_outputfilename[0], '.');
            if(pLastDot && 0 == _stricmp(pLastDot, ".gif")){
                while(!EDT_IsWritableBuffer(p_pMWContext)) FEU_StayingAlive();
            }
            return t_outputfilename[0];
        }
        else
        {
            if (t_result>CONV_OK)//not cancel or ok
            {
                ((CWnd *)p_parentwindow)->MessageBox(szLoadString(CNetscapeEditView::m_converrmsg[t_result]));
            }
            return NULL;
        }
    }
    return NULL;
}



CNSComboBox::CNSComboBox() :
    m_iSearchPos(0),
    m_bAllowSearch(0),
    m_dwButtonDownTime(0),
    m_bCheckTime(0),
    m_pNotInListText(0)
{
}

BOOL CNSComboBox::Create(RECT& rect, CWnd* pParentWnd, UINT nID)
{
	if( !CComboBox::Create(CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|CBS_AUTOHSCROLL,
                           rect, pParentWnd, nID) ){
		TRACE0("Failed to create Netscape Combo-box\n");
		return FALSE;
	}

    if( wfe_pFont )
        SetFont(wfe_pFont);

    return TRUE;    
}

BOOL CNSComboBox::Subclass(CWnd* pParentWnd, UINT nID)
{
    BOOL bResult = SubclassDlgItem(nID, pParentWnd);
    if( bResult ){
        if( wfe_pFont )
            SetFont(wfe_pFont);
    }
    return bResult;
}

int CNSComboBox::FindSelectedOrSetText(char * pText, int iStartAt)
{
    int iSel = -1;
    if( pText ){
        // Skip over initial signal for separator
        if( *pText == '_' ){
            pText++;
        }
        for( int i = iStartAt; i < GetCount(); i++ ){
            char * pItem = (char*)GetItemData(i);
            if( *pItem == '_' ){
                pItem++;
            }
            if( 0 == strcmp(pText, pItem) ){
                // We found it
                iSel = i;
                break;
            }
        }
        if( iSel == -1 ){
            // Text is not in the list
            XP_FREEIF(m_pNotInListText);
            m_pNotInListText = XP_STRDUP(pText);
        }
        SetCurSel(iSel);
        // SetCurSel doesn't display text!
        SetWindowText(pText);
    } else {
        SetWindowText("");
    }
    return iSel;
}

void CNSComboBox::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
	// all items are of fixed size
    lpMIS->itemHeight = wfe_iListItemHeight;
}

//////////////////////////////////////////////////////////////
void CNSComboBox::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
	CDC* pDC = CDC::FromHandle(lpDIS->hDC);

    CRect rectItem  = lpDIS->rcItem;

	if( lpDIS->itemState & ODS_SELECTED ){
        pDC->SetTextColor(GetSysColor(COLOR_HIGHLIGHTTEXT));
        pDC->SetBkColor(GetSysColor(COLOR_HIGHLIGHT));
	} else {
        pDC->SetTextColor(RGB(0,0,0));
        pDC->SetBkColor(RGB(255,255,255));
	}
    
    char *pData;
    if( lpDIS->itemData == -1){// When is this -1?
        // This is a real neat trick to display text in closed
        //   dropdown combobox's main window even though
        //   text is not in list (selected item = -1)
        // This text should be set in FindSelectedOrSetText
        pData = m_pNotInListText;
    } else {
        pData = (char*)lpDIS->itemData;
    }
    
    if( pData ){
        // Draw a gray line separator
        CPen pen(PS_SOLID, 1, RGB(128,128,128));
        CPen *pOldPen = pDC->SelectObject(&pen);

        BOOL bDrawSeparator = FALSE;
        if( *pData == '_' ){
            // We have a separator - skip over signal character
            pData++;
            if( GetDroppedState() ) {
                bDrawSeparator = TRUE;
            }
        }
        pDC->ExtTextOut(4, rectItem.top+1, ETO_CLIPPED | ETO_OPAQUE, &lpDIS->rcItem,
                        pData, strlen(pData), NULL);
        if( bDrawSeparator ){
            // Draw solid line as separator
            pDC->MoveTo(rectItem.left, rectItem.bottom-1);
            pDC->LineTo(rectItem.right, rectItem.bottom-1);
        }
        pDC->SelectObject(pOldPen);
    }
}

int CNSComboBox::CompareItem(LPCOMPAREITEMSTRUCT lpCIS)
{
    // We should never sort!
    return 0;
}

// These were initially in CEditFrame:
BEGIN_MESSAGE_MAP(CNSComboBox, CComboBox)
    //{{AFX_MSG_MAP(CNSComboBox)
    ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CNSComboBox::InitSearch()
{
    // Don't allow autoselect while typing 
    //   when we have a multibyte system
    if( !GetSystemMetrics(SM_DBCSENABLED) ){
        m_bAllowSearch = TRUE;
        m_iSearchPos = 0;
        m_pSearchBuf[m_iSearchPos] = '\0';
    }
}

void CNSComboBox::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    BOOL bMatch;
    // Ignor shift and control presses
    if( m_bAllowSearch && nChar != VK_SHIFT && nChar != VK_CONTROL ){
        if( nChar != VK_RETURN && 
            (nChar == 4 || XP_IS_ALPHA(nChar) ||
             XP_IS_DIGIT(nChar) || XP_IS_SPACE(nChar)) )
        {
            int iStart = 0;
            if( nChar == VK_BACK ){
                // Backup/delete key in buffer
                if( m_iSearchPos > 0 ){
                    m_iSearchPos--;
                }
            } else {
                // Append acceptable character to search buffer
                m_pSearchBuf[m_iSearchPos] = nChar;
                if( m_iSearchPos ){
                    // Start search from current item if we already had a string,
                  iStart = GetCurSel();
                }
                m_iSearchPos++;
            }
            m_pSearchBuf[m_iSearchPos] = '\0';

            for( int i = iStart; i < GetCount(); i++ )
            {
                char * pItem = (char*)GetItemData(i);
                if( *pItem == '_' ){
                    pItem++;
                }
                if( strlen(pItem) >= (size_t)m_iSearchPos ){
                    // Current item is at least as long, so compare
                    // 
                    char save = pItem[m_iSearchPos];
                    pItem[m_iSearchPos] = '\0';
                    bMatch = ( 0 == stricmp(pItem, m_pSearchBuf) );
                    // Restore
                    pItem[m_iSearchPos] = save;
                    if( bMatch ){
                        // We found it - set selection
                        SetCurSel(i);
                        break;
                    }
                }
            }
            if( !bMatch ){
                // We didn't find next char, so backup
                m_iSearchPos--;
                m_pSearchBuf[m_iSearchPos] = '\0';
            }
        } else {
            // Another other key resets the search string
            m_iSearchPos = 0;
            m_pSearchBuf[m_iSearchPos] = '\0';
        }
    }
    CComboBox::OnKeyDown(nChar, nRepCnt, nFlags);
}

BOOL CNSComboBox::PreTranslateMessage(MSG* pMsg)
{
    return CComboBox::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////////////
// Netscape Color string for Default Text color
char pDefaultFontColorString[256] = "";
// COLORREF equivalent
COLORREF crDefaultColor;

char pDefaultNoColorString[256] = "";

char * wfe_GetDefaultColorString(COLORREF crColor)
{
    if( crColor == NO_COLORREF){
        if( *pDefaultNoColorString == '\0' ){
            strcpy(pDefaultNoColorString, szLoadString(IDS_DEFAULT_NOCOLOR));
        }
        crDefaultColor = RGB(255,255,255);
        return pDefaultNoColorString;
    } 

    if( crColor == DEFAULT_COLORREF ){
        // Get default text color
        PREF_GetDefaultColorPrefDWord("browser.foreground_color", &crDefaultColor);
    } else {
        // Use the supplied color
        crDefaultColor = crColor;
    }

	sprintf(pDefaultFontColorString,"%d,%d%,%d,%s", 
	        GetRValue(crDefaultColor),
	        GetGValue(crDefaultColor),
	        GetBValue(crDefaultColor), 
	        szLoadString(IDS_DEFAULT));

    return pDefaultFontColorString;
}

/////////////////////////////////////////////////////////////////////////////
// Custom Combobox - containing colors

CColorComboBox::CColorComboBox() :
    m_crColor(MIXED_COLORREF),
    m_bCustomColor(0),
    m_bMixedColors(0),
    m_hPal(0),
    m_pParent(0)
{
}

BEGIN_MESSAGE_MAP(CColorComboBox, CComboBox)
	//{{AFX_MSG_MAP(CColorComboBox)
  	ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CColorComboBox::OnLButtonDown(UINT nFlags, CPoint cPoint)
{
    if( m_pParent ){
        // Send message to parent to do the Color Dialog
        m_pParent->SendMessage(WM_COMMAND, ID_GET_COLOR, 0);
    }
}
void CColorComboBox::OnLButtonUp(UINT nFlags, CPoint cPoint)
{
}
void CColorComboBox::OnLButtonDblClk(UINT nFlags, CPoint cPoint)
{
    if( m_pParent ){
        // Send message to parent to do the Color Dialog
        m_pParent->SendMessage(WM_COMMAND, ID_GET_COLOR, 0);
    }
}

int CColorComboBox::SubclassAndFill(CWnd* pParentWnd, UINT nID, COLORREF crDefColor)
{
    m_pParent = pParentWnd;
    if( SubclassDlgItem(nID, pParentWnd) ){
        return FillList(crDefColor);
    }
    return 0;
}

BOOL CColorComboBox::CreateAndFill(RECT& rect, CWnd* pParentWnd, UINT nID, COLORREF crDefColor)
{
    m_pParent = pParentWnd;
	if( !Create(CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL,
                rect, pParentWnd, nID) ){
		TRACE0("Failed to create Font Color combo-box\n");
		return FALSE;
	}

    if( wfe_pFont ){
        SetFont(wfe_pFont);
    }
    return FillList(crDefColor);
}

int CColorComboBox::FillList(COLORREF crDefColor)
{

    CDC * pDC = GetDC();
    if( !pDC ) return 0;

    m_hPal = WFE_GetUIPalette(GetParentFrame());
    
    // WE NO LONGER FILL THE LIST - POPUP IS USED INSTEAD

	ReleaseDC(pDC);
    return TRUE;
}

void CColorComboBox::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
	// all items are of fixed size
    lpMIS->itemHeight = wfe_iListItemHeight;
}

void CColorComboBox::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    // We need to draw outside the supplied area,
    // so use main combobox DC
    HDC hDC = GetDC()->m_hDC;

    HPALETTE hOldPal = NULL;
    if( m_hPal )
    {
		hOldPal = ::SelectPalette( hDC, m_hPal, FALSE );
    }

    RECT rect = lpDIS->rcItem;
    ::InflateRect(&rect, 1, 1);

    // Draw color sample 
    if (lpDIS->itemAction & ODA_DRAWENTIRE)
    {
        if( m_bMixedColors )
        {
            rect.top--;
            rect.left--;
            //rect.bottom++;
            ::FillRect(hDC, &rect, sysInfo.m_hbrBtnFace);
            if( sysInfo.m_bWin4 )
            {
                // It looks very odd unless we add a line to close off button
                HPEN penShadow = ::CreatePen(PS_SOLID, 1, sysInfo.m_clrBtnShadow);
                HPEN hPenOld = (HPEN)::SelectObject(hDC, penShadow);
                ::MoveToEx(hDC, rect.right, rect.top, NULL);
                ::LineTo(hDC, rect.right, rect.bottom);
                ::SelectObject(hDC, hPenOld);
                ::DeleteObject(penShadow);
            }
        } else {
            HBRUSH hBrush = ::CreateSolidBrush(m_crColor|0x02000000); // Like PALETTERGB
            ::FillRect(hDC, &rect, hBrush);
            ::DeleteObject(hBrush);

            int32 iColorTotal = GetRValue(m_crColor) + GetGValue(m_crColor) + GetBValue(m_crColor);

            COLORREF crColorHighlight = 0;
            if(  iColorTotal < 153 )
                crColorHighlight = PALETTERGB(255,255,0);
            else if( iColorTotal > 700 )
                crColorHighlight = PALETTERGB(0,0,255);

            if( sysInfo.m_bWin4 )
            {
                // Extra color highlight if color is too dark or light
                if( crColorHighlight )
                {
                    hBrush = ::CreateSolidBrush(crColorHighlight);
                    ::FrameRect(hDC, &rect, hBrush);
                    ::DeleteObject(hBrush);
                }
            } else {
                // Older UIs have a flat-look for comboboxes,
                //  and need an extra border here
                //  And we use color if current color is too dark or bright
                if( crColorHighlight )
                {
                    hBrush = ::CreateSolidBrush(crColorHighlight);
                } else {
                    hBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
                }
                ::FrameRect(hDC, &rect, hBrush);

                if( crColorHighlight )
                    ::DeleteObject(hBrush);
            } 
        }
    }
    if(m_hPal)
    {
        ::SelectPalette( hDC, hOldPal, FALSE );
    }
    ::ReleaseDC(this->m_hWnd, hDC);
}

int CColorComboBox::CompareItem(LPCOMPAREITEMSTRUCT lpCIS)
{
    // We should never sort!
    return 0;
}

int CColorComboBox::SetColor(COLORREF cr)
{
    int iIndex = -1;
    if( cr == MIXED_COLORREF )
    {
        m_crColor = RGB(255,255,255);
    }else if( cr == DEFAULT_COLORREF )
    {
        m_crColor = crDefaultColor;
        iIndex = 0;
    } else {
        m_crColor = cr; 
        LO_Color LoColor;
        LoColor.red = GetRValue(cr);
        LoColor.green = GetGValue(cr);
        LoColor.blue = GetBValue(cr);
        iIndex = EDT_GetMatchingFontColorIndex(&LoColor);
    }
    m_bMixedColors = (cr == MIXED_COLORREF);
    // Custom color is one not in our list
    m_bCustomColor = !m_bMixedColors && iIndex == -1;
    Invalidate(FALSE);
    SetCurSel(iIndex);
    return iIndex;
}


#define COLOR_ROWS 7
#define COLOR_COLS 10
#define COLOR_SWATCH_HEIGHT 16
#define COLOR_SWATCH_WIDTH  18
#define COLOR_SWATCH_SPACING 0
#define COLOR_SWATCH_TOP         (COLOR_SWATCH_HEIGHT + 4)
#define COLOR_PICKER_HEIGHT      (((COLOR_ROWS + 6)*COLOR_SWATCH_HEIGHT) + 26)
#define COLOR_PICKER_WIDTH       (COLOR_COLS*COLOR_SWATCH_WIDTH + 7)
#define BUTTON_WIDTH        (4*COLOR_SWATCH_WIDTH - 4)

// Index to the first USER custom color, which follows the Netscape colors
//   (first custom color, the "last-used" is actually 0 in color array)
// Add 1 for the Last-Used color swatch above main color grid
//   and another for the "Current color" swatch above quick palette
#define FIRST_CUSTOM_COLOR_INDEX (MAX_NS_COLORS + 2)

////////////////////////////////////////////////////////////////////////////////////////////
// CColorPicker     Widget for picking colors
// We can't use Pretranslate or SetCapture under Win16, 
//  so we must derive from CWnd, not CDialog

CColorPicker::CColorPicker(CWnd      * pParent,
                     MWContext * pMWContext,
                     COLORREF    crCurrentColor,
                     COLORREF    crDefColor,
                     UINT        nIDCaption,
                     RECT      * pCallerRect)
	: CWnd(),
      m_pParent(pParent),
      m_pMWContext(pMWContext),
      m_crCurrentColor(crCurrentColor),
      m_crDefColor(crDefColor),
      m_nIDCaption(nIDCaption),
      m_pToolTip(0),
      m_bOtherDown(0),
      m_bAutoDown(0),
      m_bHelpDown(0),
      m_bRunning(TRUE),
      m_bFirstMouseUp(TRUE),
      m_bMouseDown(FALSE),
      m_crDragColor(0)
{
    POINT ptOrigin;

    if( pCallerRect )
    {
        m_CallerRect = *pCallerRect;
    } else {
        m_CallerRect.left = m_CallerRect.right = m_CallerRect.top = m_CallerRect.bottom = 0;
    }
    m_LoColor.red = m_LoColor.green = m_LoColor.blue = 0;
    
    // If current is DEFAULT (i.e. "Automatic"), get appropriate default color
    if( m_crCurrentColor == DEFAULT_COLORREF )
    {
        if( crDefColor == DEFAULT_COLORREF || crDefColor == MIXED_COLORREF )
        {
            // Get the "Default" color used by the Browser
	        COLORREF tmpColor,defColor;
	        XP_Bool bCust;
	        PREF_GetColorPrefDWord("browser.foreground_color",&tmpColor);
	        PREF_GetDefaultColorPrefDWord("browser.foreground_color",&defColor);
	        PREF_GetBoolPref("browser.custom_text_color",&bCust);
            m_crCurrentColor = bCust ? tmpColor : defColor;
        } else if( crDefColor == BACKGROUND_COLORREF )
        {

            // Get current page's background color
            //   as "default" for table backgrounds
            CDCCX *pDC = VOID2CX(pMWContext->fe.cx, CDCCX);
            m_crCurrentColor = pDC->m_rgbBackgroundColor;
        } else
        {
            // Use the default color supplied
            m_crCurrentColor = crDefColor;
        }
    }  
   
    if( m_CallerRect.left == 0 && m_CallerRect.top == 0 )
    {
        // No caller rect supplied - locate top at cursor Y and 
        // X so cursor pt. is at horizontal center
        GetCursorPos(&ptOrigin);
        ptOrigin.x -= (ED_TB_BUTTON_WIDTH / 2);
    } else {
        // The -1 lines up light highlight with that of parent
        ptOrigin.x = m_CallerRect.left - 1;
        // We may change this below if near bottom of screen
        ptOrigin.y = m_CallerRect.bottom;
    }

    if (!CWnd::CreateEx(WS_EX_TOPMOST, 
                        AfxRegisterWndClass(CS_SAVEBITS | CS_VREDRAW,
                                            ::LoadCursor( NULL, IDC_ARROW),
                                            sysInfo.m_hbrBtnFace),
                        NULL, WS_POPUP /*| WS_VISIBLE*/ | WS_DLGFRAME,
                        ptOrigin.x, ptOrigin.y,
                        COLOR_PICKER_WIDTH, COLOR_PICKER_HEIGHT,
                        pParent->m_hWnd, NULL, NULL))
    {
        TRACE0("Warning: creation of CColorPicker window failed\n");
        return;
    }
    
    SetFont(wfe_pFont);

    // Add a tooltip control
	m_pToolTip = new CNSToolTip2;

    if(m_pToolTip && !m_pToolTip->Create(this, TTS_ALWAYSTIP) )
    {
       TRACE("Unable To create ToolTip\n");
       delete m_pToolTip;
       m_pToolTip = NULL;
    } else {
        // Lets use speedy tooltips
        m_pToolTip->SetDelayTime(200);
#ifdef XP_WIN32
        // We MUST do this for MFC tooltips
        EnableToolTips(TRUE);
#endif // WIN32
    }

    int iTop = 2;
    int iCustomTop;
    int iLeft = 4;
    int iCol = 0;
    int iRow = 0;
    int iRowLimit = COLOR_ROWS - 1; // Doesn't include custom colors
    //int iXPOffset = 0;
    int iCustomColor = 0; // Counter for custom colors

    RECT rectOther; // For the "Other..." button
    RECT rect;
    RECT rectLabel;
    char *pLabel;   // Holds static pointer for szLoadString (dont free)

    for( int i = 0; i < MAX_COLORS; i++ )
    {
       	LO_Color LoColor;

        if( i > 0 && i <= MAX_NS_COLORS ) // in main color grid
        {
            // Set top to be used for all colors in main region
            iTop = COLOR_SWATCH_TOP;
            EDT_GetNSColor(i-1, &LoColor);
            m_crColors[i] = RGB(LoColor.red, LoColor.green, LoColor.blue);
        } 
        else
        {
            if( i == 0 )
            {                
                // The very first Custom color is actually the "last-used" color
                m_crColors[i] = wfe_crLastColorPicked;

                pLabel = szLoadString(IDS_LAST_USED_COLOR);
                rectLabel.top = iTop + 1;
                rectLabel.left = COLOR_SWATCH_WIDTH + 2;
                rectLabel.right = COLOR_PICKER_WIDTH;
                rectLabel. bottom = rectLabel.top + COLOR_SWATCH_HEIGHT;
                if( !m_LastUsedLabel.Create(pLabel, WS_VISIBLE | WS_CHILD, rectLabel, this) )
                    return;
            }
            else if( i == MAX_NS_COLORS+1 ) // Should = FIRST_CUSTOM_COLOR_INDEX-1
            {
                // Current color swatch just below the main colors
                m_crColors[i] = m_crCurrentColor;

                // Set text label next to this color swatch
                pLabel = szLoadString(IDS_CURRENT_COLOR);

                iTop = COLOR_SWATCH_TOP + (COLOR_ROWS * COLOR_SWATCH_HEIGHT) + 2;
                rectLabel.top = iTop + 1;
                rectLabel.left = COLOR_SWATCH_WIDTH + 2;
                rectLabel.right = COLOR_PICKER_WIDTH - 6 - BUTTON_WIDTH;
                rectLabel. bottom = rectLabel.top + COLOR_SWATCH_HEIGHT;
                if( !m_CurrentLabel.Create(pLabel, WS_VISIBLE | WS_CHILD, rectLabel, this) )
                    return;
                iCol = 0;
                iRow = 0;
            }
            else if( i >= FIRST_CUSTOM_COLOR_INDEX )
            {                
                // Get custom colors from global palette
                m_crColors[i] = wfe_CustomPalette[iCustomColor++];

                if( i == FIRST_CUSTOM_COLOR_INDEX )
                {
                    // Add the "Automatic" button ( = "Default", i.e., no color in HTML)
                    //   at the same vertical location as "Current color" swatch
                    //  (Must create here to get tab order correct)
                    rectOther.top = iTop + 2;
                    rectOther.bottom = rectOther.top + 22;
                    rectOther.right = COLOR_PICKER_WIDTH - 8;
                    rectOther.left = rectOther.right - BUTTON_WIDTH;

                    m_OtherButton.Create(szLoadString(IDS_OTHER_BUTTON), BS_PUSHBUTTON|WS_CHILD|WS_VISIBLE|WS_GROUP|WS_TABSTOP,
                                         rectOther, this, IDC_CHOOSE_COLOR); 
                
                    // TODO: ADD A TOOLTIP FOR "AUTO" BUTTON for added instructions?
                    
                    // Label above custom colors:
                    pLabel = szLoadString(IDS_CUSTOM_COLORS_LABEL);
                    rectLabel.left = 2;
                    rectLabel.right = rectOther.left - 1;
                    rectLabel.top = iTop + COLOR_SWATCH_HEIGHT + 2;
                    // We cut 2 pixels off font height for tighter fit (TODO: PROBLEM IN OTHER LANGUAGES?)
                    rectLabel.bottom = rectLabel.top + wfe_iFontHeight - 2;
                    if( !m_CustomColorsLabel.Create(pLabel, WS_VISIBLE | WS_CHILD, rectLabel, this) )
                        return;
        
                    // Top of custom colors row - place just below label
                    iTop = iCustomTop = rectLabel.bottom;
                    iCol = 0;
                    iRow = 0;
                }
            }
            LoColor.red = GetRValue(m_crColors[i]);
            LoColor.green = GetGValue(m_crColors[i]);
            LoColor.blue = GetBValue(m_crColors[i]);
        }

        rect.left = iCol*COLOR_SWATCH_WIDTH;
        rect.top = iTop + (iRow*COLOR_SWATCH_HEIGHT);
        rect.right = rect.left + COLOR_SWATCH_WIDTH;
        rect.bottom = rect.top + COLOR_SWATCH_HEIGHT;

        // Create our custom buttons
        m_pColorButtons[i] = new CColorButton(&m_crColors[i], &m_crColor);
        if( !m_pColorButtons[i] ||
            !m_pColorButtons[i]->Create(rect, this, IDC_LAST_USED_COLOR+i, &m_crColors[i], TRUE) )
        {
            return;
        }
        // Add tooltip showing color formated as "R=xxx G=xxx B=xxx  HTML: #FFEEAA"
        if( m_pToolTip )
        {
            if( m_crColors[i] == DEFAULT_COLORREF )
            {
                *m_ppTipText[i] = 0;
            } else 
            {
                wsprintf(m_ppTipText[i], szLoadString(IDS_COLOR_TIP_FORMAT),
                         LoColor.red, LoColor.green, LoColor.blue);
                strcat(m_ppTipText[i], szLoadString(IDS_COLOR_TIP_HTML));
                char * pEnd = m_ppTipText[i] + strlen(m_ppTipText[i]);
                sprintf(pEnd, "#%02X%02X%02X",LoColor.red, LoColor.green, LoColor.blue);

                m_pToolTip->AddTool(m_pColorButtons[i], m_ppTipText[i], &rect, IDC_LAST_USED_COLOR+i);
            }
        }

        if( i >= FIRST_CUSTOM_COLOR_INDEX )
        {
            // We are within single custom color row
            iCol++;
        }
        else if( i > 0 )
        {
            // Main color grid
            // Add each button in columns first, add new column after every 7
            if(iRow < iRowLimit)
            {
                iRow++;
            } else {
                iRow = 0;
                iCol++;
            }
        }
    }

    // Add Number strings under custom color swatches
    rect.left = 6;
    rect.top = iCustomTop + COLOR_SWATCH_HEIGHT;
    rect.bottom = rect.top + COLOR_SWATCH_HEIGHT;

    for( i = 0; i < MAX_CUSTOM_COLORS; i++  )
    {
        rect.right = rect.left + COLOR_SWATCH_WIDTH;
        char pNumber[16];
        wsprintf(pNumber, "%d", (i < MAX_CUSTOM_COLORS-1) ? i+1 : 0);
        if( !m_CustomColorNumber[i].Create(pNumber, WS_VISIBLE | WS_CHILD, rect, this) )
            return;
        // Move to next location
        rect.left += COLOR_SWATCH_WIDTH;
    }

    // Place "Browser default" button in lower left corner:    
    rect.top = rect.bottom;
    rect.bottom = rect.top + 22;
    rect.left = 2;
    rect.right = rect.left + BUTTON_WIDTH + 2*COLOR_SWATCH_WIDTH; // BUTTON_WIDTH

    m_DefaultButton.Create(szLoadString(IDS_DEFAULT_BUTTON), BS_PUSHBUTTON|WS_CHILD|WS_VISIBLE|WS_GROUP|WS_TABSTOP,
                                        rect, this, IDC_DEFAULT_COLOR); 
    
    // Now we can determine bottom of entire window
    // Number added to this must be enough for 2 lines of text (this is now 30)
    // This is weird, but adding about 4 results in same place as "Other" button button,
    //  seems like button is larger than 24???
    int iTotalHeight = rect.bottom + 8;

    // Place "Help" color button in lower right corner:    
    rect.right = COLOR_PICKER_WIDTH - 8;
    rect.left = rect.right - BUTTON_WIDTH;
    // NOTE: IDS_HELP_BUTTON string is in Browser resources
    m_HelpButton.Create(szLoadString(IDS_HELP_BUTTON), BS_PUSHBUTTON|WS_CHILD|WS_VISIBLE|WS_GROUP|WS_TABSTOP,
                         rect, this, IDC_COLOR_HELP); 
    

    // Set this so color doesn't change if no interaction?
    m_crColor = WFE_GetCurrentFontColor(m_pMWContext);

    // Set our font for all static strings used
    if( wfe_pFont )
    {
        // Set the font to our usual 8-pt font
        m_OtherButton.SetFont(wfe_pFont);
        m_HelpButton.SetFont(wfe_pFont);
        m_LastUsedLabel.SetFont(wfe_pFont);
        m_CurrentLabel.SetFont(wfe_pFont);
        m_DefaultButton.SetFont(wfe_pFont);
        m_CustomColorsLabel.SetFont(wfe_pFont);
        for( int i = 0; i < MAX_CUSTOM_COLORS; i++ )
        {
            m_CustomColorNumber[i].SetFont(wfe_pFont);
        }
    }
    
    // Check if we need to move up because we would go off the screen
    if( (m_CallerRect.bottom + iTotalHeight) > sysInfo.m_iScreenHeight )
    {
        // Locate toolbar above the caller button
        //   if toolbar bottom would extend off bottom of screen
        ptOrigin.y = m_CallerRect.top - iTotalHeight;
    }
    // Move and/or resize the window and show it for the first time
    SetWindowPos(NULL, ptOrigin.x, ptOrigin.y, COLOR_PICKER_WIDTH, iTotalHeight, SWP_SHOWWINDOW );

    // Capture mouse so we can detect clicking off the dialog
    SetCapture();
    // Restore active caption highlighting to parent frame
    GetParentFrame()->SetActiveWindow();
}

COLORREF crReturnColor;

// Call this instead of DoModal() to wait in dialog and get result
COLORREF CColorPicker::GetColor(LO_Color * pLoColor)
{

    // Wait here until user selects a color or
    //   cancels the dialog with ESC or by clicking outside window
    while( m_bRunning ) FEU_StayingAlive();

    // Return selected or custom color
    if( pLoColor )
    {
        pLoColor->red = m_LoColor.red;
        pLoColor->green = m_LoColor.green;
        pLoColor->blue = m_LoColor.blue;
    }
    crReturnColor = m_crColor;
    DestroyWindow();
    return crReturnColor;
}

void CColorPicker::SetColorAndExit()
{
    if( m_crColor != DEFAULT_COLORREF )
    {
        m_LoColor.red = GetRValue(m_crColor);
        m_LoColor.green = GetGValue(m_crColor);
        m_LoColor.blue = GetBValue(m_crColor);
    }
    // Set flag that will let us return to caller and destroy dialog
    m_bRunning = FALSE;
}

void CColorPicker::CancelAndExit()
{
    m_crColor = CANCEL_COLORREF;
    m_bRunning = FALSE;
}

CColorPicker::~CColorPicker()
{
}

void CColorPicker::PostNcDestroy() 
{
    char pPref[32];
    char pColorString[32];

    // Save the last-picked color
    wsprintf(pColorString, "%d,%d,%d", GetRValue(wfe_crLastColorPicked), GetGValue(wfe_crLastColorPicked), GetBValue(wfe_crLastColorPicked));
    PREF_SetCharPref("editor.last_color_picked", pColorString);

    // Save custom colors back to prefs
    for ( int i = 0; i < MAX_CUSTOM_COLORS; i++ )
    {
        // Write new pref only if different than original value
        if( m_bColorChanged[i] )
        {
            COLORREF crColor = wfe_CustomPalette[i]; // should = m_crColors[iColorIndex];
            wsprintf(pColorString, "%d,%d,%d", GetRValue(crColor), GetGValue(crColor), GetBValue(crColor));
            wsprintf(pPref, "editor.custom_color_%d", i);
            PREF_SetCharPref(pPref, pColorString);
        }
    }

    if( m_pToolTip )
        delete m_pToolTip;

 	ReleaseCapture();
	CWnd::PostNcDestroy();
}

BEGIN_MESSAGE_MAP(CColorPicker, CWnd)
	//{{AFX_MSG_MAP(CColorPicker)
	ON_WM_LBUTTONUP()
  	ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
#endif
END_MESSAGE_MAP()

BOOL CColorPicker::PreTranslateMessage(MSG* pMsg)
{
    if( m_pToolTip && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
    {
        m_pToolTip->RelayEvent(pMsg);
    }
    if( pMsg->message == WM_KEYDOWN )
    {
        if( pMsg->wParam == VK_ESCAPE )
        {
            CancelAndExit();
        } 
        else if( pMsg->wParam == VK_F1 )
        {
            OnColorHelp();
        }
        else 
        {
            // Are any number keys pressed?
            // This is 1 less than number on key pressed
            int iNumberIndex = pMsg->wParam - '1';
            // "0" key is AFTER "9" on keyboard
            //   so treat it like a 10th color
            if( iNumberIndex == -1 )
                iNumberIndex = 9;

            if( iNumberIndex >= 0 && iNumberIndex <= 9 ) // This won't work if MAX_CUSTOM_COLORS > 10
            {
                // Set the current focus color at the custom color matching number
                m_crColors[FIRST_CUSTOM_COLOR_INDEX+iNumberIndex] = m_crColor;
                m_pColorButtons[FIRST_CUSTOM_COLOR_INDEX+iNumberIndex]->Invalidate(FALSE);
            }
        }
    }
    return CWnd::PreTranslateMessage(pMsg);
}

BOOL CColorPicker::IsMouseOverDlg(CPoint cPoint)
{
    CRect cRect;
    GetWindowRect(&cRect);
    ScreenToClient(&cRect);
    return cRect.PtInRect(cPoint);
}

BOOL CColorPicker::IsMouseOverButton(CPoint cPoint, UINT nID)
{
    CRect cRect;
    ((CButton*)GetDlgItem(nID))->GetWindowRect(&cRect);
    ScreenToClient(&cRect);
    return cRect.PtInRect(cPoint);
}

int CColorPicker::GetMouseOverColorIndex(CPoint cPoint)
{
    CRect cRect;
    for( int i = 0; i < MAX_COLORS; i++){
        GetDlgItem(IDC_LAST_USED_COLOR+i)->GetWindowRect(&cRect);
        ScreenToClient(&cRect);
        if( cRect.PtInRect(cPoint) ){
            // Select the color and return the index
            //  unless it is "default" (unknown)
            if( m_crColors[i] != DEFAULT_COLORREF )
            {
                m_crColor = m_crColors[i];
                return i;
            }
        }
    }
    return -1;
}

void CColorPicker::OnLButtonDown(UINT nFlags, CPoint cPoint)
{
    POINT pt = {cPoint.x,cPoint.y};
    // If user clicks outside of dialog, we are done
    if( !IsMouseOverDlg(cPoint) ){
        CancelAndExit();
        return;
    }

    if( IsMouseOverButton(cPoint, IDC_CHOOSE_COLOR) && !m_bOtherDown ){
        m_bOtherDown = TRUE;
        ((CButton*)GetDlgItem(IDC_CHOOSE_COLOR))->SetState(TRUE);
    } else if( IsMouseOverButton(cPoint, IDC_DEFAULT_COLOR) && !m_bAutoDown ){
        m_bAutoDown = TRUE;
        ((CButton*)GetDlgItem(IDC_DEFAULT_COLOR))->SetState(TRUE);
    } else if( IsMouseOverButton(cPoint, IDC_COLOR_HELP) && !m_bHelpDown ){
        m_bHelpDown = TRUE;
        ((CButton*)GetDlgItem(IDC_COLOR_HELP))->SetState(TRUE);
    }
    
    // Copy color for possible dragging
    int i = GetMouseOverColorIndex(cPoint);
    if( i >= 0 ){
        m_crDragColor = m_crColors[i];
        m_bMouseDown = TRUE;
        m_iMouseDownColorIndex = i;

        SetCursor(theApp.LoadCursor(IDC_DRAG_COLOR));
    }
    CWnd::OnLButtonDown(nFlags, cPoint);
}

void CColorPicker::OnLButtonUp(UINT nFlags, CPoint cPoint)
{
    // We get this when user "dragged" over combobox to trigger
    //  dialog, then move away and let mouse up. Quit just like combo-list would
    if( !IsMouseOverDlg(cPoint) )
    {
        // But check if over the caller rect
        CRect cRect(m_CallerRect);
        ScreenToClient(&cRect);
        if( !cRect.PtInRect(cPoint) )
        {
            CancelAndExit();
            return;
        }
    } else {
        // Detect mouse up on a color swatch select it
        //  or drop copied color
        int i = GetMouseOverColorIndex(cPoint);

        // Ignore first mouse up on current color
        if( i == 0 && m_bFirstMouseUp )
            return;

        m_bFirstMouseUp = FALSE;

        if( i >= 0 )
        {
            if( m_bMouseDown && i >= FIRST_CUSTOM_COLOR_INDEX &&
                m_iMouseDownColorIndex != i) // TRUE when click down/up to select quick color
            {
                // We are dropping a color onto custom palette
                m_crColors[i] = m_crDragColor;
                int iCustomIndex = i - FIRST_CUSTOM_COLOR_INDEX;
                wfe_CustomPalette[iCustomIndex] = m_crDragColor;
                m_bColorChanged[iCustomIndex] = TRUE;

                // Force redraw
                m_pColorButtons[i]->Invalidate(FALSE);
            } else if( !m_bMouseDown || m_iMouseDownColorIndex == i )
            {
                // Button came up in same color as mouse down,
                //  or up when not dragging is not possible
                //  because mouse was already down when picker started
                m_crColor = m_crColors[i];
                // Always save selected color as the "last color picked"
                wfe_crLastColorPicked = m_crColor;
                SetColorAndExit();
                return;
            }
            // We dropped color or didn't select,
            //  but we must be over a color swatch,
            //  so set appropriate cursor
            SetCursor(theApp.LoadCursor(IDC_DRAG_ARROW));
        }

        if( IsMouseOverButton(cPoint, IDC_CHOOSE_COLOR) )
        {
            OnChooseColor();
            return;
        }
        if( IsMouseOverButton(cPoint, IDC_DEFAULT_COLOR) )
        {
            OnDefaultColor();
            return;
        }
        if( IsMouseOverButton(cPoint, IDC_COLOR_HELP) )
        {
            ((CButton*)GetDlgItem(IDC_COLOR_HELP))->SetState(FALSE);
            m_bHelpDown = FALSE;
            OnColorHelp();
        }
    }
    m_bMouseDown = FALSE;
    CWnd::OnLButtonUp(nFlags, cPoint);    
}

void CColorPicker::SetButtonState(CPoint cPoint, UINT nID, BOOL* pButtonDown )
{
    BOOL bOverButton = IsMouseOverButton(cPoint, nID);

    if( !bOverButton && *pButtonDown)
    {
        // Deselect button when mouse moves off
        *pButtonDown = FALSE;
        ((CButton*)GetDlgItem(nID))->SetState(FALSE);
    } else if( bOverButton && !*pButtonDown /*&& (nFlags & MK_LBUTTON)*/ )
    {
        *pButtonDown = TRUE;
        ((CButton*)GetDlgItem(nID))->SetState(TRUE);
    }
}

void CColorPicker::OnMouseMove(UINT nFlags, CPoint cPoint)
{
    int i = GetMouseOverColorIndex(cPoint);
    if( i != 0 )
    {
        // If not over Current Color button,
        //   kill ignore-on-first-mouseup behavior
        m_bFirstMouseUp = FALSE;
    }

    // Set button shade state to act like normal dialog button
    //  when mouse is held down
    if( nFlags & MK_LBUTTON )
    {
        SetButtonState(cPoint, IDC_CHOOSE_COLOR, &m_bOtherDown);    
        SetButtonState(cPoint, IDC_DEFAULT_COLOR, &m_bAutoDown);    
        SetButtonState(cPoint, IDC_COLOR_HELP, &m_bHelpDown);    
    }

    if( m_bMouseDown )
    {
        // Give user feedback via cursor
        UINT nIDC;
        if( m_iMouseDownColorIndex == i )
            // Over source swatch - NULL icon is confusing - show arrow + color box
            nIDC = IDC_DRAG_COLOR;
        else if( i >= FIRST_CUSTOM_COLOR_INDEX )
            // Over drop target
            nIDC = IDC_DROP_COLOR;
        else 
            // Over anything else - can't drop
            nIDC = IDC_NO_DROP;

        SetCursor(theApp.LoadCursor(nIDC));
    } else {
        if( i >= 0 )
        {
            // Tells user they can drag a color
            SetCursor(theApp.LoadCursor(IDC_DRAG_ARROW));
            if( GetFocus() != m_pColorButtons[i] )
            {
                // Bring focus to color where mouse is
                m_pColorButtons[i]->SetFocus();
                m_crColor = m_crColors[i];
            }
        }
        else
        {
            // Not over color or dragging - regular cursor
            SetCursor(theApp.LoadStandardCursor(IDC_ARROW));
        }
    }
    CWnd::OnMouseMove(nFlags, cPoint);    
}

// 
#ifdef XP_WIN32 
// This is needed for non-CFrame owners of a CToolTipCtrl
BOOL CColorPicker::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
    UINT nID =pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND)
    {
        // idFrom is actually the HWND of the tool
        nID = ::GetDlgCtrlID((HWND)nID);
        if( nID ) {
            int i = nID - IDC_LAST_USED_COLOR;
            if( i >= 0 && i < MAX_COLORS && m_ppTipText[i] ){
                // Copy string into buffer - limit is 80 characters
                strcpy( pTTT->szText, m_ppTipText[i]);
                return(TRUE);
            }
        }
    }
    return(FALSE);
}
#endif

void CColorPicker::OnDefaultColor()
{
    m_crColor = DEFAULT_COLORREF;

    SetColorAndExit();
}

// Select any color via Window's color dialog
void CColorPicker::OnChooseColor()
{
    // Use CC_PREVENTFULLOPEN in flags (2nd) param if we want to suppress editing
   	CColorDialog dlg(wfe_CustomPalette[0], CC_FULLOPEN, this);
    
    // Place our current palette into the 16 Custom colors
    // Copy colors so we don't change anything if 
    COLORREF crCustomColors[16];
    memcpy((void*)crCustomColors, (void*)wfe_CustomPalette, sizeof(crCustomColors));
    
    dlg.m_cc.lpCustColors = crCustomColors;
    dlg.m_cc.lStructSize = sizeof( dlg.m_cc );

    // TODO: We need to make a derived class to control common dialog palette 
    //  or do hook for WM_INTDIALOG in m_cc struct
    //  so we can set the palette of the Window's CColorDlg

    UINT nResult = dlg.DoModal();

    if( nResult == IDOK ){
        m_crColor = dlg.GetColor();
        // Save this color as last color picked
        wfe_crLastColorPicked = m_crColor;
        // Copy custom colors back to global palette
        memcpy((void*)wfe_CustomPalette, (void*)crCustomColors, sizeof(crCustomColors));
        m_bColorChanged[0] = TRUE;
        SetColorAndExit();
    } else {
        CancelAndExit();
    }
}

void CColorPicker::OnColorHelp()
{
    NetHelp(HELP_COLOR_PICKER);
}

/////////////////////////////////////////////////////////////////////////////
// CDropdownToolbar  Popup window with thin border to place a vertical row of buttons

CDropdownToolbar::CDropdownToolbar(CWnd      * pParent,
                                   MWContext * pMWContext,
                                   RECT      * pCallerRect,
                                   UINT      nCallerID,
                                   UINT      nInitialID)
	: CWnd(),
      m_pParent(pParent),
      m_pMWContext(pMWContext),
      m_nCallerID(nCallerID),
      m_nInitialID(nInitialID),
      m_nButtonCount(0),
      m_nAllocatedCount(0),
      m_pData(NULL),
      m_pToolTip(0),
      m_bFirstButtonUp(TRUE)
{
    if( pCallerRect ){
        m_CallerRect = *pCallerRect;
    } else {
        m_CallerRect.left = m_CallerRect.right = m_CallerRect.top = m_CallerRect.bottom = 0;
    }

    if (!CWnd::CreateEx(WS_EX_TOPMOST, AfxRegisterWndClass(CS_SAVEBITS | CS_VREDRAW, ::LoadCursor( NULL, IDC_ARROW)),
                      NULL, WS_POPUP | WS_BORDER /*|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS*/,
                      0, 0, 0, 0, pParent->m_hWnd, NULL, NULL))
    {
        TRACE0("Warning: creation of CDropdownToolbar window failed\n");
        return;
    }

    // Add a tooltip control
	m_pToolTip = new CNSToolTip2;

    if(m_pToolTip && !m_pToolTip->Create(this, TTS_ALWAYSTIP) ){
       TRACE("Unable To create ToolTip\n");
       delete m_pToolTip;
       m_pToolTip = NULL;
    }
    if( m_pToolTip ){
        m_pToolTip->Activate(TRUE);
        // Lets use speedy tooltips
        m_pToolTip->SetDelayTime(50);
#ifdef XP_WIN32
        // We MUST do this for MFC tooltips
        EnableToolTips(TRUE);
#endif // WIN32
    }
}

CDropdownToolbar::~CDropdownToolbar()
{
    if( m_pData ){
        for( UINT i = 0; i < m_nButtonCount; i++ ){
            if( m_pData[i].pButton ){
                if( m_pData[i].pButtonName ) XP_FREE((void*)m_pData[i].pButtonName);
                delete m_pData[i].pButton;
            }
        }
        XP_FREE( m_pData );
    }
    if( m_pToolTip ){
        delete m_pToolTip;
    }
}

void CDropdownToolbar::PostNcDestroy() 
{
	ReleaseCapture();
	CWnd::PostNcDestroy();
    delete this;
}

// Create a CBitmapPushButton - resource name should be correct form for
//   a CBitmapButton: "Button_" will load "Button_D.BMP" and "Button_U.BMP"
//   bitmap files for Down and Up button images.
BOOL CDropdownToolbar::AddButton(LPSTR pButtonName, UINT nCommandID)
{
    ASSERT(pButtonName);
    ASSERT(nCommandID);
    if( m_nAllocatedCount < (m_nButtonCount + 1) ){
        m_nAllocatedCount += 8;
        m_pData = (DropdownToolbarData*)XP_REALLOC(m_pData,
                            m_nAllocatedCount * sizeof(DropdownToolbarData));
    }
    if( m_pData ){
        DropdownToolbarData *pData = &m_pData[m_nButtonCount];

		pData->pButton = new CBitmapPushButton(TRUE); // Use "borderless" button style
        if( ! pData->pButton ){
            TRACE0("Failed to allocated new button for DropdownToolbar\n");
            return FALSE;
        }
        pData->nCommandID = nCommandID;
        pData->nBitmapID = 0;
        pData->pButtonName = XP_STRDUP(pButtonName);
    } else {
        TRACE0("Failed to reallocated new DropdownToolbar data\n");
        return FALSE;
    }
    m_nButtonCount++;
    return TRUE;
}

BOOL CDropdownToolbar::AddButton(UINT nBitmapID, UINT nCommandID)
{
    ASSERT(nBitmapID);
    ASSERT(nCommandID);
    if( m_nAllocatedCount < (m_nButtonCount + 1) ){
        m_nAllocatedCount += 8;
        m_pData = (DropdownToolbarData*)XP_REALLOC(m_pData,
                            m_nAllocatedCount * sizeof(DropdownToolbarData));
    }
    if( m_pData ){
        DropdownToolbarData *pData = &m_pData[m_nButtonCount];

        pData->pButton = new CBitmapPushButton(TRUE); // Use "borderless" button style
        if( ! pData->pButton ){
            TRACE0("Failed to allocated new button for DropdownToolbar\n");
            return FALSE;
        }
        pData->nCommandID = nCommandID;
        pData->pButtonName = NULL;
        pData->nBitmapID = nBitmapID;
    
    } else {
        TRACE0("Failed to reallocated new DropdownToolbar data\n");
        return FALSE;
    }
    m_nButtonCount++;
    return TRUE;
}

// Call this AFTER adding all buttons to toolbaar
void CDropdownToolbar::Show()
{
    if( m_nButtonCount == 0 )
        return;

    // Full height of toolbar frame
    int   iHeight = (ED_TB_BUTTON_HEIGHT * m_nButtonCount) + 2;
    POINT ptOrigin;
    
    if( m_CallerRect.left == 0 && m_CallerRect.top == 0 ){
        // No caller rect supplied - locate top at cursor Y and 
        // X so cursor pt. is at horizontal center
        GetCursorPos(&ptOrigin);
        ptOrigin.x -= (ED_TB_BUTTON_WIDTH / 2);

    } else {
        // Minus 1 for the 1-pixel dialog border around dropdown
        //  (makes dropdown look centered under caller button)
        ptOrigin.x = m_CallerRect.left + ((m_CallerRect.right - m_CallerRect.left - ED_TB_BUTTON_WIDTH) / 2);
        
        if( (m_CallerRect.bottom + iHeight) > sysInfo.m_iScreenHeight ){
            // Locate toolbar above the caller button
            //   if toolbar bottom would extend off bottom of screen
            ptOrigin.y = m_CallerRect.top - iHeight;
        } else {
            ptOrigin.y = m_CallerRect.bottom;
        }
    }

    // Set location and size (fits tight around all buttons)
    SetWindowPos( &wndTopMost, ptOrigin.x, ptOrigin.y,
                  ED_TB_BUTTON_WIDTH + 2, iHeight, SWP_NOACTIVATE );
    
    // Convert caller rect to toolbar's coordinates
    //  to use for mouse hit testing
    if( m_CallerRect.left != 0 && m_CallerRect.top != 0 ){
        ScreenToClient(&m_CallerRect);
    }

    // Create all the buttons and load bitmaps etc
    if( m_pData && m_nButtonCount > 0 ){
        DropdownToolbarData *pData = m_pData;
        RECT rect = {0, 0, ED_TB_BUTTON_WIDTH, ED_TB_BUTTON_HEIGHT-1};

        for( UINT i = 0; i < m_nButtonCount; i++, pData++ ){
            UINT nStyle = BS_PUSHBUTTON | BS_OWNERDRAW | WS_CHILD;
            if( i == 0 ){
                nStyle |= WS_GROUP | WS_TABSTOP;
            }
            rect.bottom++;
            if( ! pData->pButton->Create(pData->pButtonName, nStyle, 
                                         rect, this, pData->nCommandID) ){
                TRACE0("Failed to allocated new button in DropdownToolbar\n");
        
            BUTTON_FAILED:
                delete pData->pButton;
                if( pData->pButtonName ) {
                    XP_FREE(pData->pButtonName);
                    pData->pButtonName = NULL;
                }
                pData->pButton = NULL;
                CWnd::DestroyWindow();
                return;
            }
            rect.bottom--;
           	if( pData->pButtonName) {
                CString buttonName(pData->pButtonName);
           	    if( ! pData->pButton->LoadBitmaps(buttonName + _T("U"),
                                                  buttonName + _T("D"), 0, 0) ){
                    TRACE0("Failed to load bitmaps for button in DropdownToolbar\n");
                    goto BUTTON_FAILED;
                }
            } else if( pData->nBitmapID ){
                if( ! pData->pButton->LoadBitmap(pData->nBitmapID) ){
                    TRACE0("Failed to load bitmaps for button in DropdownToolbar\n");
                    goto BUTTON_FAILED;
                }
            } else {
                TRACE0("No bitmap name or ID supplied for button in DropdownToolbar\n");
                goto BUTTON_FAILED;
            }
            // Set pushed-down state for button if requested
            if( m_nInitialID > 0 && pData->nCommandID == m_nInitialID ){
                pData->pButton->SetCheck(TRUE);
            }
            pData->pButton->ShowWindow(SW_SHOW);

            // Add tooltip for the button
            if( m_pToolTip ){
#ifdef XP_WIN16
                m_pToolTip->DelTool(pData->pButton->m_hWnd, pData->nCommandID);
#endif
                m_pToolTip->AddTool(pData->pButton, pData->nCommandID,
                                    &rect, pData->nCommandID);
            }
            
            // Move to next button down
            rect.top += ED_TB_BUTTON_HEIGHT;
            rect.bottom += ED_TB_BUTTON_HEIGHT; 
        }
        if( m_pToolTip ){
            m_pToolTip->Activate(TRUE);
        }
    }
    
    ShowWindow(SW_SHOW);
	// Grab the mouse so a click outside the dialog closes toolbar
	SetCapture();
    // Restore active caption highlighting to parent frame
    GetParentFrame()->SetActiveWindow();
}

BEGIN_MESSAGE_MAP(CDropdownToolbar, CWnd)
	//{{AFX_MSG_MAP(CDropdownToolbar)
	ON_WM_LBUTTONUP()
  	ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
#endif
END_MESSAGE_MAP()

BOOL CDropdownToolbar::PreTranslateMessage(MSG* pMsg)
{
    if( m_pToolTip && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
    {
#ifdef XP_WIN32 
        // This is needed to get around an MFC bug
        //   where tooltip is disabled after Modal Dialog is called
        m_pToolTip->Activate(TRUE);
#endif
        m_pToolTip->RelayEvent(pMsg);
    }
    return CWnd::PreTranslateMessage(pMsg);
}

// 
#ifdef XP_WIN32 
// This is needed for non-CFrame owners of a CToolTipCtrl
BOOL CDropdownToolbar::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
    UINT nID =pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND)
    {
        // idFrom is actually the HWND of the tool
        nID = ::GetDlgCtrlID((HWND)nID);
        if(nID) {
            char * pTipText = FEU_GetToolTipText(nID);
            if( pTipText ){
                // Copy string into buffer - limit is 80 characters
                // (use even number for foreign characters
                strncpy( pTTT->szText, pTipText, 78);

                // Just in case tip was longer than 78 characters!
                pTTT->szText[79] = '\0';
                return(TRUE);
            }
        }
    }
    return(FALSE);
}
#endif

void CDropdownToolbar::OnMouseMove(UINT nFlags, CPoint cPoint)
{
    CWnd::OnMouseMove(nFlags, cPoint);

    BOOL bStatusSet = FALSE;
    char * szStatus;
    POINT pt = {cPoint.x, cPoint.y};
    
    if( m_pMWContext && m_nCallerID && m_CallerRect.top != 0 &&
            ::PtInRect(&m_CallerRect, pt) ){
        szStatus = szLoadString(m_nCallerID);
        if( szStatus ){
            // Display status line help for caller button
            // First strip tooltip
            char * pEnter = strchr( szStatus, '\n');
            if( pEnter ){
                *pEnter = '\0';
            }
            wfe_Progress(m_pMWContext, szStatus);
            // We can skip testing toolbar buttons if we are in caller
            bStatusSet = TRUE;
        }
    }
    
    // First button rect
    RECT rect = {0, 0, ED_TB_BUTTON_WIDTH - 1, 
                 ED_TB_BUTTON_HEIGHT - 1};

    for( UINT i = 0; i < m_nButtonCount; i++ ){

        BOOL bInButton = ::PtInRect(&rect, pt);

        // Display status line help if within the toolbar
        if( m_pMWContext && bInButton && !bStatusSet ){
            szStatus = szLoadString(m_pData[i].nCommandID);
            if( szStatus ){
                //  WACKY FEATURE: Look for accelerator, eg. (Ctrl+x) 
                //    in Tooltip portion after a '\n' and move it to 
                //    show in case tooltip isn't showing
                //    (like when mouse button is down)
                char * pEnter = strchr( szStatus, '\n');
                if( pEnter ){
                    char * pParen = strchr( pEnter, '(');
                    if( pParen ){
                        *pEnter = ' ';
                        strcpy(pEnter+1, pParen);
                    } else {
                        // No accerlerator, cut off tooltip
                        *pEnter = '\0';
                    }
                }
                wfe_Progress(m_pMWContext, szStatus );
            } else {
                // If string is missing, a blank would be less 
                //  confusing then restoring the previous string!
                wfe_Progress(m_pMWContext, " ");
            }
        }

        // Set focus to any button if we move over it
        //  and set button as selected if left mouse button is down
        if( bInButton ){
            if( (nFlags & MK_LBUTTON ) && !m_pData[i].pButton->m_bSelected ){
                m_pData[i].pButton->SetState(TRUE);
            }
            if( !m_pData[i].pButton->m_bFocus ){
                m_pData[i].pButton->SetFocus();
            }
            break;
        }
        // Note: We are outside of current button
        // Unselect button we moved off of except if its in "pushdown"
        if( !m_pData[i].pButton->m_bDown &&
                m_pData[i].pButton->m_bSelected ) {
            m_pData[i].pButton->SetState(FALSE);
            break;
        }
        // Next button
        rect.top += ED_TB_BUTTON_HEIGHT;
        rect.bottom += ED_TB_BUTTON_HEIGHT;
    }
}

void CDropdownToolbar::OnLButtonDown(UINT nFlags, CPoint cPoint)
{
    // Find the button pressed and redraw
    CRect rect(0, 0, ED_TB_BUTTON_WIDTH - 1, 
               ED_TB_BUTTON_HEIGHT - 1);
    
    POINT pt = {cPoint.x, cPoint.y};
    for( UINT i = 0; i < m_nButtonCount; i++ ){
	    if( rect.PtInRect(pt) ){
            // Mark button as pressed and has focus
            m_pData[i].pButton->SetState(TRUE);
            m_pData[i].pButton->SetFocus();
            m_pData[i].pButton->Invalidate();
            break;
        }
        rect.top += ED_TB_BUTTON_HEIGHT;
        rect.bottom += ED_TB_BUTTON_HEIGHT;
    }
    CWnd::OnLButtonDown(nFlags, cPoint);
}

// First button up will always destroy toolbar
//   if not in caller button or a toolbar button
void CDropdownToolbar::OnLButtonUp(UINT nFlags, CPoint cPoint)
{
    POINT pt = {cPoint.x, cPoint.y};
    BOOL  bExit = FALSE;
    
    if( m_CallerRect.top != 0 ){
        // Don't destroy toolbar if button came up within the
        //  caller's  button - this allows tooltips to show up
        //  when mouse moves over button when not held down
        // REMOVE THIS FOR TOOLBAR DURING MOUSE-DOWN ONLY
        if( ::PtInRect(&m_CallerRect, pt) ){
            // Ignore the first time button comes up
            //   so a simple up+down click doesn't close toolbar
            if( m_bFirstButtonUp ){
                CWnd::OnLButtonUp(nFlags, cPoint);
                m_bFirstButtonUp = FALSE;
                return;
            }
            // Any other time, just exit
            bExit = TRUE;
        }
    }

    if( !bExit ){
	    // Do Hittest for all buttons
        CRect rect(0, 0, ED_TB_BUTTON_WIDTH - 1, 
                   ED_TB_BUTTON_HEIGHT - 1);

        for( UINT i = 0; i < m_nButtonCount; i++ ){
	        if( rect.PtInRect(pt) ){
                // Post message to parent to do what we're supposed to do
                //   only if different from initial state (for pushbutton style)
                if( m_pParent && m_pData[i].nCommandID != m_nInitialID ){
                    m_pParent->PostMessage(WM_COMMAND, m_pData[i].nCommandID);
                }
                break;
            }
            rect.top += ED_TB_BUTTON_HEIGHT;
            rect.bottom += ED_TB_BUTTON_HEIGHT;
        }
    }

    // Restore previous status message
    if( m_pMWContext ){
        wfe_Progress(m_pMWContext, "");
    }
    DestroyWindow();
}


/////////////////////////////////////////////////////////////////////////////
// CTagDlg dialog - The Arbitrary tag editor


CTagDlg::CTagDlg(CWnd* pParent, 
                 MWContext* pMWContext,         // MUST be supplied!
                 char *pTagData )
	: CDialog(CTagDlg::IDD, pParent),
      m_pMWContext(pMWContext),
      m_bInsert(0),
      m_bValidTag(0)
{
	//{{AFX_DATA_INIT(CTagDlg)
	m_csTagData = pTagData;
	//}}AFX_DATA_INIT
    wfe_GetLayoutViewSize(pMWContext, &m_iFullWidth, &m_iFullHeight);
}


void CTagDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTagDlg)
	DDX_Text(pDX, IDC_EDIT_TAG, m_csTagData);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTagDlg, CDialog)
	//{{AFX_MSG_MAP(CTagDlg)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	ON_BN_CLICKED(IDC_VERIFY_HTML, OnVerifyHtml)
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTagDlg message handlers

#ifdef XP_WIN32
BOOL CTagDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32



BOOL CTagDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

    if( ED_ELEMENT_UNKNOWN_TAG == EDT_GetCurrentElementType(m_pMWContext) ) {
        m_csTagData = EDT_GetUnknownTagData(m_pMWContext);
    } else {
        m_bInsert = TRUE;
    }

	CDialog::OnInitDialog();
  	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

Bool CTagDlg::DoVerifyTag( char* pTagString ){
    ED_TagValidateResult e = EDT_ValidateTag((char*)LPCSTR(m_csTagData), FALSE );
    UINT nIDS = 0;
    switch( e ){
        case ED_TAG_OK:
            return TRUE;
        case ED_TAG_UNOPENED:
            nIDS = IDS_TAG_UNOPENED;
            break;
        case ED_TAG_UNCLOSED:
            nIDS = IDS_TAG_UNCLOSED;
            break;
        case ED_TAG_UNTERMINATED_STRING:
            nIDS = IDS_TAG_UNTERMINATED_STRING;
            break;
        case ED_TAG_PREMATURE_CLOSE:
            nIDS = IDS_TAG_PREMATURE_CLOSE;
            break;
        case ED_TAG_TAGNAME_EXPECTED:
            nIDS = IDS_TAG_TAGNAME_EXPECTED;
            break;
        default:
            nIDS = IDS_TAG_ERROR;
    }

    MessageBox( szLoadString(nIDS), 
                szLoadString(IDS_ERROR_HTML_CAPTION),
                MB_OK | MB_ICONEXCLAMATION);
    return FALSE;
}


void CTagDlg::OnHelp() 
{
    NetHelp(HELP_HTML_TAG);
}

void CTagDlg::OnOK() 
{
    UpdateData(TRUE);
    if( !DoVerifyTag( (char*)LPCSTR(m_csTagData) ) ){
        return;
    }

    EDT_BeginBatchChanges(m_pMWContext);
	CDialog::OnOK();
	
    if( m_bInsert ){
        EDT_InsertUnknownTag(m_pMWContext,(char*)LPCSTR(m_csTagData));
    } else {
        EDT_SetUnknownTagData(m_pMWContext, (char*)LPCSTR(m_csTagData));
    }
    EDT_EndBatchChanges(m_pMWContext);

    //Note: For Attributes-only editing(e.g., HREF JavaScript),
    //      caller must get data from m_csTagData;
}

void CTagDlg::OnVerifyHtml() 
{
    UpdateData(TRUE);
    DoVerifyTag((char*)LPCSTR(m_csTagData));
}

/////////////////////////////////////////////////////////////////////////////
// CHRuleDlg dialog    Horizontal rule properties in single modal dialog


CHRuleDlg::CHRuleDlg(CWnd* pParent, 
	          MWContext* pMWContext,         // MUST be supplied!
              EDT_HorizRuleData* pData )
	: CDialog(CHRuleDlg::IDD, pParent),
      m_pMWContext(pMWContext)
{
    ASSERT(pMWContext);

    if (pData){
        m_pData = pData;
        m_bInsert = FALSE;
    } else {
        m_pData = EDT_NewHorizRuleData();
        m_bInsert = TRUE;
    }

	//{{AFX_DATA_INIT(CHRuleDlg)
	m_nAlign = 1;
	m_bShading = TRUE;
	m_iWidth = 0;
	m_iHeight = 0;
	m_iWidthType = 0;
	//}}AFX_DATA_INIT

    wfe_GetLayoutViewSize(pMWContext, &m_iFullWidth, NULL);
}


void CHRuleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CHRuleDlg)
	DDX_Radio(pDX, IDC_HR_ALIGN_LEFT, m_nAlign);
	DDX_Check(pDX, IDC_HR_SHADOW, m_bShading);
	DDX_Check(pDX, IDC_HR_SAVE_DEFAULT, bSaveDefaultHRule);
	DDX_CBIndex(pDX, IDC_HR_WIDTH_TYPE, m_iWidthType);
	DDX_Text(pDX, IDC_HR_HEIGHT, m_iHeight);
	DDV_MinMaxInt(pDX, m_iHeight, 1, 10000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CHRuleDlg, CDialog)
	//{{AFX_MSG_MAP(CHRuleDlg)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	ON_BN_CLICKED(IDC_EXTRA_HTML, OnExtraHTML)
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()


void CHRuleDlg::OnExtraHTML()
{
    CExtraHTMLDlg dlg(this, &m_pData->pExtra, IDS_HRULE_TAG);
    dlg.DoModal();
}

BOOL ValidateWidth(CDialog* pDlg, int* iWidth, int iWidthType)
{
    char szMessage[256];
    CEdit* pWidthControl = (CEdit*)(pDlg->GetDlgItem(IDC_HR_WIDTH));
    ASSERT (pWidthControl);
    char szWidth[16] = "";
    char *szEndWidth;
    pWidthControl->GetWindowText(szWidth, 15);
    
    int width = (int)strtol( szWidth, &szEndWidth, 10 );

    int iMaxWidth = (iWidthType == 0) ? 100 : 10000;
    
    if( (width < 1  || width > iMaxWidth) || *szEndWidth != '\0'){

        // Construct a string showing correct range
        wsprintf( szMessage, szLoadString(IDS_INTEGER_RANGE_ERROR), 1, iMaxWidth );

        // Notify user with similar message to the DDV_ validation system
        pDlg->MessageBox(szMessage, szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);
        
        // Put focus in the offending control
        // And select all text, just like DDV functions
        pWidthControl->SetFocus();
        pWidthControl->SetSel(0, -1, TRUE);
        return FALSE;
    }
    // Save values if they are good
    if( width > 0 ){
        *iWidth = width;
    }
    return TRUE;
}

void UpdateWidth(CDialog* pPage, int iWidth)
{
    char szBuf[16];
    wsprintf(szBuf, "%d", iWidth);
    pPage->GetDlgItem(IDC_HR_WIDTH)->SetWindowText(szBuf);
}

/////////////////////////////////////////////////////////////////////////////
// CHRuleDlg message handlers

BOOL CHRuleDlg::OnInitDialog() 
{
    if ( m_pMWContext == NULL ||
         m_pData == NULL ) {
        // Must have data and context!
        EndDialog(IDCANCEL);
    }
	
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

    switch (m_pData->align) {
        case ED_ALIGN_LEFT:
            m_nAlign = 0;
            break;
        case ED_ALIGN_RIGHT:
            m_nAlign = 2;
            break;
        default:
            // Default is ED_ALIGN_CENTER
            m_nAlign = 1;
            break;
    }
    m_iWidth = CASTINT(m_pData->iWidth);
    m_iWidthType =  m_pData->bWidthPercent ? 0 : 1;
    m_iHeight = CASTINT(m_pData->size);

    // NoShading is HTML tag,
    //  but its better to show positive attributes,
    //  so we use "3-D Shading", with default = TRUE
    m_bShading = !m_pData->bNoShade;

    CComboBox * pCombo = (CComboBox*)GetDlgItem(IDC_HR_WIDTH_TYPE);
    ASSERT(pCombo);
    pCombo->AddString(szLoadString(IDS_PERCENT_WINDOW));
    pCombo->AddString(szLoadString(IDS_PIXELS));

    // Set caption to indicate a new inserted object
    if ( m_bInsert ){
        SetWindowText( szLoadString(IDS_INSERT_HRULE) );
    }

    UpdateWidth(this, m_iWidth);
  	CDialog::OnInitDialog();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CHRuleDlg::OnHelp() 
{
    NetHelp(HELP_PROPS_HRULE);
}



#ifdef XP_WIN32
BOOL CHRuleDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32



void CHRuleDlg::OnOK() 
{
    if( !UpdateData(TRUE) ||
        !ValidateWidth(this, &m_iWidth, m_iWidthType ) ){
        return;
    }

    EDT_BeginBatchChanges(m_pMWContext);
    switch (m_nAlign) {
        case 0:
            m_pData->align = ED_ALIGN_LEFT;
            break;
        case 1:
            m_pData->align = ED_ALIGN_CENTER;
            break;
        case 2:
            m_pData->align = ED_ALIGN_RIGHT;
            break;
    }
    m_pData->iWidth = m_iWidth;
    m_pData->bWidthPercent = (m_iWidthType == 0);
    m_pData->size = m_iHeight;
    m_pData->bNoShade = !m_bShading;

    if ( m_bInsert ) {
        EDT_InsertHorizRule( m_pMWContext, m_pData );
    } else {
        EDT_SetHorizRuleData( m_pMWContext, m_pData );
    }
    EDT_EndBatchChanges(m_pMWContext);

    if( bSaveDefaultHRule ){
        // Save current values as default preferences
		PREF_SetIntPref("editor.hrule.height",m_iHeight);
		PREF_SetIntPref("editor.hrule.width",m_iWidth);
        if( m_iWidthType == 0 ){
			PREF_SetBoolPref("editor.hrule.width_percent",TRUE);
        } else {
			PREF_SetBoolPref("editor.hrule.width_percent",FALSE);
        }
        if( m_bShading ){
			PREF_SetBoolPref("editor.hrule.shading",TRUE);
        } else {
			PREF_SetBoolPref("editor.hrule.shading",FALSE);
        }
		PREF_SetIntPref("editor.hrule.align",m_pData->align);
    }
	CDialog::OnOK();
}

BOOL CHRuleDlg::DestroyWindow() 
{
	if (m_pData) {
	    EDT_FreeHorizRuleData(m_pData);
    }
	return CDialog::DestroyWindow();
}
/////////////////////////////////////////////////////////////////////////////
// CTargetDlg dialog    Set Target(named anchor) properties in single modal dialog


CTargetDlg::CTargetDlg(CWnd* pParent,
                       MWContext* pMWContext,       // MUST be supplied!
                       char *pName)                 // Existing target
	: CDialog(CTargetDlg::IDD, pParent),
      m_pMWContext(pMWContext),
      m_bInsert(0)
{
    ASSERT(pMWContext);
	//{{AFX_DATA_INIT(CTargetDlg)
	m_csName = pName;
	//}}AFX_DATA_INIT
}


void CTargetDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTargetDlg)
	DDX_Text(pDX, IDC_TARGET_NAME, m_csName);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTargetDlg, CDialog)
	//{{AFX_MSG_MAP(CTargetDlg)
	ON_EN_CHANGE(IDC_TARGET_NAME, OnChangeTargetName)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()

// CTargetDlg message handlers

BOOL CTargetDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

    if( ED_ELEMENT_TARGET == EDT_GetCurrentElementType(m_pMWContext) ) {
        m_csName = EDT_GetTargetData(m_pMWContext);
    } else {
        m_bInsert = TRUE;
        // Use current selected text as suggested target name...
        char *pName =  (char*)LO_GetSelectionText(ABSTRACTCX(m_pMWContext)->GetDocumentContext());
        if ( pName ){
            char *pTemp = pName;
            char *pEnd = pName + min( XP_STRLEN(pName), 50);
            // Skip over leading white-space
            while( XP_IS_SPACE(*pTemp) && pTemp < pEnd ) pTemp++;
            if( pTemp < pEnd ){
                char *pStart = pTemp;
                // Stop at any CR/LF and replace
                //  other whitespace with real spaces (OR UNDERSCORE?)
                while( pTemp <= pEnd ){
                    if( *pTemp == '\n' || *pTemp == '\r' ){
                        // Stop at end of line
                        pEnd = pTemp;
                        break;
                    }
                    if( XP_IS_SPACE(*pTemp) ) *pTemp = ' ';
                    pTemp++;
                }
                pTemp = pEnd;
                // Find last character before the last space from the end
                while( !XP_IS_SPACE(*pTemp) && pTemp != pName ) pTemp--;
                if( pTemp != pName ) *pTemp = '\0';
                m_csName = pStart;
                m_csName.TrimRight();
            }
            XP_FREE(pName);
        }
    }

    // Get the list of existing targets to
    //  warn if target name is already used
    m_pTargetList = EDT_GetAllDocumentTargets(m_pMWContext);

	CDialog::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CTargetDlg::OnHelp() 
{
    NetHelp(HELP_PROPS_TARGET);
}



#ifdef XP_WIN32
BOOL CTargetDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32



void CTargetDlg::OnOK() 
{
    EDT_BeginBatchChanges(m_pMWContext);

	CDialog::OnOK();
    // Remove trailing spaces and quote marks
    CleanupString(m_csName);

    // Strip off hash mark
    if(!m_csName.IsEmpty() && m_csName.GetAt(0) == '#'){
        m_csName = m_csName.Mid(1);;
    }    
    if( !m_csName.IsEmpty() ){
        if( m_bInsert ){
            EDT_InsertTarget(m_pMWContext, (char*)LPCSTR(m_csName));
        } else {
            EDT_SetTargetData(m_pMWContext, (char*)LPCSTR(m_csName));
        }
    }

    EDT_EndBatchChanges(m_pMWContext);
}

void CTargetDlg::OnChangeTargetName()
{
    UpdateData(TRUE);
    // Remove trailing spaces and quote marks, but
    // use temp string to cleanup and don't send string back with UpdateData
    // This upsets caret location etc.
    CString csTemp = m_csName;
    CleanupString(csTemp);
    GetDlgItem(IDOK)->EnableWindow(!csTemp.IsEmpty());
}

////////////////////////////////////////////////////////////////////////////
// Special class to control Background color of static control
//
// CColorStatic

CColorStatic::
	CColorStatic(COLORREF crTextColor,        // Default is black text
	             COLORREF crBackColor,   // on gray Background
	             ED_BorderStyle nStyle)
{
    m_crTextColor = crTextColor;
    m_crBackColor = crBackColor;
    m_nBorderStyle = nStyle;

    // Create a white brush for the Background
   m_brush.CreateSolidBrush(crBackColor|0x02000000); // Like PALETTERGB
}

CColorStatic::~CColorStatic()
{
}

BEGIN_MESSAGE_MAP(CColorStatic, CStatic)
	//{{AFX_MSG_MAP(CColorStatic)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CColorStatic message handlers
BOOL CColorStatic::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam,
                                 LRESULT* pLResult)
{
   int nCtlType = (int)HIWORD(lParam);
 
    // If "message" is not the message you're after, do default processing:
   if (message != WM_CTLCOLOR)
   {
      return CStatic::OnChildNotify(message,wParam,lParam,pLResult);
   }
 
   HDC hdcChild = (HDC)wParam;
   SetTextColor(hdcChild, m_crTextColor);
   SetBkColor(hdcChild, m_crTextColor);
 
    // Send what would have been the return value of OnCtlColor() - the brush
    // handle - back in pLResult:
   *pLResult = (LRESULT)(m_brush.GetSafeHandle());
 
    // Return TRUE to indicate that the message was handled:
   return TRUE;
}

#define MAX_META_NAME_LEN   100
#define MAX_META_VALUE_LEN  400
enum {
    EDIT_NONE,
    EDIT_EQUIV,
    EDIT_META
};
/////////////////////////////////////////////////////////////////////
// Document Properties Pages
//
// First page = general info
//    "Fixed" Meta Tags we always include
//
CDocInfoPage::CDocInfoPage(CWnd* pParent, MWContext * pMWContext,
                           CEditorResourceSwitcher * pResourceSwitcher,
                           EDT_PageData * pPageData)
    : CNetscapePropertyPage(CDocInfoPage::IDD),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher),
      m_bActivated(0),
      m_pPageData(pPageData)
{
    //{{AFX_DATA_INIT(CDocInfoPage)
	m_csAuthor = _T("");
	m_csClassification = _T("");
	m_csDescription = _T("");
	m_csTitle = _T("");
	m_csURL = _T("");
	m_csKeyWords = _T("");
	//}}AFX_DATA_INIT
    ASSERT(pMWContext);
    ASSERT(pPageData);

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}

////////////////////////////////////////////////////////////////////////
BOOL CDocInfoPage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated){
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);

    // Switch back to EXE's resources
    if( m_pResourceSwitcher ){
        m_pResourceSwitcher->Reset();
    }

	m_bActivated = TRUE;
    
    // This should be same as hist_ent->title??
    m_csTitle = m_pPageData->pTitle;

	History_entry * hist_ent = SHIST_GetCurrent(&m_pMWContext->hist);
	if(hist_ent && hist_ent->address) {
        m_csURL = hist_ent->address;
        WFE_CondenseURL(m_csURL, 80, FALSE);
        GetDlgItem(IDC_DOC_URL)->SetWindowText(CHAR_STR(m_csURL));

        // If empty, fill in title with Filename part of URL without extension
        if( m_csTitle.IsEmpty() ){
            char * pTitle = EDT_GetPageTitleFromFilename(hist_ent->address);
            if( pTitle ){
                m_csTitle = pTitle;
                XP_FREE(pTitle);
            }
        }
    }

    
    // Get data from meta tags:
    int count = EDT_MetaDataCount(m_pMWContext);
    for ( int i = 0; i < count; i++ ) {
        EDT_MetaData* pData = EDT_GetMetaData(m_pMWContext, i);
        if ( !pData->bHttpEquiv ) {
            if ( 0 == _stricmp(pData->pName, "Author") ) {
                m_csAuthor = pData->pContent;
            } else if ( 0 == _stricmp(pData->pName, "Classification") ) {
            	m_csClassification =  pData->pContent;
            } else if ( 0 == _stricmp(pData->pName, "Description") ) {
            	m_csDescription = pData->pContent;
            } else if ( 0 == _stricmp(pData->pName, "KeyWords") ) {
            	m_csKeyWords =  pData->pContent;
            }/* else if ( 0 == _stricmp(pData->pName, "Created") ) {
            	m_csCreateDate = pData->pContent;
            } else if ( 0 == _stricmp(pData->pName, "Last-Modified") ) {
                m_csUpdateDate = pData->pContent;
            }*/
        }
    }
    if( m_csAuthor.IsEmpty() ){
        // Get name from preferences if none supplied
        char * pAuthor = NULL;
        PREF_CopyCharPref("editor.author", &pAuthor);    
        m_csAuthor = pAuthor;
        XP_FREEIF(pAuthor);
    }
    UpdateData(FALSE);    
	return(TRUE);
}

// Create a MetaData structure and save it 
// This commits change - NO BUFFERING
// Be sure to strip off spaces and quotes before calling this
void CDocInfoPage::SetMetaData(char * pName, char * pValue)
{
    EDT_MetaData *pData = EDT_NewMetaData();
    if ( pData ) {
        pData->bHttpEquiv = FALSE;
        if ( pName && XP_STRLEN(pName) > 0 ) {
            pData->pName = XP_STRDUP(pName);
            if ( pValue && XP_STRLEN(pValue) > 0 ) {
                pData->pContent = XP_STRDUP(pValue);
                EDT_SetMetaData(m_pMWContext, pData);
            } else {
                // (Don't really need to do this)
                pData->pContent = NULL; 
                // Remove the item            
                EDT_DeleteMetaData(m_pMWContext, pData);
            }
            OkToClose();
        }
        EDT_FreeMetaData(pData);
    }
}

void CDocInfoPage::OnHelp() 
{
    NetHelp(HELP_DOC_PROPS_GENERAL);
}

void CDocInfoPage::OnOK() 
{
    CPropertyPage::OnOK();

    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this)){
        return;
    }    
    m_bActivated = TRUE;

    // EDT_BeginBatchChanges(m_pMWContext);

    // Replace Title and FontDefURL
    XP_FREEIF(m_pPageData->pTitle);

    CleanupString(m_csTitle);

    if ( !m_csTitle.IsEmpty() ) {
        m_pPageData->pTitle = XP_STRDUP(m_csTitle);
    }

    EDT_SetPageData(m_pMWContext, m_pPageData);

    // Trim quotes and set data meta tags
    CleanupString(m_csAuthor);
    if ( !m_csAuthor.IsEmpty() ) {
        // Don't wipe out author field if empty?
        SetMetaData("Author", CHAR_STR(m_csAuthor));
    }

    CleanupString(m_csClassification);
    SetMetaData("Classification", CHAR_STR(m_csClassification));

    CleanupString(m_csDescription);
    SetMetaData("Description", CHAR_STR(m_csDescription));

    CleanupString(m_csKeyWords);
    SetMetaData("KeyWords", CHAR_STR(m_csKeyWords));

    // TODO: MAKE THIS STRING IF NEW DOC???
    // SetMetaData(FALSE, "Created", CHAR_STR());
    // SetMetaData(FALSE, "Last-Modified", CHAR_STR());

    OkToClose();
    //EDT_EndBatchChanges(m_pMWContext);
}

void CDocInfoPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDocInfoPage)
	DDX_Text(pDX, IDC_DOC_AUTHOR, m_csAuthor);
	DDX_Text(pDX, IDC_DOC_CLASIFICATION, m_csClassification);
	DDX_Text(pDX, IDC_DOC_DESCRIPTION, m_csDescription);
	DDX_Text(pDX, IDC_DOC_TITLE, m_csTitle);
	DDX_Text(pDX, IDC_DOC_URL, m_csURL);
	DDX_Text(pDX, IDC_KEYWORDS, m_csKeyWords);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDocInfoPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CDocInfoPage)
	ON_EN_CHANGE(IDC_DOC_AUTHOR, EnableApply)
	ON_EN_CHANGE(IDC_DOC_CLASIFICATION, EnableApply)
	ON_EN_CHANGE(IDC_DOC_DESCRIPTION, EnableApply)
	ON_EN_CHANGE(IDC_DOC_TITLE, EnableApply)
	ON_EN_CHANGE(IDC_KEYWORDS, EnableApply)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CDocInfoPage::EnableApply() 
{
    SetModified(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// Local helpers for color params converversions

// Allocate and initialize a new scheme struct and add it to list
EDT_ColorSchemeData * AddNewColorData( XP_List *pSchemeData )
{
	EDT_ColorSchemeData * pColorData = XP_NEW(EDT_ColorSchemeData);

	if(!pColorData) {
        return NULL;
    }
	memset(pColorData, 0, sizeof(EDT_ColorSchemeData));

    XP_ListAddObjectToEnd(pSchemeData, pColorData);
    return pColorData;
}

//////////////////////////////////////////////////////////////////////
/*
// For reference: the built-in default colors
LO_DEFAULT_FG_RED         0
LO_DEFAULT_FG_GREEN       0
LO_DEFAULT_FG_BLUE        0

LO_DEFAULT_BG_RED         192
LO_DEFAULT_BG_GREEN       192
LO_DEFAULT_BG_BLUE        192

LO_UNVISITED_ANCHOR_RED   0
LO_UNVISITED_ANCHOR_GREEN 0
LO_UNVISITED_ANCHOR_BLUE  238

LO_VISITED_ANCHOR_RED     85
LO_VISITED_ANCHOR_GREEN   26
LO_VISITED_ANCHOR_BLUE    139

LO_SELECTED_ANCHOR_RED    255
LO_SELECTED_ANCHOR_GREEN  0
LO_SELECTED_ANCHOR_BLUE   0
// Offsets for getting defaults from lo_master_colors
// eg:  
		red = lo_master_colors[LO_COLOR_FG].red;
		green = lo_master_colors[LO_COLOR_FG].green;
		blue = lo_master_colors[LO_COLOR_FG].blue;

LO_COLOR_BG	0
LO_COLOR_FG	1
LO_COLOR_LINK	2
LO_COLOR_VLINK	3
LO_COLOR_ALINK	4
*/

/////////////////////////////////////////////////////////////////////
CDocColorPage::CDocColorPage(CWnd* pParent,
                             UINT nIDCaption,
                             UINT nIDFocus,
                             MWContext * pMWContext,
                             CEditorResourceSwitcher * pResourceSwitcher,
                             EDT_PageData * pPageData )
    : CNetscapePropertyPage(CDocColorPage::IDD, nIDCaption, nIDFocus),
      m_bActivated(0),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher),
      m_pPageData(pPageData),
      m_bImageChanged(0),
      m_bValidImage(0),
      m_pSchemeData(0),
      m_hPal(0)
{
    ASSERT(pMWContext);

    //{{AFX_DATA_INIT(CDocColorPage)
	m_csBackgroundImage = _T("");
	m_csSelectedScheme = _T("");
    m_bNoSave = 0;
	//}}AFX_DATA_INIT

    //m_csCustomBackgroundImage = _T("");
    m_csBrowserBackgroundImage = _T("");

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}

CDocColorPage::~CDocColorPage()
{
    // Destroy the scheme list
    // TODO: Move to XP code
    if ( m_pSchemeData ) {
        EDT_ColorSchemeData *pColorData;
	    XP_List * list_ptr = m_pSchemeData;
        while ((pColorData = (EDT_ColorSchemeData *)XP_ListNextObject(list_ptr))) {
            if ( pColorData->pSchemeName ) {
                XP_FREE( pColorData->pSchemeName );
            }
            if ( pColorData->pBackgroundImage ) {
                XP_FREE( pColorData->pBackgroundImage );
            }
            XP_FREE(pColorData);
        }
        XP_ListDestroy(m_pSchemeData);
    }
}

void CDocColorPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDocColorPage)
    DDX_Text(pDX, IDC_BKGRND_IMAGE, m_csBackgroundImage);
    DDX_CBString(pDX, IDC_SCHEME_LIST, m_csSelectedScheme);
    DDX_Check(pDX, IDC_NO_SAVE_IMAGE, m_bNoSave);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDocColorPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CDocColorPage)
	ON_BN_CLICKED(IDC_BKGRND_USE_IMAGE, OnUseBkgrndImage)
	ON_BN_CLICKED(IDC_CHOOSE_BACKGROUND, OnChooseBkgrndImage)
	ON_CBN_SELCHANGE(IDC_SCHEME_LIST, OnSelchangeSchemeList)
	ON_EN_CHANGE(IDC_BKGRND_IMAGE, OnChangeBkgrndImage)
	ON_EN_KILLFOCUS(IDC_BKGRND_IMAGE, OnKillfocusBkgrndImage)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_CHOOSE_TEXT_COLOR, OnChooseTextColor)
	ON_BN_CLICKED(IDC_CHOOSE_LINK_COLOR, OnChooseLinkColor)
	ON_BN_CLICKED(IDC_CHOOSE_ACTIVELINK_COLOR, OnChooseActivelinkColor)
	ON_BN_CLICKED(IDC_CHOOSE_FOLLOWEDLINK_COLOR, OnChooseFollowedlinkColor)
	ON_BN_CLICKED(IDC_CHOOSE_BKGRND_COLOR, OnChooseBkgrndColor)
	ON_BN_CLICKED(IDC_SET_DOC_COLORS, OnColorsRadioButtons)
	ON_BN_CLICKED(IDC_USE_BROWSER_COLORS, OnColorsRadioButtons)
	ON_BN_CLICKED(IDC_USE_AS_DEFAULT, OnUseAsDefault)
	ON_BN_CLICKED(IDC_NO_SAVE_IMAGE, OnNoSave)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#include "timer.h"  //For nettimer.OnIdle()

BOOL CDocColorPage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated){
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}

    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);

    // Switch back to EXE's resources
    if( m_pResourceSwitcher ){
        m_pResourceSwitcher->Reset();
    }

    m_hPal = WFE_GetUIPalette(GetParentFrame());

    if( !m_TextColorButton.Subclass(this, IDC_CHOOSE_TEXT_COLOR, &m_crText) ||
        !m_LinkColorButton.Subclass(this, IDC_CHOOSE_LINK_COLOR, &m_crLink) ||
        !m_ActiveLinkColorButton.Subclass(this, IDC_CHOOSE_ACTIVELINK_COLOR, &m_crActiveLink) ||
        !m_FollowedLinkColorButton.Subclass(this, IDC_CHOOSE_FOLLOWEDLINK_COLOR, &m_crFollowedLink) || 
        !m_BackgroundColorButton.Subclass(this, IDC_CHOOSE_BKGRND_COLOR, &m_crBackground) ){
        return FALSE;
    }

    // TODO - do stuff here
	m_bActivated = TRUE;
    
    CComboBox * pSchemeListBox = (CComboBox*)GetDlgItem(IDC_SCHEME_LIST);

    CFont fontPreview;
	//  Create a font for the preview window
    if( GetSystemMetrics(SM_DBCSENABLED) ){
		HFONT	hFont = NULL;

#ifdef _WIN32
		hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
#endif

        if (!hFont){
			hFont = (HFONT)GetStockObject(SYSTEM_FONT);
        }
		fontPreview.Attach(hFont);
        GetDlgItem(IDC_DOC_COLOR_PREVIEW)->SetFont(&fontPreview);
    } else {
  	    //  Get a 1-pixel font
	    LOGFONT logFont;
	    memset(&logFont, 0, sizeof(logFont));
	    logFont.lfHeight = -18; // about 14 points?
	    logFont.lfWeight = FW_BOLD;
	    logFont.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
	    lstrcpy(logFont.lfFaceName,"MS Serif");

        if( fontPreview.CreateFontIndirect(&logFont) ){
            GetDlgItem(IDC_DOC_COLOR_PREVIEW)->SetFont(&fontPreview);
        } else {
    		TRACE0("Could Not create preview for Color Dialog\n");
        }
    }

    // Get current stuff from preferences
	char * prefStr = NULL;
	PREF_CopyCharPref("editor.color_scheme",&prefStr);
	if (prefStr) {
		m_csSelectedScheme = prefStr;
		XP_FREE(prefStr);
	} else m_csSelectedScheme = "";

    // Construct a list of Schemes
    // TODO: Make this a global list and move to XP code
    m_pSchemeData = XP_ListNew();

	EDT_ColorSchemeData * pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        // First scheme is always the Netscape Default Colors
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_COLOR_SCHEME_NS_DEFAULT));
		
		COLORREF clr;		
		PREF_GetColorPrefDWord("editor.text_color",&clr);
        WFE_SetLO_Color( clr, &pColorData->ColorText );

		PREF_GetColorPrefDWord("editor.link_color",&clr);
        WFE_SetLO_Color( clr, &pColorData->ColorLink );

		PREF_GetColorPrefDWord("editor.active_link_color",&clr);
        WFE_SetLO_Color( clr, &pColorData->ColorActiveLink );

		PREF_GetColorPrefDWord("editor.followed_link_color",&clr);
        WFE_SetLO_Color( clr, &pColorData->ColorFollowedLink );
        
		PREF_GetColorPrefDWord("editor.background_color",&clr);
		WFE_SetLO_Color( clr, &pColorData->ColorBackground );
        // Add name to listbox
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_BLACK_ON_WHITE));
        WFE_SetLO_Color( RGB(0,0,0), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(255,255,255),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(255,0,0),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,0,128),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(0,0,255),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

    pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_BLACK_ON_OFFWHITE));
        WFE_SetLO_Color( RGB(0,0,0), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(255,240,240),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(255,0,0),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,0,128),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(0,0,255),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_BLACK_ON_LTYELLOW));
        WFE_SetLO_Color( RGB(0,0,0), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(255,255,192),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(0,0,255),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,0,128),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(255,0,255),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_BLACK_ON_YELLOW));
        WFE_SetLO_Color( RGB(64,0,64), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(255,255,128),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(0,0,255),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(0,128, 0),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(255,0,128),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_BLACK_ON_LTBLUE));
        WFE_SetLO_Color( RGB(0,0,0), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(192,192,255),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(0,0,255),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,0,128),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(255,0,128),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_BLACK_ON_MEDBLUE));
        WFE_SetLO_Color( RGB(0,0,0), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(128,128,192),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(255,255,255),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,0,128),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(255,255,0),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_BLUE_ON_ORANGE));
        WFE_SetLO_Color( RGB(0,0,128), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(255,192,64),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(0,0,255),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(0,128,0),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(0,255,255),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_WHITE_ON_BLACK));
        WFE_SetLO_Color( RGB(255,255,255), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(0,0,0),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(255,255,0),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(192,192,192),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(192,255,192),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_WHITE_ON_GREEN));
        WFE_SetLO_Color( RGB(255,255,255), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(0,64,0),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(255,255,0),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,255,128),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(0,255,64),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_WHITE_ON_BLUE));
        WFE_SetLO_Color( RGB(255,255,255), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(0,0,128),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(255,255,0),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,128,255),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(255,0,255),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	pColorData = AddNewColorData( m_pSchemeData );
    if ( pColorData ) {
        pColorData->pSchemeName = XP_STRDUP(szLoadString(IDS_WHITE_ON_VIOLET));
        WFE_SetLO_Color( RGB(255,255,255), 
                         &pColorData->ColorText );
        WFE_SetLO_Color( RGB(128,0,128),
                         &pColorData->ColorBackground );
        WFE_SetLO_Color( RGB(0,255,255),
                         &pColorData->ColorLink );
        WFE_SetLO_Color( RGB(128,255,255),
                         &pColorData->ColorFollowedLink );
        WFE_SetLO_Color( RGB(0,255,0),
                         &pColorData->ColorActiveLink );
        pSchemeListBox->AddString(pColorData->pSchemeName);
    }

	COLORREF tmpColor,defColor;
	XP_Bool bCust;
    // Get Browser preference colors
	PREF_GetColorPrefDWord("browser.foreground_color",&tmpColor);
	PREF_GetDefaultColorPrefDWord("browser.foreground_color",&defColor);
	PREF_GetBoolPref("browser.custom_text_color",&bCust);
    m_crBrowserText = bCust ? tmpColor : defColor;

	PREF_GetColorPrefDWord("browser.anchor_color",&tmpColor);
	PREF_GetDefaultColorPrefDWord("browser.anchor_color",&defColor);
	PREF_GetBoolPref("browser.custom_link_color",&bCust);
    m_crBrowserLink = bCust ? tmpColor : defColor;

    
	PREF_GetColorPrefDWord("browser.visited_color",&tmpColor);
	PREF_GetDefaultColorPrefDWord("browser.visited_color",&defColor);
	PREF_GetBoolPref("browser.custom_visited_color",&bCust);
	m_crBrowserFollowedLink = bCust ? tmpColor : defColor;

    m_crBrowserBackground = prefInfo.m_rgbBackgroundColor;

    // m_pPageData may be NULL if we are not an Editor
    // (preferences dialog called from a browser)
    //
    // We are in Document Properties Mode
    // Initialize from current document
    // Note: If the color isn't set,
    //       this will get the color from display master colors
    m_crCustomBackground = WFE_LO2COLORREF( m_pPageData->pColorBackground,LO_COLOR_BG );
    m_crCustomText = WFE_LO2COLORREF( m_pPageData->pColorText, LO_COLOR_FG );
    m_crCustomLink = WFE_LO2COLORREF( m_pPageData->pColorLink, LO_COLOR_LINK );
    m_crCustomActiveLink = WFE_LO2COLORREF( m_pPageData->pColorActiveLink, LO_COLOR_ALINK );
    m_crCustomFollowedLink = WFE_LO2COLORREF( m_pPageData->pColorFollowedLink, LO_COLOR_VLINK );

    if (( m_pPageData->pBackgroundImage )&&( XP_STRLEN(m_pPageData->pBackgroundImage) )) {
        m_csBackgroundImage = m_pPageData->pBackgroundImage;
        // Set checkbox - we have an image
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
    } else {
        // Get the default image from preferences in case they want to use it
		char * szBack = NULL;
		PREF_CopyCharPref("editor.background_image",&szBack);
        m_csBackgroundImage = szBack;
		XP_FREEIF(szBack);
        // But DON'T set the checkbox -- image will not be used
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
    }

    // If ANY color is set, then we will assume "Custom" mode
    m_bCustomColors = 
        ( m_pPageData->pColorBackground ||
          m_pPageData->pColorText ||
          m_pPageData->pColorLink ||
          m_pPageData->pColorActiveLink ||
          m_pPageData->pColorFollowedLink );
    
    // Save current state so we don't repaint unnecessarily
    m_bWasCustomColors = m_bCustomColors;
    
    // Flag to leave image at original location and not save with page
    m_bNoSave = m_pPageData->bBackgroundNoSave;
    
    if( m_bCustomColors ){
        // We will start in Custom Colors mode
	    // Redraw only if current state is Browser colors
        // Don't unselect scheme
		UseCustomColors(!m_bWasCustomColors, FALSE);
    } else {
        // We will start with Browser colors
        // Don't do update stuff
        UseBrowserColors(FALSE);
    }

    // Initialize radio buttons and color swatches
    ((CButton*)GetDlgItem(IDC_SET_DOC_COLORS))->SetCheck( m_bCustomColors ? 1 : 0 );
    ((CButton*)GetDlgItem(IDC_USE_BROWSER_COLORS))->SetCheck( m_bCustomColors ? 0 : 1 );

    // Disables Apply button (probably don't need to do this)
    SetModified(FALSE);
    UpdateData(FALSE);
	return(TRUE);
}

// Use bUpdateControls = FALSE if initializing,
// Use TRUE when switching from CustomColor state to Browser colors
void CDocColorPage::UseBrowserColors(BOOL bRedraw)
{
    m_bCustomColors = m_bWasCustomColors = FALSE;
    ((CButton*)GetDlgItem(IDC_SET_DOC_COLORS))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_USE_BROWSER_COLORS))->SetCheck(1);

    // Initialize from current preferences
    m_crBackground = m_crBrowserBackground;
    m_crText = m_crBrowserText;
    m_crLink = m_crBrowserLink;
    // Browser doesn't store this color
    m_crActiveLink = m_crBrowserLink;
    m_crFollowedLink = m_crBrowserFollowedLink;

    if( bRedraw ){
        Invalidate(FALSE);
    }
    SetModified(TRUE);
}

// Redraw dialog to show color swatches -- use bRedraw = FALSE if no colors changing
void CDocColorPage::UseCustomColors(BOOL bRedraw, BOOL bUnselectScheme)
{
    m_bCustomColors = m_bWasCustomColors = TRUE;
    ((CButton*)GetDlgItem(IDC_SET_DOC_COLORS))->SetCheck(1);
    ((CButton*)GetDlgItem(IDC_USE_BROWSER_COLORS))->SetCheck(0);

    if( bUnselectScheme ){
        // Get selected scheme name
        // (Also gets background image image name, which we ignore here)
        UpdateData(TRUE);
        // TODO: Modify this in the future to
        //   reset only if not one of built-in
        //   schemes?
        // Get currently-selected scheme
        //  to use as default name for saving
        m_csSaveScheme = m_csSelectedScheme;
        
        // Unselect scheme list when editing built-in schemes,
        //  to remove impression that user can modify them
        ((CComboBox*)GetDlgItem(IDC_SCHEME_LIST))->SetCurSel(-1);
    }
    m_crBackground = m_crCustomBackground;
    m_crText = m_crCustomText;
    m_crLink = m_crCustomLink;
    m_crActiveLink = m_crCustomActiveLink;
    m_crFollowedLink = m_crCustomFollowedLink;

    if( bRedraw ){
        // Redraw color swatches and text samples
        Invalidate(FALSE);
    }
    // Set Apply button states if we are in Properties mode
    SetModified(TRUE);
}

BOOL CDocColorPage::OnKillActive()
{
    if(m_bActivated){
        if( !UpdateData(TRUE) ){
            return FALSE;
        }
        if ( !m_csBackgroundImage.IsEmpty() ) {
            if ( m_bImageChanged && !m_bValidImage ) {
                if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() &&
                    !wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/ ) ){
                    ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
                    return FALSE;
                }
                // Send changed image back to editbox in case this is Apply usage
                UpdateData(FALSE);
            }
        }
        if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() &&
            !m_csBackgroundImage.IsEmpty() ){
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
    }
    return TRUE;
}

void CDocColorPage::OnHelp() 
{
    NetHelp(HELP_DOC_PROPS_APPEARANCE);
}

void CDocColorPage::OnOK() 
{
    //Note: Get and Validate data done in OnKillActive()
    CPropertyPage::OnOK();

    // never visited this page,
    // or no change when used in Doc. Properties mode -- don't bother
    if( !m_bActivated ||
        !IS_APPLY_ENABLED(this) ){
        return;
    }    

    // Save settings for current document
    // Erase current background image in doc
    if ( m_pPageData->pBackgroundImage ) {
        XP_FREE( m_pPageData->pBackgroundImage );
        m_pPageData->pBackgroundImage = NULL;
    }
    if ( m_bCustomColors ) {
        // Set all custom colors
        WFE_SetLO_ColorPtr( m_crCustomText, &m_pPageData->pColorText );
        WFE_SetLO_ColorPtr( m_crCustomLink, &m_pPageData->pColorLink );
        WFE_SetLO_ColorPtr( m_crCustomActiveLink, &m_pPageData->pColorActiveLink );
        WFE_SetLO_ColorPtr( m_crCustomFollowedLink, &m_pPageData->pColorFollowedLink );
        // Set color even if we use an image
        // (Color tag is written to doc, but image tag overrides)
        WFE_SetLO_ColorPtr( m_crCustomBackground, &m_pPageData->pColorBackground );

    } else {
        // Use browser colors means storing no tags in document
        // Clear all colors and background image to set the document's tags
        // Note: We ignore the Browser's background image, 
        //   based on the judgement that if they don't want colors,
        //   then the browser's image would be irrelavent (and distracting) while editing
        WFE_FreeLO_Color( &m_pPageData->pColorText );
        WFE_FreeLO_Color( &m_pPageData->pColorLink );
        WFE_FreeLO_Color( &m_pPageData->pColorActiveLink );
        WFE_FreeLO_Color( &m_pPageData->pColorFollowedLink );
        WFE_FreeLO_Color( &m_pPageData->pColorBackground );
    }

    if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() &&
        !m_csBackgroundImage.IsEmpty() ){
        m_pPageData->pBackgroundImage = XP_STRDUP(m_csBackgroundImage);

        // Save state of flag from checkbox
        m_pPageData->bBackgroundNoSave = m_bNoSave;
    }
    
    // Disable Apply an change "OK" to "Close"
    OkToClose();
    // Clear data-modified flag
    SetModified(FALSE);

    // This sets tag data for doc and calls front end
    //  to setbackground colors and image
    EDT_SetPageData(m_pMWContext, m_pPageData);

    // This forces redraw of entire window
    //  (to correct paint bug: missing redraw areas at top of window above cursor)
    HWND hView = PANECX(m_pMWContext)->GetPane();
    if( hView ){
        ::InvalidateRect(hView, NULL, TRUE);
    }

    // This forces redraw of entire window
    //  (there is some missing redraw areas at top of window)
    ::InvalidateRect(PANECX(m_pMWContext)->GetPane(), NULL, TRUE);

    if( ((CButton*)GetDlgItem(IDC_USE_AS_DEFAULT))->GetCheck() ){
        // Save current settings to Preferences
        if( ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck() &&
            !m_csBackgroundImage.IsEmpty() ){
		    PREF_SetBoolPref("editor.use_background_image",TRUE);
		    PREF_SetCharPref("editor.background_image",LPCSTR(m_csBackgroundImage));
        } else {
		    PREF_SetBoolPref("editor.use_background_image",FALSE);
        }

	    if ( m_bCustomColors ) {
		    PREF_SetBoolPref("editor.use_custom_colors",TRUE);
		    PREF_SetCharPref("editor.color_scheme",(char*)LPCSTR(m_csSelectedScheme));

		    PREF_SetColorPrefDWord("editor.text_color",m_crCustomText);
		    PREF_SetColorPrefDWord("editor.link_color",m_crCustomLink);
		    PREF_SetColorPrefDWord("editor.active_link_color",m_crCustomActiveLink);
		    PREF_SetColorPrefDWord("editor.followed_link_color",m_crCustomFollowedLink);
		    PREF_SetColorPrefDWord("editor.background_color",m_crCustomBackground);
        } else {
            // Note: don't change previous color preferences
		    PREF_SetBoolPref("editor.use_custom_colors",FALSE);
        }
    }
}

void CDocColorPage::OnUseAsDefault()
{
    SetModified(TRUE);
}

void CDocColorPage::OnColorsRadioButtons() 
{
    // Get checkbox state and change colors appropriately
    //  but only if different than previous mode
    m_bCustomColors = IsDlgButtonChecked(IDC_SET_DOC_COLORS);
    if(m_bWasCustomColors != m_bCustomColors){
        if(m_bCustomColors){
            UseCustomColors();
        } else {
            UseBrowserColors();
        }
        // Send the appropriate string to the background image editbox
        UpdateData(FALSE);
    }
}

void CDocColorPage::OnRemoveScheme() 
{
    SetModified(TRUE);
}

void CDocColorPage::OnSaveScheme() 
{
    SetModified(TRUE);
}

void CDocColorPage::OnSelchangeSchemeList() 
{
    // Get data - we want selected color scheme
    UpdateData(TRUE);
    if ( m_csSelectedScheme.IsEmpty() ) {
        return;
    }

    // Find the scheme in the list and extract color info
    EDT_ColorSchemeData *pColorData;
	XP_List * list_ptr = m_pSchemeData;
    while ((pColorData = (EDT_ColorSchemeData *)XP_ListNextObject(list_ptr))) {
        if ( 0 == XP_STRCMP(pColorData->pSchemeName, m_csSelectedScheme) ) {
            m_crCustomText = 
                WFE_LO2COLORREF( &pColorData->ColorText, LO_COLOR_FG );
            m_crCustomLink = 
                WFE_LO2COLORREF( &pColorData->ColorLink, LO_COLOR_LINK );
            m_crCustomActiveLink = 
                WFE_LO2COLORREF( &pColorData->ColorActiveLink, LO_COLOR_ALINK );
            m_crCustomFollowedLink = 
                WFE_LO2COLORREF( &pColorData->ColorFollowedLink, LO_COLOR_VLINK );
            m_crCustomBackground = 
                WFE_LO2COLORREF( &pColorData->ColorBackground, LO_COLOR_BG );
            
            if ( pColorData->pBackgroundImage ) {
                // Scheme may include a background image...
                ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
                m_csBackgroundImage = pColorData->pBackgroundImage;
            }
            // ...but if scheme doesn't have image, then leave existing
            //   image and checkbox state
            break;
        }
    }
    // Do repaint, but don't unselect the scheme listbox
    UseCustomColors(TRUE, FALSE);
}

BOOL CDocColorPage::ChooseColor(COLORREF * pColor, CColorButton * pButton, COLORREF crDefault )
{
   	RECT rect;
    pButton->GetWindowRect(&rect);
    CColorPicker ColorPicker(this, m_pMWContext, m_bCustomColors ? *pColor : DEFAULT_COLORREF, crDefault, 0, &rect);
    COLORREF crNew = ColorPicker.GetColor();
    if( crNew != CANCEL_COLORREF ){
        if( crNew == DEFAULT_COLORREF){
            *pColor = crDefault;
        } else {
            *pColor = crNew;
        }
        // Send new color to control
        UpdateData(FALSE);        
        // We must be in custom color mode now
        UseCustomColors();
        pButton->Update();
        return TRUE;
    }
    return FALSE;
}

void CDocColorPage::OnChooseTextColor() 
{
    ChooseColor(&m_crCustomText, &m_TextColorButton, m_crBrowserText);
}

void CDocColorPage::OnChooseLinkColor() 
{
    ChooseColor(&m_crCustomLink, &m_LinkColorButton, m_crBrowserLink );
}

void CDocColorPage::OnChooseActivelinkColor() 
{
    ChooseColor(&m_crCustomActiveLink, &m_ActiveLinkColorButton, m_crBrowserLink);
}

void CDocColorPage::OnChooseFollowedlinkColor() 
{
    ChooseColor(&m_crCustomFollowedLink, &m_FollowedLinkColorButton, m_crBrowserFollowedLink);
}

void CDocColorPage::OnChooseBkgrndColor() 
{
    if ( ChooseColor(&m_crCustomBackground, &m_BackgroundColorButton, m_crBrowserBackground) ) {
        // Change to custom colors if  we were Browser mode       
        UseCustomColors(!m_bWasCustomColors);
    }
}

void CDocColorPage::OnChangeBkgrndImage() 
{
    // Set flags to trigger validation on Killfocus
    m_bValidImage = FALSE;
    m_bImageChanged = TRUE;
    
    UpdateData(TRUE);
    CString csTemp = m_csBackgroundImage;
    csTemp.TrimLeft();
    csTemp.TrimRight();

    // Set checkbox if there are any characters, clear if empty
    ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(!csTemp.IsEmpty());
    SetModified(TRUE);
    // NEW: Allow selecting an image if we are in Browser color mode
    // UseCustomColors(!m_bWasCustomColors);
}

void CDocColorPage::OnKillfocusBkgrndImage() 
{
#if 0
// Use this if we want to validate as soon as they leave the editbox
    // Get the image name, make it a relative file-type URL,
    //  then send it back to editbox
    if( m_bImageChanged &&
	    UpdateData(TRUE) ){
        if( !wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/  ) ){
            ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
        }

        m_bValidImage = TRUE;
        UpdateData(FALSE);
    }
#endif
}

// Called from the View after saving file to disk -- has new background
//   image URL relative to current document
void CDocColorPage::SetImageFileSaved(char * pImageURL)
{
    m_csBackgroundImage = pImageURL;
    UpdateData(FALSE);
}

void CDocColorPage::OnUseBkgrndImage() 
{
    int iUseImageBackground = ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->GetCheck();;
    SetModified(TRUE);
    if( iUseImageBackground ){
        // User decided to use the image, so do trigger validation
        if( !wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/  ) ){
            ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
        }
        m_bValidImage = TRUE;
    }
    // TODO: Try to display image in our preview box
    // Need to load file and convert to bitmap
    // Redraw only if current state is Browser colors
    // NEW: Allow selecting an image if we are in Browser color mode
//    UseCustomColors(!m_bWasCustomColors);
}

void CDocColorPage::OnChooseBkgrndImage() 
{
    char * szFilename = wfe_GetExistingImageFileName(this->m_hWnd, 
                                         szLoadString(IDS_SELECT_IMAGE), TRUE);
    if ( szFilename == NULL ){
        return;
    }
    m_csBackgroundImage = szFilename;
    if( !wfe_ValidateImage( m_pMWContext, m_csBackgroundImage, FALSE /*TRUE*/  ) ){
        ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(0);
    }

    // Send new name to editbox
    UpdateData(FALSE);

    XP_FREE( szFilename );
    m_bValidImage = TRUE;
    m_bImageChanged = FALSE;

    // Note: allow background image without forcing custom colors
    ((CButton*)GetDlgItem(IDC_BKGRND_USE_IMAGE))->SetCheck(1);
    SetModified(TRUE);
}

void CDocColorPage::OnPaint() 
{
    CWnd *pPreview = GetDlgItem(IDC_DOC_COLOR_PREVIEW);

    // TODO: Quit here if we don't use text preview in "Use Browser Colors" mode

    // Wierd Windows! We must do this else
    //   no painting at all occurs!
	CPaintDC dc(this); // device context for painting

    // Get the DC to the Preview window
    CDC * pDC = pPreview->GetDC();
    if( !pDC ) return;
    CRect cRect;

    HPALETTE hOldPal = NULL;
    if( m_hPal ){
		hOldPal = ::SelectPalette( pDC->m_hDC, m_hPal, FALSE );
    }

    // find out how much area we can draw into
    pPreview->GetClientRect(&cRect);

    // color for the inside
    CBrush brush(m_crBackground|0x02000000); // Like PALETTERGB
    CBrush * pOldBrush = (CBrush *) pDC->SelectObject(&brush);

    pDC->LPtoDP(&cRect);

    cRect.InflateRect(-1,-1);

    // draw the background
    pDC->FillRect(cRect, &brush);

    // set the background color
    pDC->SetBkColor(m_crBackground);
    pDC->SetTextColor(m_crText);
    pDC->SetBkMode(TRANSPARENT);

    // Draw 4 sample text strings
    CString csText = szLoadString(IDS_TEXT);

    int iNextLine = (cRect.Height() - 2) / 4;
    int iTextTop = 2;
    int iTextLeft = 6;

    char* csSample = szLoadString(IDS_TEXT);
	CIntlWin::TextOut(0, pDC->GetSafeHdc(), iTextLeft, iTextTop, csSample, strlen(csSample));

    CSize sizeText = CIntlWin::GetTextExtent(0, pDC->GetSafeHdc(), csSample, strlen(csSample) );
    csSample = szLoadString(IDS_LINK_TEXT);
    pDC->SetTextColor(m_crLink);

    // This puts "Link text" on 2nd line:
    iTextTop += iNextLine;
	CIntlWin::TextOut(0, pDC->GetSafeHdc(), iTextLeft, iTextTop, csSample, strlen(csSample));

    // Underline link samples
    sizeText = CIntlWin::GetTextExtent(0, pDC->GetSafeHdc(), csSample, strlen(csSample) );
    CPen penLink(PS_SOLID, 2, m_crLink);
    CPen *pOldPen = pDC->SelectObject(&penLink);

    pDC->MoveTo(iTextLeft, iTextTop + sizeText.cy + 1);
    pDC->LineTo(iTextLeft + sizeText.cx, iTextTop + sizeText.cy + 1);
    
    // Move to a new line of text
    iTextTop += iNextLine;
    csSample = szLoadString(IDS_ACTIVE_LINK_TEXT);
    sizeText = CIntlWin::GetTextExtent(0, pDC->GetSafeHdc(), csSample, strlen(csSample) );
    pDC->SetTextColor(m_crActiveLink);
	CIntlWin::TextOut(0, pDC->GetSafeHdc(), iTextLeft, iTextTop, csSample, strlen(csSample));
    CPen penActive(PS_SOLID, 2, m_crActiveLink);
    pDC->SelectObject(&penActive);
    pDC->MoveTo(iTextLeft, iTextTop + sizeText.cy + 1);
    pDC->LineTo(iTextLeft + sizeText.cx, iTextTop + sizeText.cy + 1);
    
    // Next line
    iTextTop += iNextLine;
    csSample = szLoadString(IDS_FOLLOWED_LINK);
    sizeText = CIntlWin::GetTextExtent(0, pDC->GetSafeHdc(), csSample, strlen(csSample) );
    pDC->SetTextColor(m_crFollowedLink);
	CIntlWin::TextOut(0, pDC->GetSafeHdc(), iTextLeft, iTextTop, csSample, strlen(csSample));
    CPen penFollowed(PS_SOLID, 2, m_crFollowedLink);
    pDC->SelectObject(&penFollowed);
    pDC->MoveTo(iTextLeft, iTextTop + sizeText.cy + 1);
    pDC->LineTo(iTextLeft + sizeText.cx, iTextTop + sizeText.cy + 1);

    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);

    if(m_hPal){
        ::SelectPalette( pDC->m_hDC, hOldPal, FALSE );
    }
    // give the CDC back to the system
    pPreview->ReleaseDC(pDC);
}


void CDocColorPage::OnNoSave() 
{
    SetModified(TRUE);
}

////////////////////////////////////////////////////
CDocMetaPage::CDocMetaPage(CWnd* pParent, MWContext * pMWContext,
                           CEditorResourceSwitcher * pResourceSwitcher)
    : CNetscapePropertyPage(CDocMetaPage::IDD),
      m_bActivated(FALSE),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher)
{
    //{{AFX_DATA_INIT(CDocMetaPage)
	//}}AFX_DATA_INIT
    ASSERT(pMWContext);

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}

BOOL CDocMetaPage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated){
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);

    // Switch back to EXE's resources
    if( m_pResourceSwitcher ){
        m_pResourceSwitcher->Reset();
    }

    ((CEdit*)GetDlgItem(IDC_VAR_NAME))->LimitText(MAX_META_NAME_LEN);
    ((CEdit*)GetDlgItem(IDC_VAR_VALUE))->LimitText(MAX_META_VALUE_LEN);

    // Get all the items for the lists editbox
    GetMetaData();
    
    // Setup list for a new meta item
    ((CListBox*)GetDlgItem(IDC_META_LIST))->SetCurSel(m_iMetaCount);

	m_bActivated = TRUE;
	return(TRUE);
}

void CDocMetaPage::OnSelchangeEquivList() 
{
    CListBox *pEquivList = (CListBox*)GetDlgItem(IDC_EQUIV_LIST);
    CListBox *pMetaList = (CListBox*)GetDlgItem(IDC_META_LIST);
    
    int iSel = pEquivList->GetCurSel();
    BOOL bCanSetOrDelete = FALSE;

    // Don't set Name and Value if selecting the last item
    //   unless we are switch from other list
    if ( iSel >= 0 &&
         (-1 != pMetaList->GetCurSel() ||
          iSel != m_iEquivCount) ) {
        CString csEntry;
        CString csName;
        CString csValue;
        pEquivList->GetText(iSel, csEntry);
        if (GetNameAndValue(csEntry, csName, csValue)) {
            ((CEdit*)GetDlgItem(IDC_VAR_NAME))->SetWindowText( LPCSTR(csName) );
            ((CEdit*)GetDlgItem(IDC_VAR_VALUE))->SetWindowText( LPCSTR(csValue) );
            bCanSetOrDelete = TRUE;
        } else {
            ClearNameAndValue();
        }
    }
    SetModified(TRUE);
    // Only one list can be selected at a time
    // We use selected list as a "radio button" to inform
    //   user which list the Name/Value pair applies to
    pMetaList->SetCurSel(-1);
    EnableButtons(bCanSetOrDelete);
}

void CDocMetaPage::OnSelchangeMetaList() 
{
    CListBox *pMetaList = (CListBox*)GetDlgItem(IDC_META_LIST);
    CListBox *pEquivList = (CListBox*)GetDlgItem(IDC_EQUIV_LIST);
    
    int iSel = pMetaList->GetCurSel();
    BOOL bCanSetOrDelete = FALSE;

    // Don't set Name and Value if selecting the last item
    //   unless we are switch from other list
    if ( iSel >= 0 && 
         (-1 != pEquivList->GetCurSel() ||
          iSel != m_iMetaCount)) {
        CString csEntry;
        CString csName;
        CString csValue;
        pMetaList->GetText(iSel, csEntry);
        if (GetNameAndValue(csEntry, csName, csValue)) {
            ((CEdit*)GetDlgItem(IDC_VAR_NAME))->SetWindowText( LPCSTR(csName) );
            ((CEdit*)GetDlgItem(IDC_VAR_VALUE))->SetWindowText( LPCSTR(csValue) );
            bCanSetOrDelete = TRUE;
        } else {
            ClearNameAndValue();
        }
    }
    SetModified(TRUE);

    // Only one list can be selected at a time
    pEquivList->SetCurSel(-1);
    EnableButtons(bCanSetOrDelete);
}

// Make clicking below list items the same as 
//   selecting the last (empty) item so we 
//   always change lists when clicked on
void CDocMetaPage::OnSetfocusEquivList() 
{
    if (((CListBox*)GetDlgItem(IDC_EQUIV_LIST))->GetCurSel() == -1 ) {
        ((CListBox*)GetDlgItem(IDC_EQUIV_LIST))->SetCurSel(m_iEquivCount);
        OnSelchangeEquivList();
    }
}

void CDocMetaPage::OnSetfocusMetaList() 
{
    if (((CListBox*)GetDlgItem(IDC_META_LIST))->GetCurSel() == -1 ) {
        ((CListBox*)GetDlgItem(IDC_META_LIST))->SetCurSel(m_iMetaCount);
        OnSelchangeMetaList();
    }
}

void CDocMetaPage::OnChangeVarNameOrValue() 
{
    CString csName;
    GetDlgItem(IDC_VAR_NAME)->GetWindowText(csName);
    CString csValue;
    GetDlgItem(IDC_VAR_VALUE)->GetWindowText(csValue);
    CleanupString(csName);
    CleanupString(csValue);
    GetDlgItem(IDC_VAR_SET)->EnableWindow(!csName.IsEmpty() && !csValue.IsEmpty());
    GetDlgItem(IDC_VAR_NEW)->EnableWindow(!csName.IsEmpty() || !csValue.IsEmpty());
    GetDlgItem(IDC_VAR_DELETE)->EnableWindow(!csName.IsEmpty() );
    SetModified(TRUE);
}

void CDocMetaPage::OnVarSet() 
{
    CListBox *pMetaList = (CListBox*)GetDlgItem(IDC_META_LIST);
    CListBox *pEquivList = (CListBox*)GetDlgItem(IDC_EQUIV_LIST);

    CString csName;
    GetDlgItem(IDC_VAR_NAME)->GetWindowText(csName);
    CString csValue;
    GetDlgItem(IDC_VAR_VALUE)->GetWindowText(csValue);
    CleanupString(csName);
    CleanupString(csValue);
   
    
    if ( !csName.IsEmpty() ) {
        int iEqual = csName.Find('=');
        if ( iEqual != -1 ) {
            csName = csName.Left(iEqual);
            CleanupString(csName);
        }
        CString csSearch = csName + "=";
        BOOL bEquiv;
        // We determine which list to use by which was last selected
        if ( pEquivList->GetCurSel() != -1 ) {
            SetMetaData(TRUE, CHAR_STR(csName), CHAR_STR(csValue));
            bEquiv = TRUE;
        } else {
            SetMetaData(FALSE, CHAR_STR(csName), CHAR_STR(csValue));
            bEquiv = FALSE;
        }
        GetMetaData();

        BOOL bCanDelete = FALSE;
        // Set the selection to the item just set,
        //  but set to last item if (for some very weird reason) we fail
        if (bEquiv) {
            if ( -1 == pEquivList->SelectString(-1,LPCSTR(csSearch)) ) {
                pEquivList->SetCurSel(m_iEquivCount);
            } else {
                bCanDelete = TRUE;
            }
            pMetaList->SetCurSel(-1);
        } else {
            if ( -1 == pMetaList->SelectString(-1,LPCSTR(csSearch)) ) {
                pMetaList->SetCurSel(m_iEquivCount);
            } else {
                bCanDelete = TRUE;
            }
            pEquivList->SetCurSel(-1);
        }
        // We can delete if a non-blank item is selected
        ((CButton*)GetDlgItem(IDC_VAR_DELETE))->EnableWindow(bCanDelete);
    }
   ((CEdit*)GetDlgItem(IDC_VAR_NAME))->SetFocus();
}

void CDocMetaPage::OnVarDelete() 
{
    CListBox *pMetaList = (CListBox*)GetDlgItem(IDC_META_LIST);
    CListBox *pEquivList = (CListBox*)GetDlgItem(IDC_EQUIV_LIST);

    int nSel = pEquivList->GetCurSel();
    CString csSelString;
    CString csName;
    CString csValue;
    BOOL bEquiv = FALSE;
    
    // Get the strings from which ever list has the selected item
    if ( nSel != -1 ) {
        pEquivList->GetText(nSel, csSelString);
        bEquiv = TRUE;
    } else {
        nSel = pMetaList->GetCurSel();
        if ( nSel != -1 ) {
            pMetaList->GetText(nSel, csSelString);
        }
    }
    if ( nSel != -1 && GetNameAndValue(csSelString, csName, csValue ) ) {
        // This will delete entry in EDT data with selected Name
        SetMetaData(bEquiv, CHAR_STR(csName), NULL);
        // Rebuild the list
        GetMetaData();
        SetModified(TRUE);
    }

    // Set selection to end of list just deleted
    //  (or to end of meta list if error deleting)
    if ( bEquiv ) {
        pEquivList->SetCurSel(m_iEquivCount);
    } else {
        pMetaList->SetCurSel(m_iMetaCount);
    }
   ((CEdit*)GetDlgItem(IDC_VAR_NAME))->SetFocus();
}

void CDocMetaPage::OnVarNew() 
{
    CListBox *pMetaList = (CListBox*)GetDlgItem(IDC_META_LIST);
    CListBox *pEquivList = (CListBox*)GetDlgItem(IDC_EQUIV_LIST);

    // Clear the Name-Value edit boxes and 
    //   remove selection in lists
    ClearNameAndValue();

    // Reset the selected item to the blank at end
    //  of list already selected
    if ( pEquivList->GetCurSel() != -1 ) {
        pEquivList->SetCurSel(m_iEquivCount);
        pMetaList->SetCurSel(-1);
    } else {
        pMetaList->SetCurSel(m_iMetaCount);
    }

    // Disable all buttons
    EnableButtons(FALSE);
    ((CEdit*)GetDlgItem(IDC_VAR_NAME))->SetFocus();
}

void CDocMetaPage::EnableButtons(BOOL bEnable)
{
    ((CButton*)GetDlgItem(IDC_VAR_SET))->EnableWindow(bEnable);
    ((CButton*)GetDlgItem(IDC_VAR_DELETE))->EnableWindow(bEnable);
    ((CButton*)GetDlgItem(IDC_VAR_NEW))->EnableWindow(bEnable);
} 

void CDocMetaPage::ClearNameAndValue()
{
    ((CEdit*)GetDlgItem(IDC_VAR_NAME))->SetWindowText("");
    ((CEdit*)GetDlgItem(IDC_VAR_VALUE))->SetWindowText("");    
    EnableButtons(FALSE);
}

void CDocMetaPage::GetMetaData() 
{
    CListBox *pMetaList = (CListBox*)GetDlgItem(IDC_META_LIST);
    CListBox *pEquivList = (CListBox*)GetDlgItem(IDC_EQUIV_LIST);

    // Rebuild our list from the Edit data
    pEquivList->ResetContent();
    pMetaList->ResetContent();
    
    //   Name = 100, content = 400
    char pName[MAX_META_NAME_LEN+1];
    char pValue[MAX_META_VALUE_LEN+1];
    CString csEntry;

    m_iEquivCount = 0;
    m_iMetaCount = 0;

    int count = EDT_MetaDataCount(m_pMWContext);
    for ( int i = 0; i < count; i++ ) {
        EDT_MetaData* pData = EDT_GetMetaData(m_pMWContext, i);
        PR_snprintf(pName, MAX_META_NAME_LEN, "%s", pData->pName);
        PR_snprintf(pValue, MAX_META_VALUE_LEN, "%s", pData->pContent);
        csEntry = pName;
        csEntry += "=";
        csEntry += pValue;
        if ( pData->bHttpEquiv ) {
            pEquivList->AddString(csEntry);
            m_iEquivCount++;
        } else if ( 0 != _stricmp(pName, "Author")         &&
                    0 != _stricmp(pName, "Description")    &&
                    0 != _stricmp(pName, "Generator")      &&
                    0 != _stricmp(pName, "Last-Modified")  &&
                    0 != _stricmp(pName, "Created")        &&
                    0 != _stricmp(pName, "Classification") &&
                    0 != _stricmp(pName, "Keywords")       
                    /* && 0 != _stricmp(pName, "SourceURL") */
                    ) {
            // Skip the fields used in General Info page
            // TODO: PUT META STRINGS IN RESOURCES?
            pMetaList->AddString(csEntry);
            m_iMetaCount++;
        }
    }
    // We add a blank string to each list 
    //   to ensure a selectable item when none really exists
    // Note: Listbox should NOT be sorted for this to work right
    pEquivList->AddString("");
    pMetaList->AddString("");
    ClearNameAndValue();
}

// Create a MetaData structure and save it 
// This commits change - NO BUFFERING
// Be sure to strip off spaces and quotes before calling this
void CDocMetaPage::SetMetaData(BOOL bHttpEquiv, char * pName, char * pValue)
{
    EDT_MetaData *pData = EDT_NewMetaData();
    if ( pData ) {
        pData->bHttpEquiv = bHttpEquiv;
        if ( pName && XP_STRLEN(pName) > 0 ) {
            pData->pName = XP_STRDUP(pName);
            if ( pValue && XP_STRLEN(pValue) > 0 ) {
                pData->pContent = XP_STRDUP(pValue);
                EDT_SetMetaData(m_pMWContext, pData);
            } else {
                // (Don't really need to do this)
                pData->pContent = NULL; 
                // Remove the item            
                EDT_DeleteMetaData(m_pMWContext, pData);
            }
            OkToClose();
        }
        EDT_FreeMetaData(pData);
    }
}

void CDocMetaPage::OnHelp() 
{
    NetHelp(HELP_DOC_PROPS_ADVANCED);
}

void CDocMetaPage::OnOK() 
{
    CPropertyPage::OnOK();

    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this)){
        return;
    }    

    //EDT_BeginBatchChanges(m_pMWContext);

    // Set anything in the current Name/Value strings
    OnVarSet();
    
    OkToClose();

    //EDT_EndBatchChanges(m_pMWContext);
}

void CDocMetaPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDocMetaPage)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDocMetaPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CDocMetaPage)
	ON_LBN_SELCHANGE(IDC_EQUIV_LIST, OnSelchangeEquivList)
	ON_LBN_SELCHANGE(IDC_META_LIST, OnSelchangeMetaList)
	ON_BN_CLICKED(IDC_VAR_SET, OnVarSet)
	ON_BN_CLICKED(IDC_VAR_DELETE, OnVarDelete)
	ON_BN_CLICKED(IDC_VAR_NEW, OnVarNew)
	ON_EN_CHANGE(IDC_VAR_NAME, OnChangeVarNameOrValue)
	ON_EN_CHANGE(IDC_VAR_VALUE, OnChangeVarNameOrValue)
	ON_LBN_SETFOCUS(IDC_EQUIV_LIST, OnSetfocusEquivList)
	ON_LBN_SETFOCUS(IDC_META_LIST, OnSetfocusMetaList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


// This needs to match contents of Font Size drop-down listbox
// 0 = "Default", 1 - 7 = font sizes, 8 = "Don't Change"
static int iDontChangeFontSizeIndex = 8;
static int iDontChangeFontColorIndex = 0;

///////////////////////////////////////////////////////////////////
CCharacterPage::CCharacterPage(CWnd* pParent,
                               MWContext *pMWContext,
                               CEditorResourceSwitcher * pResourceSwitcher,
                               EDT_CharacterData *pData)
    : CNetscapePropertyPage(CCharacterPage::IDD),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher),
      m_pData(pData),
      m_bActivated(0),
      m_pPageData(0),
      m_crDefault(0),
      m_bCustomColor(0),
      m_iNoChangeFontSize(0),
      m_iNoChangeFontFace(0),
      m_iOtherIndex(0),
      m_bMultipleFonts(0),
      m_bUseDefault(0)
{
    //{{AFX_DATA_INIT(CCharacterPage)
    //}}AFX_DATA_INIT
    ASSERT(pMWContext);
    // Could we ever get here and have no data?
    // We shouldn't, but maybe we shouldn't assume it here
    //  and just gray all controls if no character data.
    ASSERT(pData);

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}


void CCharacterPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCharacterPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CCharacterPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CCharacterPage)
    ON_BN_CLICKED(IDC_OVERRIDE_COLOR, EnableApply)
    ON_BN_CLICKED(IDC_DONT_CHANGE_COLOR, EnableApply)
	ON_BN_CLICKED(IDC_CHECK_BOLD, EnableApply)
	ON_BN_CLICKED(IDC_CHECK_ITALIC, EnableApply)
	ON_BN_CLICKED(IDC_CHECK_UNDERLINE, EnableApply)
	ON_BN_CLICKED(IDC_CHECK_STRIKETHROUGH, EnableApply)
	ON_BN_CLICKED(IDC_CHECK_BLINK, EnableApply)
	ON_BN_CLICKED(IDC_CHECK_NOBREAK, EnableApply)
	ON_BN_CLICKED(IDC_CHECK_SUBSCRIPT, OnCheckSubscript)
	ON_BN_CLICKED(IDC_CHECK_SUPERSCRIPT, OnCheckSuperscript)
	ON_BN_CLICKED(IDC_CLEAR_ALL_STYLES, OnClearAllStyles)
	ON_BN_CLICKED(IDC_CLEAR_TEXT_STYLES, OnClearTextStyles)
	ON_CBN_SELCHANGE(IDC_FONT_SIZE_COMBO, OnSelchangeFontSize)
	ON_CBN_SELCHANGE(IDC_FONT_SIZE_COMBO2, OnSelchangeFontSize)
	ON_CBN_SELCHANGE(IDC_FONTFACE_COMBO, OnSelchangeFontFace)
	ON_CBN_EDITCHANGE(IDC_FONTFACE_COMBO, OnChangeFontFace)
	ON_CBN_EDITCHANGE(IDC_FONT_SIZE_COMBO2, EnableApply)
	ON_CBN_SELENDOK(IDC_FONTFACE_COMBO, OnSelendokFontFace)
	ON_BN_CLICKED(IDC_CHOOSE_COLOR, OnChooseColor)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
//	ON_BN_CLICKED(IDC_CHOOSE_FONT, OnChooseLocalFont)

BOOL CCharacterPage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated){
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return FALSE;

    if(m_bActivated)
        return TRUE;

    // Switch back to EXE's resources
    if( m_pResourceSwitcher ){
        m_pResourceSwitcher->Reset();
    }

    m_pPageData = EDT_GetPageData(m_pMWContext);

    if( m_pPageData == NULL ){
        return FALSE;
    }
   
    // Note: Don't set m_bActivated yet! -- SEE BELOW

    // Attach color picker button to our color button class
    if( !m_TextColorButton.Subclass(this, IDC_CHOOSE_COLOR, &m_crColor) ){
        return FALSE;
    }
    
    // Get the "Default" color used by the Browser
	COLORREF tmpColor,defColor;
	XP_Bool bCust;
	PREF_GetColorPrefDWord("browser.foreground_color",&tmpColor);
	PREF_GetDefaultColorPrefDWord("browser.foreground_color",&defColor);
	PREF_GetBoolPref("browser.custom_text_color",&bCust);
    m_crDefault = bCust ? tmpColor : defColor;

    // Get current color at cursor or selection
    m_crColor = WFE_GetCurrentFontColor(m_pMWContext);

    BOOL bUseColor = TRUE;
    if( m_crColor == DEFAULT_COLORREF){
        // Show the default color on the button
        m_crColor = m_crDefault;
        m_bUseDefault = TRUE;
    }
    if( m_crColor == MIXED_COLORREF ){
        // Change the text on radio button to: "Don't change (mixed colors)
        GetDlgItem(IDC_DONT_CHANGE_COLOR)->SetWindowText(szLoadString(IDS_MIXED_COLORS));
        ((CButton*)GetDlgItem(IDC_DONT_CHANGE_COLOR))->SetCheck(1);
    } else {
        ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->SetCheck(1);
    }
    
    // Attach CNSComboBox class to existing control to do user-drawing
	if( !m_FontFaceCombo.Subclass(this, IDC_FONTFACE_COMBO) ){
        return FALSE;
    }
    
    // Fill the combobox with global lists of fonts (MUST BE STATIC STRINGS!)
    // This will add "Other..." at bottom of list only if too many fonts are installed
    m_iOtherIndex = wfe_FillFontComboBox(&m_FontFaceCombo);

    // Get current font face and index to it in font list
    char * pFace = EDT_GetFontFace(m_pMWContext);
    int iFontIndex = m_FontFaceCombo.FindSelectedOrSetText(pFace);
    
    // Save whether or not we have mulitple fonts at startup
    m_bMultipleFonts = (pFace == NULL);
    char * pNoChangeText = m_bMultipleFonts ? ed_pMixedFonts : ed_pDontChange;
    // Add the "Dont Change" item at the bottom
    m_iNoChangeFontFace = m_FontFaceCombo.AddString(pNoChangeText);
    
    // Set the message explaining the font after selecting it in listbox
    SetFontFaceMessage(iFontIndex);

    // We need to rebuild font size display if font face changes from
    //   Variable to Fixed Width or vice versa, so monitor state
    m_bFixedWidth = (iFontIndex == 1);

    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_BOLD)),TF_BOLD);
    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_ITALIC)), TF_ITALIC);
    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_UNDERLINE)), TF_UNDERLINE);
    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_STRIKETHROUGH)), TF_STRIKEOUT);
    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_SUPERSCRIPT)), TF_SUPER);
    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_SUBSCRIPT)), TF_SUB); 
    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_BLINK)), TF_BLINK);      
    InitCheckbox(((CButton*)GetDlgItem(IDC_CHECK_NOBREAK)), TF_NOBREAK); 

    // We have 2 comboboxes: 1 droplist for regular (relative size) mode,
    //   and combobox with edit style for advanced mode
    if( !m_FontSizeCombo.Subclass(this, wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED ? 
                                        IDC_FONT_SIZE_COMBO2 : IDC_FONT_SIZE_COMBO) ){
        return FALSE;
    }
    // Hide the combobox we will not use
    GetDlgItem(wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED ? IDC_FONT_SIZE_COMBO : IDC_FONT_SIZE_COMBO2)->ShowWindow(SW_HIDE);


    // Fill Font Size combo. 3rd param is TRUE if font face = Fixed Width
    wfe_FillFontSizeCombo(m_pMWContext, &m_FontSizeCombo, m_bFixedWidth);

    // Add the "Dont Change" item at the bottom
    m_iNoChangeFontSize = m_FontSizeCombo.AddString(ed_pDontChange);

    // Change message to that describing advanced mode
    if( wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED ){
        GetDlgItem(IDC_FONT_SIZE_MSG)->SetWindowText(szLoadString(IDS_ADVANCED_FONTSIZE_MSG));
    }

    // Set FontSize combobox selected
    int iSel = -1;
    if( m_pData->mask & TF_FONT_SIZE ){
        // If no size, use default instead
        iSel =  m_pData->iSize ? m_pData->iSize - 1 : 2;
        m_FontSizeCombo.SetCurSel(iSel);
        // Weird -- we must set string manually
        char * pSelected = (char*)m_FontSizeCombo.GetItemData(iSel);
        if( *pSelected == '_'){
            pSelected++;
        }
        m_FontSizeCombo.SetWindowText(pSelected);
    } else if( (m_pData->mask & TF_FONT_POINT_SIZE) && m_pData->iPointSize ){
        char pSize[16];
        wsprintf(pSize, "%d", m_pData->iPointSize);
        m_FontSizeCombo.FindSelectedOrSetText(pSize, MAX_FONT_SIZE);
    }

    // We set this last because initialization
    //  uses it to know first-time through
	m_bActivated = TRUE;

    // Send data to controls
    UpdateData(FALSE);
	return(TRUE);
}

void CCharacterPage::OnHelp() 
{
    NetHelp(HELP_PROPS_CHARACTER);
}

BOOL CCharacterPage::OnKillActive()

{
    if( ! GetAndValidateFontSizes() ){
        return FALSE;
    }
    // Contrary to MFC help, this does NOT call our OnOK
    return CPropertyPage::OnKillActive();
}

BOOL CCharacterPage::GetAndValidateFontSizes()
{
    int iSel = m_FontSizeCombo.GetCurSel();
    if( iSel == iDontChangeFontSizeIndex ||
        (iSel == -1 && wfe_iFontSizeMode != ED_FONTSIZE_ADVANCED) ){
        // Ignore this attribute
        FE_CLEAR_BIT(m_pData->mask, TF_FONT_SIZE);
        return TRUE;
    }
    if( wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED){
        CString csAbsoluteSize;
        m_FontSizeCombo.GetWindowText(csAbsoluteSize);
        csAbsoluteSize.TrimLeft();
        csAbsoluteSize.TrimRight();

        // Empty is OK - same as "Don't change"
        // So get the Relative size 
        if( csAbsoluteSize.IsEmpty() ){
            // We may be ignore this attribute if no
            //  relative size is selected
            FE_CLEAR_BIT(m_pData->mask, TF_FONT_SIZE);
            m_pData->iPointSize = 0;
        } else if( iSel == -1 || iSel >= MAX_FONT_SIZE ){
            char* pEnd;
            int32 iSize = (int)strtol( LPCSTR(csAbsoluteSize), &pEnd, 10 );
            // Bad conversion if end pointer isn't at terminal null;
            if( *pEnd != '\0' ||
                iSize < MIN_FONT_SIZE_RELATIVE ||
                iSize > ED_FONT_POINT_SIZE_MAX ){
        
                char szMessage[256];
                // Construct a string showing correct range
                wsprintf( szMessage, szLoadString(IDS_INTEGER_RANGE_ERROR),
                          ED_FONT_POINT_SIZE_MIN, ED_FONT_POINT_SIZE_MAX );

                // Notify user with similar message to the DDV_ validation system
                MessageBox(szMessage, szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);
                return FALSE;
            }
            if( iSize >= MIN_FONT_SIZE_RELATIVE && iSize <= 4 ){
                // Assume they really wanted the Relative scale
                iSel = iSize + 3;
                m_pData->iPointSize = 0;
            } else {
                // Size is OK - store it
                m_pData->iPointSize = (int16)iSize;

                // Set bit mask to indicate we want PointSize, not HTML relative size
                FE_CLEAR_BIT(m_pData->mask, TF_FONT_SIZE);
                FE_SET_BIT(m_pData->mask, TF_FONT_POINT_SIZE);
                FE_SET_BIT(m_pData->values, TF_FONT_POINT_SIZE);
                m_pData->iPointSize = (int16)iSize;
            }
        }
    }

    if( m_pData->iPointSize == 0){
        if( m_iNoChangeFontSize > 0 && iSel == m_iNoChangeFontSize ){
            // Clear the bit so we will ignore this attribute
            FE_CLEAR_BIT(m_pData->mask, TF_FONT_SIZE);
        } else if( iSel >= 0 && iSel < MAX_FONT_SIZE ){
            // Set the size to the designated value - XP handles default
            // (3rd item = "default", i.e., no SIZE param written)
            FE_SET_BIT(m_pData->mask, TF_FONT_SIZE);
            FE_SET_BIT(m_pData->values, TF_FONT_SIZE);
            m_pData->iSize = iSel+1;
        }
    }
    return TRUE;
}

void CCharacterPage::OnOK() 
{
    CPropertyPage::OnOK();

    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this)){
        return;
    }    
    
    //EDT_BeginBatchChanges(m_pMWContext);
    if( !GetAndValidateFontSizes() ){
        return;
    }

    // Set color if checkbox is checked
    if( ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->GetCheck() ){
        if( m_bUseDefault ){
            // Set the color to the default size by
            //  setting mask bit and clearing value
            FE_SET_BIT(m_pData->mask, TF_FONT_COLOR);
            FE_CLEAR_BIT(m_pData->values, TF_FONT_COLOR);
        } else {
            // Set the color to the designated value
            FE_SET_BIT(m_pData->mask, TF_FONT_COLOR);
            FE_SET_BIT(m_pData->values, TF_FONT_COLOR);
            WFE_SetLO_ColorPtr( m_crColor, &m_pData->pColor );
        }
    } else {
        // Ignore this attribute
        FE_CLEAR_BIT(m_pData->mask, TF_FONT_COLOR);
    }
     
    int iSel = m_FontFaceCombo.GetCurSel();
    CString csFace;
    char * pNewFace = NULL;
    if( iSel == -1 ){
        m_FontFaceCombo.GetWindowText(csFace);
        csFace.TrimLeft();
        csFace.TrimRight();
        if( !csFace.IsEmpty() ){
            // We have a local font name
            pNewFace = (char*)LPCSTR(csFace);
            FE_SET_BIT(m_pData->mask, TF_FONT_FACE);
        }
    } else if( m_iNoChangeFontFace > 0 && iSel == m_iNoChangeFontFace ){
        // Clear the bit so we will ignore this attribute
        FE_CLEAR_BIT(m_pData->mask, TF_FONT_FACE);
    } else {
        // We may be sure of the font and its in our list
        //  (EDT_SetFontFace will set default variable or fixed width 
        //   correctly using the supplied "font face" string
        FE_SET_BIT(m_pData->mask, TF_FONT_FACE);
        // Get pointer to fontface string in combobox
        pNewFace = (char*)m_FontFaceCombo.GetItemData(iSel);
    }
    // Set the bits in our data struct
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_BOLD)), TF_BOLD);
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_ITALIC)), TF_ITALIC);
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_UNDERLINE)), TF_UNDERLINE);
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_STRIKETHROUGH)), TF_STRIKEOUT);
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_SUPERSCRIPT)), TF_SUPER);
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_SUBSCRIPT)), TF_SUB);
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_BLINK)), TF_BLINK);
    SetCharacterStyle(((CButton*)GetDlgItem(IDC_CHECK_NOBREAK)), TF_NOBREAK);

    // Set font face in the supplied struct and set all character data
    EDT_SetFontFace(m_pMWContext, m_pData, iSel, pNewFace);

    OkToClose();
    //EDT_EndBatchChanges(m_pMWContext);
}

void CCharacterPage::InitCheckbox(CButton *pButton, ED_TextFormat tf)
{
    // Get current style but clear all relevant button-style bits
    UINT nStyle = CASTUINT(pButton->GetButtonStyle() & 0xFFFFFFF0L);

    if( m_pData->mask & tf ) {
        // We know the style, so we can use 2-state boxes
        nStyle |= BS_AUTOCHECKBOX;
    } else {
        // We don't know the style, so we use 3-state boxes
        //  so user can return to the "don't change" state
        nStyle |= BS_AUTO3STATE;
    }
    pButton->SetButtonStyle(nStyle);
    SetCheck(pButton, tf);
}

int CCharacterPage::SetCheck(CButton *pButton, ED_TextFormat tf)
{
    int iCheck = 2; // Uncertain/mixed state
    if( m_pData->mask & tf){ 
        iCheck = (m_pData->values & tf) ? 1 : 0;
    }
    pButton->SetCheck(iCheck);
    return iCheck;
}

void CCharacterPage::SetCharacterStyle(CButton *pButton, ED_TextFormat tf)
{
    int iCheck = pButton->GetCheck();
    
    if( iCheck == 2 ){
        // Don't change this style - clear mask bit
        FE_CLEAR_BIT(m_pData->mask,tf);
    } else {
        // Set the style -- set mask bit
        FE_SET_BIT(m_pData->mask, tf);
    }

    if( iCheck == 1){
        // Set the value bit
        FE_SET_BIT(m_pData->values,tf);
    } else {
        FE_CLEAR_BIT(m_pData->values,tf);
    }
}

void CCharacterPage::OnChooseLocalFont()
{
    CFontDialog dlg(NULL, CF_SCREENFONTS | CF_TTONLY, NULL, this);
    if( dlg.DoModal() ){
        CString csNewFont = dlg.GetFaceName();
        m_FontFaceCombo.SetWindowText(LPCSTR(csNewFont));
        GetDlgItem(IDC_FONTFACE_MSG)->SetWindowText(szLoadString(IDS_TRUE_TYPE));
        SetModified(TRUE);
    }
}

void CCharacterPage::OnChooseColor() 
{
    // Get the combobox location so we popup new dialog just under it
    RECT rect;
    GetDlgItem(IDC_CHOOSE_COLOR)->GetWindowRect(&rect);

    CColorPicker ColorPicker(this, m_pMWContext, m_crColor, DEFAULT_COLORREF, 0, &rect);
    COLORREF crNew = ColorPicker.GetColor();

    if( crNew != CANCEL_COLORREF ){
        if( crNew == DEFAULT_COLORREF ){
            m_crColor = m_crDefault;
            m_bUseDefault = TRUE;
        } else {
            m_crColor = crNew;
            m_bUseDefault = FALSE;
        }
        // Refresh color button
        m_TextColorButton.Update();
        // Set radio buttons to show "Use color"
        ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->SetCheck(1);
        ((CButton*)GetDlgItem(IDC_DONT_CHANGE_COLOR))->SetCheck(0);
        SetModified(TRUE);
    }
}

void CCharacterPage::EnableApply() 
{
    SetModified(TRUE);
}

void CCharacterPage::OnCheckSubscript() 
{
    // Super and Sub are mutually exclusive
    // Problem: This wipes out mixed-state items!
    if( ((CButton*)GetDlgItem(IDC_CHECK_SUBSCRIPT))->GetCheck() == 1 ){
        ((CButton*)GetDlgItem(IDC_CHECK_SUPERSCRIPT))->SetCheck(0);
    }
    SetModified(TRUE);
}

void CCharacterPage::OnCheckSuperscript() 
{
    // Super and Sub are mutually exclusive
    // Problem: This wipes out mixed-state items!
    if( ((CButton*)GetDlgItem(IDC_CHECK_SUPERSCRIPT))->GetCheck() == 1 ){
        ((CButton*)GetDlgItem(IDC_CHECK_SUBSCRIPT))->SetCheck(0);
    }
    SetModified(TRUE);
}

// Next 2 don't change the doc, 
//  just change the control states
void CCharacterPage::OnClearAllStyles() 
{
    // Remove all styles in the controls
    
    // Set to the "default" font size
    m_FontSizeCombo.SetCurSel(2);
    // Update the display
    OnSelchangeFontSize();

    // Set to default color
    m_bUseDefault = TRUE;
    m_crColor = m_crDefault;
    m_TextColorButton.Update();

    ((CButton*)GetDlgItem(IDC_DONT_CHANGE_COLOR))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_OVERRIDE_COLOR))->SetCheck(1);
   
    // Set to default variable font face
    m_FontFaceCombo.SetCurSel(0);
    OnSelchangeFontFace();
    
    OnClearTextStyles();
}

void CCharacterPage::OnClearTextStyles() 
{
    ((CButton*)GetDlgItem(IDC_CHECK_BOLD))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_CHECK_ITALIC))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_CHECK_UNDERLINE))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_CHECK_STRIKETHROUGH))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_CHECK_SUPERSCRIPT))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_CHECK_SUBSCRIPT))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_CHECK_BLINK))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_CHECK_NOBREAK))->SetCheck(0);
    SetModified(TRUE);
}

void CCharacterPage::OnChangeFontFace() 
{
    GetDlgItem(IDC_FONTFACE_MSG)->SetWindowText("");
    SetModified(TRUE);
    // Do any autosearch here?
}

void CCharacterPage::SetFontFaceMessage(int iSel)
{
    UINT nID;
    if( iSel == -1 || 
        (iSel == m_iNoChangeFontFace && !m_bMultipleFonts) ||
        (m_iOtherIndex && iSel == m_iOtherIndex) ){
        GetDlgItem(IDC_FONTFACE_MSG)->SetWindowText("");
        return;
    }
    if( iSel == m_iNoChangeFontFace ){
        nID = IDS_MULTIPLE_FONTS;
    } else if( iSel == 0){
        nID = IDS_DEFAULT_VARIABLE;
    } else if ( iSel == 1 ){
        nID = IDS_DEFAULT_FIXED_WIDTH;
    } else { 
        char * pFace = (char*)m_FontFaceCombo.GetItemData(iSel);
        // Give a different message if we find an "XP" font face
        if( pFace != EDT_TranslateToXPFontFace(pFace) ){
            nID = IDS_XP_FONT;
        } else {
            nID = IDS_LOCAL_FONT;
        }
    }
    GetDlgItem(IDC_FONTFACE_MSG)->SetWindowText(szLoadString(nID));
}

void CCharacterPage::OnSelchangeFontFace() 
{
    int iSel = m_FontFaceCombo.GetCurSel();
    if( m_iOtherIndex && iSel == m_iOtherIndex ){
        m_FontFaceCombo.SetCurSel(-1);
        iSel = -1;
    }
    if( iSel >= 0 ){
        char * pSelected = (char*)m_FontFaceCombo.GetItemData(iSel);
        if( *pSelected == '_'){
            pSelected++;
        }
        m_FontFaceCombo.SetWindowText(pSelected);
    }
    // Rebuild font size list if changing from variable to fixed width or vice versa
    if( m_bFixedWidth != (iSel == 1) ){
        m_bFixedWidth = (iSel == 1);
        wfe_FillFontSizeCombo(m_pMWContext, &m_FontSizeCombo, m_bFixedWidth);
    }
    SetFontFaceMessage(iSel);
    SetModified(TRUE);
}

void CCharacterPage::OnSelendokFontFace()
{
    int iSel = m_FontFaceCombo.GetCurSel();
    if( m_iOtherIndex &&
        iSel == m_iOtherIndex ){
        m_FontFaceCombo.SetCurSel(-1);
        OnChooseLocalFont();
        return;
    }
}

void CCharacterPage::OnSelchangeFontSize() 
{
    // User can use the last item to not change any sizes,
    //  but it looks better to show blank combobox field
    int iSel = m_FontSizeCombo.GetCurSel();
    if( iSel == iDontChangeFontSizeIndex ){
        m_FontSizeCombo.SetCurSel(-1);
    }
    if( wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED ){
        // We must set text manually in edit box
        if( iSel >= 0 ){
            char * pSelected = (char*)m_FontSizeCombo.GetItemData(iSel);
            if( *pSelected == '_'){
                pSelected++;
            }
            m_FontSizeCombo.SetWindowText(pSelected);
        } else {
            m_FontSizeCombo.SetWindowText("");
        }
    } else {
        m_pData->iPointSize = 0;
    }
    SetModified(TRUE);
}

void CCharacterPage::OnClose() 
{
    // Free extra data we allocated
	if( m_pPageData	){
        EDT_FreePageData(m_pPageData);
    }
	CNetscapePropertyPage::OnClose();
}

///////////////////////////////////////////////////////////////////
/*
// For quick reference:
typedef enum {
    ED_LIST_TYPE_DEFAULT,
    ED_LIST_TYPE_DIGIT,         
    ED_LIST_TYPE_ROMAN,
    ED_LIST_TYPE_BIG_LETTERS,
    ED_LIST_TYPE_SMALL_LETTERS,
    ED_LIST_TYPE_CIRCLE,
    ED_LIST_TYPE_SQUARE,
    ED_LIST_TYPE_DISC,
} ED_ListType;


struct _EDT_ListData {
    intn iTagType;         P_UNUM_LIST, P_NUM_LIST, P_BLOCKQUOTE
                           P_DIRECTORY, P_MENU, P_DESC_LIST
    Bool bCompact;
    ED_ListType eType;
    int iStart;            automatically maps, start is 1
};
*/

//Container styles:
enum {
    ED_NO_CONTAINER,
    ED_LIST,
    ED_BLOCKQUOTE
};


// List Styles
enum {
    ED_UNUM_LIST,
    ED_NUM_LIST,
    ED_DIR_LIST,
    ED_MENU_LIST,
    ED_DESC_LIST
};


//Numbered List Item styles
enum {
    ED_AUTOMATIC,
    ED_DIGIT,
    ED_BIG_ROMAN,
    ED_SMALL_ROMAN,
    ED_BIG_LETTERS,
    ED_SMALL_LETTERS
};

//Unumbered List Item styles
enum {
    ED_SOLID_CIRCLE = 1, // We share     ED_AUTOMATIC,
    ED_OPEN_CIRCLE,
    ED_SOLID_SQUARE
};

#define ED_DEFAULT     -1

// This is in edframe.cpp. Its the tag types that map
//  onto the listbox index, just as in the paragraph format toolbar
extern TagType FEED_nParagraphTags[];

// Fixed index values for Paragraph tags list
#define ED_PARA_NORMAL     0
#define ED_PARA_LIST       9
#define ED_PARA_DESC_TITLE 10
#define ED_PARA_DESC_TEXT  11

/////////////////////////////////////////
// Paragraph / Lists / Alignment
//
CParagraphPage::CParagraphPage(CWnd* pParent, MWContext * pMWContext,
                               CEditorResourceSwitcher * pResourceSwitcher)
    : CNetscapePropertyPage(CParagraphPage::IDD),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher),
      m_pListData(NULL),
      m_bActivated(0),
      m_Align(ED_ALIGN_LEFT),
      m_iParagraphStyle(0),
      m_iContainerStyle(0),
      m_iListStyle(-1),
      m_iBulletStyle(0),
      m_iNumberStyle(0),
      m_iMixedStyle(0)
{
    //{{AFX_DATA_INIT(CParagraphPage)
	m_iStartNumber = 1;
	m_bCompact = FALSE;
	//}}AFX_DATA_INIT
    ASSERT(pMWContext);

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}


void CParagraphPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CParagraphPage)
	DDX_Text(pDX, IDC_LIST_START_NUMBER, m_iStartNumber);
	DDV_MinMaxInt(pDX, m_iStartNumber, 1, 10000);
	DDX_Check(pDX, IDC_COMPACT_LIST, m_bCompact);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CParagraphPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CParagraphPage)
	ON_BN_CLICKED(IDC_ALIGN_CENTER, OnAlignCenter)
	ON_BN_CLICKED(IDC_ALIGN_LEFT, OnAlignLeft)
	ON_BN_CLICKED(IDC_ALIGN_RIGHT, OnAlignRight)
	ON_CBN_SELCHANGE(IDC_CONTAINER_STYLES, OnSelchangeContainerStyle)
	ON_CBN_SELCHANGE(IDC_LIST_ITEM_STYLES, OnSelchangeListItemStyle)
	ON_EN_CHANGE(IDC_LIST_START_NUMBER, OnChangeListStartNumber)
	ON_CBN_SELCHANGE(IDC_LIST_STYLES, OnSelchangeListStyles)
	ON_CBN_SELCHANGE(IDC_PARAGRAPH_STYLES, OnSelchangeParagraphStyles)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CParagraphPage::OnSetActive() 
{
	int i;
	if(m_pResourceSwitcher && !m_bActivated){
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);


    // Switch back to EXE's resources
    if( m_pResourceSwitcher ){
        m_pResourceSwitcher->Reset();
    }

	m_bActivated = TRUE;
    CComboBox * pContainerStyles;
    CComboBox * pListStyles;

    // Initialize alignment param and radio buttons
    m_Align = EDT_GetParagraphAlign( m_pMWContext );

    switch (m_Align) {
        case ED_ALIGN_CENTER:
        case ED_ALIGN_ABSCENTER:
            ((CButton*)GetDlgItem(IDC_ALIGN_CENTER))->SetCheck(1);
            break;
        case ED_ALIGN_RIGHT:
            ((CButton*)GetDlgItem(IDC_ALIGN_RIGHT))->SetCheck(1);
            break;
        default:
            ((CButton*)GetDlgItem(IDC_ALIGN_LEFT))->SetCheck(1);
            // We end up here for ED_ALIGN_DEFAULT as well as ED_ALIGN_LEFT
            m_Align = ED_ALIGN_LEFT;    
            break;
    }

    CComboBox *pParagraphStyles = (CComboBox*)GetDlgItem(IDC_PARAGRAPH_STYLES);
    
    m_ParagraphFormat = EDT_GetParagraphFormatting(m_pMWContext);
    //Try to get Container data
    // (We can't do this just for list because we may be
    //  a block quote, which may have any paragraph style)
    m_pListData = EDT_GetListData(m_pMWContext);

    // Fill the Paragraph styles list:	
	for ( i = 0; FEED_nParagraphTags[i] != P_UNKNOWN; i++ ){
        // Major KLUDGE: We want "Description List" to appear in Toolbar Combobox,
        //   but not in this combobox - we put it in List Styles combo instead
//        if( FEED_nParagraphTags[i] != P_DESC_LIST)
		    pParagraphStyles->AddString(szLoadString(
		                            CASTUINT(ID_LIST_TEXT_PARAGRAPH_BASE+FEED_nParagraphTags[i])));
    }
    // We will add another item at end to allow user to see
    // "Mixed (don't change)" in listbox
    m_iMixedStyle = i;

    // Figure out the Paragraph style:
    if( m_ParagraphFormat == P_UNKNOWN ){
        m_iParagraphStyle = m_iMixedStyle;
		pParagraphStyles->AddString(szLoadString(IDS_MIXED_STYLE));
    } 
    else for( i = 0; FEED_nParagraphTags[i] != P_UNKNOWN; i++ ){
		if( FEED_nParagraphTags[i] == m_ParagraphFormat ){
		    m_iParagraphStyle = i;
		    break;
		}
    }

    if( m_pListData ) {
        m_iContainerStyle = ED_LIST;
        switch( m_pListData->iTagType ){
            case P_UNUM_LIST:
                m_iListStyle = ED_UNUM_LIST;
                break;
            case P_NUM_LIST:
                m_iListStyle = ED_NUM_LIST;
                m_iStartNumber = CASTINT(m_pListData->iStart);
                break;
            case P_BLOCKQUOTE:
                m_iContainerStyle = ED_BLOCKQUOTE;
                m_iListStyle = ED_DEFAULT;
                break;
            case P_DIRECTORY:
                m_iListStyle = ED_DIR_LIST;
                break;
            case P_MENU:
                m_iListStyle = ED_MENU_LIST;
                break;
            case P_DESC_LIST:
                m_iListStyle = ED_DESC_LIST;
                break;
        }

        m_iBulletStyle = m_iNumberStyle = ED_DEFAULT;
        switch( m_pListData->eType ){
            case ED_LIST_TYPE_DIGIT:         
                m_iNumberStyle = ED_DIGIT;
                break;
            case ED_LIST_TYPE_BIG_ROMAN:
                m_iNumberStyle = ED_BIG_ROMAN;
                break;
            case ED_LIST_TYPE_SMALL_ROMAN:
                m_iNumberStyle = ED_SMALL_ROMAN;
                break;
            case ED_LIST_TYPE_BIG_LETTERS:
                m_iNumberStyle = ED_BIG_LETTERS;
                break;
            case ED_LIST_TYPE_SMALL_LETTERS:
                m_iNumberStyle = ED_SMALL_LETTERS;
                break;
            case ED_LIST_TYPE_DISC:
                m_iBulletStyle = ED_SOLID_CIRCLE;
                break;
            case ED_LIST_TYPE_CIRCLE:
                m_iBulletStyle = ED_OPEN_CIRCLE;
                break;
            case ED_LIST_TYPE_SQUARE:
                m_iBulletStyle = ED_SOLID_SQUARE;
                break;
        }
        m_bCompact = m_pListData->bCompact;
    } else {
        m_iContainerStyle = ED_NO_CONTAINER;
    }


    VERIFY(pContainerStyles = (CComboBox*)GetDlgItem(IDC_CONTAINER_STYLES));
    VERIFY(pListStyles = (CComboBox*)GetDlgItem(IDC_LIST_STYLES));

    // Fill the Container styles list
    pContainerStyles->AddString(szLoadString(IDS_DEFAULT));
    pContainerStyles->AddString(szLoadString(IDS_LIST));
    pContainerStyles->AddString(szLoadString(IDS_BLOCK_QUOTE));
    
    
    // Fill the List styles list
    pListStyles->AddString(szLoadString(IDS_UNUM_LIST));
    pListStyles->AddString(szLoadString(IDS_NUM_LIST));
    pListStyles->AddString(szLoadString(IDS_DIRECTORY_LIST));
    pListStyles->AddString(szLoadString(IDS_MENU_LIST));
    pListStyles->AddString(szLoadString(IDS_DESCRIPTION_LIST));

    // Remove/rebuild item styles depending on current state
    UpdateLists();

    // TODO - How to get current alignment?
    //        (Set align radio button to this value)

    // Send data to controls
    UpdateData(FALSE);
	return(TRUE);
}

void CParagraphPage::OnHelp() 
{
    NetHelp(HELP_PROPS_PARAGRAPH);
}

void CParagraphPage::OnOK() 
{
    CPropertyPage::OnOK();

    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this)){
        return;
    }    

    //EDT_BeginBatchChanges(m_pMWContext);

    m_iParagraphStyle = ((CComboBox*)GetDlgItem(IDC_PARAGRAPH_STYLES))->GetCurSel();
    m_iContainerStyle = ((CComboBox*)GetDlgItem(IDC_CONTAINER_STYLES))->GetCurSel();
    m_iListStyle = ((CComboBox*)GetDlgItem(IDC_LIST_STYLES))->GetCurSel();

    // Set alignment if it was changed
    EDT_SetParagraphAlign( m_pMWContext, m_Align );

    // Default values
    TagType     iTagType = P_UNUM_LIST;
    ED_ListType iListItemType = ED_LIST_TYPE_DEFAULT;
    CComboBox * pListItemStyles = (CComboBox*)GetDlgItem(IDC_LIST_ITEM_STYLES);

    if( m_iContainerStyle == ED_BLOCKQUOTE ){
        iTagType = P_BLOCKQUOTE;
    } else if( m_iContainerStyle == ED_LIST ){
        switch( m_iListStyle ){
            case ED_NUM_LIST:
                iTagType = P_NUM_LIST;
                break;
            case ED_DIR_LIST:
                iTagType = P_DIRECTORY;
                break;
            case ED_MENU_LIST:
                iTagType = P_MENU;
                break;
            case ED_DESC_LIST:
                iTagType = P_DESC_LIST;
                break;
            //Note: UNUM_LIST is default
        }

        if( iTagType == P_UNUM_LIST ) {
            m_iBulletStyle = pListItemStyles->GetCurSel();
            switch( m_iBulletStyle ){
                case ED_SOLID_CIRCLE:
                    iListItemType = ED_LIST_TYPE_DISC;
                    break;
                case ED_OPEN_CIRCLE:
                    iListItemType = ED_LIST_TYPE_CIRCLE;
                    break;
                case ED_SOLID_SQUARE:
                    iListItemType = ED_LIST_TYPE_SQUARE;
                    break;
            }
        } else if( iTagType == P_NUM_LIST ) {
            m_iNumberStyle = pListItemStyles->GetCurSel();
            switch( m_iNumberStyle ){
                case ED_DIGIT:
                    iListItemType = ED_LIST_TYPE_DIGIT;
                    break;
                case ED_BIG_ROMAN:
                    iListItemType = ED_LIST_TYPE_BIG_ROMAN;
                    break;
                case ED_SMALL_ROMAN:
                    iListItemType = ED_LIST_TYPE_SMALL_ROMAN;
                    break;
                case ED_BIG_LETTERS:
                    iListItemType = ED_LIST_TYPE_BIG_LETTERS;
                    break;
                case ED_SMALL_LETTERS:
                    iListItemType = ED_LIST_TYPE_SMALL_LETTERS;
                    break;
            }
    	}
    }

    //TODO: CHECK THIS -- WE MAY NOT WANT TO DO IT
    // We don't want a container but had one before
    if( m_pListData && m_iContainerStyle == ED_NO_CONTAINER ){
        // TODO: SHOULD WE UNDO INDENT FOR LIST ITEMS ALSO?
//        if ( iTagType == P_BLOCKQUOTE ) {
            EDT_ListData * pListData;
            // Repeat removing indent until last Unnumbered list is gone
            while ( (pListData = EDT_GetListData(m_pMWContext)) ){
                EDT_FreeListData(pListData);
                EDT_Outdent(m_pMWContext);
            }
//        }
    }

    // We should have been maintaining paragraph style
    //  to match container/list style, so this should work
    // But if "Don't change" style, NEVER set the container style
    if( m_iParagraphStyle != m_iMixedStyle ){
        TagType NewFormat = FEED_nParagraphTags[m_iParagraphStyle];
        if( m_ParagraphFormat != NewFormat ){
            EDT_MorphContainer( m_pMWContext, NewFormat);
        }
    }

    // We didn't already have a container but want one - create it
    if( ! m_pListData && m_iContainerStyle != ED_NO_CONTAINER) {
        EDT_Indent(m_pMWContext);
        m_pListData = EDT_GetListData(m_pMWContext);
    }

    // Set List type and attributes
    if ( m_pListData ) {
        // If no container, we Outdented above, so don't set data here
        if( m_iContainerStyle != ED_NO_CONTAINER ) {
            m_pListData->iTagType = iTagType;
            m_pListData->eType = iListItemType;
            m_pListData->bCompact = m_bCompact;
            m_pListData->iStart = m_iStartNumber;
        
            // Now we can change the list attribute:
            EDT_SetListData( m_pMWContext, m_pListData );
        }
    }
    OkToClose();
    //EDT_EndBatchChanges(m_pMWContext);
}

// Set listbox selected items depending on current state
// Primary key is m_iContainerStyle
// Other style values will be kept but ignored when setting
//   selected item or disabling/enabling dependent listboxes
// Goal is to show list style and bullet/numbering only if appropriate
//
void CParagraphPage::UpdateLists()
{
    // Set selected items    
    ((CComboBox*)GetDlgItem(IDC_PARAGRAPH_STYLES))->SetCurSel(m_iParagraphStyle);
    ((CComboBox*)GetDlgItem(IDC_CONTAINER_STYLES))->SetCurSel(m_iContainerStyle);
    
    // Remove and disable the selection in List Styles if not a List,
    //  else show what kind of list it is
    ((CComboBox*)GetDlgItem(IDC_LIST_STYLES))->SetCurSel( 
        m_iContainerStyle == ED_LIST ? m_iListStyle : -1);
    ((CComboBox*)GetDlgItem(IDC_LIST_STYLES))->EnableWindow(m_iContainerStyle == ED_LIST);

    // Rebuild the List Items styles list
    CComboBox * pListItemStyles = (CComboBox*)GetDlgItem(IDC_LIST_ITEM_STYLES);
    pListItemStyles->ResetContent();

    CWnd *pListItemLabel = GetDlgItem(IDC_LIST_ITEM_LABEL);

    BOOL bEnableStartNumber = FALSE;
    BOOL bEnableItemList = FALSE;

    if ( m_iContainerStyle == ED_LIST ){
        if( m_iListStyle == ED_UNUM_LIST ||
            m_iListStyle == ED_NUM_LIST ){

            pListItemStyles->EnableWindow(TRUE);
            pListItemStyles->AddString(szLoadString(IDS_AUTOMATIC));
            bEnableItemList = TRUE;

            if( m_iListStyle == ED_UNUM_LIST ){
                // Be sure we are at least on the first item
                m_iBulletStyle = max(0,m_iBulletStyle);
                pListItemStyles->AddString(szLoadString(IDS_SOLID_CIRCLE));
                pListItemStyles->AddString(szLoadString(IDS_OPEN_CIRCLE));
                pListItemStyles->AddString(szLoadString(IDS_SOLID_SQUARE));
                pListItemStyles->SetCurSel(m_iBulletStyle);
                pListItemLabel->SetWindowText(szLoadString(IDS_BULLET_STYLE));
            } else if( m_iListStyle == ED_NUM_LIST ){
                m_iNumberStyle = max(0,m_iNumberStyle);
                pListItemStyles->AddString(szLoadString(IDS_DIGIT));
                pListItemStyles->AddString(szLoadString(IDS_BIG_ROMAN));
                pListItemStyles->AddString(szLoadString(IDS_SMALL_ROMAN));
                pListItemStyles->AddString(szLoadString(IDS_BIG_LETTERS));
                pListItemStyles->AddString(szLoadString(IDS_SMALL_LETTERS));
                pListItemStyles->SetCurSel(m_iNumberStyle);
                pListItemLabel->SetWindowText(szLoadString(IDS_NUMBER_STYLE));
                bEnableStartNumber = TRUE;
            }
        }
    }

    // Set enable depending on bullet or number styles
    pListItemStyles->EnableWindow(bEnableItemList);
    if( !bEnableItemList ) {
        // Remove label if disabled
        pListItemLabel->SetWindowText("");
    } 

    // Disable start number if not a numbered list
    GetDlgItem(IDC_LIST_START_NUMBER)->EnableWindow(bEnableStartNumber);

}

void CParagraphPage::OnSelchangeParagraphStyles() 
{
    SetModified(TRUE);
    m_iParagraphStyle = ((CComboBox*)GetDlgItem(IDC_PARAGRAPH_STYLES))->GetCurSel();

    // Selecting a list item will force
    // List container and default type: Unnumbered List
    if( m_iParagraphStyle == ED_PARA_LIST ){
        m_iContainerStyle = ED_LIST;
        if( m_iListStyle == ED_DEFAULT ){
            m_iListStyle = ED_UNUM_LIST;
        }
    } else if( m_iParagraphStyle == ED_PARA_DESC_TITLE || m_iParagraphStyle == ED_PARA_DESC_TEXT){
        m_iContainerStyle = ED_LIST;
        if( m_iListStyle == ED_DEFAULT ){
            m_iListStyle = ED_DESC_LIST;
        }
    } else if( m_iContainerStyle == ED_LIST ){
        // Remove List container
        // Note that it is left as is for BLOCK QUOTE
        m_iContainerStyle = ED_NO_CONTAINER;
    }
    UpdateLists();
}

void CParagraphPage::OnSelchangeContainerStyle() 
{
    SetModified(TRUE);
    m_iContainerStyle = ((CComboBox*)GetDlgItem(IDC_CONTAINER_STYLES))->GetCurSel();

    // Force List paragraph style
    if( m_iContainerStyle == ED_LIST ){
        m_iParagraphStyle = ED_PARA_LIST;

        if( m_iListStyle == ED_DEFAULT ){
            m_iListStyle = ED_UNUM_LIST;
        }
    }
    UpdateLists();
}

void CParagraphPage::OnSelchangeListStyles() 
{
    SetModified(TRUE);
    m_iListStyle = ((CComboBox*)GetDlgItem(IDC_LIST_STYLES))->GetCurSel();
    // Reset Bullet/Numbering styles 
    //  depending on new List Style
    UpdateLists();
}

void CParagraphPage::OnSelchangeListItemStyle() 
{
    SetModified(TRUE);
    m_iListStyle = ((CComboBox*)GetDlgItem(IDC_LIST_STYLES))->GetCurSel();
    int iListItemStyle = ((CComboBox*)GetDlgItem(IDC_LIST_STYLES))->GetCurSel();

    // Just record index in proper list item category
    if( m_iListStyle == ED_UNUM_LIST){
        m_iBulletStyle = iListItemStyle;
    } else if( m_iListStyle == ED_NUM_LIST){
        m_iNumberStyle = iListItemStyle;
    }
}

void CParagraphPage::OnChangeListStartNumber() 
{
    SetModified(TRUE);
}

void CParagraphPage::OnAlignCenter() 
{
    m_Align = ED_ALIGN_ABSCENTER; // ED_ALIGN_CENTER;
    SetModified(TRUE);
}

void CParagraphPage::OnAlignLeft() 
{
    m_Align = ED_ALIGN_LEFT;
    SetModified(TRUE);
}

void CParagraphPage::OnAlignRight() 
{
    m_Align = ED_ALIGN_RIGHT;
    SetModified(TRUE);
}

void CParagraphPage::OnClose() 
{
    // We can't release data in OnOK 
    //  since we may stay around after
    //  it is called for "Apply" function
    if( m_pListData ){
        EDT_FreeListData(m_pListData);
    }
	CNetscapePropertyPage::OnClose();
}

//////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CColorButton, CButton)

CColorButton::CColorButton(COLORREF * pColorRef, COLORREF * pSetFocusColor) :
    m_pColorRef(pColorRef),
    m_pSetFocusColor(pSetFocusColor),
    m_hPal(0),
    m_bColorSwatchMode(0),
    m_pToolTip(0)
{
}

CColorButton::~CColorButton()
{
    if( m_pToolTip ){
        delete m_pToolTip;
    }
}

BEGIN_MESSAGE_MAP(CColorButton, CButton)
	//{{AFX_MSG_MAP(CColorButton)
    ON_WM_MOUSEMOVE()
    ON_WM_SETFOCUS()
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
#endif
END_MESSAGE_MAP()

BOOL CColorButton::PreTranslateMessage(MSG* pMsg)
{
    if( m_pToolTip && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
    {
#ifdef XP_WIN32 
                // This is needed to get around an MFC bug
                //   where tooltip is disabled after Modal Dialog is called
                m_pToolTip->Activate(TRUE);
#endif
        m_pToolTip->RelayEvent(pMsg);
    }
    return CButton::PreTranslateMessage(pMsg);
}

// 
#ifdef XP_WIN32 
// This is needed for non-CFrame owners of a CToolTipCtrl
BOOL CColorButton::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
    UINT nID =pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND)
    {
        if(::GetDlgCtrlID((HWND)nID) == 1) {
            strcpy(pTTT->szText, m_pTipText);
            return(TRUE);
        }
    }
    return(FALSE);
}
#endif

BOOL CColorButton::Subclass(CWnd * pParentWnd, UINT nID, COLORREF * pColorRef, BOOL bColorSwatchMode)
{
    if(!pParentWnd || !pColorRef){
        return FALSE;
    }
    m_pColorRef = pColorRef;
    m_bColorSwatchMode = bColorSwatchMode;

    // Attach derived class to existing control and route messages to parent dialog
	BOOL bResult = SubclassDlgItem(nID, pParentWnd);

    // Must do subclass before we can use GetParentFrame()
    m_hPal = WFE_GetUIPalette(GetParentFrame());

    AddToolTip();        

    return bResult;
}

BOOL CColorButton::Create(RECT& rect, CWnd * pParentWnd, UINT nID, COLORREF * pColorRef, BOOL bColorSwatchMode)
{
    if(!pParentWnd || !pColorRef){
        return FALSE;
    }
    m_pColorRef = pColorRef;
    m_bColorSwatchMode = bColorSwatchMode;

    BOOL bResult = CButton::Create(NULL, BS_PUSHBUTTON|BS_OWNERDRAW|WS_CHILD|WS_VISIBLE|WS_GROUP|WS_TABSTOP,
                                   rect, pParentWnd, nID);  
    // Must do Create before we can use GetParentFrame()
    m_hPal = WFE_GetUIPalette(GetParentFrame());
    
    AddToolTip();        

    return bResult;
}

void CColorButton::Update()
{
    // Redraw the button
    Invalidate(FALSE);
    
    // Update tooltip for new color
    if( m_pToolTip ){
        // Make a tip with "R=xxx G=xxx B=xxx"
        wsprintf(m_pTipText, szLoadString(IDS_COLOR_TIP_FORMAT),
                 GetRValue(*m_pColorRef),GetGValue(*m_pColorRef),GetBValue(*m_pColorRef));
        strcat(m_pTipText, szLoadString(IDS_COLOR_TIP_HTML));
        char * pEnd = m_pTipText + strlen(m_pTipText);
        sprintf(pEnd, "#%02X%02X%02X",GetRValue(*m_pColorRef),GetGValue(*m_pColorRef),GetBValue(*m_pColorRef));
        m_pToolTip->UpdateTipText(m_pTipText, this, 1);
    }    
}

void CColorButton::AddToolTip()
{
    // Add tooltip only if a real button
    // In "color swatch mode", the parent CColorPicker
    //   has 1 tooltip associated with all the button controls
    if( !m_bColorSwatchMode ){
        // Add a tooltip control
	    m_pToolTip = new CNSToolTip2;

        if(m_pToolTip && !m_pToolTip->Create(this, TTS_ALWAYSTIP) ){
            TRACE("Unable To create ToolTip\n");
            delete m_pToolTip;
            m_pToolTip = NULL;
            return;
        }
        // Lets use speedy tooltips
        m_pToolTip->SetDelayTime(200);
#ifdef XP_WIN32
        // We MUST do this for MFC tooltips
        EnableToolTips(TRUE);
#endif // WIN32
        RECT  rect;
        GetClientRect(&rect);
        m_pToolTip->AddTool(this, m_pTipText, &rect, 1);
        // Set the text to show the colors
        Update();
    } 
}

void CColorButton::OnMouseMove(UINT nFlags, CPoint point)
{
    CButton::OnMouseMove(nFlags, point);
    
    // For the "Color swatch" mode, automatically pull focus to
    //  the button with any mouse over
    if( m_bColorSwatchMode && GetFocus() != this ){
        // Set the color for the caller
        // This allows just mouse move to set the color
        //  after user presses Enter key without clicking on a color button
        //  and also lets us use OnOK() for all button clicks
        SetFocus();
    }
}

void CColorButton::OnSetFocus(CWnd* pOldWnd)
{
    // Send the color back to the caller
    if( m_pSetFocusColor ){
        *m_pSetFocusColor = *m_pColorRef;
    }
    CButton::OnSetFocus(pOldWnd);   
}

#define COLOR_DROPDOWN_WIDTH 10

void CColorButton::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    HPALETTE hOldPal = NULL;
	CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    if( m_hPal )
    {
		hOldPal = ::SelectPalette( pDC->m_hDC, m_hPal, FALSE );
    }
	BOOL bSelected = lpDIS->itemState & ODS_SELECTED;

    HDC  hDC = lpDIS->hDC;
	RECT rect = lpDIS->rcItem;
    if( !m_bColorSwatchMode )
    {
        // We can't draw on last pixels of right and bottom???
        rect.right--;
        rect.bottom--;
    }

	// Use the button face color as our background
    ::FillRect(hDC, &rect, sysInfo.m_hbrBtnFace);

    HPEN penShadow = ::CreatePen(PS_SOLID, 1, sysInfo.m_clrBtnShadow);
    HPEN penHighlight = ::CreatePen(PS_SOLID, 1, sysInfo.m_clrBtnHilite);
    HPEN penBlack = ::CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HPEN penOld = (HPEN)::SelectObject(hDC, penBlack);
    
    RECT rectColor = rect;

    if( m_bColorSwatchMode )
    {
        // Draw the depressed 3D look like Window's color picker,
        // Note that there is a built-in blank border
        //   only focus rect draws on actual window borders
        ::InflateRect(&rect, -2, -2);
        rectColor.left += 4;
        rectColor.top += 4;
        rectColor.right -= 3;
        rectColor.bottom -= 3;
    } else {
        rectColor.left += 2;
        rectColor.top += 2;
        rectColor.bottom -= 1;
        rectColor.right -= (COLOR_DROPDOWN_WIDTH + 7); 
    }
    
    // Draw depressed border for color swatch
    ::MoveToEx(hDC, rect.left+1, rect.bottom-1, NULL);
    ::LineTo(hDC, rect.left+1, rect.top+1);
    ::LineTo(hDC, rect.right, rect.top+1);

    ::SelectObject(hDC, penShadow);
    ::MoveToEx(hDC, rect.left, rect.bottom, NULL);
    ::LineTo(hDC, rect.left, rect.top);
    ::LineTo(hDC, rect.right+1, rect.top);

    ::SelectObject(hDC, penHighlight);
    ::MoveToEx(hDC, rect.left+1, rect.bottom, NULL);
    ::LineTo(hDC, rect.right, rect.bottom);
    ::LineTo(hDC, rect.right, rect.top);
    
    ::SelectObject(hDC, penBlack);    
    
    if( !m_bColorSwatchMode ) 
    {
        ::InflateRect(&rect, -2, -2);
        rect.top++;
        rect.left = rect.right - (COLOR_DROPDOWN_WIDTH + 4); 
        
        // Location of drop-down arrow 
        // Width is actually COLOR_DROPDOWN_WIDTH-4, 
        // but LineTo needs extra pixel to end where we want
        int iTWidth = COLOR_DROPDOWN_WIDTH - 3; 
        int iTLeft = rect.right - iTWidth - 4;
        int iTTop = rect.top + ((rect.bottom - rect.top)/2) - 2;

        // Offset down and right for pressed-down state
        if( lpDIS->itemState & ODS_SELECTED )
        {
            iTLeft++;
            iTTop++;
        }
        // Draw the black drop-down triangle
        while( iTWidth >= 0 )
        {
            ::MoveToEx(hDC, iTLeft, iTTop, NULL);
            ::LineTo(hDC, iTLeft + iTWidth, iTTop);
            iTTop++;
            iTLeft++;
            iTWidth -= 2;
        }

        if( lpDIS->itemState & ODS_SELECTED )
        {
            rect.right++;
            rect.bottom++;
            HBRUSH brushShadow = ::CreateSolidBrush(sysInfo.m_clrBtnShadow|0x02000000);
            ::FrameRect(hDC, &rect, brushShadow);
            ::DeleteObject(brushShadow);
        } else {
            // Draw the raised 3D button shadows
            ::MoveToEx(hDC, rect.left, rect.bottom, NULL);
            ::LineTo(hDC, rect.right, rect.bottom);
            ::LineTo(hDC, rect.right, rect.top-1);

            ::SelectObject(hDC, penShadow);
            ::MoveToEx(hDC, rect.left+1, rect.bottom-1, NULL);
            ::LineTo(hDC, rect.right-1, rect.bottom-1);
            ::LineTo(hDC, rect.right-1, rect.top);
            ::SelectObject(hDC, penHighlight);
            ::MoveToEx(hDC, rect.left, rect.bottom-1, NULL);
            ::LineTo(hDC, rect.left, rect.top);
            ::LineTo(hDC, rect.right, rect.top);
        }
    }
    // Draw the color rectangle unless we have a "no color" or "default" state
    if( *m_pColorRef != NO_COLORREF && *m_pColorRef != DEFAULT_COLORREF)
    {

        HBRUSH brushColor = ::CreateSolidBrush((*m_pColorRef)|0x02000000); // Like PALETTERGB
        // Fill a rect with supplied color
        HBRUSH brushOld = (HBRUSH)::SelectObject(hDC, brushColor);
        ::FillRect(hDC, &rectColor, brushColor);

        ::SelectObject(hDC, brushOld);
        ::DeleteObject(brushColor);
    }

    // Manually draw the focus rect
	if( lpDIS->itemState & ODS_FOCUS )
    {
        RECT rectFocus;
        if( m_bColorSwatchMode  )
        {
            rectFocus = lpDIS->rcItem;
            rect.left++;
            rect.top++;
        } else {
            rectFocus = rectColor;
            ::InflateRect(&rectFocus, -1, -1);
        }
        ::DrawFocusRect(hDC, &rectFocus);
    }

    // Cleanup
    ::SelectObject(hDC, penOld);
    ::DeleteObject(penShadow);
    ::DeleteObject(penHighlight);
    ::DeleteObject(penBlack);
    
    if(m_hPal)
    {
        ::SelectPalette( pDC->m_hDC, hOldPal, FALSE );
    }
}

/////////////////////////////////////////////////////////////////////////////
// CBitmapPushButton
// Allows toolbar-like pushbutton behavior

CBitmapPushButton::CBitmapPushButton(BOOL bNoBorder) :
    m_bDown(0),
    m_bFocus(0),
    m_bSelected(0),
    m_hPal(0),
    m_bNoBorder(bNoBorder)
{
}

int CBitmapPushButton::OnCreate(LPCREATESTRUCT lpCreateStruct )
{
    if( -1 == CBitmapButton::OnCreate(lpCreateStruct) ){
        return -1;
    }
    // Get the global default palette
    m_hPal = WFE_GetUIPalette(GetParentFrame());
    return 0;
}

CBitmapPushButton::~CBitmapPushButton()
{
}


BEGIN_MESSAGE_MAP(CBitmapPushButton, CBitmapButton)
	//{{AFX_MSG_MAP(CBitmapPushButton)
    ON_WM_CREATE()
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CBitmapPushButton::LoadBitmap(UINT nBitmapID)
{
 	// delete old bitmaps (if present)
	m_bitmap.DeleteObject();
	m_bitmapSel.DeleteObject();
	m_bitmapFocus.DeleteObject();
	m_bitmapDisabled.DeleteObject();

	if ( !m_bitmap.LoadBitmap( nBitmapID ) ) {
		return FALSE;
	}
    return TRUE;
}

void CBitmapPushButton::SetCheck(BOOL bCheck)
{
    BOOL bChanged = m_bDown != bCheck;
    m_bDown = bCheck;
    SetState(bCheck);    
    if(bChanged){
        // We need to force redraw sometimes
        Invalidate(FALSE);
    }
}

/////////////////////////////////////////////////////////////////////////////
// CBitmapPushButton message handlers
void CBitmapPushButton::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    // Save current focus state so we know what 
    //  button is pressed via keyboard in CDropdownToolbar handler
    m_bFocus = (lpDIS->itemState & ODS_FOCUS);

    // If our state is pushed down ("checked"),
    //  then always force selected bit so
    //  we don't unselect when losing focus
	if(m_bDown) {
        lpDIS->itemState |= ODS_SELECTED;
    }

	m_bSelected = lpDIS->itemState & ODS_SELECTED;

    // Draw the bitmap ourselves to do transparent overlay

    // Get the bitmap for the image
	HBITMAP hBmpImg = 0;

    if(m_bSelected && m_bitmapSel.m_hObject ){
        // HBITMAP(m_bitmapSel) doesn't work in old MFC (Win16)
        hBmpImg = (HBITMAP)m_bitmapSel.m_hObject;
    } else if( m_bFocus && m_bitmapFocus.m_hObject ) {
        hBmpImg = (HBITMAP)m_bitmapFocus.m_hObject;
    } else if ( (lpDIS->itemState & ODS_DISABLED) && m_bitmapDisabled.m_hObject ) {
        hBmpImg = (HBITMAP)m_bitmapDisabled.m_hObject;
    } else {
        // Check for no bitmap at all!
        if( !m_bitmap.m_hObject ){
            return;
        }
        hBmpImg = (HBITMAP)m_bitmap.m_hObject;
    }

    HDC  hDC = lpDIS->hDC;
	HDC  hBmpDC  = ::CreateCompatibleDC(hDC);
	RECT rect = lpDIS->rcItem;

	// Use the button face color as our background
	::FillRect(hDC, &rect, sysInfo.m_hbrBtnFace);

    // If no bitmap, we must be disabled and no image supplied - leave blank TODO: GRAY IMAGE
    // WEIRD - m_bitmapDisabled.m_hObject is not NULL
//    if( hBmpImg ){
    if( ! (lpDIS->itemState & ODS_DISABLED) ){

	    HBITMAP hOldBmp = (HBITMAP)::SelectObject(hBmpDC, hBmpImg);

        // Get this for the real bimap width and height
	    BITMAP bmp;
	    ::GetObject(hBmpImg, sizeof(bmp), &bmp);
	    
	    int x, y;
	    // Center the image within the button	
	    x = rect.left + ((rect.right - rect.left) - bmp.bmWidth) / 2;			
	    y = rect.top + ((rect.bottom - rect.top) - bmp.bmHeight) / 2;

        // If button is pushed down, shift image right and down 1 pixel,
        //  (but only if not using supplied selected bitmap)
        if(m_bSelected && m_bitmapSel.m_hObject == NULL ){
            x++;
            y++;
        }	

	    // Call the handy transparent blit function to paint the bitmap over whatever colors exist.
	    ::FEU_TransBlt( hDC, x, y , bmp.bmWidth, bmp.bmHeight,
					    hBmpDC, 0, 0, m_hPal);

	    ::SelectObject(hBmpDC, hOldBmp);
	    ::DeleteDC(hBmpDC);
    }

    // Draw the 3D shadow borders

    // Reduce to use inclusive border
    rect.right --;
    rect.bottom --;
    ::InflateRect(&rect, -1, -1);
    HPEN penShadow = ::CreatePen(PS_SOLID, 1, sysInfo.m_clrBtnShadow);
    HPEN penHighlight = ::CreatePen(PS_SOLID, 1, sysInfo.m_clrBtnHilite);
    HPEN penBlack = ::CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HPEN penOld = (HPEN)::SelectObject(hDC, penBlack);

    if( m_bSelected ){
        // Draw the depressed 3D button shadows
        //  only if bitmap wasn't supplied
        if( m_bitmapSel.m_hObject == NULL ){
            ::MoveToEx(hDC, rect.left, rect.bottom, NULL);
            ::LineTo(hDC, rect.left, rect.top);
            ::LineTo(hDC, rect.right+1, rect.top);

            ::SelectObject(hDC, penShadow);
            ::MoveToEx(hDC, rect.left+1, rect.bottom-1, NULL);
            ::LineTo(hDC, rect.left+1, rect.top+1);
            ::LineTo(hDC, rect.right, rect.top+1);

            ::SelectObject(hDC, penHighlight);
            ::LineTo(hDC, rect.right, rect.bottom);
            ::LineTo(hDC, rect.left, rect.bottom);
        }
    } else if( !m_bNoBorder ){
        // Draw the raised 3D button shadows
        ::MoveToEx(hDC, rect.left, rect.bottom, NULL);
        ::LineTo(hDC, rect.right, rect.bottom);
        ::LineTo(hDC, rect.right, rect.top-1);

        ::SelectObject(hDC, penShadow);
        ::MoveToEx(hDC, rect.left+1, rect.bottom-1, NULL);
        ::LineTo(hDC, rect.right-1, rect.bottom-1);
        ::LineTo(hDC, rect.right-1, rect.top);

        ::SelectObject(hDC, penHighlight);
        ::MoveToEx(hDC, rect.left, rect.bottom-1, NULL);
        ::LineTo(hDC, rect.left, rect.top);
        ::LineTo(hDC, rect.right, rect.top);
    }

    // Manually draw the focus rect if we don't have an image
    // This is optimized for a 1-pixel highlight bevel
    //   and 2-pixel shadow bevel
    // Note that CBitmapButton will NEVER
    //   show a distinct Selected+Focus state, 
    //   so this supplies that feature
	if( m_bFocus && m_bitmapFocus.m_hObject == NULL ){
        if( !m_bSelected ){
            // Draw the raised 3D button shadows if we are not pressing down
            ::SelectObject(hDC, penHighlight);
            ::MoveToEx(hDC, rect.left, rect.bottom-1, NULL);
            ::LineTo(hDC, rect.left, rect.top);
            ::LineTo(hDC, rect.right, rect.top);

            ::SelectObject(hDC, penShadow);
            ::MoveToEx(hDC, rect.left, rect.bottom, NULL);
            ::LineTo(hDC, rect.right, rect.bottom);
            ::LineTo(hDC, rect.right, rect.top-1);
        }
        // Draw a solid black frame just outside of 3D shadow rect
        ::InflateRect(&rect, 1, 1);
        // Stupid algorithm doesn't include actual right/bottom
        rect.bottom++;
        rect.right++;
        FrameRect(hDC, &rect, (HBRUSH)::GetStockObject(BLACK_BRUSH));
    }

    // Cleanup
    ::SelectObject(hDC, penOld);
    ::DeleteObject(penShadow);
    ::DeleteObject(penHighlight);
    ::DeleteObject(penBlack);
}

// Collection of Alignment/Size/Border controls used by 
//   Image, Java, and PlugIn dialogs
/////////////////////////////////////////////////////////////////////
CAlignControls::CAlignControls() :
    m_nIDAlign(0),
    m_EdAlign(ED_ALIGN_DEFAULT)
{
}

BOOL CAlignControls::Init(CWnd *pParent)
{
    ASSERT(pParent);
    m_pParent = pParent;

    // Load the bitmaps for our alignment buttons
    VERIFY(m_BtnAlignTop.AutoLoad(IDC_EDAL_T, pParent));
    VERIFY(m_BtnAlignCenter.AutoLoad(IDC_EDAL_C, pParent));
    VERIFY(m_BtnAlignCenterBaseline.AutoLoad(IDC_EDALCB, pParent));
    VERIFY(m_BtnAlignBottomBaseline.AutoLoad(IDC_EDAL_A, pParent));
    VERIFY(m_BtnAlignBottom.AutoLoad(IDC_EDAL_B, pParent));
    VERIFY(m_BtnAlignLeft.AutoLoad(IDC_EDAL_L, pParent));
    VERIFY(m_BtnAlignRight.AutoLoad(IDC_EDAL_R, pParent));

    // Intialize controls
    SetAlignment();
    return TRUE;
}

// Return if we really changed the align state
//  as compared to the previous state
BOOL CAlignControls::OnAlignButtonClick(UINT nID)
{
    if ( nID == 0 ) {
        nID = m_nIDAlign ? m_nIDAlign : IDC_EDAL_B;
    }
    BOOL bChanged = nID != m_nIDAlign;
    m_nIDAlign = nID;

    m_BtnAlignTop.SetCheck(m_nIDAlign == IDC_EDAL_T);
    m_BtnAlignCenter.SetCheck(m_nIDAlign == IDC_EDAL_C);
    m_BtnAlignCenterBaseline.SetCheck(m_nIDAlign == IDC_EDALCB);
    m_BtnAlignBottomBaseline.SetCheck(m_nIDAlign == IDC_EDAL_A);
    m_BtnAlignBottom.SetCheck(m_nIDAlign == IDC_EDAL_B);
    m_BtnAlignLeft.SetCheck(m_nIDAlign == IDC_EDAL_L);
    m_BtnAlignRight.SetCheck(m_nIDAlign == IDC_EDAL_R);

    if(bChanged &&
       m_pParent->IsKindOf(RUNTIME_CLASS(CPropertyPage))){
        ((CPropertyPage*)m_pParent)->SetModified(TRUE);
    }
    return bChanged;
}

ED_Alignment CAlignControls::GetAlignment()
{
    // NOTE: ED_ defines are backward for Center and Bottom
    //  we should change them in EDTTYPES.H, but too much
    //  other code depends on them being wrong!
    switch( m_nIDAlign ){
        case IDC_EDAL_T:
            m_EdAlign = ED_ALIGN_TOP;
            break;
        case IDC_EDAL_C:
            m_EdAlign = ED_ALIGN_CENTER;
//            m_EdAlign =  ED_ALIGN_ABSCENTER;
            break;
        case IDC_EDALCB:
            m_EdAlign =  ED_ALIGN_ABSCENTER;
//            m_EdAlign = ED_ALIGN_CENTER;
            break;
        case IDC_EDAL_B:
            m_EdAlign = ED_ALIGN_BOTTOM;
//            m_EdAlign = ED_ALIGN_ABSBOTTOM;
            break;
        case IDC_EDAL_L:
            m_EdAlign = ED_ALIGN_LEFT;
            break;
        case IDC_EDAL_R:
            m_EdAlign = ED_ALIGN_RIGHT;
            break;
        default:
            m_EdAlign = ED_ALIGN_BASELINE;
            break;
    }
    return m_EdAlign;
}

void CAlignControls::SetAlignment()
{
    OnAlignButtonClick(m_nIDAlign);
}


//////////////////////////////////////////////////
// Image dialog page.
// Note that we must supply Image data since we may be sharing
//   it with Href data. 
// Thus we need a flag to tell us to insert new image.
/////////////////////////////////////////////////////////////////////
CImagePage::CImagePage(CWnd* pParent, MWContext * pMWContext,
                       CEditorResourceSwitcher * pResourceSwitcher,
                       EDT_ImageData * pData, BOOL bInsert)
    : CNetscapePropertyPage(CImagePage::IDD),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher),
      m_bActivated(0),
      m_bInsert(bInsert),
      m_pData(pData)
{
    ASSERT(pMWContext);
    ASSERT(pData);

	//{{AFX_DATA_INIT(CImagePage)
	m_csImage = _T("");
	m_csLowRes = _T("");
	m_csAltText = _T("");
    m_bNoSave = 0;
    m_bSetAsBackground = 0;
	m_iHeight = 0;
	m_iWidth = 0;
	m_iHSpace = 0;
	m_iVSpace = 0;
	m_iBorder = 0;
    m_bDefaultBorder = FALSE;
	m_iHeightPixOrPercent = 0;
	m_iWidthPixOrPercent = 0;
	m_bLockAspect = 1;
	//}}AFX_DATA_INIT
	m_csHref = _T("");
    m_csImageStart = _T(""); 
    m_csLastValidImage = _T("");
//    m_csLastValidLowRes = _T("");
    m_bValidImage = FALSE;
//    m_bValidLowRes = FALSE;
    m_bImageChanged = FALSE;
    m_bOriginalButtonPressed = FALSE;
    m_bLockAspect = TRUE;
    
    wfe_GetLayoutViewSize(pMWContext, &m_iFullWidth, &m_iFullHeight);

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}

void CImagePage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CImagePage)
	DDX_Text(pDX, IDC_IMAGE_URL, m_csImage);
	DDX_Check(pDX, IDC_NO_SAVE_IMAGE, m_bNoSave);
	DDX_Check(pDX, IDC_LOCK_ASPECT, m_bLockAspect);
	DDX_Check(pDX, IDC_MAKE_IMAGE_BACKGROUND, m_bSetAsBackground);
	DDX_Text(pDX, IDC_IMAGE_HEIGHT, m_iHeight);
	DDV_MinMaxInt(pDX, m_iHeight, 0, 10000);
	DDX_Text(pDX, IDC_IMAGE_WIDTH, m_iWidth);
	DDV_MinMaxInt(pDX, m_iWidth, 0, 10000);
	DDX_Text(pDX, IDC_IMAGE_SPACE_HORIZ, m_iHSpace);
	DDV_MinMaxInt(pDX, m_iHSpace, 0, 1000);
	DDX_Text(pDX, IDC_IMAGE_SPACE_VERT, m_iVSpace);
	DDV_MinMaxInt(pDX, m_iVSpace, 0, 1000);
	DDX_Text(pDX, IDC_IMAGE_BORDER, m_iBorder);
	DDV_MinMaxInt(pDX, m_iBorder, 0, 1000);
	DDX_CBIndex(pDX, IDC_HEIGHT_PIX_OR_PERCENT, m_iHeightPixOrPercent);
	DDX_CBIndex(pDX, IDC_WIDTH_PIX_OR_PERCENT, m_iWidthPixOrPercent);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CImagePage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CImagePage)
	ON_BN_CLICKED(IDC_IMAGE_FILE, OnImageFile)
	ON_EN_CHANGE(IDC_IMAGE_URL, OnChangeImageURL)
	ON_EN_KILLFOCUS(IDC_LOWRES_URL, OnKillfocusImage)
	ON_BN_CLICKED(IDC_IMAGE_ORIGINAL_SIZE, OnImageOriginalSize)
	ON_BN_CLICKED(IDC_EDIT_IMAGE, OnEditImage)
	ON_BN_CLICKED(IDC_ALT_TEXT_LOWRES, OnAltTextLowRes)
	ON_BN_CLICKED(IDC_NO_SAVE_IMAGE, OnNoSave)
	ON_BN_CLICKED(IDC_REMOVE_ISMAP, OnRemoveIsmap)
	ON_BN_CLICKED(IDC_MAKE_IMAGE_BACKGROUND, OnSetAsBackground)
	ON_BN_CLICKED(IDC_EDAL_A, OnAlignBaseline)
	ON_BN_CLICKED(IDC_EDAL_B, OnAlignBottom)
	ON_BN_CLICKED(IDC_EDAL_C, OnAlignCenter)
	ON_BN_CLICKED(IDC_EDAL_L, OnAlignLeft)
	ON_BN_CLICKED(IDC_EDAL_R, OnAlignRight)
	ON_BN_CLICKED(IDC_EDAL_T, OnAlignTop)
	ON_BN_CLICKED(IDC_EDALCB, OnAlignCenterBaseline)
	ON_EN_CHANGE(IDC_IMAGE_HEIGHT, OnChangeHeight)
	ON_EN_CHANGE(IDC_IMAGE_WIDTH, OnChangeWidth)
	ON_CBN_SELCHANGE(IDC_HEIGHT_PIX_OR_PERCENT, OnSelchangeHeightPixOrPercent)
	ON_CBN_SELCHANGE(IDC_WIDTH_PIX_OR_PERCENT, OnSelchangeWidthPixOrPercent)
	ON_EN_CHANGE(IDC_IMAGE_SPACE_HORIZ, OnChangeSpaceHoriz)
	ON_EN_CHANGE(IDC_IMAGE_SPACE_VERT, OnChangeSpaceVert)
	ON_EN_CHANGE(IDC_IMAGE_BORDER, OnChangeBorder)
	ON_BN_CLICKED(IDC_EXTRA_HTML, OnExtraHTML)
    ON_BN_CLICKED(IDC_LOCK_ASPECT, OnLockAspect)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CImagePage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated){
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
    // Switch back to EXE's resources
    if( m_pResourceSwitcher ){
        m_pResourceSwitcher->Reset();
    }

    if ( m_pData->pSrc && XP_STRLEN(m_pData->pSrc) > 0 ){
        m_csImage = XP_STRDUP(m_pData->pSrc);
    }
    else if ( !m_bInsert ) {
        TRACE0("No Image Filename for Image Properties\n");
        return FALSE;
    }

	m_bActivated = TRUE;

    // Translate ImageData defines Align buttons index
    // NOTE: ED_ defines are backward for Center and Bottom
    //  we should change them in EDTTYPES.H, but too much
    //  other code depends on them being wrong!
    m_AlignControls.m_EdAlign = m_pData->align;
    switch ( m_pData->align ){
        case ED_ALIGN_LEFT:
            m_AlignControls.m_nIDAlign = IDC_EDAL_L;
            break;
        case ED_ALIGN_RIGHT:
            m_AlignControls.m_nIDAlign = IDC_EDAL_R;
            break;
        case ED_ALIGN_TOP:
        case ED_ALIGN_ABSTOP:
            m_AlignControls.m_nIDAlign = IDC_EDAL_T;
            break;
        case ED_ALIGN_ABSCENTER:
            m_AlignControls.m_nIDAlign = IDC_EDALCB;
            break;
        case ED_ALIGN_CENTER:
            m_AlignControls.m_nIDAlign = IDC_EDAL_C;
            break;
        case ED_ALIGN_BOTTOM:
            m_AlignControls.m_nIDAlign = IDC_EDAL_B;
            break;
        case ED_ALIGN_ABSBOTTOM:
        case ED_ALIGN_BASELINE:
        default:
            m_AlignControls.m_nIDAlign = IDC_EDAL_A;
            break;
    }

    // Intialize the common Alignment / Sizing controls;
    if ( !m_AlignControls.Init(this) ) {
        return FALSE;
    }
    // Use suplied values only if they existed,
    //  use our defaults (5 pixels) for new object
    if ( ! m_bInsert ){
        m_iVSpace = CASTINT(m_pData->iVSpace);
        m_iHSpace = CASTINT(m_pData->iHSpace);
    }
    if( m_pData->iBorder >= 0 ){
        m_iBorder = CASTINT(m_pData->iBorder);
    } else {
        // We were given the default border value of -1
        // Set flag to restore -1 if user doesn't change it,
        //  but it shows as 0 in the edit box
        m_bDefaultBorder = TRUE;
        m_iBorder = EDT_GetDefaultBorderWidth(m_pMWContext);
    }
    m_iWidth = CASTINT(m_pData->iWidth);
    m_iHeight = CASTINT(m_pData->iHeight);
    m_iHeightPixOrPercent = m_pData->bHeightPercent ? 1 : 0;
    m_iWidthPixOrPercent = m_pData->bWidthPercent ? 1 : 0;

    // We try to get the "Original" dimensions, or at least
    //  those when image was last loaded
    if( m_pData->iOriginalWidth ){
        m_iOriginalWidth = m_pData->iOriginalWidth;
        m_iOriginalHeight = m_pData->iOriginalHeight;
    } else {
        m_iOriginalWidth = m_pData->iWidth;
        m_iOriginalHeight = m_pData->iHeight;
    }
    // Avoid divide by zero
    m_iOriginalWidth = max(1, m_iOriginalWidth);
    m_iOriginalHeight = max(1, m_iOriginalHeight);

    // Fill drop-lists of units
    wfe_InitPixOrPercentCombos(this);
    
    // Controls specific to Image page:
    if ( m_pData->pLowSrc ){
        m_csLowRes = m_pData->pLowSrc;
    }
    m_csAltText = m_pData->pAlt;

    // Get possible HREF for image
    if(m_pData->pHREFData && m_pData->pHREFData->pURL){
        m_csHref = m_pData->pHREFData->pURL;
    }
    
    // Save initial image name to test
    //  before creating a link
    m_csImageStart = m_csImage;
    // Also save last valid image filenames
    m_csLastValidImage = m_csImage;
//    m_csLastValidLowRes = m_csLowRes;
    m_bImageChanged = FALSE;
//    m_bLowResChanged = FALSE;

    // Only allow removing bIsMap on images that already have it,
    //  i.e., we can't add it to raw images  (YET!)
    (GetDlgItem(IDC_REMOVE_ISMAP))->EnableWindow(m_pData->bIsMap);



    m_bNoSave = m_pData->bNoSave;

    SetLockAspectEnable();
    
    // Send data to controls
    UpdateData(FALSE);
    // Allow Apply button to be active if we are inserting a new object
    SetModified(m_bInsert);
	return(TRUE);
}

void CImagePage::OnHelp() 
{
    NetHelp(HELP_PROPS_IMAGE);
}

void CImagePage::OnOK() 
{
    //EDT_BeginBatchChanges(m_pMWContext);


    // Always set HREF data for image if struct exists, even if we didn't visit this page
    //  since the value may be changed by CLinkPage
    if( m_pData->pHREFData ){
        // If m_pData->pHREFData->pURL is NULL or empty, this clears any existing link
        EDT_SetHREFData(m_pMWContext, m_pData->pHREFData);
    }

    if(!m_bActivated ||
        // no change
        !IS_APPLY_ENABLED(this) ||
        // or error in data
        !UpdateData(TRUE) ) {
        //EDT_EndBatchChanges(m_pMWContext);
        return;
    }


    CleanupString(m_csImage);

    if ( m_csImage.IsEmpty() ){
        // No image -- do nothing 
        // WHAT IF THERE IS A LOWRES IMAGE??? ADD MESSAGEBOX?
        if ( m_bInsert ){
            return;
        }
        // TODO: delete current image here?
        // Currently, EDT_SetImageData does not check for m_pImageData = NULL;
        return;
    }
    // Validate/Relativize images
    // (Shouldn't really need this - validation is done on killfocus of edit boxes)
    if ( m_bImageChanged && !m_bValidImage ){
        if ( !wfe_ValidateImage( m_pMWContext, m_csImage ) ) {
            m_bValidImage = TRUE;
            UpdateData(FALSE);
            return;
        }
    }
    int iLastDot = m_csImage.ReverseFind('.');
    CString csExt;
    if(iLastDot > 0)
        csExt= m_csImage.Mid(iLastDot);

    //we must check to see if file is a bmp!
    if (0 == csExt.CompareNoCase(".bmp"))
    {
        char *t_outputfilename=wfe_ConvertImage(m_csImage.GetBuffer(0),(void *)this,m_pMWContext);
        if (t_outputfilename)
        {
            m_csImage=t_outputfilename;
            wfe_ValidateImage( m_pMWContext, m_csImage );
            XP_FREE(t_outputfilename);
            UpdateData(FALSE);//we need to update m_csImage!
        }
        else 
            return;
    }
    
    if( m_bSetAsBackground ){
        // Real simple - ignore all data except for image name and save
        EDT_PageData * pPageData = EDT_GetPageData(m_pMWContext);
        if( pPageData ){
            XP_FREEIF(pPageData->pBackgroundImage);
            pPageData->pBackgroundImage = XP_STRDUP((char*)LPCSTR(m_csImage));
            pPageData->bBackgroundNoSave = m_bNoSave;
            EDT_SetPageData(m_pMWContext, pPageData);
            EDT_FreePageData(pPageData);
        }
    } else {
        // Get the Alignment/Size data
        m_pData->align = m_AlignControls.GetAlignment();
        if( m_bOriginalButtonPressed ){
            // Trick backend into getting size from image,
            //   not the values edited
            m_pData->iWidth = 0;
            m_pData->iHeight = 0;
        } else {
            m_pData->iWidth = m_iWidth;
            m_pData->iHeight = m_iHeight;
        }
        m_pData->iHSpace = m_iHSpace;
        m_pData->iVSpace = m_iVSpace;
        if( m_bDefaultBorder ){
            m_pData->iBorder = -1;
        } else {
            m_pData->iBorder = m_iBorder;
        }
        m_pData->bWidthPercent = m_iWidthPixOrPercent;
        m_pData->bHeightPercent = m_iHeightPixOrPercent;
    
        // Data specific to Image:
        CleanupString(m_csLowRes);
        CleanupString(m_csAltText);

        if ( m_pData->pSrc ){
            XP_FREE(m_pData->pSrc);
        }
        m_pData->pSrc = XP_STRDUP(m_csImage);

        if ( m_pData->pLowSrc ){
            XP_FREE(m_pData->pLowSrc);
            m_pData->pLowSrc = NULL;
        }

        if ( !m_csLowRes.IsEmpty() ){
            m_pData->pLowSrc = XP_STRDUP(m_csLowRes);
        }

        // Note: deleting Alt text in editbox to remove Alt Text
        if ( m_pData->pAlt ){
            XP_FREE(m_pData->pAlt);
            m_pData->pAlt = NULL;
        }
        if ( !m_csAltText.IsEmpty() ){
            m_pData->pAlt = XP_STRDUP(m_csAltText);
        }

        m_pData->bNoSave = m_bNoSave;
        if ( m_bInsert )
            {
            EDT_InsertImage(m_pMWContext, m_pData, !m_bNoSave);
            // We insert just ONE image (on 1st "Apply" usage)
            // Thus other Apply or OK will modify newly-inserted image        
            m_bInsert = FALSE;
        }
        else {
            EDT_SetImageData(m_pMWContext, m_pData, !m_bNoSave);
        }
        //Note: ImageData and HrefData should be freed by caller
    }

    OkToClose();
    //EDT_EndBatchChanges(m_pMWContext);
    CPropertyPage::OnOK();
}

// Get and validate the Image name
// so it is up to date if we switch to the Link dialog page
BOOL CImagePage::OnKillActive()
{
    if( !UpdateData(TRUE) ){
        return FALSE;
    }
    if ( m_bImageChanged && !m_bValidImage ){
        wfe_ValidateImage( m_pMWContext, m_csImage );
    }
    if ( m_pData->pSrc ){
        XP_FREE(m_pData->pSrc);
    }
    m_pData->pSrc = XP_STRDUP(m_csImage);

    // Contrary to MFC help, this does NOT call our OnOK
    return CPropertyPage::OnKillActive();
}

// Called from the View after saving file to disk -- has new image
//   in a URL form relative to current document
void CImagePage::SetImageFileSaved(char * pImageURL, int iImageNumber )
{
    UpdateData(TRUE);

    if( iImageNumber == 1 ){
        m_csImage = pImageURL;
    } else if ( iImageNumber == 2 ) {
        m_csLowRes = pImageURL;
    }
    UpdateData(FALSE);
}

void CImagePage::OnImageFile()
{
    UpdateData(TRUE);
    char * szFilename = wfe_GetExistingImageFileName(this->m_hWnd, 
                                         szLoadString(IDS_SELECT_IMAGE), TRUE);
    if ( szFilename != NULL ){
        m_csImage = szFilename;
        // Note that we don't tell user if file is "bad" since
        //  it is difficult to validate in all cases
        wfe_ValidateImage( m_pMWContext, m_csImage );
        UpdateData(FALSE);
        XP_FREE( szFilename );
        SetModified(TRUE);
        UpdateData(FALSE);
        m_bValidImage = TRUE;
        m_csLastValidImage = m_csImage;
    }
}

void CImagePage::OnChangeImageURL()
{
    m_bImageChanged = TRUE;
    m_bValidImage = FALSE;
    SetModified(TRUE);
    // Disable Edit button if no image name
    GetDlgItem(IDC_IMAGE_URL)->GetWindowText(m_csImage);
    m_csImage.TrimLeft();
    m_csImage.TrimRight();
    GetDlgItem(IDC_EDIT_IMAGE)->EnableWindow(!m_csImage.IsEmpty());
}

void CImagePage::OnKillfocusImage() 
{
    if( m_bImageChanged &&
	    UpdateData(TRUE) ){
        wfe_ValidateImage( m_pMWContext, m_csImage );
        m_bValidImage = TRUE;
        UpdateData(FALSE);
    }
}

void CImagePage::OnEditImage() 
{
   UpdateData(TRUE);
    // Get our view from the context and call edit method
   ((CNetscapeEditView*)WINCX(m_pMWContext)->GetView())->EditImage((char*)LPCSTR(m_csImage));
}

void CImagePage::SetLockAspectEnable()
{
    GetDlgItem(IDC_LOCK_ASPECT)->EnableWindow(!m_bSetAsBackground && !m_iHeightPixOrPercent && !m_iWidthPixOrPercent);
}

void CImagePage::OnSetAsBackground()
{
    m_bSetAsBackground = ((CButton*)GetDlgItem(IDC_MAKE_IMAGE_BACKGROUND))->GetCheck();
    // Enable or Disable all other controls - irrelevant when simply setting background
    GetDlgItem(IDC_EDAL_T)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_EDAL_C)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_EDALCB)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_EDAL_A)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_EDAL_B)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_EDAL_L)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_EDAL_R)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_IMAGE_HEIGHT)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_HEIGHT_PIX_OR_PERCENT)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_IMAGE_WIDTH)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_WIDTH_PIX_OR_PERCENT)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_IMAGE_ORIGINAL_SIZE)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_IMAGE_SPACE_HORIZ)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_IMAGE_SPACE_VERT)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_IMAGE_BORDER)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_ALT_TEXT_LOWRES)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_EXTRA_HTML)->EnableWindow(!m_bSetAsBackground);
    GetDlgItem(IDC_REMOVE_ISMAP)->EnableWindow(!m_bSetAsBackground && m_pData->bIsMap);
    SetLockAspectEnable();
    SetModified(TRUE);
}

void CImagePage::OnAltTextLowRes()
{
    CImageAltDlg dlg(this, m_pMWContext, m_csAltText, m_csLowRes);
    if( dlg.DoModal() ){
        // Get new strings only if they changed
        if( dlg.m_csAltText != m_csAltText ){
            SetModified(TRUE);
            m_csAltText  = dlg.m_csAltText;
        }
        if( dlg.m_csLowRes != m_csLowRes ){
            SetModified(TRUE);
            m_csLowRes = dlg.m_csLowRes;
        }
    }
}

void CImagePage::OnNoSave() 
{
    SetModified(TRUE);
}

void CImagePage::OnRemoveIsmap() 
{
    m_pData->bIsMap = FALSE;
    // Once removed, we can't add it back
    (GetDlgItem(IDC_REMOVE_ISMAP))->EnableWindow(FALSE);
    SetModified(TRUE);
}

// Align/Size controls:
void CImagePage::OnAlignBaseline() 
{
    m_AlignControls.OnAlignButtonClick(IDC_EDAL_A);
}

void CImagePage::OnAlignBottom() 
{
    m_AlignControls.OnAlignButtonClick(IDC_EDAL_B);
}

void CImagePage::OnAlignCenter() 
{
    m_AlignControls.OnAlignButtonClick(IDC_EDAL_C);
}

void CImagePage::OnAlignLeft() 
{
    m_AlignControls.OnAlignButtonClick(IDC_EDAL_L);
}

void CImagePage::OnAlignRight() 
{
    m_AlignControls.OnAlignButtonClick(IDC_EDAL_R);
}

void CImagePage::OnAlignTop() 
{
    m_AlignControls.OnAlignButtonClick(IDC_EDAL_T);
}

void CImagePage::OnAlignCenterBaseline() 
{
    m_AlignControls.OnAlignButtonClick(IDC_EDALCB);
}


void CImagePage::OnImageOriginalSize() 
{
    // Set flag so we're sure user wants this later
    m_bOriginalButtonPressed = TRUE;
    
    UpdateData(TRUE);
    
    m_iWidth = CASTINT(m_iOriginalWidth);
    m_iHeight = CASTINT(m_iOriginalHeight);

    // We must be in pixel mode, not % mode
    m_iHeightPixOrPercent = 0;
    m_iWidthPixOrPercent = 0;

    UpdateData(FALSE);
    SetModified(TRUE);
}

void CImagePage::OnChangeHeight() 
{
    if( m_bLockAspect && ((CButton*)GetDlgItem(IDC_LOCK_ASPECT))->IsWindowEnabled() ) {
        // Get value just enterred and set the opposite
        //  to a value that keeps aspect ratio of original
        CWnd *pHeightEdit = GetDlgItem(IDC_IMAGE_HEIGHT);
        CWnd *pWidthEdit = GetDlgItem(IDC_IMAGE_WIDTH);
        char  pValue[16];
        char* pEnd;
        pHeightEdit->GetWindowText(pValue, 10);
        int32 iHeight = (int)strtol( pValue, &pEnd, 10 );
    
        // Bad conversion if end pointer isn't at terminal null;
        if( *pEnd == '\0' ){
            m_iHeight = iHeight;
            // Add 0.5 to round off when converting back to int
            m_iWidth = (int)((iHeight * m_iOriginalWidth) / m_iOriginalHeight);
            wsprintf(pValue, "%d", m_iWidth);
            // Avoid bouncing back and forth (and killing stack!)
            // SetWindowText triggers OnChangeWidth
            m_bLockAspect = FALSE;
            pWidthEdit->SetWindowText(pValue);
            m_bLockAspect = TRUE;
        }
    }
    if( m_iHeight != m_iOriginalHeight){
        m_bOriginalButtonPressed = FALSE;
    }
    SetModified(TRUE);
}

void CImagePage::OnChangeWidth() 
{
    if( m_bLockAspect && ((CButton*)GetDlgItem(IDC_LOCK_ASPECT))->IsWindowEnabled() ){
        // Get value just enterred and set the opposite
        //  to a value that keeps aspect ratio of original
        CWnd *pWidthEdit = GetDlgItem(IDC_IMAGE_WIDTH);
        CWnd *pHeightEdit = GetDlgItem(IDC_IMAGE_HEIGHT);
        char  pValue[16];
        char* pEnd;
        pWidthEdit->GetWindowText(pValue, 10);
        int32 iWidth = (int32)strtol( pValue, &pEnd, 10 );
    
        if( *pEnd == '\0' ){
            m_iWidth = iWidth;
            m_iHeight = (int)((iWidth * m_iOriginalHeight) / m_iOriginalWidth);
            wsprintf(pValue, "%d", m_iHeight);

            // Avoid bouncing back and forth (and killing stack!)
            // SetWindowText triggers OnChangeHeight
            m_bLockAspect = FALSE;
            pHeightEdit->SetWindowText(pValue);
            m_bLockAspect = TRUE;
        }
    }
    if( m_iWidth != m_iOriginalWidth){
        m_bOriginalButtonPressed = FALSE;
    }
    SetModified(TRUE);
}

void CImagePage::OnSelchangeHeightPixOrPercent() 
{
    UpdateData();
    SetLockAspectEnable();
    //TODO: do number conversion if switching state?
    SetModified(TRUE);
}

void CImagePage::OnSelchangeWidthPixOrPercent() 
{
    UpdateData();
    SetLockAspectEnable();
    //TODO: do number conversion if switching state?
    SetModified(TRUE);
}

void CImagePage::OnChangeSpaceHoriz() 
{
    SetModified(TRUE);
}

void CImagePage::OnChangeSpaceVert() 
{
    SetModified(TRUE);
}


void CImagePage::OnChangeBorder() 
{
    SetModified(TRUE);
    // If user changed the border,
    //  then use that number instead
    //  of the default -1
    m_bDefaultBorder = FALSE;
}

void CImagePage::OnExtraHTML()
{
    CExtraHTMLDlg dlg(this, &m_pData->pExtra, IDS_IMG_TAG);
    if( dlg.DoModal() && dlg.m_bDataChanged ){
        SetModified(TRUE);
    }
}

void CImagePage::OnLockAspect()
{
    m_bLockAspect = ((CButton*)GetDlgItem(IDC_LOCK_ASPECT))->GetCheck();
}

/////////////////////////////////////////////////////////////////////////////
// CAltImageDlg dialog (modal popup over CImagePage for Alt text and Lowres Image)

CImageAltDlg::CImageAltDlg(CWnd *pParent,
                         MWContext *pMWContext,
                         CString& csAltText, 
                         CString& csLowRes )
	: CDialog(CImageAltDlg::IDD, pParent),
    m_pMWContext(pMWContext),
    m_bImageChanged(FALSE)
{
	//{{AFX_DATA_INIT(CImageAltDlg)
	m_csAltText = csAltText;
	m_csLowRes = csLowRes;
 	//}}AFX_DATA_INIT
    ASSERT( pMWContext );
}

CImageAltDlg::~CImageAltDlg()
{
}

void CImageAltDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CImageAltDlg)
	DDX_Text(pDX, IDC_LOWRES_URL, m_csLowRes);
	DDX_Text(pDX, IDC_IMAGE_ALT_TEXT, m_csAltText);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CImageAltDlg, CDialog)
	//{{AFX_MSG_MAP(CImageAltDlg)
	ON_BN_CLICKED(IDC_IMAGE_FILE, OnLowResFile)
	ON_EN_CHANGE(IDC_LOWRES_URL, OnChangeLowResURL)
	ON_BN_CLICKED(IDC_EDIT_IMAGE, OnEditImage)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()


BOOL CImageAltDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

 	CDialog::OnInitDialog();

    GetDlgItem(IDC_EDIT_IMAGE)->EnableWindow(!m_csLowRes.IsEmpty());

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CImageAltDlg::OnHelp() 
{
    NetHelp(HELP_PROPS_IMAGE);    // TODO: ADD THIS TO HELP  
}

#ifdef XP_WIN32
BOOL CImageAltDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32

void CImageAltDlg::OnOK() 
{
    CDialog::OnOK();    
    // Strip off leading and ending spaces
    m_csAltText.TrimLeft();
    m_csAltText.TrimRight();
    m_csLowRes.TrimLeft();
    m_csLowRes.TrimRight();
    if( !m_bImageChanged ){
        wfe_ValidateImage( m_pMWContext, m_csLowRes );
    }
}

void CImageAltDlg::OnLowResFile() 
{
    UpdateData(TRUE);
    char * szFilename = wfe_GetExistingImageFileName(this->m_hWnd, 
                                         szLoadString(IDS_SELECT_LOWRES_IMAGE), TRUE);
    if ( szFilename == NULL ){
        return;
    }

    m_csLowRes = szFilename;
    wfe_ValidateImage( m_pMWContext, m_csLowRes );
    m_bImageChanged = TRUE;
    UpdateData(FALSE);
    XP_FREE( szFilename );
}

void CImageAltDlg::OnChangeLowResURL()
{
    m_bImageChanged = FALSE;
    // Disable Edit button if no image name
    GetDlgItem(IDC_LOWRES_URL)->GetWindowText(m_csLowRes);
    m_csLowRes.TrimLeft();
    m_csLowRes.TrimRight();
    GetDlgItem(IDC_EDIT_IMAGE)->EnableWindow(!m_csLowRes.IsEmpty());
}

void CImageAltDlg::OnEditImage() 
{
   UpdateData(TRUE);
    // Get our view from the context and call edit method
   ((CNetscapeEditView*)WINCX(m_pMWContext)->GetView())->EditImage((char*)LPCSTR(m_csLowRes));
}

/////////////////////////////////////////////////////////////////////////////
// CExtraHTMLDlg dialog (modal popup over CImagePage or CLinkPage for Extra HTML

CExtraHTMLDlg::CExtraHTMLDlg(CWnd *pParent, char **ppExtraHTML, UINT nIDTagType)
	: CDialog(CExtraHTMLDlg::IDD, pParent),
    m_ppExtraHTML(ppExtraHTML),
    m_bDataChanged(FALSE),
    m_nIDTagType(nIDTagType)
{
    ASSERT( ppExtraHTML );
	//{{AFX_DATA_INIT(CExtraHTMLDlg)
	m_csExtraHTML = *ppExtraHTML;
 	//}}AFX_DATA_INIT
}

CExtraHTMLDlg::~CExtraHTMLDlg()
{
}

void CExtraHTMLDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CExtraHTMLDlg)
	DDX_Text(pDX, IDC_EXTRA_HTML_TEXT, m_csExtraHTML);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CExtraHTMLDlg, CDialog)
	//{{AFX_MSG_MAP(CExtraHTMLDlg)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()


BOOL CExtraHTMLDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

 	CDialog::OnInitDialog();

    // Insert the text describing the tag type into the message and display
    CString csMsg;
    AfxFormatString1( csMsg, IDS_EXTRA_HTML_MSG, szLoadString(m_nIDTagType) );
    GetDlgItem(IDC_EXTRA_HTML_MSG)->SetWindowText(LPCSTR(csMsg));

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CExtraHTMLDlg::OnHelp() 
{
    NetHelp(HELP_EXTRA_HTML);
}

#ifdef XP_WIN32
BOOL CExtraHTMLDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32

void CExtraHTMLDlg::OnOK() 
{
    CDialog::OnOK();    
    // Strip off leading and ending spaces
    m_csExtraHTML.TrimLeft();
    m_csExtraHTML.TrimRight();

    // Did we already have some data?
    BOOL bHadExtra = 0 != *m_ppExtraHTML;

    if( !bHadExtra && m_csExtraHTML.IsEmpty() ){
        // We didn't have any before and none now, so we're done
        return;
    }

    if( bHadExtra && 0 == XP_STRCMP(*m_ppExtraHTML, LPCSTR(m_csExtraHTML)) ){
        // We had data before and it didn't change so we're done
        return;
    }

    // If here, new data must be different than previous text
    m_bDataChanged = TRUE;
    if( bHadExtra ) XP_FREE(*m_ppExtraHTML);

    if(m_csExtraHTML.IsEmpty() ){
        // No new text
        *m_ppExtraHTML = NULL;
    } else {
        // Copy new text
        *m_ppExtraHTML = XP_STRDUP(LPCSTR(m_csExtraHTML));
    }
}

///////////////////////////////////////////////////////////////
// Links dialog page.
// Note that we must supply Href data since we may be sharing
//   it with Image data. If link is an image, *ppImage has name.
// Thus we need a flag to tell us to insert new link.
/////////////////////////////////////////////////////////////////////
CLinkPage::CLinkPage(CWnd* pParent, MWContext * pMWContext,
                     CEditorResourceSwitcher * pResourceSwitcher,
                     EDT_HREFData *pData, BOOL bInsert,
                     BOOL bMayHaveOtherLinks, char **ppImage)
    : CNetscapePropertyPage(CLinkPage::IDD),
      m_bActivated(0),
      m_bInsert(bInsert),
      m_pMWContext(pMWContext),
      m_pResourceSwitcher(pResourceSwitcher),
      m_bMayHaveOtherLinks(bMayHaveOtherLinks),
      m_ppImage(ppImage),
      m_pData(pData),
      m_iTargetCount(0),
      m_bValidHref(0),
      m_bHrefChanged(0),
      m_iCaretMovedBack(0)
{
    //}}AFX_DATA_INIT
    ASSERT(pMWContext);
    ASSERT(pData);

    m_szBaseDocument = NULL;

    // Base URL is the address of current document
    History_entry * hist_ent = SHIST_GetCurrent(&(m_pMWContext->hist));
    if ( hist_ent ){
        m_szBaseDocument = hist_ent->address;
    }

    // We will use some helper functions from our view
    m_pView = (CNetscapeView*)WINCX(pMWContext)->GetView();

    //{{AFX_DATA_INIT(CLinkPage)
	m_csHref = _T("");
	m_csAnchorEdit = _T("");
	m_csAnchor = _T("");
	//}}AFX_DATA_INIT

    m_csLastValidHref = _T("");

#ifdef XP_WIN32
    // Set the hInstance so we get template from editor's resource DLL
    m_psp.hInstance = AfxGetResourceHandle();
#endif
}

CLinkPage::~CLinkPage()
{
    // Reposition the caret to where it was
    //  when new link text was inserted
    while( m_iCaretMovedBack ){
        EDT_NextChar(m_pMWContext, FALSE);
        m_iCaretMovedBack--;
    }

}

void CLinkPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLinkPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	DDX_Text(pDX, IDC_HREF_URL, m_csHref);
	DDX_Text(pDX, IDC_ANCHOR_EDIT, m_csAnchorEdit);
	DDX_Text(pDX, IDC_ANCHOR, m_csAnchor);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CLinkPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CLinkPage)
	ON_BN_CLICKED(IDC_HREF_FILE, OnHrefFile)
	ON_BN_CLICKED(IDC_HREF_UNLINK, OnHrefUnlink)
	ON_LBN_SELCHANGE(IDC_TARGET_LIST, OnSelchangeTargetList)
	ON_EN_CHANGE(IDC_HREF_URL, OnChangeHrefUrl)
	ON_EN_KILLFOCUS(IDC_HREF_URL, OnKillfocusHrefUrl)
	ON_BN_CLICKED(IDC_TARGETS_IN_CURRENT_DOC, OnTargetsInCurrentDoc)
	ON_BN_CLICKED(IDC_TARGETS_IN_FILE, OnTargetsInFile)
	ON_BN_CLICKED(IDC_EXTRA_HTML, OnExtraHTML)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CLinkPage::OnSetActive() 
{
	if(m_pResourceSwitcher && !m_bActivated){
		// We must be sure we have switched
		//  the first time here - before dialog creation
		m_pResourceSwitcher->switchResources();
	}
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    BOOL bImageAnchor = FALSE;

    // Always check the Imagename that might
    //  be changed in the Image property page
    if( m_ppImage && *m_ppImage ){
        m_csAnchor = *m_ppImage;
        if(!m_csAnchor.IsEmpty()){
            ((CEdit*)GetDlgItem(IDC_ANCHOR))->SetWindowText(m_csAnchor);
            bImageAnchor = TRUE;
        }
    }

    if(m_bActivated)
        return(TRUE);

    // Switch back to EXE's resources
    if( m_pResourceSwitcher ){
        m_pResourceSwitcher->Reset();
    }

	m_bActivated = TRUE;

    // Get the current URL
    m_csHref = m_pData->pURL;

    //TODO: GET CURRENT CHARACTER STATE OR PASS IN?


    // Fill the "Display text" editbox
    //  with selected text 	
    UINT nIDLabel = 0;
    BOOL bSelected = EDT_IsSelected(m_pMWContext);

    if (bImageAnchor){
        // Change label for image
        // (Note: We set Image filename string above)
        nIDLabel = IDS_ANCHOR_IMAGE;
    } else {
        if(m_bInsert && !bSelected ){
            // We will create/insert a new Anchor + Href at caret location
            // Use editbox instead of text control
            ((CEdit*)GetDlgItem(IDC_ANCHOR))->ShowWindow(SW_HIDE);
            nIDLabel = IDS_NEW_ANCHOR_TEXT;
        } 
        else if( EDT_CanSetHREF(m_pMWContext) ) {
            // We have a text link or selected text
            if ( bSelected ) {
                m_csAnchor = (char*)LO_GetSelectionText(ABSTRACTCX(m_pMWContext)->GetDocumentContext());
            } else {
                // We are not selected, but are within a text anchor
                char * pText = EDT_GetHREFText(m_pMWContext);
                m_csAnchor = pText;
                XP_FREE(pText);
            }
            // Replace CR/LF with spaces to avoid ugly break in static display
            for( int i=0; i < m_csAnchor.GetLength(); i++ ){
                if( m_csAnchor.GetAt(i) == '\r' || m_csAnchor.GetAt(i) == '\n' ){
                    m_csAnchor.SetAt(i, ' ');
                }
            }
        }
    }
    if( nIDLabel ) {
        // Set label above image name or new text editbox
        GetDlgItem(IDC_ANCHOR_LABEL)->SetWindowText(szLoadString(nIDLabel));
    }

    if( nIDLabel != IDS_NEW_ANCHOR_TEXT ) {
        // Make the existing Anchor Text or Image Filename BOLD by using System Font
//    	if( INTL_CharSetType(m_pMWContext->doc_csid) == SINGLEBYTE*/) {
            // This should be OK even in foreign systems
            CFont* pfontBold = CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FONT));
            ((CEdit*)GetDlgItem(IDC_ANCHOR))->SetFont(pfontBold);
//        }
        // No editable anchor, so hide editbox
        (GetDlgItem(IDC_ANCHOR_EDIT))->ShowWindow(SW_HIDE);
    }

    // Save initial HREF to restore string
    //   if user changes to something invalid
    m_csLastValidHref = m_csHref;

    // Init unlink button state. Note that button shows
    //   if we may have other links within selection that we can remove
    (GetDlgItem(IDC_HREF_UNLINK))->EnableWindow(!m_csHref.IsEmpty() || m_bMayHaveOtherLinks);

    // Get the list of Targets (named anchors) in current doc
    m_pTargetList = EDT_GetAllDocumentTargets(m_pMWContext);
    
    if( m_pTargetList == NULL ){
        // No targets in current doc, so disable button
        ((CButton*)GetDlgItem(IDC_TARGETS_IN_CURRENT_DOC))->EnableWindow(FALSE);
    }

    // Try to generate a Targets list for current HREF (if local file),
    //  if none found, show the local list if it exists
    if ( 0 == GetTargetsInFile() && m_pTargetList ){
        if ( 0 == GetTargetsInDoc() ){
            (GetDlgItem(IDC_TARGET_LABEL))->SetWindowText(szLoadString(IDS_NO_TARGETS));
        }
    }
    if( m_bMayHaveOtherLinks ){
        (GetDlgItem(IDC_HREF_UNLINK))->SetWindowText(szLoadString(IDS_REMOVE_LINKS));        
    }

    // Send data to controls
    UpdateData(FALSE);
    // Allow Apply button to be active if we are inserting a new object
    SetModified(m_bInsert);
	return(TRUE);
}

// Set the HREF data 
// It will be used by Image property page 
//   to set HREF to an image
BOOL CLinkPage::OnKillActive()
{
    if( !UpdateData() ){
        return FALSE;
    }
    if ( !m_bValidHref ){
        ValidateHref();
    }
    return CPropertyPage::OnKillActive();
}

int nTargetIndex = -1;

int CLinkPage::GetTargetsInDoc()
{
    m_iTargetCount = 0;
    CListBox * pListBox = (CListBox*)GetDlgItem(IDC_TARGET_LIST);
	pListBox->ResetContent();

    if( m_pTargetList ){
        char * pString = m_pTargetList;
        int iLen;    
    	while( (iLen = XP_STRLEN(pString)) > 0 ) {
    		// add to the end of the list
    		pListBox->AddString(pString);
            pString += iLen+1;
            m_iTargetCount++;
        }
        ((CButton*)GetDlgItem(IDC_TARGETS_IN_CURRENT_DOC))->SetCheck(1/*m_iTargetCount ? 1 : 0*/);
        ((CButton*)GetDlgItem(IDC_TARGETS_IN_FILE))->SetCheck(0);

        if( m_iTargetCount ){
            (GetDlgItem(IDC_TARGET_LABEL))->SetWindowText(szLoadString(IDS_TARGETS_IN_CURRENT_DOC));
        }
    }

    return m_iTargetCount;
}

// File read from HTML to get list of anchors
int CLinkPage::GetTargetsInFile()
{
    if( m_csHref.IsEmpty() ){
        return 0;
    }
    m_iTargetCount = 0;

    // Build list from file
    char * pTargetList = EDT_GetAllDocumentTargetsInFile(
                                    m_pMWContext, (char*)LPCSTR(m_csHref));
    if( pTargetList ){
    	CListBox * pListBox = (CListBox*)GetDlgItem(IDC_TARGET_LIST);
        pListBox->ResetContent();
        char * pString = pTargetList;
        int iLen;    
    	while( (iLen = XP_STRLEN(pString)) > 0 ) {
    		// add to the end of the list
    		pListBox->AddString(pString);
            pString += iLen+1;
            m_iTargetCount++;
        }

        XP_FREE(pTargetList);
        // Save filename so we can avoid rereading file needlessly
        m_csTargetFile = m_csHref;
    } else {
        // HREF wasn't a file or
        //  was same as current
        // TODO: How can we know if current URL is same as HREF?
        ((CButton*)GetDlgItem(IDC_TARGETS_IN_FILE))->SetCheck(0);
        return 0;
    }

    ((CButton*)GetDlgItem(IDC_TARGETS_IN_CURRENT_DOC))->SetCheck(0);
    ((CButton*)GetDlgItem(IDC_TARGETS_IN_FILE))->SetCheck( m_iTargetCount ? 1 : 0);
        
    // Set appropriate message above targets list
    if ( m_iTargetCount ){
        (GetDlgItem(IDC_TARGET_LABEL))->SetWindowText(szLoadString(IDS_TARGETS_IN_FILE));
    } else {
        (GetDlgItem(IDC_TARGET_LABEL))->SetWindowText(szLoadString(IDS_NO_TARGETS));
    }
    return m_iTargetCount;
}


void CLinkPage::OnTargetsInCurrentDoc() 
{
    if( m_pTargetList ){
        GetTargetsInDoc();
    }
}

void CLinkPage::OnTargetsInFile() 
{
    UpdateData();
    GetTargetsInFile();
}

void CLinkPage::OnSelchangeTargetList() 
{
    UpdateData(TRUE);
    SetModified(TRUE);
    m_bValidHref = FALSE;

    // Copy selection text
    int nSel = ((CListBox*)GetDlgItem(IDC_TARGET_LIST))->GetCurSel();

    nTargetIndex = nSel;
    if( ((CButton*)GetDlgItem(IDC_TARGETS_IN_CURRENT_DOC))->GetCheck() ){
        // For current doc, we need just the target
        m_csHref = '#';
    } else {
        // For file, append target to current HREF,
        // but strip off existing target first
        int iHash = m_csHref.Find('#');
        if( iHash>=0 ){
            m_csHref = m_csHref.Left(iHash+1);
        } else {
            m_csHref += '#';
        }
    }
    CString csTarget;
    ((CListBox*)GetDlgItem(IDC_TARGET_LIST))->GetText(nSel, csTarget);
    m_csHref += csTarget;
    (GetDlgItem(IDC_HREF_UNLINK))->EnableWindow(TRUE);
    UpdateData(FALSE);
}

void CLinkPage::OnHrefFile() 
{
    // Get data from dialog
    if ( !UpdateData(TRUE) ){
        return;
    }
    // Note that the file filter is set to *.html,
    //   but all other file types are also available
    CString csFile = wfe_GetExistingFileName(this->m_hWnd,
                       szLoadString(IDS_LINK_TO_FILE), HTM, TRUE);
    if( !csFile.IsEmpty() ) {
        WFE_ConvertFile2Url(m_csHref, csFile);
        // Convert to a relative URL and validate
        ValidateHref();
        UpdateData(FALSE);
        SetModified(TRUE);
        // Try to get list of targets from local file
        OnTargetsInFile();
    }
}

void CLinkPage::OnHrefUnlink() 
{
    SetModified(TRUE);

    // Clear link from our member variable and 
    //   the passed-in data, and the edit box
    m_csHref = "";
    if(m_pData->pURL){
        XP_FREE(m_pData->pURL);
        m_pData->pURL = NULL;
    }
    ((CEdit *)GetDlgItem(IDC_HREF_URL))->SetWindowText("");

    ED_ElementType type = EDT_GetCurrentElementType(m_pMWContext);
    if( (m_bMayHaveOtherLinks && type == ED_ELEMENT_SELECTION /*|| 
         (type == ED_ELEMENT_SELECTION && EDT_CanSetHREF(m_pMWContext)) */) &&
             IDYES == MessageBox(szLoadString(IDS_REMOVE_OTHER_LINKS),
                               szLoadString(IDS_REMOVE_LINKS_CAPTION),
                               MB_ICONQUESTION | MB_YESNO) ){
        // Remove just the HREF immediately using masked bits        
        EDT_CharacterData *pCharData = EDT_NewCharacterData();
        if( pCharData ){
            pCharData->mask = TF_HREF;
            // "New" should set all values = 0;
            EDT_SetCharacterData(m_pMWContext, pCharData);
            EDT_FreeCharacterData(pCharData);
            // We now have no links to remove
            m_bMayHaveOtherLinks = FALSE;
            // and our action is similar to Apply button
            //  in that we commited an action
            OkToClose();
            (GetDlgItem(IDC_HREF_UNLINK))->SetWindowText(szLoadString(IDS_REMOVE_LINK));
        }
    }
    (GetDlgItem(IDC_HREF_UNLINK))->EnableWindow(FALSE);
}

void CLinkPage::OnChangeHrefUrl() 
{
    SetModified(TRUE);
    m_bHrefChanged = TRUE;
    m_bValidHref = FALSE;
    // We only need HREF, so don't bother with UpdateData()
    CString csHREF;
    GetDlgItem(IDC_HREF_URL)->GetWindowText(csHREF);
    CleanupString(csHREF);

    if( csHREF.IsEmpty() ){
        // No URL, seems safe to redisplay the current list
        GetTargetsInDoc();
    } else if( (m_iTargetCount &&
                ((CButton*)GetDlgItem(IDC_TARGETS_IN_CURRENT_DOC))->GetCheck()) ||
               (((CButton*)GetDlgItem(IDC_TARGETS_IN_FILE))->GetCheck() &&
                m_csHref.Find(LPCSTR(m_csTargetFile)) == -1) ){
        // We had a current target list, or URL was changed
        //   so it isn't the same as the last target file used.
        // To remove uncertainty of what the target list refers to, remove it
        m_iTargetCount = 0;
        m_csTargetFile.Empty();
    	((CListBox*)GetDlgItem(IDC_TARGET_LIST))->ResetContent();
        ((CButton*)GetDlgItem(IDC_TARGETS_IN_CURRENT_DOC))->SetCheck(0);
        ((CButton*)GetDlgItem(IDC_TARGETS_IN_FILE))->SetCheck(0);
    }
    (GetDlgItem(IDC_HREF_UNLINK))->EnableWindow(!csHREF.IsEmpty() || m_bMayHaveOtherLinks);

    // Update the common data structure with new HREF
    // Can't use SetHrefData() cause we can't change m_csHREF
    if(m_pData->pURL){
        XP_FREE(m_pData->pURL);
        m_pData->pURL = NULL;
    }
    if(!csHREF.IsEmpty()){
        m_pData->pURL = XP_STRDUP(csHREF);
    }
}

// Validate only after user leaves edit box
void CLinkPage::OnKillfocusHrefUrl() 
{
	if(m_bHrefChanged && UpdateData(TRUE)){
        ValidateHref();
    }
}

// Check for valid Href - convert to relative URL
// NOTE: Data must be read from control via DDX first
// This logic assumes that base document is local file,
// (We force saving a remote document before changing links)
void CLinkPage::ValidateHref()
{
    m_bValidHref = TRUE;
    CleanupString(m_csHref);

    // Empty is OK - this is how we remove links
    if ( m_csHref.IsEmpty() ){
        m_csLastValidHref.Empty();
        return;
    }
    // Strip off "#named_anchor" part and place in 
    //  separate string???

	XP_Bool bKeepLinks;
	PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);
    if ( FE_ResolveLinkURL(m_pMWContext, m_csHref,bKeepLinks) ) {
        // Save this as a valid reference
        m_csLastValidHref = m_csHref;
    } else {
        // Error or user rejected the Href,
        //   restore to previous value
        m_csHref = m_csLastValidHref;
        SetModified(TRUE);
        UpdateData(FALSE);
    }
    // We must always set the HREF data immediately 
    //  common data because it will be accessed by
    //  CImagePage::OnOK before CLinkPage::OnOK
    SetHrefData();
}

void CLinkPage::SetHrefData()
{
    if(m_pData->pURL){
        XP_FREE(m_pData->pURL);
        m_pData->pURL = NULL;
    }
    if(!m_csHref.IsEmpty()){
        m_pData->pURL = XP_STRDUP(m_csHref);
    }
    //TODO: Set pTarget and pMocha strings
#if 0
    // Use this if we supply UI to change target frame
    // For now, just pass through what's there?
    if(m_pData->pTarget){
        XP_FREE(m_pData->pTarget);
        m_pData->pTarget = NULL;
    }
    if(!m_csTarget.IsEmpty()){
        m_pData->pTarget = XP_STRDUP(m_csTarget);
    }
#endif
}

void CLinkPage::OnExtraHTML()
{
    CExtraHTMLDlg dlg(this, &m_pData->pExtra, IDS_HREF_TAG);
    if( dlg.DoModal() && dlg.m_bDataChanged ){
        SetModified(TRUE);
    }
}

void CLinkPage::OnHelp() 
{
    NetHelp(HELP_PROPS_LINK);
}

void CLinkPage::OnOK() 
{
    CPropertyPage::OnOK();

    // never visited this page or no change -- don't bother
    if(!m_bActivated ||
       !IS_APPLY_ENABLED(this)){
        return;
    }    

    GetDlgItem(IDC_HREF_URL)->GetWindowText(m_csHref);
    CleanupString(m_csHref);

    //EDT_BeginBatchChanges(m_pMWContext);

    CleanupString(m_csAnchorEdit);
    int nResult = 0;
    // TODO: Test nResult for valid URL and add messages to user
    //   when there are problems

    if ( m_iTargetCount ) {
        nTargetIndex = ((CListBox*)GetDlgItem(IDC_TARGET_LIST))->GetCurSel();
        //TODO: Append the "#named_anchor" to the m_csHref;
    }

    // If we have an image Anchor, the image property page
    //  will set HREF data. 
    if( ! (m_ppImage && *m_ppImage && **m_ppImage != '\0') ){
        if ( EDT_CanSetHREF(m_pMWContext) ) {
            // Associate a URL with selected text 
            //   or existing link (text or image anchor)
            // TODO: LLOYD: return error: nResult = 
            // EDT_SetHREF( m_pMWContext, szURL );
            EDT_SetHREFData(m_pMWContext, m_pData);
            // Note: This will remove a link if m_pData->pURL is NULL
        }
        else /* if(m_pData->pURL) */{
            // We created a new link - 
            // Anchor text should have beeen typed
            char * szAnchor;
            if ( m_csAnchorEdit.IsEmpty() ) {
                // No anchor text supplied, use URL
                szAnchor = m_pData->pURL;
            } else {
                // Can't do cast of LPCSTR() directly! Gives syntax error!
                const char * szStupidCompiler = LPCSTR(m_csAnchorEdit);
                szAnchor = (char*)szStupidCompiler; // (char*)(LPCSTR(m_csAnchorEdit));
            }
            if( m_bInsert && m_pData->pURL ) {
                // Insert both Anchor text and Href
                // TODO: LLOYD: return error: nResult = 
                EDT_PasteHREF( m_pMWContext, &m_pData->pURL, &szAnchor, 1 );
                // Set flag so future Apply actions will not insert another link
                m_bInsert = FALSE;
                // Move back one character so subsequent attributes Applied
                //  don't include following text.
                EDT_PreviousChar( m_pMWContext, FALSE );
                m_iCaretMovedBack++;
                
                // Get the text just inserted
                char * pNewText = EDT_GetHREFText(m_pMWContext);
                if( pNewText == NULL ){
                    // If we insert a space after text, then
                    //  the link text will be empty.
                    //  We need to move back another character
                    //  so current text element is the text, not the space,
                    //  else subsequent calls to EDT_SetHREFData will
                    //  attach URL to space and following text, not the 
                    //  newly-inserted text
                    EDT_PreviousChar( m_pMWContext, FALSE );
                    m_iCaretMovedBack++;
                } else {
                    XP_FREE(pNewText);
                }
                    
                // and set flag so we move forward upon exiting

                // It would be a pain to allow replacing inserted anchor text,
                //   so disable control after 1st insert
                (GetDlgItem(IDC_HREF_UNLINK))->EnableWindow(FALSE);
            } else {
                EDT_SetHREFData(m_pMWContext, m_pData);
            }
        }
    }

    OkToClose();
    //EDT_EndBatchChanges(m_pMWContext);
}

////////////////////////////////////////////////////////////////
/*
// **** TEMPLATE for property pages
/////////////////////////////////////////////////////////////////////
CPage::CPage(CWnd* pParent, MWContext * pMWContext,
                EDT_<Data> * pData)              // EDT data for this property
    : CNetscapePropertyPage(CPage::IDD)
{
    //{{AFX_DATA_INIT(CPage)
    //}}AFX_DATA_INIT
    ASSERT(pMWContext);
//    ASSERT(pData);

    m_bActivated = FALSE;
    m_pMWContext = pMWContext;
    EDT_<Data> * m_pData;       // EDT data for this property
}


void CPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPage, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CPage::OnSetActive() 
{
    if(!CPropertyPage::OnSetActive())
        return(FALSE);

    if(m_bActivated)
        return(TRUE);

    // TODO - Fill in controls here
	m_bActivated = TRUE;

    // Send data to controls
    UpdateData(FALSE);
	return(TRUE);
}

void CPage::OnOK() 
{

    CPropertyPage::OnOK();

    // never visited this page so don't bother
    if(!m_bActivated)
        return;

    // TODO - Get control data back into pData here
}
*/
#endif // EDITOR
