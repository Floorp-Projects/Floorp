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

///
//
// STATBAR.CPP
//
// DESCRIPTION:
//		This file contains the implementation of the CNetscapeStatusBar
//		class.
//
// CREATED:	03/01/96, 12:18:16		AUTHOR: Scott Jones
//
///


/**	INCLUDE	**/
#include "stdafx.h"
#include "statbar.h"
#include "ssl.h"
#include "tooltip.h"
#include "secnav.h"

#ifdef _DEBUG
#undef THIS_FILE
static char	BASED_CODE THIS_FILE[] = __FILE__;
#endif


#ifndef DYNAMIC_DOWNCAST
#define DYNAMIC_DOWNCAST( classname, ptr ) (classname *)(ptr)
#endif

//------------------------------------------------------------------------------
//	CONSTANTS

static const int nSEC_KEY_WIDTH = 34;

#ifdef _WIN32
 static const char szNSStatusProp[] = "NSStatusProp";
#else
 static const char szNSStatusProp1[] = "NSStatusProp1";
 static const char szNSStatusProp2[] = "NSStatusProp2"; 
#endif

#define IDT_PULSE       1966  // Pulsing vapor trails timer id
#define PULSE_INTERVAL   100  // Pulse interval for timer (ms)

#define IDT_STATBARMONITOR 1968
#define MONITOR_INTERVAL   50

// status bar format
static const UINT BASED_CODE pPaneIndicators[] =
{
    IDS_SECURITY_STATUS,
    IDS_TRANSFER_STATUS,
    ID_SEPARATOR
#ifdef MOZ_TASKBAR
	,IDS_TASKBAR
#endif /* MOZ_TASKBAR */
};

static const UINT BASED_CODE pSimpleIndicators[] =
{
    ID_SEPARATOR
};



BEGIN_MESSAGE_MAP(CNetscapeStatusBar, CNetscapeStatusBarBase)
	//{{AFX_MSG_MAP(CNetscapeStatusBar)
	ON_WM_PAINT()
	ON_WM_DESTROY()
	ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

HBITMAP CNetscapeStatusBar::sm_hbmpSecure    = NULL;
SIZE    CNetscapeStatusBar::sm_sizeSecure = {0,0};
int		CNetscapeStatusBar::sm_iRefCount = 0;

//------------------------------------------------------------------------------
CNetscapeStatusBar::CNetscapeStatusBar()
{
	m_nDone     = 0L;
    
    m_uTimerId  = 0;
    m_uVaporPos = 0;
    
    m_iStatBarPaneWidth = 0;
    m_pTaskBar = NULL;
    
	m_anIDSaved = NULL;
	m_iSavedCount = 0;

    m_enStatBarMode = eSBM_Panes;

    m_iAnimRef = 0;
    
    pParentSubclass = NULL;    

    m_pTooltip = NULL;

    m_pProxy2Frame = NULL;
    
    if( !sm_hbmpSecure )
    { 
    	VERIFY( sm_hbmpSecure = ::LoadBitmap( AfxGetResourceHandle(), MAKEINTRESOURCE( IDB_SECURE_STATUS ) ));
        
    	BITMAP bm;
    	::GetObject( sm_hbmpSecure, sizeof(bm), &bm );
    	sm_sizeSecure.cx = bm.bmWidth / 3;
    	sm_sizeSecure.cy = bm.bmHeight;
    }
	sm_iRefCount++;
} 

//------------------------------------------------------------------------------
CNetscapeStatusBar::~CNetscapeStatusBar()
{
    delete pParentSubclass;
    
    if( m_pTooltip )
    {
        delete m_pTooltip;
    }
    
	delete [] m_anIDSaved;

	if (!--sm_iRefCount) {
		VERIFY(::DeleteObject(sm_hbmpSecure));
		sm_hbmpSecure = NULL;
	}
}

