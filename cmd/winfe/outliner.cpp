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

#include "stdafx.h"
#include "outliner.h"
#include "imagemap.h"
#include "prefapi.h"
#include "tip.h"
#include "fegui.h"
#ifdef _WIN32
#include "intelli.h"
#endif

#include "qahook.h"		// rhp - added for QA Partner automated testing messages

#define COL_LEFT_MARGIN	((m_cxChar+1)/2)
#define OUTLINE_TEXT_OFFSET 8

#define ID_OUTLINER_HEARTBEAT	2
#define ID_OUTLINER_TIMER       3
#define ID_OUTLINER_TIMER_DELAY 500

#define OUTLINER_PERCENTFACTOR 10000
#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(COutlinerView, CView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#ifndef _WIN32
HGDIOBJ GetCurrentObject(HDC hdc, UINT uObjectType)
{
	HGDIOBJ res = NULL;
	switch (uObjectType) {
	case OBJ_BRUSH:
		res = ::SelectObject(hdc, GetStockObject(NULL_BRUSH));
		::SelectObject(hdc, res);
		break;
	case OBJ_PEN:
		res = ::SelectObject(hdc, GetStockObject(NULL_PEN));
		::SelectObject(hdc, res);
		break;
	default:
		break;
	}
	return res;
}
#endif


//////////////////////////////////////////////////////////////////////////////
// COutliner

BOOL COutliner::m_bTipsEnabled = TRUE;

#define TIP_WAITING	1
#define TIP_SHOWING	2
#define TIP_SHOWN	3
#define TIP_HEARTBEAT	100
#define TIP_DELAY		250

#define DRAG_HEARTBEAT	250

BEGIN_MESSAGE_MAP(COutliner, CWnd)
	ON_WM_SETCURSOR ( )
	ON_WM_CREATE ( )
	ON_WM_PAINT ( )
	ON_WM_SIZE ( )
	ON_WM_GETMINMAXINFO ( )
	ON_WM_DESTROY ( )
	ON_WM_LBUTTONDOWN ( )
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN ( )
    ON_WM_RBUTTONUP()
	ON_WM_VSCROLL ( )
	ON_WM_SETFOCUS ( )
	ON_WM_KILLFOCUS ( )
	ON_WM_SYSKEYDOWN()
	ON_WM_KEYDOWN ( )
    ON_WM_KEYUP()
	ON_WM_LBUTTONDBLCLK ( )
	ON_WM_ERASEBKGND ( )
	ON_WM_TIMER ( )
	ON_WM_SYSCOLORCHANGE ( )
	ON_WM_GETDLGCODE()
#if defined(XP_WIN32) && _MSC_VER >= 1100
    ON_REGISTERED_MESSAGE(msg_MouseWheel, OnHackedMouseWheel)
    ON_MESSAGE(WM_MOUSEWHEEL, OnMouseWheel)
#endif

	// rhp - For QA partner message handling
	ON_MESSAGE(WM_COPYDATA, OnProcessOLQAHook)
	// rhp

END_MESSAGE_MAP()

class CNSOutlinerFactory :  public  CGenericFactory
{
public:
    CNSOutlinerFactory();
    ~CNSOutlinerFactory();
    STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj);
};

CNSOutlinerFactory::CNSOutlinerFactory()
{
   ApiApiPtr(api);
	api->RegisterClassFactory(APICLASS_OUTLINER,this);
}

CNSOutlinerFactory::~CNSOutlinerFactory()
{
}

STDMETHODIMP CNSOutlinerFactory::CreateInstance(
    LPUNKNOWN pUnkOuter,
    REFIID refiid,
    LPVOID * ppvObj)
{
    COutliner * pOutliner = new COutliner;
    *ppvObj = (LPVOID)((LPUNKNOWN)pOutliner);
    return NOERROR;
}

DECLARE_FACTORY(CNSOutlinerFactory);

COutliner::COutliner ( )
{
#ifdef _WIN32
    m_iWheelDelta = 0;
#endif
	m_iDragSelection = -1;
	m_iDragSelectionLineHalf = -1;
	m_iDragSelectionLineThird = -1;
	m_bDragSectionChanged = FALSE;
    m_iLastSelected = -1;
	m_iTotalLines   = 0;
	m_iTopLine      = 0;
	m_iSelection    = -1;
	m_iFocus		= -1;
	m_pColumn       = NULL;
	m_iNumColumns   = 0;
	m_iVisColumns	= 0;
	m_iTotalWidth	= 0;
	m_idImageCol	= 0;
	m_bHasPipes		= TRUE;
	m_pDropTarget   = NULL;
    m_bDraggingData = FALSE;
	m_bClearOnRelease = FALSE;
	m_bSelectOnRelease = FALSE;
	
	m_pTip = new CTip();
	m_pTip->Create();

	m_iTipState = 0;
	m_iTipTimer = 0;
	m_iTipRow = m_iTipCol = -1;

	m_hBoldFont = NULL;
	m_hRegFont = NULL;
	m_hItalFont = NULL;

	m_pUnkImage = NULL;
	ApiApiPtr(api);
	m_pUnkImage = api->CreateClassInstance(
		APICLASS_IMAGEMAP,NULL,(APISIGNATURE)GetOutlinerBitmap());
	m_pUnkImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIImage);
	ASSERT(m_pIImage);
	if (!m_pIImage->GetResourceID())
		m_pIImage->Initialize(GetOutlinerBitmap(),16,16);
}

COutliner::~COutliner ( )
{
    if(m_pDropTarget) {
        m_pDropTarget->Revoke();
        delete m_pDropTarget;
        m_pDropTarget = NULL;
    }

    if (m_pUnkImage) {
        if (m_pIImage)
            m_pUnkImage->Release();
    }

    if ( m_pColumn )
    {
        while ( m_iNumColumns )
        {
            if (m_pColumn[ m_iNumColumns - 1]->pHeader)
                free((char*)m_pColumn[ m_iNumColumns - 1]->pHeader);
            delete(m_pColumn[ m_iNumColumns - 1 ]);
            m_iNumColumns--;
        }
        free(m_pColumn);
    }
	if (m_hBoldFont) {
		theApp.ReleaseAppFont(m_hBoldFont);
	}
	if (m_hRegFont) {
		theApp.ReleaseAppFont(m_hRegFont);
	}
	if (m_hItalFont) {
		theApp.ReleaseAppFont(m_hItalFont);
	}

	delete m_pTip;
}

STDMETHODIMP COutliner::QueryInterface(
   REFIID refiid,
   LPVOID * ppv)
{
    *ppv = NULL;

    if (IsEqualIID(refiid,IID_IOutliner))
        *ppv = (LPIOUTLINER) this;

    if (*ppv != NULL) {
         AddRef();
         return NOERROR;
    }

    return CGenericObject::QueryInterface(refiid,ppv);
}

void COutliner::OnDestroy()
{
	if (m_iTipTimer) {
		KillTimer(m_iTipTimer);
		m_iTipTimer = 0;
	}
	TipHide();
}

void COutliner::TipHide()
{
	if (m_iTipTimer) {
		KillTimer(m_iTipTimer);
		m_iTipTimer = 0;
	}
	m_pTip->Hide();
	m_iTipState = TIP_SHOWN;
}

void COutliner::EnableTips(BOOL s)
{
	TipHide();
	m_iTipState = 0;
	m_bTipsEnabled = s;
}

BOOL COutliner::TestRowCol(POINT point, int &iRow, int &iCol)
{
	RECT rcClient;
	GetClientRect(&rcClient);

	if (::PtInRect(&rcClient, point)) {
		void *pLineData = NULL;
		int iSel = point.y  / m_itemHeight;
    	int iNewSel = m_iTopLine + iSel;
	    if ( ( pLineData = AcquireLineData ( iNewSel ) ) != NULL ) {
			ReleaseLineData( pLineData );
		    int i, offset;
			int y = iSel * m_itemHeight;

	        for ( i = 0, offset = 0; i < m_iVisColumns; i++ )
	        {
	            CRect rect ( offset, y,
	                		 offset + m_pColumn[ i ]->iCol, y + m_itemHeight );

		        if ( rect.PtInRect(point) ) 
	    	    {
					iRow = iNewSel;
					iCol = i;
					m_rcHit = rect;

					return TRUE;
	    	    }
	        	offset += m_pColumn[ i ]->iCol;
	        }
		}
	}
	return FALSE;
}

UINT COutliner::GetOutlinerBitmap(void)
{
	return IDB_OUTLINER;
}

BOOL COutliner::OnEraseBkgnd( CDC * )
{
    return TRUE;
}

int COutliner::GetColumnSize ( UINT idCol )
{

    if ( m_pColumn )
        for ( int iColumn = 0; iColumn < m_iNumColumns; iColumn++ )    
            if ( m_pColumn[ iColumn ]->iCommand == idCol )
                return m_pColumn[ iColumn ]->iCol;
    return 0;
}

int COutliner::GetColumnPercent ( UINT idCol )
{
    if ( m_pColumn )
        for ( int iColumn = 0; iColumn < m_iNumColumns; iColumn++ )    
            if ( m_pColumn[ iColumn ]->iCommand == idCol )
                return (int)(m_pColumn[ iColumn ]->fPercent * OUTLINER_PERCENTFACTOR);
    return 0;
}

int COutliner::GetColumnPos ( UINT idCol )
{
	if ( m_pColumn )
		for ( int iColumn = 0; iColumn < m_iNumColumns; iColumn++ )
			if ( m_pColumn[ iColumn ]->iCommand == idCol )
				return iColumn;
	return -1;
}

UINT COutliner::GetColumnAtPos ( int iPos )
{
	if ( m_pColumn )
		if (iPos >= 0 && iPos < m_iNumColumns)
			return m_pColumn[ iPos ]->iCommand;

	return 0;
}

int COutliner::AddColumn ( 
    LPCTSTR header, UINT command,
    int iMinCol, int iMaxCol, 
    Column_t ColType, 
    int iPercent, 
    BOOL bButton,
    CropType_t ct, AlignType_t at )
{
    OutlinerColumn_t * pColumn = new OutlinerColumn_t;
    pColumn->pHeader = _tcsdup(header);
    pColumn->cType = ColType;
    pColumn->bIsButton = bButton;
    pColumn->iMinColSize = iMinCol;
    pColumn->iMaxColSize = iMaxCol;
    pColumn->fPercent = (FLOAT)((FLOAT)iPercent/(FLOAT)OUTLINER_PERCENTFACTOR);
    pColumn->fDesiredPercent = pColumn->fPercent;
    pColumn->iCol =  iMinCol;
    pColumn->bDepressed = 0;
    pColumn->iCommand = command;
    pColumn->cropping = ct;
	pColumn->alignment = at;
    if ( !m_pColumn )
        m_pColumn = (OutlinerColumn_t **)malloc(sizeof(OutlinerColumn_t *));
    else
        m_pColumn = (OutlinerColumn_t **)realloc(m_pColumn,sizeof(OutlinerColumn_t*)*(m_iNumColumns+1));
    m_pColumn[ m_iNumColumns ] = pColumn;
	m_iVisColumns = ++m_iNumColumns;

    return m_iNumColumns;
}

void COutliner::SetColumnPos ( UINT idCol, int iColumn ) {
	int iOldColumn;

	iOldColumn = GetColumnPos(idCol);
	
	if ( (iOldColumn < 0) || (iColumn >= m_iNumColumns) || (iColumn < 0) )
		return;

    OutlinerColumn_t *temp;
	temp = m_pColumn[ iColumn ];
    m_pColumn[ iColumn ] = m_pColumn[ iOldColumn ];
    m_pColumn[ iOldColumn ] = temp;

    Invalidate ( );
}

void COutliner::SetColumnName ( UINT idCol, LPCTSTR pName )
{
	int iColumn;
	if ( ( iColumn = GetColumnPos( idCol ) ) < 0 )
		return;

    if (m_pColumn[ iColumn ]->pHeader)
        free((char*)m_pColumn[ iColumn ]->pHeader);
	m_pColumn[ iColumn ]->pHeader = _tcsdup(pName);
}

void COutliner::SetColumnSize ( UINT idCol, int iSize )
{
	int iColumn;
	if ( ( iColumn = GetColumnPos( idCol ) ) < 0 )
		return;

	m_pColumn[ iColumn ]->iCol = iSize;
}

void COutliner::SetColumnPercent ( UINT idCol, int iPercent )
{
	int iColumn;
	if ( ( iColumn = GetColumnPos( idCol ) ) < 0 )
		return;

	m_pColumn[ iColumn ]->fPercent = m_pColumn[iColumn]->fDesiredPercent = (FLOAT)((FLOAT)iPercent/(FLOAT)OUTLINER_PERCENTFACTOR);
}

void COutliner::LoadXPPrefs( const char *prefname )
{
	int i, j;
	
	char buf[256];
	char *formatString = buf;
	int iLen = 256;
	memset(formatString, 0, 256);
	PREF_GetCharPref(prefname, formatString, &iLen);
	char *end = formatString + iLen;

	if ( formatString[0] == 'v' && formatString[1] == '1') {
		formatString += 2;
	} else {
		return;
	}

	i = 0;
	j = 0;

	int nVis;
	if (sscanf(formatString, " %d %n", &nVis, &j) ==  1) {
		formatString += j;
	} else {
		return;
	}

	while (formatString < end && formatString[0]) {
		int id, nWidth;

		if (sscanf(formatString, " %d : %d %n", &id, &nWidth, &j) == 2) {

			// We should really check these values in case 
			// someone mucks with their registry.

			SetColumnPos( id, i );
			SetColumnPercent( id, nWidth );

			formatString += j;
			i++;
		} else {
			break;
		}
	}

	SetVisibleColumns( nVis );

	// Make it so
	RECT rcClient;
	GetClientRect(&rcClient);
	OnSize(0, rcClient.right, rcClient.bottom);

	GetParent()->Invalidate();
}

void COutliner::SaveXPPrefs( const char *prefname )
{
	CString cs, cs2;

	cs = "v1";
	cs2.Format(" %d", GetVisibleColumns());
	cs += cs2; 
	for (int i = 0; i < m_iNumColumns; i++) {
		cs2.Format(_T(" %d:%d"), 
				   GetColumnAtPos(i),
				   GetColumnPercent(GetColumnAtPos(i)));
		cs += cs2;
	}

	PREF_SetCharPref( prefname, cs );
}

void COutliner::OnSize( UINT nType, int cx, int cy )
{
    SqueezeColumns( -1, 0, FALSE );
    m_iPaintLines = ( cy / m_itemHeight ) + 1;
    EnableScrollBars ( );
}

void COutliner::OnGetMinMaxInfo ( MINMAXINFO FAR* lpMMI )
{
	CWnd::OnGetMinMaxInfo( lpMMI );
	int max = 0, min = 0;
	int i;

    for ( i = 0; i < m_iVisColumns; i++ ) {
		if (max >= 0) {
			if (m_pColumn[ i ]->iMaxColSize > 0) {
				max += m_pColumn[ i ]->iMaxColSize;
			} else {
				max = -1;
			}
		}
		if (min >= 0) {
			if (m_pColumn[ i ]->iMinColSize > 0) {
				min += m_pColumn[ i ]->iMinColSize;
			}
		}
	}
	if (max >= 0) {
		lpMMI->ptMaxSize.x;
		lpMMI->ptMaxTrackSize.x;
	}
	if (min >= 0) {
	    lpMMI->ptMinTrackSize.x;
	}
}

