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
#include "secnav.h"
#include "frameglu.h"
#include "mainfrm.h"
#include "ipframe.h"
#include "prefapi.h"

//	Our active frame stack.
CPtrList CFrameGlue::m_cplActiveFrameStack;


//	A way to correctly get the frame glue out of a frame at run time though
//		the class derivations and casting are not right....
CFrameGlue *CFrameGlue::GetFrameGlue(CFrameWnd *pFrame)	{
	//	Use IsKindOf to determine the frame's class type.
	//	From there, we can correctly cast the frame window pointer, and
	//		further cast to the abstract base class of CFrameGlue.
	CFrameGlue *pRetval = NULL;
	if(pFrame->IsKindOf(RUNTIME_CLASS(CGenericFrame)))	{
		pRetval = (CFrameGlue *)((CGenericFrame *)pFrame);		
	}
	else if(pFrame->IsKindOf(RUNTIME_CLASS(CInPlaceFrame)))	{
		pRetval = (CFrameGlue *)((CInPlaceFrame *)pFrame);
	}

#ifdef DEBUG
	if(pRetval == NULL)	{
		//	If you got here, either you need to add appropriate casting statements
		//		in the above if to resolve the CFrameGlue, or you have found a bug.
		TRACE("Invalid frame window has no frame glue!\n");
//		ASSERT(0);
	}
#endif // DEBUG

	return(pRetval);
}


CFrameGlue::CFrameGlue()	{
    m_pActiveContext = NULL;
    m_pMainContext = NULL;
	m_bCanRestoreState = FALSE;
	m_iCSID = theApp.m_iCSID;
	m_pChrome = NULL;

    // location bar
    // note I just changed this from 1.0 version to be more appropriate chouck 24-jan-95
	XP_Bool bBar;
	PREF_GetBoolPref("browser.chrome.show_url_bar",&bBar);
    m_bLocationBar = bBar;

    // show the starter buttons?
	PREF_GetBoolPref("browser.chrome.show_directory_buttons",&bBar);
    m_bStarter = bBar;

	PREF_GetBoolPref("browser.chrome.show_toolbar",&bBar);
    m_bShowToolbar = bBar;

	//	No find replace dialog yet.
	m_pFindReplace = NULL;

    m_pClipChildMap = NULL;
	m_isBackgroundPalette = FALSE;
}

CFrameGlue::~CFrameGlue()	{
	//	Take all entries of this frame in the active frame stack out.
	POSITION pos;

	while( pos = m_cplActiveFrameStack.Find( (LPVOID) this ) ){
		m_cplActiveFrameStack.RemoveAt( pos );
	}


	//	When the frame goes down, let the window context know.
	if(GetMainWinContext())	{
		GetMainWinContext()->ClearFrame();
	}

    if( m_pClipChildMap != NULL ) {
        m_pClipChildMap->RemoveAll();

        delete m_pClipChildMap;
        m_pClipChildMap = NULL;
    }
}


//	Generic way to get a frame out of the context.
CFrameGlue *GetFrame(MWContext *pContext)	
{
	CFrameGlue *pRetval = NULL;
	if(pContext != NULL)	{
		if(ABSTRACTCX(pContext) && ABSTRACTCX(pContext)->IsFrameContext())	{
			pRetval = ABSTRACTCX(pContext)->GetFrame();
		}
	}

	return(pRetval);
}

CAbstractCX *CFrameGlue::GetMainContext() const
{
	return(m_pMainContext);
}

CAbstractCX *CFrameGlue::GetActiveContext() const 
{
	return(m_pActiveContext);
}

CWinCX *CFrameGlue::GetMainWinContext() const
{
	if (m_pMainContext && m_pMainContext->IsFrameContext()) {		
		return((CWinCX *) m_pMainContext);
	}
	return NULL;
}

CWinCX *CFrameGlue::GetActiveWinContext() const
{
	if (m_pActiveContext && m_pActiveContext->IsFrameContext()) {		
		return((CWinCX *) m_pActiveContext);
	}
	return NULL;
}

void CFrameGlue::SetMainContext(CAbstractCX *pContext)	{
	m_pMainContext = pContext;
}

CPtrList CFrameGlue::m_cplActiveContextCBList;

