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

// edcombtb.cpp : implementation file
//
#include "stdafx.h"
#ifdef EDITOR
#include "edcombtb.h"
//#include "edres1.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#define     HIDDEN_WIDTH        1   // Overlap between buttons, thus width of "removed" item

static int iComboRightBorder = 6;

/////////////////////////////////////////////////////////////////////////////
// CComboToolBar
CComboToolBar::CComboToolBar()
{
    m_pInfo = NULL;
    m_nComboBoxCount = 0;
    m_nButtonCount = 0;
    m_nComboBoxCount = 0;
    m_nControlCount = 0;
    // This is in CToolBar
    m_nCount = 0;
    
    // This is OK for default small toolbar, 
    //  but we will try to get better estimate when toolbar is created
    m_nComboTop = 3;  

    m_pEnableConfig = NULL;
    m_sizeImage.cx = m_sizeImage.cy = 16;
    m_nIDBitmap = 0;
	m_pToolbar = NULL;
#ifdef XP_WIN16
    m_pToolTip = NULL;
#endif
}

CComboToolBar::~CComboToolBar()
{
    if ( m_pInfo ) {
        // NOTE: TOOLBAR OWNER MUST DELETE COMBOBOXES!
        delete m_pInfo;
    }
    if ( m_pEnableConfig ) {
        delete m_pEnableConfig;
    }
#ifdef XP_WIN16
    if( m_pToolTip ){
        delete m_pToolTip;
    }
#endif
}


BEGIN_MESSAGE_MAP(CComboToolBar, CToolBar)
	//{{AFX_MSG_MAP(CComboToolBar)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_CLOSE()
	ON_WM_SHOWWINDOW()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
BOOL CComboToolBar::Create( BOOL bIsPageComposer, CWnd* pParent, UINT nIDBar, UINT nIDCaption,
                            UINT * pIDArray, int nIDCount,      // Command ID array and count
                            UINT nIDBitmap, SIZE sizeButton, SIZE sizeImage )
{
    ASSERT( pParent );
	ASSERT(nIDCount >= 1);  // must be at least one of them
	ASSERT(pIDArray == NULL ||
		AfxIsValidAddress(pIDArray, sizeof(UINT) * nIDCount, FALSE));

#ifdef XP_WIN16
    DWORD dwStyle = WS_CHILD|WS_VISIBLE | CBRS_TOP;
	m_pToolTip = new CNSToolTip();
    if(m_pToolTip && !m_pToolTip->Create(this, TTS_ALWAYSTIP) ){
       TRACE("Unable To create ToolTip\n");
       delete m_pToolTip;
       m_pToolTip = NULL;
    } 
    if( m_pToolTip ){
        m_pToolTip->Activate(TRUE);
        // Lets use speedy tooltips
        m_pToolTip->SetDelayTime(200);
    }
#else
    DWORD dwStyle = WS_CHILD|WS_VISIBLE|CBRS_TOOLTIPS|CBRS_TOP|CBRS_FLYBY;
#endif

	// Toolbar is NOT initially visible
	if (!CToolBar::Create(pParent,
	                      dwStyle,
	                      nIDBar ) )
	{
		TRACE0("Failed to create CToolBar for CComboToolBar\n");
		return FALSE;   
	}
       
#ifdef XP_WIN32      
	if(bIsPageComposer){
		dwStyle = GetBarStyle();
		SetBarStyle(dwStyle & ~CBRS_BORDER_TOP);
	}           
#endif	

    if ( nIDBitmap && !LoadBitmap(nIDBitmap) )
	{
		TRACE0("Failed to load bitmap for CComboToolBar\n");
		return FALSE;   
	}

    // Allocate space for buttons in base class
    if ( ! SetButtons( NULL, nIDCount ) ){
        return FALSE;
    }

    // Allocate memory for our info structues
    m_pInfo = new TB_CONTROLINFO[nIDCount];
    ASSERT( m_pInfo );
    
    // clear out!
  	memset( (void*)m_pInfo, 0, nIDCount * sizeof(TB_CONTROLINFO) );

    // Allocate memory for config array
    m_pEnableConfig = new BOOL[nIDCount];
    ASSERT( m_pEnableConfig );
  	memset( (void*)m_pEnableConfig, 0, nIDCount * sizeof(BOOL) );

    // Fill our info array and set each item's info in base class
    UINT * pID = pIDArray;

    // Save it
    m_sizeButton = sizeButton;

    // Set the size of the toolbar buttons and images
    SetSizes(sizeButton, sizeImage );

    // Save stuff
    m_sizeImage = sizeImage;
    m_nIDBitmap = nIDBitmap;    
    
    // Extra margin to try to center any comboboxes (Buttons are at + 5)
    // (Approximate -- probably depends on font etc...)
    // m_nComboTop = 5 + ( (sizeButton.cy - 22 ) / 2);

    LPTB_CONTROLINFO pInfo = m_pInfo;    

    for ( int i=0; i < nIDCount; i++, pID++, pInfo++ ) {
        pInfo->nID =  *pID;
        if ( pInfo->nID == ID_SEPARATOR ) {
            pInfo->nWidth = SEPARATOR_WIDTH;     // Default separator
            SetButtonInfo( i, pInfo->nID, TBBS_SEPARATOR, pInfo->nWidth );
        }
        else if ( *pID == ID_COMBOBOX ) {
            pInfo->bComboBox = TRUE;

            // Set base class info - probably not really needed
            //   since we must fill in ComboBox data later!
            // Last param = width, which we set later with SetComboBox()
            // ***** SetButtonInfo( i, 0, TBBS_SEPARATOR, 0 ); // CAUSES CRASH??
            m_nComboBoxCount++;
            pInfo->pComboBox = NULL;  // Owner of toolbar must set this later!
        }
        else {
            SetButtonInfo( i, pInfo->nID, TBBS_BUTTON, m_nButtonCount );
            pInfo->nImageIndex = m_nButtonCount++;
            pInfo->bIsButton = TRUE;
            pInfo->nWidth = sizeImage.cx;
        }
        pInfo->bShow = TRUE;

        // Default is to show allow configuring all items        
        m_pEnableConfig[i] = TRUE;
    }

    m_nControlCount = m_nButtonCount + m_nComboBoxCount;
    
    // Upper limit imposed by resource ID allocation range
    //  for configuring which controls show on toolbar
    ASSERT( m_nControlCount<= MAX_TOOLBAR_CONTROLS );
    
    // ASSUME WE WANT DOCKING AND TOOLTIPS!

#ifdef WIN32
    if ( m_nComboBoxCount )
        // Ugly! Takes too much room to dock on sides if we have comboboxes
        EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
    else
        EnableDocking(CBRS_ALIGN_ANY);
#endif

    // Set caption that shows if toolbar is floating
    if ( nIDCaption ) {
        SetWindowText(szLoadString(nIDCaption));
    }
    return TRUE;
}