//------------------------------------------------------------------------------
// 
// Sizes variable width columns to fit the current width of the outliner.
// 
// iColFrom:  -1 implies all columns will size to fit.
//             0+ implies the column is sizing and all following columns will size to fit.
//
// iDelta:    Specifies the delta for the size.  If 0, all columns adjust to size.
//
// bRepaint:  Repaint all the affected columns after the size.
//
BOOL COutliner::SqueezeColumns( int iColFrom /*=-1*/, int iDelta /*=0*/, BOOL bRepaint /*=TRUE*/ )
{
    ASSERT( iColFrom >= -1 );
    
    if( m_iVisColumns == 0 )
    {
        return FALSE;
    }

    ASSERT( iColFrom < m_iVisColumns );
        
    if( (iColFrom != -1) && (iDelta == 0) )
    {
        return FALSE;
    }

    CFont *pOldFont = NULL;
    CDC *pDC = GetDC();
    if( GetFont() ) 
    {
        pOldFont = pDC->SelectObject( GetFont() );
    }
	CSize csMinWidth = pDC->GetTextExtent( "W", _tcslen( "W" ) );
    if( pOldFont ) 
    {
        pDC->SelectObject( pOldFont );
    }
    ReleaseDC( pDC );
    
    //
    // Calculate variable column width and percent
    //
	LONG iVariableWidth        = m_iTotalWidth;    
    LONG iActualVariableWidth  = 0;
	FLOAT fDesiredVariablePercent = (FLOAT)0;    
    int iOldRightWidth         = 0;
    LONG iActualTotalWidth     = 0;
    BOOL bResetPercents        = FALSE;
    int i;
    int iMinLeftWidth = 0;
    for( i = 0; i < m_iVisColumns; i++ )
    {
        if( m_pColumn[i]->cType == ColumnVariable )
        {
            if( i <= iColFrom )
            {
    			iOldRightWidth += m_pColumn[i]->iCol;
            }
            else
            {
                if( bResetPercents || (m_pColumn[i]->fPercent == m_pColumn[i]->fDesiredPercent) )
                {
                    bResetPercents = TRUE;
                    
                    m_pColumn[i]->fDesiredPercent = m_pColumn[i]->fPercent;
                }
                iMinLeftWidth += csMinWidth.cx; //max( m_pColumn[i]->iMinColSize, csMinWidth.cx );
            }
            
			fDesiredVariablePercent += m_pColumn[i]->fDesiredPercent;            
            
            // Only used when iDelta is not specified and is calculated e.g., during window size
            iActualVariableWidth += m_pColumn[i]->iCol;
		} 
        else 
        {
            if( i > iColFrom )
            {
    			iMinLeftWidth += m_pColumn[i]->iCol;
            }
        
			iVariableWidth -= m_pColumn[i]->iCol;
		}
        
        iActualTotalWidth += m_pColumn[i]->iCol;
	}
    
    if( (iColFrom == -1) && (iDelta == 0) )
    {
        //
        // We must calculate the delta when iColFrom is -1.  Most likely the outliner's window
        // has changed size.
        //
        iDelta = CASTINT(iActualTotalWidth - m_iTotalWidth);
    }

	if( iVariableWidth < 0 ) 
    {
		return FALSE;
	}

	LONG iSurplus = iVariableWidth;
    
	if( fDesiredVariablePercent == 0 ) 
    {
		return FALSE;
	}

    //
    // Calculate the smallest allowable variable column width and percent
    //
    FLOAT fMinPercent = (FLOAT)csMinWidth.cx/(FLOAT)iVariableWidth;
    
    if( (iColFrom != -1) && (iDelta < 0) && (m_pColumn[iColFrom]->fPercent <= fMinPercent) )
    {
        // Can't size it any smaller
        return FALSE;
    }
    
    int iNewLeftWidth = 0;
    
    //
    // Enforce the delta limit for left sizing
    //
    if( (iColFrom != -1) && ((m_pColumn[iColFrom]->iCol + iDelta) <= csMinWidth.cx) )
    {
        iDelta = m_pColumn[iColFrom]->iCol - csMinWidth.cx;
    }
    
    //
    // Enforce the delta limit for right sizing
    //
    int iNumLeftCols = (m_iVisColumns - iColFrom) - 1;
    int iNewRightWidth = iOldRightWidth + iDelta;
    iMinLeftWidth = iNumLeftCols * csMinWidth.cx;
    
    iNewLeftWidth = CASTINT(((iColFrom == -1) ? iActualVariableWidth : iVariableWidth) - iNewRightWidth);
    
    BOOL bSign = (iDelta > 0);
    
    if( iNewLeftWidth < iMinLeftWidth )
    {
        iDelta -= (iMinLeftWidth - iNewLeftWidth);
        iNewLeftWidth = CASTINT(iVariableWidth - (iOldRightWidth + iDelta));
        iNewRightWidth = CASTINT(iVariableWidth - iNewLeftWidth);
        ASSERT( iNewRightWidth == (iOldRightWidth + iDelta) );
    }
    
    if( iNewLeftWidth < iMinLeftWidth )
    {
        return FALSE;
    }
    
    if( iDelta == 0 )
    {
        return FALSE;
    }
    
    if( bSign != (iDelta > 0) )
    {
        // The sign should not have changed, but just to be safe...    
        return FALSE;
    }
    
    FLOAT fNewLeftPercent = (FLOAT)iNewLeftWidth/(FLOAT)iVariableWidth;

    for( i = 0; i < m_iVisColumns; i++ )
    {
        if( m_pColumn[i]->cType != ColumnVariable ) 
        {
            continue;
        }

        if( i < iColFrom )    
        {
            // Column's to the right of the sizing column don't change, so just decrement the var percent
            
            fDesiredVariablePercent -= m_pColumn[i]->fDesiredPercent;            
        }
        else if( i == iColFrom )
        {
            // Decrement the percent BEFORE we calc the new percent
            
            fDesiredVariablePercent -= m_pColumn[i]->fDesiredPercent;            
            
            // Add the delta and calc the new percent
                                
            m_pColumn[i]->iCol += iDelta;
            m_pColumn[i]->fPercent = (FLOAT)m_pColumn[i]->iCol/(FLOAT)iVariableWidth;
            m_pColumn[i]->fDesiredPercent = m_pColumn[i]->fPercent;
        }
        else
        {
            FLOAT fPrevPercent = m_pColumn[i]->fPercent;
            FLOAT fPrevDesiredPercent = m_pColumn[i]->fDesiredPercent;            
            
            m_pColumn[i]->fPercent = fNewLeftPercent * m_pColumn[i]->fDesiredPercent/fDesiredVariablePercent;
            m_pColumn[i]->fPercent = max( m_pColumn[i]->fPercent, fMinPercent );
                        
            m_pColumn[i]->iCol = (int)((FLOAT)iNewLeftWidth * m_pColumn[i]->fPercent/fNewLeftPercent);
            m_pColumn[i]->iCol = max( m_pColumn[i]->iCol, csMinWidth.cx );
        }
        
        iSurplus -= m_pColumn[i]->iCol;        
    }

    //
    // Consume the surplus space somewhat evenly
    //
    while( iSurplus )
    {
        for( i = iColFrom+1; i < m_iVisColumns; i++ )
        {
            if( m_pColumn[i]->cType != ColumnVariable ) 
            {
                continue;
            }
        
            if( iSurplus > 0 )
            {
                m_pColumn[i]->iCol += 1;
                iSurplus--;
            }
            else if( m_pColumn[i]->iCol > csMinWidth.cx )
            {
                m_pColumn[i]->iCol -= 1;
                iSurplus++;
            }
            
            if( !iSurplus )
            {
                break;
            }
        }
    }
        
    if( !bRepaint )
    {
        return TRUE;
    }
    
  	for( i = iColFrom; i < m_iVisColumns; i++) 
    {
  		InvalidateColumn( i );
    }
    
    return TRUE;
}

// iDesiredSize is what the outliner wants to be.  In this default case
// we use this height.
void COutliner::InitializeItemHeight(int iDesiredSize)
{
	m_itemHeight = iDesiredSize;

}

void COutliner::PropertyMenu( int iLine, UINT flags )
{
}

BOOL COutliner::ColumnCommand( int iCol, int iLine )
{
	return FALSE;
}

void COutliner::InitializeClipFormats()
{
}

void COutliner::OnRButtonDown ( UINT nFlags, CPoint point )
{
	TipHide();
    CWnd::OnRButtonDown ( nFlags, point );
    m_ptHit = point;
	int iSel = m_iTopLine + (point.y  / m_itemHeight);
	SelectItem( iSel, OUTLINER_RBUTTONDOWN );
}

void COutliner::OnRButtonUp( UINT nFlags, CPoint point )
{
    m_ptHit = point;
	int iSel = m_iTopLine + (point.y  / m_itemHeight);
    
	PropertyMenu( iSel, nFlags );
}

void COutliner::HandleMouseMove( POINT point )
{
	if (m_bTipsEnabled) {
		int iRow, iCol;
		CPoint ptCorner;

		// make sure the outline itself is active
#ifdef _WIN32
		CWnd* pParent = GetParentOwner();
#else
		CWnd* pParent = GetParent();
		if (pParent)
		{
			LONG lStyle = GetWindowLong(pParent->GetSafeHwnd(), GWL_STYLE);
			while ((lStyle & WS_CHILD) && (pParent = pParent->GetParent()))
			{
				lStyle = GetWindowLong(pParent->GetSafeHwnd(), GWL_STYLE);
			}
		}
#endif
		if ((pParent != GetActiveWindow()) || !pParent->IsWindowEnabled()) {
			TipHide();
			return;
		}

		// check for this application tracking (capture set)
		CWnd* pCapture = GetCapture();
		if (pCapture) {
			TipHide();
			return;
		}

		if (TestRowCol(point, iRow, iCol)) {
			if (( iCol != m_iTipCol) || (iRow != m_iTipRow)) {
				ASSERT(iCol < m_iNumColumns);
				ASSERT(iRow < m_iTotalLines);
				m_iTipRow = iRow;
				m_iTipCol = iCol;

				switch (m_iTipState) {
				case 0: case TIP_WAITING:
					m_iTipTimer = SetTimer(ID_OUTLINER_HEARTBEAT, TIP_DELAY, NULL);
					break;
				case TIP_SHOWING: 
					m_pTip->Hide();
					m_iTipState = TIP_WAITING;
					m_iTipTimer = SetTimer(ID_OUTLINER_HEARTBEAT, TIP_HEARTBEAT, NULL);
					break;
				case TIP_SHOWN:
					break;
				}

			}
			return;
		}

		TipHide();
		m_iTipCol = -1;
		m_iTipRow = -1;
		m_iTipState = 0;	
	}
	return;
}

void COutliner::OnMouseMove(UINT nFlags, CPoint point)
{
    if (GetCapture() == this) {
        // See if the mouse has moved far enough to start
        // a drag operation
        if ((abs(point.x - m_ptHit.x) > 3)
        || (abs(point.y - m_ptHit.y) > 3)) {
            // release the mouse capture
            ReleaseCapture();
            InitiateDragDrop();

			m_bClearOnRelease = FALSE;
			m_bSelectOnRelease = FALSE;
			SelectItem( m_iSelection, OUTLINER_LBUTTONUP, nFlags );
        }
    }
	if (m_iTipState != TIP_SHOWING)
		m_iTipState = TIP_WAITING;
	HandleMouseMove( point );
}

void COutliner::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this) {
		ReleaseCapture();
		SelectItem( m_iSelection, OUTLINER_LBUTTONUP, nFlags );
	}
}

void COutliner::OnLButtonDown ( UINT nFlags, CPoint point )
{
	Default();
	TipHide();

	SetFocus();

    m_ptHit = point;
    
	int iRow, iCol;

	if ( TestRowCol( point, iRow, iCol ) ){
		m_iRowHit = iRow;
		m_iColHit = iCol;

		if ( m_pColumn[ iCol ]->iCommand == m_idImageCol ) {
			void * pLineData;
			if ( ( pLineData = AcquireLineData ( iRow ) ) ) {
				int iDepth;
				GetTreeInfo ( iRow, NULL, &iDepth, NULL );
				ReleaseLineData ( pLineData );

				int iImageWidth = m_pIImage->GetImageWidth ( );
				RECT rcToggle = m_rcHit;
				rcToggle.left += iDepth * iImageWidth;
				rcToggle.right = rcToggle.left + iImageWidth;

				if ( ::PtInRect( &rcToggle, point ) ) {
					DoToggleExpansion ( iRow );
					return;
				}
			}
		}
		if ( ColumnCommand( m_pColumn[ iCol ]->iCommand, iRow) )
			return;

		SetCapture();
		SelectItem( iRow, OUTLINER_LBUTTONDOWN, nFlags );
	}
}

void COutliner::OnLButtonDblClk ( UINT nFlags, CPoint point )
{
	Default();
	TipHide();

	int iRow, iCol;
	
	if (TestRowCol( point, iRow, iCol )) {
		if ( m_pColumn[ iCol ]->iCommand == m_idImageCol ) {
			void * pLineData;
			if ( ( pLineData = AcquireLineData ( iRow ) ) ) {
				int iDepth;
				GetTreeInfo ( iRow, NULL, &iDepth, NULL );
				ReleaseLineData ( pLineData );

				int iImageWidth = m_pIImage->GetImageWidth ( );
				RECT rcToggle = m_rcHit;
				rcToggle.left += iDepth * iImageWidth;
				rcToggle.right = rcToggle.left + iImageWidth;

				if ( ::PtInRect( &rcToggle, point ) ) {
					DoToggleExpansion ( iRow );
					return;
				}
			}
		}
		if ( ColumnCommand( m_pColumn[ iCol ]->iCommand, iRow ) )
			return;

		SetCapture();
		SelectItem( iRow, OUTLINER_LBUTTONDBLCLK, nFlags );
	}
}

int COutliner::GetTotalLines()
{
	return m_iTotalLines;
}

int COutliner::GetDropLine()
{
	return m_iDragSelection;
}

int COutliner::GetDragHeartbeat()
{
	// Override to set a different drag time on scroll.
	return DRAG_HEARTBEAT;
}

HFONT COutliner::GetLineFont(void *pData)
{
	return m_hRegFont;
}

void COutliner::SetFocusLine( int iLine ) 
{
	if (iLine == -1)
		return;
	InvalidateLine(m_iFocus);
	m_iFocus = iLine;
	InvalidateLine(m_iFocus);
}

int COutliner::GetFocusLine()
{
	return m_iFocus;
}

int COutliner::GetDepth( int iLine )
{
	return 0;
}

int COutliner::GetNumChildren( int iLine )
{
	return 0;
}

int COutliner::GetParentIndex( int iLine )
{
	int depth = GetDepth( iLine );
	if ( iLine > 0 ) {
		int i = iLine - 1;
		while ( i > 0 && GetDepth( i ) >= depth ) {
			i--;
		}
		return i;
	} else {
		return 0;
	}
}

BOOL COutliner::IsCollapsed( int iLine )
{
	return FALSE;
}

void *COutliner::AcquireLineData( int iLine )
{
	return NULL;
}

void COutliner::ReleaseLineData( void *pData )
{

}

LPCTSTR COutliner::GetColumnText( UINT iCol, void *pData )
{
	return _T("");	
}

LPCTSTR COutliner::GetColumnTip( UINT iCol, void *pData )
{
	return NULL;	
}