void CFrameGlue::SetActiveContext(CAbstractCX *pContext)
{
    m_pActiveContext = pContext;

    POSITION rIndex = m_cplActiveContextCBList.GetHeadPosition();
    while(rIndex != NULL)   {
        ActiveContextCB cb = (ActiveContextCB)m_cplActiveContextCBList.GetNext(rIndex);
        (cb)(this, pContext);
    }
}

void CFrameGlue::AddActiveContextCB(ActiveContextCB cb)
{
    m_cplActiveContextCBList.AddTail((LPVOID)cb);
}

void CFrameGlue::RemoveActiveContextCB(ActiveContextCB cb)
{
    POSITION rIndex = m_cplActiveContextCBList.Find((LPVOID)cb);
    if(rIndex)  {
        m_cplActiveContextCBList.RemoveAt(rIndex);
    }
}

// Override to test if frame is an editor
BOOL CFrameGlue::IsEditFrame() 
{
    return(FALSE);
}

CPtrList CFrameGlue::m_cplActiveNotifyCBList;

//	Add to the top of the stack of active frames.
void CFrameGlue::SetAsActiveFrame()
{
	POSITION rIndex = m_cplActiveNotifyCBList.GetHeadPosition();
	while(rIndex != NULL)	{
		ActiveNotifyCB cb = (ActiveNotifyCB) m_cplActiveNotifyCBList.GetNext(rIndex);
		(cb)(this);
	}

	POSITION pos = m_cplActiveFrameStack.Find( (LPVOID) this );
	if (pos) {
		m_cplActiveFrameStack.RemoveAt( pos );
	}

	TRY	{
		m_cplActiveFrameStack.AddHead((void *)this);

	}
	CATCH(CException, e)	{
		//	didn't work, Silently fail...
		ASSERT(0);
	}
	END_CATCH
}

void CFrameGlue::AddActiveNotifyCB( ActiveNotifyCB cb )
{
	m_cplActiveNotifyCBList.AddTail( (LPVOID) cb );
}

void CFrameGlue::RemoveActiveNotifyCB( ActiveNotifyCB cb )
{
	POSITION rIndex = m_cplActiveNotifyCBList.Find( (LPVOID) cb );
	if (rIndex) {
		m_cplActiveNotifyCBList.RemoveAt( rIndex );
	}
}

//	Find a browser window in the active stack, only use cxType types.
//  Default is to NOT find editor windows when cxType = MWContextBrowser
CFrameGlue *CFrameGlue::GetLastActiveFrame(MWContextType cxType, int nFindEditor)
{
	CFrameGlue *pRetval = NULL;

	//	Loop through all active frames.
	POSITION rIndex = m_cplActiveFrameStack.GetHeadPosition();
	while(rIndex != NULL)	{
		pRetval = (CFrameGlue *)m_cplActiveFrameStack.GetNext(rIndex);
		
		//	See if we can get the type from the context, and verify the type.
		if(pRetval->GetMainContext() != NULL)	{
			MWContextType cxRetType = pRetval->GetMainContext()->GetContext()->type;
			if(cxType == MWContextAny || cxRetType == cxType)	{
                // If looking for only Browser, skip an editor frame
				if (cxType == MWContextBrowser){
					BOOL bIsEditor = EDT_IS_EDITOR(pRetval->GetMainContext()->GetContext());
					if((nFindEditor == FEU_FINDBROWSERONLY && bIsEditor) ||
					   (nFindEditor == FEU_FINDEDITORONLY && !bIsEditor)) {
				
            		//	Failed the check. clear retval.
	            		pRetval = NULL;
		                continue;
					}
                }

				// Don't allow Netcaster either
				if (pRetval->GetMainContext()->GetContext() == theApp.m_pNetcasterWindow
			    || (pRetval->GetMainContext()->GetContext()->name && (XP_STRCASECMP(pRetval->GetMainContext()->GetContext()->name,"Netscape_Netcaster_Drawer") == 0))) {
	            	pRetval = NULL;
		            continue;
				}

				//	Found one.
				break;
			}
		}

		//	Failed the check. clear retval.
		pRetval = NULL;
	}

	return(pRetval);
}

