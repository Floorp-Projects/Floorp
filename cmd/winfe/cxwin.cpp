/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "cxwin.h"

#include "cntritem.h"
#include "medit.h"
#include "button.h"
#include "fmbutton.h"
#include "gridedge.h"
#include "cxsave.h"
#include "netsvw.h"
#include "shcut.h"
#include "mainfrm.h"
#include "dialog.h"
#include "cxprint.h"
#include "findrepl.h"
#include "feembed.h"
#include "libevent.h"
#include "np.h"
#include "nppg.h"
#include "nppriv.h"
#include "winclose.h"
#include "tooltip.h"
#include "slavewnd.h"

#include "intl_csi.h"
#include "abdefn.h"
#include "feimage.h"
#include "edt.h"
extern char * EDT_NEW_DOC_URL;
extern char * EDT_NEW_DOC_NAME;
#define ED_SIZE_FEEDBACK_BORDER   3

#define EDT_IS_SIZING   ( EDT_IS_EDITOR(GetContext()) && EDT_IsSizing(GetContext()) )

#ifdef JAVA
#include "np.h"
#include "java.h"
#include "prlog.h"
#ifdef DDRAW
static DDSURFACEDESC ddsd;
#endif
extern "C" {
#ifndef NSPR20
PR_LOG_DEFINE(APPLET);
#else
extern PRLogModuleInfo *APPLET;
#endif

/*
** API for querying the Navigator's Color Palette from the "outside"...
** 
** This is used by AWT, when it is first started, to clone the navigator's
** color palette. 
*/
PR_PUBLIC_API(HPALETTE) GET_APPLICATION_PALETTE(void)
{
    HPALETTE hPal = (HPALETTE)NULL;
    CWinCX *pActiveContext;
    CGenericFrame *pFrame;

	// XXX This is busted/need to move palette in frame -- Hokay?
    pFrame = ((CNetscapeApp *)AfxGetApp())->m_pFrameList;
    if( pFrame ) {
        pActiveContext = pFrame->GetActiveWinContext();
        if( pActiveContext ) {
            hPal = pActiveContext->GetPalette();
        }
    }
    return hPal;
}
};
#endif  /* JAVA */

// older versions of MFC don't have this #define
#ifndef DEFAULT_GUI_FONT
#define DEFAULT_GUI_FONT ANSI_FIXED_FONT
#endif

#define NOT_A_DIALOG(wincx) \
    ((wincx) ? \
     (wincx)->GetFrame() ? \
     (wincx)->GetFrame()->GetMainContext() ? \
     (wincx)->GetFrame()->GetMainContext()->GetContext() ? \
     (wincx)->GetFrame()->GetMainContext()->GetContext()->type != MWContextDialog \
     : TRUE : TRUE : TRUE : TRUE)


//	An empty frame API so that this code will work without the presence of a frame window.
CNullFrame *CWinCX::m_pNullFrame = NULL;
void *CWinCX::m_pExitCookie = NULL;

void wincx_exit(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //  ExitInstance, clean up if possible.
    if(CWinCX::m_pNullFrame) {
        delete CWinCX::m_pNullFrame;
        CWinCX::m_pNullFrame = NULL;
    }
    if(CWinCX::m_pExitCookie) {
        slavewnd.UnRegister(CWinCX::m_pExitCookie);
        CWinCX::m_pExitCookie = NULL;
    }
}


CWinCX::CWinCX(CGenericDoc *pDocument, CFrameGlue *pFrame, CGenericView *pView, MWContextType mwType, ContextType cxType) : CPaneCX(pView ? pView->m_hWnd : NULL, FALSE) {
//  Purpose:    Construct a window context
//  Arguments:  pDocument   The document, if NULL, one is created in CDCCX.
//                          It should be noted that this has to be a CNetscapeDoc
//                              currently, since it handles certain Ole aspects in
//                              the Netscape way.
//              pFrame  The frame which owns this view.  Can be NULL (turns off
//                          any frame access).
//              hView   The view which this context interacts with.  Can not be NULL.
//  Returns:    none
//  Comments:   Basically set the context type.
//  Revision History:
//      06-25-95    created GAB
//

    //  Set the context type.
    m_cxType = cxType;
    GetContext()->type = mwType;	//	by default only.

    m_pGenView = pView;
	
    //  No previous mouse event in this context.
    m_LastMouseEvent = m_None;
    m_bScrollingTimerSet = FALSE;
    m_bMouseMoveTimerSet = FALSE;
    //  We start off thinking we have a border when a frame cell.
    m_bHasBorder = TRUE;

	m_MM = MM_TEXT;

    //  And the frame.
    m_pFrame = pFrame;
	pLastToolTipImg = 0;
	m_pLastToolTipAnchor = NULL;

    //  Set up a callback in exit instance.
    if(NULL == m_pExitCookie) {
        m_pExitCookie = slavewnd.Register(SLAVE_EXITINSTANCE, wincx_exit);
    }
    //  If there's no NULL frame yet, create one.
    if(NULL == m_pNullFrame)	{
        m_pNullFrame = new CNullFrame;
    }

	//	If there's no frame, use the Null Frame.
	if(m_pFrame == NULL)	{
		TRACE("Using the NULL frame\n");
		m_pFrame = m_pNullFrame;
	}

    //  And the document.
    m_pDocument = pDocument;
#ifdef DDRAW
	m_ScrollWindow = FALSE;
	m_physicWinRect.Empty();
#endif
	//	We're not active.
	m_bActiveState = FALSE;

    //  We're not laying out.
    m_bIsLayingOut = FALSE;

	//	We've not highlighted an anchor.
	m_pLastArmedAnchor = NULL;

	//	No old progress;
	m_lOldPercent = 0;

    // not doing a drag/drop operation
    m_bDragging = FALSE;

	//      No selected embedded item.
	m_pSelected = NULL;
    
    m_bSelectingCells = FALSE;
    m_bInPopupMenu = FALSE;

	//	Inform the view of its new context.
    if(m_pGenView)   {
        m_pGenView->SetContext(this);
    }

	/*__EDITOR__*/
    m_bMouseInSelection = FALSE;

	//	We've not over an image.
	m_pLastImageObject = NULL;

	m_crWindowRect = CRect(0, 0, 0, 0);

	//	Mouse is up not down.
	m_bLBDown = FALSE;
	m_bLBUp = TRUE;
	m_uMouseFlags = 0;
	m_cpMMove.x = m_cpMMove.y = 0;
	m_ToolTip = 0;


    //  size is not chrome specified yet.
    m_bSizeIsChrome = FALSE;

    //  Clear those old elements that we will track in the
    //      loaded page (see FireMouseOverEvent....)
    m_pLastOverAnchorData = NULL;
    m_pLastOverElement = NULL;
    m_pStartSelectionCell = NULL;
    m_bLastOverTextSet = FALSE;

//#ifndef NO_TAB_NAVIGATION 
	m_lastTabFocus.pElement		= NULL;
	m_lastTabFocus.mapAreaIndex	= 0;		// 0 means no focus, start with index 1.
	m_lastTabFocus.pAnchor			= NULL;
    m_isReEntry_setLastTabFocusElement  = 0;     // to provent re-entry
//#endif	/* NO_TAB_NAVIGATION */

	imageToolTip = NULL;
}

CWinCX::~CWinCX()   {
//  Purpose:    Destroy a window context.
//  Arguments:  none
//  Returns:    none
//  Comments:   Does context instance specific cleanup.
//  Revision History:
//      06-25-95    created GABby
//
    
    MWContext *pContext = GetContext();

	if(GetFrame() != NULL)	{
		GetFrame()->ClearContext(this);
	}

	if (m_ToolTip)
		delete m_ToolTip;

	XP_ListDestroy (imageToolTip);

	// Netcaster going away
	if (GetContext() == theApp.m_pNetcasterWindow)
		theApp.m_pNetcasterWindow = NULL ;
}

#ifdef DDRAW
void CWinCX::CalcWinPos()
{
	RECT tempRect1, tempRect2;

	::GetWindowRect(GetPane(), &tempRect1);
	::GetClientRect(GetPane(), &tempRect2);

	int wWidth, cWidth;
	int wHeight, cHeight;
	int width, height;

	wWidth = tempRect1.right - tempRect1.left;
	cWidth = tempRect2.right - tempRect2.left;
	width =	wWidth - cWidth;
	if (IsVScrollBarOn())
		width -= sysInfo.m_iScrollWidth; 
	m_physicWinRect.left = tempRect1.left + (width / 2);
	m_physicWinRect.right = tempRect1.right - (width / 2);

	wHeight = tempRect1.bottom - tempRect1.top;
	cHeight = tempRect2.bottom - tempRect2.top;
	height = wHeight - cHeight;
	if (IsHScrollBarOn())
		height -= sysInfo.m_iScrollHeight; 
	m_physicWinRect.top = tempRect1.top + (height / 2);
	m_physicWinRect.bottom = tempRect1.bottom - (width / 2);

}
#endif

void CWinCX::Initialize(BOOL bOwnDC, RECT *pRect, BOOL bInitialPalette, BOOL bNewMemDC)   {
//  Purpose:    Initialize properties of this context.
//  Arguments:  void
//  Returns:    void
//  Comments:   Basically set up the iWidth and iHeight of the context.
//              To be called after initial construction.
//  Revision History:
//      06-25-95    created GABby
//

    CPaneCX::Initialize(bOwnDC, pRect, bInitialPalette, bNewMemDC);
#ifdef DDRAW
	m_lpDDSPrimary = NULL;
	m_lpDDSBack = NULL;
	mg_pPal = NULL;
	m_pClipper = NULL;
	m_pOffScreenClipper = NULL;
    m_offScreenDC = 0;
    m_lpDD = NULL;
	/*
     * create the main DirectDraw object
     */
    if (DirectDrawCreate( NULL, &m_lpDD, NULL ) != DD_OK) {
        m_lpDD->Release();
        m_lpDD = NULL;
    }
	// initialize DirectDraw stuff.
	if (m_lpDD && m_lpDD->SetCooperativeLevel( GetPane(), DDSCL_NORMAL  )!= DD_OK) {
		ReleaseDrawSurface();
	}
	if (m_lpDD) {
		SetUseDibPalColors(FALSE);
//		DDSURFACEDESC       ddsd;
		DDCAPS             ddscaps, ddshel;
		ZeroMemory((void*)&ddscaps, sizeof(DDCAPS));
		// Create the primary surface with 1 back buffer
		ddscaps.dwSize = sizeof( DDCAPS );
		ZeroMemory((void*)&ddshel, sizeof(DDCAPS));
		// Create the primary surface with 1 back buffer
		ddshel.dwSize = sizeof( DDCAPS );
		m_lpDD->GetCaps(&ddscaps, &ddshel);

		ZeroMemory((void*)&ddsd, sizeof(DDSURFACEDESC));
		// Create the primary surface with 1 back buffer
		ddsd.dwSize = sizeof( ddsd );
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		if (m_lpDD->CreateSurface( &ddsd, &m_lpDDSPrimary, NULL ) != DD_OK) {
			ReleaseDrawSurface();
		}
		else {  // create Offscreen draw surface. 
			CreateAndLockOffscreenSurface(*pRect);
			// now create the palette for direct draw surface.
			if (m_lpDDSPrimary) {
			    ZeroMemory(&m_surfDesc, sizeof(ddsd));
				m_surfDesc.dwSize = sizeof(ddsd);

			    m_surfDesc.dwFlags = DDSD_PIXELFORMAT;
				m_lpDDSBack->GetSurfaceDesc(&m_surfDesc);
				DDPIXELFORMAT pFormat;
				m_lpDDSPrimary->GetPixelFormat(&pFormat);
				// If this surface supports a palette, then do it

				if (m_surfDesc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) {

					PALETTEENTRY palEntry[256];
					::GetPaletteEntries(GetPalette(), 0, 255, palEntry);
					if (m_lpDD->CreatePalette(DDPCAPS_8BIT, palEntry, &mg_pPal, NULL) != DD_OK) {
						ReleaseDrawSurface();
					}
					else {
						if (m_lpDDSPrimary->SetPalette(mg_pPal )!= DD_OK) {
							ReleaseDrawSurface();
						}
						else {
							m_lpDDSBack->SetPalette(mg_pPal ); 
						}
					}
				}

				// Use a clipper object for clipping when in windowed mode

				if (m_lpDD->CreateClipper(0, &m_pClipper, NULL) != DD_OK) {
					ReleaseDrawSurface();
				}
				if (m_pClipper) {
					if (m_pClipper->SetHWnd(0, GetPane()) != DD_OK) {
						ReleaseDrawSurface();
					}
					else {
						if (m_lpDDSPrimary->SetClipper(m_pClipper) != DD_OK) {
							ReleaseDrawSurface();
						}
					}
				}
				if (m_lpDD->CreateClipper(0, &m_pOffScreenClipper, NULL) != DD_OK) {
					ReleaseDrawSurface();
				}
				else {
					if (m_lpDDSBack->SetClipper(m_pOffScreenClipper) != DD_OK) {
						ReleaseDrawSurface();
					}
				}

			}
		}
	}
#endif
}

#ifdef DDRAW
void CWinCX::SetClipOnDrawSurface(LPDIRECTDRAWSURFACE surface, HRGN hClipRgn)
{
    if (m_offScreenDC) {
		if (hClipRgn == FE_NULL_REGION)	{
			::SelectClipRgn(m_offScreenDC, NULL);
		}
		else {
			::SelectClipRgn(m_offScreenDC, hClipRgn);
		}
    }
}

void CWinCX::RestoreAllDrawSurface()
{
#ifdef XP_WIN32
    if( m_lpDDSPrimary->Restore() == DD_OK ) {
		if (m_lpDDSBack->Restore() == DD_OK) {
			return;
		}
	}

	// if the surface can not be restore.
	TRACE("There is something wrong with DirectDraw surface.\n");
	ReleaseDrawSurface();
#endif
}

void CWinCX::ReleaseDrawSurface()
	{
#ifdef XP_WIN32
		if (m_offScreenDC) {
			ReleaseOffscreenSurfDC();
			m_offScreenDC = 0;
		}
		if (m_pOffScreenClipper) {
			m_pOffScreenClipper->Release();
			m_pOffScreenClipper = NULL;
		}
		if (m_pClipper) {
			m_pClipper->Release();
			m_pClipper = NULL;
		}
		if (m_lpDDSPrimary) {
			m_lpDDSPrimary->Release();
			m_lpDDSPrimary = NULL;
		}
		if (m_lpDDSBack) {
			m_lpDDSBack->Release();
			m_lpDDSBack = NULL;
		}
		if (mg_pPal) {
			mg_pPal->Release();
			mg_pPal = NULL;
		}
		if (m_lpDD) {
			m_lpDD->Release();
			m_lpDD = NULL;
		}
		HDC hdc = GetContextDC();
		CDCCX::SetUseDibPalColors(!IsPrintContext() && (::GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE));
		ReleaseContextDC(hdc);
#endif  // WIN32
	}
#endif // DDRAW




void CWinCX::ClearFrame()	{
	m_pFrame = NULL;

	//	Go through all our immediate children, clearing
	//		their frame.
	//	They will in turn call thier children, if present.
	MWContext *pChild;
	XP_List *pTraverse = GetContext()->grid_children;
	while (pChild = (MWContext*)XP_ListNextObject(pTraverse)) {
		WINCX(pChild)->ClearFrame();
	}
}
//	If we're a grid cell, we need to return the parent's palette.
HPALETTE CWinCX::GetPalette() const	{
	if(IsGridCell() == TRUE && GetParentContext() != NULL)	{
		ASSERT(ABSTRACTCX(GetParentContext())->IsWindowContext());
		return(WINCX(GetParentContext())->GetPalette());
	}
	//	Return the normal stuff.
	return(CDCCX::GetPalette());
}


void CWinCX::DestroyContext()   {
    if(IsDestroyed() == FALSE)  {
		ResetToolTipImg();

        //  Deactivate any embedded items.
        OnDeactivateEmbedCX();

        //	Release our modality if any is in effect.
        POSITION rTraverse = m_cplModalOver.GetHeadPosition();
        while(rTraverse)	{
            HWND	hwndOwner;

            //	We stored the HWND of the window we're being modal over
            //	Retrieve it.
            hwndOwner = (HWND)m_cplModalOver.GetNext(rTraverse);
            if (::IsWindow(hwndOwner)) {
                //  Must make sure no one else thinks they are modal over the
                //      window before we go off and enable it.
                int iTraverse = 1;
                BOOL bEnable = TRUE;
                MWContext *pMW = NULL;
                XP_List *pTraverse = XP_GetGlobalContextList();
                while (pMW = (MWContext *)XP_ListNextObject(pTraverse)) {
                    if(ABSTRACTCX(pMW) && ABSTRACTCX(pMW)->IsWindowContext()) {
                        CWinCX *pCX = WINCX(pMW);
                        if(this == pCX) {
                            //  Exclude ourselves from the check.
                            continue;
                        }
                        POSITION rInsane = pCX->m_cplModalOver.GetHeadPosition();
                        HWND hCheck = NULL;
                        while(rInsane) {
                            hCheck = (HWND)pCX->m_cplModalOver.GetNext(rInsane);
                            if(hCheck == hwndOwner) {
                                //  Someone else will enable it later.
                                bEnable = FALSE;
                                break;
                            }
                        }
                    }
                    if(FALSE == bEnable) {
                        //  No need to continue if decided already.
                        break;
                    }
                }
                if(bEnable) {
                    ::EnableWindow(hwndOwner, TRUE);
                }
            }
        }
        m_cplModalOver.RemoveAll();

	    //	Perform our close callbacks.
	    if(!m_cplCloseCallbacks.IsEmpty())	{
            POSITION rFuncs = m_cplCloseCallbacks.GetHeadPosition();
            POSITION rArgs = m_cplCloseCallbackArgs.GetHeadPosition();
	        void (*pCloseCallback)(void *) = NULL;
	        void *pCloseCallbackArg = NULL;

            while(rFuncs)   {
                pCloseCallback = (void (*)(void *))m_cplCloseCallbacks.GetNext(rFuncs);
                pCloseCallbackArg = m_cplCloseCallbackArgs.GetNext(rArgs);

                if(pCloseCallback != NULL)  {
                    pCloseCallback(pCloseCallbackArg);
                }
            }

            m_cplCloseCallbacks.RemoveAll();
            m_cplCloseCallbackArgs.RemoveAll();
	    }

	    MWContext *pContext = GetContext();

	    //JavaScript has lifetime-linked windows which must close now too
	    if (pContext->js_dependent_list) {
		MWContext *pDepContext;
		XP_List *pTraverse = pContext->js_dependent_list;
		while (pDepContext = (MWContext *)XP_ListNextObject(pTraverse)) {
		    pDepContext->js_parent = 0;
		    FE_DestroyWindow(pDepContext);
		}
		XP_ListDestroy(pContext->js_dependent_list);
		pContext->js_dependent_list = NULL;
	    }
	    if (pContext->js_parent) {
		if (XP_ListCount(pContext->js_parent->js_dependent_list) == 1) {
		    XP_ListDestroy(pContext->js_parent->js_dependent_list);
		    pContext->js_parent->js_dependent_list=NULL;
		}
		else {
		    XP_ListRemoveObject(pContext->js_parent->js_dependent_list, pContext);
		}

	    }

		//  Call the base.  This will delete the object !!
		//  To make sure that m_pPal is not being selected to a dc
		HDC hdc = GetContextDC();
		::SelectPalette(hdc,  (HPALETTE)::GetStockObject(DEFAULT_PALETTE), FALSE);
		//  Get rid of the home grown DC that we created.
		if(GetPane() != NULL)	{
		#ifdef DDRAW
            ReleaseDrawSurface();
		#endif
            ReleaseContextDC(hdc);
		}
#ifdef JAVA
		//
		// Discard the events pending for the context...
		//
		LJ_DiscardEventsForContext(pContext);
#endif /* JAVA */
    }
    

    CPaneCX::DestroyContext();

}


void CWinCX::OnDeactivateEmbedCX()  {
    CGenericView *pView = GetView();
    if(pView != NULL && m_pSelected != NULL) {
		//	Obtain the plugin structure.
		NPEmbeddedApp *pPluginShim = (NPEmbeddedApp *)m_pSelected->FE_Data;
		//	Make sure it is not a plugin, but an OLE container item.
		if(pPluginShim != NULL && wfe_IsTypePlugin(pPluginShim) == FALSE)	{
			//	Get the container item, and deactivate it.
	        CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pPluginShim->fe_data;
	        if(pItem != NULL && pItem->IsInPlaceActive() == TRUE)   {
				TRY	{
	            	pItem->Deactivate();

				}
				CATCH(CException, e)	{
					//	Something went wrong in OLE (other app down).
					//	No complicated handling here, just keep running.
				}
				END_CATCH
	        }
		}
		//	Clear that nothing is currently selected.
        m_pSelected = NULL;
    }
}

#ifdef EDITOR
void CWinCX::Scroll(int iBars, UINT uSBCode, UINT uPos, HWND hCtrl, UINT uTimes)
{
    int32 lOldOrgY = GetOriginY();
    int32 lOldOrgX = GetOriginX();
    
    CPaneCX::Scroll(iBars, uSBCode, uPos, hCtrl, uTimes);
    
    if(lOldOrgY != GetOriginY() || lOldOrgX != GetOriginX()) {
        if( EDT_IS_EDITOR(GetContext()) ){
            EDT_WindowScrolled( GetContext() );
        }
    }
}
#endif


#ifdef DDRAW
void CWinCX::ScrollWindow(int x, int y) 
{
	RECT source, dest;
	dest.top = m_physicWinRect.top;
	dest.bottom = m_physicWinRect.bottom;
	dest.left = m_physicWinRect.left;
	dest.right = m_physicWinRect.right;
	source.top = m_physicWinRect.top;
	source.bottom = m_physicWinRect.bottom;
	source.left = m_physicWinRect.left;
	source.right = m_physicWinRect.right;
	RECT updateRect;
	updateRect.top = m_physicWinRect.top;
	updateRect.bottom = m_physicWinRect.bottom;
	updateRect.left = m_physicWinRect.left;
	updateRect.right = m_physicWinRect.right;

	if (y == 0) { // scrolling horz
		if (x > 0) {// scroll right.
			dest.left += x;
			source.right -= x;
		}
		else {
			dest.right += x;
			source.left -= x;
		}
		if (m_physicWinRect.left == dest.left) {
			updateRect.left = dest.right;
		}
		else
			updateRect.left = m_physicWinRect.left;
		updateRect.right = updateRect.left + abs(x);
	}
	else if (x == 0) { // scroll vert.
		if (y > 0) {// scroll down.
			dest.top += y;
			source.bottom -= y;
		}
		else {
			dest.bottom += y;
			source.top -= y;
		}
		if (m_physicWinRect.top == dest.top) {
			updateRect.top = dest.bottom;
		}
		else
			updateRect.top = m_physicWinRect.top;
		updateRect.bottom = updateRect.top + abs(y);
	}
	if (y < 0 || x < 0) {
		if(IsVScrollBarOn() ) { 
			updateRect.left -= sysInfo.m_iScrollWidth;
			updateRect.right -= sysInfo.m_iScrollWidth;
		}
 		if(IsHScrollBarOn()) {
			updateRect.top -= sysInfo.m_iScrollWidth;
			updateRect.bottom -= sysInfo.m_iScrollWidth;
		}
	}
	::OffsetRect(&updateRect, -GetWindowsXPos(), -GetWindowsYPos());
  	HRESULT err = m_lpDDSBack->ReleaseDC(m_offScreenDC);
	::OffsetRect(&dest, -GetWindowsXPos(), -GetWindowsYPos());
	err = m_lpDDSBack->Blt(&dest, m_lpDDSPrimary, &source, DDBLT_WAIT, NULL);
	if (err == DDERR_SURFACELOST) {
		RestoreAllDrawSurface();
		err = m_lpDDSBack->Blt(&dest, m_lpDDSPrimary, &source, DDBLT_WAIT, NULL);
	}
	m_lpDDSBack->GetDC(&m_offScreenDC);

	m_ScrollWindow = TRUE;
#ifdef DEBUG_mhwang
	TRACE("Scroll Window\n");
#endif
	m_pAlternateDC = m_offScreenDC;
	RefreshArea(m_offScreenDC, updateRect.left + m_lOrgX, updateRect.top + m_lOrgY, 
					updateRect.right - updateRect.left,
					updateRect.bottom - updateRect.top);
	RECT prcUpdate;
//	::ScrollWindowEx(GetPane(), x, (int) y, NULL, NULL, NULL, &prcUpdate, SW_SCROLLCHILDREN);
	if (m_lpDDSPrimary ) {
		LTRB rect(m_physicWinRect);
		rect.left -= GetWindowsXPos();
		rect.right -= GetWindowsXPos();
		rect.top -= GetWindowsYPos();
		rect.bottom -= GetWindowsYPos();
		FE_Region hCurClip = GetDrawingClip();
		HDC hdc;
		m_lpDDSPrimary->GetDC(&hdc);
		::SelectClipRgn(hdc, NULL);
		m_lpDDSPrimary->ReleaseDC(hdc);
		BltToScreen(rect, NULL);
	}
	m_pAlternateDC = 0;
	m_ScrollWindow = FALSE;
}
#endif
//	This function get's called when the window moves around.
void CWinCX::OnMoveCX()	{
    //  WARNING:m_crWindowRect will be invalid until next AftWMSize!

	//	Go through all our immediate children, telling them their screen location
	//		has changed.
	//	They will in turn call thier children, if present.
	MWContext *pChild;
	XP_List *pTraverse = GetContext()->grid_children;
	while (pChild = (MWContext*)XP_ListNextObject(pTraverse)) {
		WINCX(pChild)->OnMoveCX();
	}
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_MOVE;

    CFrameWnd *pFWnd = GetFrame()->GetFrameWnd();
    if(pFWnd) {
        CRect crFrame;
        pFWnd->GetWindowRect(crFrame);
        event->x = (int32)crFrame.left;
        event->y = (int32)crFrame.top;
        event->screenx = (int32)crFrame.left;
        event->screeny = (int32)crFrame.top;
        ET_SendEvent(GetContext(), 0, event,
                     0, 0);
    }
#ifdef DDRAW
	CalcWinPos();
#endif
}

static void
wfe_ResizeFullPagePlugin(MWContext* pContext, int32 lWidth, int32 lHeight)
{
	ASSERT(pContext);
	NPWindow* npWindow = pContext->pluginList->wdata;

	ASSERT(npWindow);
	if (npWindow) {
		npWindow->width = lWidth;
		npWindow->height = lHeight;
		::SetWindowPos((HWND)npWindow->window,
			NULL,
			0,
			0,
			(int)npWindow->width,
			(int)npWindow->height,
			SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
		NPL_EmbedSize(pContext->pluginList);
	}
}

#ifdef DDRAW
void CWinCX::CreateAndLockOffscreenSurface(RECT& rect)
{
	// create new offscreen surface.
	if (m_lpDDSPrimary) {
		if (m_lpDDSBack) {
			m_lpDDSBack->ReleaseDC(m_offScreenDC);
			m_offScreenDC = 0;
			m_lpDDSBack->Release();
		}
		m_lpDDSBack = CreateOffscreenSurface(rect);
		RestoreAllDrawSurface();
		if (m_lpDDSBack)
			m_lpDDSBack->GetDC(&m_offScreenDC);
	}
}

LPDIRECTDRAWSURFACE CWinCX::CreateOffscreenSurface(RECT& rect)
{
	LPDIRECTDRAWSURFACE retval;
    // create new offscreen surface.
	ZeroMemory((void*)&ddsd, sizeof(DDSURFACEDESC));
	ddsd.dwSize = sizeof( ddsd );
	// Create a offscreen bitmap.
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwHeight = rect.bottom - rect.top;
	ddsd.dwWidth = rect.right - rect.left;
	if (m_lpDD->CreateSurface( &ddsd, &retval, NULL )!=DD_OK) { 
		ReleaseDrawSurface();
		return NULL;
	}
	else 
		return retval;
}
#endif

void CWinCX::AftWMSize(PaneMessage *pMessage)
{
//  Purpose:    Informs the context that the size of the displayable area has changed.
//  Arguments:  As OnSize in MFC
//  Returns:    void
//  Comments:   Pretty much handles every want of a resizeable window.
//  Revision History:
//      07-13-95    created GABby
//

    //  Call the base.
    //  Preserve old size for logic in this function.
    int32 lOldWidth = GetWidth();
    int32 lOldHeight = GetHeight();
    CPaneCX::AftWMSize(pMessage);

    UINT uType = (UINT)pMessage->wParam;
    int iX = LOWORD(pMessage->lParam);
    int iY = HIWORD(pMessage->lParam);

    //  Wether or not we should call NiceReload before we return from this function.
    //  We stall the call, as we need to update all of our state size variables
    //      before we start a load which may depend upon these state variables
    //      to be correct for display purposes and for turning on and off scroll
    //      bars etc.
    BOOL bNiceReload = FALSE;

    //  Block flailing random message we receive when a new browser
    //      window is opened and we're minimized.
    if(GetFrame() && GetFrame()->GetFrameWnd() &&
        GetFrame()->GetFrameWnd()->IsIconic() &&
        uType == 0 && iX == 0 && iY == 0)    {
        //  Stop the stupid windows behavior.
        return;
    }

	// There is no window.
	if(!GetPane() )
		return;

	//	Update our idea of where we exist.
    CRect crNewRect(0, 0, 0, 0);
    ::GetWindowRect(GetPane(), crNewRect);

#ifdef DDRAW
	// create new offscreen surface.
	if (m_lpDDSPrimary) {
		CreateAndLockOffscreenSurface(crNewRect);
	}

	// mwh - reset our windows's screen position.  so it will be calcuate
	// in DisplayPixmap again.
	m_physicWinRect.Empty();
#endif
	if(IsGridCell() && !m_crWindowRect.EqualRect(crNewRect))    {
		//  NCAPI sizing support.
		CWindowChangeItem::Sizing(GetContextID(), crNewRect.left, crNewRect.top, crNewRect.Width(), crNewRect.Height());
	}

	//	Don't do this on anything but restoration and maximization.
	//	We don't care if we're minimized.
	if(uType != SIZE_MAXIMIZED && uType != SIZE_RESTORED)	{
		TRACE("Don't handle size messages of type %u\n", uType);
		return;
	}

    //  Update what our document thinks if In Place.
    if(GetDocument() && GetDocument()->IsInPlaceActive() && GetPane())   {
        int32 lIPWidth = Twips2MetricX(iX);
        int32 lIPHeight = Twips2MetricY(iY);

        //  quasi-hack alert.
        //  When the scroll bars are on, our twips to metrics conversions
        //      are off by 1 pixel in the direction of the scroller.
        //  Adjust.
        //  This seems reverse logic, but is correct (horiz scroll bar
        //      shortens height of window when present).
        if(IsHScrollBarOn()) {            
            lIPHeight -= Twips2MetricX(1);
        }
        if(IsVScrollBarOn())    {
            lIPWidth -= Twips2MetricY(1);
        }

        GetDocument()->m_csViewExtent = CSize(CASTINT(lIPWidth), CASTINT(lIPHeight));
        TRACE("InPlace Document extents now %ld,%ld\n", lIPWidth, lIPHeight);
    }

    CGenericView *pView = GetView();
    ASSERT(pView);
    
    //  We can never allow resize when there are frame cells and we are in
    //      print preview.  The print preview state holds a pointer to
    //      one of the views in the frameset.  Once print preview is
    //      completed, the code will attempt to set that view to be the
    //      active view.  If deleted (as from a reload/resize), then we crash.
    //  Also, if only the Y value changed, then just adjust the iHeight.
    //      This doesn't apply to grid parents OR Composer, as they need to reload in
    //      all resize cases.
    if( !EDT_IS_EDITOR(GetContext()) &&
         ((IsGridParent() && pView->IsInPrintPreview()) ||
         (GetWidth() == lOldWidth && IsGridParent() == FALSE))
    ) {
        if(ContainsFullPagePlugin()) {
            wfe_ResizeFullPagePlugin(GetContext(), GetWidth(), GetHeight());
        }

		{
			MWContext *context = GetContext();

			/* Mark the entire window dirty and force the compositor to composite now for
			   the case when the user resizes the top or bottom edge of the window. 
			   This can be a potential performance problem!.  Be careful.
			*/
			if (context->compositor && GetHeight() != lOldHeight)
			{
				CL_Compositor *compositor = context->compositor;
				XP_Rect rect;
				CL_OffscreenMode save_offscreen_mode;
        
				rect.left = 0;
				rect.top = 0;
				rect.right = GetWidth();
				rect.bottom = GetHeight();
				CL_UpdateDocumentRect(compositor,
									  &rect, (PRBool)FALSE);

				/*	Temporarily force drawing to use the offscreen buffering area to reduce
					flicker when resizing. (If no offscreen store is allocated, this code will
					have no effect, but it will do no harm.) */
				save_offscreen_mode = CL_GetCompositorOffscreenDrawing(compositor);
				CL_SetCompositorOffscreenDrawing(compositor, CL_OFFSCREEN_ENABLED);
				CL_CompositeNow(compositor);
				CL_SetCompositorOffscreenDrawing(compositor, save_offscreen_mode);

				/*	Call Win32 API call to validate entire windows so that the WM_PAINT message
					that follows a resize does not force a repaint of the window.  The repaint
					happens via the compositor in the CL_UpdateDocumentRect call above */
				::ValidateRect( GetPane(), NULL );
			}						
		}

        return;
    }

    //  Check the history to see if what we have is a binary image,
    //      or if this is a view-source URL.
    //  Don't reload if this is the case.
    History_entry *pEntry = SHIST_GetCurrent(&(GetContext()->hist));
    if(pEntry != NULL &&
        (pEntry->is_binary ||
        (pEntry->address && 0 == strncmp(pEntry->address, "view-source:", 12)))) {
	    if(ContainsFullPagePlugin()) {
	        // There can be only one plugin if it is full page
			wfe_ResizeFullPagePlugin(GetContext(), GetWidth(), GetHeight());
        }
        return;
    }
    //  Have any embedded items update.
    CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)GetDocument()->GetInPlaceActiveItem(pView);
    if(pItem != NULL)   {
		pItem->SetItemRects();
    }

#ifdef EDITOR
    if( EDT_IS_EDITOR(GetContext()) ){
        EDT_RefreshLayout( GetContext() );
    }
    else 
#endif // EDITOR
    {
        // If this is edit_view_source, then we can't reload because the 
        //  data didn't come from an acutal URL
        if( !GetContext()->edit_view_source_hack ){
            //  Reload, we need to relayout everything to the new dimenstions.
            if(m_crWindowRect.Width() != crNewRect.Width() ||
                m_crWindowRect.Height() != crNewRect.Height()) {
                bNiceReload = TRUE;
            }
        }
    }
    m_crWindowRect = crNewRect;
    
    //  Now that all variables are updated and in sync, we can call
    //      NiceReload if needed.
    if(bNiceReload && XP_DOCID(GetDocumentContext())) {
		/* MWContext *context = GetContext(); */

	//	Instead of re-loading from scratch, we want to pass new width, height of window 
	//	to the re-layout routine.
		LO_RelayoutOnResize(GetDocumentContext(), GetWidth(), GetHeight(), m_lLeftMargin, m_lTopMargin);

		/* Call Win32 API call to validate entire window so that the WM_PAINT message
		   that follows a resize does not force a repaint of the window.  The repaint
		   happens in the LO_RelayoutOnResize call above */
		::ValidateRect( GetPane(), NULL );

		/*
        NiceResizeReload();
		*/
    }
}