BOOL COutliner::RenderData ( UINT idCol, CRect &rect, CDC &dc, LPCTSTR lpsz )
{
	return FALSE;
}

int COutliner::Expand(int iLine)
{
	return 0;
}

int COutliner::Collapse(int iLine)
{
	return 0;
}

int COutliner::ToggleExpansion(int iLine)
{
	return 0;
}

int COutliner::ExpandAll(int iLine)
{
	return DoExpandAll(iLine);
}

int COutliner::CollapseAll(int iLine)
{
	return DoCollapseAll(iLine);
}

void COutliner::GetTreeInfo( int iLine, unsigned long *pFlags, int *iDepth,
							 OutlinerAncestorInfo **pAncestor )
{

}

int COutliner::TranslateIcon( void *pData )
{
	return 0;
}

int COutliner::TranslateIconFolder( void *pData )
{
	return 0;
}

void COutliner::SelectItem( int iSel, int mode, UINT flags )
{
	void *pData;
	if ( pData = AcquireLineData( iSel ) ) {
		ReleaseLineData( pData );
		switch ( mode ) {
		case OUTLINER_LBUTTONDOWN:
			InvalidateLine( m_iSelection );
			m_iSelection = iSel;
			InvalidateLine( m_iSelection );
			break;

		case OUTLINER_LBUTTONUP:
			InvalidateLine( m_iFocus );
			m_iFocus = m_iSelection;
			InvalidateLine( m_iFocus );
			OnSelChanged();
            m_iLastSelected = m_iSelection;
			break;

		case 0:
			ASSERT(0);

		case OUTLINER_RBUTTONDOWN:
		case OUTLINER_KEYDOWN:
		case OUTLINER_SET:
			InvalidateLine( m_iFocus );
			InvalidateLine( m_iSelection );
			m_iFocus = m_iSelection = iSel;
			OnSelChanged();
            m_iLastSelected = m_iSelection;
			break;

		case OUTLINER_LBUTTONDBLCLK:
		case OUTLINER_RETURN:
			OnSelDblClk();
			break;

		default:
			ASSERT("What kind of garbage are you passing me?" == NULL);
		}
		UpdateWindow();
	}
}

void COutliner::InvalidateLine ( int iLineNo )
{
	if (iLineNo == -1)
		return;
    CRect rect, out;
    GetClientRect ( &rect );
    RectFromLine ( iLineNo - m_iTopLine, rect, out );
    InvalidateRect ( &out );
}

void COutliner::InvalidateLines( int iStart, int iCount )
{
    RECT rect, out;

	// Sanity check away
	int iInvStart = iStart - m_iTopLine;
	if (iInvStart < 0) {
		// Off top
		iCount += iInvStart; 
		iInvStart = 0;
	}

	if (iInvStart > m_iPaintLines)
		 // Nothing visible changed
		return;

	int iInvCount = (iInvStart + iCount) > m_iPaintLines ? 
					m_iPaintLines - iInvStart : iCount;

	if (iInvCount < 0)
		// Nothing visible changed
		return;

	// The case iInvCount < 0 is handled by for loop

    GetClientRect ( &rect );
	::SetRect ( &out, 
				rect.left, m_itemHeight * iInvStart, 
				rect.right, m_itemHeight * (iInvStart + iInvCount) );

	InvalidateRect ( &out );
}

BOOL COutliner::HighlightIfDragging(void)
{
	return TRUE;
}

void COutliner::GetColumnRect( int iCol, RECT &rc )
{
	GetClientRect(&rc);
	rc.left = 0;
	rc.right = 0;
	if ( iCol >= 0 && iCol < m_iVisColumns ) {
		for (int i = 0; i <= iCol; i++) {
			rc.left = rc.right;
			rc.right += m_pColumn[ i ]->iCol;
		}
	}
}

void COutliner::InvalidateColumn( int iCol )
{
	RECT rc;
	GetColumnRect( iCol, rc );
	InvalidateRect( &rc );
}

void COutliner::OnTimer( UINT timer)
{
    if (timer == ID_OUTLINER_TIMER) {
		int flags = 0;
        if (GetKeyState(VK_SHIFT)&0x8000)
            flags |= MK_SHIFT;
        if (GetKeyState(VK_CONTROL)&0x8000)
            flags |= MK_CONTROL;
	    SelectItem( m_iSelection,OUTLINER_TIMER, flags);
        KillTimer(ID_OUTLINER_TIMER);        
        return;
    }
	if (timer == ID_OUTLINER_HEARTBEAT) {
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(&point);

		if (m_iTipState == TIP_WAITING) {
  			CPoint ptCorner;
			int iRow, iCol;

			if (TestRowCol(point, iRow, iCol)) {
 				void * pLineData;
				if ( pLineData = AcquireLineData ( m_iTipRow ) ) {
					if ( ( iRow == m_iTipRow ) && ( iCol == m_iTipCol ) ) {
						int x = 0;
						if ( m_pColumn[ m_iTipCol ]->iCommand == m_idImageCol ) {
							 int iDepth = 0;
							 int iImageWidth = m_pIImage->GetImageWidth ( );
							 if ( m_bHasPipes ) {
								 GetTreeInfo ( m_iTipRow, NULL, &iDepth, NULL );
								 x += (iDepth + 1) * iImageWidth;
							 }
							 x += iImageWidth + OUTLINE_TEXT_OFFSET;
						}
						if ( m_pTip ) {
							HFONT hTipFont = GetLineFont(pLineData);
							LPCSTR lpszTipText = GetColumnTip(m_pColumn[ m_iTipCol ]->iCommand, pLineData);
							DWORD dwStyle = IsSelected( iRow ) ? NSTTS_SELECTED : 0;
							if (!lpszTipText) {
								lpszTipText = GetColumnText(m_pColumn[ m_iTipCol ]->iCommand, pLineData);
							} else {
								dwStyle |= NSTTS_ALWAYSSHOW;
							}
							dwStyle |= m_pColumn[ m_iTipCol ]->alignment == AlignRight ? NSTTS_RIGHT : 0;

							m_pTip->Show( this->GetSafeHwnd(),
										  m_rcHit.left + x, m_rcHit.top, 
										  m_pColumn[ m_iTipCol ]->iCol - x, m_itemHeight,
										  lpszTipText,dwStyle, hTipFont);
							m_iTipState = TIP_SHOWING;
							if ( !(m_iTipTimer = SetTimer( ID_OUTLINER_HEARTBEAT, 100, NULL )) )
								// Can't get timer, so give up
								TipHide();
						}
					} else {
						m_iTipTimer = SetTimer(ID_OUTLINER_HEARTBEAT, 100, NULL);
					}

					ReleaseLineData(pLineData);
					return;
				}
			}
			 // For whatever reason we shouldn't show
			TipHide();
		} else { // Since we're not waiting we must be showing
			HandleMouseMove(point);			
		}
	}
}

void COutliner::PositionNext ( void )
{
	if ( m_iFocus < m_iTotalLines - 1 ) {
		m_iFocus++;
    } 
}

void COutliner::PositionPrevious ( void )
{
	if ( m_iFocus > 0 ) {
        m_iFocus--;
    }
}

void COutliner::PositionHome ( void )
{
	m_iFocus = 0;
	m_iTopLine = 0;
}

void COutliner::PositionEnd ( void )
{
	m_iFocus = m_iTotalLines - 1;
	if (m_iFocus > ( m_iTopLine + m_iPaintLines - 2 ) ) {
		m_iTopLine = m_iFocus - (m_iPaintLines - 2);
	}
}

void COutliner::PositionPageDown()
{
	if ( ( m_iFocus + m_iPaintLines ) > m_iTotalLines ) {
		m_iFocus = m_iTotalLines - 1;
	} else {
		m_iFocus += m_iPaintLines - 2;
	}
}

void COutliner::PositionPageUp()
{
	if ( ( m_iFocus - m_iPaintLines - 2) < 0 ){
		m_iFocus = 0;
	} else {
		m_iFocus -= m_iPaintLines - 2;
	}
}

void COutliner::OnSysKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	TipHide();
    switch (nChar) {
	case VK_RETURN:
		if (m_iFocus >= 0)
			SelectItem(m_iFocus,OUTLINER_PROPERTIES);
		break;
    }
    CWnd::OnSysKeyDown(nChar,nRepCnt,nFlags);
}

void COutliner::OnKeyUp ( UINT nChar, UINT nRepCnt, UINT nFlags )
{
    Default();
    if (m_iSelection != m_iLastSelected)
        SetTimer(ID_OUTLINER_TIMER,ID_OUTLINER_TIMER_DELAY,NULL);
}

void COutliner::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags )
{
    int iOldSel = m_iSelection;
    int iOldTopLine = m_iTopLine;

	TipHide();

	if ( (GetKeyState(VK_CONTROL)&0x8000) && (nChar != VK_DELETE) ) {
		int nSBCode = -1;
		switch (nChar) {
			case VK_DOWN:
				nSBCode = SB_LINEDOWN;
				break;

			case VK_UP:
				nSBCode = SB_LINEUP;
				break;

			case VK_NEXT:
				nSBCode = SB_PAGEDOWN;
				break;                

			case VK_PRIOR:
				nSBCode = SB_PAGEUP;
				break;

			case VK_HOME:
				nSBCode = SB_TOP;
				break;

			case VK_END:
				nSBCode = SB_BOTTOM;
				break;

			default:
				Default();
		}
		if ( nSBCode != -1 ) {
			OnVScroll( nSBCode, 0, GetScrollBarCtrl( SB_VERT ));
		}
		return;
	} else {
		switch (nChar) {
			case VK_DELETE:
				DeleteItem ( m_iSelection );
				break;

			case VK_DOWN:
				PositionNext();
				break;

			case VK_UP:
				PositionPrevious();
				break;

			case VK_LEFT:
				if ( GetNumChildren( m_iSelection ) > 0 && !IsCollapsed( m_iSelection ) ) {
					DoCollapse( m_iSelection );
					iOldTopLine = m_iTopLine; // Scroll is handled by DoCollapse
				} else {
					int idx = GetParentIndex( m_iSelection );
					SelectItem( idx );
					ScrollIntoView( idx );
				}
				break;

			case VK_RIGHT:
				if ( GetNumChildren( m_iSelection ) > 0 ) {
					if ( IsCollapsed( m_iSelection ) ) {
						DoExpand( m_iSelection );
						iOldTopLine = m_iTopLine; // Scroll is handled by DoExpand
					} else {
						PositionNext();
					}
				}
				break;
			case VK_NEXT:
				PositionPageDown();
				break;                

			case VK_PRIOR:
				PositionPageUp();
				break; 
            
			case VK_MENU:               
				if (GetAsyncKeyState(VK_RETURN)&0x80)
					SelectItem( m_iSelection,OUTLINER_PROPERTIES);
				break;
			case VK_RETURN:
				SelectItem ( m_iFocus, OUTLINER_RETURN );
				break;

			case VK_HOME:
				PositionHome ( );
				break;

			case VK_END:
				PositionEnd ( );
				break;

			case VK_SUBTRACT:
    			if( m_iSelection >= 0 ) {
					DoCollapse( m_iSelection );
					iOldTopLine = m_iTopLine; // Scroll is handled by DoCollapse
				}
				break;

			case VK_ADD:
    			if( m_iSelection >= 0 ) {
					DoExpand( m_iSelection );
					iOldTopLine = m_iTopLine; // Scroll is handled by DoExpand
				}
				break;

			case VK_SPACE:
    			if( m_iSelection >= 0 ) {
					DoToggleExpansion ( m_iSelection );
					iOldTopLine = m_iTopLine; // Scroll is handled by DoToggleExpansion
				}
				break;
      
			case VK_MULTIPLY:
				if ( m_iSelection >= 0) {
					ExpandAll( m_iSelection );
					iOldTopLine = m_iTopLine; // Scroll is handled by ExpandAll
				}
				break;

			case VK_DIVIDE:
				if ( m_iSelection >= 0) {
					CollapseAll( m_iSelection );
					iOldTopLine = m_iTopLine; // Scroll is handled by CollapseAll
				}
				break;

			default:
				Default();
				return;
		}
	}

	m_iSelection = m_iFocus;

    SetScrollPos ( SB_VERT, m_iTopLine );

 	if ( m_iSelection < m_iTopLine && m_iSelection >= 0) {
        m_iTopLine = m_iSelection;
	}
    if ( m_iSelection > ( m_iTopLine + m_iPaintLines - 2 ) ) {
        m_iTopLine = m_iSelection - (m_iPaintLines - 2);
	}

	BOOL bRepaint = FALSE;

    int iDiff = iOldTopLine - m_iTopLine;
    if ( iDiff != 0 )
    {
        ScrollWindow ( 0, m_itemHeight * iDiff );
		bRepaint = TRUE;
    }

    if ( m_iSelection != iOldSel )
    {
		InvalidateLine(m_iSelection);
		InvalidateLine(iOldSel);
        UINT flags = 0;
        if (GetKeyState(VK_SHIFT)&0x8000)
            flags |= MK_SHIFT;
		if( m_iSelection >= 0 )
	        SelectItem ( m_iSelection, OUTLINER_KEYDOWN, flags );
        KillTimer(ID_OUTLINER_TIMER);

		bRepaint = TRUE;
    }

	if (bRepaint)
        UpdateWindow ( );

    Default();
}

void COutliner::OnSysColorChange ( )
{

    CWnd::OnSysColorChange ( );
	m_pIImage->ReInitialize();
	m_pIUserImage->ReInitialize();
    Invalidate ( );
}

UINT COutliner::OnGetDlgCode( )
{
	return DLGC_WANTARROWS;
}

void COutliner::OnKillFocus ( CWnd * pNewWnd )
{
    CWnd::OnKillFocus ( pNewWnd );
	if (m_iSelection >= 0)
        for (int i =0; i < (m_iTopLine+m_iPaintLines); i++)
            if (IsSelected(i))
	            InvalidateLine(i);

	((COutlinerParent*)GetParent())->UpdateFocusFrame();
}

void COutliner::OnSetFocus ( CWnd * pOldWnd )
{
	Default();

	if (m_iSelection >= 0)
        for (int i =0; i < (m_iTopLine+m_iPaintLines); i++)
            if (IsSelected(i))
	            InvalidateLine(i);

	((COutlinerParent*)GetParent())->UpdateFocusFrame();
}

void COutliner::SetTotalLines( int iLines )
{
	m_iTotalLines = iLines;
	if (m_iTopLine > m_iTotalLines) {
		m_iTopLine = m_iTotalLines;
	}
	EnableScrollBars();
}

void COutliner::EnableScrollBars ( void )
{
    if (m_iTotalLines >= m_iPaintLines) {
        ShowScrollBar(SB_VERT);
#ifdef WIN32
        SCROLLINFO ScrollInfo;
        ScrollInfo.cbSize = sizeof(SCROLLINFO);
        ScrollInfo.fMask = SIF_PAGE|SIF_RANGE|SIF_POS;
		ScrollInfo.nMin = 0;
		ScrollInfo.nMax = m_iTotalLines - 1;
        ScrollInfo.nPage = m_iPaintLines - 1;
		ScrollInfo.nPos = m_iTopLine;
        SetScrollInfo(SB_VERT,&ScrollInfo, TRUE);
#else
		SetScrollPos( SB_VERT, m_iTopLine );
        SetScrollRange(SB_VERT,0,(m_iTotalLines - m_iPaintLines)+1, TRUE);
#endif
    } else {
        if (m_iTopLine > 0) {
            m_iTopLine = 0;
            Invalidate();
        }
        ShowScrollBar ( SB_VERT, FALSE );
    }
}