// Get the rect of a button in screen coordinates
// Used primarily with CDropdownToolbar
BOOL CComboToolBar::GetButtonRect( UINT nID, RECT * pRect )
{
    if( pRect ){
        LPTB_CONTROLINFO pInfo = m_pInfo;

        for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
            if ( pInfo->nID == nID ) {
                GetItemRect(i, pRect);
                ClientToScreen(pRect);
                return TRUE;
            }
        }
    }
    return FALSE;
}

// After creating toobar, call this to enable action on button down
// Used primarily when action is creation of a CDropdownToolbar
void CComboToolBar::SetDoOnButtonDown( UINT nID, BOOL bSet )
{
    BOOL bFound = FALSE;
    if( m_pToolbar && !m_pToolbar->SetDoOnButtonDownByCommand(nID, bSet) )
    {
        // Search for our buttons if no NSToolbar is used or command not found
        LPTB_CONTROLINFO pInfo = m_pInfo;
        ASSERT( pInfo );
        for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
            if ( pInfo->bIsButton && pInfo->nID == nID ) {
                 pInfo->bDoOnButtonDown = bSet;
                 break;
            }
        }
    }
}

void CComboToolBar::SetComboBox( UINT nID, CComboBox * pComboBox, 
                                 UINT nWidth, UINT nListWidth, UINT nListHeight )
{
    LPTB_CONTROLINFO pInfo = m_pInfo;
    ASSERT( pInfo );
    ASSERT( pComboBox );

    // Assign next available structure predesignated to be a combobox
    if ( m_nComboBoxCount )
    {
        int i;
		for ( i = 0; i < m_nCount; i++, pInfo++ ) {
			// Find the top of any of the buttons to use for
			//   top of combobox (+1 looks better)
            if ( pInfo->bIsButton && !pInfo->bComboBox ) {
				RECT rect;
				GetItemRect(i, &rect);
				m_nComboTop = rect.top + 1;
				break;
			}
		}
		i = 0;
		pInfo = m_pInfo;
		for ( ; i < m_nCount; i++, pInfo++ ) {
            if ( pInfo->bComboBox && pInfo->pComboBox == 0 )
            {
                // Calculate full height from number of items,
                if ( nListHeight == 0 ) {
                    // Need extra amount: 10 pixels above and below edit box?
                    int iEditHeight = pComboBox->GetItemHeight(-1) + 10;
                    int iItemHeight = pComboBox->GetItemHeight(0);   
                    //  but limit size to less than half the screen height
                    //  else single click in combo will immediately select because
                    //  list is drawn on top of closed combobbox
                    nListHeight = min( sysInfo.m_iScreenHeight / 2,
                                       iEditHeight + (pComboBox->GetCount() * iItemHeight) ); 
                }
                // Save the pointer for resizing box when controls are hidden
                pInfo->pComboBox = pComboBox;
                
                // Save command ID and width
                pInfo->nID = nID;

                // Add extra space (built-in separator) after right edge,
                //   with a little extra for Win16/NT3.51 version (more crowded layout)
                // Save this as static so GetToolbarWidth can use it
                iComboRightBorder = (sysInfo.m_bWin4 ? 6 : 8);
                pInfo->nWidth = nWidth + iComboRightBorder;

                CRect rect;
                GetItemRect( i, &rect );    // Get the left location from separator
                
                // Set size in base class (2 extra pixels at left edge)
                SetButtonInfo( i, nID, TBBS_SEPARATOR, pInfo->nWidth /*nWidth+2*/ );

                // Change the location and size of combobox window
                // We always add 2 to left edge to avoid overlap with adjacent buttons
                pComboBox->SetWindowPos( NULL, rect.left/*+2*/, m_nComboTop,
                                         nWidth, nListHeight,
                                         SWP_NOZORDER | SWP_NOACTIVATE  );
#ifdef _WIN32
                if( nListWidth && sysInfo.m_bWin4 ){
                    // Set the minimum width of the drop-down list when open                    
                    // TODO:  CAN'T FIGURE OUT HOW TO SET LIST WIDTH IN WIN16
                    pComboBox->SetDroppedWidth(nListWidth);
                }
#endif // XXX WIN16
                break; 
            }
        }
    }
}
    