#ifdef EDITOR
void CWinCX::PreWMErasebkgnd(PaneMessage *pMessage)
{
    MWContext *pMWContext = GetContext();
    if( pMWContext && EDT_IS_EDITOR(pMWContext) ){
        WFE_HideEditCaret(pMWContext);
    }

    CPaneCX::PreWMErasebkgnd(pMessage);

    if( pMWContext && EDT_IS_EDITOR(pMWContext) ){
        WFE_ShowEditCaret(pMWContext);
    }
}
#endif

BOOL CWinCX::EraseTextBkgnd(HDC pDC, RECT& cRect, LO_TextStruct* pText)
{
	// If there's a background color specified then use it
	return CDCCX::_EraseBkgnd(pDC, cRect, GetOriginX(), GetOriginY(), 
					   pText->text_attr->no_background ? NULL : &pText->text_attr->bg);
}

#ifdef LAYERS
BOOL CWinCX::HandleLayerEvent(CL_Layer * pLayer, CL_Event * pEvent)
{
    XY point(pEvent->x, pEvent->y); // Event location, in layer coordinates
    BOOL bReturn = TRUE;
    fe_EventStruct *pFEEvent = (fe_EventStruct *)pEvent->fe_event;
    UINT uFlags, nFlags, nRepCnt, nChar;
    CPoint cpPoint;             // Event location, in screen coordinates
    LO_Element *pElement;
    CFormElement * pFormElement;    

    if (CL_IS_MOUSE_EVENT(pEvent)) {
        if (pFEEvent) {
            uFlags = pFEEvent->uFlags;
            cpPoint.x = pFEEvent->x;
	    cpPoint.y = pFEEvent->y;
        }
        else {
            // This is a synthesized event and we need to fill in
            // the FE part. We can just used the uFlags from the
            // previous mouse event. Should we know all the information
            // to create the cpPoint?
            uFlags = m_uMouseFlags;
			int32 layer_x_offset = CL_GetLayerXOrigin(pLayer);
			int32 layer_y_offset = CL_GetLayerYOrigin(pLayer);
            cpPoint.x = CASTINT(pEvent->x + layer_x_offset - m_lOrgX);
            cpPoint.y = CASTINT(pEvent->y + layer_y_offset - m_lOrgY);
        }
    }

    if (CL_IS_KEY_EVENT(pEvent)) {
        if (pFEEvent) {
	    nFlags = HIWORD(pFEEvent->fe_modifiers);
	    nRepCnt = LOWORD(pFEEvent->fe_modifiers);
	    nChar = pFEEvent->nChar;
        }
    }
   
    switch(pEvent->type) {
		case CL_EVENT_MOUSE_BUTTON_DOWN:
			if (pEvent->which == 1)
				OnLButtonDownForLayerCX(uFlags, cpPoint, point, pLayer);
			// The right button is passed to the view for popup stuff
			else if (pEvent->which == 3) {
				if(!OnRButtonDownForLayerCX(uFlags, cpPoint, 
								point, pLayer)) {   
                    CGenericView *pView = GetView();
                    if(pView)   {
				    bReturn = pView->OnRButtonDownForLayer(uFlags, cpPoint, 
									    point.x, point.y,
									    pLayer);
                    }
                }
			}
			break;
		case CL_EVENT_MOUSE_BUTTON_UP:
			if (pEvent->which == 1)
				OnLButtonUpForLayerCX(uFlags, cpPoint, point, pLayer, pFEEvent->pbReturnImmediately);
			else if (pEvent->which == 3) 
				OnRButtonUpForLayerCX(uFlags, cpPoint, point, pLayer);
			break;
		case CL_EVENT_MOUSE_MOVE:
	                OnMouseMoveForLayerCX(uFlags, cpPoint, point, pLayer, pFEEvent->pbReturnImmediately);
			break;
		case CL_EVENT_MOUSE_BUTTON_MULTI_CLICK:
			if (pEvent->which == 1)
				OnLButtonDblClkForLayerCX(uFlags, cpPoint, point, pLayer);
			else if (pEvent->which == 3) 
				OnRButtonDblClkForLayerCX(uFlags, cpPoint, point, pLayer);
            break;
        case CL_EVENT_KEY_FOCUS_GAINED:
            bReturn = TRUE;
            break;
        case CL_EVENT_KEY_UP:
	    pElement = GetLayoutElement(point, pLayer);
	    // Check for form element and send event back to form element's class.
	    if (pElement != NULL && pElement->type == LO_FORM_ELE && pFEEvent->x == 1) {
		switch (pElement->lo_form.element_data->type) {
		    case FORM_TYPE_TEXT:
		    case FORM_TYPE_PASSWORD:
		    case FORM_TYPE_TEXTAREA:
		    case FORM_TYPE_FILE:
			pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
			((CNetscapeEdit *)CEdit::FromHandlePermanent(pFormElement->GetRaw()))->OnEditKeyEvent(pEvent->type, pEvent->which, nRepCnt, nFlags);
			break;
		    default:
			break;
		}
	    } 
	    break;
        case CL_EVENT_KEY_DOWN:
	    pElement = GetLayoutElement(point, pLayer);
	    // Check for form element and send event back to form element's class.
	    if (pElement != NULL && pElement->type == LO_FORM_ELE && pFEEvent->x == 1) {
		switch (pElement->lo_form.element_data->type) {
		    case FORM_TYPE_TEXT:
		    case FORM_TYPE_PASSWORD:
		    case FORM_TYPE_TEXTAREA:
		    case FORM_TYPE_FILE:
			pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
			((CNetscapeEdit *)CEdit::FromHandlePermanent(pFormElement->GetRaw()))->OnEditKeyEvent(pEvent->type, pEvent->which, nRepCnt, nFlags);
			break;
		    default:
			break;
		}
	    }
	    else {
		switch(nChar) {
		    case ' ':
		    case VK_NEXT:
			// page down
			Scroll(SB_VERT, SB_PAGEDOWN, 0, NULL);
			break;
		    case VK_BACK:
		    case VK_PRIOR:
			// page up
			Scroll(SB_VERT, SB_PAGEUP, 0, NULL);
			break;
		    case VK_UP:
			// line up
			Scroll(SB_VERT, SB_LINEUP, 0, NULL);
			break;
		    case VK_DOWN:
			// line down
			Scroll(SB_VERT, SB_LINEDOWN, 0, NULL);
			break;
		    case VK_RIGHT:
			// line right
			Scroll(SB_HORZ, SB_LINERIGHT, 0, NULL);
			break;
		    case VK_LEFT:
			// line left
			Scroll(SB_HORZ, SB_LINELEFT, 0, NULL);
			break;
		    case VK_HOME:
			    if (::GetKeyState(VK_CONTROL) < 0)
				    Scroll(SB_VERT, SB_TOP, 0, NULL);
			    else
				    Scroll(SB_HORZ, SB_TOP, 0, NULL);
			    break;
		    case VK_END:
			    if (::GetKeyState(VK_CONTROL) < 0)
				    Scroll(SB_VERT, SB_BOTTOM, 0, NULL);
			    else
				    Scroll(SB_HORZ, SB_BOTTOM, 0, NULL);
			    break;

		    case VK_ESCAPE:
			    //  escape, kill off any selected items.
			    if(m_pSelected != NULL) {
				OnDeactivateEmbedCX();
			    }
			break;
		    default:
			break;
		}
	    }
	    break;
        case CL_EVENT_KEY_FOCUS_LOST:
        default:
            bReturn = FALSE;
            break;
    }

    return bReturn;
}

BOOL CWinCX::HandleEmbedEvent(LO_EmbedStruct *embed, CL_Event *pEvent)
{
    NPEvent npEvent;
    fe_EventStruct *pFEEvent = (fe_EventStruct *)pEvent->fe_event;
    NPEmbeddedApp *pEmbeddedApp = (NPEmbeddedApp *)embed->FE_Data;

    if (CL_IS_MOUSE_EVENT(pEvent)) {
        if (pFEEvent)
            npEvent.wParam = pFEEvent->uFlags;
        else 
            npEvent.wParam = m_uMouseFlags;
        
        npEvent.lParam = MAKELONG(pEvent->x, pEvent->y);
    }
    else if (CL_IS_KEY_EVENT(pEvent)) {
        npEvent.wParam = (uint32)pEvent->which;
        npEvent.lParam = (uint32)pEvent->modifiers;
    }
    else if (pEvent->type == CL_EVENT_MOUSE_ENTER) {
        npEvent.wParam = 0;
        npEvent.lParam = MAKELONG(HTCLIENT, 0);
    }
    else {
        npEvent.wParam = 0;
        npEvent.lParam = 0;
    }
   
    switch(pEvent->type) {
		case CL_EVENT_MOUSE_BUTTON_DOWN:
            if (pEvent->which == 1) {
                npEvent.event = WM_LBUTTONDOWN;
            }
            else if (pEvent->which == 2)
                npEvent.event = WM_MBUTTONDOWN;
            else
                npEvent.event = WM_RBUTTONDOWN;
            break;
		case CL_EVENT_MOUSE_BUTTON_UP:
            if (pEvent->which == 1)
                npEvent.event = WM_LBUTTONUP;
            else if (pEvent->which == 2)
                npEvent.event = WM_MBUTTONUP;
            else
                npEvent.event = WM_RBUTTONUP;
            break;
		case CL_EVENT_MOUSE_MOVE:
            npEvent.event = WM_MOUSEMOVE;
            break;
        case CL_EVENT_MOUSE_BUTTON_MULTI_CLICK:
            if (pEvent->which == 1)
                npEvent.event = WM_LBUTTONDBLCLK;
            else if (pEvent->which == 2)
                npEvent.event = WM_MBUTTONDBLCLK;
            else
                npEvent.event = WM_RBUTTONDBLCLK;
            break;
        case CL_EVENT_KEY_UP:
            npEvent.event = WM_KEYUP;
            break;
        case CL_EVENT_KEY_DOWN:
            npEvent.event = WM_KEYDOWN;
            break;
        case CL_EVENT_MOUSE_ENTER:
            npEvent.event = WM_SETCURSOR;
            break;
        case CL_EVENT_KEY_FOCUS_GAINED:
            npEvent.event = WM_SETFOCUS;
            break;
        case CL_EVENT_KEY_FOCUS_LOST:
            npEvent.event = WM_KILLFOCUS;
            break;
        default:
            return FALSE;
    }
    
    return (BOOL)NPL_HandleEvent(pEmbeddedApp, &npEvent, (void*)npEvent.wParam);
}
#endif /* LAYERS */

BOOL CWinCX::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    BOOL bReturn = FALSE;
    
	//	Don't continue if this context is destroyed.
	if(IsDestroyed())	{
		return bReturn;
	}

#ifdef LAYERS
    /* 
     * If there's a compositor and someone has keyboard focus.
     * Note that if noone has event focus, we set the event focus
     * to the main document.
     */
    if (GetContext()->compositor) {
	if (CL_IsKeyEventGrabber(GetContext()->compositor, NULL) && 
	    CL_GetCompositorRoot(GetContext()->compositor)) {

	    CL_GrabKeyEvents(GetContext()->compositor, CL_GetLayerChildByName(
		CL_GetCompositorRoot(GetContext()->compositor), LO_BODY_LAYER_NAME));
	}

	if (!CL_IsKeyEventGrabber(GetContext()->compositor, NULL)) {

	    CL_Event event;
	    fe_EventStruct fe_event;

	    //	Convert the point to something we understand.
	    XY Point;
	    WORD asciiChar = 0;
	     
	    ResolvePoint(Point, m_cpMMove);

        if (!EDT_IS_EDITOR(GetContext())) {
    	    BYTE kbstate[256];
            GetKeyboardState(kbstate);
#ifdef WIN32	
	        ToAscii(nChar, nFlags & 0xff, kbstate, &asciiChar, 0);
#else
	        ToAscii(nChar, nFlags & 0xff, kbstate, (DWORD*)&asciiChar, 0);
#endif
        }

	    fe_event.fe_modifiers = MAKELONG(nRepCnt, nFlags);	    
	    fe_event.nChar = nChar;
	    fe_event.x = 0;

	    event.type = CL_EVENT_KEY_UP;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.which = asciiChar;
	    event.modifiers = (GetKeyState(VK_SHIFT) < 0 ? EVENT_SHIFT_MASK : 0) 
			    | (GetKeyState(VK_CONTROL) < 0 ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	    event.x = Point.x;
	    event.y = Point.y;
    
	    bReturn = (BOOL)CL_DispatchEvent(GetContext()->compositor, &event);
	}
    }
#endif /* LAYERS */

    return bReturn;
}

BOOL CWinCX::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    BOOL bReturn = FALSE;
    
	//	Don't continue if this context is destroyed.
	if(IsDestroyed())	{
		return bReturn;
	}

#ifdef LAYERS
    /* 
     * If there's a compositor and someone has keyboard focus.
     * Note that if noone has event focus, we set the event focus
     * to the main document.
     */
    if (GetContext()->compositor) {
	if (CL_IsKeyEventGrabber(GetContext()->compositor, NULL) && 
	    CL_GetCompositorRoot(GetContext()->compositor)) {

	    CL_GrabKeyEvents(GetContext()->compositor, CL_GetLayerChildByName(
		CL_GetCompositorRoot(GetContext()->compositor), LO_BODY_LAYER_NAME));

	}
	
	if (!CL_IsKeyEventGrabber(GetContext()->compositor, NULL)) {

	    CL_Event event;
	    fe_EventStruct fe_event;

	    //	Convert the point to something we understand.
	    XY Point;
	    WORD asciiChar = 0;
	     
	    ResolvePoint(Point, m_cpMMove);

        if (!EDT_IS_EDITOR(GetContext())) {
    	    BYTE kbstate[256];
            GetKeyboardState(kbstate);
#ifdef WIN32	
	        ToAscii(nChar, nFlags & 0xff, kbstate, &asciiChar, 0);
#else
    	    ToAscii(nChar, nFlags & 0xff, kbstate, (DWORD*)&asciiChar, 0);
#endif
        }

		
	    fe_event.fe_modifiers = MAKELONG(nRepCnt, nFlags);	    
	    fe_event.nChar = nChar;
	    fe_event.x = 0;

	    event.type = CL_EVENT_KEY_DOWN;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.which = asciiChar;
	    event.modifiers = (GetKeyState(VK_SHIFT) < 0 ? EVENT_SHIFT_MASK : 0) 
			    | (GetKeyState(VK_CONTROL) < 0 ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	    event.x = Point.x;
	    event.y = Point.y;
	    event.data = nFlags>>14 & 1;//Bit represeting key repetition

	    bReturn = (BOOL)CL_DispatchEvent(GetContext()->compositor, &event);
	}
    }
#endif /* LAYERS */

    return bReturn;
}

void CWinCX::OnLButtonDblClkCX(UINT uFlags, CPoint cpPoint)	{
	//	Only do this if clicking is enabled.
	if(IsClickingEnabled() == FALSE)	{
		return;
	}

	//	Don't continue if this context is destroyed.
	if(IsDestroyed())	{
		return;
	}

	//	Remember....
    m_LastMouseEvent = m_LBDClick;
	m_cpLBDClick = cpPoint;
	m_uMouseFlags = uFlags;

	//	Convert the point to something we understand.
	XY Point;
	ResolvePoint(Point, cpPoint);

#ifdef LAYERS
	if (GetContext()->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    
	    event.type = CL_EVENT_MOUSE_BUTTON_MULTI_CLICK;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	    event.which = 1;
	    event.data = 2;
	
	    CL_DispatchEvent(GetContext()->compositor,
			     &event);
	}
	else 
	    OnLButtonDblClkForLayerCX(uFlags, cpPoint, Point, NULL);

    //  Have the mouse timer handler do some dirty work.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
    return;
}

void 
CWinCX::OnLButtonDblClkForLayerCX(UINT uFlags, CPoint& cpPoint,
				  XY& Point, CL_Layer *layer)
{
// With LAYERS turned on, the orginal method 
// OnLButtonDblClkCX is separated into two methods,
// one of which is a per-layer method.
#endif /* LAYERS */

	//	Process any embed activation.
#ifdef LAYERS
	LO_Element *pElement = GetLayoutElement(Point, layer);
#else
	LO_Element *pElement = GetLayoutElement(Point);
#endif

	if (pElement != NULL && pElement->type == LO_FORM_ELE &&
		(Point.x - pElement->lo_form.x - pElement->lo_form.x_offset < pElement->lo_form.width) &&
		(Point.x - pElement->lo_form.x - pElement->lo_form.x_offset > 0) &&
		(Point.y - pElement->lo_form.y - pElement->lo_form.y_offset < pElement->lo_form.height) &&
		(Point.y - pElement->lo_form.y - pElement->lo_form.y_offset > 0)) {
       
	   CFormElement * pFormElement;
	   CNetscapeButton *pButton;
	   switch (pElement->lo_form.element_data->type) {
	       case FORM_TYPE_BUTTON:
	       case FORM_TYPE_RESET:
	       case FORM_TYPE_SUBMIT:
	       case FORM_TYPE_CHECKBOX:
	       case FORM_TYPE_RADIO:
		   pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
		   ((CNetscapeButton *)CButton::FromHandlePermanent(pFormElement->GetRaw()))
			    ->OnButtonEvent(CL_EVENT_MOUSE_BUTTON_MULTI_CLICK, uFlags, cpPoint);
		   break;
	       case FORM_TYPE_FILE:
		   pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
		   pButton = (CNetscapeButton *)pFormElement->GetSecondaryWidget();
		   if (pButton)
		       pButton->OnButtonEvent(CL_EVENT_MOUSE_BUTTON_MULTI_CLICK, uFlags, cpPoint);
		   break;
	       default:
		   break;
	   }
	return;
	}

	if(pElement != NULL && pElement->type == LO_EMBED)	{
		//	Any previous activated embed was deactivated in the button down event.
		//	However, we should not reactivate an already active item.
        NPEmbeddedApp* pEmbeddedApp = (NPEmbeddedApp*)pElement->lo_embed.FE_Data;
        ASSERT(pEmbeddedApp);
		CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;
		if(pItem != NULL)	{
			if(pItem->m_bBroken == FALSE && pItem->m_bDelayed == FALSE &&
				pItem->m_bLoading == FALSE)	{
				if(pItem->IsInPlaceActive() == FALSE)	{
					//	Set the active item.
					//	This value is held in the view for now....
                    CGenericView *pView = GetView();
                    if(pView)   {
					    m_pSelected = &(pElement->lo_embed);

					    long lVerb = OLEIVERB_PRIMARY;
					    if(uFlags & MK_CONTROL)	{
						    lVerb = OLEIVERB_OPEN;
					    }

					    pView->BeginWaitCursor();
					    TRY	{
						    pItem->Activate(lVerb, pView);

					    }
					    CATCH(CException, e)	{
						    //	Object wouldn't activate or something went wrong,
						    //		and almost caused us to go down with it.
						    m_pSelected = NULL;
					    }
					    END_CATCH
					    pView->EndWaitCursor();
					    //	If it's not in place active at this point, there's no need
					    //		to keep track of it.
					    if(m_pSelected != NULL && pItem->IsInPlaceActive() == FALSE)	{
						    m_pSelected = NULL;
					    }
                    }
				}
			}
		    }

        // Dbl-click is not the same as holding mouse down
        m_bLBDown = FALSE;
        m_bLBUp = TRUE;
    }
    else {
     	if(GetPane())	{
            ::SetCapture(GetPane());
    	}

#ifdef EDITOR
        if ( EDT_IS_EDITOR(GetContext()) )
        {
            // Let the editor handle the double-click
            EDT_DoubleClick(GetContext(), Point.x, Point.y);
            // Dbl-click is NOT the same as holding mouse down
            m_bLBDown = FALSE;
            m_bLBUp = TRUE;
        }
        else
        {
#ifdef LAYERS
            LO_DoubleClick(GetDocumentContext(), Point.x, Point.y, layer);
#else
            LO_DoubleClick(GetDocumentContext(), Point.x, Point.y);
#endif /* LAYERS */
            // Double-click is the same as holding mouse down when
            // we're selecting. 
            //cmanske: WHY??? DOES THE BROWSER NEED THIS? BAD FOR EDITOR!
#endif // EDITOR
            m_bLBDown = TRUE;
            m_bLBUp = FALSE;
#ifdef EDITOR
        }
#endif
    }

    //  Have the mouse timer handler do some dirty work.
    //  Please don't return in the above code, I'd like this to get called
    //      in all cases with the state of the buttons set correctly.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
}

BOOL CWinCX::PtInSelectedCell(CPoint &DocPoint, LO_CellStruct *cell, 
                              BOOL &bContinue, LO_Element *start_element,
                              LO_Element *end_element)
{
    BOOL bPtInRegion = FALSE;

    for ( LO_Any_struct * element = (LO_Any_struct *)cell->cell_list; ;
              element = (LO_Any_struct *)(element->next) ) {
        if( element == 0 ){
            bContinue = TRUE;
            return FALSE;
        }

        // Linefeed rect is from end of text to right ledge,
        //  so lets ignore it
        if ( element->type != LO_LINEFEED ) {
            if ( element->type == LO_TEXT && 
                 (element == (LO_Any_struct*)start_element || 
                  element == (LO_Any_struct*)end_element) ) {
                // With 1st and last text elements, we need to
                // account for character offsets from start or end of selection
                LO_TextStruct *text = (LO_TextStruct*)element;
                LTRB rect;
                // We may have a null text element in Tables,
                //  so use closest non-null text element
                if( text->text == NULL){
                    if( text->prev != NULL && text->prev->type == LO_TEXT ){
                        text = (LO_TextStruct*)text->prev;
                    } else if( text->next != NULL && text->next->type == LO_TEXT ){
                        text = (LO_TextStruct*)text->next;
                    }
                }
                if( text->text ){
                    ResolveElement( rect, text,
                                    (int)(text->x + text->x_offset),     // Start location
                                    (int32)(text->sel_start), 
                                    (int32)(text->sel_end), FALSE ); 
                    int x = CASTINT(DocPoint.x - GetOriginX());
                    int y = CASTINT(DocPoint.y - GetOriginY());
                    bPtInRegion = x > rect.left &&
                        x < rect.right &&
                        y > rect.top &&
                        y < rect.bottom;
                } else {
                    bPtInRegion = FALSE;
                }
            } 
            else if (element->type == LO_CELL) {
                bPtInRegion = PtInSelectedCell(DocPoint, 
                                               (LO_CellStruct *)element,
                                               bContinue, start_element,
                                               end_element);
            }
            else if (element->type != LO_TABLE) {
                // Get the rect surrounding selected element,
                CRect cRect;
                cRect.left = CASTINT(element->x + element->x_offset);
                cRect.top = CASTINT(element->y + element->y_offset);
                cRect.right = CASTINT(cRect.left + element->width);
                cRect.bottom = CASTINT(cRect.top + element->height);
                bPtInRegion = cRect.PtInRect( DocPoint );
            }
        }
        // We're done if we are in a rect or finished with last element
        if ( bPtInRegion || !bContinue || 
             element == (LO_Any_struct*)end_element ) {
            break;
        }
    }
    
    bContinue = FALSE;
    return bPtInRegion;
}

//  Test if point, such mouse cursor, is within the selected region
BOOL CWinCX::PtInSelectedRegion(CPoint cPoint, BOOL bConvertToDocCoordinates,
                                CL_Layer *layer)
{
    BOOL bPtInRegion = FALSE;
    BOOL bContinue = TRUE;

    CPoint DocPoint;
    if( bConvertToDocCoordinates ){
    	XY Point;
	    ResolvePoint(Point, cPoint);
        DocPoint.x = CASTINT(Point.x);
        DocPoint.y = CASTINT(Point.y);
    } else {
        DocPoint = cPoint;
    }

    int32 start_selection, end_selection;
	LO_Element * start_element = NULL;
	LO_Element * end_element = NULL;
    CL_Layer *sel_layer = NULL;
    int32 x_origin, y_origin, old_x_origin, old_y_origin;

	// Start the search from the current selection location	
	LO_GetSelectionEndpoints(GetDocumentContext(), 
	                     &start_element, 
	                     &end_element, 
	                     &start_selection, 
	                     &end_selection,
                         &sel_layer);
    
    if ( start_element == NULL ) {
        return FALSE;
    } 
    
    /* 
     * If the selection layer is different from the one in which
     * the event occured, there isn't a match.
     */
    if ( layer && (layer != sel_layer) ) {
        return FALSE;
    }
       
    /* 
     * Temporarily change the drawing origin to that of the selection
     * layer so that resolving the positions of the elements will 
     * result in correct coordinate space translation.
     */
    if ( sel_layer ) {
        x_origin = CL_GetLayerXOrigin(sel_layer);
        y_origin = CL_GetLayerYOrigin(sel_layer);
    }
    else {
        x_origin = 0;
        y_origin = 0;
    }
    
    CDrawable *pDrawable = GetDrawable();
    if (pDrawable) {
        pDrawable->GetOrigin(&old_x_origin, &old_y_origin);
        pDrawable->SetOrigin(x_origin, y_origin);
    }
    
    for ( LO_Any_struct * element = (LO_Any_struct *)start_element; ;
          element = (LO_Any_struct *)(element->next) ) {
        // KLUDGE: This prevents crashing when multiple selection
        //         within cells of a table
        if( element == 0 ){
            if ( pDrawable ) {
                pDrawable->SetOrigin(old_x_origin, old_y_origin);
            }
            return FALSE;
        }
            // Linefeed rect is from end of text to right ledge,
            //  so lets ignore it
        if ( element->type != LO_LINEFEED ) {
            if ( element->type == LO_TEXT && 
                 (element == (LO_Any_struct*)start_element || 
                  element == (LO_Any_struct*)end_element) ) {
                // With 1st and last text elements, we need to
                // account for character offsets from start or end of selection
                LO_TextStruct *text = (LO_TextStruct*)element;
                LTRB rect;
                // We may have a null text element in Tables,
                //  so use closest non-null text element
                if( text->text == NULL){
                    if( text->prev != NULL && text->prev->type == LO_TEXT ){
                        text = (LO_TextStruct*)text->prev;
                    } else if( text->next != NULL && text->next->type == LO_TEXT ){
                        text = (LO_TextStruct*)text->next;
                    }
                }
                if( text->text ){
                    ResolveElement( rect, text,
                                    (int)(text->x + text->x_offset),     // Start location
                                    (int32)(text->sel_start), 
                                    (int32)(text->sel_end), FALSE ); 
                    int x = CASTINT(DocPoint.x - GetOriginX());
                    int y = CASTINT(DocPoint.y - GetOriginY());
                    bPtInRegion = x > rect.left &&
                                  x < rect.right &&
                                  y > rect.top &&
                                  y < rect.bottom;
                } else {
                    bPtInRegion = FALSE;
                }
            } 
            else if (element->type == LO_CELL) {
                bPtInRegion = PtInSelectedCell(DocPoint, 
                                               (LO_CellStruct *)element,
                                               bContinue, start_element,
                                               end_element);
            }
            else if (element->type != LO_TABLE) {
                // Get the rect surrounding selected element,
                CRect cRect;
                cRect.left = CASTINT(element->x + element->x_offset);
                cRect.top = CASTINT(element->y + element->y_offset);
                cRect.right = CASTINT(cRect.left + element->width);
                cRect.bottom = CASTINT(cRect.top + element->height);
                bPtInRegion = cRect.PtInRect( DocPoint );
            }
        }
        // We're done if we are in a rect or finished with last element
        if ( bPtInRegion || !bContinue || 
             element == (LO_Any_struct*)end_element ) {
            break;
        }
    }

    if ( pDrawable ) {
        pDrawable->SetOrigin(old_x_origin, old_y_origin);
    }
        
    return bPtInRegion;
}