void COutliner::ScrollIntoView( int iVisible )
{
    if (iVisible != -1) {
 		int iOldTop = m_iTopLine;
        CRect rect;
        GetClientRect(&rect);
        int iAdjust = ((rect.Height() - ((m_iPaintLines-1)*m_itemHeight))+2)/m_itemHeight;
        if (iAdjust == 0) 
            iAdjust = 1;
        else
            iAdjust = 0;

        if ( ( iVisible < m_iTopLine ) ||
            ( iVisible > ((m_iTopLine + m_iPaintLines) - iAdjust))) {

            m_iTopLine = iVisible - (m_iPaintLines/2);
            if ( m_iTopLine < 0 )
                m_iTopLine = 0;
            Invalidate ( );
        }
        else if (iVisible == ((m_iTopLine+m_iPaintLines)-iAdjust)) {
            m_iTopLine++;
            Invalidate();
        }
		UpdateWindow( );
		SetScrollPos( SB_VERT, m_iTopLine );
    }
}

void COutliner::EnsureVisible( int iVisible )
{
    if (iVisible != -1) {
		int iOldTop = m_iTopLine;
		if (iVisible < m_iTopLine) {
			m_iTopLine = iVisible;
		} else if (iVisible >= (m_iTopLine + m_iPaintLines - 1)) {
			m_iTopLine = iVisible - m_iPaintLines + 2;
		} else {
			return;
		}
		int iDelta = iOldTop - m_iTopLine;

        ScrollWindow( 0, m_itemHeight * iDelta );
        UpdateWindow( );
	    SetScrollPos( SB_VERT, m_iTopLine );
    }
}

void COutliner::OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar )
{
    int iOldTopLine = m_iTopLine;
    switch ( nSBCode )
    {
        case SB_BOTTOM:
			m_iTopLine = m_iTotalLines - (m_iPaintLines - 2);
            break;
        case SB_LINEDOWN:
            if ( m_iTopLine + ( m_iPaintLines - 1 ) < m_iTotalLines + 1 )
                m_iTopLine++;
            break;
        case SB_LINEUP:
            if ( m_iTopLine )
                m_iTopLine--;
            break;
        case SB_PAGEDOWN:
            if ( m_iTopLine + ( m_iPaintLines - 1 ) < m_iTotalLines )
                m_iTopLine += ( m_iPaintLines - 1 );
            break;
        case SB_PAGEUP:
            m_iTopLine -= (m_iPaintLines - 1);
            break;
        case SB_THUMBPOSITION:
            m_iTopLine = nPos;
            break;
        case SB_THUMBTRACK:
            m_iTopLine = nPos;
            break;
        case SB_TOP:
			m_iTopLine = 0;
            break;
    }

    if ( m_iTopLine < 0 )
        m_iTopLine = 0;

    int iDiff = iOldTopLine - m_iTopLine;
    if ( iDiff != 0 )
    {
        ScrollWindow ( 0, m_itemHeight * iDiff );
        UpdateWindow ( );
    }

    SetScrollPos ( SB_VERT, m_iTopLine );
	Default();
}

BOOL COutliner::ViewerHasFocus ( )
{
    return ( GetFocus ( ) == this );
}

void COutliner::DoToggleExpansion( int iLine )
{
	int iDelta = ToggleExpansion( iLine );
	if ( iDelta > 0 ) {
		iDelta = iDelta > m_iPaintLines - 2 ? m_iPaintLines - 2 : iDelta;
		EnsureVisible( iLine + iDelta );
	}
}


void COutliner::DoExpand( int iLine )
{
	int iDelta = Expand( iLine );
	if ( iDelta > 0 ) {
		iDelta = iDelta > m_iPaintLines - 2 ? m_iPaintLines - 2 : iDelta;
		EnsureVisible( iLine + iDelta );
	}
}

void COutliner::DoCollapse( int iLine )
{
	Collapse( iLine );
}

int COutliner::DoExpandAll( int iLine )
{
	int iDelta = 0;
	int iDepth = GetDepth( iLine );
	iDelta += Expand(iLine);
	
	for (int i = iLine + 1; GetDepth(i) > iDepth; i++)
		iDelta += Expand(i);	

	if ( iDelta > 0 ) {
		iDelta = iDelta > m_iPaintLines - 2 ? m_iPaintLines - 2 : iDelta;
		EnsureVisible( iLine + iDelta );
	}
	return iDelta;
}

int COutliner::DoCollapseAll( int iLine )
{
	int iDelta = 0;
	int iDepth = GetDepth( iLine );
	int i = iLine;
	while ((i + 1) < m_iTotalLines && GetDepth(i + 1) > iDepth)
		i++;

	while (i >= iLine) {
		iDelta += Collapse(i);
		i--;
	}
	return iDelta;
}

int COutliner::GetPipeIndex (void *pData, int iDepth, OutlinerAncestorInfo * pAncestor )
{
    int iTranslated = TranslateIconFolder(pData);
    switch ( iTranslated )
    {
        case OUTLINER_OPENFOLDER:
        case OUTLINER_CLOSEDFOLDER:

            if ( iTranslated == OUTLINER_OPENFOLDER )
            {
                if ( pAncestor->has_prev && pAncestor->has_next )
                    return IDX_OPENMIDDLEPARENT;
                else if ( pAncestor->has_prev )
                    return IDX_OPENBOTTOMPARENT;
                else if ( pAncestor->has_next &&  iDepth )
                    return IDX_OPENMIDDLEPARENT;
                else if ( pAncestor->has_next )         
                    return IDX_OPENTOPPARENT;
                else if ( iDepth )
                    return IDX_OPENBOTTOMPARENT;
                else
                    return IDX_OPENSINGLEPARENT;
            }
            else
            {
                if ( pAncestor->has_prev && pAncestor->has_next )
                    return IDX_CLOSEDMIDDLEPARENT;
                else if ( pAncestor->has_prev )
                    return IDX_CLOSEDBOTTOMPARENT;
                else if (  pAncestor->has_next && iDepth )
                    return IDX_CLOSEDMIDDLEPARENT;
                else if ( pAncestor->has_next )         
                    return IDX_CLOSEDTOPPARENT;
                else if ( iDepth )
                    return IDX_CLOSEDBOTTOMPARENT;
                else
                    return IDX_CLOSEDSINGLEPARENT;
            }
            break;

        default:

            if ( pAncestor->has_prev && pAncestor->has_next )
                return IDX_MIDDLEITEM;
            else if ( pAncestor->has_prev )
                return IDX_BOTTOMITEM;
            else if ( pAncestor->has_next && iDepth )
                return IDX_MIDDLEITEM;
            else if ( pAncestor->has_next )         
                return IDX_TOPITEM;
            else if ( iDepth )
                return IDX_BOTTOMITEM;
            break;
            
    }
    return IDX_EMPTYITEM;
}

void COutliner::RectFromLine ( int iLineNo, LPRECT lpRect, LPRECT lpOutRect )
{
    ::SetRect ( lpOutRect, 
				0, m_itemHeight * iLineNo, 
				lpRect->right, m_itemHeight * iLineNo + m_itemHeight );
}