// Enable or Disable ALL controls in toolbar at once to same state
void CComboToolBar::EnableAll( BOOL bEnable )
{
    LPTB_CONTROLINFO pInfo = m_pInfo;
    ASSERT( m_pInfo );

    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        if ( pInfo->nID != ID_SEPARATOR ) {
             _Enable( i, bEnable );
        }
    }
}


void CComboToolBar::Enable( UINT nID, BOOL bEnable )
{
    LPTB_CONTROLINFO pInfo = m_pInfo;

    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        if ( pInfo->nID == nID ) {
             _Enable( i, bEnable );
            break;
        }
    }
}

void CComboToolBar::_Enable( int iIndex, BOOL bEnable )
{
#ifdef FEATURE_EDCOMBTB
#include "edtcombtb.i00"
#endif	
}

void CComboToolBar::EnableConfigure( UINT nID, BOOL bEnable )
{
	ASSERT(m_pEnableConfig == NULL ||
		AfxIsValidAddress(m_pEnableConfig, sizeof(int) * m_nCount, FALSE));

    LPTB_CONTROLINFO pInfo = m_pInfo;
    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        if ( pInfo->nID == nID ) {
            m_pEnableConfig[i] = bEnable;
            break;
        }
    }
}

// iCheck: 0 = no, 1 = checked, 2 = indeterminate 
// Supply an array of check values 
// ARRAY MUST CORRESPOND TO ALL CONTROLS, NOT JUST BUTTONS!
void CComboToolBar::SetCheckAll( int * pCheckArray )
{	
	ASSERT(pCheckArray == NULL ||
           AfxIsValidAddress(pCheckArray, sizeof(int) * m_nCount, FALSE));

    LPTB_CONTROLINFO pInfo = m_pInfo;
    ASSERT( m_pInfo);

    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        if ( pInfo->bIsButton ) {
            _SetCheck( i, pCheckArray[i] );
        }
    }
}

void CComboToolBar::SetCheck( UINT nID, int iCheck )
{
    LPTB_CONTROLINFO pInfo = m_pInfo;
    ASSERT( m_pInfo);

    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        if ( pInfo->bIsButton && pInfo->nID == nID ) {
            _SetCheck( i, iCheck );
            break;
        }
    }
}

void CComboToolBar::_SetCheck( int iIndex, int iCheck )
{
#ifdef FEATURE_EDCOMBTB
#include "edtcombtb.i01"
#endif

	
}