CFrameGlue *CFrameGlue::GetLastActiveFrameByCustToolbarType(CString custToolbar, CFrameWnd *pCurrentFrame, BOOL bUseSaveInfo)
{

	CFrameGlue *pRetval = NULL;

	//	Loop through all active frames.
	POSITION rIndex = m_cplActiveFrameStack.GetHeadPosition();
	while(rIndex != NULL)	{
		
		pRetval = (CFrameGlue *)m_cplActiveFrameStack.GetNext(rIndex);
		
		// if it's the current frame, then keep looking because we want the one before it.
		if(pRetval->GetFrameWnd() == pCurrentFrame)	{
			pRetval = NULL;
			continue;
		}
		//	See if we share the same custtoolbar string.
		if((pRetval->m_pChrome) && (pRetval->m_pChrome->GetCustToolbarString() == custToolbar)){
			if(bUseSaveInfo){
			// if customizable toolbar saves its prefs, then break
			// this is used to ignore windows like View Page Source and View Page Info
			 if(pRetval->m_pChrome->GetCustomizableToolbar()->GetSaveToolbarInfo())
				break;
			}
			else
				break;
		}

		//	Failed the check. clear retval.
		pRetval = NULL;
	}

	return(pRetval);
}

CFrameGlue *CFrameGlue::GetBottomFrame(MWContextType cxType, int nFindEditor)
{

	CFrameGlue *pRetval = NULL;

	//	Loop through all active frames until we reach the bottom.
	POSITION rIndex = m_cplActiveFrameStack.GetTailPosition();
	while(rIndex != NULL)	{
		pRetval = (CFrameGlue *)m_cplActiveFrameStack.GetPrev(rIndex);


		//	See if we can get the type from the context, and verify the type.
		if(pRetval->GetMainContext() != NULL)	{
			MWContextType cxRetType = pRetval->GetMainContext()->GetContext()->type;
			if(cxType == MWContextAny || cxRetType == cxType)	{
                // If looking for only Browser, skip an editor frame
 				if (cxType == MWContextBrowser){
					BOOL bIsEditor = EDT_IS_EDITOR(pRetval->GetMainContext()->GetContext());
					if((nFindEditor == FEU_FINDBROWSERONLY && bIsEditor) ||
					   (nFindEditor == FEU_FINDEDITORONLY && !bIsEditor)) {
            		//	Failed the check. clear retval.
            			continue;
					}
                }
				// Found one
				break;
			}
		}
	}

	return(pRetval);

}

// Find the number of active frames of type cxType
int CFrameGlue::GetNumActiveFrames(MWContextType cxType, int nFindEditor)
{
	int nCount = 0;
	CFrameGlue *pRetval = NULL;
	//	Loop through all active frames.
	POSITION rIndex = m_cplActiveFrameStack.GetHeadPosition();
	while(rIndex != NULL)	{
		pRetval = (CFrameGlue *)m_cplActiveFrameStack.GetNext(rIndex);
		
		//	See if we can get the type from the context, and verify the type.
		if(pRetval->GetMainContext() != NULL)	{
			MWContextType cxRetType = pRetval->GetMainContext()->GetContext()->type;
			if(cxType == MWContextAny || cxRetType == cxType)	{
                // If looking for only Browser, skip an editor frame
				if (cxType == MWContextBrowser){
					BOOL bIsEditor = EDT_IS_EDITOR(pRetval->GetMainContext()->GetContext());
					if((nFindEditor == FEU_FINDBROWSERONLY && bIsEditor) ||
						(nFindEditor == FEU_FINDEDITORONLY && !bIsEditor)) {

	                      continue;
					}
                }
				//	Found one so increase count.

				nCount++;
			}
		}
	}

	return(nCount);

}

//	Find a browser window by context ID....
CFrameGlue *CFrameGlue::FindFrameByID(DWORD dwID, MWContextType cxType)
{
	CFrameGlue *pRetval = NULL;

	//	Must have a context first.
	CAbstractCX *pCX = CAbstractCX::FindContextByID(dwID);
	if(pCX != NULL)	{
		//	Must be a window context.
		if(pCX->IsFrameContext() == TRUE)	{
			//	Make sure the context is of the correct type.
			if(cxType == MWContextAny || cxType == pCX->GetContext()->type)	{
				pRetval = VOID2CX(pCX, CAbstractCX)->GetFrame();
			}
		}
	}

	return(pRetval);
}