void CWinCX::OnLButtonDownCX(UINT uFlags, CPoint cpPoint)
{
	RelayToolTipEvent(cpPoint, WM_LBUTTONDOWN);

	//	Only do this if clicking is enabled.
	if(IsClickingEnabled() == FALSE)	{
		return;
	}

	//	Don't continue if this context is destroyed.
	if(IsDestroyed())	{
		return;
	}

	//	Start capturing all mouse events.
	if(GetPane())   {
        ::SetCapture(GetPane());
	}

	XY Point;
	ResolvePoint(Point, cpPoint);

#ifdef LAYERS
	
	if (GetContext()->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    
	    event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	    event.x = Point.x;
	    event.y = Point.y;
	    event.which = 1;
	
	    CL_DispatchEvent(GetContext()->compositor,
			     &event);
	}
	else 
	    OnLButtonDownForLayerCX(uFlags, cpPoint, Point, NULL);

    //  Have the mouse timer handler do some dirty work.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
    return;
}

void CWinCX::ResetToolTipImg() {
	pLastToolTipImg = 0;
	m_pLastToolTipAnchor = NULL;
	if (m_ToolTip) {
		m_ToolTip->Activate(FALSE);
		delete m_ToolTip;
		m_ToolTip = 0;
	}
}


void CWinCX::RelayToolTipEvent(POINT pt, UINT message)
{
	if (m_ToolTip) {
		MSG msg;
		msg.message = message;
		msg.hwnd = GetPane();
		msg.pt = pt;
		::ClientToScreen(msg.hwnd, &msg.pt);
		m_ToolTip->RelayEvent(&msg);
	}
}

void
CWinCX::OnLButtonDownForLayerCX(UINT uFlags, CPoint &cpPoint, XY& Point, 
				CL_Layer *layer)
{
    MWContext *pMWContext = GetContext();
    XP_ASSERT(pMWContext);

// With LAYERS turned on, the orginal method 
// OnLButtonDownCX is separated into two methods,
// one of which is a per-layer method.
    if (pMWContext->compositor)
      CL_GrabMouseEvents(pMWContext->compositor, layer);
#endif /* LAYERS */

#ifdef LAYERS
	LO_Element *pElement = GetLayoutElement(Point, layer);
#else
	LO_Element *pElement = GetLayoutElement(Point);
#endif /* LAYERS */

    // Check for form element and send event back to form element's class.
   if (pElement != NULL && pElement->type == LO_FORM_ELE &&
		(Point.x - pElement->lo_form.x - pElement->lo_form.x_offset < pElement->lo_form.width) &&
		(Point.x - pElement->lo_form.x - pElement->lo_form.x_offset > 0) &&
		(Point.y - pElement->lo_form.y - pElement->lo_form.y_offset < pElement->lo_form.height) &&
		(Point.y - pElement->lo_form.y - pElement->lo_form.y_offset > 0)) {
       
       CFormElement * pFormElement;
       CNetscapeButton * pButton;
       switch (pElement->lo_form.element_data->type) {
           case FORM_TYPE_BUTTON:
           case FORM_TYPE_RESET:
           case FORM_TYPE_SUBMIT:
           case FORM_TYPE_CHECKBOX:
           case FORM_TYPE_RADIO:
               pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
               ((CNetscapeButton *)CButton::FromHandlePermanent(pFormElement->GetRaw()))
			    ->OnButtonEvent(CL_EVENT_MOUSE_BUTTON_DOWN, uFlags, cpPoint);
               break;
	   case FORM_TYPE_FILE:
               pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
	       pButton = (CNetscapeButton *)pFormElement->GetSecondaryWidget();
	       if (pButton)
		    pButton->OnButtonEvent(CL_EVENT_MOUSE_BUTTON_DOWN, uFlags, cpPoint);
               break;
           default:
               break;
	}
	return;
   }

    //	Wait to set these until we know the event hasn't be cancelled
    //  by JS or really meant for a form element.  Remember....
    m_LastMouseEvent = m_LBDown;
    m_cpLBDown = cpPoint;
    m_uMouseFlags = uFlags;
    m_bLBDown = TRUE;
    m_bLBUp = FALSE;


    //	Deactivate OLE embedded items if active.
    CGenericView *pView = GetView();
	if(pView != NULL && pElement != (LO_Element *)m_pSelected && m_pSelected != NULL)	{
		OnDeactivateEmbedCX();
	}
    
#ifdef EDITOR
    ED_HitType iTableHit = ED_HIT_NONE;
    LO_Element * pTableOrCellElement = NULL;

    if( EDT_IS_EDITOR(pMWContext) )
    {
        m_pStartSelectionCell = NULL;

        // Check if user pressed down near a sizeable object border
        //   and start sizing mode if we are
        // Note: X, Y values are in Document coordinates
        if( !m_bDragging && !EDT_IsSizing(pMWContext) )
        {
            BOOL bSizeTable = FALSE;

            // Left button down test for selection/sizing/dragging
            iTableHit = EDT_GetTableHitRegion( pMWContext, Point.x, Point.y, &pTableOrCellElement, (uFlags & MK_CONTROL) );

            // Check if we can select or size a Table or Cell,
            //   but not if Alt key is pressed, ignore the table
            //   to allow sizing objects tightly surrounded by Cell border
            if( GetAsyncKeyState(VK_MENU) >= 0 &&
                pTableOrCellElement )
            {
                // Save cell we are in
                if( pTableOrCellElement->type == LO_CELL )
                    m_pStartSelectionCell = pTableOrCellElement;

                if( iTableHit == ED_HIT_SIZE_TABLE_WIDTH || iTableHit == ED_HIT_SIZE_TABLE_HEIGHT ||
                    iTableHit == ED_HIT_SIZE_COL || iTableHit == ED_HIT_SIZE_ROW ||
                    iTableHit == ED_HIT_ADD_ROWS || iTableHit == ED_HIT_ADD_COLS )
                {
                    // We are sizing a table, row, or column
                    pElement = pTableOrCellElement;
                    bSizeTable = TRUE;
                } else if( iTableHit != ED_HIT_NONE && iTableHit != ED_HIT_DRAG_TABLE )
                {
                    m_bSelectingCells = TRUE;
                    // Mouse is in a selectable region for table, row, column, or cell.
                    // If Ctrl key is down and cell is selected, it is appended to other table cells selected
                    // Otherwise, any other cell or table selected is cleared before new element is selected.
                    // If Shift key is down, then extend selection to new cell
                    EDT_SelectTableElement(pMWContext, Point.x, Point.y, pTableOrCellElement, iTableHit, 
                                           (uFlags & MK_CONTROL), (uFlags & MK_SHIFT));
                    goto MOUSE_TIMER;
                }
            }            

            if( bSizeTable ||
                (pElement && EDT_CanSizeObject(pMWContext, pElement, Point.x, Point.y)) )
            {
                // Flag to override the normal lock when sizing corners
                BOOL bLock = !(BOOL)(uFlags & MK_CONTROL);
                XP_Rect rect;
                if( EDT_StartSizing(pMWContext, pElement, Point.x, Point.y, bLock, &rect) )
                {
                    // Force redraw of table or cells that might have been selected
                    //   else we have NOT conflicts and garbage at overlaps
                    UpdateWindow(GetPane());
                    // Save the new rect.
                    m_rectSizing.left = rect.left;
                    m_rectSizing.right = rect.right;
                    m_rectSizing.top = rect.top;
                    m_rectSizing.bottom = rect.bottom;
                    // Draw the initial feedback -- similar to when selected
                    DisplaySelectionFeedback(LO_ELE_SELECTED, m_rectSizing);
                    goto MOUSE_TIMER;
                }
            }
        }
    }
    // Drag Copy/Move - do only if we will not be sizing
    if( GetView()->IsKindOf(RUNTIME_CLASS(CNetscapeView)) &&
        (iTableHit == ED_HIT_DRAG_TABLE || PtInSelectedRegion(cpPoint, TRUE, layer)) ) 
    {
        // Setup to possibly do Drag Copy/Move
        // Check these flags during mouse move 
        //   and start drag only if mouse moved enough
        if( iTableHit == ED_HIT_DRAG_TABLE )
        {
            // Setup XP data for possible dragging of table or cells
            EDT_StartDragTable(pMWContext, Point.x, Point.y);
        }
        m_bMouseInSelection = TRUE;

    } else
#endif // EDITOR
    if ( ! m_bDragging ) {
        // If the shift key is down, we need to extend the selection.
    	if( (uFlags & MK_SHIFT) ) {
#ifdef EDITOR
            if( EDT_IS_EDITOR(pMWContext) ){
        		EDT_ExtendSelection(pMWContext, Point.x, Point.y);
            } 
            else
#endif // EDITOR
            {
        		LO_ExtendSelection(GetDocumentContext(), Point.x, Point.y);
            }
        } else {
            // Start a normal selection
#ifdef EDITOR
            if( EDT_IS_EDITOR(pMWContext) ) {
    	    	EDT_StartSelection(pMWContext, Point.x, Point.y);
            } 
            else
#endif // EDITOR
            {
#ifdef LAYERS 
            	LO_StartSelection(GetDocumentContext(), Point.x, Point.y, layer);
#else
        		LO_StartSelection(GetDocumentContext(), Point.x, Point.y);
#endif /* LAYERS */
            }
        }
    }
	//	Highlight an anchor if we are over it.

    // Save an image element so we can drag into editor
    if ( pElement != NULL && pElement->type == LO_IMAGE ) {
        m_pLastImageObject = (LO_ImageStruct*)pElement;
    }

    // DON'T drag links when in editor - just highlight the text in the link
    if(!EDT_IS_EDITOR(pMWContext) &&
       pElement != NULL && pElement->type == LO_TEXT && pElement->lo_text.anchor_href != NULL) {
		m_pLastArmedAnchor = pElement;
		LO_HighlightAnchor(GetDocumentContext(), pElement, TRUE);
	}

MOUSE_TIMER:
    //  Have the mouse timer handler do some dirty work.
    //  Please don't return in the above code, I'd like this to get called
    //      in all cases with the state of the buttons set correctly.
    MouseTimerData mt(pMWContext);
    FEU_MouseTimer(&mt);
}

typedef struct click_closure {
    char   * szRefer;
    int	     x, y;
    CWinCX * pWin;
    BOOL     bCloseOnFail;
    LO_Element * pElement;
} click_closure;

static void
MapToAnchorAndTarget(MWContext * context, LO_Element * pElement, int x, int y, 
		     CString& csAnchor, CString& csTarget)
{

    switch(pElement->type) {

    case LO_TEXT:
	if(pElement->lo_text.anchor_href && pElement->lo_text.anchor_href->anchor) {
	    csAnchor = (char *) pElement->lo_text.anchor_href->anchor;
	    csTarget = (char *) pElement->lo_text.anchor_href->target;
	}
	break;

    case LO_IMAGE:
    	//	Check for usemaps (client side ismaps).
	if(pElement->lo_image.image_attr && pElement->lo_image.image_attr->usemap_name != NULL)	{
	    LO_AnchorData *pAnchorData = LO_MapXYToAreaAnchor(context, 
							&pElement->lo_image, x, y);
	    if(pAnchorData != NULL)	{
		csAnchor = (char *) pAnchorData->anchor;
		csTarget = (char *) pAnchorData->target;
	    }
	    break;
	}

	//	Check for ismaps, normal image anchors
	if(pElement->lo_image.anchor_href != NULL && 
	    pElement->lo_image.anchor_href->anchor != NULL) {
	    
	    if(pElement->lo_image.image_attr && pElement->lo_image.image_attr->attrmask & LO_ATTR_ISMAP) {

		char * pAnchor = PR_smprintf("%s?%d,%d", 
				pElement->lo_image.anchor_href->anchor, x, y);
		csAnchor = pAnchor;
		XP_FREE(pAnchor);
		csTarget =(char *)pElement->lo_image.anchor_href->target;
	    }
	    else {
		csAnchor = (char *)pElement->lo_image.anchor_href->anchor;
		csTarget = (char *)pElement->lo_image.anchor_href->target;
	    }
	}
	break;
	break;

    default:
	break;
    }

}


//
// Mocha has processed a click on an element.  If everything is OK
//   we now will do the actual load.  If libmocha said to not
//   load and we created a new window explicitly for the load
//   delete the window
//
static void
win_click_callback(MWContext * pContext, LO_Element * pEle, int32 event,
                     void * pObj, ETEventStatus status)
{

    CString csAnchor, csTarget;

    // make sure document hasn't gone away
    if(status == EVENT_PANIC) {
	if (((click_closure *)pObj)->pElement && pEle) {
	    XP_FREE(pEle);
	}
	XP_FREE(pObj);
        return;
    }

    // find out who we are
    click_closure * pClose = (click_closure *) pObj;
    CWinCX * pWin = pClose->pWin;

    // Imagemaps send click pretending to be links.  Free the link now and 
    // set the element back to the image it is.
    if (pClose->pElement) {
	if(pEle)
	    XP_FREE(pEle);
	pEle = pClose->pElement;
    }

    MapToAnchorAndTarget(pWin->GetContext(), pEle, pClose->x, pClose->y, csAnchor, csTarget);

#ifdef EDITOR
    if( EDT_IS_EDITOR(pWin->GetContext()) ){
        // Ctrl Click = edit the URL
        FE_LoadUrl((char*)LPCSTR(csAnchor), LOAD_URL_COMPOSER);
    	goto done;
    }
#endif // EDITOR

    if(status == EVENT_OK)  {
        CWinCX *pLoader = pWin->DetermineTarget(csTarget);
        pLoader->NormalGetUrl(csAnchor, pClose->szRefer, (csTarget.IsEmpty() ? NULL : (LPCSTR)csTarget));
    } else {
        if(pClose->bCloseOnFail)
            //  Make it go away if mocha said no.
            FE_DestroyWindow(pContext);
    }

done:
    if(pClose->szRefer)
        XP_FREE(pClose->szRefer);
    XP_FREE(pClose);

}

//
// The user clicked on a form image.  We told mocha about it and now mocha
//   is either going to tell us to submit the form or ignore it
//			 
static void
image_form_click_callback(MWContext * pContext, LO_Element * pElement, int32 event,
			  void * pObj, ETEventStatus status)
{

    // only continue if OK
    if(status != EVENT_OK) {
	XP_FREE(pObj);
	return;
    }

    click_closure * pClose = (click_closure *) pObj;
    CWinCX * pWin = pClose->pWin;

    LO_FormSubmitData *pSubmit = LO_SubmitImageForm(pContext, 
					            &pElement->lo_image, 
						    pClose->x, 
						    pClose->y);

    if(pSubmit == NULL)	{
	//	Nothing to do.
	return;
    }

    //	Have to do a manual load here.
    URL_Struct *pUrl = NET_CreateURLStruct((const char *)pSubmit->action, NET_DONT_RELOAD);
    NET_AddLOSubmitDataToURLStruct(pSubmit, pUrl);

    //	Set the referrer here manually too.
    if(pClose->szRefer)
        pUrl->referer = XP_STRDUP(pClose->szRefer);
    else
        pUrl->referer = NULL;

    //	Request.
    pWin->GetUrl(pUrl, FO_CACHE_AND_PRESENT);

    //	Release.
    LO_FreeSubmitData(pSubmit);

}


void CWinCX::OnLButtonUpCX(UINT uFlags, CPoint cpPoint, BOOL &bReturnImmediately)	
{
	RelayToolTipEvent(cpPoint, WM_LBUTTONUP);

	//	Only do this if clicking is enabled.
	if(IsClickingEnabled() == FALSE)	{
		return;
	}

	//	Release mouse capture.
	if(GetPane())	{
		::ReleaseCapture();
	}

	//	Don't continue if this context is destroyed.
	if(IsDestroyed())	{
		return;
	}

        XY Point;
	ResolvePoint(Point, cpPoint);
	
#ifdef LAYERS
	
	if (GetContext()->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    fe_event.pbReturnImmediately = bReturnImmediately;

	    event.type = CL_EVENT_MOUSE_BUTTON_UP;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	    event.which = 1;
	
	    CL_DispatchEvent(GetContext()->compositor,
			     &event);

	    bReturnImmediately = fe_event.pbReturnImmediately;
	}
	else 
	    OnLButtonUpForLayerCX(uFlags, cpPoint, Point, NULL, bReturnImmediately);

    //  Have the mouse timer handler do some dirty work.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
    return;
}


void
CWinCX::OnLButtonUpForLayerCX(UINT uFlags, CPoint& cpPoint, XY& Point,
			      CL_Layer *layer, BOOL &bReturnImmediately)
{

    History_entry *pHist = NULL;
    click_closure * pClosure = NULL;

    // With LAYERS turned on, the orginal method 
    // OnLButtonUpCX is separated into two methods,
    // one of which is a per-layer method.
    if (GetContext()->compositor)
        CL_GrabMouseEvents(GetContext()->compositor, NULL);
#endif /* LAYERS */

#ifdef LAYERS
	LO_Element *pElement = GetLayoutElement(Point, layer);
#else
	LO_Element *pElement = GetLayoutElement(Point);
#endif /* LAYERS */

   if (pElement != NULL && pElement->type == LO_FORM_ELE &&
		(Point.x - pElement->lo_form.x - pElement->lo_form.x_offset < pElement->lo_form.width) &&
		(Point.x - pElement->lo_form.x - pElement->lo_form.x_offset > 0) &&
		(Point.y - pElement->lo_form.y - pElement->lo_form.y_offset < pElement->lo_form.height) &&
		(Point.y - pElement->lo_form.y - pElement->lo_form.y_offset > 0)) {
       
       CFormElement * pFormElement;
       CNetscapeButton * pButton;
       switch (pElement->lo_form.element_data->type) {
           case FORM_TYPE_BUTTON:
           case FORM_TYPE_RESET:
           case FORM_TYPE_SUBMIT:
           case FORM_TYPE_CHECKBOX:
           case FORM_TYPE_RADIO:
               pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
               ((CNetscapeButton *)CButton::FromHandlePermanent(pFormElement->GetRaw()))
			    ->OnButtonEvent(CL_EVENT_MOUSE_BUTTON_UP, uFlags, cpPoint);
               break;
	   case FORM_TYPE_FILE:
               pFormElement=(CFormElement *)pElement->lo_form.element_data->ele_minimal.FE_Data;
	       pButton = (CNetscapeButton *)pFormElement->GetSecondaryWidget();
	       if (pButton)
		    pButton->OnButtonEvent(CL_EVENT_MOUSE_BUTTON_UP, uFlags, cpPoint);
               break;
           default:
               break;
	}
	m_bLBDown = FALSE;
	m_bLBUp = TRUE;
	return;

   }

    //	Wait to set these until we know the event hasn't be cancelled
    //  by JS or really meant for a form element.  Remember....
    m_LastMouseEvent = m_LBUp;
    m_cpLBUp = cpPoint;
    m_uMouseFlags = uFlags;
    m_bLBDown = FALSE;
    m_bLBUp = TRUE;

    // JS needs screen coords for click events.
    CPoint cpScreenPoint(cpPoint);
    ClientToScreen(GetPane(), &cpScreenPoint);

#ifdef EDITOR

    m_pStartSelectionCell = NULL;
    MWContext *pMWContext = GetContext();

    // Finish sizing an object only if we moved enough
    if( EDT_IS_SIZING ){
        // Remove last sizing feedback
        DisplaySelectionFeedback(LO_ELE_SELECTED, m_rectSizing);

        // We need this check or it is impossible to place a caret between
        //  adjacent objects)
        if(abs(m_cpLBUp.x - m_cpLBDown.x) > CLICK_THRESHOLD ||
	        abs(m_cpLBUp.y - m_cpLBDown.y) > CLICK_THRESHOLD)	{
            ASSERT(cpPoint.y == Point.y - GetOriginY()); // REMOVEME

            // Resize the object
            EDT_EndSizing(pMWContext);

            // See comment at end - shouldn't return here???
            //  (many other places below do return!)
            goto MOUSE_TIMER;
        } else {
            EDT_CancelSizing(pMWContext);
            // Move caret to where cursor is
            EDT_StartSelection(pMWContext, Point.x, Point.y);
        }
    } else if(!m_bDragging && !m_bSelectingCells) {
        // If within an existing selection, we didn't trigger StartSelection
        //   on MouseDown, so do it now
        if( m_bMouseInSelection ){
            if( EDT_IS_EDITOR(pMWContext) ) {
                EDT_StartSelection(pMWContext, Point.x, Point.y);
            } else {
#ifdef LAYERS 
        		LO_StartSelection(GetDocumentContext(), Point.x, Point.y, layer);
#else
    		    LO_StartSelection(GetDocumentContext(), Point.x, Point.y);
#endif /* LAYERS */
            }
        }
        // Cleanup selection - check if start=end
        if( EDT_IS_EDITOR(pMWContext) )
            EDT_EndSelection(pMWContext, Point.x, Point.y);

    }
    if( EDT_IS_EDITOR(pMWContext) )
    {
        if( EDT_CanPasteStyle(pMWContext) )
        {
            // Apply style if available
            EDT_PasteStyle(pMWContext, TRUE);
        }
        // Cancel any attempt to drag a table or cell
        EDT_StopDragTable(pMWContext);
    }
#endif // EDITOR

    m_bMouseInSelection = FALSE;
    m_bDragging = FALSE;
    m_bSelectingCells = FALSE;

    //	If we previously highlighted an anchor in button down, unhighlight it.
    if(m_pLastArmedAnchor != NULL)	{
        LO_HighlightAnchor(GetDocumentContext(), m_pLastArmedAnchor, FALSE);
        m_pLastArmedAnchor = NULL;
    }

    //	If the user moved the mouse beyond the clicking threshold, then consider
    //		this a text selection operation and don't continue.
    if(abs(m_cpLBUp.x - m_cpLBDown.x) > CLICK_THRESHOLD ||
	abs(m_cpLBUp.y - m_cpLBDown.y) > CLICK_THRESHOLD)	{
	    return;
    }
#ifdef EDITOR
    // Done with possible drag image
    m_pLastImageObject = NULL;
#endif

    //	If there's no element, there's no need to see if we should load something.
    //  Just send a JS Click event to the document and return.
    if(pElement == NULL) {
        JSEvent *event;
        event = XP_NEW_ZAP(JSEvent);
        event->type = EVENT_CLICK;
        event->which = 1;
        event->x = Point.x;
        event->y = Point.y;
        event->docx = Point.x + CL_GetLayerXOrigin(layer);
        event->docy = Point.y + CL_GetLayerYOrigin(layer);
        event->screenx = cpScreenPoint.x;
        event->screeny = cpScreenPoint.y;
        event->layer_id = LO_GetIdFromLayer(GetContext(), layer);

        ET_SendEvent(GetContext(), NULL, event, NULL, NULL);
        return;
    }

// 
// Control click to chase anchors in the editor
//
#ifdef EDITOR
    if((uFlags & MK_CONTROL) == 0 && EDT_IS_EDITOR(GetContext())){
        m_pLastImageObject = NULL;
        return;
    }
#endif

    //	It's probably we're loading.
    //	Figure out what we'll be sending as the referrer field.
    pHist = SHIST_GetCurrent(&GetContext()->hist);
    pClosure = XP_NEW_ZAP(click_closure);
    pClosure->pWin = this;
    pClosure->pElement = NULL;
    if(pHist != NULL && pHist->address != NULL)
        pClosure->szRefer = strdup(pHist->origin_url ? pHist->origin_url : pHist->address);
    else
	pClosure->szRefer = NULL;

    //	To find out what to do, switch on the element's type.
    switch(pElement->type)
    {
        case LO_TEXT:
            if(pElement->lo_text.anchor_href && pElement->lo_text.anchor_href->anchor)
            {
                // set tab_focus to this text element with a link.
                //#52932 setFormElementTabFocus( (LO_Element *) pElement );

                // if the shift-key is down save the object --- don't load it
                if(uFlags & MK_SHIFT && NOT_A_DIALOG(this))
                {
                    //  Should Mocha block save operations too?
                    CSaveCX::SaveAnchorObject((const char *)pElement->lo_text.anchor_href->anchor, NULL);
                    XP_FREE(pClosure);
                    return;
                } 

                // we are about to follow a link via a user click -- 
                //   tell the mocha library.  Don't follow the link
                //   until we get to our closure
                JSEvent *event;
                event = XP_NEW_ZAP(JSEvent);
                event->type = EVENT_CLICK;
                event->which = 1;
                event->x = Point.x;
                event->y = Point.y;
                event->docx = Point.x + CL_GetLayerXOrigin(layer);
                event->docy = Point.y + CL_GetLayerYOrigin(layer);
                event->screenx = cpScreenPoint.x;
                event->screeny = cpScreenPoint.y;
                event->layer_id = LO_GetIdFromLayer(GetContext(), layer);

                ET_SendEvent(GetContext(), pElement, event, win_click_callback, pClosure);
                return;
            }
            break;
        
        case LO_IMAGE:	
        {
            //	Glean the FE data.

            //	Figure out where the click occurred.
            LO_ImageStruct* pImage = &pElement->lo_image;

            //	Layout wants this in pixels, not FE units.
            //  Point is in layer coordinates, as is the position of the image
            CPoint cpMap((int) (Point.x - Twips2PixX(pImage->x + pImage->x_offset + pImage->border_width)), 
                         (int) (Point.y - Twips2PixY(pImage->y + pImage->y_offset + pImage->border_width)));

            pClosure->x = cpMap.x;
            pClosure->y = cpMap.y;

            //	Check for usemaps (client side ismaps).
            if(pElement->lo_image.image_attr->usemap_name != NULL)
            {
                LO_AnchorData *pAnchorData = LO_MapXYToAreaAnchor(GetDocumentContext(), 
						                &pElement->lo_image, cpMap.x, cpMap.y);
                if(pAnchorData != NULL)
                {

	                // set tab_focus to this image element with a link. todo need to get the area index
	                //setFormElementTabFocus( (LO_Element *) pElement );

	                if(uFlags & MK_SHIFT && NOT_A_DIALOG(this)) {
                        //	Save it.
                        CSaveCX::SaveAnchorObject((const char *)pAnchorData->anchor, NULL);
                        XP_FREE(pClosure);
	                }
	                else {
                        LO_Element * pDummy;

                        // Imagemap area pretend to be links for JavaScript.
                        pDummy = (LO_Element *) XP_NEW_ZAP(LO_Element);
                        pDummy->lo_text.type = LO_TEXT;
                        pDummy->lo_text.anchor_href = pAnchorData;
                        // We use the text of the element to determine if it is still
                        // valid later so give the dummy text struct's text a value.
                        if (pDummy->lo_text.anchor_href->anchor)
	                        pDummy->lo_text.text = pDummy->lo_text.anchor_href->anchor;

                        // We'll need the image element later in the callback.
                        pClosure->pElement = pElement;

                        // we are about to follow a link via a user click -- 
                        //   tell the mocha library.  Don't follow the link
                        //   until we get to our closure
                        JSEvent *event;
                        event = XP_NEW_ZAP(JSEvent);
                        event->type = EVENT_CLICK;
                        event->which = 1;
	                        event->x = Point.x;
                        event->y = Point.y;
                        event->docx = Point.x + CL_GetLayerXOrigin(layer);
                        event->docy = Point.y + CL_GetLayerYOrigin(layer);
                        event->screenx = cpScreenPoint.x;
                        event->screeny = cpScreenPoint.y;
                        event->layer_id = LO_GetIdFromLayer(GetContext(), layer);

                        ET_SendEvent(GetContext(), pDummy, event, 
		                         win_click_callback, pClosure);
	                }
	                //	We're out, don't fall through.
	                return;
                }
            }
            //	Check for ismaps, normal image anchors
            else if(pElement->lo_image.anchor_href != NULL && 
	            pElement->lo_image.anchor_href->anchor != NULL)
            {
                // set tab_focus to this image element with a link.
                //#52932 setFormElementTabFocus( (LO_Element *) pElement );

                //	Ismap?
                if(pElement->lo_image.image_attr->attrmask & LO_ATTR_ISMAP)
                {

	                if(uFlags & MK_SHIFT && NOT_A_DIALOG(this))
                    {
                        char * pAnchor = PR_smprintf("%s?%d,%d", 
		                        pElement->lo_image.anchor_href->anchor, cpMap.x, cpMap.y);
                        CSaveCX::SaveAnchorObject(pAnchor, NULL);
                        XP_FREE(pClosure);
                        XP_FREE(pAnchor);
	                } 
	                else {
                        // set tab_focus to this image element with a link.
                        //#52932 setFormElementTabFocus((LO_Element *) pElement);

                        // remember where we are going
                        pClosure->x = cpMap.x;
                        pClosure->y = cpMap.y;

                        // we are about to follow a link via a user click -- 
                        //   tell the mocha library.  Don't follow the link
                        //   until we get to our closure
                        JSEvent *event;
                        event = XP_NEW_ZAP(JSEvent);
                        event->type = EVENT_CLICK;
                        event->which = 1;
	                        event->x = Point.x;
                        event->y = Point.y;
                        event->docx = Point.x + CL_GetLayerXOrigin(layer);
                        event->docy = Point.y + CL_GetLayerYOrigin(layer);
                        event->screenx = cpScreenPoint.x;
                        event->screeny = cpScreenPoint.y;
                        event->layer_id = LO_GetIdFromLayer(GetContext(), layer);

                        ET_SendEvent(GetContext(), pElement, event, 
		                         win_click_callback, pClosure);
	                }
                }
                //	Anchor.
                else {
	                if(uFlags & MK_SHIFT && NOT_A_DIALOG(this)) {
                        //  Should mocha block save operations too?
                        CSaveCX::SaveAnchorObject((const char *)pElement->lo_image.anchor_href->anchor, NULL);
                        XP_FREE(pClosure);
                    }
                    else {
                        // we are about to follow a link via a user click -- 
                        //   tell the mocha library.  Don't follow the link
                        //   until we get to our closure
                        JSEvent *event;
                        event = XP_NEW_ZAP(JSEvent);
                        event->type = EVENT_CLICK;
                        event->which = 1;
                        event->x = Point.x;
                        event->y = Point.y;
                        event->docx = Point.x + CL_GetLayerXOrigin(layer);
                        event->docy = Point.y + CL_GetLayerYOrigin(layer);
                        event->screenx = cpScreenPoint.x;
                        event->screeny = cpScreenPoint.y;
                        event->layer_id = LO_GetIdFromLayer(GetContext(), layer);
	                        
                        ET_SendEvent(GetContext(), pElement, event, 
		                         win_click_callback, pClosure);
	                }
                }

                //	We're out, don't fall through.
                return;
            }

            //	Check for images as submit buttons.
            //	Lot's of these on test pages, but in the real world?
            if(pElement->lo_image.image_attr && 
              (pElement->lo_image.image_attr->attrmask & LO_ATTR_ISFORM))
            {
                pClosure->x = cpMap.x;
                pClosure->y = cpMap.y;

                JSEvent *event;
                event = XP_NEW_ZAP(JSEvent);
                event->type = EVENT_SUBMIT;
	                event->layer_id = LO_GetIdFromLayer(GetContext(), layer);

                ET_SendEvent(GetContext(), (LO_Element *)&pElement->lo_image, event,
		                 image_form_click_callback, pClosure);

                return;
            }
            break; // case LO_IMAGE
        }

        default:
	        //	nothing doing.
	        XP_FREE(pClosure);	    
	        break;
	}

MOUSE_TIMER:
    //  Have the mouse timer handler do some dirty work.
    //  Please don't return in the above code, I'd like this to get called
    //      in all cases with the state of the buttons set correctly.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
}