void COutliner::EraseLine ( int iLineNo, HDC hdc, LPRECT lpWinRect )
{
	if ( iLineNo >= 0 ) {
	    RECT rect;
	    RectFromLine ( iLineNo, lpWinRect, &rect );  
		::FillRect(hdc, &rect, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
	}
}


int COutliner::DrawPipes ( int iLineNo, int iColNo, int offset, HDC hdc, void * pLineData )
{
	int iImageWidth = m_pIImage->GetImageWidth ( );
	int iMaxX = offset + m_pColumn[ iColNo ]->iCol;
	int idx;

	int iDepth;
	uint32 flags;
	OutlinerAncestorInfo * pAncestor;
	GetTreeInfo ( m_iTopLine + iLineNo, &flags, &iDepth, &pAncestor );

	RECT rect;
	rect.left = offset;
	rect.right = rect.left + iImageWidth;
	rect.top = iLineNo * m_itemHeight;
	rect.bottom = rect.top + m_itemHeight;

	if ( m_bHasPipes ) {
		for ( int i = 0; i < iDepth; i++ ) {
			if ( rect.right <= iMaxX ) {
				if ( pAncestor && pAncestor[ i ].has_next ) {
					m_pIImage->DrawImage( IDX_VERTPIPE, rect.left, rect.top, hdc, FALSE );
				} else {
					::FillRect(hdc, &rect, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
				}
			}
			rect.left += iImageWidth;
			rect.right += iImageWidth;
		}
    
		if ( rect.right <= iMaxX ) {
			idx = 0;
			if ( pAncestor ) {
				idx = GetPipeIndex ( pLineData, iDepth, &pAncestor[iDepth] );
			}
			if ( idx ) { 
				m_pIImage->DrawImage( idx, rect.left, rect.top, hdc, FALSE );
			} else {
				::FillRect(hdc, &rect, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
			}
		}
		rect.left += iImageWidth;
		rect.right += iImageWidth;
	}
    if ( rect.right <= iMaxX ) {
        idx = TranslateIcon (pLineData);
        m_pIUserImage->DrawImage ( idx, rect.left, rect.top, hdc, FALSE );
    }
	return rect.right;
}

void COutliner::DrawColumnText ( HDC hdc, LPRECT lpColumnRect, LPCTSTR lpszString,
								 CropType_t cropping, AlignType_t alignment )
{
	if (!(lpColumnRect->right - lpColumnRect->left))
		return;
	ASSERT(lpszString);
    int iLength = _tcslen(lpszString);
	if (!iLength)
		return;

	UINT dwDTFormat = DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
	switch (alignment) {
	case AlignCenter:
		dwDTFormat |= DT_CENTER;
		break;
	case AlignRight:
		dwDTFormat |= DT_RIGHT;
		break;
	case AlignLeft:
	default:
		dwDTFormat |= DT_LEFT;
	}

	UINT dwMoreFormat = 0;
	switch (cropping) {
	case CropCenter:
		dwMoreFormat |= WFE_DT_CROPCENTER;
		break;
	case CropLeft:
		dwMoreFormat |= WFE_DT_CROPLEFT;
		break;
	case CropRight:
	default:
		dwMoreFormat |= WFE_DT_CROPRIGHT;
		break;
	}

	RECT textRect = *lpColumnRect;
	// Adjust the text rectangle for the left and right margins
	textRect.left += COL_LEFT_MARGIN;
	textRect.right -= COL_LEFT_MARGIN;

	WFE_DrawTextEx( m_iCSID, hdc, (LPTSTR) lpszString, iLength, &textRect, dwDTFormat, dwMoreFormat );
}

void COutliner::PaintColumn ( int iLineNo, int iColumn, LPRECT lpColumnRect, 
							  HDC hdc, void * pLineData )
{
    if (iColumn < m_iVisColumns) {
		LPCTSTR lpsz = GetColumnText (m_pColumn[ iColumn ]->iCommand,pLineData);
		if ( !RenderData( m_pColumn[iColumn]->iCommand, CRect(lpColumnRect), 
						  *CDC::FromHandle( hdc ), lpsz) ) {
			if (lpsz) {
				DrawColumnText ( hdc, lpColumnRect, lpsz, 
								 m_pColumn[ iColumn ]->cropping, 
								 m_pColumn[ iColumn ]->alignment );
			}
		}
	}
}

void COutliner::PaintLine ( int iLineNo, HDC hdc, LPRECT lpPaintRect )
{
    void * pLineData;
    int iImageWidth = m_pIImage->GetImageWidth ( );
    CRect WinRect;
    GetClientRect(&WinRect);

    int y = m_itemHeight * iLineNo;

    int iColumn, offset;
	CRect rectColumn, rectInter;

	rectColumn.top = y;
	rectColumn.bottom = y + m_itemHeight;

	if ( !( pLineData = AcquireLineData( iLineNo + m_iTopLine )) ) {
		EraseLine ( iLineNo, hdc, lpPaintRect );
		if (ViewerHasFocus() && HasFocus(iLineNo + m_iTopLine)) {
			rectColumn.left = WinRect.left;
			rectColumn.right = WinRect.right;
			DrawFocusRect ( hdc, &rectColumn );
		}
		return;
	}

    HFONT hOldFont =(HFONT) ::SelectObject ( hdc, GetLineFont ( pLineData ) );

    for ( iColumn = offset = 0; iColumn < m_iVisColumns; iColumn++ )
    {
		rectColumn.left = offset;
		rectColumn.right = offset + m_pColumn[ iColumn ]->iCol;

        if ( rectInter.IntersectRect ( &rectColumn, lpPaintRect ) ) {
			::FillRect(hdc, &rectColumn, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
			if ( m_pColumn[ iColumn ]->iCommand == m_idImageCol ) {
				rectColumn.left = DrawPipes ( iLineNo, iColumn, offset, hdc, pLineData );
				rectColumn.left += OUTLINE_TEXT_OFFSET;
			}
            PaintColumn ( iLineNo, iColumn, rectColumn, hdc, pLineData );
		}

        offset += m_pColumn[ iColumn ]->iCol;
    }

    rectColumn.left = offset;
	rectColumn.right = WinRect.right;

	::FillRect(hdc, &rectColumn, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
    PaintColumn ( iLineNo, iColumn, rectColumn, hdc, pLineData );

	rectColumn.left = WinRect.left;
	rectColumn.right = WinRect.right;

	// if we are dragging we and we don't highlight we need to draw the drag line
	if(m_iDragSelection == m_iTopLine + iLineNo && !HighlightIfDragging())
		PaintDragLine(hdc, rectColumn);

	if (ViewerHasFocus() && HasFocus(iLineNo + m_iTopLine)){
		DrawFocusRect ( hdc, &rectColumn );
    }

    ::SelectObject ( hdc, hOldFont );
    ReleaseLineData ( pLineData );
}

void COutliner::PaintDragLine(HDC hdc, CRect &rectColumn)
{

}

BOOL COutliner::IsSelected ( int iLineNo )
{
    return ( iLineNo == m_iSelection );
}

BOOL COutliner::HasFocus ( int iLineNo )
{
    return ( iLineNo == m_iFocus );
}

BOOL COutliner::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{
	if (!CWnd::OnSetCursor( pWnd, nHitTest, message )) {
	    SetCursor ( ::LoadCursor( NULL, IDC_ARROW ) );
	}
    return TRUE;    
}

void COutliner::OnPaint ( )
{
	RECT rcClient;
	GetClientRect(&rcClient);
    CPaintDC pdc ( this );

	HBRUSH hRegBrush = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
    HPEN hRegPen = ::CreatePen( PS_SOLID, 0, GetSysColor ( COLOR_WINDOW ) );
	HBRUSH hHighBrush = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
	HPEN hHighPen = ::CreatePen( PS_SOLID, 0, GetSysColor ( COLOR_HIGHLIGHT ) );

    HBRUSH hOldBrush = (HBRUSH) pdc.SelectObject ( hRegBrush );
    HPEN hOldPen = (HPEN) pdc.SelectObject ( hRegPen );
    COLORREF cOldText = pdc.SetTextColor ( GetSysColor ( COLOR_WINDOWTEXT ) );
    COLORREF cOldBk = pdc.SetBkColor ( GetSysColor ( COLOR_WINDOW ) );

    int i;            
    for ( i = pdc.m_ps.rcPaint.top / m_itemHeight; 
        i < ( pdc.m_ps.rcPaint.bottom / m_itemHeight ) + 1; i++ ) {
			int index = m_iTopLine + i;

            if ( ( IsSelected( index ) && ViewerHasFocus () && m_iDragSelection == -1 ) ||
                 (( index == m_iDragSelection ) && HighlightIfDragging()) ) {
                pdc.SelectObject ( hHighBrush );
                pdc.SelectObject ( hHighPen );
                pdc.SetTextColor ( GetSysColor ( COLOR_HIGHLIGHTTEXT ) );
                pdc.SetBkColor ( GetSysColor ( COLOR_HIGHLIGHT ) );
				m_pIImage->UseHighlight ( );
				m_pIUserImage->UseHighlight( );

                PaintLine ( i, pdc.m_hDC, &pdc.m_ps.rcPaint );

                pdc.SetTextColor ( GetSysColor ( COLOR_WINDOWTEXT ) );
                pdc.SetBkColor ( GetSysColor ( COLOR_WINDOW ) );
                pdc.SelectObject ( hRegBrush);
                pdc.SelectObject ( hRegPen );
				m_pIImage->UseNormal( );
				m_pIUserImage->UseNormal( );
            }
            else    
                PaintLine ( i, pdc.m_hDC, &pdc.m_ps.rcPaint );

 			if ( !ViewerHasFocus() && IsSelected( m_iTopLine + i ) ) {
				RECT rcLine;
				RectFromLine( i, &rcClient, &rcLine);
				DrawFocusRect ( pdc.m_hDC, &rcLine );
			}
       }

    pdc.SetTextColor ( cOldText );
    pdc.SetBkColor ( cOldBk );        
    pdc.SelectObject ( hOldPen );
    pdc.SelectObject ( hOldBrush );

	VERIFY(DeleteObject( hRegBrush ));
	VERIFY(DeleteObject( hRegPen ));
	VERIFY(DeleteObject( hHighBrush ));
	VERIFY(DeleteObject( hHighPen ));
}

int COutliner::OnCreate ( LPCREATESTRUCT lpCreateStruct )
{
    int iRetVal = CWnd::OnCreate ( lpCreateStruct );
    
    m_pDropTarget = CreateDropTarget();
	
    m_pDropTarget->Register(this);
    InitializeClipFormats ( );

	SetCSID(INTL_DefaultWinCharSetID(0)); 

    return iRetVal;
}

COutlinerDropTarget* COutliner::CreateDropTarget()
{
	return new COutlinerDropTarget(this);
}

void COutliner::SetCSID(int csid)
{
    CClientDC pdc ( this );
    LOGFONT lf;

	m_iCSID = csid;

	if (m_hRegFont) {
		theApp.ReleaseAppFont(m_hRegFont);
	}
	if (m_hBoldFont) {
		theApp.ReleaseAppFont(m_hBoldFont);
	}
	if (m_hItalFont) {
		theApp.ReleaseAppFont(m_hItalFont);
	}

    memset(&lf,0,sizeof(LOGFONT));
    lf.lfPitchAndFamily = FF_SWISS;
	lf.lfCharSet = IntlGetLfCharset(csid);
	if (csid == CS_LATIN1)
 		_tcscpy(lf.lfFaceName, "MS Sans Serif");
	else
 		_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(csid));
   	lf.lfHeight = -MulDiv(9,pdc.GetDeviceCaps(LOGPIXELSY), 72);
	m_hRegFont = theApp.CreateAppFont( lf );
    lf.lfWeight = 700;
	m_hBoldFont = theApp.CreateAppFont( lf );
	lf.lfWeight = 0;
	lf.lfItalic = TRUE;
	m_hItalFont = theApp.CreateAppFont( lf );
    TEXTMETRIC tm;

    HFONT pOldFont = (HFONT) pdc.SelectObject ( m_hBoldFont );
    pdc.GetTextMetrics ( &tm );
    pdc.SelectObject(pOldFont);
    m_cxChar = tm.tmAveCharWidth;
    m_cyChar = tm.tmHeight + tm.tmExternalLeading;
    m_itemHeight = m_cyChar;
    if ( m_pIImage )
        InitializeItemHeight(max(m_pIImage->GetImageHeight(),m_itemHeight));

	Invalidate();

	m_pTip->SetCSID(m_iCSID);
}


int COutliner::LineFromPoint (POINT point)
{
	int iLine = m_iTopLine + (point.y / m_itemHeight);

	// Figure out which portion of the line the point is on.
	int iDiff = point.y - ((iLine - m_iTopLine) *m_itemHeight);
	
	int iOldLineHalf = m_iDragSelectionLineHalf, iOldLineThird = m_iDragSelectionLineThird;
	m_iDragSelectionLineHalf = (iDiff <= m_itemHeight / 2) ? 1 : 2;
	m_iDragSelectionLineThird = (iDiff <= m_itemHeight / 3) ? 1 : (iDiff <= 2 * (m_itemHeight/3)) ? 2 : 3;
    
	if(m_iTopLine !=0)
		int i =0;
	m_bDragSectionChanged = (iOldLineHalf != m_iDragSelectionLineHalf || iOldLineThird !=m_iDragSelectionLineThird);

	return ( iLine );
}

DROPEFFECT COutliner::DropSelect (int iLineNo, COleDataObject *object )
{
    if (iLineNo >= m_iTotalLines)
        return DROPEFFECT_NONE;
    else if (iLineNo == m_iDragSelection && !m_bDragSectionChanged)
        return DROPEFFECT_MOVE;

    if (m_iDragSelection != -1)
        InvalidateLine (m_iDragSelection);

    m_iDragSelection = iLineNo;
    InvalidateLine (m_iDragSelection);
    
    return DROPEFFECT_MOVE;
}

void COutliner::EndDropSelect(void)
{
    if (m_iDragSelection != -1) {
        m_iDragSelection = -1;
		m_iDragSelectionLineHalf = -1;
		m_iDragSelectionLineThird = -1;
		m_bDragSectionChanged = FALSE;
		Invalidate();
		UpdateWindow();
    }
}

COleDataSource *COutliner::GetDataSource(void)
{
	return NULL;
}

void COutliner::InitiateDragDrop(void)
{
    m_bDraggingData = TRUE;
    COleDataSource * pDataSource = GetDataSource();
    if ( pDataSource) {
        DROPEFFECT res = pDataSource->DoDragDrop
            (DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE | DROPEFFECT_SCROLL);
        pDataSource->Empty();
        delete pDataSource;
    }
    m_bDraggingData = FALSE;
}

BOOL COutliner::RecognizedFormat (COleDataObject * pDataObject)
{
    CLIPFORMAT * pFormatList = GetClipFormatList();
    if (pFormatList != NULL) {
        while (*pFormatList) {
            if (pDataObject->IsDataAvailable(*pFormatList))
                return TRUE;
            pFormatList++;
        }
    }
    return FALSE;
}

void COutliner::AcceptDrop(int iLineNo, class COleDataObject *pDataObject, 
						   DROPEFFECT dropEffect)
{

}

CLIPFORMAT * COutliner::GetClipFormatList(void)
{
    return NULL;
}    

void COutliner::OnSelChanged()
{
}

void COutliner::OnSelDblClk()
{
}

#if defined(XP_WIN32) && _MSC_VER >= 1100
LONG COutliner::OnHackedMouseWheel(WPARAM wParam, LPARAM lParam)
{
    return(OnMouseWheel(wParam << 16, lParam));
}

LONG COutliner::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    //  Increase the delta.
    m_iWheelDelta += MOUSEWHEEL_DELTA(wParam, lParam);

    //  Number of lines to scroll.
    UINT uScroll = intelli.ScrollLines();

    //  Direction.
    BOOL bForward = TRUE;
    if(m_iWheelDelta < 0)   {
        bForward = FALSE;
    }

    //  Scroll bar code to use.
    UINT uSBCode = SB_LINEUP;

    if(m_iWheelDelta / WHEEL_DELTA)  {
        if(uScroll == WHEEL_PAGESCROLL)   {
            if(bForward)    {
                uSBCode = SB_PAGEUP;
            }
            else    {
                uSBCode = SB_PAGEDOWN;
            }
            uScroll = 1;
        }
        else    {
            if(bForward)    {
                uSBCode = SB_LINEUP;
            }
            else    {
                uSBCode = SB_LINEDOWN;
            }
        }

        //  Take off scroll increment.
        UINT uLoops = 0;
        while(m_iWheelDelta / WHEEL_DELTA)  {
            if(bForward)   {
                m_iWheelDelta -= WHEEL_DELTA;
            }
            else    {
                m_iWheelDelta += WHEEL_DELTA;
            }
            uLoops++;
        }

        //  Do it.
        if(uLoops)        {
            OnVScroll(uSBCode, 0, NULL);
        }
    }

    return(1);
}
#endif

//
// rhp - Added for QA Partner hooks - NO NEED TO I18N/L10N SINCE THIS 
// IS FOR QA WORK ONLY!!!
//
#define		NULL_STRING "<Null>"

LPCSTR
NullIt(LPCTSTR inVar)
{
	if (!inVar)
		return NULL_STRING;

	if (inVar[0] == '\0')
		return NULL_STRING;

	return inVar;
}

// This is the result of a WM_COPYDATA message. This will be used to send requests
// into Communicator to get data out of Outliner objects as well as drive the 
// application for automated testing. The return codes of these call will either be
// the data/number asked for or just a status of processing the request and getting
// the information into the share memory segment.
//
// The description of the parameters coming into this call are:
//
//    wParam = (WPARAM) (HWND) hwnd;            // handle of sending window 
//    lParam = (LPARAM) (PCOPYDATASTRUCT) pcds; // pointer to structure with data 
//
//		typedef struct tagCOPYDATASTRUCT {  // cds  
//			DWORD dwData;			// the ID of the request
//			DWORD cbData;			// the size of the argument
//			PVOID lpData;			// the data (i.e. the pointer/name of shared mem)
//		} COPYDATASTRUCT; 
//
//    The lpData will be NULL for all Win32 calls and it will be a pointer to
//    the (CSharedData *) structure to fill on Win16
//
// Returns: This is call specific. For calls that only return numeric values,
// the return code is results. For calls that return buffers of data, this is a 
// status code witht the following meaning:
//
//			0 - Problem with the processing.
//          1 - Everything is ok.
//
LONG COutliner::OnProcessOLQAHook(WPARAM wParam, LPARAM lParam)
{
	CSharedData	*memChunk = NULL;

	PCOPYDATASTRUCT	 pcds = (PCOPYDATASTRUCT) lParam;
	if (lParam == NULL)
	{
		return(0);
	}

	TRACE("OLQAHook Message ID = %d\n", pcds->dwData);
	switch (pcds->dwData) 
	{
	case QA_OLGETCOUNT:
		return(GetTotalLines());
		break;

	case QA_OLGETVISIBLECOUNT:
		return(GetVisibleColumns());
		break;

	case QA_OLGETFOCUSLINE:
		return(GetFocusLine());
		break;

  case QA_OLGETCOLCOUNT:
		return(GetNumColumns());
    break;

  // rhp - new code as of 12/2
  case QA_OLGETISCOLLAPSED:
    {
      DWORD	*dwPtr;
      
      dwPtr = (DWORD *)pcds->lpData;
      return(IsCollapsed( *dwPtr ));
      break;
    }

  case QA_OLGETNUMCHILDREN:
    {
      DWORD	*dwPtr;
      
      dwPtr = (DWORD *)pcds->lpData;
      return(GetNumChildren( *dwPtr ));
      break;
    }

  case QA_OLSCROLLINTOVIEW:
    {
      DWORD	*dwPtr;
      
      dwPtr = (DWORD *)pcds->lpData;
      ScrollIntoView( *dwPtr );
      return(1);
    }

	case QA_OLSETFOCUSLINE:
		{
		DWORD	*dwPtr;

			dwPtr = (DWORD *)pcds->lpData;
			SetFocusLine(*dwPtr);
			SelectItem( *dwPtr );
			return (1);
			break;
		}
	case QA_OLGETTEXT:
		{
#ifdef WIN32
			memChunk = OpenExistingSharedMemory();
#else
			memChunk = (CSharedData *) pcds->lpData;
#endif
			if (!memChunk)
			{
				return(0);
			}

			DWORD	i;
			DWORD	lineNumber = GetFocusLine();
			char	workLine[8192];
			DWORD	copySize;
			void	*pLineData = AcquireLineData ( lineNumber ); // Make this a parameter...

			memset(workLine, 0, sizeof(workLine));
			for (i=0; i<GetNumColumns(); i++)
			{
				LPCTSTR colText = GetColumnText ( m_pColumn[ i ]->iCommand, pLineData );
				if (!colText)
					continue;

				lstrcat(workLine, NullIt(m_pColumn[ i ]->pHeader));
				lstrcat(workLine, "=");

				lstrcat(workLine, NullIt(colText));
				lstrcat(workLine, "|");
			}

			ReleaseLineData(pLineData);

			//
			// Make sure we don't do something stupid!
			//
			if (strlen((char *)workLine) < memChunk->m_dwSize)
			{
				copySize = strlen((char *)workLine);
			}
			else
			{
				copySize = memChunk->m_dwSize;
			}

			strncpy((char *) memChunk->m_buf, (char *)workLine, copySize);
			memChunk->m_buf[copySize] = '\0';
			memChunk->m_dwBytesUsed = copySize + 1;

			CloseSharedMemory();
			break;
		}

	default:
		break;
	}

	return(1);
}
// rhp

//////////////////////////////////////////////////////////////////////////////
// CMSelectOutliner

CMSelectOutliner::CMSelectOutliner()
{
	m_bNoMultiSel = FALSE;
	m_iShiftAnchor = -1;
	m_iIndicesSize = 64;
	m_pIndices = new MSG_ViewIndex[m_iIndicesSize];
	m_iIndicesCount = 0;
}

CMSelectOutliner::~CMSelectOutliner()
{
	delete [] m_pIndices;
}


void CMSelectOutliner::ClearSelection()
{
	int i;
	for (i = 0; i < m_iIndicesCount; i++ ) {
		InvalidateLine(CASTINT(m_pIndices[i]));
	}
	m_iIndicesCount = 0;
}

void CMSelectOutliner::GetSelection( const MSG_ViewIndex *&indices, int &count )
{
	indices = m_pIndices;
	count = m_iIndicesCount;
}

void CMSelectOutliner::AddSelection( MSG_ViewIndex iSel )
{
	int i;

	for (i = 0; i < m_iIndicesCount; i++) {
		if ( m_pIndices[i] == iSel ) {
			return;
		}
	}

	if (i >= m_iIndicesSize) {
		// Resize storage array (exponential grow)
		MSG_ViewIndex *tmp = new MSG_ViewIndex[m_iIndicesSize * 2];
		memcpy(tmp, m_pIndices, m_iIndicesSize * sizeof(MSG_ViewIndex));
		delete [] m_pIndices;
		m_pIndices = tmp;
		m_iIndicesSize *= 2;
	}
	if (iSel >= 0 && iSel < m_iTotalLines) {
		m_pIndices[i] = iSel;
		m_iIndicesCount++;
	}

	InvalidateLine( CASTINT(iSel) );
}

void CMSelectOutliner::RemoveSelection( MSG_ViewIndex iSel )
{
	int i, j;

	for (i = 0, j = 0; i < m_iIndicesCount; i++) {
		if (m_pIndices[i] == iSel) {
			InvalidateLine( CASTINT(iSel) );
		} else {
			m_pIndices[j] = m_pIndices[i];
			j++;
		}
	}
	m_iIndicesCount = j;
	m_iSelection = -1;	   
	m_iFocus = -1;		   
}

void CMSelectOutliner::RemoveSelectionRange( MSG_ViewIndex iSelBegin, MSG_ViewIndex iSelEnd )
{
	int i, j;
	if ( iSelBegin > iSelEnd ) {
		MSG_ViewIndex tmp;
		tmp = iSelEnd;
		iSelEnd = iSelBegin;
		iSelBegin = tmp;
	}
	for (i = 0, j = 0; i < m_iIndicesCount; i++ ) {
		if (m_pIndices[i] >= iSelBegin && m_pIndices[i] <= iSelEnd) {
			InvalidateLine( CASTINT(m_pIndices[i]) );
		} else {
			m_pIndices[j] = m_pIndices[i];
			j++;
		}
	}
	m_iIndicesCount = j;
	m_iSelection = -1;	   
	m_iFocus = -1;		   
}

BOOL CMSelectOutliner::IsSelected ( int iLine )
{
	int i;
	for (i = 0; i < m_iIndicesCount; i++) {
		if ( (int) m_pIndices[i] == iLine ) {
			return TRUE;
		}
	}
	return FALSE;
}

void CMSelectOutliner::SelectRange( MSG_ViewIndex iSelBegin, MSG_ViewIndex iSelEnd )
{
	// WHS - we may want to optimize this
	MSG_ViewIndex i;

	if ( iSelBegin > iSelEnd ) {
		MSG_ViewIndex tmp;
		tmp = iSelEnd;
		iSelEnd = iSelBegin;
		iSelBegin = tmp;
	}

	for ( i = iSelBegin; i <= iSelEnd; i++ ) {
		AddSelection(i);
	}
}

BOOL CMSelectOutliner::HandleInsert( MSG_ViewIndex iStart, int32 iCount )
{
	int i;
	for (i = 0; i < m_iIndicesCount; i++) {
		if ( m_pIndices[i] >= iStart ) {
			m_pIndices[i] += iCount; // Shift over to make room
		}
	}
	if (m_iSelection >= (int) iStart ) {
		m_iSelection += CASTINT(iCount);
	}
	if (m_iFocus >= (int) iStart ) {
		m_iFocus += CASTINT(iCount);
	}
	if (m_iShiftAnchor >= (int) iStart ) {
		m_iShiftAnchor += CASTINT(iCount);
	}

	m_iTotalLines += CASTINT(iCount);

	// Invalidate previous line so pipes are drawn correctly
	if ( iStart ) {
		iStart--;
		iCount++;
	}
	InvalidateLines( CASTINT(iStart), CASTINT(m_iTotalLines - iStart + iCount));

	return FALSE;
}

BOOL CMSelectOutliner::HandleDelete( MSG_ViewIndex iStart, int32 iCount )
{
	BOOL res = FALSE;
	int i, j;

	for (i = 0, j = 0; i < m_iIndicesCount; i++) {
		if ( m_pIndices[i] >= iStart ) {
			if (m_pIndices[i] < (iStart + iCount)) {
				res = TRUE;
				continue;
			} else {
				m_pIndices[i] -= iCount; // Shift into gap
			}
		}
		m_pIndices[j] = m_pIndices[i];
		j++;
	}
	m_iIndicesCount = j;
	if (m_iSelection >= int(iStart) ) {
		if (m_iSelection < int(iStart + iCount))
			m_iSelection = int(iStart);
		else
			m_iSelection -= CASTINT(iCount);

		if (m_iSelection >= m_iTotalLines - iCount)
			m_iSelection = CASTINT(m_iTotalLines - iCount - 1);

		if (m_iSelection < 0 )
			m_iSelection = (m_iTotalLines - iCount) > 0 ? 0 : -1;
	}
	if (m_iFocus >= int(iStart) ) {
		if (m_iFocus < int(iStart + iCount))
			m_iFocus = int(iStart);
		else
			m_iFocus -= CASTINT(iCount);

		if (m_iFocus >= m_iTotalLines - iCount)
			m_iFocus = CASTINT(m_iTotalLines - iCount - 1);

		if (m_iFocus < 0)
			m_iFocus = 0;
	}
	if (m_iShiftAnchor >= int(iStart) ) {
		if (m_iShiftAnchor < int(iStart + iCount))
			m_iShiftAnchor = int(iStart);
		else
			m_iShiftAnchor -= CASTINT(iCount);

		if (m_iShiftAnchor >= m_iTotalLines - iCount)
			m_iShiftAnchor = CASTINT(m_iTotalLines - iCount - 1);

		if (m_iShiftAnchor < 0)
			m_iShiftAnchor = 0;
	}

	m_iTotalLines -= CASTINT(iCount);

	if (!IsSelected(m_iSelection) && (m_iTotalLines > 0))
		AddSelection( m_iSelection );

	// Invalidate previous line so pipes are drawn correctly
	if ( iStart ) {
		iStart--;
		iCount++;
	}
	InvalidateLines(CASTINT(iStart), CASTINT(m_iTotalLines - iStart + iCount));

	return res;
}

void CMSelectOutliner::HandleScramble()
{
	ASSERT(0); // nothing we can do.
}

void CMSelectOutliner::PositionHome( )
{
	int iOldFocus = m_iFocus;

	COutliner::PositionHome();

	if (iOldFocus != m_iFocus ) {
		ClearSelection();
		AddSelection(m_iFocus);
	}
}

void CMSelectOutliner::PositionEnd( )
{
	int iOldFocus = m_iFocus;

	COutliner::PositionEnd();

	if (iOldFocus != m_iFocus ) {
		ClearSelection();
		AddSelection(m_iFocus);
	}
}

void CMSelectOutliner::PositionPrevious( )
{
	int iOldFocus = m_iFocus;

	COutliner::PositionPrevious();

	if (iOldFocus != m_iFocus ) {
		ClearSelection();
		AddSelection(m_iFocus);
	}
}

void CMSelectOutliner::PositionNext( )
{
	int iOldFocus = m_iFocus;

	COutliner::PositionNext();

	if (iOldFocus != m_iFocus ) {
		ClearSelection();
		AddSelection(m_iFocus);
	}
}

void CMSelectOutliner::PositionPageDown( )
{
	int iOldFocus = m_iFocus;

	COutliner::PositionPageDown();

	if (iOldFocus != m_iFocus ) {
		ClearSelection();
		AddSelection(m_iFocus);
	}
}

void CMSelectOutliner::PositionPageUp( )
{
	int iOldFocus = m_iFocus;

	COutliner::PositionPageUp();

	if (iOldFocus != m_iFocus ) {
		ClearSelection();
		AddSelection(m_iFocus);
	}
}

void CMSelectOutliner::SetTotalLines ( int iLines )
{
	COutliner::SetTotalLines( iLines );
}

void CMSelectOutliner::SelectItem( int iSel, int mode, UINT flags )
{
	if ( m_bNoMultiSel ) {
		flags = 0;
	}

	if ( (iSel >= -1) || (iSel < GetTotalLines()) ) {

		if ( !(flags & MK_SHIFT) || (m_iShiftAnchor == -1 ) )
			m_iShiftAnchor = iSel;

		switch ( mode ) {
		case OUTLINER_LBUTTONDOWN:
			m_bClearOnRelease = FALSE;
			m_bSelectOnRelease = FALSE;

			InvalidateLine( m_iFocus );
			m_iFocus = m_iSelection = iSel;
			InvalidateLine( m_iFocus );

			if ( IsSelected(iSel) ) {
				if ( flags & MK_CONTROL ) {
					m_bClearOnRelease = TRUE;
				} else if ( !( flags & MK_SHIFT) ){
					m_bSelectOnRelease = TRUE;
				}
			} else {
				if ( !(flags & MK_CONTROL) ) {
					ClearSelection();
				}
			}

			if ( flags & MK_SHIFT ) {
				ClearSelection();
				SelectRange( m_iShiftAnchor, m_iSelection);
			} else {
				AddSelection(m_iSelection);
			}
			break;

		case OUTLINER_LBUTTONUP:
			if (m_bClearOnRelease) {
				RemoveSelection(iSel);
			}
			if ( m_bSelectOnRelease ) {
				ClearSelection();
				AddSelection(iSel);
			}
			OnSelChanged();
            m_iLastSelected = iSel;	    
			m_bClearOnRelease = FALSE;
			m_bSelectOnRelease = FALSE;
			break;

		case 0:
			ASSERT(0);

        case OUTLINER_KEYDOWN:
			InvalidateLine( m_iFocus );
			m_iFocus = m_iSelection = iSel;
			InvalidateLine( m_iFocus );
			ClearSelection();
			if ( flags & MK_SHIFT ) {
				SelectRange( m_iShiftAnchor, m_iSelection);
			} else {
				AddSelection(m_iSelection);
			}
            if (m_iLastSelected == -1)
            {
                OnSelChanged();
                m_iLastSelected = m_iSelection;
            }
			m_bClearOnRelease = FALSE;
			m_bSelectOnRelease = FALSE;
			break;

		case OUTLINER_RBUTTONDOWN:
			if (!IsSelected( iSel )) {
				InvalidateLine( m_iFocus );
				m_iFocus = m_iSelection = iSel;
				InvalidateLine( m_iFocus );
				ClearSelection();
				AddSelection( m_iSelection );
				OnSelChanged();
				m_iLastSelected = m_iSelection;
			}
			break;

		case OUTLINER_SET:
			InvalidateLine( m_iFocus );
			m_iFocus = m_iSelection = iSel;
			InvalidateLine( m_iFocus );
			if (!(flags & MK_CONTROL)) {
				ClearSelection();
			}
			AddSelection(m_iSelection);
			m_bClearOnRelease = FALSE;
			m_bSelectOnRelease = FALSE;
        
		case OUTLINER_TIMER:
            OnSelChanged();
            m_iLastSelected = m_iSelection;
            break;

		case OUTLINER_RETURN:
			if (!IsSelected(iSel)) {
				InvalidateLine( m_iFocus );
				m_iFocus = m_iSelection = iSel;
				InvalidateLine( m_iFocus );
				if (!(flags & MK_CONTROL)) {
					ClearSelection();
				}
				AddSelection(m_iSelection);
				break;
			}

		case OUTLINER_LBUTTONDBLCLK:
			OnSelDblClk();
			m_bClearOnRelease = FALSE;
			m_bSelectOnRelease = FALSE;
			break;

		default:
			ASSERT("What kind of garbage are you passing me?" == NULL);
		}
		UpdateWindow();
	}
}

void CMSelectOutliner::SelectRange( int iStart, int iEnd, BOOL bNotify )
{
	if ( iStart == -1 ) {
		ClearSelection();
	} else { 
		if (iEnd == -1) {
			iEnd = m_iTotalLines - 1;
		}
		SelectRange( iStart, iEnd );
	}
	if ( bNotify )
		OnSelChanged();
}

//////////////////////////////////////////////////////////////////////////////
// COutlinerDropTarget

COutlinerDropTarget::COutlinerDropTarget( COutliner *pOutliner )
{
	ASSERT(pOutliner);
	m_pOutliner = pOutliner;
	m_dwOldTicks = 0;
}

void COutlinerDropTarget::DragScroll( BOOL bBackwards )
{
    DWORD dwTicks = GetTickCount();
    if (!dwTicks || (dwTicks - m_dwOldTicks) > m_pOutliner->GetDragHeartbeat()) {
		if (dwTicks) {
			m_pOutliner->OnVScroll(bBackwards ? SB_LINEUP : SB_LINEDOWN, 0, 0);
			m_pOutliner->UpdateWindow();
		}
        m_dwOldTicks = dwTicks;
    }
}

BOOL COutlinerDropTarget::OnDrop(
    CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	m_dwOldTicks = 0;

    if(!pDataObject || !m_pOutliner)
        return(FALSE);

    if (!m_pOutliner->RecognizedFormat(pDataObject))
        return FALSE;

	m_pOutliner->m_ptHit = point;

    m_pOutliner->AcceptDrop( m_pOutliner->GetDropLine(), pDataObject, dropEffect);
    m_pOutliner->EndDropSelect();

    return(TRUE);
}


DROPEFFECT COutlinerDropTarget::OnDragEnter(
    CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT res = OnDragOver(pWnd, pDataObject, dwKeyState, point);

	if ( res != DROPEFFECT_NONE) {
		m_pOutliner->Invalidate();
		m_pOutliner->UpdateWindow();
	}

	return res;
}

DROPEFFECT COutlinerDropTarget::OnDragOver(
    CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
    DROPEFFECT effect = DROPEFFECT_NONE;

    if (m_pOutliner->RecognizedFormat(pDataObject)) {
		RECT rect;
		pWnd->GetClientRect(&rect);

		if ( point.y < m_pOutliner->m_itemHeight ) {
			DragScroll(TRUE);
			effect |= DROPEFFECT_SCROLL;
		} else if ( point.y > ( rect.bottom - m_pOutliner->m_itemHeight ) ) {
			DragScroll(FALSE);
			effect |= DROPEFFECT_SCROLL;
		} else {
			m_dwOldTicks = 0;
		}

		m_pOutliner->m_ptHit = point;

		effect |= m_pOutliner->DropSelect(m_pOutliner->LineFromPoint(point), pDataObject);

		// Default is to move
		if ( ( effect & DROPEFFECT_MOVE) && (dwKeyState & MK_CONTROL) )
			effect = (effect & ~DROPEFFECT_MOVE) | DROPEFFECT_COPY;
	}
    return effect;
}

void COutlinerDropTarget::OnDragLeave(CWnd* pWnd)
{
    m_pOutliner->EndDropSelect();
}
                                           
#ifdef _WIN32
DROPEFFECT COutlinerDropTarget::OnDragScroll(
    CWnd* pWnd, DWORD dwKeyState, CPoint point)
#else	
BOOL COutlinerDropTarget::OnDragScroll(
    CWnd* pWnd, DWORD dwKeyState, CPoint point)
#endif
{
	return COleDropTarget::OnDragScroll(pWnd, dwKeyState, point);
}
//////////////////////////////////////////////////////////////////////////////
// COutlinerParent

void COutlinerParent::SetOutliner ( COutliner * pOutliner ) 
{ 
    m_pOutliner = pOutliner; 
}

void COutlinerParent::InvalidatePusher()
{
	RECT rect;
	GetClientRect(&rect);
	rect.left = rect.right - m_iPusherWidth;
	rect.bottom = m_iHeaderHeight;
	InvalidateRect(&rect);
}

int COutlinerParent::TestPusher( POINT &pt )
{
	RECT rect;
	GetClientRect(&rect);
	rect.left = rect.right - m_iPusherWidth;
	rect.bottom = m_iHeaderHeight;
	if (::PtInRect(&rect, pt)) {
		switch(m_iPusherState) {
		case pusherLeft:
		case pusherRight:
		case pusherLeftRight:
			rect.right = rect.left + (rect.right - rect.left) / 2;
			if (::PtInRect(&rect, pt)) {
				return pusherLeft;
			} else {
				return pusherRight;
			}
		}
	}
	return pusherNone;
}

BEGIN_MESSAGE_MAP(COutlinerParent, CWnd)
    ON_WM_PAINT ( )
    ON_WM_CREATE ( )
    ON_WM_SIZE ( )    
    ON_WM_SETFOCUS ( )
    ON_WM_KEYDOWN ( )
    ON_WM_ERASEBKGND ( )
    ON_WM_MOUSEMOVE ( )
    ON_WM_LBUTTONDOWN ( )
    ON_WM_LBUTTONUP ( )
    ON_WM_SETCURSOR ( )
END_MESSAGE_MAP()

class CNSOutlinerParentFactory :  public  CGenericFactory
{
public:
    CNSOutlinerParentFactory();
    ~CNSOutlinerParentFactory();
    STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj);
};

CNSOutlinerParentFactory::CNSOutlinerParentFactory()
{
   ApiApiPtr(api);
	api->RegisterClassFactory(APICLASS_OUTLINERPARENT,this);
}

CNSOutlinerParentFactory::~CNSOutlinerParentFactory()
{
}

STDMETHODIMP CNSOutlinerParentFactory::CreateInstance(
    LPUNKNOWN pUnkOuter,
    REFIID refiid,
    LPVOID * ppvObj)
{
    COutlinerParent * pParent = new COutlinerParent;
    *ppvObj = (LPVOID)((LPUNKNOWN)pParent);
    return NOERROR;
}

DECLARE_FACTORY(CNSOutlinerParentFactory);

COutlinerParent::COutlinerParent ( ) 
{
	m_bResizeArea = FALSE;
    m_bResizeColumn = FALSE;
	m_bHeaderSelected = FALSE;
	m_bDraggingHeader = FALSE;
    m_idColHit = 0;
	m_iColHit = -1;
	m_iColLoser = -1;
    m_bHasBorder = FALSE;
    m_bDisableHeaders = FALSE;
	m_bEnableFocusFrame = FALSE;
    m_pOutliner = NULL;
	m_hToolFont = NULL;
	m_hbmDrag = NULL;
	m_hdcDrag = NULL;
    m_iHeaderHeight = 0;
	m_iPusherRgn = pusherNone;
	m_iPusherHit = pusherNone;

    ApiApiPtr(api);
    m_pUnkImage = api->CreateClassInstance(
        APICLASS_IMAGEMAP,NULL,(APISIGNATURE)IDB_COLUMN);
    m_pUnkImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIImage);
    ASSERT(m_pIImage);
    if (!m_pIImage->GetResourceID())
        m_pIImage->Initialize(IDB_COLUMN,8,8);
}

COutlinerParent::~COutlinerParent ( )
{
	if (m_hToolFont) {
		theApp.ReleaseAppFont(m_hToolFont);
	}
	if ( m_hdcDrag )
		VERIFY(::DeleteDC( m_hdcDrag ));
	if ( m_hbmDrag )
		VERIFY(::DeleteObject( m_hbmDrag ));

    delete m_pOutliner;
    if (m_pUnkImage) {
        if (m_pIImage)
            m_pUnkImage->Release();
    }
}

BOOL COutlinerParent::ResizeClipCursor()
{
    CFont *pOldFont = NULL;
    CDC *pDC = m_pOutliner->GetDC();
    if( GetFont() ) 
    {
        pOldFont = pDC->SelectObject( GetFont() );
    }
	CSize csMinWidth = pDC->GetTextExtent( "W", _tcslen( "W" ) );
    csMinWidth.cx++;
    if( pOldFont ) 
    {
        pDC->SelectObject( pOldFont );
    }
    m_pOutliner->ReleaseDC( pDC );

    int iLeft = csMinWidth.cx; //max( m_pOutliner->m_pColumn[m_iColHit]->iMinColSize, csMinWidth.cx );
    int iRight = 0;
    for( int i = 0; i < m_pOutliner->m_iVisColumns; i++ )
    {
        if( i < m_iColHit )
        {
    		iLeft += m_pOutliner->m_pColumn[i]->iCol;
        }
        else if( i > m_iColHit )
        {
            if( m_pOutliner->m_pColumn[i]->cType == ColumnVariable )
            {
                iRight += csMinWidth.cx; //max( m_pOutliner->m_pColumn[i]->iMinColSize, csMinWidth.cx );
            }
            else
            {
                iRight += m_pOutliner->m_pColumn[i]->iCol;
            }
        }
	}
    iRight = m_pOutliner->m_iTotalWidth - iRight;
    if( iRight < iLeft )
    {
       iRight = iLeft;
    }
    RECT rcClip = { iLeft, 0, iRight, m_iHeaderHeight };
    MapWindowPoints( CWnd::FromHandle( HWND_DESKTOP ), &rcClip );
    
   #ifdef _WIN32
    return ClipCursor( &rcClip );
   #else
    return TRUE;
   #endif
}

BOOL COutlinerParent::TestCol( POINT &pt, int &iCol )
{
	iCol = -1;
    int i;
	BOOL res = FALSE;
    RECT rc, rcResize, rcClient;

	m_bResizeArea = FALSE;

	GetClientRect(&rcClient);

	rc.top = rcResize.top = 0;
	rc.bottom = rcResize.bottom = m_iHeaderHeight;
	rc.left = 0;
	rc.right = 0;

    for ( i = 0; i < m_pOutliner->m_iVisColumns; i++ )
    {
		rc.left = rc.right;
		rc.right = rc.left + m_pOutliner->m_pColumn[ i ]->iCol;

		if (rc.right > (rcClient.right - m_iPusherWidth)) {
			rc.right = rcClient.right - m_iPusherWidth;
		}

		if ( i < m_pOutliner->m_iVisColumns - 1 ) {
			rcResize.left = rc.right - 4;
			rcResize.right = rc.right + 4;

			if ( ::PtInRect( &rcResize, pt ) ) {
				m_bResizeArea = TRUE;
				m_iColResize = i;
			}
		}

		if ( ::PtInRect( &rc, pt ) ) {
			m_rcTest = rc;
			iCol = i;
			res = TRUE;
		}
	}
	return res;
}

void COutlinerParent::GetColumnRect( int iCol, RECT &rc )
{
	GetClientRect(&rc);
	if (m_bEnableFocusFrame)
	{
		rc.top = 1;
		rc.left = 1;
		rc.right = 1;
	}
	else
	{
		rc.left = 0;
		rc.right = 0;
	}
	rc.bottom = m_iHeaderHeight;
	if ( iCol >= 0 && iCol < m_pOutliner->m_iVisColumns ) {
		for (int i = 0; i <= iCol; i++) {
			rc.left = rc.right;
			rc.right += m_pOutliner->m_pColumn[ i ]->iCol;
		}
	}
}

void COutlinerParent::InvalidateColumn( int iCol )
{
	RECT rc;
	GetColumnRect( iCol, rc );
	InvalidateRect( &rc );
}

void COutlinerParent::UpdateFocusFrame()
{
	if (m_bEnableFocusFrame)
	{
		RECT clientRect;
		GetClientRect(&clientRect);
		InvalidateRect(&clientRect, FALSE);
		::InflateRect(&clientRect, -1, -1);
		clientRect.top = m_iHeaderHeight + 1;
		ValidateRect(&clientRect);
		UpdateWindow();
	}
}

BOOL COutlinerParent::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
		m_wndTip.RelayEvent(pMsg);
    
    return CWnd::PreTranslateMessage(pMsg);
}