//	Common command handler for all frames.
//	Call in your OnCommand derivation.
BOOL CFrameGlue::CommonCommand(UINT wParam, LONG lParam)	{
	UINT nID = LOWORD(wParam);

	if (IS_PLACESMENU_ID(nID)  || IS_HELPMENU_ID(nID)) {
		char * url = NULL;
		if (IS_PLACESMENU_ID(nID))
			PREF_CopyIndexConfigString("menu.places.item",CASTINT(nID-FIRST_PLACES_MENU_ID),"url",&url);
		else 
			PREF_CopyIndexConfigString("menu.help.item",CASTINT(nID-FIRST_HELP_MENU_ID),"url",&url);
		if (!url) return FALSE;

        if(!GetMainContext())
            return(FALSE);
        // if we are a browser window and NOT an editor just load it locally
        if(GetMainContext()->GetContext()->type == MWContextBrowser &&
           !EDT_IS_EDITOR(GetMainContext()->GetContext()) ){
    		GetMainContext()->NormalGetUrl(url);
            return(TRUE);
        }

        // look for a browser window we can load into
		CFrameGlue *pFrameGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser);
		if(pFrameGlue != NULL && pFrameGlue->GetMainContext() != NULL)	{
            CAbstractCX * pCX = pFrameGlue->GetMainContext();
            if (pCX != NULL) {
                CFrameWnd * pWnd = pFrameGlue->GetFrameWnd();
                if (pWnd->IsIconic())
                    pWnd->ShowWindow(SW_RESTORE);
                pWnd->BringWindowToTop();
                pCX->NormalGetUrl(url);
            }
//	        pFrameGlue->GetMainContext()->NormalGetUrl(szLoadString(nID + LOAD_URL_COUNT));
            return(TRUE);
		}

        // if we got here we must not have found a viable window --- create a new one
        MWContext * pContext = CFE_CreateNewDocWindow(NULL, NULL);
        ABSTRACTCX(pContext)->NormalGetUrl(url);
		XP_FREE(url);
        return(TRUE);	
	} else if ( nID == ID_SECURITY_ADVISOR ) {
		CAbstractCX *pCX = GetMainContext();
		if (pCX != NULL) 
		{
			MWContext * pContext = pCX->GetContext();
			if (pContext != NULL)
			{
				URL_Struct * pURL = pCX->CreateUrlFromHist(TRUE);

				SECNAV_SecurityAdvisor(pContext, pURL);
			}
		}
	}

	return(FALSE);
}

#define CLIP_BIT        0x80000000L
#define GET_CLIP_BIT(x) ( (DWORD)(x) & CLIP_BIT )
#define CLIP_COUNT(x)   ( (DWORD)(x) & 0x0000FFFFL )

void CFrameGlue::ClipChildren(CWnd *pWnd, BOOL bSet)
{

//    ASSERT( GetFrameWnd()->IsChild(pWnd) );
#ifdef _DEBUG
	if (GetFrameWnd() && !GetFrameWnd()->IsChild(pWnd))
		TRACE("pWnd is not a child of this frame\n");
#endif
    // If the child window hash table does not exist, then create one
    if( m_pClipChildMap == NULL ) {
        m_pClipChildMap = new CMapPtrToPtr();
    }
    //
    // For every window in the hierarchy between pWnd and the top level 
    // frame, set or clear its WS_CLIPCHILDREN style bit.
    //
    while( (pWnd = pWnd->GetParent()) != NULL ) {
        DWORD dwStyle;
        void *item = 0;
        void *key = (void*)pWnd->m_hWnd;

        dwStyle = ::GetWindowLong(pWnd->m_hWnd, GWL_STYLE);

        // Add the window to the map if necessary...
        if( m_pClipChildMap->Lookup(key, item) == FALSE && bSet ) {

            item = (void *)((dwStyle & WS_CLIPCHILDREN) ? CLIP_BIT : 0L);
            m_pClipChildMap->SetAt(key, item);
        }

        // Setting the WS_CLIPCHILDREN bit...        
        if( bSet ) {
            // Set the style the first time...
            if( CLIP_COUNT(item) == 0 ) {
                ::SetWindowLong(pWnd->m_hWnd, GWL_STYLE,(dwStyle|WS_CLIPCHILDREN));
            }
            // Increment the count and save the new state...
            item = (void *) (((DWORD)item) + 1);
            m_pClipChildMap->SetAt(key, item);
        } 

        // Clearing the WS_CLIPCHILDREN bit...
        else if (CLIP_COUNT(item)) {
            // Decrement the count...
            item = (void *) (((DWORD)item) - 1);

             // Restore the window to its original state and remove it
             // from the map.
            if( CLIP_COUNT(item) == 0 ) {
                if( GET_CLIP_BIT(item) == 0 ) {
                    ::SetWindowLong(pWnd->m_hWnd, GWL_STYLE,
                                    (dwStyle & ~WS_CLIPCHILDREN) );
                }
                m_pClipChildMap->RemoveKey(key);
            } 
            // Save the new state...
            else {
                m_pClipChildMap->SetAt(key, item);
            }
        }
    }
}