// convert LO_Text to a bookmark object we can drag into
//  the bookmark window
PRIVATE HGLOBAL 
wfe_textObjectToBookmarkHandle(LO_TextStruct * text, char * title)
{
    if(!text)
        return(NULL);

    HGLOBAL hBookmark = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, sizeof(BOOKMARKITEM));
    if(!hBookmark)
        return(NULL);

    LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)GlobalLock(hBookmark);

    if(text->anchor_href && text->anchor_href->anchor)
        PR_snprintf(pBookmark->szAnchor,sizeof(pBookmark->szAnchor),"%s",text->anchor_href->anchor);

    if( title ) {
        // First try to use the title supplied,
        // This may contain more text than the text structure has
        PR_snprintf(pBookmark->szText,sizeof(pBookmark->szText),"%s",title);
    } else if(text->text) {
        PR_snprintf(pBookmark->szText,sizeof(pBookmark->szText),"%s",text->text);
    }

    GlobalUnlock(hBookmark);

    return hBookmark;
}

// Creates OLE drag data source for selected text
void CWinCX::DragSelection()
{            
    // Begin the drag and drop operation
    // REMEMBER: OnDrop: Check if end pt is withing selection,
    //   if yes, ignore drop
    MWContext  * pMWContext = GetContext();

#ifdef EDITOR
    // Don't bother if no selection or not allowed in Editor
    if ( EDT_IS_EDITOR(pMWContext) && 
         EDT_COP_OK != EDT_CanCopy(pMWContext, TRUE))
      return;
#endif // EDITOR
    
    // Here's where we put the data
    COleDataSource * pDataSource = new COleDataSource;  
    // This is used to override cursors during dragging
    UINT nDragType = FE_DRAG_TEXT;
#ifdef EDITOR
    if( EDT_IS_EDITOR(pMWContext) )
    {
        nDragType = EDT_IsDraggingTable(pMWContext) ? FE_DRAG_TABLE : FE_DRAG_HTML;
    }
#endif    
    CViewDropSource * pDropSource = new CViewDropSource(nDragType);
    
    char* pText = NULL;
    XP_HUGE_CHAR_PTR pGlobal;
    XP_HUGE_CHAR_PTR pHTML;
    int32  textLen = 0;
    int32  htmlLen;
    m_bDragging = FALSE;
#ifdef EDITOR
    if( EDT_IS_EDITOR(pMWContext) ){
        if( EDT_COP_OK == EDT_CanCopy(pMWContext, TRUE) &&
            EDT_COP_OK == EDT_CopySelection(pMWContext, &pText, &textLen, &pHTML, &htmlLen) ){
            // Put HTML-formated text in OLE data object
            HGLOBAL hHTML = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, (int)htmlLen);
            if(hHTML) {
                pGlobal = (char *) GlobalLock(hHTML);
                XP_HUGE_MEMCPY(pGlobal, pHTML, (int) htmlLen);
                XP_HUGE_FREE(pHTML);
                GlobalUnlock(hHTML);
                pDataSource->CacheGlobalData(RegisterClipboardFormat(NETSCAPE_EDIT_FORMAT), hHTML);
                m_bDragging = TRUE;
            }
        }
    }
    else 
#endif // EDITOR
    {
        // Browser-only
        pText = (char *) LO_GetSelectionText(GetDocumentContext());
        if( pText )
            textLen = XP_STRLEN(pText);
    }

    // Put unformated text in OLE data object - may be dropped in other containers
    HGLOBAL hText = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, (int)textLen+1);
    DROPEFFECT res = 0;
    if( pText && textLen > 0 ){
 #ifdef XP_WIN32
	// Also try to put CF_UNICODETEXT
		int datacsid = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo( GetDocumentContext() )) & ~CS_AUTO;
		if((CS_USER_DEFINED_ENCODING != datacsid) && (0 != datacsid))
		{
			int len = (INTL_StrToUnicodeLen(datacsid, (unsigned char*)pText)+1) * 2;
			HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) len);
			if(hData)
			{
				unsigned char* string = (unsigned char *) GlobalLock(hData);
				if(string)
				{
					INTL_StrToUnicode(datacsid, (unsigned char*)pText, (INTL_Unicode*)string, len);
			
					GlobalUnlock(hData);	
					pDataSource->CacheGlobalData(CF_UNICODETEXT, hData);
				}
			}
		}
#endif
		if(hText) {
            pGlobal = (char *) GlobalLock(hText);
            XP_MEMCPY(pGlobal, pText, (int) textLen+1);
            XP_FREE(pText);
            GlobalUnlock(hText);
            pDataSource->CacheGlobalData(CF_TEXT, hText);
            m_bDragging = TRUE;
        }
    }

    if( m_bDragging ){
        BOOL bWaitingMode = pMWContext->waitingMode;
        // Prevent closing/interaction with source window while dragging
        pMWContext->waitingMode = TRUE;
        
        res = pDataSource->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_SCROLL,
                                                 NULL, pDropSource);
        pMWContext->waitingMode = bWaitingMode;
    }
    m_bDragging = FALSE;

#ifdef EDITOR
    if( EDT_IS_EDITOR(pMWContext) ){
        EDT_StopDragTable(pMWContext);
    }
#endif
	// Prevent selection-extension when finished
	m_bLBDown = FALSE;
	m_bLBUp = TRUE;
    
    // its over so clean up
    pDataSource->Empty();
    delete pDataSource;
    delete pDropSource;
}

// Triggered on button up on our bitmap on the menu
void CWinCX::CopyCurrentURL()
{
    MWContext *pMWContext = GetContext();
    if ( pMWContext == NULL ) {
        return;
    } 

    History_entry * hist_ent = SHIST_GetCurrent(&(pMWContext->hist));
    if ( hist_ent == NULL || hist_ent->address == NULL ){
        return;
    }
    HGLOBAL hData = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, 
                                XP_STRLEN((char *) hist_ent->address) + 2);
    if(!hData) {
        return;
    }
    // lock the string and copy the data over
    char * pString = (char *) GlobalLock(hData);
    strcpy(pString, (char *) hist_ent->address);
    GlobalUnlock(hData);

	GetFrame()->GetFrameWnd()->OpenClipboard();
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, hData);

    // Also copy bookmark-formatted data so we can paste full link,
    //  not just text, into the Editor
    hData = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, sizeof(BOOKMARKITEM));
    if(hData){
        LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)GlobalLock(hData);
        PR_snprintf(pBookmark->szAnchor, sizeof(pBookmark->szAnchor), "%s",
                    hist_ent->address);
        PR_snprintf(pBookmark->szText, sizeof(pBookmark->szText), "%s",
                    hist_ent->title);
        GlobalUnlock(hData);
        ::SetClipboardData(RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT), hData);
    }
	::CloseClipboard();
}

// Drag from the Bitmap Menu item or URL bar icon
void CWinCX::DragCurrentURL()
{
    MWContext *pMWContext = GetContext();
    if ( pMWContext == NULL ) {
        return;
    } 

    History_entry * hist_ent = SHIST_GetCurrent(&(pMWContext->hist));
    if ( hist_ent == NULL || hist_ent->address == NULL ){
        return;
    }
    HGLOBAL hAddrString = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, 
                                      XP_STRLEN((char *) hist_ent->address) + 2);
    if(!hAddrString) {
        return;
    }

    // lock the string and copy the data over
    char * pAddrString = (char *) GlobalLock(hAddrString);
    strcpy(pAddrString, (char *) hist_ent->address);

    GlobalUnlock(hAddrString);

    HGLOBAL hBookmark = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, sizeof(BOOKMARKITEM));
    if(!hBookmark) {
        return;
    }
    LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)GlobalLock(hBookmark);
    PR_snprintf(pBookmark->szAnchor,sizeof(pBookmark->szAnchor),"%s",hist_ent->address);
    PR_snprintf(pBookmark->szText,sizeof(pBookmark->szText),"%s",hist_ent->title);
    GlobalUnlock(hBookmark);

    // Create the DataSourceObject
    COleDataSource * pDataSource = new COleDataSource;  
    pDataSource->CacheGlobalData(CF_TEXT, hAddrString);
    pDataSource->CacheGlobalData(RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT), hBookmark);
    // This is used to override cursors during dragging
    CViewDropSource * pDropSource = new CViewDropSource(FE_DRAG_LINK);

    DragInternetShortcut ( pDataSource,
        (char*)hist_ent->title,
        (char*)hist_ent->address );

    // do the drag/drop operation.
    // set the m_bDragging flag so that we can prevent ourseleves from dropping on
    //   ourselves                
    
    m_bDragging = TRUE;
    // no saved image for next time
    // Must do this before DoDragDrop to prevent
    //  arriving here again!
    m_pLastImageObject = NULL;
    m_pLastArmedAnchor = NULL;

    // Don't start drag until outside this rect    
    RECT rectDragStart = {0,0,20,20};

    // We supply the DropSource object instead of default behavior
    // This prevents closing source frame during drag and drop
    BOOL bWaitingMode = pMWContext->waitingMode;
    pMWContext->waitingMode = TRUE;
    
    pDataSource->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE | DROPEFFECT_SCROLL,
                            &rectDragStart, pDropSource);
    
    pMWContext->waitingMode = bWaitingMode;
    m_bDragging = FALSE;

    // After dragging, moving mouse in browser acts like button is down,
    //  and ugly selection extension happens. This prevents that.
    m_bLBDown = FALSE;
    m_bLBUp = TRUE;

    // its over so clean up
    pDataSource->Empty();
    delete pDataSource;
    delete pDropSource;
}

void wfe_Progress(MWContext *pContext, const char *pMessage);

void CWinCX::OnMouseMoveCX(UINT uFlags, CPoint cpPoint, BOOL &bReturnImmediately)	
{
	//	Must have a view to continue.
	if(GetPane() == NULL)	{
		return;
	}

	//	Don't continue if this context is destroyed.
	if(IsDestroyed())	{
		return;
	}

    // This is set TRUE only by CNetscapeEditView::OnRButtonDown
    //   while popup menu is active
    // We ignore this message else it changes cursor to something inappropriate
    if( m_bInPopupMenu )
        return;

	//	Remember....
    m_LastMouseEvent = m_MMove;
	m_cpMMove = cpPoint;
	m_uMouseFlags = uFlags;

    // Convert from screen to window coordinates
    XY xyPoint;
    ResolvePoint(xyPoint, cpPoint);

#ifdef LAYERS
	MWContext  * context  = GetContext();
	if (context->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    fe_event.pbReturnImmediately = bReturnImmediately;
	    
	    event.type = CL_EVENT_MOUSE_MOVE;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = xyPoint.x;
	    event.y = xyPoint.y;
	    event.which = 1;
    	    event.modifiers = 0;
	    
	    CL_DispatchEvent(context->compositor, &event);
	}
	else 
	    OnMouseMoveForLayerCX(uFlags, cpPoint, xyPoint, NULL, bReturnImmediately);

    //  Have the mouse timer handler do some dirty work.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
    return;
}

void
CWinCX::OnMouseMoveForLayerCX(UINT uFlags, CPoint& cpPoint,
			      XY& xyPoint, CL_Layer *layer, BOOL &bReturnImmediately)
{
// With LAYERS turned on, the orginal method 
// OnMouseMoveCX is separated into two methods,
// one of which is a per-layer method.
#endif /* LAYERS */

    MWContext  * context  = GetContext();

    BOOL bTextSet = FALSE;

    LO_Element *pElement = GetLayoutElement(xyPoint, layer);
    if (pElement && (pElement->type == LO_IMAGE)) {
        LO_ImageStruct* pImage = &pElement->lo_image;
        if (pImage) {
            CreateToolTip(pImage, cpPoint, layer);
            RelayToolTipEvent(cpPoint, WM_MOUSEMOVE);
        }
    }

    int32 xVal = xyPoint.x;
    int32 yVal = xyPoint.y;

    // don't do anything if we are waiting for the netlib to get
    //   into gear
    if (context->waitingMode) {
        // Change cursor only if not doing internal drag
        if( !m_bDragging ){
            SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
        }
        return;  // can't load while loading
    }

    // Note: don't use (uFlags & MK_LBUTTON) because this is
    //    reported as TRUE on a mouse move message
    //    following a click off of a dialog or popup menu
    //
    if ( m_bLBDown )
    {
#ifdef EDITOR
        // If we are selecting cells, skip the rest
        if( m_bSelectingCells )
        {
            // We must fire an event to setup CLOSURE struct
            //   and call mouse_closure_callback
            FireMouseOverEvent(NULL, xVal, yVal, layer);
            goto MOUSE_TIMER;
        }
        if( EDT_IS_SIZING )
        {
            // We are sizing 
            
            BOOL bLock = !(BOOL)(uFlags & MK_CONTROL);
            XP_Rect new_rect;
            if( EDT_GetSizingRect(context, xVal, yVal, bLock, &new_rect) )
            {
                // Remove last sizing feedback
                DisplaySelectionFeedback(LO_ELE_SELECTED, m_rectSizing);
                // Save the new rect.
                m_rectSizing.left = new_rect.left;
                m_rectSizing.right = new_rect.right;
                m_rectSizing.top = new_rect.top;
                m_rectSizing.bottom = new_rect.bottom;
                // then draw new feedback
                DisplaySelectionFeedback(LO_ELE_SELECTED, m_rectSizing);

            }
            // Status text was set by XP code, so set flag here
            bTextSet = TRUE;
            goto MOUSE_TIMER;
        }
        // Check for cell selection only if not starting drag of table cells
        if( !EDT_IsDraggingTable(context) )
        {
            // We are not currently selecting cells, so get the cell we may be over
            // Note: This will return cell and ED_HIT_SIZE_COL if inbetween columns,
            //    so check that because we don't want to select cell if just before the left edge
            LO_Element *pCellElement = NULL;
            // Mouse move test with left button down - extend selection to other cells
            ED_HitType iTableHit = EDT_GetTableHitRegion(context, xVal, yVal, &pCellElement, FALSE);
            if( m_pStartSelectionCell && pCellElement && 
                (iTableHit != ED_HIT_SIZE_COL) &&
                pCellElement->type == LO_CELL && (pCellElement != m_pStartSelectionCell) )
            {
                m_bSelectingCells = TRUE;
                if( m_pStartSelectionCell )
                {
                    // Mouse is in a different cell then when we started selecting
                    // So switch to cell-selection mode. 
                    // 1st FALSE param means clear any other selection (shouldn't be any)
                    //     (last param is used to extend selection)
                    EDT_SelectTableElement(context, m_pStartSelectionCell->lo_any.x, m_pStartSelectionCell->lo_any.y,
                                           m_pStartSelectionCell, ED_HIT_SEL_CELL, FALSE, FALSE);
                }
                // Select new cell as well: If previously selecting, last param = TRUE
                //  and we append this cell
                EDT_SelectTableElement(context, xVal, yVal, pCellElement, ED_HIT_SEL_CELL, 
                                       FALSE, m_pStartSelectionCell != NULL);
                goto MOUSE_TIMER;
            }
        }
#endif // EDITOR

        // Don't bother to do selection or dragging unless we actually moved
        if( (abs(cpPoint.x - m_cpLBDown.x) > 5)
              || (abs(cpPoint.y - m_cpLBDown.y) > 5) )
        {
            if( m_bMouseInSelection )
            {
                // release the mouse capture
                ::ReleaseCapture();
                m_bMouseInSelection = FALSE;

                // Get and drag the selection
                DragSelection();
                // Don't do anything else if we are dragging!
                goto MOUSE_TIMER;
            }
            // Extend the selection
#ifdef EDITOR
            if( EDT_IS_EDITOR(context) )
            {
                EDT_ExtendSelection(context, xVal, yVal);
            } else 
#endif // EDITOR
            {
                LO_ExtendSelection(GetDocumentContext(), xVal, yVal);
            }
        }

		int32 lYPos = GetOriginY();
		int32 lXPos = GetOriginX();
	    int32 xCur = xVal;
	    int32 yCur = yVal;
#ifdef LAYERS
        if (layer)
        {
			int32 layer_x_offset = CL_GetLayerXOrigin(layer);
			int32 layer_y_offset = CL_GetLayerYOrigin(layer);

		    xCur += layer_x_offset;
		    yCur += layer_y_offset;
		}
#endif // LAYERS
		if(xCur < GetOriginX())	{
			lXPos = xCur;
		}
		else if(xCur > GetWidth() + GetOriginX())	{
			lXPos = xCur - GetWidth();
		}

		if(yCur < GetOriginY())	{
			lYPos = yCur;
		}
		else if(yVal > GetHeight() + GetOriginY())	{
			lYPos = yCur - GetHeight();
		}

		//	Validate position recommendations, and reposition if necessary.
		if(lXPos > GetDocumentWidth() - GetWidth())	{
			lXPos = GetDocumentWidth() - GetWidth();
		}
		if(lXPos < 0)	{
			lXPos = GetOriginX();
		}
		if(lYPos > GetDocumentHeight() - GetHeight())	{
			lYPos = GetDocumentHeight() - GetHeight();
		}
		if(lYPos < 0)	{
			lYPos = GetOriginY();
		}

		if(lYPos != GetOriginY() || lXPos != GetOriginX())	{
			//	Reposition.
			SetDocPosition(context, FE_VIEW, lXPos, lYPos);
		}

        // We need to unhighlight an anchor if we highlighted it on buttonDown
        // The anchor element is held within the last_armed_xref global
        // If last_armed_xref is non-null then the anchor is highlighted and
        // needs to be unhighlighted and a drag and drop operation started.
        if( m_pLastArmedAnchor && !EDT_IS_EDITOR(context) ) {
        	if(abs(m_cpMMove.x - m_cpLBDown.x) > CLICK_THRESHOLD ||
        		abs(m_cpMMove.y - m_cpLBDown.y) > CLICK_THRESHOLD)	{

                LO_HighlightAnchor(GetDocumentContext(), m_pLastArmedAnchor, FALSE);

                // Convert the selection into stuff other people can understand

                LO_TextStruct * text = (LO_TextStruct *) m_pLastArmedAnchor;
                if(!text)
                    goto MOUSE_TIMER;

                int len = XP_STRLEN((char *) text->anchor_href->anchor) + 2;

                // make an assumption that will bite us later.  Shove everything into
                //   global space cuz it will be small
                HGLOBAL hAddrString = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, len);
                if(!hAddrString)
                    goto MOUSE_TIMER;

                // lock the string and copy the data over
                char * pAddrString = (char *) GlobalLock(hAddrString);
                strcpy(pAddrString, (char *) text->anchor_href->anchor);

                GlobalUnlock(hAddrString);

                //CLM: Select the full link object, which scans  neighboring text elements
                //     to gather text that may have different formatting but same HREF
                XY Point;
            	ResolvePoint(Point, m_cpLBDown);
#ifdef LAYERS
                LO_SelectObject(GetDocumentContext(), Point.x, Point.y, NULL);
#else
                LO_SelectObject(GetDocumentContext(), Point.x, Point.y);
#endif /* LAYERS */
				// check to see if its an address book url.  If it is convert it to 
				// vcard clipboard format
				COleDataSource * pDataSource = NULL;
				CViewDropSource * pDropSource = NULL;
				char * url = (char*) text->anchor_href->anchor;
				char * path = NET_ParseURL((char *) text->anchor_href->anchor, GET_PATH_PART);
				char * search = NET_ParseURL((char *) text->anchor_href->anchor, GET_SEARCH_PART);
				if (!XP_STRNCASECMP(path,"add",3)) {
					if (!XP_STRNCASECMP (search, "?vcard=", 7)) {
						// Create the DataSourceObject
						CLIPFORMAT mVcardClipboardFormat = (CLIPFORMAT)RegisterClipboardFormat(vCardClipboardFormat);
						char * escVcard = XP_STRDUP (search+7);
						if (escVcard) {
							escVcard = NET_UnEscape(escVcard);
							HANDLE hString = 0;
							hString = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT,strlen(escVcard)+1);
							LPSTR lpszString = (LPSTR)GlobalLock(hString);
							strcpy(lpszString, escVcard);
							GlobalUnlock(hString);
							XP_FREEIF (escVcard);
							pDataSource = new COleDataSource;  
							pDataSource->CacheGlobalData(mVcardClipboardFormat, hString);
							pDataSource->CacheGlobalData(CF_TEXT, hString);
							pDropSource = new CViewDropSource(FE_DRAG_VCARD);
						}
					}
				}
				else {

					char *pFullLink = (char *) LO_GetSelectionText(GetDocumentContext());

					HGLOBAL hBookmark = wfe_textObjectToBookmarkHandle(text, pFullLink);

					// make sure we have a bookmark format defined
					CLIPFORMAT mBookmarkClipboardFormat = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

					// Create the DataSourceObject
					pDataSource = new COleDataSource;  
					pDataSource->CacheGlobalData(CF_TEXT, hAddrString);
					pDataSource->CacheGlobalData(mBookmarkClipboardFormat, hBookmark);

					// This is used to override cursors during dragging
					pDropSource = new CViewDropSource(FE_DRAG_LINK);

					DragInternetShortcut ( pDataSource,
						pFullLink ? pFullLink : (char*)text->text,
						(char*)text->anchor_href->anchor );
				}

                // do the drag/drop operation.
                // set the m_bDragging flag so that we can prevent ourseleves from dropping on
                //   ourselves                
                m_bDragging = TRUE;
               // Must do this before DoDragDrop to prevent
               //  arriving here again!
                m_pLastArmedAnchor = NULL;
                m_pLastImageObject = NULL;
                
                // This prevents closing source frame during drag and drop
                BOOL bWaitingMode = context->waitingMode;
                context->waitingMode = TRUE;
                
                // We supply the DropSource object instead of default behavior
                // (we don't need return value, do we?)
                pDataSource->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE | DROPEFFECT_SCROLL,
                                        NULL, pDropSource);
                
                context->waitingMode = bWaitingMode;
                m_bDragging = FALSE;

				// After dragging, moving mouse in browser acts like button is down,
				//  and ugly selection extension happens. This prevents that.
                m_bLBDown = FALSE;
                m_bLBUp = TRUE;

                // its over so clean up
                pDataSource->Empty();
                delete pDataSource;
                delete pDropSource;
            }
        }
#ifdef EDITOR
        // Drag image from an Editor or Browser
        else if ( m_pLastImageObject &&
                  (abs(m_cpMMove.x - m_cpLBDown.x) > CLICK_THRESHOLD ||
        		   abs(m_cpMMove.y - m_cpLBDown.y) > CLICK_THRESHOLD) ) {
            
			LO_ImageStruct *pImage = (LO_ImageStruct *)m_pLastImageObject;
    	    char *pImageURL = NULL;

            HGLOBAL hImageData = WFE_CreateCopyImageData(context, pImage);
            if ( hImageData ) {

                // make sure we have a clipboard format defined
                CLIPFORMAT mImageFormat = (CLIPFORMAT)RegisterClipboardFormat(
                                                NETSCAPE_IMAGE_FORMAT);

                // Create the DataSourceObject
                COleDataSource * pDataSource = new COleDataSource;  
                pDataSource->CacheGlobalData(mImageFormat, hImageData);

                // This is used to override cursors during dragging
                CViewDropSource * pDropSource = new CViewDropSource(FE_DRAG_IMAGE);

                // do the drag/drop operation.
                // set the m_bDragging flag so that we can prevent ourseleves from dropping on
                //   ourselves                
                m_bDragging = TRUE;
                // no saved image for next time
                m_pLastImageObject = NULL;
                m_pLastArmedAnchor = NULL;

                //Weird problem:
                //  Return from dropping after InsertImage
                //  sometimes results in hourglass cursor
                // So save and restore after DoDragDrop
                HCURSOR hCursor = GetCursor();

                // This prevents closing source frame during drag and drop
                BOOL bWaitingMode = context->waitingMode;
                context->waitingMode = TRUE;

                pDataSource->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_SCROLL,
                                        NULL, pDropSource);
                
                context->waitingMode = bWaitingMode;
                SetCursor(hCursor);
                m_bDragging = FALSE;

                // Prevent selection mode when returning to browser
                m_bLBDown = FALSE;
                m_bLBUp = TRUE;

                // its over so clean up
                pDataSource->Empty();
                delete pDataSource;
                delete pDropSource;
            }
        }
#endif // EDITOR
    }
    else {    // Left button is not down...

        // If there are connections being initiated (i.e. the watch 
        //   cursor is up) don't blow away the text so that the netlib 
        //   messages persist in the status bar
        if(context->waitingMode)
            goto MOUSE_TIMER;

#ifdef LAYERS
        LO_Element *lo_element = LO_XYToElement(GetDocumentContext(), xVal, yVal, layer);
#else
        LO_Element *lo_element = LO_XYToElement(GetDocumentContext(), xVal, yVal);
#endif /* LAYERS */

        //  Handle mouse over event processing with back end libs.
        if(!bTextSet)   {
            // Let the backend take a crack at handling the text
            FireMouseOverEvent(lo_element,xVal, yVal, layer);

            // if the backend didn't set the text we will set the
            //   text in our closure --- don't do anything else
            //   in this routine
		}
    }

MOUSE_TIMER:
    //  Have the mouse timer handler do some dirty work.
    //  Please don't return in the above code, I'd like this to get called
    //      in all cases with the state of the buttons set correctly.
    MouseTimerData mt(context);
    FEU_MouseTimer(&mt);

}

void CWinCX::OnRButtonDblClkCX(UINT uFlags, CPoint cpPoint)	{
	//	Only do this if clicking is enabled.
	if(IsClickingEnabled() == FALSE)	{
		return;
	}

	//	Don't continue if this context is destroyed.
	if(IsDestroyed())	{
		return;
	}

	XY xyPoint;
	ResolvePoint(xyPoint, cpPoint);

	if (GetContext()->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    
	    event.type = CL_EVENT_MOUSE_BUTTON_MULTI_CLICK;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = xyPoint.x;
	    event.y = xyPoint.y;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	    event.which = 3;
	    event.data = 2;
    
	    CL_DispatchEvent(GetContext()->compositor, &event);
	}
	else 
	    OnRButtonDblClkForLayerCX(uFlags, cpPoint, xyPoint, NULL);


    //  Have the mouse timer handler do some dirty work.
    //  Please don't return in the above code, I'd like this to get called
    //      in all cases with the state of the buttons set correctly.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
}

void
CWinCX::OnRButtonDblClkForLayerCX(UINT uFlags, CPoint& cpPoint,
				XY& Point, CL_Layer *layer)
{
// With LAYERS turned on, the orginal method 
// OnRButtonDblClkCX is separated into two methods,
// one of which is a per-layer method.

    //	Remember....
    m_LastMouseEvent = m_RBDClick;
    m_cpRBDClick = cpPoint;
    m_uMouseFlags = uFlags;
}

void CWinCX::OnRButtonDownCX(UINT uFlags, CPoint cpPoint)
{
	RelayToolTipEvent(cpPoint, WM_RBUTTONDOWN);

    MWContext * pMWContext = GetContext();
	
    //	Only do this if clicking is enabled, we have a context,
    //   or context is not destoyed
	if(IsClickingEnabled() == FALSE ||
	   pMWContext == NULL ||
	   IsDestroyed()) {
		    return;
	}

    XY xyPoint;
    ResolvePoint(xyPoint, cpPoint);

#ifdef LAYERS
	if (pMWContext->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    
	    event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = xyPoint.x;
	    event.y = xyPoint.y;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	    event.which = 3;
	
	    CL_DispatchEvent(pMWContext->compositor, &event);
	}
	else 
	    OnRButtonDownForLayerCX(uFlags, cpPoint, xyPoint, NULL);

    //  Have the mouse timer handler do some dirty work.
    MouseTimerData mt(pMWContext);
    FEU_MouseTimer(&mt);
    return;
}

BOOL
CWinCX::OnRButtonDownForLayerCX(UINT uFlags, CPoint& cpPoint,
				XY& Point, CL_Layer *layer)
{
// With LAYERS turned on, the orginal method 
// OnRButtonDownCX is separated into two methods,
// one of which is a per-layer method.
#endif /* LAYERS */
    //	Remember....
    m_LastMouseEvent = m_RBDown;
    m_cpRBDown = cpPoint;
    m_uMouseFlags = uFlags;

    return FALSE;
}