void CComboToolBar::Show( UINT nID, BOOL bShow )
{
    LPTB_CONTROLINFO pInfo = m_pInfo;

    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        if ( pInfo->nID == nID && pInfo->bShow != bShow ) {
            ShowByIndex( i, bShow );
            break;
        }
    }
    RecalcLayout();
}

// Sets all values at once with an array of BOOLs: one for each control including separators
void CComboToolBar::ShowAll( BOOL * pShowArray )
{
    LPTB_CONTROLINFO pInfo = m_pInfo;
	ASSERT(pShowArray == NULL ||
		AfxIsValidAddress(pShowArray, sizeof(BOOL) * m_nCount, FALSE));

    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        if ( pInfo->bShow != pShowArray[i] ) {
            ShowByIndex( i, pShowArray[i] );
        }
    }
    RecalcLayout();
}

// Note: Always call RecalcLayout after using this
//       adjust width of Separators and hidden items
void CComboToolBar::ShowByIndex( int iIndex, BOOL bShow )
{
    ASSERT( iIndex >=0 && iIndex < m_nCount );
    LPTB_CONTROLINFO pInfo = m_pInfo + iIndex;
    ASSERT( pInfo );
    pInfo->bShow = bShow;

    if ( bShow )
    {
        if (pInfo->nID == ID_SEPARATOR )
            SetButtonInfo( iIndex, ID_SEPARATOR, TBBS_SEPARATOR, SEPARATOR_WIDTH);
        else if ( pInfo->pComboBox )
            SetButtonInfo( iIndex, ID_SEPARATOR, TBBS_SEPARATOR, pInfo->nWidth );
        else    // Button
            SetButtonInfo( iIndex, pInfo->nID, TBBS_BUTTON, pInfo->nImageIndex );
    }
    else {  // Hide an item by turning it into a separator
        SetButtonInfo( iIndex, ID_SEPARATOR, TBBS_SEPARATOR, HIDDEN_WIDTH);
    }
}

// GetWindowRect() never reports the width of a toolbar,
//   but gives CFrame's client width, so we need to 
//   calculate it.  If bCNSToolbar is true then include the CNSToolbar's width
int CComboToolBar::GetToolbarWidth(BOOL bCNSToolbar)
{
    RecalcLayout();
	int nToolbarWidth = 0;

    // Include width of embedded custom toolbar if requested
	if(bCNSToolbar && m_pToolbar)
		nToolbarWidth = m_pToolbar->GetWidth();

    // Get last toolbar item (relative to toolbar window)
    RECT rectLast;
    LPTB_CONTROLINFO pInfo;
    // Scan from end of items to find last non-separator
    // Use its right edge as width of the "old" toolbar region
    for ( int i = GetCount() - 1; i >= 0; i-- ) {
        pInfo = m_pInfo+i;
        if ( pInfo->nID != ID_SEPARATOR ) {
            GetItemRect(i, &rectLast);
            // Combobox has a built-in border - don't include it
            if( pInfo->pComboBox ){
                return( rectLast.right -  iComboRightBorder + nToolbarWidth);
            }
            return( rectLast.right + nToolbarWidth );
        }
    }
    return nToolbarWidth;
}

void CComboToolBar::RecalcLayout( int iStart )
{
    // The separators and buttons get calculated by CToolBar,
    //   but we must move the ComboBox windows
    LPTB_CONTROLINFO pInfo = m_pInfo;
    ASSERT(iStart < GetCount());
    BOOL bPrevShownIsSeparator = FALSE;
    UINT nWidth = 0;

    for ( int i = iStart; i < GetCount(); i++, pInfo++ ) {
        if ( pInfo->nID == ID_SEPARATOR ) {
            nWidth = bPrevShownIsSeparator ? 1 : SEPARATOR_WIDTH;                   
            SetButtonInfo( i, ID_SEPARATOR, TBBS_SEPARATOR, nWidth );
            bPrevShownIsSeparator = TRUE;
        } else {    // Button or Combobox
            if ( pInfo->bShow ) {
                bPrevShownIsSeparator = FALSE;
                nWidth = pInfo->nWidth;
            }
            else {   // We have a hidden button or combo
                // Very funky algorithm to cope with multiple hidden items in
                //   a row. Trying to deal with the 1-pixel overlap when items are drawn
                nWidth = (nWidth == 1 ) ? 2 : 1;
                
                SetButtonInfo( i, ID_SEPARATOR, TBBS_SEPARATOR, nWidth );
            }                    

            if ( pInfo->pComboBox ) {
                CRect rect;
                // New Rect.left for combo is calculated by GetItemRect
                GetItemRect( i, &rect );
                UINT nFlags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;

                // Set flag to show or hide ComboBox window
                if ( pInfo->bShow )
                    nFlags |= SWP_SHOWWINDOW;
                else
                    nFlags |= SWP_HIDEWINDOW;

                // Change the location, but not its size
                // We always add 2 to left edge to avoid overlap with adjacent buttons
                pInfo->pComboBox->SetWindowPos( NULL, rect.left/*+2*/, 
                                                m_nComboTop, 0, 0, nFlags );
            }
#ifdef XP_WIN16
            if( m_pToolTip ){
                RECT rect;
                GetItemRect( i, &rect );
                // Remove any previously-assigned tip, then add new one
                //m_pToolTip->DelTool(this->m_hWnd, pInfo->nID);
                m_pToolTip->AddTool(this, pInfo->nID, &rect, pInfo->nID);
                m_pToolTip->Activate(TRUE);
            }
#endif
        }
    }

    Invalidate( TRUE ); // Redraw toolbar and erase background
    GetParentFrame()->RecalcLayout(); // Tell parent frame to adjust for changes
}