int COutlinerParent::OnCreate ( LPCREATESTRUCT lpCreateStruct )
{
    int iRetVal = (int) Default();

    if (m_pOutliner = GetOutliner ( ) ) {
        CClientDC pdc ( this );
		int m_rescsid = INTL_CharSetNameToID(INTL_ResourceCharSet());
        LOGFONT lf;
        memset(&lf,0,sizeof(LOGFONT));
        lf.lfPitchAndFamily = FF_SWISS;
		lf.lfCharSet = IntlGetLfCharset(m_rescsid);  // Global lfCharSet
		if (m_rescsid == CS_LATIN1)
			strcpy(lf.lfFaceName, "MS Sans Serif");
		else
 			strcpy(lf.lfFaceName, IntlGetUIPropFaceName(m_rescsid));
        lf.lfHeight = -MulDiv(9,pdc.GetDeviceCaps(LOGPIXELSY), 72);
		m_hToolFont = theApp.CreateAppFont( lf );

        TEXTMETRIC tm;
        HFONT hOldFont = (HFONT) pdc.SelectObject ( m_hToolFont );
        pdc.GetTextMetrics ( &tm );
        pdc.SelectObject ( hOldFont );

        m_iHeaderHeight = tm.tmHeight + tm.tmExternalLeading + 4;
	    m_cxChar = tm.tmAveCharWidth;

		LPCTSTR lpszClass = NULL;
        m_pOutliner->CreateEx ( 0, lpszClass, NULL, 
            ( m_bHasBorder ? WS_BORDER : 0 )|WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 
								0, 0, 0, 0, this->m_hWnd, lpCreateStruct->hMenu );

		m_wndTip.Create(this);
		m_wndTip.AddTool(this, IDS_SHOWHIDECOLUMNS, CRect(0,0,0,0), 1);
		m_wndTip.Activate(TRUE);
    } else {
		iRetVal = -1;
	}
	m_iPusherWidth = GetSystemMetrics(SM_CXVSCROLL);
	m_iPusherWidth = m_iPusherWidth > 20 ? m_iPusherWidth : 20;

    return iRetVal;
}