void CWinCX::OnRButtonUpCX(UINT uFlags, CPoint cpPoint)
{
    RelayToolTipEvent(cpPoint, WM_RBUTTONUP);

    //	Only do this if clicking is enabled.
    if(IsClickingEnabled() == FALSE)	{
	    return;
    }

    //	Don't continue if this context is destroyed.
    if(IsDestroyed())	{
	    return;
    }

    XY xyPoint;
    ResolvePoint(xyPoint, cpPoint);

    if (GetContext()->compositor) {
	CL_Event event;
	fe_EventStruct fe_event;
	
	fe_event.uFlags = uFlags;
	fe_event.x = cpPoint.x;
	fe_event.y = cpPoint.y;
	
	event.type = CL_EVENT_MOUSE_BUTTON_UP;
	event.fe_event = (void *)&fe_event;
	event.fe_event_size = sizeof(fe_EventStruct);
	event.x = xyPoint.x;
	event.y = xyPoint.y;
	event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			| (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			| (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	event.which = 3;
    
	CL_DispatchEvent(GetContext()->compositor, &event);
    }
    else 
	OnRButtonUpForLayerCX(uFlags, cpPoint, xyPoint, NULL);

    //  Have the mouse timer handler do some dirty work.
    //  Please don't return in the above code, I'd like this to get called
    //      in all cases with the state of the buttons set correctly.
    MouseTimerData mt(GetContext());
    FEU_MouseTimer(&mt);
}

void
CWinCX::OnRButtonUpForLayerCX(UINT uFlags, CPoint& cpPoint,
				XY& Point, CL_Layer *layer)
{
// With LAYERS turned on, the orginal method 
// OnRButtonUpCX is separated into two methods,
// one of which is a per-layer method.

    //	Remember....
    m_LastMouseEvent = m_RBUp;
    m_cpLBUp = cpPoint;
    m_uMouseFlags = uFlags;
}

CWnd *CWinCX::GetDialogOwner() const    {
    CGenericView *pView = m_pGenView;
    CWnd *pOwner = NULL;
    CWnd *pWnd;

    if(pView)   {
        pOwner = pView->GetOwner();
    }

    if(pOwner == NULL)  {
        //  No owner, we must take it.
        CWnd * pWnd = pView->GetParentFrame();
        if (pWnd)
            pOwner = pWnd;
        pOwner = (CWnd *)pView;
    }
    pWnd = pOwner->GetParentFrame();
    if (pWnd)
        pOwner = pWnd;
    
    // When a modal dialog (usually a preference dialog) is active, 
    //     it should be the message diaolg's parent,
	//     else user can loose alert message behind it and user can interact
    //     with preference dialog with bad consequences!
    return(pOwner->GetLastActivePopup());
}

int CWinCX::GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoading, BOOL bForceNew)   
{
    // If we are POSTing data (publishing), or forcing a new page, don't ask user to save current page first
    //  If user cancels when being prompted to save current document, return without action
#ifdef EDITOR
    if ( !bForceNew && pUrl->method != URL_POST_METHOD 
        && !FE_CheckAndSaveDocument(GetContext()) 
        ) {
        return( MK_NO_ACTION );
    }
#endif // EDITOR
//	ResetToolTipImg();

#ifdef EDITOR
#ifdef XP_WIN32
    // If we are talking to LiveWire communications system,
    //  tell Site Manager we are about to load a new URL
    //  if we are NOT simply creating a new document
    if ( GetContext() &&
         pUrl && pUrl->address && 
         0 != XP_STRCMP(pUrl->address, EDT_NEW_DOC_URL) &&
         bSiteMgrIsActive ) {
        pITalkSMClient->LoadingURL(pUrl->address);
    }
#endif // XP_WIN32
#endif // EDITOR

    //  Enable the wait cursor.
    SetCursor(theApp.LoadStandardCursor(IDC_WAIT));

    //  We need to disable/deactivate any embedded items.
    OnDeactivateEmbedCX();

#ifdef EDITOR
    if( EDT_IS_EDITOR(GetContext()) ){
        FE_DestroyCaret(GetContext());
    }
#endif
    //Caret will be shown automatically after new URL is loaded
	//	Save the frame's URL bar text.
	//	We use this to determine wether or not the user has
	//		changed the URL bar since the new load began, if
	//		if so, we won't blow away what they've typed.

	if(GetContext()->type == MWContextBrowser && !EDT_IS_EDITOR(GetContext())){
        IChrome *pChrome = GetFrame()->GetChrome();
		CWnd *pWnd = pChrome ? pChrome->GetToolbar(ID_LOCATION_TOOLBAR) : NULL;

		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CURLBar))){
			CURLBar *pUrlBar = (CURLBar *) pWnd;

			if(pUrlBar != NULL)
				pUrlBar->m_pBox->GetWindowText(m_csSaveLocationBarText);
			else
				m_csSaveLocationBarText.Empty();
		}
		else
			m_csSaveLocationBarText.Empty();
	}
	else
		m_csSaveLocationBarText.Empty();
	//	For dialog context's, we want to raise them to the top when they
	//		begin a load (view source, doc info, html dialogs).
	if(GetContext()->type == MWContextDialog && GetFrame()->GetFrameWnd() != NULL &&
        GetFrame()->GetFrameWnd()->IsWindowEnabled())	{		
		//	Bring it to the front.
		GetFrame()->GetFrameWnd()->BringWindowToTop();

		//	Now if it was an icon, bring it back up.
		if(GetFrame()->GetFrameWnd()->IsIconic())	{
			GetFrame()->GetFrameWnd()->ShowWindow(SW_RESTORE);
		}
	}


    //  Call the base.                          
    return(CPaneCX::GetUrl(pUrl, iFormatOut, bReallyLoading, bForceNew));
}

CNSToolTip*	CWinCX::CreateToolTip(LO_ImageStruct* pImage, CPoint& cpPoint, CL_Layer *layer)
{
	// Added tool tip to the image.
	if ((!pImage || !pImage->image_attr) ) return NULL; // image is not ready yet.

	//	Layout wants this in pixels, not FE units.
	LTRB Rect;
	ResolveElement(Rect, IL_GetImagePixmap(pImage->image_req), pImage->x_offset, pImage->y_offset, 
						pImage->x, pImage->y, pImage->width, pImage->height);
	CPoint cpMap((int) (cpPoint.x - Twips2PixX(Rect.left)), 
		         (int) (cpPoint.y - Twips2PixY(Rect.top)));
	char* alt_text;
	lo_MapRec *map;
	lo_MapAreaRec_struct* loMapRec;
	LO_AnchorData *pAnchorData = LO_MapXYToAreaAnchor(GetDocumentContext(), pImage,
 			cpMap.x, cpMap.y);

	if(pImage == pLastToolTipImg && m_pLastToolTipAnchor == pAnchorData)
	{
		return NULL;

	}	

	if(pAnchorData != NULL)	
		PA_LOCK(alt_text, char *, pAnchorData->alt);
	else
		PA_LOCK(alt_text, char *, pImage->alt);

	pLastToolTipImg = pImage;
	m_pLastToolTipAnchor = pAnchorData;
	delete m_ToolTip;

	if (alt_text && (*alt_text)) {

		m_ToolTip = new CNSToolTip();

		m_ToolTip->Create(CWnd::FromHandle(GetPane()), TTS_ALWAYSTIP);
		if (::IsWindow(m_ToolTip->GetSafeHwnd())){
			m_ToolTip->SetCSID( INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo( GetDocumentContext() )));
			CRect rect(0, 0, 30, 10);
			m_ToolTip->AddTool(CWnd::FromHandle(GetPane()), "", &rect, 1);
			m_ToolTip->Activate(TRUE);
			m_ToolTip->SetDelayTime(250);
			if(pAnchorData != NULL)	{
				map = (lo_MapRec*)pImage->image_attr->usemap_ptr;
				loMapRec = map->areas;
				BOOL done = FALSE;
				int tempPoint[4];

				while (loMapRec != map->areas_last) {
					if (loMapRec->anchor == pAnchorData && loMapRec->coords) {
						tempPoint[0] = *(loMapRec->coords);
						tempPoint[1] = *(loMapRec->coords+1);
						tempPoint[2] = *(loMapRec->coords+2);
						tempPoint[3] = *(loMapRec->coords+3);
						m_ToolTip->SetBounding(tempPoint, loMapRec->coord_cnt, pImage->x, pImage->y);
						break;
					}
					loMapRec = loMapRec->next;
				}
				if ((loMapRec == map->areas_last) && (loMapRec->anchor == pAnchorData) &&
					loMapRec->coords) {
					tempPoint[0] = *(loMapRec->coords);
					tempPoint[1] = *(loMapRec->coords+1);
					tempPoint[2] = *(loMapRec->coords+2);
					tempPoint[3] = *(loMapRec->coords+3);
					m_ToolTip->SetBounding(tempPoint, loMapRec->coord_cnt, pImage->x, pImage->y);
				}
			}
			else {
				RECT rect;
				::SetRect(&rect, CASTINT(pImage->x - GetOriginX()), 
								 CASTINT(pImage->y - GetOriginY()),
								 CASTINT(pImage->x - GetOriginX() + pImage->width),
								 CASTINT(pImage->y - GetOriginY() + pImage->height));
                int32 x_offset = CL_GetLayerXOrigin(layer);
                int32 y_offset = CL_GetLayerYOrigin(layer);

				::OffsetRect(&rect, (int)x_offset, (int)y_offset);
				m_ToolTip->SetBounding((int*)&rect, 4);
			}
			m_ToolTip->UpdateTipText(alt_text, CWnd::FromHandle(GetPane()), 1);
		}
		else {
			delete m_ToolTip;
			m_ToolTip = NULL;
		}
	}
	else
		m_ToolTip = NULL;
	if(pAnchorData != NULL)	
		PA_UNLOCK(pAnchorData->alt);
	else
		PA_UNLOCK(pImage->alt);
	return m_ToolTip;
}

void CWinCX::ClipChildren(CWnd *pWnd, BOOL bSet)
{
    CFrameGlue *pGlue = GetFrame();
    if (pGlue) {
	pGlue->ClipChildren(pWnd, bSet);
    }
}
#ifdef DDRAW
void CWinCX::BltToScreen(LTRB& rect, DDBLTFX* fx)
{
	if (m_physicWinRect.IsEmpty())
	   CalcWinPos();

	if (m_lpDDSPrimary)	{
		RECT destRect;
		RECT rcRect;
		destRect.left = rect.left + GetWindowsXPos();
		destRect.top = rect.top + GetWindowsYPos();
		destRect.right = rect.right + GetWindowsXPos();
		destRect.bottom = rect.bottom + GetWindowsYPos();
		if (destRect.bottom > m_physicWinRect.bottom) {
			destRect.bottom = m_physicWinRect.bottom;
		}
		if (destRect.right > m_physicWinRect.right) {
			destRect.right = m_physicWinRect.right;
		}
		if (destRect.top > destRect.bottom) {  // for scrolling case
			destRect.top = destRect.top % (destRect.bottom - destRect.top);
		}
		if (destRect.left > destRect.right) {  // for scrolling case
			destRect.left = destRect.left % (destRect.right - destRect.left);
		}
		rcRect.left = rect.left;
		rcRect.top = rect.top;
		rcRect.right = rect.right;
		rcRect.bottom = rect.bottom;
		HRESULT err;
		err = m_lpDDSBack->ReleaseDC(m_offScreenDC);
		err = m_lpDDSPrimary->Blt(&destRect, m_lpDDSBack, &rcRect, DDBLT_WAIT, NULL);
		if (err == DDERR_SURFACELOST) {
			RestoreAllDrawSurface();
			err = m_lpDDSPrimary->Blt(&destRect, m_lpDDSBack, &rcRect, DDBLT_WAIT, NULL);
		}
#ifdef DEBUG_mhwang
		if ( err != DD_OK) {
		TRACE("CWinCX::BlttoScreen err = %x\n",
				err);
		}
#endif
		m_lpDDSBack->GetDC(&m_offScreenDC);
	}
}
#endif


#ifdef DDRAW
int	 CWinCX::DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, LTRB& rect)
{
	SetClipOnDrawSurface(m_lpDDSBack, (HRGN)m_pDrawable->GetClip());
	CDCCX::DisplayPixmap(image, mask, x, y, x_offset, y_offset, width, height, rect);
	BOOL offScreenDrawing = m_pDrawable && (m_pDrawable  == m_pOffscreenDrawable); 
	if (m_lpDDSPrimary && !offScreenDrawing && !m_ScrollWindow) { 
		BltToScreen(rect, NULL);
	}
	return (1);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// Embedded Stuff
////////////////////////////////////////////////////////////////////////////////


/*
 * Front-end callback from lib/plugin that creates the window for a
 * brand new plugin
 */
void CWinCX::CreateEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp)
{
    if (XP_FAIL_ASSERT(pContext != NULL && pApp != NULL && pApp->np_data != NULL))
        return;

    LO_EmbedStruct *pEmbed = ((np_data *) pApp->np_data)->lo_struct;
    if (XP_FAIL_ASSERT(pEmbed != NULL))
        return;

    // Register the window class	
    HINSTANCE hinst = AfxGetInstanceHandle();
    char szClassName[] = "aPluginWinClass";
    WNDCLASS wc;
    
    if(! GetClassInfo(hinst, szClassName, &wc)) {
        wc.style         = 0;
        wc.lpfnWndProc   = DefWindowProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hinst;
        wc.hIcon         = NULL;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        wc.lpszMenuName  = (LPCSTR) NULL;
        wc.lpszClassName = szClassName;

        if (XP_FAIL_ASSERT(RegisterClass(&wc) != 0))
            return;
    }

    // get the current view for param 5 of CreateWindow() below
    HWND cView = PANECX(pContext)->GetPane();

    //  Determine location of plugin rect in PIXELS
    RECT rect;

    // XXX This is a hack to get stuff working. The iLocation
    // parameter is passed to FE_GetEmbedSize(); unfortunately, I
    // don't yet pass it down through to NPL_CreatePlugin(). As far as
    // I can tell, it's always set to FE_VIEW, so I'll just hack it
    // for now.
    //
    //GetPluginRect(pContext, pEmbed, rect, iLocation, TRUE);
    GetPluginRect(pContext, pEmbed, rect, FE_VIEW, TRUE);

    HWND hWnd = ::CreateWindow(szClassName,
                               "a Plugin Window",
                               WS_CHILD | (pEmbed->ele_attrmask & LO_ELE_INVISIBLE ? 0 : WS_VISIBLE),
                               rect.left,
                          rect.top,
                          rect.right - rect.left,
                          rect.bottom - rect.top,
                          cView,
                          NULL,
                          hinst,
                          NULL);

    if (XP_FAIL_ASSERT(hWnd != NULL))
        return;

    // Allocate a new NPWindow structure to hold the plugin's window
    // information.
    NPWindow* pAppWin = XP_NEW(NPWindow);
    if (XP_FAIL_ASSERT(pAppWin != NULL)) {
        ::DestroyWindow(hWnd);
        return;
    }

    pAppWin->window = (void*)hWnd;

    // set the NPWindow rect
    pAppWin->x      = rect.left;
    pAppWin->y      = rect.top;
    pAppWin->width  = rect.right - rect.left;
    pAppWin->height = rect.bottom - rect.top;
    pAppWin->type   = NPWindowTypeWindow;
        
    // Adobe hack. Turn on clip children
    CFrameGlue *pGlue = WINCX(pContext)->GetFrame();
    if (pGlue)
        pGlue->ClipChildren(CWnd::FromHandle((HWND)pAppWin->window), TRUE);

    pApp->wdata = pAppWin;
}

/*
 * Front-end callback from lib/plugin that saves a plug-in's window in
 * a safe place to be restored (or destroyed) later.
 */
void CWinCX::SaveEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp)
{
    if (XP_FAIL_ASSERT(pContext != NULL && pApp != NULL))
        return;

    NPWindow* pAppWin = pApp->wdata;
    if (XP_FAIL_ASSERT(pAppWin != NULL))
        return;

    // If this isn't a windowed plugin, we've got nothing to do...
    if (pAppWin->type != NPWindowTypeWindow)
        return;

    // Find the first non-grid parent of the applet window
    MWContext* pSafeContext = XP_GetNonGridContext(pContext);
    if (XP_FAIL_ASSERT(pSafeContext != NULL))
        return;

    HWND parent = PANECX(pSafeContext)->GetPane();
    if (XP_FAIL_ASSERT(parent != NULL))
        return;

    // Hide and re-parent the window. We'll restore it in GetEmbedSize()
    ::ShowWindow((HWND)pAppWin->window, SW_HIDE);
    ::SetParent((HWND)pAppWin->window, parent);
}

/*
 * Front-end callback from lib/plugin that tells us to restore a
 * previously saved window and reparent it to the current context.
 */
void CWinCX::RestoreEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp)
{
    if (XP_FAIL_ASSERT(pContext != NULL && pApp != NULL))
        return;

    NPWindow* pAppWin = pApp->wdata;
    if (XP_FAIL_ASSERT(pAppWin != NULL))
        return;

    HWND parent = WINCX(pContext)->GetPane();
    if (XP_FAIL_ASSERT(parent != NULL))
        return;

    ::SetParent((HWND)pAppWin->window, parent);
    ::ShowWindow((HWND)pAppWin->window, SW_SHOW);
}

/*
 * Front-end callback from lib/plugin that tells us to destroy
 * an embedded window
 */
void CWinCX::DestroyEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp)
{
    if (XP_FAIL_ASSERT(pContext != NULL && pApp != NULL))
        return;

    NPWindow* pAppWin = pApp->wdata;
    if (XP_FAIL_ASSERT(pAppWin != NULL))
        return;

    if((pAppWin->window != NULL) && (pAppWin->type == NPWindowTypeWindow)) {
        // the Shockwave and WebFX plugins have bugs that set
        // WS_CLIPCHILDREN without clearing it so do it for them
        HWND hWndParent = ::GetParent((HWND)(DWORD)pAppWin->window);
        ::SetWindowLong(hWndParent, GWL_STYLE,
                        ::GetWindowLong(hWndParent, GWL_STYLE) & ~WS_CLIPCHILDREN);

        // Clear the WS_CLIPCHILDREN style for all parent windows. This
        // was set to satify the Adobe wankers who create a child window that's
        // in a separate process
        if (! IsPrintContext()) {
            CWinCX *pGCX = WINCX(pContext);
            if(pGCX) {
                CFrameGlue *pGlue = pGCX->GetFrame();
                if(pGlue) {
                    pGlue->ClipChildren(CWnd::FromHandle((HWND)pAppWin->window), FALSE);
                }
            }
        }

        // destroy the plugin client area
        DestroyWindow((HWND)(DWORD)pAppWin->window);
        pAppWin->window = NULL;
    }	
    XP_FREE(pAppWin);	

    // turn scrollbars back on
    if(pApp->pagePluginType == NP_FullPage)
        FE_ShowScrollBars(pContext, TRUE);
}




////////////////////////////////////////////////////////////////////////////////
// Java Stuff
////////////////////////////////////////////////////////////////////////////////

#ifdef JAVA

void PR_CALLBACK 
FE_DisplayNoJavaIcon(MWContext *pContext, LO_JavaAppStruct *java_struct)
{
    /* write me */
}

void* PR_CALLBACK 
FE_CreateJavaWindow(MWContext *pContext, LO_JavaAppStruct *java_struct,
		    int32 xp, int32 yp, int32 xs, int32 ys)
{
    CWinCX* ctxt = WINCX(pContext);
    LJAppletData* ad = (LJAppletData*)java_struct->session_data;
    HWND parent;

    PR_ASSERT(ad);

    // get the current view 
    parent = ctxt->GetPane();

    /* Adjust xp and yp for their offsets within the window */
    xp -= ctxt->m_lOrgX;
    yp -= ctxt->m_lOrgY;

    // Register the window class
    HINSTANCE hinst = AfxGetInstanceHandle();
    HWND hWnd = NULL;
    char szClassName[] = "aJavaAppletWinClass";
    WNDCLASS wc;
    BOOL result = FALSE;

    if((result = GetClassInfo(hinst, szClassName, &wc)) == FALSE)
    {
	wc.style         = 0;
	wc.lpfnWndProc   = DefWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hinst;
	wc.hIcon         = NULL;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszMenuName  = (LPCSTR) NULL;
	wc.lpszClassName = szClassName;
	result = RegisterClass(&wc);
    }
    if(result != FALSE) {
	hWnd = ::CreateWindow(szClassName,
			      "a JavaApplet Window",
			      WS_CHILD | WS_CLIPCHILDREN,
			      xp,
			      yp,
			      xs,
			      ys,
			      parent,
			      NULL,
			      hinst,
			      NULL);
    }
    /* set the default naviagtor palette for use by awt */
    ad->fe_data = (void*)ctxt->GetPalette();

    return (void*)hWnd;
}

void* PR_CALLBACK
FE_GetAwtWindow(MWContext *pContext, LJAppletData* ad)
{
    if (::GetWindow((HWND)ad->window,GW_CHILD)==NULL) {
	return NULL;
	}
	
    /* Above was added as an interim fix just so we can release 4.02.
       The problem was that GetWindow is returning a near pointer and
       this routine is simply putting DS into DX so it can return a far
       pointer to its caller.  But if GetWindow returns null (AX=0), it
       is still putting DS into DX so it returns DS:0 to it s caller
       rather than 0:0.  Probably a casting bug but it needs further
       investigation before a final fix is made -- Steve Morse */

    return (void*)::GetWindow((HWND)ad->window, GW_CHILD);
}

void PR_CALLBACK 
FE_RestoreJavaWindow(MWContext *pContext, LJAppletData* ad,
		     int32 xp, int32 yp, int32 xs, int32 ys)
{
    CWinCX* ctxt = WINCX(pContext);

    /* Adjust xp and yp for their offsets within the window */
    xp -= ctxt->m_lOrgX;
    yp -= ctxt->m_lOrgY;

    XP_TRACE(("Restore: win=%x isWin=%d\n", ad->window, ::IsWindow((HWND)ad->window)));

    if (pContext->is_grid_cell) {
	/*
	** Reparent the window onto the current window in case it got
	** moved from a grid cell when stopped.
	*/
	HWND parent = ctxt->GetPane();
	PR_ASSERT( ::IsWindow( (HWND)ad->window) );
	::SetParent( (HWND)ad->window, parent );
    }
    /*
    ** Set the CLIPCHILDREN style for the parent window to prevent
    ** EraseBackground events from obliterating the java window
    */
    ctxt->ClipChildren(CWnd::FromHandle((HWND)ad->window), TRUE);

    ::SetWindowPos((HWND)ad->window, NULL, xp, yp, xs, ys, SWP_NOZORDER);
}

void PR_CALLBACK 
FE_SetJavaWindowPos(MWContext *pContext, void* window,
		    int32 xp, int32 yp, int32 xs, int32 ys)
{
    CWinCX* ctxt = WINCX(pContext);

    /* Adjust xp and yp for their offsets within the window */
    xp -= ctxt->m_lOrgX;
    yp -= ctxt->m_lOrgY;

    ::SetWindowPos((HWND)window, NULL, xp, yp, xs, ys, SWP_NOZORDER);
}

void PR_CALLBACK 
FE_SetJavaWindowVisibility(MWContext *context, void* window, PRBool visible)
{
    if (visible && !::IsWindowVisible((HWND)window))
	::ShowWindow((HWND)window, SW_SHOW);
    else if (!visible && ::IsWindowVisible((HWND)window))
	::ShowWindow((HWND)window, SW_HIDE);
}

void PR_CALLBACK
FE_SaveJavaWindow(MWContext *pContext, LJAppletData* ad, void* pWindow)
{
    CWinCX* ctxt = WINCX(pContext);
    HWND window = (HWND)pWindow;

    if (window == NULL || 0 == ::IsWindow(window)) return;

    /* Hide the java applet window */
    ::ShowWindow( window, SW_HIDE );

    /*
    ** Clear the WS_CLIPCHILDREN style for all parent windows.  This 
    ** was set to prevent EraseBackground events from obliterating
    ** the java window...
    */
    ctxt->ClipChildren(CWnd::FromHandle(window), FALSE);
            
    if (pContext->is_grid_cell) {
	/*
	** Reparent the applet window to the first non-grid window
	** if it was on a grid cell because the grid will be
	** destroyed.
	**
	** Note: The ad->context contains the non-grid context at this point.
	*/
	PR_ASSERT(!ad->context->is_grid_cell);
	HWND parent = PANECX(ad->context)->GetPane();

	::SetParent( window, parent );
    }

    /* Move the java applet window off screen - for now */
    ::SetWindowPos(window, NULL, -100, -100, 10, 10, 
		   SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER);
}

void PR_CALLBACK
FE_FreeJavaWindow(MWContext *context, struct LJAppletData *appletData,
		  void* window)
{
    HWND hwndFrame = (HWND)(DWORD)window;
    if( hwndFrame ) {
        DestroyWindow(hwndFrame);
    }
}

#endif  /* JAVA */

void CWinCX::DisplayJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *java_struct)
{
#ifdef JAVA
    LJ_DisplayJavaApp(pContext, java_struct,
		      FE_DisplayNoJavaIcon,
		      FE_GetFullWindowSize,
		      FE_CreateJavaWindow,
		      FE_GetAwtWindow,
		      FE_RestoreJavaWindow,
		      FE_SetJavaWindowPos,
		      FE_SetJavaWindowVisibility);
#endif  /* JAVA */
}

void CWinCX::HideJavaAppElement(MWContext *pContext, LJAppletData * session_data)
{
#ifdef JAVA
    LJ_HideJavaAppElement(pContext, session_data, FE_SaveJavaWindow);
#endif /* JAVA */
}

void CWinCX::FreeJavaAppElement(MWContext *pContext, LJAppletData *ad)
{
#ifdef JAVA
    LJ_FreeJavaAppElement(pContext, ad,
			  FE_SaveJavaWindow,
			  FE_FreeJavaWindow);
#endif  /* JAVA */
}

void CWinCX::GetJavaAppSize(MWContext *pContext, LO_JavaAppStruct *java_struct,
                            NET_ReloadMethod reloadMethod)
{
#ifdef JAVA
    LJ_GetJavaAppSize(pContext, java_struct, reloadMethod);
#else
// jevering: should this be inside ifdef JAVA?
//    FE_DisplayNoJavaIcon(pContext, java_struct);
    java_struct->width = 1;
    java_struct->height = 1;
#endif  /* ! JAVA */
}

////////////////////////////////////////////////////////////////////////////////
// End of Java Stuff
////////////////////////////////////////////////////////////////////////////////

void CWinCX::HandleClippingView(MWContext *pContext, LJAppletData *appletD, int x, int y, int width, int height)
{
}

void CWinCX::LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight)
{
	//  Call the base.
    CPaneCX::LayoutNewDocument(pContext, pURL, pWidth, pHeight, pmWidth, pmHeight);

	//	Set our security indicators.
	if(GetFrame()->GetMainContext() != NULL)	{
		GetFrame()->SetSecurityStatus(XP_GetSecurityStatus(GetFrame()->GetMainContext()->GetContext()));
	}

    //  We're beginning to layout.
    m_bIsLayingOut = TRUE;

    //  Make sure we have our normal arrow cursor loaded (we loaded the wait
    //      in GetUrl).
    SetCursor(theApp.LoadStandardCursor(IDC_ARROW));

	//	No old progress;
	m_lOldPercent = 0;

	//	Update the Frame's URL bar,
	//		only if what is currently there hasn't changed, and only
	//		if we're not a grid cell.
	CString csText;

	if(GetContext()->type == MWContextBrowser && !EDT_IS_EDITOR(GetContext()))	{
	    LPCHROME pChrome = GetFrame()->GetChrome();
	    if(pChrome) {
    		CWnd *pWnd = pChrome->GetToolbar(ID_LOCATION_TOOLBAR);

    		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CURLBar))){
    			CURLBar *pUrlBar = (CURLBar *) pWnd;

    			if(pUrlBar != NULL) {
    				pUrlBar->m_pBox->GetWindowText(csText);
        			if(m_csSaveLocationBarText == csText && IsGridCell() == FALSE)	{
    					pUrlBar->UpdateFields(pURL->address);
        			}
    			}
    			else
    				csText.Empty();
    		}
    		else
    			csText.Empty();
	    }
	}
	else
		csText.Empty();

    //  Clear those old elements that we tracked in the previously
    //      loaded page (see FireMouseOverEvent....)
    m_pLastOverAnchorData = NULL;
    m_pLastOverElement = NULL;
    m_pStartSelectionCell = NULL;
    m_bLastOverTextSet = FALSE;
    m_pLastImageObject = NULL;

//#ifndef NO_TAB_NAVIGATION
	m_lastTabFocus.pElement		= NULL;
	m_lastTabFocus.mapAreaIndex	= 0;		// 0 means no focus, start with index 1.
	m_lastTabFocus.pAnchor			= NULL;
	m_isReEntry_setLastTabFocusElement  = 0;     // to prevent re-entry

	ClearMainFrmTabFocusFlag();

//#endif	/* NO_TAB_NAVIGATION */
	::SetTextAlign(GetContextDC(),TA_NOUPDATECP);
}  // LayoutNewDocument()

void CWinCX::SetMainFrmTabFocusFlag( int nn )
{

	CFrameGlue * pFrame = GetFrame();
	if( pFrame ) {
		CFrameWnd * pFrameWindow = pFrame->GetFrameWnd();
		if( pFrameWindow && pFrameWindow->IsKindOf(RUNTIME_CLASS(CMainFrame)))	{
			((CMainFrame *)pFrameWindow)->SetTabFocusFlag( nn );
		}
	}
}

/* A specialized form of SetMainFrmTabFocusFlag used for setting that flag
   to a generic state (outside of any frames).  This is intended for use
   when loading new contents into this CWinCX.  It only clears the flag
   if we are the main frame's current active view (or if the current
   active view can't be identified).  This prevents the loading of one frame's
   contents from affecting a tab focus currently within another frame.
*/
void CWinCX::ClearMainFrmTabFocusFlag(void) {

	CFrameGlue * pFrame = GetFrame();
	if( pFrame ) {
		CFrameWnd * pFrameWindow = pFrame->GetFrameWnd();
		if( pFrameWindow && pFrameWindow->IsKindOf(RUNTIME_CLASS(CMainFrame))) {
			CMainFrame * pWin = (CMainFrame *) pFrameWindow;
			CWinCX * pActiveContext = pWin->GetActiveWinContext();
			if( pActiveContext == NULL || pActiveContext == this )
				pWin->SetTabFocusFlag(CMainFrame::TAB_FOCUS_IN_NULL);
		}
	}
}

void CWinCX::FinishedLayout(MWContext *pContext)    {
    //  Call the base.
    CPaneCX::FinishedLayout(pContext);

    //  We're no longer laying out.
    m_bIsLayingOut = FALSE;

	//	Progress should be maxed.
	m_lOldPercent = 100;
}

void CWinCX::AllConnectionsComplete(MWContext *pContext)    
{
    //  Call the base.
    CDCCX::AllConnectionsComplete(pContext);

	//	Stop our frame's animation, if the main context of the frame is no longer busy.
    if(GetFrame()->GetMainContext()) {
    	if(XP_IsContextBusy(GetFrame()->GetMainContext()->GetContext()) == FALSE) {
    		//	Okay, stop the animation.
    		StopAnimation();

    		//	Also, we can clear the progress bar now.
    		LPCHROME pChrome = GetFrame()->GetChrome();
    		if(pChrome) {
    			LPNSSTATUSBAR pIStatusBar = NULL;
    			pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
    			if ( pIStatusBar ) {
    				pIStatusBar->SetProgress(0);
    				pIStatusBar->Release();
    			}
    		}

			// We need to make sure the toolbar buttons are correctly updated. If we
			// don't force it now it won't happen until the app goes idle (which could
			// be when the user moves the mouse over the window, for example)
            CGenericView *pView = GetView();
            ASSERT(pView);

			CFrameWnd*	pFrameWnd = pView->GetParentFrame();

			ASSERT(pFrameWnd);
			if (pFrameWnd) {
				pFrameWnd->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
			}
    	}
    }

	if( theGlobalNSFont.WebfontsNeedReload( pContext ) )
	{
		// need to remove all font cache before reload.
		ClearFontCache();
		int usePassInType = 1;
		NiceReload(usePassInType, NET_RESIZE_RELOAD );
	}
}

void CWinCX::UpdateStopState(MWContext *pContext)
{
#ifdef XP_WIN32
    // Force the toolbar buttons to be correctly updated. If we
    // don't force it now it won't happen until the app goes idle (which could
    // be when the user moves the mouse over the window, for example)
    CGenericView *pView = GetView();
    if(pView) {
        CFrameGlue *pGlue = GetFrame();
        CFrameWnd *pFWnd = pGlue->GetFrameWnd();
        if(pFWnd) {
            pFWnd->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
        }
    }
#endif
}