//------------------------------------------------------------------------------
BOOL CNetscapeStatusBar::Create( CWnd *pParent, BOOL bSecurityStatus /*=TRUE*/, BOOL bTaskbar /*=TRUE*/ )
{
    m_bTaskbar = bTaskbar;
	m_bSecurityStatus = bSecurityStatus;
    
	if( !CNetscapeStatusBarBase::Create( pParent ) )
        return FALSE;

    if( m_bSecurityStatus )
    {
        m_pTooltip = new CNSToolTip2;
        if( !m_pTooltip )
        {
            return FALSE;
        }
        
        m_pTooltip->Create( this );
        m_pTooltip->AddTool( this, IDS_STATBAR_SECURITY, CRect(0,0,0,0), IDS_SECURITY_STATUS );
        m_pTooltip->AddTool( this, IDS_STATBAR_SECURITY, CRect(0,0,0,0), IDS_SIGNED_STATUS );
    }

	if (!CreateDefaultPanes())
		return FALSE;

    pParentSubclass = new CNetscapeStatusBar::CParentSubclass( pParent, this );
    ASSERT( pParentSubclass );
    
#ifdef MOZ_TASKBAR
    if( m_bTaskbar )
    {    
        //
       	// Tell the Task Bar manager about us so it can be docked on our status bar.
        //
       	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().RegisterStatusBar( this );
		SaveModeState();
	}
#endif /* MOZ_TASKBAR */
        
	return TRUE;
}

#ifdef _WIN32
//------------------------------------------------------------------------------
CWnd *CNetscapeStatusBar::SetParent( CWnd *pWndNewParent )
{
	ASSERT( ::IsWindow( m_hWnd ) );

    if( m_pDockSite )
    {
        m_pDockSite->RemoveControlBar( this );
        m_pDockSite = NULL;
    }
    
    CWnd *pOld = CWnd::FromHandle( ::SetParent( m_hWnd, pWndNewParent->GetSafeHwnd() ) );

    return pOld;
}
#endif

//------------------------------------------------------------------------------
BOOL CNetscapeStatusBar::CreateDefaultPanes() 
{
	if( m_bSecurityStatus ) {
        if( !SetIndicators( pPaneIndicators, sizeof(pPaneIndicators)/sizeof(UINT) ) )
		   return FALSE;
	} else {
		if( !SetIndicators( pPaneIndicators + 1, sizeof(pPaneIndicators)/sizeof(UINT) - 1 ) )            
		   return FALSE;
	}
	return TRUE;
}