void COutlinerParent::OnSize ( UINT nType, int cx, int cy )
{
    if ( m_pOutliner )
    {
		m_pOutliner->m_iTotalWidth = cx - m_iPusherWidth;
		if (m_bEnableFocusFrame)
		{
			m_pOutliner->MoveWindow ( 1,
									  m_bDisableHeaders ? 0 : m_iHeaderHeight, 
									  cx - 2,
									  cy - ( m_bDisableHeaders ? 1 : m_iHeaderHeight + 1 ), TRUE );
		}
		else
		{
			m_pOutliner->MoveWindow ( 0,
									  m_bDisableHeaders ? 0 : m_iHeaderHeight, 
									  cx,
									  cy - ( m_bDisableHeaders ? 0 : m_iHeaderHeight ), TRUE );
		}
		m_wndTip.SetToolRect(this, 1, CRect(cx - m_iPusherWidth, 0, cx, m_iHeaderHeight));
    }
}

void COutlinerParent::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags )
{
    if ( m_pOutliner )
        m_pOutliner->OnKeyDown ( nChar, nRepCnt, nFlags );
	else
	    Default();
}

void COutlinerParent::OnSetFocus ( CWnd * pOldWnd )
{
    Default();

    if ( m_pOutliner )
        m_pOutliner->SetFocus ( );
}

void COutlinerParent::OnLButtonDown( UINT nFlags, CPoint point )
{
    if ( m_pOutliner )
    {
        SetCapture ( );
		m_ptHit = point;

        if( m_bResizeArea ) 
        {
            m_bResizeColumn = TRUE;
            
            ResizeClipCursor();
        } 
        else 
        {
			int iCol = 0;
			if (TestCol( point, iCol )) {
				m_rcDrag = m_rcHit = m_rcTest;
				m_bHeaderSelected = TRUE;
				m_iColHit = iCol;
				m_idColHit = m_pOutliner->m_pColumn[ iCol ]->iCommand;

                if ( m_pOutliner->m_pColumn[ iCol ]->bIsButton ) {
                     m_pOutliner->m_pColumn[ iCol ]->bDepressed = TRUE;
                    InvalidateColumn ( iCol );
                }
            }
			int iTestPush = TestPusher(point);
			if (iTestPush & m_iPusherState) {
				m_iPusherHit = m_iPusherRgn = iTestPush;
				InvalidatePusher();
				UpdateWindow();
			}
        }
    }
	Default();
}

void COutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	if ( GetCapture() != this )
		return;

	ReleaseCapture();
    ClipCursor( NULL );
    
	m_bHeaderSelected = FALSE;

	if ( m_bResizeColumn )
	{
		m_bResizeColumn = FALSE;
		m_bResizeArea = FALSE;
		return;
	}

	if ( m_bDraggingHeader ) {
		CDC *pDC = GetDC();
//		pDC->DrawFocusRect(&m_rcDrag);
		::BitBlt( pDC->m_hDC,
				  m_rcDrag.left, 
				  m_rcDrag.top,
				  m_rcDrag.right - m_rcDrag.left,
				  m_rcDrag.bottom - m_rcDrag.top,
				  m_hdcDrag,
				  0,
				  0,
				  SRCINVERT );
		VERIFY(::DeleteDC( m_hdcDrag ));
		m_hdcDrag = NULL;
		VERIFY(::DeleteObject( m_hbmDrag ));
		m_hbmDrag = NULL;
		ReleaseDC(pDC);
	}

	if ( m_iColHit != -1 && m_pOutliner->m_pColumn[ m_iColHit ]->bDepressed ) {
		if ( !m_bDraggingHeader ) {
			ColumnCommand ( m_pOutliner->m_pColumn[ m_iColHit ]->iCommand );
		} 
		m_pOutliner->m_pColumn[ m_iColHit ]->bDepressed = FALSE;
		InvalidateColumn( m_iColHit );
	}

	m_bDraggingHeader = FALSE;

	if ( m_iPusherHit ) {
		m_iPusherRgn = TestPusher(point);
		if ( m_iPusherRgn == m_iPusherHit) {
			RECT rectClient;
			GetClientRect(&rectClient);
			switch (m_iPusherHit) {
			case pusherLeft:
				if ( m_iPusherState & pusherLeft ) {
					m_pOutliner->m_iVisColumns++;
					m_pOutliner->OnSize ( 0, 
										  rectClient.right,
										  rectClient.bottom - ( m_bDisableHeaders ? 0 : m_iHeaderHeight ) );
					Invalidate();
					m_pOutliner->Invalidate();
				}
				break;
			case pusherRight:
				if ( m_iPusherState & pusherRight ) {
					m_pOutliner->m_iVisColumns--;
					m_pOutliner->OnSize ( 0, 
										  rectClient.right,
										  rectClient.bottom - ( m_bDisableHeaders ? 0 : m_iHeaderHeight ) );
					Invalidate();
					m_pOutliner->Invalidate();
				}
				break;
			}
		}
		m_iPusherRgn = pusherNone;
		m_iPusherHit = pusherNone;
	}
}

BOOL COutlinerParent::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(&pt);

	if (!CWnd::OnSetCursor( pWnd, nHitTest, message )) {
		int iCol = -1;
		if ( nHitTest == HTCLIENT && 
			 TestCol( pt, iCol) && 
			 m_bResizeArea &&
			 m_pOutliner->m_pColumn[ m_iColResize ]->cType == ColumnVariable ) {
			iCol = m_iColResize;
			for ( int i = iCol + 1; i < m_pOutliner->m_iVisColumns; i++ ) {
				if ( m_pOutliner->m_pColumn[ i ]->cType == ColumnVariable ) {
					SetCursor ( theApp.LoadCursor ( AFX_IDC_HSPLITBAR ) );
					m_iColLoser = i;
					m_iColHit = iCol;

					return TRUE;
				}
			}
		}
		m_bResizeArea = FALSE;

		SetCursor ( ::LoadCursor( NULL, IDC_ARROW ) );

		return TRUE;
	}
	return TRUE;
}	