// Note: We now use pTitle = NULL to clear an existing title
void CWinCX::SetDocTitle(MWContext *pContext, char *pTitle) 
{
    //  Call the base.
    CDCCX::SetDocTitle(pContext, pTitle);
    MWContext * pMWContext = GetContext();
    BOOL        bIsPageComposer = EDT_IS_EDITOR(pMWContext) && (pMWContext->type != MWContextMessageComposition);
                                
    // Guard against the case where our window has gone away and the
    //   closing of the window causes the stream to complete which
    //   causes us to get back in here with a half torn down window
    if( GetDocumentContext() == NULL )
	return;

	//	We need to be the main context in order to set a
	//		frames title.
	if( GetFrame()->GetMainContext() == this ) {
	    //  Munge the title string.
	    CString csTitle;

  	    CString csUrlTitle = pTitle;
        // This should be set at end of GetUrl so we
        //  don't have to depend on history
        CString csBaseURL = LO_GetBaseURL( GetDocumentContext() );

        BOOL bTitleIsSameAsUrl = (csBaseURL == csUrlTitle);

	    if(!csUrlTitle.IsEmpty()) {
            if ( bTitleIsSameAsUrl ) {
                // We won't be adding on the URL after this,
                //  so make a bigger title
                // If we are a URL, cut from the middle
                 WFE_CondenseURL(csUrlTitle, 50, FALSE);
            } else {
                if ( bIsPageComposer ) {
                    // Use just left portion of title
                    csUrlTitle = csUrlTitle.Left(30);
                } else {
                    // We won't add URL to Browser, so use more
                    csUrlTitle = csUrlTitle.Left(100);
                }
            }
            csTitle += csUrlTitle;
        }

        if( bIsPageComposer && !bTitleIsSameAsUrl ){
            // Append URL if we didn't already use it as the title
            // Limit text inside of ( )
	        if( !csUrlTitle.IsEmpty() ){
                csTitle += " : ";
            }
            // Strip off username and password from URL
            char * pStripped = NULL;
            NET_ParseUploadURL( (char*)LPCSTR(csBaseURL), &pStripped, NULL, NULL );
            csBaseURL = pStripped;
            XP_FREEIF(pStripped);
            WFE_CondenseURL(csBaseURL, 50 - (min(csUrlTitle.GetLength(),20)), FALSE);
            csTitle += csBaseURL;
        }

        LPCHROME pChrome = GetFrame()->GetChrome();
        if(pChrome) {
            pChrome->SetDocumentTitle(csTitle);
        }
	}
}

void CWinCX::ClearView(MWContext *pContext, int iView)  {
    //  Call the base.
    CDCCX::ClearView(pContext, iView);

    //  Have the view erase it's background to clear.
    RECT crClear;
	::SetRect(&crClear, 0, 0, (int) GetWidth(), (int) GetHeight());
	if(GetPane())	{
		//	Must first update the window, to get rid of any
		//		queud erase backgrounds and stuff which will
		//		cause the display to become corrupted.
		RECT tempRect;
		::SetRect(&tempRect,0, 0, CASTINT(GetWidth()), CASTINT(GetHeight()));
        ::InvalidateRect(GetPane(), &tempRect, FALSE);
//        ::UpdateWindow(GetPane());
	}
	else	{
	    //  Tell layout to specifically refresh our area.
	    RefreshArea(GetOriginX(), GetOriginY(), GetWidth(), GetHeight());
	}
}

void CWinCX::SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength)   {
    // This saves old Y, which changes under certain delete situations...
    int32 iOriginY = GetOriginY();

    // Make sure that the origin is still visible. (This case only matters when the document
    // height shrinks, which currently only happens when editing.)
    m_lOrgY = max(0, min(m_lOrgY, lLength - m_lHeight));

    // Anytime we change the scrolling origin, we have to tell the compositor.
    if ( GetContext()->compositor)
        CL_ScrollCompositorWindow(GetContext()->compositor, m_lOrgX, m_lOrgY);

    //  Call the base.
    CPaneCX::SetDocDimension(pContext, iLocation, lWidth, lLength);

#ifdef EDITOR
    if(EDT_IS_EDITOR(pContext) &&
       GetOriginY() != iOriginY ) {
        // Redraw entire view
        // This is needed instead to be sure caret is moved to correct location
        //   after doc size has changed, like after a deletion 
        //   at bottom of doc (bug 65199)
        EDT_RefreshLayout(pContext);
    }
#endif /* EDITOR */    
}

//
// Fake scroll messages so that we use scrollwindowex() to move the window
//   so that our form elements actually move
// 
void CWinCX::SetDocPosition(MWContext *pContext, int iLocation, int32 lX, int32 lY) 
{
    int iPos;

	//	If we're (the window) wider than the document (layout),
	//		then don't use the X value.
	if(GetDocumentWidth() <= GetWidth())	{
		lX = GetOriginX();
	}

    int32 lRemY = GetOriginY();
    int32 lRemX = GetOriginX();

    //  Call the base.
    CDCCX::SetDocPosition(pContext, iLocation, lX, lY);

    // LTNOTE: in the editor documents can shrink.  Editor contexts therefor
    //  must always scroll.

    //  Scrolling in On?ScrollCX is lossy, and causes wild results
    //      (scrolling right will cause to scroll right and up).
    //  Make sure there is a need to scroll before attempting to scroll.

    // scroll to the correct Y location
    if((m_lDocHeight - m_lHeight > 0) && lRemY != lY) {
        iPos = (int) ((double) lY * (double) GetPageY() / (double) (m_lDocHeight - m_lHeight));
        Scroll(SB_VERT, SB_THUMBTRACK, iPos, NULL);
    }
    else if ( EDT_IS_EDITOR( pContext ) ){
        if((m_lDocHeight - m_lHeight > 0)) {
            iPos = (int) ((double) lY * (double) GetPageY() / (double) (m_lDocHeight - m_lHeight));
            Scroll(SB_VERT, SB_THUMBTRACK, iPos, NULL);
        }
        else    {
            Scroll(SB_VERT, SB_THUMBTRACK, 0, NULL);
        }
    }
        

    // now do X
    if((m_lDocWidth - m_lWidth > 0) && lRemX != lX) {
        iPos = (int) ((double) lX * (double) GetPageX() / (double) (m_lDocWidth - m_lWidth));
        Scroll(SB_HORZ, SB_THUMBTRACK, iPos, NULL);
    }
    else if( EDT_IS_EDITOR(pContext) ){
        if((m_lDocWidth - m_lWidth > 0)) {
            iPos = (int) ((double) lX * (double) GetPageX() / (double) (m_lDocWidth - m_lWidth));
            Scroll(SB_HORZ, SB_THUMBTRACK, iPos, NULL);
        }
        else    {
            Scroll(SB_HORZ, SB_THUMBTRACK, 0, NULL);
        }
    }
    // realize scrollbars will have been called by the scrolling routines
}

//#ifndef NO_TAB_NAVIGATION 
// CWinCX::DisplayFeedback() is called by CFE_DisplayFeedback() to draw image selection feedback,
// and tab focus.
//
// For clear calling relation, CDCCX::DisplayFeedback() is renamed to CDCCX::DisplayImageFeedback() 
// and added theArea, drawFlag parameters.
void CWinCX::DisplayFeedback(MWContext *pContext, int iLocation, LO_Element *pElement)
{
	if( pElement->lo_any.type == LO_IMAGE ) {
		// Only if the pImage has the tab focus, can we get the tab focus area.
		// otherwise it will return NULL.
		lo_MapAreaRec * theArea = NULL;
		uint32			drawFlag = 0;
		getImageDrawFlag(pContext, (LO_ImageStruct *)pElement, &theArea, &drawFlag);
		
		CDCCX::DisplayImageFeedback(pContext, iLocation, pElement, theArea, drawFlag );
	}
	// do nothing if it is not image.
}
//#endif /* NO_TAB_NAVIGATION */

void CWinCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent)	{
	//	Enusre the safety of the value.
	lPercent = lPercent < -1 ? 0 : ( lPercent > 100 ? 100 : lPercent );

	if ( m_lOldPercent == lPercent ) {
		return;
	}

	m_lOldPercent = lPercent;
	if (GetFrame()) {
	    LPCHROME pChrome = GetFrame()->GetChrome();
	    if(pChrome) {
    		LPNSSTATUSBAR pIStatusBar = NULL;
    		pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
    		if ( pIStatusBar ) {
    			pIStatusBar->SetProgress(CASTINT(lPercent));
    			pIStatusBar->Release();
    		}
	    }
	}
}

void CWinCX::Progress(MWContext *pContext, const char *pMessage) {
	if (GetFrame()) {
	    LPCHROME pChrome = GetFrame()->GetChrome();
	    if(pChrome) {
        	LPNSSTATUSBAR pIStatusBar = NULL;
        	pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
        	if ( pIStatusBar ) {
        		pIStatusBar->SetStatusText(pMessage);
        		pIStatusBar->Release();
        	}
	    }
	}
}

void CWinCX::DisplayEdge(MWContext *pContext, int iLocation, LO_EdgeStruct *pEdge)	{
	//	Create an edge window if none currently exists.
	if(pEdge->FE_Data == NULL)	{
		CGridEdge *pNewEdge = new CGridEdge(pEdge, this);
		pEdge->FE_Data = pNewEdge;
	}
	else	{
		//	Update the object's idea of it's owner in case layout is swapping objects on us,
		//		this in turn causes the edge to display in a possibly new location.
		((CGridEdge *)pEdge->FE_Data)->UpdateEdge(pEdge);
	}
}

void CWinCX::FreeEdgeElement(MWContext *pContext, LO_EdgeStruct *pEdge)	{
	//	Get rid of the Edge.
	if(pEdge && pEdge->FE_Data)	{
		delete((CGridEdge *)(pEdge->FE_Data));
		pEdge->FE_Data = NULL;
	}
}

CWinCX *CWinCX::DetermineTarget(const char *pTargetName)	{
	//	This function decides what target will load a URL.
	//	This is to facilitate grids loading into targeted locations.

	//	Ensure the target name exists.
	if(pTargetName == NULL || *pTargetName == '\0')	{
        //  If we're a dialog, and we can not close ourselves, and we're modal over another
        //      window context, send the url to that context instead.
        if(GetContext()->type == MWContextDialog &&
           m_cplModalOver.IsEmpty() == FALSE &&
           GetDocument() &&
           GetDocument()->CanClose() == FALSE)   {
            HWND hDestination = (HWND)m_cplModalOver.GetHead();
            if(IsWindow(hDestination))  {
               CWnd *pDestination = CWnd::FromHandlePermanent(hDestination);
                if(pDestination && pDestination->IsKindOf(RUNTIME_CLASS(CGenericFrame)))    {
                    CGenericFrame *pFrame = (CGenericFrame *)pDestination;
                    if(pFrame->GetActiveContext() &&
                       pFrame->GetActiveContext()->IsWindowContext() &&
                       pFrame->GetActiveContext()->IsDestroyed() == FALSE)  {
                        //  GARRETT:  This needs to support PANECX
                        return(VOID2CX(pFrame->GetActiveContext(), CWinCX)->DetermineTarget(pTargetName));
                    }
                }
            }
        }
		return(this);
	}

	//	Attempt to find the context already in existance.
	MWContext *pCandidate = XP_FindNamedContextInList(GetContext(), (char *)pTargetName);
	if(pCandidate != NULL)	{
		if(ABSTRACTCX(pCandidate)->IsWindowContext() == FALSE)	{
			//	Don't allow anything but windows to take this burden.
			return(this);
		}
		else	{
			return(VOID2CX(pCandidate->fe.cx, CWinCX));
		}
	}

	//	Create a new one, dammit.
	pCandidate = FE_MakeBlankWindow(GetContext(), NULL, (char *)pTargetName);
	if(pCandidate == NULL)	{
		//	Just use ourselves, I see no way out.
		return(this);
	}
	return(VOID2CX(pCandidate->fe.cx, CWinCX));
}

int32 CWinCX::QueryProgressPercent()	{
	//	If we've no children, return our percentage.
	if(IsGridParent() == FALSE)	{
		return(m_lOldPercent);
	}

	//	It's not empty.
	//	We need to return the sum of each of our grid children, divided by the number
	//		of the grid children.
	int32 lCount = XP_ListCount(GetContext()->grid_children);
	int32 lSum = 0;
	
	//	Go through each child.
	MWContext *pChild;
	XP_List *pTraverse = GetContext()->grid_children;
	while (pChild = (MWContext *)XP_ListNextObject(pTraverse)) {
		ASSERT(ABSTRACTCX(pChild)->IsWindowContext());
		lSum += WINCX(pChild)->QueryProgressPercent();
	}

    // just in case...
    if (lCount < 1)
        return(m_lOldPercent);

	//	Return the sum divided by the count.
	return(lSum / lCount);
}

void CWinCX::StopAnimation()
{
    LPCHROME pChrome = GetFrame()->GetChrome();
    if(pChrome) {
        pChrome->StopAnimation();
    }
}

void CWinCX::StartAnimation()
{
    LPCHROME pChrome = GetFrame()->GetChrome();
    if(pChrome) {
        pChrome->StartAnimation();
    }
}

void CWinCX::OpenFile()	{
	//	If we've a parent context, let them deal with this.
	if(GetParentContext())	{
		if(ABSTRACTCX(GetParentContext())->IsWindowContext())	{
			WINCX(GetParentContext())->OpenFile();
		}
		else	{
			ASSERT(0);
		}
	}
	else if(IsDestroyed() == FALSE)	{
		//	Have the frame bring up the open file dialog.
        if(GetFrame()->GetFrameWnd())	{
            MWContext * pMWContext = GetContext();
            
            // Restrict opening to only HTML files if initiated from Composer frame
            BOOL bOpenIntoEditor = EDT_IS_EDITOR(pMWContext) && (pMWContext->type == MWContextBrowser);
            CWnd * cWnd = GetFrame()->GetFrameWnd();
            
		    char * pName = wfe_GetExistingFileName(
		                    	cWnd->m_hWnd,
		    	                szLoadString(IDS_OPEN), bOpenIntoEditor ? HTM_ONLY : HTM, TRUE);

			if(pName == NULL) {
				//	Canceled.
				return;
			}
#ifdef EDITOR
            if( bOpenIntoEditor ){
                FE_LoadUrl(pName, LOAD_URL_COMPOSER);
            } else {
                if( EDT_IS_EDITOR(pMWContext) ){
                    // Open a file into a new browse window from an Editor
                    FE_LoadUrl(pName, LOAD_URL_NAVIGATOR);
                } else {
                    // Load URL into existing Browser window
    			    NormalGetUrl(pName);
                }
            }
#else
            NormalGetUrl(pName);
#endif // EDITOR
	    	XP_FREE(pName);
		}
		else	{
			ASSERT(0);
		}
	}
}

BOOL CWinCX::CanOpenFile()	{
	//	If we've a parent context, let them deal with this.
	if(GetParentContext())	{
		if(ABSTRACTCX(GetParentContext())->IsWindowContext())	{
			return(WINCX(GetParentContext())->CanOpenFile());
		}
		else	{
			ASSERT(0);
			return(FALSE);
		}
	}
	else if(IsDestroyed() == TRUE)	{
		return(FALSE);
	}
	else	{
		//	Can always open a file.
		//	Perhaps not if we're in some kiosk mode.
		return(TRUE);
	}
}

BOOL CWinCX::SaveOleDocument() {
	CGenericDoc * pDoc = GetDocument();  

	return pDoc->OnSaveDocument(NULL);
	
}
void CWinCX::SaveAs()	{
	if (!SaveOleDocument()) {
		if(IsDestroyed() == FALSE && IsGridParent() == FALSE)	{
			//	Make sure we have a history entry.
			History_entry *pHist = SHIST_GetCurrent(&(GetContext()->hist));
			if(pHist != NULL && pHist->address != NULL)	{
				//	Can save.
				//	Don't care if GetFrameWnd returns NULL.
				CSaveCX::SaveAnchorObject((const char *)pHist->address,
					pHist,  GetFrame()->m_iCSID, GetFrame()->GetFrameWnd());
			}
		}
	}
}

BOOL CWinCX::CanSaveAs()	{

	// if there is only 1 ole item there, don't save the documant.
	CGenericDoc * pDoc = GetDocument();
	POSITION pos = pDoc->GetStartPosition(); 
	CNetscapeCntrItem *pItem;
    CDocItem* item = pDoc->GetNextItem(pos );
	// If the current embed item is full page OLE and is not in-placed actived, return FALSE
	// so the saveAs menu item will be disabled.
	while (item && item->IsKindOf(RUNTIME_CLASS(CNetscapeCntrItem))) {
		pItem = (CNetscapeCntrItem*)item;
		if (pItem->m_isFullPage && !pItem->IsInPlaceActive( ))
			return FALSE;   
		item = pDoc->GetNextItem(pos );
	}
    pItem = (CNetscapeCntrItem *)pDoc->GetInPlaceActiveItem(CWnd::FromHandle(GetPane()));
	if (pItem) { 
		if (pItem->m_bCanSavedByOLE)   // see can this item be saved.
			return TRUE;
		else
			return FALSE;
	}
	if(IsDestroyed() == TRUE || IsGridParent() == TRUE)	{
		return(FALSE);
	}
	else	{
		//	Can't save if there's no history entry.
		History_entry *pHist = SHIST_GetCurrent(&(GetContext()->hist));
		if(pHist == NULL)	{
			return(FALSE);
		}
		else if(pHist->address == NULL)	{
			return(FALSE);
		}
		else	{
			return(TRUE);
		}
	}
}

void CWinCX::PrintContext() {
    //Abstraction layer has context type print which
    //conflicts with abstracting the Print() function
    Print();
}

void CWinCX::Print()	{
	if(IsDestroyed() == TRUE || CanPrint() == FALSE)	{
		return;
	}
    MWContext *pMWContext = GetContext();
    if( !pMWContext )
        return;
    // Note: We no longer have to force saving a page 
    //  before printing because we use a temporary file instead
    
    //  Always pass the WYSIWYG attribute for printing URLs (javascript).
    SHIST_SavedData SavedData;
    URL_Struct *pUrl = CreateUrlFromHist(TRUE, &SavedData, TRUE);

    char *pDisplayUrl = NULL;
#ifdef EDITOR
    if( EDT_IS_EDITOR(pMWContext) )
    {
        // Save actual address we want to show in header or footer
        //  to pass on to print context
        if( pUrl->address )
            pDisplayUrl = XP_STRDUP(pUrl->address);

        // Save current contents to a temporary file if
        //  we are a Mail Message, a New Document, or there are changes to current page
        // This will change pUrl->address to temp filename
        if( !FE_PrepareTempFileUrl(pMWContext, pUrl) )
        {
            XP_FREEIF(pDisplayUrl);
            // Failed to save to the temp file - abort
            return;
        }
    }
#endif //EDITOR

	// Copy the necessary information into the URL's saved data so that we don't
	// make a copy of the plug-ins when printing
	NPL_PreparePrint(pMWContext, &pUrl->savedData);

    CGenericView *pView = GetView();
    if(pView)   {
	    CPrintCX::PrintAnchorObject(pUrl, pView, &SavedData, pDisplayUrl);
    }
    XP_FREEIF(pDisplayUrl);
}

BOOL CWinCX::CanPrint(BOOL bPreview)	{
	//	Can't print if we're destroyed, if there's no view, or if we've no
	//		history or we're a grid parent.
	BOOL bRetval = TRUE;
	if(FALSE == sysInfo.m_bPrinter) {
		bRetval = FALSE;
	}
	else if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else if(GetPane() == NULL)	{
		bRetval = FALSE;
	}
	else if(CanCreateUrlFromHist() == FALSE)	{
		bRetval = FALSE;
	}
	else if(IsGridParent() == TRUE)	{
		bRetval = FALSE;
	}

	//	Lastly, we only allow one print context at a time.
	//	There can be multiple print preview context's however.
	if(bRetval && bPreview == FALSE)	{
		MWContext *pTraverseContext = NULL;
		CAbstractCX *pTraverseCX = NULL;
		XP_List *pTraverse = XP_GetGlobalContextList();
		while (pTraverseContext = (MWContext *)XP_ListNextObject(pTraverse)) {
			if(pTraverseContext != NULL && ABSTRACTCX(pTraverseContext) != NULL)	{
				pTraverseCX = ABSTRACTCX(pTraverseContext);
				if(pTraverseCX->IsPrintContext() == TRUE)	{
					CPrintCX *pPrintCX = VOID2CX(pTraverseCX, CPrintCX);
					if(pPrintCX->IsPrintPreview() == FALSE)	{
						//	Already a print job in progress.
						bRetval = FALSE;
						break;
					}
				}
			}
		}
	}

	return(bRetval);
}

void CWinCX::AllFind(MWContext *pSearchContext)	{
	//	If we've got a parent context, let it deal.
	if(GetParentContext())	{
		if(ABSTRACTCX(GetParentContext())->IsWindowContext())	{
			WINCX(GetParentContext())->AllFind(pSearchContext);
		}
		else	{
			ASSERT(0);
		}
	}
	else if(CanAllFind())	{
		if(GetFrame()->GetFrameWnd())	{
			CNetscapeFindReplaceDialog *dlg;
	  
			dlg = new CNetscapeFindReplaceDialog();
            UINT flags = FR_DOWN | FR_NOWHOLEWORD | FR_HIDEWHOLEWORD;
            BOOL bBrowser = TRUE;
#ifdef EDITOR
            if( pSearchContext )
            {
                bBrowser = !EDT_IS_EDITOR(pSearchContext);
            } else {
                bBrowser = !EDT_IS_EDITOR(GetContext());
            }
#endif
			dlg->Create(bBrowser,
			            theApp.m_csFindString, 
			            bBrowser ? NULL : (LPCTSTR)theApp.m_csReplaceString,
			            flags,
			            GetFrame()->GetFrameWnd());

			//	Let them know who the frame is.
			dlg->SetFrameGlue(GetFrame());
			// If a frame was specified as a search context then perform
			// the search there.  If no context was specified or if 
			// the search context is a top level window then don't set
			// the search context and we'll search normally on the focused frame.
			if (pSearchContext && pSearchContext->grid_parent)
			    dlg->SetSearchContext(pSearchContext);
		}
		else	{
			ASSERT(0);
		}
	}
}

BOOL CWinCX::CanAllFind()	{
	//	If we've a parent context, let them deal with this.
	if(GetParentContext())	{
		if(ABSTRACTCX(GetParentContext())->IsWindowContext())	{
			return(WINCX(GetParentContext())->CanAllFind());
		}
		else	{
			ASSERT(0);
			return(FALSE);
		}
	}
	else if(IsDestroyed() == TRUE)	{
		return(FALSE);
	}
	else	{
		BOOL bRetval = TRUE;
		//	Can't find if there's no history entry.
		History_entry *pHist = SHIST_GetCurrent(&(GetContext()->hist));
		if(pHist == NULL)	{
			bRetval = FALSE;
		}
		else if(pHist->address == NULL)	{
			bRetval = FALSE;
		}
		else if(GetFrame()->CanFindReplace() == FALSE)	{
			//	Frame already is finding/replacing.
			bRetval = FALSE;
		}
		else	{
			bRetval = TRUE;
		}

		return(bRetval);
	}
}

//
// Find again using the settings from last time
//
void CWinCX::FindAgain()
{
    DoFind(NULL, (const char *) theApp.m_csFindString, theApp.m_bMatchCase, 
						theApp.m_bSearchDown, TRUE);
}