//------------------------------------------------------------------------------
BOOL CNetscapeStatusBar::SetIndicators(const UINT* lpIDArray, int nIDCount)
{
	BOOL res = CNetscapeStatusBarBase::SetIndicators(lpIDArray, nIDCount);
	if (res) {
		m_iSavedCount = nIDCount;
		delete [] m_anIDSaved;
		if (m_iSavedCount) {
			m_anIDSaved = new UINT[m_iSavedCount];
			for (int i = 0; i < m_iSavedCount; i++) {
				m_anIDSaved[i] = lpIDArray[i];
			}
		} else {
			m_anIDSaved = NULL;
		}
		SetupMode();
#ifdef MOZ_TASKBAR
		((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().OnSizeStatusBar( this );
#endif /* MOZ_TASKBAR */
		SaveModeState();
	}
	return res;
}

//------------------------------------------------------------------------------
BOOL CNetscapeStatusBar::ResetPanes( EStatBarMode enStatBarMode, BOOL bForce /*=FALSE*/ )
{
    if( !bForce && (m_enStatBarMode == enStatBarMode) )
    {
        return TRUE;
    }

    if( m_enStatBarMode != enStatBarMode )
    {
        // Save state info of the current pane mode
        SaveModeState();
    }
    
    //
    // Set new panes/indicators
    //
    m_enStatBarMode = enStatBarMode;
    
    if( enStatBarMode != eSBM_Simple )
    {
        // The previous mode shall never be eSBM_Simple (eSBM_Simple is for menu help text)

        pParentSubclass->SetPrevMode( enStatBarMode );
    }
    
    SetupMode();
    
    return TRUE;
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::SaveModeState()
{
    switch( m_enStatBarMode )
    {
        case eSBM_Panes:
        {
#ifdef MOZ_TASKBAR
        	if( m_bTaskbar ) 
            {
	            // Save the IDS_TASKBAR pane width
	            
	            UINT u;
	            GetPaneInfo( CommandToIndex( IDS_TASKBAR ), u, u, m_iStatBarPaneWidth );
	        }
#endif /* MOZ_TASKBAR */
            break;
        }
        
        case eSBM_Simple:
        {
            // Don't care to save anything
            break;
        }
        
        default:
        {
           break;
        }
    }
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::SetupMode()
{
    switch( m_enStatBarMode )
    {
        case eSBM_Simple:
        {
			if (!CNetscapeStatusBarBase::SetIndicators(pSimpleIndicators, 1))
				return;

            SetPaneInfo( 0, ID_SEPARATOR, SBPS_STRETCH | SBPS_NOBORDERS, 0 );
            
            // Hide the task bar window (if docked)
            
#ifdef MOZ_TASKBAR
            ShowTaskBar( SW_HIDE );
#endif /* MOZ_TASKBAR */
            
            break;
        }
        
        case eSBM_Panes:
        {
			if (!m_anIDSaved)
				return;

			if (!CNetscapeStatusBarBase::SetIndicators(m_anIDSaved, m_iSavedCount))
				return;

#ifdef _WIN32                
            int iFudge = 4;
#else
            int iFudge = 2;
#endif
            
			RECT rcTool;

			int idx = CommandToIndex(IDS_SECURITY_STATUS);
			if (idx > -1) {
	            SetPaneInfo(idx, IDS_SECURITY_STATUS, SBPS_DISABLED, sm_sizeSecure.cx - iFudge);

				if (m_pTooltip) {
	                GetItemRect(idx, &rcTool);
		            m_pTooltip->SetToolRect(this, IDS_SECURITY_STATUS, &rcTool);
				}
			}
			idx = CommandToIndex(IDS_SIGNED_STATUS);
			if (idx > -1) {
	            SetPaneInfo(idx, IDS_SIGNED_STATUS, SBPS_DISABLED, sm_sizeSecure.cx - iFudge);       

				if (m_pTooltip) {
	                GetItemRect(idx, &rcTool);
		            m_pTooltip->SetToolRect(this, IDS_SIGNED_STATUS, &rcTool);
				}
			}	
            
            //
            // Set common pane info (size, style, etc).
            // WHS -- I'm assuming we'll always have these, probably not good in the long term
			//

            SetPaneInfo( CommandToIndex( ID_SEPARATOR ),        ID_SEPARATOR,        SBPS_STRETCH, 0 );
            SetPaneInfo( CommandToIndex( IDS_TRANSFER_STATUS ), IDS_TRANSFER_STATUS, SBPS_NORMAL,  90 );
                
            // Note the taskbar mgr sets the width of the taskbar pane
            // Also note we must call these SetPaneXXX methods even if m_bTaskbar is FALSE because
            // pPaneIndicators specifies the IDS_TASKBAR.  If we don't, the default CStatusBar
            // implementation is to display the IDS_TASKBAR string resource in the status - not good.
#ifdef MOZ_TASKBAR
			idx = CommandToIndex( IDS_TASKBAR );
			if (idx > -1) 
            {
				SetPaneInfo( idx, IDS_TASKBAR, SBPS_NOBORDERS, m_iStatBarPaneWidth );
				SetPaneText( idx, "", FALSE );

				// Show the task bar window (if docked)

				if( m_iStatBarPaneWidth )
					ShowTaskBar( SW_SHOWNA );
                 
                if( m_pTaskBar )
                {   
                    CRect rc;
                    GetItemRect( CommandToIndex( IDS_TASKBAR ), &rc );
                    //Get Item Rect is returning us the wrong dimensions.  It's off by 6 pixels
                    //Eventually figure out what's going wrong
                    rc.right -= 6;
                    m_pTaskBar->MoveWindow( &rc );
                    m_pTaskBar = NULL;
                }    
			}
#endif /* MOZ_TASKBAR */

            break;
        }
        
        default:
        {
           break;
        }
    }
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::ShowTaskBar( int iCmdShow )
{
#ifdef MOZ_TASKBAR
    if( !m_bTaskbar )
    {    
        return;
    }
    
    CWnd *pChild = GetWindow( GW_CHILD );
    while( pChild )
    {
        pChild->ShowWindow( iCmdShow );
        pChild = pChild->GetWindow( GW_HWNDNEXT );
    }
#endif
}

//------------------------------------------------------------------------------
BOOL CNetscapeStatusBar::SetTaskBarPaneWidth( int iWidth )
{
#ifdef MOZ_TASKBAR
    if( !m_bTaskbar || (iWidth < 0) )
    {
        return FALSE;
    }
    
    int nIndex = CommandToIndex( IDS_TASKBAR );
    if( nIndex != -1 )
    {
        UINT nID, nStyle; 
        int nWidth;
        GetPaneInfo( nIndex, nID, nStyle, nWidth );
        SetPaneInfo( nIndex, nID, nStyle, iWidth );
    }
    else
    {
        // The pane is not available right now.  We'll resize the pane later.
        m_iStatBarPaneWidth = iWidth;
    }
#endif /* MOZ_TASKBAR */
    return TRUE;
}

//------------------------------------------------------------------------------
BOOL CNetscapeStatusBar::SetTaskBarSize( CDockedTaskBar *pTaskBar )
{
#ifdef MOZ_TASKBAR
    if( !m_bTaskbar || !pTaskBar )
    {
        return FALSE;
    }
    
	CRect rc;
    int iIndex = CommandToIndex( IDS_TASKBAR );
    if( iIndex != -1 )
    {
        GetItemRect( iIndex, &rc );
    	//Get Item Rect is returning us the wrong dimensions.  It's off by 6 pixels
    	//Eventually figure out what's going wrong
    	rc.right -= 6;
    	pTaskBar->MoveWindow( &rc );
    }
    else
    {
        // The pane is not available right now.  We'll resize the task bar later.
        m_pTaskBar = pTaskBar;
    }
    
#endif /* MOZ_TASKBAR */
    return TRUE;
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::StartAnimation()
{
    m_iAnimRef = TRUE;
    
    SetPercentDone( -1 );
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::StopAnimation()
{
    if( m_iAnimRef )
    {
        m_iAnimRef = FALSE;
    }
    
    if( !m_iAnimRef )
    {
        SetPercentDone( 0 );
    }
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::SetPercentDone(const int32 nPercent)
{
	// Destroying the context in response to the window being destroyed
	// sometimes calls this, so we need to check that the window
	// still exists or we die.

	if (!m_hWnd)
		return;

	if( m_nDone != nPercent )
    {
        if( m_iAnimRef && (nPercent == 0) )
        {
            m_nDone = -1;
        }
        else
        {
    		m_nDone = nPercent;
        }
        
		DrawProgressBar();
        
        if( m_nDone >= 100 )
        {
            //
            // Ensure progress never sits on 100% i.e., the back end might never
            // reset progress to 0 after specifying 100. 
            //
            m_nDone = m_iAnimRef ? -1 : 0;
            DrawProgressBar();            
        }
	}
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::OnDestroy()
{
#ifdef MOZ_TASKBAR
    if( m_bTaskbar )
    {    
    	// Tell the Task Bar manager not to worry about us anymore
        
    	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().UnRegisterStatusBar( this );
	}
#endif /* MOZ_TASKBAR */
    
	CNetscapeStatusBarBase::OnDestroy();
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::OnLButtonDown( UINT nFlags, CPoint point )
{
    if( m_bSecurityStatus )
    {
    	RECT rc;
    	GetItemRect( CommandToIndex( IDS_SECURITY_STATUS ), &rc );

    	if( PtInRect( &rc, point ) ) 
        {
            CGenericFrame *pGenFrame = DYNAMIC_DOWNCAST( CGenericFrame, GetParentFrame() );
            if( pGenFrame )
            {
                MWContext *pContext = pGenFrame->GetMainContext()->GetContext();  

                if( pContext )
                {
                    CAbstractCX *pCX = ABSTRACTCX(pContext);   

                    if( pCX )
                    {      
                        URL_Struct *pURL = pCX->CreateUrlFromHist( TRUE ); 

                        SECNAV_SecurityAdvisor( pContext, pURL );
                    }  
                }  
                
            }
    	}
    }
    
	CNetscapeStatusBarBase::OnLButtonDown( nFlags, point );
}

//------------------------------------------------------------------------------
// We process the WM_SIZE message just to notify the Task Bar manager
// that we've changed, so it can do what it needs with any task bars
// docked on top of us.
//
void CNetscapeStatusBar::OnSize( UINT nType, int cx, int cy )
{
	CNetscapeStatusBarBase::OnSize( nType, cx, cy );
	if ( nType != SIZE_MINIMIZED && cx && cy) {
#ifdef MOZ_TASKBAR
	    if( m_bTaskbar ) 
		{
			// Tell the Task Bar manager
			((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().OnSizeStatusBar( this );
		}
#endif /* MOZ_TASKBAR */
		RECT rcTool;

		int idx = CommandToIndex(IDS_SECURITY_STATUS);
		if (idx > -1) {
			if (m_pTooltip) {
	            GetItemRect(idx, &rcTool);
		        m_pTooltip->SetToolRect(this, IDS_SECURITY_STATUS, &rcTool);
			}
		}
		idx = CommandToIndex(IDS_SIGNED_STATUS);
		if (idx > -1) {
			if (m_pTooltip) {
	            GetItemRect(idx, &rcTool);
		        m_pTooltip->SetToolRect(this, IDS_SIGNED_STATUS, &rcTool);
			}
		}	
	}
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::OnPaint()
{
#if defined(MSVC4)
	CStatusBar::OnPaint();
	CClientDC dc(this);
#else
	CPaintDC dc(this);
	// Do not call CStatusBar::OnPaint()!
    CNetscapeStatusBarBase::DoPaint( &dc );
#endif	// MSVC4

    //
    // Special per mode painting
    //
    switch( m_enStatBarMode )
    {
        case eSBM_Panes:
        {
           // Nothing to do
            break;
        }
        
        case eSBM_Simple:
        default:
        {
           // Nothing to do
           break;
        }
    }
    
    DrawSecureStatus(dc.m_hDC);
	DrawSignedStatus(dc.m_hDC);
   	DrawProgressBar();
}

//------------------------------------------------------------------------------
BOOL CNetscapeStatusBar::PreTranslateMessage( MSG *pMsg )
{
    if( m_pTooltip && (m_enStatBarMode != eSBM_Simple) )
    {
		if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
            m_pTooltip->RelayEvent( pMsg );
    }
    
    return CNetscapeStatusBarBase::PreTranslateMessage( pMsg );
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::DrawSecureStatus(HDC hdc)
{
 	int idx = CommandToIndex( IDS_SECURITY_STATUS );
 	if (idx < 0) 
        return;

	UINT nID, nStyle;
	int cxWidth;
	GetPaneInfo(idx, nID, nStyle, cxWidth);
	BOOL bSecure = !(nStyle & SBPS_DISABLED);
    
  	RECT rect; 
  	GetItemRect( idx, &rect );

  	HDC hdcBitmap = ::CreateCompatibleDC( hdc );
  	HBITMAP hbmOld = (HBITMAP)::SelectObject( hdcBitmap, sm_hbmpSecure );

  	FEU_TransBlt( hdc, rect.left+1, rect.top+1, sm_sizeSecure.cx, sm_sizeSecure.cy, hdcBitmap, bSecure ? sm_sizeSecure.cx : 0, 0,WFE_GetUIPalette(GetParentFrame()) );

  	::SelectObject( hdcBitmap, hbmOld );
  	VERIFY( ::DeleteDC( hdcBitmap ));
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::DrawSignedStatus(HDC hdc)
{
 	int idx = CommandToIndex( IDS_SIGNED_STATUS );
 	if (idx < 0) 
        return;

	UINT nID, nStyle;
	int cxWidth;
	GetPaneInfo(idx, nID, nStyle, cxWidth);
	BOOL bSigned = !(nStyle & SBPS_DISABLED);
    
	if (bSigned) {
  		RECT rect; 
  		GetItemRect( idx, &rect );

  		HDC hdcBitmap = ::CreateCompatibleDC( hdc );

  		HBITMAP hbmOld = (HBITMAP)::SelectObject( hdcBitmap, sm_hbmpSecure );

  		FEU_TransBlt( hdc, rect.left+1, rect.top+1, sm_sizeSecure.cx, sm_sizeSecure.cy, hdcBitmap, sm_sizeSecure.cx * 2, 0 ,WFE_GetUIPalette(GetParentFrame()));

  		::SelectObject( hdcBitmap, hbmOld );
  		VERIFY( ::DeleteDC( hdcBitmap ));
	}
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::DrawProgressBar()
{
	int idx = CommandToIndex( IDS_TRANSFER_STATUS );
	if (idx < 0)
		return;

	RECT rcBarArea;
	GetItemRect( idx, &rcBarArea );

    CClientDC dc( this );
    
	if( m_nDone == 0 )	
    {
        StopPulse();
        
		::InflateRect( &rcBarArea, -1, -1 );
        dc.FillRect( &rcBarArea, CBrush::FromHandle( sysInfo.m_hbrBtnFace ) );
        
        return;
	} 
    
    if( m_nDone == -1 )
    {
        if( !m_uTimerId )
        {
           StartPulse();
        }
        
        return;
    }
    
    // Decide the correct size of the thermo.
	InflateRect( &rcBarArea, -5, -2 );

    CDC dcMem;
    dcMem.CreateCompatibleDC( &dc );
    
    // Create a bitmap big enough for progress bar area
    CBitmap bmMem;
    bmMem.CreateCompatibleBitmap( &dc, rcBarArea.right-rcBarArea.left, rcBarArea.bottom-rcBarArea.top );
    CBitmap *pbmOld = dcMem.SelectObject( &bmMem );

    // Select the font
    CFont *pOldFont = NULL;
    if( GetFont() ) 
    {
        pOldFont = dcMem.SelectObject( GetFont() );
    }

    char szPercent[8];
    wsprintf( szPercent, "%u%%", m_nDone );
    dcMem.SetBkMode( TRANSPARENT );

    RECT rcBm = { 0, 0, rcBarArea.right-rcBarArea.left, rcBarArea.bottom-rcBarArea.top };

    int iNumColors = dc.GetDeviceCaps( NUMCOLORS );
    
    if( iNumColors == -1 || iNumColors > 256 )
    {
        // Render the image into the mem DC
    	dcMem.FillRect( &rcBm, CBrush::FromHandle( sysInfo.m_hbrBtnFace ) );

        dcMem.SetTextColor( GetSysColor( COLOR_BTNTEXT ) );
        dcMem.DrawText( szPercent, -1, &rcBm, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
        
        RECT rcBar = { 0, 0, CASTINT(rcBm.right * m_nDone / 100), CASTINT(rcBm.bottom) };
        dcMem.InvertRect( &rcBar );
    }
    else
    {
        // There's reasonable probability that the inverse color of btnface and btnshadow could be the same
        // when we are dealing with a palette of 256 colors or less.  So instead of using the invertrect
        // algorithm above, we'll just ensure the text is legible by painting a white background for it.

        // Render the image into the mem DC
    	dcMem.FillRect( &rcBm, CBrush::FromHandle( (HBRUSH)GetStockObject( WHITE_BRUSH ) ) );
        
        dcMem.SetTextColor( RGB(0,0,0) );
        dcMem.DrawText( szPercent, -1, &rcBm, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
        
        RECT rcBar = { 0, 0, CASTINT(rcBm.right * m_nDone / 100), CASTINT(rcBm.bottom) };
        dcMem.InvertRect( &rcBar );
    }
        
    dc.BitBlt( rcBarArea.left, rcBarArea.top, rcBarArea.right-rcBarArea.left, rcBarArea.bottom-rcBarArea.top, &dcMem, 0, 0, SRCCOPY );

    // Tidy up
    
    if( pOldFont ) 
    {
        dcMem.SelectObject( pOldFont );
    }
    dcMem.SelectObject( pbmOld );
}

//------------------------------------------------------------------------------
#define VAPOR_PERIOD 40 // Note, must be divisible by 2

void CNetscapeStatusBar::PulsingVaporTrails()
{
	RECT rcBarArea;
	GetItemRect( CommandToIndex( IDS_TRANSFER_STATUS ), &rcBarArea );

    CClientDC dc( this );
    
	InflateRect( &rcBarArea, -5, -2 );

    CDC dcMem;
    dcMem.CreateCompatibleDC( &dc );
    
    // Create a bitmap big enough for progress bar area
    CBitmap bmMem;
    bmMem.CreateCompatibleBitmap( &dc, rcBarArea.right-rcBarArea.left, rcBarArea.bottom-rcBarArea.top );
    CBitmap *pbmOld = dcMem.SelectObject( &bmMem );

    // Render the image into the mem DC
    
    RECT rcBm = { 0, 0, rcBarArea.right-rcBarArea.left, rcBarArea.bottom-rcBarArea.top };
	dcMem.FillRect( &rcBm, CBrush::FromHandle( sysInfo.m_hbrBtnFace ) );

    COLORREF crTrail = GetSysColor( COLOR_BTNSHADOW );
    COLORREF crFade  = GetSysColor( COLOR_BTNFACE );
    
    int iTrailR = (int)GetRValue( crTrail );
    int iTrailG = (int)GetGValue( crTrail );
    int iTrailB = (int)GetBValue( crTrail );        

    int iFadeR = (int)GetRValue( crFade );
    int iFadeG = (int)GetGValue( crFade );
    int iFadeB = (int)GetBValue( crFade );

    int iTrailLen = rcBm.right/2;

    UINT uHalfPeriod = m_uVaporPos;

    int iNumColors = dc.GetDeviceCaps( NUMCOLORS );
        
    if( m_uVaporPos <= VAPOR_PERIOD/2 )
    {
       for( int u = 0; u < iTrailLen; u++ )
       {
           COLORREF crVapor = RGB( (BYTE)(iTrailR - ((float)(iTrailR - iFadeR)/(float)iTrailLen * (float)u)), 
                                   (BYTE)(iTrailG - ((float)(iTrailG - iFadeG)/(float)iTrailLen * (float)u)), 
                                   (BYTE)(iTrailB - ((float)(iTrailB - iFadeB)/(float)iTrailLen * (float)u)) );
           RECT rcVapor = rcBm;
           rcVapor.right = -u + (rcBm.right / (VAPOR_PERIOD/2))*uHalfPeriod+1;
           rcVapor.left = rcVapor.right - (iTrailLen-u);
           CBrush br( crVapor );           
           dcMem.FillRect( &rcVapor, &br );
           
           if( (iNumColors != -1) && (iNumColors <= 256) )
           {
              // Just paint a solid bouncing block for 8-bit (or less) color
              break;
           }
       }
    }
    else
    {
       uHalfPeriod = m_uVaporPos - VAPOR_PERIOD/2;
       
       for( int u = 0; u < iTrailLen; u++ )
       {
           COLORREF crVapor = RGB( (BYTE)(iTrailR - ((float)(iTrailR - iFadeR)/(float)iTrailLen * (float)u)), 
                                   (BYTE)(iTrailG - ((float)(iTrailG - iFadeG)/(float)iTrailLen * (float)u)), 
                                   (BYTE)(iTrailB - ((float)(iTrailB - iFadeB)/(float)iTrailLen * (float)u)) );
           RECT rcVapor = rcBm;           
           rcVapor.left = (rcVapor.right + u) - ((rcBm.right / (VAPOR_PERIOD/2))*uHalfPeriod-1);
           rcVapor.right = rcVapor.left + (iTrailLen-u);
           CBrush br( crVapor );           
           dcMem.FillRect( &rcVapor, &br );           
           
           if( (iNumColors != -1) && (iNumColors <= 256) )
           {
              // Just paint a solid bouncing block for 8-bit (or less) color           
              break;
           }
       }
    }

    if( m_uVaporPos == VAPOR_PERIOD )
    {
       m_uVaporPos = 0;
    }
    else
    {
       m_uVaporPos++;
    }
        
    dc.BitBlt( rcBarArea.left, rcBarArea.top, rcBarArea.right-rcBarArea.left, rcBarArea.bottom-rcBarArea.top, &dcMem, 0, 0, SRCCOPY );

    // Tidy up
    dcMem.SelectObject( pbmOld );
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::StartPulse()
{
   m_uVaporPos = 0;
   
   m_uTimerId = SetTimer( IDT_PULSE, PULSE_INTERVAL, CNetscapeStatusBar::PulseTimerProc );
   
   if( !m_uTimerId )
   {
      return;
   }
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::StopPulse()
{
   if( !m_uTimerId )
   {
      return;
   }
   
   KillTimer( IDT_PULSE );
   
   m_uTimerId = 0;
}

//------------------------------------------------------------------------------
void CALLBACK EXPORT 
CNetscapeStatusBar::PulseTimerProc( HWND hwnd, UINT uMsg, UINT uTimerId, DWORD dwTime )
{
   CNetscapeStatusBar *pBar = (CNetscapeStatusBar *)CWnd::FromHandle( hwnd );
 
   if( (pBar->m_nDone == -1) && (pBar->GetStatBarMode() == eSBM_Panes) )
   {
      pBar->PulsingVaporTrails();
   }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
CNetscapeStatusBar::CParentSubclass::CParentSubclass( CWnd *pParent, CNetscapeStatusBar *pStatusBar )
{
    VERIFY( m_pStatusBar = pStatusBar );
    VERIFY( m_pParent    = pParent );
 
    ASSERT( IsWindow( m_pParent->m_hWnd ) );
    
    //
    // Subclass the parent window so we can process the WM_MENUSELECT msg i.e., so we
    // can switch modes from Simple/Panes without client intervention.
    //
    
   #ifdef _WIN32
    VERIFY( SetProp( pParent->m_hWnd, szNSStatusProp, this ) );
   #else
    VERIFY( SetProp( pParent->m_hWnd, szNSStatusProp1, (HANDLE)LOWORD((DWORD)this) ) );
    VERIFY( SetProp( pParent->m_hWnd, szNSStatusProp2, (HANDLE)HIWORD((DWORD)this) ) );
   #endif
   
    m_pfOldWndProc = (WNDPROC)SetWindowLong( m_pParent->m_hWnd, GWL_WNDPROC, 
                                             (LONG)CNetscapeStatusBar::CParentSubclass::ParentSubclassProc );
}
    
//------------------------------------------------------------------------------
CNetscapeStatusBar::CParentSubclass::~CParentSubclass()
{
    if( IsWindow( m_pParent->m_hWnd ) && 
        GetWindowLong( m_pParent->m_hWnd, GWL_WNDPROC ) == (LONG)CNetscapeStatusBar::CParentSubclass::ParentSubclassProc )
    {
        //
        // Unsubclass the parent window
        //
        
       #ifdef _WIN32
        VERIFY( RemoveProp( m_pParent->m_hWnd, szNSStatusProp ) );
       #else
        VERIFY( RemoveProp( m_pParent->m_hWnd, szNSStatusProp1 ) );
        VERIFY( RemoveProp( m_pParent->m_hWnd, szNSStatusProp2 ) );
       #endif
       
        SetWindowLong( m_pParent->m_hWnd, GWL_WNDPROC, (LONG)m_pfOldWndProc );
    }
}

//------------------------------------------------------------------------------
LRESULT CALLBACK EXPORT
CNetscapeStatusBar::CParentSubclass::ParentSubclassProc( HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
    CNetscapeStatusBar::CParentSubclass *pThis = NULL;
    
   #ifdef _WIN32
    pThis = (CNetscapeStatusBar::CParentSubclass *)GetProp( hWnd, szNSStatusProp );
   #else
    pThis = (CNetscapeStatusBar::CParentSubclass *)MAKELONG( GetProp( hWnd, szNSStatusProp1 ),
                                                             GetProp( hWnd, szNSStatusProp2 ) );
   #endif
    ASSERT( pThis );   
    
    LRESULT lRet = 0L;
    
    switch( uMessage )
    {
        case WM_MENUSELECT:
        {
           #ifdef _WIN32
            UINT uFlags = (UINT)HIWORD(wParam);
            HMENU hMenu = (HMENU)lParam;
           #else
            UINT uFlags = (UINT)LOWORD(lParam);
            HMENU hMenu = (HMENU)HIWORD(lParam);
           #endif
           
            //
            // Put the status bar into the correct mode for either:
            // 1.) Menu help text (aka Simple Text)
            // 2.) Multiple panes of info for progress etc.
            //
            
            BOOL bMenuClosed = (uFlags & 0xffff) && !hMenu;
            
            if( !bMenuClosed )
            {
                if( eSBM_Simple != pThis->m_pStatusBar->GetStatBarMode() )
                {
                    pThis->SetPrevMode( pThis->m_pStatusBar->GetStatBarMode() );
                    pThis->m_pStatusBar->ResetPanes( eSBM_Simple ); 
                    
                    // Start the timer for monitoring when the menu closes
                    pThis->StartMonitor();                    
                }
            }
          //## We have to reset on a timer notification because Windows doesn't
          //## always send the WM_MENUSELECT when menus close.  Typical.
          //  else
          //  {
          //      pThis->m_pStatusBar->ResetPanes( pThis->GetPrevMode() );
          //  }
            
            lRet = CallWindowProc( (WNDPROC)pThis->m_pfOldWndProc, hWnd, uMessage, wParam, lParam );
            break;
        }

        case WM_DESTROY:
        {
            pThis->StopMonitor();
            lRet = CallWindowProc( (WNDPROC)pThis->m_pfOldWndProc, hWnd, uMessage, wParam, lParam );
            
            break;
        }
                
        default:
           lRet = CallWindowProc( (WNDPROC)pThis->m_pfOldWndProc, hWnd, uMessage, wParam, lParam );
    }
    
    return lRet;
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::CParentSubclass::StartMonitor()
{
    m_uTimerId = ::SetTimer( m_pStatusBar->m_hWnd, IDT_STATBARMONITOR, MONITOR_INTERVAL, CNetscapeStatusBar::CParentSubclass::MonitorTimerProc );
}

//------------------------------------------------------------------------------
void CNetscapeStatusBar::CParentSubclass::StopMonitor()
{
    if( !m_uTimerId )
    {
        return;
    }
   
    ::KillTimer( m_pStatusBar->m_hWnd, IDT_STATBARMONITOR );
    
    m_uTimerId = 0;
}

//------------------------------------------------------------------------------
void CALLBACK EXPORT 
CNetscapeStatusBar::CParentSubclass::MonitorTimerProc( HWND hwnd, UINT uMsg, UINT uTimerId, DWORD dwTime )
{
    CNetscapeStatusBar *pBar = (CNetscapeStatusBar *)CWnd::FromHandlePermanent( hwnd );
    if( !pBar )
    {
        return;
    }
    
    CNetscapeStatusBar::CParentSubclass *pThis = pBar->pParentSubclass;
  
    if( !GetCapture() && (eSBM_Simple == pThis->m_pStatusBar->GetStatBarMode()) )
    {
        pThis->m_pStatusBar->ResetPanes( pThis->GetPrevMode() );
        pThis->StopMonitor();
    }
}

IMPLEMENT_DYNAMIC(CNetscapeStatusBar, CNetscapeStatusBarBase)