BOOL CFrameGlue::RealizePalette(CWnd *pWnd, HWND hFocusWnd,BOOL background)
{
	CWinCX *pWinCX = GetMainWinContext();
	HDC hdc = pWinCX->GetContextDC();
	if (hdc) {
		::SelectPalette(hdc, pWinCX->GetPalette(), background);	
		int colorRealized = ::RealizePalette(hdc);
		int lCount = XP_ListCount(pWinCX->GetContext()->grid_children);
		if (lCount) {
			//	Go through each child.
			MWContext *pChild;
			XP_List *pTraverse = pWinCX->GetContext()->grid_children;
			while (pChild = (MWContext *)XP_ListNextObject(pTraverse)) {
				ASSERT(ABSTRACTCX(pChild)->IsWindowContext());
				::InvalidateRect(PANECX(pChild)->GetPane(), NULL, TRUE);
				::UpdateWindow(PANECX(pChild)->GetPane());
			}
		}
		SetIsBackgroundPalette(background);
		::InvalidateRect(pWinCX->GetPane(), NULL, TRUE);
		::UpdateWindow(pWinCX->GetPane());
	#ifdef MOZ_TASKBAR
		if(theApp.GetTaskBarMgr().IsInitialized() && theApp.GetTaskBarMgr().IsFloating())
			theApp.GetTaskBarMgr().ChangeTaskBarsPalette(hFocusWnd);
	#endif /* MOZ_TASKBAR */
		pWnd->SendMessageToDescendants(WM_PALETTECHANGED, (WPARAM)hFocusWnd);
		return (colorRealized > 0) ? TRUE  : FALSE;
	}
	else return FALSE;
}

void CFrameGlue::ClearContext(CAbstractCX *pGone) {
    //	Clear out the appropriate context
    //		fields, if set.
    //	Can't have an active without a main.
    if(GetMainContext() == pGone)	{
        SetActiveContext(NULL);
        SetMainContext(NULL);
    }
    else if(GetActiveContext() == pGone)	{
        SetActiveContext(NULL);
    }
    else if(pGone && pGone->IsGridParent()) {
        //  Need to see if pGone is a parent of the active context.
        //  If so, we need to clear our active context as the child
        //      context won't be able to look us up to call this
        //      function due to the recursive implemenation of
        //      CWinCX::GetFrame() which it uses to make this call
        MWContext *pChild;
		XP_List *pTraverse = pGone->GetContext()->grid_children;
        while((pChild = (MWContext*)XP_ListNextObject(pTraverse)))	{
            ClearContext(ABSTRACTCX(pChild));
        }
    }
}



//	NULL frame implementation, for CFrameGlue when there is no frame.
CFrameWnd *CNullFrame::GetFrameWnd()	{
	return(NULL);
}

void CNullFrame::UpdateHistoryDialog()	{
}

CAbstractCX *CNullFrame::GetMainContext() const	{
	return(NULL);
}

CAbstractCX *CNullFrame::GetActiveContext() const	{
	return(NULL);
}