//
// Actually do the find operation
//
BOOL CWinCX::DoFind(CWnd * pWnd, const char * pFindString, BOOL bMatchCase, 
						BOOL bSearchDown, BOOL bAlertOnNotFound)
{

	int32 start_position, end_position;
	LO_Element * start_ele_loc = NULL;
	LO_Element * end_ele_loc = NULL;
        CL_Layer *sel_layer = NULL;

	// Start the search from the current selection location	
	LO_GetSelectionEndpoints(GetDocumentContext(), 
	                         &start_ele_loc, 
	                         &end_ele_loc, 
	                         &start_position, 
	                         &end_position,
                                 &sel_layer);

	// look for the text	                         
	if (LO_FindText(GetDocumentContext(),
	                (char *) pFindString,
	                &start_ele_loc, 
	                &start_position, 
	                &end_ele_loc, 
	                &end_position,
	                bMatchCase,
	                bSearchDown) == 1)
	{   
		int32 x, y;
		      
		LO_SelectText(GetDocumentContext(), 
		              start_ele_loc, 
		              start_position, 
		              end_ele_loc,
            		  end_position, 
            		  &x, 
            		  &y);  
		
        // TODO: This is not correct - it scrolls over unnecessarily
        //FE_SetDocPosition(GetContext(), FE_VIEW, x, y);

        // Get current location and scroll only as necessary
		int32 iViewLeft, iViewTop, iViewWidth, iViewHeight;
        
        FE_GetDocAndWindowPosition(GetContext(), &iViewLeft, &iViewTop, &iViewWidth, &iViewHeight);
        int32 iViewBottom = iViewTop + iViewHeight;
        int32 iViewRight = iViewLeft + iViewWidth;
        BOOL bScroll = FALSE;
        if( x < iViewLeft || x > iViewRight  )
        {
            iViewLeft = max(0,(x - iViewWidth/4));
            bScroll = TRUE;
        }
        // Place found text at 1/4 of window height
        if( y < iViewTop || y > iViewBottom )
        {
            iViewTop = max(0,(y - iViewHeight/4));
            bScroll = TRUE;
        }
        if( bScroll )
            FE_SetDocPosition(GetContext(), FE_VIEW, iViewLeft, iViewTop);
        
        //
        // Ensure the Find/Replace dlg doesn't cover the selection
        //
        CNetscapeFindReplaceDialog *pDlg = GetFrame() ? GetFrame()->GetFindReplace() : NULL;
        if( pDlg )
        {
            RECT rcDlg;
            pDlg->GetWindowRect( &rcDlg );

            RECT rcView;
            ::GetWindowRect( GetPane(), &rcView );
            
            int32 iTextY = rcView.top + (y - GetOriginY());
            
            if( (rcDlg.top < iTextY) && (rcDlg.bottom > iTextY) )
            {
                CRect rcDlgNew( rcDlg );
                
                rcDlgNew.OffsetRect( 0, iTextY - rcDlgNew.top + 20 );
                if( rcDlgNew.bottom > GetSystemMetrics( SM_CYSCREEN ) )
                {
                    rcDlgNew.CopyRect( &rcDlg );
                    rcDlgNew.OffsetRect( 0, iTextY - rcDlgNew.bottom - 1 );
                }
                pDlg->SetWindowPos( NULL, rcDlgNew.left, rcDlgNew.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
            }
        }
        
		return TRUE;
	
	} 
	else if(bAlertOnNotFound){
        HWND hBox = NULL;
        if(NULL == pWnd)    {
            hBox = GetPane();
        }
        else    {
            hBox = pWnd->m_hWnd;
        }

        if(hBox)
            ::MessageBox(hBox, szLoadString(IDS_FIND_NOT_FOUND), szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);

    }
	return FALSE;

}

//
// cause the form containing the given element to be submitted
//
void FE_SubmitInputElement(MWContext * pContext, LO_Element * pElement)
{

	LO_FormSubmitData * submit;
	URL_Struct        * URL_s;

    if(!pContext || !pElement)
        return;

	submit = LO_SubmitForm(ABSTRACTCX(pContext)->GetDocumentContext(), (LO_FormElementStruct *) pElement);
	if(!submit)
		return;

    // Create the URL to load
	URL_s = NET_CreateURLStruct((char *)submit->action, NET_DONT_RELOAD); 

    // attach form data to the URL
	NET_AddLOSubmitDataToURLStruct(submit, URL_s);

    // add referer field if we've got it
    {
        History_entry *he = SHIST_GetCurrent (&pContext->hist);
        if(he && he->address)
            URL_s->referer = XP_STRDUP(he->address);
        else
            URL_s->referer = NULL;
    }

    // start the spinning icon
	//	Load up.
	ABSTRACTCX(pContext)->GetUrl(URL_s, FO_CACHE_AND_PRESENT);

	LO_FreeSubmitData(submit);

}

//	Say wether or not view source is allowed.
BOOL CWinCX::CanViewSource()
{
	//	If we've got a parent context, we need to pass it on up the chain.
	if(GetParentContext() != NULL)	{
		return(WINCX(GetParentContext())->CanViewSource());
	}

	//	We're the top level context, make sure we're not destroyed.
	if(IsDestroyed())	{
		return(FALSE);
	}

	//	Don't let this happen while a context is loading otherwise,
	//		not all the document source is present, and we interrupt
	//		the load.
	if(XP_IsContextBusy(GetContext()) == TRUE)	{
		return(FALSE);
	}

	//	All that matters now is wether or not we can create a URL struct from
	//		our history.
	return(CanCreateUrlFromHist());
}

//	View the source.
void CWinCX::ViewSource()
{
	//	If we've a parent context, we need to pass it on up the chain.
	if(GetParentContext() != NULL)	{
		WINCX(GetParentContext())->ViewSource();
		return;
	}

	if(CanViewSource())	{
		//	Clear the state data.
		URL_Struct *pUrl = CreateUrlFromHist(TRUE);

		if(pUrl != NULL)	{
#ifdef EDITOR
            if( EDT_IS_EDITOR(GetContext()) ){
                EDT_DisplaySource(GetContext());	//	Make sure that it's okay to do this.
                return;
            }
#endif //EDITOR
    			//	Load up.
    	    GetUrl(pUrl, FO_CACHE_AND_VIEW_SOURCE);
		}
	}
}

BOOL CWinCX::CanDocumentInfo()
{
	//	If we've got a parent context, we need to pass it on up the chain.
	if(GetParentContext() != NULL)	{
		return(WINCX(GetParentContext())->CanDocumentInfo());
	}

	//	We're the top level context, make sure we're not destroyed.
	if(IsDestroyed())	{
		return(FALSE);
	}

	//	Don't allow this while we're loading.
	//	They won't be able to view the entire document's tree.
	//	Also, this won't interrupt the current load.
	if(XP_IsContextBusy(GetContext()) == TRUE)	{
		return(FALSE);
	}

	//	All that matters now is wether or not we can create a URL struct from
	//		our history.
	return(CanCreateUrlFromHist());
}

void CWinCX::DocumentInfo()
{
	//	If we've got a parent context, we need to pass it on up the chain.
	if(GetParentContext() != NULL)	{
		WINCX(GetParentContext())->DocumentInfo();
		return;
	}

	//	Make sure it's okay to try this.
	if(CanDocumentInfo())	{
		//	Do it.
		NormalGetUrl("about:document");
	}
}

BOOL CWinCX::CanFrameSource()
{
	if(IsDestroyed())	{
		return(FALSE);
	}

    if(IsGridCell() == FALSE)   {
        return(FALSE);
    }

	//	Don't let this happen while a context is loading otherwise,
	//		not all the document source is present, and we interrupt
	//		the load.
	if(XP_IsContextBusy(GetContext()) == TRUE)	{
		return(FALSE);
	}

	//	All that matters now is wether or not we can create a URL struct from
	//		our history.
	return(CanCreateUrlFromHist());
}

void CWinCX::FrameSource()
{
	if(CanFrameSource())	{
		//	Clear the state data.
		URL_Struct *pUrl = CreateUrlFromHist(TRUE);

		if(pUrl != NULL)	{
#ifdef EDITOR
            if( EDT_IS_EDITOR(GetContext()) ){
                EDT_DisplaySource(GetContext());	//	Make sure that it's okay to do this.
                return;
            }
#endif
    			//	Load up.
    	    GetUrl(pUrl, FO_CACHE_AND_VIEW_SOURCE);
		}
	}
}

BOOL CWinCX::CanFrameInfo()
{
	if(IsDestroyed())	{
		return(FALSE);
	}

    if(IsGridCell() == FALSE)   {
        return(FALSE);
    }

	//	Don't allow this while we're loading.
	//	They won't be able to view the entire document's tree.
	//	Also, this won't interrupt the current load.
	if(XP_IsContextBusy(GetContext()) == TRUE)	{
		return(FALSE);
	}

	//	All that matters now is wether or not we can create a URL struct from
	//		our history.
	return(CanCreateUrlFromHist());
}

void CWinCX::FrameInfo()
{
	//	Make sure it's okay to try this.
	if(CanFrameInfo())	{
		//	Do it.
		NormalGetUrl("about:document");
	}
}

BOOL CWinCX::CanGoHome()
{
	//	Have the parent handle.
	if(GetParentContext() != NULL)	{
		return(WINCX(GetParentContext())->CanGoHome());
	}

	//	We can't be destroyed.
	if(IsDestroyed())	{
		return(FALSE);
	}

	//	We need to have a home page defined.
	if(theApp.m_pHomePage.IsEmpty()) {
		return(FALSE);
	}
	
	return(TRUE);
}

void CWinCX::GoHome()
{
	//	Have the parent handle.
	if(GetParentContext() != NULL)	{
		WINCX(GetParentContext())->GoHome();
		return;
	}

	//	Make sure we can go home.
	if(CanGoHome())	{
		//	Load it.
    	NormalGetUrl(theApp.m_pHomePage);
	}
}

//  This function get's called to size the non client area.
//  On grid cells, we like to put a pixel border on the outsize so we can show activation.
void CWinCX::OnNcCalcSizeCX(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
	if(IsGridCell() == TRUE &&
		IsGridParent() == FALSE &&
		GetPane() != NULL &&
        IsIconic(GetPane()) == FALSE &&
		IsDestroyed() == FALSE &&
        m_bHasBorder == TRUE
		)	{
        if(lpncsp)  {
            lpncsp->rgrc[0].left += 1;
            lpncsp->rgrc[0].top += 1;
            lpncsp->rgrc[0].right -= 1;
            lpncsp->rgrc[0].bottom -= 1;
        }
    }
}


//	This function get's called to draw the area surrounding the client area of
//		the view, if anything special needs be done.
void CWinCX::OnNcPaintCX()	{
	if(IsGridCell() == TRUE &&
		IsGridParent() == FALSE &&
		GetPane() != NULL &&		
		IsDestroyed() == FALSE
#ifdef XP_WIN16
		&& GetFrame()->GetFrameWnd() != NULL
#endif
        && m_bHasBorder == TRUE ) {


		//	Get a DC from our true window, we'll be using it to do the deed, as
		//		we're drawing outside of our client region.
		HDC hDC;
#ifdef XP_WIN16
		CDC *pDC = GetFrame()->GetFrameWnd()->GetDC();
		hDC = pDC->GetSafeHdc();
#else
        hDC = ::GetWindowDC(GetPane());
#endif
		if(hDC != NULL)	{
            RECT crWindowRect;
            ::GetWindowRect(GetPane(), &crWindowRect);

		    RECT crBorder;
            ::SetRect(
                &crBorder,
                0,
                0,
                crWindowRect.right - crWindowRect.left,
                crWindowRect.bottom - crWindowRect.top);

#ifdef XP_WIN16
            //  16 bit working in frame coordinates.
            ::MapWindowPoints(
                GetPane(),
                GetFrame()->GetFrameWnd()->m_hWnd,
                (POINT *)&crBorder,
                2);

            //  Need to shift up and left one pixel.
            crBorder.left -= 1;
            crBorder.right -= 1;
            crBorder.top -= 1;
            crBorder.bottom -= 1;
#endif

			//	Figure out what colors we are drawing with.
			COLORREF rgbOuter = m_rgbBackgroundColor;

			//	We need to change these if we're active.
			if(m_bActiveState == TRUE)	{
                int iSum =
                    (int)GetRValue(rgbOuter) +
                    (int)GetGValue(rgbOuter) +
                    (int)GetBValue(rgbOuter);

                if(iSum > (127 * 3)) {
                    //  Bright, use black.
                    rgbOuter = RGB(0, 0, 0);
                }
                else    {
                    //  Dark, use white.
                    rgbOuter = RGB(255, 255, 255);
                }
			}

            HPEN hpO = ::CreatePen(PS_SOLID, 0, rgbOuter);
			HPEN pOldPen = (HPEN)::SelectObject(hDC, hpO);

			//  Careful to avoid scrollers NC paints on NT and 3.1.
            //  We'll be detecting this by deciding how wide the window borders are,
            //      these systems report 1, but 95 reports 2 (and probably future versions
            //      of NT with a the 95 UI).
            BOOL bFullPaint = FALSE;
            if(sysInfo.m_iBorderWidth != 1) {
                bFullPaint = TRUE;
            }

            //  Draw left side.
			::MoveToEx(hDC, crBorder.left, crBorder.top, NULL);
            if(bFullPaint)  {
                ::LineTo(hDC, crBorder.left, crBorder.bottom);
            }
            else if(IsHScrollBarOn()) {
                ::LineTo(hDC, crBorder.left, crBorder.bottom - sysInfo.m_iScrollHeight);
            }
            else    {
                ::LineTo(hDC, crBorder.left, crBorder.bottom);
            }

            //  Draw bottom side.
            if(bFullPaint)  {
                ::MoveToEx(hDC, crBorder.left, crBorder.bottom - 1, NULL);
                ::LineTo(hDC, crBorder.right, crBorder.bottom - 1);
            }
            else if(!IsHScrollBarOn()) {
                ::MoveToEx(hDC, crBorder.left, crBorder.bottom - 1, NULL);

                if(IsVScrollBarOn()) {
                    ::LineTo(hDC, crBorder.right - sysInfo.m_iScrollWidth, crBorder.bottom - 1);
                }
                else    {
                    ::LineTo(hDC, crBorder.right, crBorder.bottom - 1);
                }
            }
            else if(IsHScrollBarOn() && IsVScrollBarOn())    {
                ::MoveToEx(hDC, crBorder.right - sysInfo.m_iScrollWidth + 1, crBorder.bottom - 1, NULL);
                ::LineTo(hDC, crBorder.right, crBorder.bottom - 1);
            }

            //  Draw right side.
            if(bFullPaint)  {
                ::MoveToEx(hDC, crBorder.right- 1, crBorder.top, NULL);
                ::LineTo(hDC, crBorder.right - 1, crBorder.bottom);
            }
            else if(!IsVScrollBarOn())    {
                ::MoveToEx(hDC, crBorder.right - 1, crBorder.top, NULL);

                if(IsHScrollBarOn()) {
                    ::LineTo(hDC, crBorder.right - 1, crBorder.bottom - sysInfo.m_iScrollHeight);
                }
                else    {
                    ::LineTo(hDC, crBorder.right - 1, crBorder.bottom);
                }
            }
            else if(IsVScrollBarOn() && IsHScrollBarOn()) {
                ::MoveToEx(hDC, crBorder.right - 1, crBorder.bottom - sysInfo.m_iScrollHeight + 1, NULL);
                ::LineTo(hDC, crBorder.right - 1, crBorder.bottom);
            }
            
            //  Draw top side.
            ::MoveToEx(hDC, crBorder.left, crBorder.top, NULL);
            if(bFullPaint)  {
                ::LineTo(hDC, crBorder.right, crBorder.top);
            }
            else if(IsVScrollBarOn()) {
                ::LineTo(hDC, crBorder.right - sysInfo.m_iScrollWidth, crBorder.top);
            }
            else    {
                ::LineTo(hDC, crBorder.right, crBorder.top);
            }

			::SelectObject(hDC, pOldPen);
			::DeleteObject(hpO);
			//	Done with the DC.
#ifndef XP_WIN16
            ::ReleaseDC(GetPane(), hDC);
#else
			GetFrame()->GetFrameWnd()->ReleaseDC(pDC);
#endif
		}
	}
}

//	Tells the context wether it is made active, or inactive.
//	This function mainly used to give a visual indication of the currently
//		selected frame cell.
void CWinCX::ActivateCX(BOOL bNowActive)	{	
	//	Update our active state.
	BOOL bOldState = m_bActiveState;
	m_bActiveState = bNowActive;
	
	//	In either event, see if we should update the non client area.
	if(m_bActiveState != bOldState)	{
		OnNcPaintCX();
	}
}

//	Clicking is allowed again.
//	Act as though the mouse moved, so that the correct cursor is put back up.
void CWinCX::EnableClicking(MWContext *pContext)
{
    BOOL bReturnImmediately = FALSE;
	OnMouseMoveCX(m_uMouseFlags, m_cpMMove, bReturnImmediately);
    if(bReturnImmediately)  {
        return;
    }
}

//	Function to get the view's offset into the frame's top left position.
void CWinCX::GetViewOffsetInFrame(CPoint& cpRetval)	const {
	//	Init.
	cpRetval = CPoint(0, 0);

	if(GetFrame()->GetFrameWnd() != NULL && GetPane() != NULL)	{
		CRect cr;
        ::GetWindowRect(GetPane(), cr);
		GetFrame()->GetFrameWnd()->ScreenToClient(cr);
		cpRetval = CPoint(cr.left, cr.top);
	}
}

//	Attempt to act modal over the other context.
//  This is assumming that this is being called from FE_MakeNewWindow after
//  the popup window is created as a modal dialog.
void CWinCX::GoModal(MWContext *pParent)
{
	//	See if the other context can be told to disable it's UI.
	if(pParent == NULL || ABSTRACTCX(pParent) == NULL ||
		ABSTRACTCX(pParent)->IsWindowContext() == FALSE ||
		WINCX(pParent)->GetFrame()->GetFrameWnd() == NULL)	{
		
		//	No way, no UI to deactivate.
		return;
	}

	//	Okay, we can do this.
	CWinCX *pDisableCX = WINCX(pParent);

	// Disable their window.
	// If the frame has an owned window, e.g. a modal property sheet, disable that
	HWND	popup = ::GetLastActivePopup(pDisableCX->GetFrame()->GetFrameWnd()->m_hWnd);
	HWND popupOwner = NULL;
	// Because of the order in which this takes place, the modal dialog has already been added
	// in FE_MakeNewWindow, therefore it will be the last active popup. So we need to get its
	// parent.
	if(popup)
		popupOwner = ::GetParent(popup);
	if(popupOwner)
		::EnableWindow(popupOwner, FALSE);
	// The above call will also disable the popup since it is a child of popupOwner.  Therefore
	// we need to enable the popup.
	::EnableWindow(popup, TRUE);

	//	Mark that we need to re enable the other context when we're destroyed.
	//	We support modality over any number of context's (in case we need to
	//		go modal over the application, this allows it).
	if(popupOwner)
		m_cplModalOver.AddTail((void *)popupOwner);
}

//	Register a function to be called when the DestroyContext is called.
//	We only support one at a time, so a list implementation will have to
//		be provided if more than one is needed.
void CWinCX::CloseCallback(void (*pFunc)(void *), void *pArg)
{
	//	Just assign them in.
    if(pFunc)   {
	    m_cplCloseCallbacks.AddTail((void *)pFunc);
	    m_cplCloseCallbackArgs.AddTail(pArg);
    }
}

//	This simply tells the frame if it should do anything special in it's min max handler.
//	If you really need each context to not resize (such as in Frames), then you'll have to
//		redisign this.
void CWinCX::EnableResize(BOOL bEnable)
{
	//	Must have a frame, a CGenericFrame.
	if(GetFrame()->GetFrameWnd() != NULL &&
		GetFrame()->GetFrameWnd()->IsKindOf(RUNTIME_CLASS(CGenericFrame)))	{
		CGenericFrame *pFrame = (CGenericFrame *)GetFrame()->GetFrameWnd();
		pFrame->EnableResize(bEnable);
	}
}

//	Don't allow the frame to close.
//	This can't really work on a view by view basis (frames).
//	Also, this assumes the CWinCX resides in a CFrameWnd, which will
//		before handling close call CanCloseDocument on the document.
void CWinCX::EnableClose(BOOL bEnable)
{
	//	Get our document, and tell it wether or not it will allow the frame to close.
	if(GetDocument())	{
		GetDocument()->EnableClose(bEnable);
	}
}

void CWinCX::DisableHotkeys(BOOL bDisable)
{
	//	Must have a frame, a CGenericFrame.
	if(GetFrame()->GetFrameWnd() != NULL &&
		GetFrame()->GetFrameWnd()->IsKindOf(RUNTIME_CLASS(CGenericFrame)))	{
		CGenericFrame *pFrame = (CGenericFrame *)GetFrame()->GetFrameWnd();
		pFrame->DisableHotkeys(bDisable);
	}
}

//  Do not allow window to change z-ordering
void CWinCX::SetZOrder(BOOL bZLock, BOOL bBottommost)
{
	//	Must have a frame, a CGenericFrame.
	if(GetFrame()->GetFrameWnd() != NULL &&
		GetFrame()->GetFrameWnd()->IsKindOf(RUNTIME_CLASS(CGenericFrame)))	{
		CGenericFrame *pFrame = (CGenericFrame *)GetFrame()->GetFrameWnd();
		pFrame->SetZOrder(bZLock, bBottommost);
	}
}

//	Determine wether or not we can upload files.
BOOL CWinCX::CanUploadFile()
{
	BOOL bRetval = TRUE;

	if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else if(GetPane() == NULL)	{
		bRetval = FALSE;
	}
	else if(CanCreateUrlFromHist() == FALSE)	{
		bRetval = FALSE;
	}
	else	{
		//	Must check to make sure we're on the correct type of FTP
		//		directory.
		History_entry *pHistoryEntry = SHIST_GetCurrent(&(GetContext()->hist));
		if(pHistoryEntry == NULL || pHistoryEntry->address == NULL)	{
			bRetval = FALSE;
		}
		else if(strnicmp(pHistoryEntry->address, "ftp://", 6))	{
			bRetval = FALSE;
		}
		else if(pHistoryEntry->address[strlen(pHistoryEntry->address) - 1] != '/')	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

//	Upload files.
void CWinCX::UploadFile()
{
	if(CanUploadFile() == FALSE)	{
		return;
	}

	//	Get the files from the user.
    char *pFileName = wfe_GetExistingFileName(GetPane(), szLoadString(IDS_FILE_UPLOAD), HTM, TRUE);

	//	Null if user hit cancel.
	if(pFileName != NULL)	{
		URL_Struct *pUrl = CreateUrlFromHist(TRUE);
		if(pUrl != NULL)	{
			//	Set this to super reload (per montulli).
			pUrl->force_reload = NET_SUPER_RELOAD;

			size_t stAlloc = 2 * sizeof(char *);
			pUrl->files_to_post = (char **)XP_ALLOC(stAlloc);
			if(pUrl->files_to_post != NULL)	{
				//	Clear the array.
				memset(pUrl->files_to_post, 0, stAlloc);

				//	Put in the name.
				//	It's already been allocated by the prompt routine, no need to free
				//		it in this case.
				pUrl->files_to_post[0] = pFileName;

				//	Ask for it.
				GetUrl(pUrl, FO_CACHE_AND_PRESENT);
			}
			else	{
				NET_FreeURLStruct(pUrl);
			}
		}
	}
}

typedef struct mouse_over_closure{
    int32     xVal, yVal;
    BOOL      bDeleteLO_Element;  // in case we created a fake one
    CWinCX  * pWin;
} mouse_over_closure;

//
// The backend just had a chance at a mouse-over event.  If it
//   didn't set the status bar text take care of setting it now
//
static void
mouse_over_callback(MWContext * context, LO_Element * lo_element, int32 event,
                     void * pObj, ETEventStatus status)
{
    // keep track of what we have done already so that we
    //   don't thrash
    BOOL bTextSet = FALSE;
    BOOL bCursorSet = FALSE;
    LO_EmbedStruct *pEmbed = NULL;
    LO_ImageStruct * image_struct = NULL;
    LO_TextStruct * text_struct = NULL;

    // get our stashed data
    mouse_over_closure * pClose = (mouse_over_closure *) pObj;
    CWinCX * pWin = pClose->pWin;
    
    // make sure the document didn't disappear
    if(status == EVENT_PANIC) {
        XP_FREE(pClose);
        return;
    }

    // if the status is OK then that means the backend set the
    //   status bar text
    if(status == EVENT_OK)
        bTextSet = TRUE;
    
    CPoint DocPoint(CASTINT(pClose->xVal), CASTINT(pClose->yVal));
    BOOL bMouseInSelection = pClose->pWin->PtInSelectedRegion( DocPoint );

#ifdef EDITOR
    // First check for Table mouse-over events - we don't need the current element
    //  since we will look for closest table hit region
    // Don't do this if ALT key is pressed - this allows sizing images (and other objects)
    //   which are tightly surrounded by a cell boundary.
    //  TODO: Alt key leaves menu select mode on - how do we defeat that behavior?

    if( EDT_IS_EDITOR(context) && !EDT_IsSizing(context) )
    {
        ED_HitType iTableHit = ED_HIT_NONE;
        if( pWin->m_bSelectingCells )
        {
            // User is dragging mouse to select table cells
            // Return value tells us what kind of cursor to use
            iTableHit = EDT_ExtendTableCellSelection(context, pClose->xVal, pClose->yVal);
        }
        else if( GetAsyncKeyState(VK_MENU) >= 0 ) // Alt key test
        {
            // See if we are over a table or cell "hit" region and need a special cursor
            // Get hit region - we don't need to know element, so use NULL for 4th param
            // If 5th param is TRUE, then select all cells if in upper left corner of table,
            //   or select/unselect cell if mouse is ANYWHERE inside of the cell
            iTableHit = EDT_GetTableHitRegion(context, pClose->xVal, pClose->yVal, NULL, GetAsyncKeyState(VK_CONTROL));
        }
        if( iTableHit )
        {
            bCursorSet = TRUE;
            switch( iTableHit )
            {
                case ED_HIT_SEL_TABLE:      // Upper left corner
                    SetCursor(theApp.LoadCursor(IDC_TABLE_SEL));
                    break;
                case ED_HIT_SEL_ALL_CELLS:  // Upper left corner with Ctrl pressed
                    SetCursor(theApp.LoadCursor(IDC_ALL_CELLS_SEL));
                    break;
                case ED_HIT_SEL_COL:        // Near Top table border
                    SetCursor(theApp.LoadCursor(IDC_COL_SEL));
                    break;
                case ED_HIT_SEL_ROW:        // Near left table border
                    SetCursor(theApp.LoadCursor(IDC_ROW_SEL));
                    break;
                case ED_HIT_SEL_CELL:       // Not sure - remaining cell border not matching other regions
                    SetCursor(theApp.LoadCursor(IDC_CELL_SEL));
                    break;
                case ED_HIT_SIZE_TABLE_WIDTH:     // Right edge of table
                    SetCursor(theApp.LoadCursor(IDC_TABLE_SIZE));
                    break;
                case ED_HIT_SIZE_TABLE_HEIGHT:    // Bottom edge of table
                    SetCursor(theApp.LoadCursor(IDC_TABLE_VSIZE));
                    break;
                case ED_HIT_SIZE_COL:       // Right border of a cell
                    SetCursor(theApp.LoadCursor(IDC_COL_SIZE));
                    break;
                case ED_HIT_SIZE_ROW:       // Right border of a cell
                    SetCursor(theApp.LoadCursor(IDC_ROW_SIZE));
                    break;
                case ED_HIT_ADD_ROWS:       // Lower left corner
                    SetCursor(theApp.LoadCursor(IDC_ADD_ROWS));
                    break;
                case ED_HIT_ADD_COLS:       // Lower right corner
                    SetCursor(theApp.LoadCursor(IDC_ADD_COLS));
                    break;
                case ED_HIT_DRAG_TABLE: // Near bottom
                    SetCursor(theApp.LoadCursor(IDC_ARROW_HAND));
                    break;
            }
            goto FINISH_MOUSE_OVER;
        }
    }
#endif

    // See if we are over some text that is a link
    text_struct = (LO_TextStruct *) lo_element;

    // Imagemaps send their mouseover events as dummy text structs.  Be very careful
    // about what you try to use from this text struct as many of the fields are
    // invalid.
    if(text_struct && text_struct->type == LO_TEXT && text_struct->text)
    {
        BOOL bIsLink = (text_struct->anchor_href && text_struct->anchor_href->anchor);
        if(bIsLink && !bTextSet)
        {
            wfe_Progress(context, (char *) text_struct->anchor_href->anchor);
            bTextSet = TRUE;
        }
        if( EDT_IS_EDITOR(context) )
        {
#ifdef EDITOR
            if( EDT_CanPasteStyle(context) )
            {
                SetCursor(theApp.LoadCursor(IDC_COPY_STYLE));
                bCursorSet = TRUE;
            }
            else if( bMouseInSelection )
            {
                // Indicate that user can drag selected text with special cursor
                SetCursor(theApp.LoadCursor(IDC_IBEAM_HAND));
                bCursorSet = TRUE;
            }
#else
            XP_ASSERT(0);
#endif 
        } else if( bIsLink )
        {
            // Set anchor cursor only if not in editor
            SetCursor(theApp.LoadCursor(IDC_SELECTANCHOR));
            bCursorSet = TRUE;
        }
        if( !bCursorSet )
        {
            // We have 2 text cursor sizes -- more visible for large fonts
            SetCursor( theApp.LoadCursor(
                            text_struct->height > 20 ? IDC_EDIT_IBEAM_LARGE : IDC_EDIT_IBEAM) );
            bCursorSet = TRUE;
        }
    } 
#ifdef EDITOR
    else if( EDT_IS_EDITOR(context) &&
               lo_element && 
               !EDT_IsSizing(context) )
    {

        // Test for proximity to border of sizeable objects
        // This sets the cursor to system sizing cursor
        // Note: X, Y are in View coordinates

        int sizing = EDT_CanSizeObject(context, lo_element, pClose->xVal, pClose->yVal);
        if( sizing )
        {
            switch ( sizing )
            {
                case ED_SIZE_TOP:
                case ED_SIZE_BOTTOM:
                    SetCursor(theApp.LoadStandardCursor(IDC_SIZENS));
                    break;
                case ED_SIZE_RIGHT:
                    // Tables and cells can only size from the right border
                    if( lo_element->type == LO_TABLE )
                    {
                        SetCursor(theApp.LoadCursor(IDC_TABLE_SIZE));
                        break;
                    }
                    if( lo_element->type == LO_CELL )
                    {
                        SetCursor(theApp.LoadCursor(IDC_COL_SIZE));
                        break;
                    }
                    // If not Table or Cell, fall through 
                    //  to set horizontal sizing cursor
                case ED_SIZE_LEFT:
                    SetCursor(theApp.LoadStandardCursor(IDC_SIZEWE));
                    break;
                case ED_SIZE_TOP_RIGHT:
                case ED_SIZE_BOTTOM_LEFT:
                    SetCursor(theApp.LoadStandardCursor(IDC_SIZENESW));
                    break;
                case ED_SIZE_TOP_LEFT:
                case ED_SIZE_BOTTOM_RIGHT:
                    SetCursor(theApp.LoadStandardCursor(IDC_SIZENWSE));
                    break;
            }
            bCursorSet = TRUE;
        }
    }
#endif // EDITOR
    // NOTE: We set the imagemap and status line stuff even if we set a "sizing" cursor

    // See if we are over an image that is a link
    image_struct = (LO_ImageStruct *) lo_element;
    if(image_struct && image_struct->type == LO_IMAGE && image_struct->image_attr)	{
		if(image_struct->image_attr->usemap_name != NULL)	{
			CPoint image_offset;
			image_offset.x = CASTINT(pClose->xVal - (image_struct->x + image_struct->x_offset + image_struct->border_width));
			image_offset.y = CASTINT(pClose->yVal - (image_struct->y + image_struct->y_offset + image_struct->border_width));

			LO_AnchorData *pAnchorData = LO_MapXYToAreaAnchor(pWin->GetDocumentContext(), image_struct,
				image_offset.x, image_offset.y);
			if(pAnchorData != NULL)	{
                if( !EDT_IS_EDITOR(context) && !bMouseInSelection ){
                    SetCursor(theApp.LoadCursor(IDC_SELECTANCHOR));
                    bCursorSet = TRUE;
                }
		        if( pAnchorData->anchor && !bTextSet ) {
		            wfe_Progress(context, (char *) pAnchorData->anchor);
                    bTextSet = TRUE;
                }
			}
		}
        else if((image_struct->image_attr->attrmask & LO_ATTR_ISMAP) && image_struct->anchor_href) {
            // if its an ismap print the coordinates too
            char buf[32];
            CPoint point;

            point.x = (int) (pClose->xVal - (image_struct->x + image_struct->x_offset));
            point.y = (int) (pClose->yVal - (image_struct->y + image_struct->y_offset));

            sprintf(buf, "?%d,%d", point.x, point.y);

            CString anchor;
            anchor = (char *) image_struct->anchor_href->anchor;
            anchor += buf;

            if( !EDT_IS_EDITOR(context) && !bMouseInSelection ){
                SetCursor(theApp.LoadCursor(IDC_SELECTANCHOR));
                bCursorSet = TRUE;
            }
            //  Set progress text if mocha allows it.
            if(!bTextSet) {
                wfe_Progress(context, (const char *)anchor);
                bTextSet = TRUE;
            }
        }
        else if(image_struct->anchor_href) {
            // Set anchor cursor only if not in editor and not in a selection
            if( !EDT_IS_EDITOR(context) && !bMouseInSelection ) {
    	        SetCursor(theApp.LoadCursor(IDC_SELECTANCHOR));
                bCursorSet = TRUE;
            }
	        if (image_struct->anchor_href->anchor && !bTextSet) {
    	        wfe_Progress(context, (char *) image_struct->anchor_href->anchor);
    	        bTextSet = TRUE;
	        }
		}
        else if(image_struct->image_attr->attrmask & LO_ATTR_INTERNAL_IMAGE 
                && EDT_IS_EDITOR(context) 
                && image_struct->alt != NULL ){

            char *str;
            if(!bTextSet)   {
                PA_LOCK(str, char *, image_struct->alt);
                if( str != 0 ){
                    wfe_Progress( context, str );
                    bTextSet = TRUE;
                }
                PA_UNLOCK(image_struct->alt);
            }
        }
        if( EDT_IS_EDITOR(context) && !bCursorSet ){
            // Experimental: Image can  be dragged, so if no cursor set above,
            // use Open Hand with arrow to indicate "draggability" of the image
            SetCursor(theApp.LoadCursor(IDC_ARROW_HAND));
            bCursorSet = TRUE;
        }
    }

    //  See if we are over an embedded item.
    pEmbed = (LO_EmbedStruct *)lo_element;
    if(pEmbed && pEmbed->type == LO_EMBED && pEmbed->FE_Data)  {
		NPEmbeddedApp *pPluginShim = (NPEmbeddedApp *)pEmbed->FE_Data;
		if(pPluginShim != NULL && wfe_IsTypePlugin(pPluginShim) == FALSE)	{
	        CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pPluginShim->fe_data;
	        if(pItem != NULL && pItem->m_bLoading == FALSE && pItem->m_bBroken == FALSE &&
	            pItem->m_bDelayed == FALSE && pItem->m_lpObject != NULL)   
	        {
	            if( !bMouseInSelection ){
                    SetCursor(theApp.LoadCursor(IDC_ACTIVATE_EMBED));
	                bCursorSet = TRUE;
                }

                if(!bTextSet)   {
	                CString csEmbed;
	                csEmbed.LoadString(IDS_ACTIVATE_EMBED_STATUS);
	                CString csDescrip;
	                pItem->GetUserType(USERCLASSTYPE_FULL, csDescrip);
	                csEmbed += csDescrip;

	                wfe_Progress(context, (char *)(const char *)csEmbed);
	                bTextSet = TRUE;
                }
	        }
		}
    }

FINISH_MOUSE_OVER:
    // If nothing set yet blank it out and make sure we have the 
    //   normal cursor.  Do we really want to reset the cursor 
    //   here????
    // We want to show link text but keep edit cursor in Editor
    if(!bCursorSet){
        SetCursor(theApp.LoadStandardCursor(IDC_ARROW));
    }
    if(!bTextSet) {
        wfe_Progress(context, "");
        bTextSet = TRUE;
    }

    // For mouse overs on anchors we may have created a fake
    //   LO_Element structure.  If so, blow it away now
    if(pClose->bDeleteLO_Element)
        XP_FREE(lo_element);

    XP_FREE(pClose);

    //  Have the mouse timer handler do some dirty work.
    //  Please don't return in the above code, I'd like this to get called
    //      in all cases with the state of the buttons set correctly.
    MouseTimerData mt(context);
    FEU_MouseTimer(&mt);

}

//  Function to handle the details of the cursor being over an element
//    and telling the back end about it.  If the backend doesn't
//    set the text, the text will get set in our clallback routine 
void CWinCX::FireMouseOverEvent(LO_Element *pElement, int32 xVal, int32 yVal,
                                CL_Layer *layer)
{
    mouse_over_closure * pClose = NULL;
    LO_Element * pDummy;
    BOOL bEventSent = FALSE;

#define CREATE_CLOSURE() { pClose = XP_NEW_ZAP(mouse_over_closure);  \
                           pClose->xVal = xVal; pClose->yVal = yVal; \
                           pClose->pWin = this; }

    //  The mouse is over nothing, fire an out message for the last element.
    if(pElement == NULL)    {
        FireMouseOutEvent(TRUE, TRUE, xVal, yVal, layer);
    }
    //  If a different element, fire an out message for the last element,
    //      and then fire an over message for the new element.
    else if(pElement != m_pLastOverElement) {
        FireMouseOutEvent(TRUE, TRUE, xVal, yVal, layer);

        m_pLastOverElement = pElement;
        m_pLastOverAnchorData = GetAreaAnchorData(m_pLastOverElement);

	    // JS needs screen coords for click events.
        CPoint cpScreenPoint(xVal, yVal);
	    ClientToScreen(GetPane(), &cpScreenPoint);

        //  Fire over messages.
        if(m_pLastOverAnchorData)
        {
            CREATE_CLOSURE();
            // need to make a fake wrapper for m_pLastOverAnchorData
            pDummy = (LO_Element *) XP_NEW_ZAP(LO_Element);
            pDummy->lo_text.type = LO_TEXT;
            pDummy->lo_text.anchor_href = m_pLastOverAnchorData;
            // We use the text of the element to determine if it is still
            // valid later so give the dummy text struct's text a value.
            if (pDummy->lo_text.anchor_href->anchor)
                pDummy->lo_text.text = pDummy->lo_text.anchor_href->anchor;

            //   need to free fake wrapper for m_pLastOverAnchorData
            pClose->bDeleteLO_Element = TRUE;

            // its safe to send the event now
            JSEvent *event;
            event = XP_NEW_ZAP(JSEvent);
            event->type = EVENT_MOUSEOVER;
            event->x = xVal;
            event->y = yVal;
            event->docx = xVal + CL_GetLayerXOrigin(layer);
            event->docy = yVal + CL_GetLayerYOrigin(layer);
            event->screenx = cpScreenPoint.x;
            event->screeny = cpScreenPoint.y;

            event->layer_id = LO_GetIdFromLayer(GetContext(), layer);

            ET_SendEvent(GetContext(), pDummy, event,
                         mouse_over_callback, pClose);
            bEventSent = TRUE;
        }
        else if(m_pLastOverElement && (m_pLastOverElement->type == LO_IMAGE || 
				m_pLastOverElement->type == LO_FORM_ELE || 
				(m_pLastOverElement->type == LO_TEXT 
				&& m_pLastOverElement->lo_text.anchor_href) 
#ifdef DOM
				|| LO_IsWithinSpan(m_pLastOverElement)
#endif
				))
        {
            CREATE_CLOSURE();
            JSEvent *event;
            event = XP_NEW_ZAP(JSEvent);
            event->type = EVENT_MOUSEOVER;
            event->x = xVal;
            event->y = yVal;
            event->docx = xVal + CL_GetLayerXOrigin(layer);
            event->docy = yVal + CL_GetLayerYOrigin(layer);
            event->screenx = cpScreenPoint.x;
            event->screeny = cpScreenPoint.y;
            event->layer_id = LO_GetIdFromLayer(GetContext(), layer);

            ET_SendEvent(GetContext(), m_pLastOverElement, event,
                         mouse_over_callback, pClose);
            bEventSent = TRUE;
        } 
    }
    //  If the same element, they have possibly traversed into a new
    //      AnchorData AREA, fire an out message, and then fire an
    //      over message if so.
    //  DO NOT send redundant over messages.
    else if(pElement == m_pLastOverElement)   {
        LO_AnchorData *pAnchorData = GetAreaAnchorData(pElement);
        if(pAnchorData != m_pLastOverAnchorData)    {
            if(m_pLastOverAnchorData)   {
                //  Only do this if there is last anchor data, or we
                //      fire mouse out on the element and not the data.
                FireMouseOutEvent(FALSE, TRUE, xVal, yVal, layer);
            }
            m_pLastOverAnchorData = pAnchorData;
            if(m_pLastOverAnchorData)   {
                // JS needs screen coords for click events.
                CPoint cpScreenPoint(xVal, yVal);
                ClientToScreen(GetPane(), &cpScreenPoint);

                CREATE_CLOSURE();
                // need to make a fake wrapper for m_pLastOverAnchorData
                pDummy = (LO_Element *) XP_NEW_ZAP(LO_Element);
                pDummy->lo_text.type = LO_TEXT;
                pDummy->lo_text.anchor_href = m_pLastOverAnchorData;

                // We use the text of the element to determine if it is still
                // valid later so give the dummy text struct's text a value.
                if (pDummy->lo_text.anchor_href->anchor)
	                pDummy->lo_text.text = pDummy->lo_text.anchor_href->anchor;

                //   need to free fake wrapper for m_pLastOverAnchorData
                pClose->bDeleteLO_Element = TRUE;

                // its safe to send the event now
                JSEvent *event;
                event = XP_NEW_ZAP(JSEvent);
                event->type = EVENT_MOUSEOVER;
                event->x = xVal;
                event->y = yVal;
                event->docx = xVal + CL_GetLayerXOrigin(layer);
                event->docy = yVal + CL_GetLayerYOrigin(layer);
                event->screenx = cpScreenPoint.x;
                event->screeny = cpScreenPoint.y;
                event->layer_id = LO_GetIdFromLayer(GetContext(), layer);
	                
                ET_SendEvent(GetContext(), pDummy, event,
                             mouse_over_callback, pClose);
                bEventSent = TRUE;
            }
        }
        // If this is an ismap and we've moved within it, we need to 
        // change status without firing off a mouse over.
        else if(pElement->lo_any.type == LO_IMAGE) {
            LO_ImageStruct *image_struct = (LO_ImageStruct *)pElement;
            if((image_struct->image_attr->usemap_name == NULL) && (image_struct->image_attr->attrmask & LO_ATTR_ISMAP) && image_struct->anchor_href) {
                // if its an ismap print the coordinates too
                char buf[32];
                CPoint point;

                point.x = (int) (xVal - (image_struct->x + image_struct->x_offset));
                point.y = (int) (yVal - (image_struct->y + image_struct->y_offset));

                sprintf(buf, "?%d,%d", point.x, point.y);
                
                CString anchor;
                anchor = (char *) image_struct->anchor_href->anchor;
                anchor += buf;

                if (!EDT_IS_EDITOR(GetContext())) {
                    SetCursor(theApp.LoadCursor(IDC_SELECTANCHOR));
                }

                //  Set progress text
                wfe_Progress(GetContext(), (const char *)anchor);
            }
	    if (!EDT_IS_EDITOR(GetContext()))
            bEventSent = TRUE;
        }
	// Editor wants callback to be callled every time, the Browser does not.  If 
	// we're over the same element it may have set the status bar message when we 
	// entered it.  Don't let the Browser set the status bar again until we leave it.  
	// If the element didn't want to set the status bar, the Browser will have already
    // set it correctly anyway.
	else if (!EDT_IS_EDITOR(GetContext()))
	    bEventSent = TRUE;
    }

    // In the Editor, ALWAYS call the final callback
    //   so we can monitor mouse location for dynamic resizing of elements
    if( !bEventSent ) {
        CREATE_CLOSURE();
        // Note: param 3 ("event") isn't used in mouse_over_callback
        mouse_over_callback(GetContext(), pElement, 0, (void*)pClose, EVENT_CANCEL);
    }


    // We're using a timer to deal with the lack of a Windows call for entrance
    // and exit for a window.  Deal with it here.

    if (!m_bMouseMoveTimerSet) {
        // This is our first time into the document or our last.  Start the timer to check
        // for our exit and fire a mouseover for the window.
        POINT mp;
        // get mouse position
        ::GetCursorPos(&mp);

        if (::WindowFromPoint(mp) == GetPane()) {
	        MouseMoveTimerData mt(GetContext());
	        FEU_MouseMoveTimer(&mt);
        }
    }

    /* Unix and Mac are not up to spec on these and won't be for 4.0  Commenting
     * them out for future use later */

	/*JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_MOUSEOVER;
	event->x = xVal;
	event->y = yVal;
	event->layer_id = LO_DOCUMENT_LAYER_ID;

	ET_SendEvent(GetContext(), 0, event, 0, 0);
    }
    else {
        // We may have left the document. If so, send a mouseout to the window.
        POINT mp;
        // get mouse position
        ::GetCursorPos(&mp);

        if ((::WindowFromPoint(mp) != GetPane()) && !m_bLBDown) {
	        //Yup, we left.
	        JSEvent *event;
	        event = XP_NEW_ZAP(JSEvent);
	        event->type = EVENT_MOUSEOUT;
	        event->x = xVal;
	        event->y = yVal;
	        event->layer_id = LO_DOCUMENT_LAYER_ID;

	        ET_SendEvent(GetContext(), 0, event, 0, 0);
        }
    }    */
    return;

#undef CREATE_CLOSURE
}

//  Retrieve anchor data out of areas only (usemaps)
//  Use last known mouse move coordinates to do so.
LO_AnchorData *CWinCX::GetAreaAnchorData(LO_Element *pElement)  {
    LO_AnchorData *pRetval = NULL;

    //  Make sure this is an image element.
    if(pElement && pElement->lo_any.type == LO_IMAGE)   {
        //  Determine coordinates in pixels with image as origin and
        //      ask layout for area anchor data, will return NULL if none.
		LTRB Rect;
        LO_ImageStruct *pLOImage = (LO_ImageStruct *)pElement;

        // Use the returned value to convert the mouse coordinates
        // to be image relative.
		ResolveElement(Rect, pLOImage->x, pLOImage->y, 
                               pLOImage->x_offset + pLOImage->border_width, 
                               pLOImage->y_offset + pLOImage->border_width, 
                               0, 0);
        if (pLOImage->layer) {
            CL_Layer *parent;
            int32 x_offset, y_offset;

            parent = CL_GetLayerParent(pLOImage->layer);

            if (parent) {
                x_offset = CL_GetLayerXOrigin(parent);
                y_offset = CL_GetLayerYOrigin(parent);

                Rect.left += x_offset;
                Rect.right += x_offset;
                Rect.top += y_offset;
                Rect.bottom += y_offset;
            }
        }

        pRetval = LO_MapXYToAreaAnchor(GetDocumentContext(), pLOImage,
            m_cpMMove.x - Rect.left, m_cpMMove.y - Rect.top);
    }

    return(pRetval);
}

static void
free_this_callback(MWContext * context, LO_Element * lo_element, int32 event,
                  void * pObj, ETEventStatus status)
{
    XP_FREE(pObj);
}

//  Fire mouse out events for cached element types.
void CWinCX::FireMouseOutEvent(BOOL bClearElement, BOOL bClearAnchor, int32 xVal,
			       int32 yVal, CL_Layer *layer)
{
    // JS needs screen coords for click events.
    CPoint cpScreenPoint(xVal, yVal);
    ClientToScreen(GetPane(), &cpScreenPoint);

    //  Determine if we're sending an anchor out or an element out.
    if(m_pLastOverAnchorData)
    {
        //  Anchor data.
        //  Create a fake holder element here since we didn't save enough
        //    information and Win16 won't let us pass stack variables
        LO_Element * element = XP_NEW_ZAP(LO_Element);
        element->lo_text.type = LO_TEXT;
        element->lo_text.anchor_href = m_pLastOverAnchorData;

        // We use the text of the element to determine if it is still
        // valid later so give the dummy text struct's text a value.
        if (element->lo_text.anchor_href->anchor)
            element->lo_text.text = element->lo_text.anchor_href->anchor;

        JSEvent *event;
        event = XP_NEW_ZAP(JSEvent);
        event->type = EVENT_MOUSEOUT;
        event->x = xVal;
        event->y = yVal;
        event->docx = xVal + CL_GetLayerXOrigin(layer);
        event->docy = yVal + CL_GetLayerYOrigin(layer);
        event->screenx = cpScreenPoint.x;
        event->screeny = cpScreenPoint.y;

        event->layer_id = LO_GetIdFromLayer(GetContext(), layer);
	        
        ET_SendEvent(GetContext(), element, event,
                     free_this_callback, element);
    }
    else if(m_pLastOverElement && (m_pLastOverElement->type == LO_IMAGE
			|| m_pLastOverElement->type == LO_FORM_ELE ||	    
			(m_pLastOverElement->type == LO_TEXT && 
			m_pLastOverElement->lo_text.anchor_href) 
#ifdef DOM
			|| LO_IsWithinSpan(m_pLastOverElement)
#endif
			))
    {
        //  Element.
        JSEvent *event;
        event = XP_NEW_ZAP(JSEvent);
        event->type = EVENT_MOUSEOUT;
        event->x = xVal;
        event->y = yVal;
        event->docx = xVal + CL_GetLayerXOrigin(layer);
        event->docy = yVal + CL_GetLayerYOrigin(layer);
        event->screenx = cpScreenPoint.x;
        event->screeny = cpScreenPoint.y;

        event->layer_id = LO_GetIdFromLayer(GetContext(), layer);
     
        ET_SendEvent(GetContext(), m_pLastOverElement, event,
                     NULL, this);
    }

    //
    // Mocha may have trashed the docuemnt, but it will still be safe
    //   to set the following elements to NULL.  Mocha can't have
    //   destroyed our context yet
    //

    if(bClearElement)   {
        m_pLastOverElement = NULL;
    }
    if(bClearAnchor)    {
        m_pLastOverAnchorData = NULL;
    }

    //  Clean up, these can now be empty.
    m_bLastOverTextSet = FALSE;
}

CFrameGlue *CWinCX::GetFrame() const
{
	//	If we're a grid cell, return the
	//		parent's frame (one in the same)
	//	This allows those scary times when we're
	//		a grid in an OLE server to work better
	//		when the frame's switch.
	MWContext *pParent = GetParentContext();
	if(IsGridCell() && pParent)	{
		if(ABSTRACTCX(pParent)->IsFrameContext())	{
			return(WINCX(pParent)->GetFrame());
		}
	}

	//	Make sure our frame isn't NULL.
	//	If so, go with the NULL frame.
	//	Could happen if the frame is destroyed, but the
	//		context lives.
	if(m_pFrame == NULL)	{
		return(m_pNullFrame);
	}
	return m_pFrame;
}

void CWinCX::ClearView()
{
	//	Tell the view and frame that we're no more.
	if(m_pGenView != NULL)	{
		//	Also, if we're selecting text, clear our capture of the mouse.
		if(m_bLBDown == TRUE)	{
			::ReleaseCapture();
		}
        m_pGenView->ClearContext();
        m_pGenView = NULL;
	}
}

BOOL CWinCX::IsClickingEnabled() const
{
#ifdef EDITOR
	return(GetContext()->waitingMode == FALSE && GetContext()->edit_saving_url == FALSE);
#else
	return(GetContext()->waitingMode == FALSE);
#endif
}

//#ifndef NO_TAB_NAVIGATION 
LO_TabFocusData * CWinCX::getLastTabFocusData()
{
	return( &m_lastTabFocus );
}

LO_Element * CWinCX::getLastTabFocusElement()
{
	return( m_lastTabFocus.pElement );
}

int32 CWinCX::getLastFocusAreaIndex()
{
	return( m_lastTabFocus.mapAreaIndex ); 	// 0 means no focus, start with index 1.
}

LO_AnchorData	*CWinCX::getLastFocusAnchorPointer()
{
	return( m_lastTabFocus.pAnchor );
}

char *CWinCX::getLastFocusAnchorStr()
{
	if( m_lastTabFocus.pAnchor == NULL)
		return( NULL );

	return( (char *)m_lastTabFocus.pAnchor->anchor );
}

// When an element gets or losts Tab Focus, we need to invalidate it and hence to redraw it.
// check if(GetPane() && CanBlockDisplay() ) before calling invalidateElement().
void CWinCX::invalidateElement( LO_Element *pElement )
{
	if( pElement == NULL )
		return;

	// find out which element needs refresh.
    LTRB			Rect;
	BOOL			rectEmpty;
	LO_ImageStruct	*pImage;
	LO_Any			*pp = (LO_Any *)pElement;
	
	rectEmpty	= TRUE;
	
	switch( pElement->lo_any.type ) {
	case LO_FORM_ELE :
		if( LO_isFormElementNeedTextTabFocus( (LO_FormElementStruct *)pElement) ) {
			// for Form element type FORM_TYPE_RADIO and FORM_TYPE_CHECKBOX,
			// it is the next Text element, with dotted box, needs refresh.
			pp = (LO_Any *)pElement->lo_any.next;	
		}
		// for other Form element type( button, text input...), the widget handles the focus.
		if (pp)
		{
			ResolveElement(Rect, pp->x, pp->y, pp->x_offset,pp->y_offset, pp->width, pp->height);
			rectEmpty = FALSE;
		}
		break;
	case LO_TEXT :
		// for a text(link), the element itself has a dotted box as focus.
		ResolveElement(Rect, pp->x, pp->y, pp->x_offset,pp->y_offset, pp->width, pp->height);
		rectEmpty = FALSE;
		break;
	case LO_IMAGE :
		// TODO only invalidate the area, not the whole map image.
		pImage = &pElement->lo_image;
		ResolveElement(Rect, IL_GetImagePixmap(pImage->image_req), 
			           pImage->x_offset + pImage->border_width,
					   pImage->y_offset + pImage->border_width, 
					   pImage->x, pImage->y, pImage->width, pImage->height);

		rectEmpty = FALSE;
		break;
	default :
		rectEmpty = TRUE;
		break;
	}

	if( rectEmpty )
		return;

	// no background erase
	::InvalidateRect(GetPane(), CRect(CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom)), FALSE);

}

void CWinCX::SetActiveWindow()
{
	// Set my view to be the active view, so I will get keyboard input.
	// SetFocus cannot get keyboard, and it would grab focus from
	// Form elements. They need focus to process SpaceBar etc.
	CFrameGlue * pFrame = GetFrame();
	if( pFrame ) {
		CFrameWnd * pFrameWindow = pFrame->GetFrameWnd();
		if( pFrameWindow && pFrameWindow->IsKindOf(RUNTIME_CLASS(CGenericFrame)))	{
			CGenericView *pView = GetView();
			if( pView ) 	
				pFrameWindow->SetActiveView( (CView *)pView );
		}
	} // else something wrong.

}

// this function is trigered by clicking in a form element, or link,
// the form element has set focus to itself. we need to clear old focus only.
// for clicking on link, we don't need to call Windows' setFocus().
void CWinCX::setFormElementTabFocus( LO_Element * pFormElement )
{
	LO_TabFocusData	newTabFocus;	

	if(pFormElement == getLastTabFocusElement() )  // clicking on the focused element.
		return;                                      

	newTabFocus.pElement		= pFormElement;
	MWContext *pContext = GetContext();

	// for clicking area.
	newTabFocus.mapAreaIndex	= 0;		// 0 means no area.
	newTabFocus.pAnchor			= NULL;
	
	// double check tab-able, and fill in pAnchor
	if( LO_isTabableElement(pContext, &newTabFocus ) ) {
		setLastTabFocusElement( &newTabFocus, 0 );  // clicked, 0 means don't needSetFocus
		SetMainFrmTabFocusFlag(CMainFrame::TAB_FOCUS_IN_GRID);  // I have tab focus.
	}

}

// text element may be fragmented in multiple lines, for Tab_focus, they
// need to be treated as one tab stop.
int CWinCX::invalidateSegmentedTextElement(LO_TabFocusData *pNextTabFocus, int forward )
{
	int		count = 0;
	LO_Element *pElement;
	pElement = pNextTabFocus->pElement;
	while(	pElement && 
			(pElement->lo_any.type == LO_TEXT || pElement->lo_any.type == LO_LINEFEED) ) {
														// skip LO_LINEFEED
		if( pElement->lo_any.type == LO_TEXT ) {
			if( pElement->lo_text.anchor_href 
				&& pElement->lo_text.anchor_href == pNextTabFocus->pAnchor ) {
				FEU_MakeElementVisible(  GetContext(), (LO_Any *) pElement );  
				count++;
				invalidateElement( pElement );
				if( ! forward )
					pNextTabFocus->pElement = pElement;  // move the current focus pointer to the first segment.
			} else {
				break;			// stop searching ;
			}
		}	// if( pElement->lo_any.type == LO_TEXT ) 

		if( forward)
			pElement = pElement->lo_any.next;
		else
			pElement = pElement->lo_any.prev;

	}	// while(	pElement && 
	return count;
}

// this function is trigered by TAB key
// or called from setFormElementTabFocus() with byClickFormElement = 0, in such case
// the form element is visible and has the fucos already.
void CWinCX::setLastTabFocusElement( LO_TabFocusData *pNextTabFocus, int needSetFocus ) 
{
	LO_Element *pElement;

	// both old and new element can be NULL.

	// avoid setting focus on the same element.
	// For image map, the element may be the same, but the mapAreaIndex 
	// increase for each Tab.
	
	if( pNextTabFocus
		&& m_lastTabFocus.pElement == pNextTabFocus->pElement
		&& m_lastTabFocus.mapAreaIndex	== pNextTabFocus->mapAreaIndex	)
		return;                                      

    if( m_isReEntry_setLastTabFocusElement ) 
        return;
    m_isReEntry_setLastTabFocusElement = 1;

	// need to redraw those 2 elements for visual feedback.
	if(GetPane() && CanBlockDisplay() )	{
		// the new element gets the focus.
		pElement = pNextTabFocus->pElement;
		if( NULL != pElement)  {
			if( needSetFocus ) {
				SetActiveWindow();
                // FEU_MakeElementVisible must happen before invalidate
			    FEU_MakeElementVisible(  GetContext(), (LO_Any *) pElement );  
			    invalidateElement( pElement );
		    
			    if( pElement->lo_any.type == LO_FORM_ELE) {		
				    // Form elements need focus to get keyboard input.
                    // this will loop back to setFormElementTabFocus(), That
                    // is why we need the re-entry protection.
				    FE_FocusInputElement(GetContext(), pElement ); 
			    } else {
					CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();
					if( pFrame )
						pFrame->SetFocus();		// set focus to the window.
				}
            }
				
			if( pElement->lo_any.type == LO_TEXT			// defined_as 1
				|| pElement->lo_any.type == LO_FORM_ELE		// defined_as 6
					&& LO_isFormElementNeedTextTabFocus( (LO_FormElementStruct *)pElement) ) {
				
				// first trace back for the first segmented text.
				// need to do this when: tab backward, and click on not-first segment.
				invalidateSegmentedTextElement(pNextTabFocus, 1);	// forward
				
				// trace down the fragmented text for a link.
				invalidateSegmentedTextElement(pNextTabFocus, 0);	// backward, set to first
			}
		}	// if( NULL != pElement) , the new element

		// the old element losts the focus
		pElement = getLastTabFocusElement();
		if( NULL !=  pElement)  {
			
			invalidateElement( pElement );

			if( pElement->lo_any.type == LO_FORM_ELE) {
				FE_BlurInputElement(GetContext(), pElement );
			} 
			
			if( pElement->lo_any.type == LO_TEXT 
				|| pElement->lo_any.type == LO_FORM_ELE 
					&& LO_isFormElementNeedTextTabFocus( (LO_FormElementStruct *)pElement) ) {
				// remove focus box for all continnuios text fragments.
				// because it is always pointing to the first segment, no need to go backward.
				invalidateSegmentedTextElement(getLastTabFocusData(), 1);	// forward
			}

		}	// if( NULL !=  pElement), the old element
		
	}	// if(GetPane() && CanBlockDisplay() )	

	m_lastTabFocus.pElement		= pNextTabFocus->pElement;
	m_lastTabFocus.mapAreaIndex	= pNextTabFocus->mapAreaIndex;		// 0 means no focus, start with index 1.
	m_lastTabFocus.pAnchor		= pNextTabFocus->pAnchor	;

	// update the status bar.
	// For all LO_ types which update status bar, see CWinCX::OnMouseMoveForLayerCX(UINT uFlags, CPoint& cpPoint,

    if( getLastFocusAnchorStr() != NULL )
        wfe_Progress(GetContext(), getLastFocusAnchorStr() );
	else
		wfe_Progress(GetContext(), "");
	
    m_isReEntry_setLastTabFocusElement = 0;
	return;
}	// CWinCX::setLastTabFocusElement()

// try to set Tab Focus in this CWinCX only.
BOOL CWinCX::setNextTabFocusInWin( int forward )
{
	BOOL			found;
	LO_TabFocusData	newTabFocus;	

	newTabFocus.pElement		= m_lastTabFocus.pElement;
	newTabFocus.mapAreaIndex	= m_lastTabFocus.mapAreaIndex;		// 0 means no area
	newTabFocus.pAnchor			= m_lastTabFocus.pAnchor	;

	found = LO_getNextTabableElement( GetContext(), &newTabFocus, forward );

	// even  new element is NULL, need to clear the old focus
	setLastTabFocusElement( &newTabFocus, 1 );    // key, not click, needSetFocus

	return( found );
}	// BOOL CWinCX::setNextTabFocusInWin( int forward )

// this function searchs for sibling CWinCX, and set Tab focus.
// See MWContext * XP_FindNamedContextInList(MWContext * context, char *name)
// in ns\lib\xp\xp_cntxt.c for navigating grid_parent - grid_children tree.
BOOL CWinCX::setTabFocusNext( int forward )
{
	BOOL		ret;

	// First try myself and my children.
	ret = setTabFocusNextChild( NULL, forward ) ;  // no exclusive child
	if( ret )				
		return( TRUE );				// all done

	// TODO loop to search up all parents
	// now try to set focus to siblings

	// if I am on the top, I cannot have sibling.
	if( IsGridCell() != TRUE ) 
		return( FALSE );
	
	MWContext *pParent = GetParentContext();	
	if(  pParent == NULL)	
		return( FALSE );

	if( ! ABSTRACTCX(pParent)->IsWindowContext() )
		return( FALSE );
	
	ret = WINCX(pParent)->setTabFocusNextChild( GetContext(), forward ) ;
	if ( ret )
		return( TRUE );
	
	return( FALSE );
}

BOOL CWinCX::setTabFocusNextChild( MWContext *currentChildContext, int forward ) 
{
	BOOL		ret;
	XP_List		*gridListHead, *gridList;
	MWContext	*pChild;
	// MWContext	*pCurrent = ( MWContext *) currentChild;

	// try myself first
	ret = setNextTabFocusInWin( forward ) ;
	if( ret ) {
		return( TRUE );
	}

	//	If not a parent, return. over kill?
	if( IsGridParent() == FALSE)
		return( FALSE );

	// see ns\lib\xp\xp_list.c for navigating the list.
	gridListHead	= GetContext()->grid_children;
	if( currentChildContext )
		gridList	= XP_ListFindObject( gridListHead, currentChildContext );
	else 
		gridList  = gridListHead;		// , start from head, The head node is always empty(no data).

	if( gridList != NULL )			// revers order??
		gridList = forward ? gridList->prev : gridList->next ;		// the list is in revers order
	
	while( gridList != NULL ) {
		pChild = (MWContext *) gridList->object ;
		if( ABSTRACTCX(pChild)->IsWindowContext() ) {
			TRACE0("recursive setTabFocusNextChild()\n");
			ret = WINCX(pChild)->setTabFocusNextChild( NULL, forward ) ;  // no exlusive child.
			if( ret )
				return( ret );
		}
		gridList = forward ? gridList->prev : gridList->next ;		// the list is in revers order
	}

	return( FALSE );
}
	
BOOL CWinCX::fireTabFocusElement( UINT nChar)		
{
	int32			mapAreaIndex, xx, yy;
	lo_MapAreaRec	*theArea;

	LO_Element *pFocusElement	= getLastTabFocusElement();
	if( pFocusElement == NULL )
		return( FALSE );
    
	// SpaceBar Return are handled by Form itself for 
	// all Form elements, including checkbox and radio.
	// Here we only take care of the links

	if( pFocusElement->lo_any.type == LO_IMAGE )	{ 
		mapAreaIndex = getLastFocusAreaIndex();
		if( mapAreaIndex > 0 ) {
			// the focus is in a map area
			theArea = LO_getTabableMapArea( GetContext(), (LO_ImageStruct *)pFocusElement, mapAreaIndex );
			if( theArea && LO_findApointInArea( theArea, &xx, &yy ) ) {
				// Area coordinates are relative to the image, add the left-top of the image.
				xx += pFocusElement->lo_any.x;	
				yy += pFocusElement->lo_any.y;
				if ( FE_ClickAnyElement( GetContext(), pFocusElement, 1, xx, yy ) )	// click the center
					return( TRUE );
			}
			return( FALSE );
		}	
		// else mapAreaIndex not > 0, it is a link for the whole image, fall through.
	}

	if(		pFocusElement->lo_any.type == LO_TEXT 
		||	pFocusElement->lo_any.type == LO_IMAGE )	{ 
		// it is a link
		if ( FE_ClickAnyElement( GetContext(), pFocusElement, 0, 0, 0 ) )	// click the center
			return( TRUE );
	}

	return( FALSE );
}

int CWinCX::getImageDrawFlag( MWContext *pContext, LO_ImageStruct *pImage, lo_MapAreaRec **ppArea, uint32 *pFlag )
{
	LO_Element		*pFocusElement;
	int32			lastAreaIndex;
	lo_MapAreaRec	*theArea;

	// clear the flag first
	*pFlag &= ~FE_DRAW_TAB_FOCUS ;

	
	if( pImage == NULL )
		return( 0 );

	pFocusElement	= getLastTabFocusElement();
	if( pFocusElement == NULL)
		return( 0 );

	if( pFocusElement != (LO_Element *) pImage )
		return( 0 );

	lastAreaIndex = getLastFocusAreaIndex();
	if( lastAreaIndex < 0 )					// error
		return(0);

	if( lastAreaIndex == 0 ) {		// no area, the whole image is focused.
        LO_AnchorData *pLastAnchor = getLastFocusAnchorPointer();
        if (pLastAnchor && !(pLastAnchor->flags & ANCHOR_SUPPRESS_FEEDBACK)) {
			*pFlag |= FE_DRAW_TAB_FOCUS;
			return(1);
		}
	}
	
	theArea = LO_getTabableMapArea( pContext, pImage, lastAreaIndex );

    // Don't draw selection rectangle if it's been disabled
	if(theArea && !(theArea->anchor && (theArea->anchor->flags & ANCHOR_SUPPRESS_FEEDBACK))) {
		*pFlag |= FE_DRAW_TAB_FOCUS ;
		*ppArea = theArea;			// only the area is focused.
		return( 1 );
	}
	
	return( 0 );

}

int CWinCX::setTextTabFocusDrawFlag( LO_TextStruct *pText, uint32 *pFlag )
{
	// Visual feedback for Tab Focus is a dotted box around text or image.
	// For 2 Form elements, check box and radio button, the box is on
	// the text following the button.
	//
	// The text may have been fragmented into multiple lines because the line folding.
	// When line width changes, fragments can be generated, or merged. It can happen 
	// anytime when the width of Navigator is changed. So, it cannot be handled
	// when Tab key is pressed.
	// 
	// When searching for next Tabable element, focus is moved to next different anchor.
	// Continuious text fragments are skipped.
	// See LO_getNextTabableElement() in lib\layout\laysel.c file.
	//
	// To find all focus text fragments, the Anchor data is checked. Every fragment has
	// its own copy of duplicated Anchor pointer. If it is the same as the Focused element,
	// the text will be drawn as focused.
	//
	// When an element losts Tab focus, all dotted fragments need redraw to erase the focus.
	//

	// assuming no focus and clear the flag first.
	*pFlag &= ~FE_DRAW_TAB_FOCUS ;

	if( pText == NULL )
		return(0);

	LO_Element *pCurrentElement, *pPreviousElement, *pFocusElement;

	pFocusElement	= getLastTabFocusElement();
	if( pFocusElement == NULL)
		return(0);

    // Don't draw selection rectangle if it's been disabled
    if ( pText->anchor_href 
         && pText->anchor_href->flags & ANCHOR_SUPPRESS_FEEDBACK)
        return(0);

	pCurrentElement = (LO_Element *) pText;

	// return focus if it is a link and it has Tab focus
	if( pFocusElement == pCurrentElement ) {
		*pFlag |= FE_DRAW_TAB_FOCUS;
		return( 1 );
	}

	// test fragmented text for a link.
	// is one fragmented element.
	if( pFocusElement->lo_any.type == LO_TEXT          // fix for bug #45111
		&& pText->anchor_href 
		&& pText->anchor_href == getLastFocusAnchorPointer() ) {
		*pFlag |= FE_DRAW_TAB_FOCUS ;
		return( 1 );
	}

	// Test if it is a text element following a Form element checkBox(or radio button),
	// which has focus. If it is, the text(not the button) needs to draw the focus box.
	//todo in html, two separated element may have the same link. need to make sure it
	pPreviousElement = pCurrentElement->lo_any.prev;
	if( pPreviousElement != NULL  && pPreviousElement == pFocusElement ) {
		if( LO_isFormElementNeedTextTabFocus( (LO_FormElementStruct *)pPreviousElement ) ) {
			*pFlag |= FE_DRAW_TAB_FOCUS ;
			return(1);
		}
	}

	return(0);
	
}	// isTabFocusText()
//#endif	/* NO_TAB_NAVIGATION */

void CWinCX::CancelSizing()
{
#ifdef EDITOR
    ASSERT(EDT_IS_EDITOR(GetContext()));
    // Remove last sizing feedback
    DisplaySelectionFeedback(LO_ELE_SELECTED, m_rectSizing);
    EDT_CancelSizing(GetContext());
#endif // EDITOR
}