void CComboToolBar::SetCNSToolbar(CNSToolbar2 *pToolbar)
{
	m_pToolbar = pToolbar;
	if(m_pToolbar)
	{
		m_pToolbar->SetParent(this);
		m_pToolbar->SetOwner(this);
	}

}

void CComboToolBar::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{
	m_pToolbar->OnUpdateCmdUI(pTarget, bDisableIfNoHndler);

	CToolBar::OnUpdateCmdUI(pTarget, bDisableIfNoHndler);
}

CSize CComboToolBar::CalcDynamicLayout(int nLength, DWORD dwMode )
{     
	CSize size(0,0);
	
#ifdef XP_WIN32
	size = CToolBar::CalcDynamicLayout(nLength, dwMode);
#endif
	
	int nHeight = m_pToolbar->GetHeight();

	if(size.cy < nHeight)
	{
		// 6 is what a CToolbar starts off as without any buttons
		size.cy = nHeight + 8;
	}

	return size;

}

/////////////////////////////////////////////////////////////////////////////
// CComboToolBar message handlers

#ifdef XP_WIN16
BOOL CComboToolBar::PreTranslateMessage(MSG* pMsg)
{
    if( m_pToolTip && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
    {
        m_pToolTip->RelayEvent(pMsg);
    }
    return CToolBar::PreTranslateMessage(pMsg);
}
#endif

void CComboToolBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    CToolBar::OnLButtonDown(nFlags, point);

    LPTB_CONTROLINFO pInfo = m_pInfo;

    for ( int i = 0; i < m_nCount; i++, pInfo++ ) {
        // Test if we clicked inside a button
        if ( pInfo->bIsButton ) {
            CRect rect;
            GetItemRect( i, &rect );
            if ( rect.PtInRect(point) && pInfo->bDoOnButtonDown ) {
                // Trigger command by simulating button up
                PostMessage(WM_LBUTTONUP, (WPARAM)nFlags, MAKELONG(point.x, point.y) );
                return;
            }
        }
    }

	// Send this message to the customizable toolbar for dragging
	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));

}

void CComboToolBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    CToolBar::OnLButtonUp(nFlags, point);

	// Send this message to the customizable toolbar for dragging
	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_LBUTTONUP, nFlags, MAKELPARAM(point.x, point.y));


}

void CComboToolBar::OnMouseMove(UINT nFlags, CPoint point)
{
    CToolBar::OnMouseMove(nFlags, point);

	// Send this message to the customizable toolbar for dragging
	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_MOUSEMOVE, nFlags, MAKELPARAM(point.x, point.y));

}

void CComboToolBar::OnSize( UINT nType, int cx, int cy )
{

	if(m_pToolbar)	{

		m_pToolbar->ShowWindow(SW_SHOW);
        // Use the combox top and height to size ourselves
        RECT rect;
        GetItemRect(0,&rect);

        // Get width without the CNSToolbar (just combobox region)
        int nPreviousWidth = GetToolbarWidth(FALSE);
		int nHeight = m_pToolbar->GetHeight();
		m_pToolbar->MoveWindow(nPreviousWidth, rect.top, cx - nPreviousWidth+4,
                               (rect.bottom-rect.top)+2*rect.top);
	}
    CToolBar::OnSize(nType, cx, cy);
}

#endif // EDITOR