void COutlinerParent::OnMouseMove( UINT nFlags, CPoint point )
{
    m_pt = point;

	if (GetCapture() != this)
		return;

	int i;

    if ( m_bResizeColumn ) {
		int iDelta = point.x - m_ptHit.x;

        m_ptHit = point;      
        
        m_pOutliner->SqueezeColumns( m_iColHit, iDelta );
 		for( i = m_iColHit; i < m_pOutliner->m_iVisColumns; i++ ) 
        {
 			InvalidateColumn( i );
        }
    } else {
		if ( m_bHeaderSelected ) {
			if ( abs(point.x - m_ptHit.x) > 3 ) {
				m_bDraggingHeader = TRUE;
				m_bHeaderSelected = FALSE;
				m_pOutliner->m_pColumn[ m_iColHit ]->bDepressed = FALSE;
				InvalidateColumn( m_iColHit );
				UpdateWindow();

				// Create bitmap for dragging header
				CDC *pDC = GetDC();

				RECT rcBorder;
				::SetRect( &rcBorder, 0, 0, 
						   m_rcDrag.right - m_rcDrag.left, m_rcDrag.bottom - m_rcDrag.top );
				m_hbmDrag = ::CreateBitmap( rcBorder.right, rcBorder.bottom, 1, 1, NULL );
				m_hdcDrag = ::CreateCompatibleDC( pDC->m_hDC );
				::SelectObject( m_hdcDrag, m_hbmDrag );
				HFONT hOldFont = (HFONT) ::SelectObject ( m_hdcDrag, m_hToolFont );
				::FillRect( m_hdcDrag, &rcBorder, (HBRUSH) GetStockObject( WHITE_BRUSH ) );
				::FrameRect( m_hdcDrag, &rcBorder, (HBRUSH) GetStockObject( BLACK_BRUSH ) );
				::SetBkColor( m_hdcDrag, RGB(255, 255, 255) );
				::SetTextColor( m_hdcDrag, RGB(0, 0, 0) );

				DrawColumnHeader( m_hdcDrag, rcBorder, m_iColHit );

				// Invert the sucker
				::BitBlt( m_hdcDrag,
						  0, 0, rcBorder.right, rcBorder.bottom,
						  m_hdcDrag,
						  0, 0,
						  NOTSRCCOPY );

				// Initially XOR the bitmap
				::BitBlt( pDC->m_hDC,
						  m_rcDrag.left, 
						  m_rcDrag.top,
						  m_rcDrag.right - m_rcDrag.left,
						  m_rcDrag.bottom - m_rcDrag.top,
						  m_hdcDrag,
						  0,
						  0,
						  SRCINVERT );

				ReleaseDC(pDC);
			}
		}
			
		if ( m_bDraggingHeader ) {
			int iCol;
			CDC *pDC = GetDC();
			// Undo the last XOR
			::BitBlt( pDC->m_hDC,
					  m_rcDrag.left, 
					  m_rcDrag.top,
					  m_rcDrag.right - m_rcDrag.left,
					  m_rcDrag.bottom - m_rcDrag.top,
					  m_hdcDrag,
					  0,
					  0,
					  SRCINVERT );
			POINT pt;
			pt.x = point.x;
			pt.y = 1; // ignore vertical movement
			if ( TestCol( pt, iCol ) &&
				 (m_pOutliner->m_pColumn[ iCol ]->iCommand != m_idColHit) ) {
				int iWidth = m_pOutliner->m_pColumn[ m_iColHit ]->iCol;
				if ( ( iCol < m_iColHit && point.x - m_rcTest.left < iWidth ) ||
					 ( iCol > m_iColHit && point.x > m_rcTest.right - iWidth ) ) {

					// Shove everything down

					OutlinerColumn_t *tmp;
					tmp = m_pOutliner->m_pColumn[ m_iColHit ];
					if (iCol < m_iColHit) {
						for (i = m_iColHit; i > iCol; i--) {
							m_pOutliner->m_pColumn[i] = m_pOutliner->m_pColumn[i - 1];
						}
					} else {
						for (i = m_iColHit; i < iCol; i++) {
							m_pOutliner->m_pColumn[i] = m_pOutliner->m_pColumn[i + 1];
						}
					}
					m_pOutliner->m_pColumn[ iCol ] = tmp;

					// Allow the Outliner a chance to respond to the fact that the columns
					// have been adjusted.  (Dave H.)
					m_pOutliner->ColumnsSwapped();

					// Redraw the relevent stuff

					int iStart = m_iColHit < iCol ? m_iColHit : iCol;
					int iEnd = m_iColHit < iCol ? iCol : m_iColHit;

					for ( i = iStart; i <= iEnd; i++) {
						m_pOutliner->InvalidateColumn ( i );
						InvalidateColumn ( i );
					}

					m_pOutliner->UpdateWindow();
					UpdateWindow();
					m_iColHit = iCol;
				}
			}
			m_rcDrag.left = point.x + (m_rcHit.left - m_ptHit.x);
			m_rcDrag.right = point.x + (m_rcHit.right - m_ptHit.x);

			// XOR the header bitmap
			::BitBlt( pDC->m_hDC,
					  m_rcDrag.left, 
					  m_rcDrag.top,
					  m_rcDrag.right - m_rcDrag.left,
					  m_rcDrag.bottom - m_rcDrag.top,
					  m_hdcDrag,
					  0,
					  0,
					  SRCINVERT );
			ReleaseDC(pDC);
		}

		if ( m_iPusherHit ) {
			int iTestPush;
			if ( (iTestPush = TestPusher( point )) != m_iPusherRgn ) {
				m_iPusherRgn = iTestPush;
				InvalidatePusher();
				UpdateWindow();
			}
		}
	}
	Default();
}

void COutlinerParent::DrawButtonRect( HDC hDC, const RECT &rect, BOOL bDepressed )
{
	HPEN hBlackPen = (HPEN) ::GetStockObject(BLACK_PEN);
	HPEN hShadowPen = ::CreatePen( PS_SOLID, 0, GetSysColor ( COLOR_BTNSHADOW ) );
	HPEN hHighLightPen = ::CreatePen( PS_SOLID, 0, GetSysColor ( COLOR_BTNHIGHLIGHT ) );
	HPEN hLightPen = NULL;
#ifdef _WIN32
	if ( sysInfo.m_bWin4 ) {
		hLightPen = ::CreatePen( PS_SOLID, 0, GetSysColor( COLOR_3DLIGHT ) );
	}
#endif

    HPEN hOldPen = (HPEN) ::SelectObject ( hDC, bDepressed ? hShadowPen : hBlackPen );
    ::MoveToEx( hDC, rect.left, rect.bottom - 1, NULL );
    ::LineTo( hDC, rect.right - 1, rect.bottom - 1 );
    ::LineTo( hDC, rect.right - 1, rect.top - 1 );

    if ( !bDepressed )
	{
        ::SelectObject( hDC, hShadowPen );
        ::MoveToEx( hDC, rect.left + 1, rect.bottom - 2, NULL );
        ::LineTo( hDC, rect.right - 2, rect.bottom - 2 );
        ::LineTo( hDC, rect.right - 2, rect.top );

		if ( hLightPen ) {
			::SelectObject( hDC, hLightPen );
		} else { 
			::SelectObject( hDC, hHighLightPen );
		}
		::MoveToEx( hDC, rect.left + 1, rect.bottom - 3, NULL );
		::LineTo( hDC, rect.left + 1, rect.top + 1 );
		::LineTo( hDC, rect.right - 2, rect.top + 1 );
    }

    ::SelectObject( hDC, bDepressed ? hShadowPen : hHighLightPen );
    ::MoveToEx( hDC, rect.left, rect.bottom - 2, NULL );
    ::LineTo( hDC, rect.left, rect.top );
    ::LineTo( hDC, rect.right - 1, rect.top );

	::SelectObject( hDC, hOldPen );

	VERIFY( ::DeleteObject( hShadowPen ));
	VERIFY( ::DeleteObject( hHighLightPen ));
	if ( hLightPen )
		VERIFY( ::DeleteObject( hLightPen ));
}

void COutlinerParent::DrawColumnHeader( HDC hdc, const RECT &rect, int iCol )
{
    CRect rcText = rect;
    BOOL bDep = m_pOutliner->m_pColumn[ iCol ]->bDepressed &&
				m_pOutliner->m_pColumn[ iCol ]->bIsButton;

	if ( bDep ) {
		::OffsetRect(&rcText, 1, 1);
	}

    if (!RenderData( m_pOutliner->m_pColumn[ iCol ]->iCommand, rcText, *(CDC::FromHandle(hdc)), 
					 m_pOutliner->m_pColumn[ iCol ]->pHeader )) {
		::InflateRect(&rcText, -4, 0);

		UINT dwDTFormat = DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;

		switch ( m_pOutliner->m_pColumn[ iCol ]->alignment ) {
		case AlignCenter:
			dwDTFormat |= DT_CENTER;
			break;
		case AlignRight:
			dwDTFormat |= DT_RIGHT;
			break;
		case AlignLeft:
			dwDTFormat |= DT_LEFT;
		}

		WFE_DrawTextEx( 0, hdc, 
						(LPTSTR) m_pOutliner->m_pColumn[ iCol ]->pHeader, -1, 
						&rcText, dwDTFormat, WFE_DT_CROPRIGHT );
	}
}
void COutlinerParent::OnPaint ( )
{                            
    CPaintDC pdc ( this );

    if ( !m_pOutliner || m_bDisableHeaders )
        return;

    int i, offset;

	// we might use these in the for() loop below --- make sure
	//  they stay in scope since we don't restore into the CDC until
	//  the end of the routine
    HFONT hOldFont = (HFONT) pdc.SelectObject ( m_hToolFont );
    COLORREF cOldText = pdc.SetTextColor(GetSysColor(COLOR_BTNTEXT));
    COLORREF cOldBk   = pdc.SetBkColor(GetSysColor(COLOR_BTNFACE));

	CRect rectClient;
	GetClientRect ( &rectClient );
	int iMaxHeaderWidth = rectClient.right - m_iPusherWidth;

    for ( i = offset = 0; (i < m_pOutliner->m_iVisColumns) && (offset < iMaxHeaderWidth); i++ )
    {
        BOOL bDep = m_pOutliner->m_pColumn[ i ]->bDepressed &&
					m_pOutliner->m_pColumn[ i ]->bIsButton;
		CRect rect( offset, 0, m_pOutliner->m_pColumn[ i ]->iCol + offset, m_iHeaderHeight );

		if (rect.right > iMaxHeaderWidth ) {
			rect.right = iMaxHeaderWidth;
		}

        RECT rcInter;
        if ( ::IntersectRect ( &rcInter, &pdc.m_ps.rcPaint, &rect ) )
        {
			RECT rcText = rect;
			::InflateRect(&rcText,-2,-2);
			::FillRect(pdc.m_hDC, &rcText, sysInfo.m_hbrBtnFace );
			DrawColumnHeader( pdc.m_hDC, rcText, i );
			DrawButtonRect( pdc.m_hDC, rect, bDep );
		}

        offset += m_pOutliner->m_pColumn[ i ]->iCol;
    }

	// Fill in the gap on the right

	if (offset < iMaxHeaderWidth) {
		RECT rect = {offset, 0, iMaxHeaderWidth, m_iHeaderHeight};
		RECT rcInter;

		if ( IntersectRect( &rcInter, &pdc.m_ps.rcPaint, &rect ) ) {
			RECT rcText = rect;
			::InflateRect(&rcText,-2,-2);
			::FillRect(pdc.m_hDC, &rcText, sysInfo.m_hbrBtnFace );
			DrawButtonRect( pdc.m_hDC, rect, FALSE );
		}
	}

	CRect rect(rectClient.right - m_iPusherWidth, 0, rectClient.right, m_iHeaderHeight );
    CRect iRect;

    if ( iRect.IntersectRect ( &pdc.m_ps.rcPaint, &rect ) ) {
		int idxImage;
		::FillRect ( pdc.m_hDC, &rect, sysInfo.m_hbrBtnFace );

		m_iPusherState = pusherNone;
		if ( m_pOutliner->m_iNumColumns > 1 ) {
			if (m_pOutliner->m_iVisColumns > 1) {
				m_iPusherState |= pusherRight;
			}
			if (m_pOutliner->m_iVisColumns < m_pOutliner->m_iNumColumns) {
				m_iPusherState |= pusherLeft;
			}
		}

		POINT ptBitmap;
		RECT rect2 = rect;

		// Draw left pusher
		rect2.right = (rect.left + rect.right + 1) / 2;

		ptBitmap.x = (rect2.left + rect2.right + 1) / 2 - 4;
		ptBitmap.y = (rect2.top + rect2.bottom + 1) / 2 - 4;
		if ( m_iPusherRgn == pusherLeft ) {
			ptBitmap.x++;
			ptBitmap.y++;
		}
		idxImage = m_iPusherState & pusherLeft ? 
				   IDX_PUSHLEFT : IDX_PUSHLEFTI;
		m_pIImage->DrawImage( idxImage, ptBitmap.x, ptBitmap.y, &pdc, TRUE);
		DrawButtonRect( pdc.m_hDC, rect2, m_iPusherRgn == pusherLeft );
		
		// Draw right pusher
		rect2.left = rect2.right;
		rect2.right = rect.right;
		ptBitmap.x = (rect2.left + rect2.right + 1) / 2 - 4;
		ptBitmap.y = (rect2.top + rect2.bottom + 1) / 2 - 4;
		if ( m_iPusherRgn == pusherRight ) {
			ptBitmap.x++;
			ptBitmap.y++;
		}
 		idxImage = m_iPusherState & pusherRight ? 
				   IDX_PUSHRIGHT : IDX_PUSHRIGHTI;
		m_pIImage->DrawImage( idxImage, ptBitmap.x, ptBitmap.y, &pdc, TRUE);
		DrawButtonRect( pdc.m_hDC, rect2, m_iPusherRgn == pusherRight );
	}

    pdc.SelectObject ( hOldFont );
    pdc.SetTextColor ( cOldText );
    pdc.SetBkColor ( cOldBk );

	if (m_bEnableFocusFrame)
	{
		HBRUSH hBrush = NULL;
		if (GetFocus() == m_pOutliner)
			hBrush = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
		else
			hBrush = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );

		RECT clientRect;
		GetClientRect(&clientRect);
		::FrameRect( pdc.m_hDC, &clientRect, hBrush );	 
		VERIFY(DeleteObject( hBrush ));
	}

}

BOOL COutlinerParent::OnEraseBkgnd( CDC * )
{
    return TRUE;
}

// Overloadable methods

BOOL COutlinerParent::ColumnCommand(int idCol)
{
	return FALSE;
}

BOOL COutlinerParent::RenderData(int idColumn, CRect & rect, CDC & dc, LPCTSTR lpsz )
{
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// COutlinerView

BEGIN_MESSAGE_MAP(COutlinerView,CView)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

BOOL COutlinerView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	return CView::PreCreateWindow(cs);
}

void COutlinerView::OnDraw ( CDC * pDC )
{
}

int COutlinerView::OnCreate ( LPCREATESTRUCT lpCreateStruct )
{
    int iRetVal = CView::OnCreate ( lpCreateStruct );
	LPCTSTR lpszClass = AfxRegisterWndClass( CS_VREDRAW, ::LoadCursor(NULL, IDC_ARROW));
    m_pOutlinerParent->Create( lpszClass, _T("NSOutlinerParent"), 
							   WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN,
							   CRect(0,0,0,0), this, 101 );
    return iRetVal;
}

void COutlinerView::OnSize ( UINT nType, int cx, int cy )
{
    CView::OnSize ( nType, cx, cy );
    m_pOutlinerParent->MoveWindow ( 0, 0, cx, cy, TRUE );
}

void COutlinerView::OnSetFocus ( CWnd * pOldWnd )
{
    CView::OnSetFocus ( pOldWnd );
    m_pOutlinerParent->SetFocus ( );
}


