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
#include "gridedge.h"
#include "netsvw.h"
#include "template.h"

#define VIEW_SOURCE_TARGET_WINDOW_NAME "%ViewSourceWindow"	/* from ns/lib/libnet/cvcolor.c */

#ifndef MOZ_NGLAYOUT
MWContext *FE_MakeGridWindow(MWContext *pOldContext, void *hist_list, void *pHistory,
	int32 lX, int32 lY,
	int32 lWidth, int32 lHeight, char *pUrlStr, char *pWindowName, int8 iScrollType,
    NET_ReloadMethod eReloadThis, Bool no_edge)	{

	//	First, ensure that the context we're going into is of an appropriate type.
	if(pOldContext == NULL || ABSTRACTCX(pOldContext)->IsFrameContext() == FALSE)	{
		//	Only let this happen to window contexts for now.
		return(NULL);
	}
	else	{
		//	The goal here is to create a new, fabulous, grid that is a child of the
		//		other window context's frame.
		CWinCX *pParentCX = VOID2CX(pOldContext->fe.cx, CWinCX);

		//	Safety dance.
		if(pParentCX->GetPane() == NULL)	{
			return(NULL);
		}

		HWND hParentWnd = pParentCX->GetPane();

		//	Bring up a create context, so that the Create call won't complain on
		//		16 bit windows.
		//	Since we create the context here, and the document, along with the view,
		//		the view takes care of destroying the docuemnt on close, so we don't
		//		need to worry about it.
		CCreateContext cccGrid;
		cccGrid.m_pNewViewClass = RUNTIME_CLASS(CNetscapeView);
		cccGrid.m_pCurrentDoc = new CGenericDoc();

        //	Create the view.
        DWORD dwGridStyle = WS_HSCROLL | WS_VSCROLL;
		CNetscapeView *pNewView = new CNetscapeView();
		pNewView->Create(NULL,
		   	pWindowName,
		   	dwGridStyle,
            CRect(0, 0, 1, 1),
			CWnd::FromHandle(hParentWnd),
			0,
			&cccGrid);

		CWinCX *pWinCX = new CWinCX((CGenericDoc *)cccGrid.m_pCurrentDoc, pParentCX->GetFrame(), pNewView);

		//	Set the context's name.
		//	Set its parent.
		pWinCX->SetContextName(pWindowName);
		pWinCX->SetParentContext(pOldContext);

		RECT rect;
		rect.left = CASTINT(pParentCX->Twips2PixX(lX));
		rect.top = CASTINT(pParentCX->Twips2PixY(lY));
		rect.right = CASTINT(pParentCX->Twips2PixX(lX) + pParentCX->Twips2PixX(lWidth));
		rect.bottom = CASTINT(pParentCX->Twips2PixY(lY) + pParentCX->Twips2PixY(lHeight));
		//	Initialize the context.
		pWinCX->Initialize(pWinCX->CDCCX::IsOwnDC(), &rect);

        //  Tell it wether or not it will have an edge (border in the
        //      non client area).
        pWinCX->SetBorder(!no_edge);

        //  Size the window to final coords.
        //  This causes the non client calc to happen which is needed
        //      now that it knows it's a grid cell.
        pNewView->MoveWindow(
            CRect(CASTINT(pParentCX->Twips2PixX(lX)),
		   	CASTINT(pParentCX->Twips2PixY(lY)),
		   	CASTINT(pParentCX->Twips2PixX(lX) + pParentCX->Twips2PixX(lWidth)),
		   	CASTINT(pParentCX->Twips2PixY(lY) + pParentCX->Twips2PixY(lHeight))),
            FALSE);

		//	Decide what we're doing about scroll bars.
		switch(iScrollType)	{
		case LO_SCROLL_NO:
			//	Shouldn't ever have scrollbars.
			//	Get rid of them.
			pWinCX->SetDynamicScrollBars(FALSE);
			pWinCX->ShowScrollBars(SB_BOTH, FALSE);
			pWinCX->SetAlwaysShowScrollBars(FALSE);
			break;
		case LO_SCROLL_AUTO:
			//	Aha, the hard case.
			//	Set a flag so that when the load is complete in the context, such
			//		that we will hide the scroll bars if there is already enough room.
			//	This means that the entire document fits on the screen (not much data),
			//		and that doing a load from resize at that point will be minimal
			//		overhead.
			pWinCX->SetDynamicScrollBars(TRUE);
			pWinCX->SetAlwaysShowScrollBars(FALSE);
			break;
		case LO_SCROLL_YES:
			//	Have the scroll bars show.
			pWinCX->SetDynamicScrollBars(FALSE);
			pWinCX->SetAlwaysShowScrollBars(TRUE);
			pWinCX->ShowScrollBars(SB_BOTH, TRUE);
			break;
		default:
			ASSERT(0);	// user mush pass in invalid value.
		}

		//	If we have a history list, we need to stick it in the context.
		History_entry *pHistEnt = (History_entry *)pHistory;
		XP_List *pHistList = (XP_List *)hist_list;
		if(pHistList != NULL)	{
			pWinCX->GetContext()->hist.list_ptr = pHistList;
		}
		else	{
			//	If we have a history entry, we need to add it to the session.
			if(pHistEnt != NULL)	{
				SHIST_AddDocument(pWinCX->GetContext(), pHistEnt);
			}
		}

		//	Have it load the url in question.
		//	This could be from the history entry.
		if(pHistEnt == NULL)	{
			URL_Struct *pUrl = NET_CreateURLStruct(pUrlStr, NET_DONT_RELOAD);
			pUrl->force_reload = eReloadThis;

            //  Be sure to add the referrer field of the top level context.
            MWContext *mwRefer = pOldContext;
            while(mwRefer->grid_parent) {
                mwRefer = mwRefer->grid_parent;
            }
            History_entry *he = SHIST_GetCurrent (&mwRefer->hist);
            if(he && he->referer)   {
    		    pUrl->referer = XP_STRDUP(he->referer);
            }

			pWinCX->GetUrl(pUrl, FO_CACHE_AND_PRESENT);
		}
		else	{
			URL_Struct *pUrl = SHIST_CreateURLStructFromHistoryEntry(pWinCX->GetContext(), pHistEnt);
			pUrl->force_reload = eReloadThis;
			pWinCX->GetUrl(pUrl, FO_CACHE_AND_PRESENT);
		}

        // Grid parents have WS_CLIPCHILDREN turned on. This allows them
        // to draw (backgrounds for example) into their windows without
        // worrying about their children.
        CFrameGlue *pGlue = pWinCX->GetFrame();
        if(pGlue) {
            pGlue->ClipChildren(pNewView, TRUE);
        }

		//	Show the window, but don't activate it.
		pNewView->ShowWindow(SW_SHOWNA);

		return(pWinCX->GetContext());
	}
}

void FE_LoadGridCellFromHistory(MWContext *pContext, void *hist, NET_ReloadMethod netReloadMethod) {
	History_entry *pHist = (History_entry *)hist;

	if (pHist != NULL)	{
        URL_Struct *pUrl = SHIST_CreateURLStructFromHistoryEntry(pContext, pHist);
        if(pUrl)    {
            pUrl->force_reload = netReloadMethod;
	        ABSTRACTCX(pContext)->GetUrl(pUrl, FO_CACHE_AND_PRESENT, TRUE);
        }
	}
}

void *FE_FreeGridWindow(MWContext *pContext, XP_Bool bSaveHistory)	{
	ASSERT(ABSTRACTCX(pContext)->IsWindowContext());

	void *pRetval = NULL;

	//	Destroy the view then the doc/it in turn destroys the context.
	//	We have to clean up the doc, since we passed it in....
	CWinCX *pWinCX = VOID2CX(pContext->fe.cx, CWinCX);
	CWinCX *pParentCX =  WINCX(pWinCX->GetParentContext());

	//	We may want to return the history entry.
	if(bSaveHistory == TRUE)	{
        //  Reset to top of document
   		SHIST_SetPositionOfCurrentDoc(&(pWinCX->GetContext()->hist), 0);
        if(pWinCX->GetOriginX() || pWinCX->GetOriginY())    {
#ifdef LAYERS
		    LO_Any *pAny = (LO_Any *)LO_XYToNearestElement(pWinCX->GetContext(), pWinCX->GetOriginX(), pWinCX->GetOriginY(), NULL);
#else
		    LO_Any *pAny = (LO_Any *)LO_XYToNearestElement(pWinCX->GetContext(), pWinCX->GetOriginX(), pWinCX->GetOriginY());
#endif

		    //	Save the location of the current document.
    		if(pAny != NULL)	{
	    		SHIST_SetPositionOfCurrentDoc(&(pWinCX->GetContext()->hist), pAny->ele_id);
		    }
        }

#ifdef MOZ_NGLAYOUT
    ASSERT(0);
#else
		LO_DiscardDocument(pWinCX->GetContext());
#endif
		XP_List *hist_list = pWinCX->GetContext()->hist.list_ptr;
		pWinCX->GetContext()->hist.list_ptr = NULL;

		pRetval = (void *)hist_list;
	}

    CFrameGlue *frame = pWinCX->GetFrame();

	//  Make sure child grid cell going away doesn't hold onto frame
	//	which may also be going down.
	pWinCX->ClearFrame();

	ASSERT(pWinCX->GetPane());
	if(pWinCX->GetPane())	{
		CGenericView *pView = pWinCX->GetView();

		ASSERT(pView);
		if(pView)   {
            // Tell our parent(s) that they don't need to clip children
            // anymore just for our sake.
            if(frame) {
                frame->ClipChildren(pView, FALSE);
            }
		    pView->FrameClosing();
			pView->SendMessage(WM_CLOSE);
        }
	}

	return(pRetval);
}
#endif /* MOZ_NGLAYOUT */

void FE_GetFullWindowSize(MWContext *pContext, int32 *plWidth, int32 *plHeight)	{

	//	Only do this on windows.
	if(ABSTRACTCX(pContext)->IsWindowContext() == TRUE && ABSTRACTCX(pContext)->IsDestroyed() == FALSE)	{
		CPaneCX *pWindow = PANECX(pContext);

		RECT crDimensions;

		//	Get the full size of the window.
        ::GetClientRect(pWindow->GetPane(), &crDimensions);

		//	Convert the dimensions to twips.
		HDC hDC = pWindow->GetContextDC();
		::DPtoLP(hDC, (POINT *)&crDimensions, 2);
		pWindow->ReleaseContextDC(hDC);

		*plWidth = crDimensions.right - crDimensions.left;
		*plHeight = crDimensions.bottom - crDimensions.top;
		
		//  Adjust to not include scroll bars.
		if(pWindow->IsVScrollBarOn()) {
		    *plWidth += sysInfo.m_iScrollWidth;
		}
		if(pWindow->IsHScrollBarOn()) {
		    *plHeight += sysInfo.m_iScrollHeight;
		}
	}
	else	{
		//	No window here....
        //  Set to 1 to avoid divide by zero errors in Back end.
		*plHeight = *plWidth = 1;
	}
}

#ifndef MOZ_NGLAYOUT
void FE_RestructureGridWindow(MWContext *pContext, int32 lX, int32 lY, int32 lWidth, int32 lHeight)	{
	TRACE("FE_RestructureGridWindow(%p, %ld, %ld, %ld, %ld)\n", pContext, lX, lY, lWidth, lHeight);

	//	Safety dance.
	if(pContext == NULL)	{
		return;
	}

	//	We're under the immediate assumption that the context is a window.
	ASSERT(ABSTRACTCX(pContext)->IsWindowContext());

	CWinCX *pWinCX = WINCX(pContext);

	//	We need to move the window.
	//	It will automagically redraw itself.
    ::MoveWindow(pWinCX->GetPane(), CASTINT(pWinCX->Twips2PixX(lX)),
		CASTINT(pWinCX->Twips2PixY(lY)),
		CASTINT(pWinCX->Twips2PixX(lWidth)),
		CASTINT(pWinCX->Twips2PixY(lHeight)),
        TRUE);

	//	Dynamic scroll bars are handled internally, as is resize.
}
#endif /* MOZ_NGLAYOUT */

void FE_SetWindowLoading(MWContext *pContext, URL_Struct *pUrl, Net_GetUrlExitFunc **ppExitRoutine)	{

	//	A url load is changing context.
	//	Telling the exit routine that we'd like to be called.
	*ppExitRoutine = CFE_GetUrlExitRoutine;

	//	Now, call the GetUrl routine, but don't really load.
	//	This bootstraps the normal loading procedure.
	//	DON'T REALLY CARE WHAT WE PASS IN AS THE PRESENTATION TYPE, yet.
	ABSTRACTCX(pContext)->GetUrl(pUrl, FO_CACHE_AND_PRESENT, FALSE);
}

#ifndef MOZ_NGLAYOUT
void FE_GetEdgeMinSize(MWContext *pContext, int32 *pSize, Bool no_edge)	{
	TRACE("FE_GetEdgeMinSize(%p, %p)\n", pContext, pSize);

    //  Don't care anymore.
	*pSize = 5;

    if(!no_edge)    {
		*pSize = 5;
    }
}
#endif /* MOZ_NGLAYOUT */

MWContext *FE_MakeBlankWindow(MWContext *pOldContext, URL_Struct *pUrl, char *pContextName)	{
//	TRACE("FE_MakeBlankWindow(%p, %p, %s)\n", pOldContext, pUrl, pContextName);

	//	Create a new top level blank window.
	//	Only don't load anything.  Leave it blank.
	//	HACK ALERT:
	//	In order to better do this, we need to have the CreateNewDocWindow
	//		function not load what's in the history.  It only does this with
	//		other browser windows.  Anyhow, switch our context type temporarily
	//		in order to get the effect.
	MWContextType OldType = pOldContext->type;
	pOldContext->type = (MWContextType)-1;
	MWContext *pRetval = pOldContext->funcs->CreateNewDocWindow(pOldContext, NULL);
	pOldContext->type = OldType;

	if(pRetval != NULL)	{
		ABSTRACTCX(pRetval)->SetContextName(pContextName);
	}

	return(pRetval);
}

//	Create a new window.
//	If pChrome is NULL, do a FE_MakeBlankWindow....
//	pChrome specifies the attributes of a window.
//  If you use this call, Toolbar information will not be saved in the preferences.
MWContext *FE_MakeNewWindow(MWContext *pOldContext, URL_Struct *pUrl, char *pContextName, Chrome *pChrome)
{

    BOOL bNetHelpWnd = FALSE;
    
    
	//	Decide which type of context this will be (we use this to possible access some templates).
	MWContextType cxType = MWContextAny;
	if(pChrome != NULL)	{
		switch(pChrome->type)	{
            case MWContextHTMLHelp:
                pChrome->type = MWContextBrowser;
              #ifndef _WIN32
                pChrome->allow_resize = FALSE;
              #endif  
                bNetHelpWnd = TRUE;
                // Fall through...
			case MWContextBrowser:
				cxType = MWContextBrowser;
				break;
			case MWContextDialog:
				cxType = MWContextDialog;
				break;
		}
	}
	else	{
		//	Default to MWContextBrowser;
		cxType = MWContextBrowser;
	}

	//	A type came in that we're not ready to handle.
	//	Seems more code needs to be written....
	ASSERT(cxType != MWContextAny);
	if(cxType == MWContextAny)	{
		return(NULL);
	}

    //  Decide now wether or not to make the window initially visible.
    //  We decide by detecting wether or not we'll be enforcing a resize.
    //  We'll make the window visible after we're done sizing.
    //  Only works with 32 bits right now.
    BOOL bMakeVisible = TRUE;
	if(pChrome && ((pChrome->outw_hint && pChrome->outh_hint) || 
		(pChrome->w_hint && pChrome->h_hint)   ||
		pChrome->location_is_chrome ||
		pChrome->topmost || pChrome->bottommost)) {
        bMakeVisible = FALSE;
	}

	//  Must check if user wants a titlebar now since it affects the type of 
	//  window used during window creation.  If no titlebar, use popup type.
	BOOL bHideTitlebar = FALSE;
	BOOL bBorder = TRUE;
	if (pChrome && pChrome->hide_title_bar){
		bHideTitlebar = TRUE;
	}

	BOOL bDependent = FALSE;
	if (pOldContext && pChrome && pChrome->dependent) {	
	    bDependent = TRUE;
	}

	//	Take the template of the appropriate window type, and pull it up.
	CAbstractCX *pCX = NULL;

	// If there is a parent, get its hwnd
	CWinCX *pOldWinCX = NULL;
	if(pOldContext)
		pOldWinCX = WINCX(pOldContext);

	HWND	hParentHwnd = NULL;
	if(pOldWinCX && pOldWinCX->GetFrame() && pOldWinCX->GetFrame()->GetFrameWnd())
		hParentHwnd = pOldWinCX->GetFrame()->GetFrameWnd()->GetSafeHwnd();


	switch(cxType)	{
		case MWContextBrowser:
		case MWContextDialog:	{
			BOOL bPopup = FALSE;
			HWND hPopupParent = NULL;

			// if we are creating a modal window then we need to become a child of
			// the calling frame and of type WS_POPUP.  The values below are set to
			// tell the frame to create itself this way.
			if(hParentHwnd && pChrome && pChrome->is_modal)
			{
				bPopup = TRUE;
				hPopupParent = ::GetLastActivePopup(hParentHwnd);
			}

            CGenericDoc *pDoc = 
				(CGenericDoc *)theApp.m_ViewTmplate->OpenDocumentFile(NULL, bMakeVisible, 
								    bHideTitlebar, bDependent, bPopup, hPopupParent);
				if(pDoc != NULL)	{
					pCX = pDoc->GetContext();
				}
			}

			
			break;
		default:
			//	Please add your case statement.
			ASSERT(0);
			break;
	}

	//	If the context wasn't created, we can not continue.
	if(pCX == NULL)	{
		return(NULL);
	}

    // Hack to identify a NetHelp window
    pCX->m_bNetHelpWnd = bNetHelpWnd;
    
	// JavaScript can create windows which are lifetime-linked to their parent.
	// This is set here instead of in JS to avoid threading problems. 
    
	if (bDependent) {	
	    if (pOldContext->js_dependent_list == NULL)
		    pOldContext->js_dependent_list = XP_ListNew();
	    if (pOldContext->js_dependent_list == NULL)
		    return(NULL);
	    XP_ListAddObject(pOldContext->js_dependent_list, pCX->GetContext());
	    pCX->GetContext()->js_parent = pOldContext;
	}


	//	Get the context, and manually assign its type.
	//	The XP context that is.
	pCX->GetContext()->type = cxType;

	//	Assign the context name, if provided.
	if(pContextName != NULL)	{
		pCX->SetContextName(pContextName);
	}

	//	Copy over the same char set ID as the old context if we're a window.
	if(pCX->IsFrameContext() == TRUE && pOldContext != NULL)	{
		CWinCX *pWinCX = VOID2CX(pCX, CWinCX);
		if(cxType != MWContextDialog && !bNetHelpWnd) {
			pWinCX->GetFrame()->m_iCSID = INTL_DefaultDocCharSetID(pOldContext);
		} else {
			// use the client's default encoding for HTML dialogs (fix for #78838)
			pWinCX->GetFrame()->m_iCSID = INTL_CharSetNameToID(INTL_ResourceCharSet());
			if (pContextName != NULL) {
				if(strncmp(pContextName, VIEW_SOURCE_TARGET_WINDOW_NAME,
					strlen(VIEW_SOURCE_TARGET_WINDOW_NAME)) == 0)	{
					// but not for View Source which gets the old context char set ID 
					// (fix for #79072)
					pWinCX->GetFrame()->m_iCSID = INTL_DefaultDocCharSetID(pOldContext);
				}
				if(strncmp(pContextName, "%DocInfoWindow", 14) == 0)	{
					// %DocInfoWindow is from ns/lib/libnet/mkgeturl.c net_output_about_url()
					// Document Info gets the user-selectable default encoding
					pWinCX->GetFrame()->m_iCSID = INTL_DefaultWinCharSetID(NULL);
				}
			}
		}
	}

  #ifdef _WIN32
	if(sysInfo.m_bWin4 && bNetHelpWnd && pCX->IsFrameContext()) {
    
		CWinCX *pWinCX = VOID2CX(pCX, CWinCX);
        CFrameWnd *pFrame = pWinCX->GetFrame()->GetFrameWnd();
    
        if( pFrame ) {
           	HICON hIcon = theApp.LoadIcon( "IDR_NETHELP_ICON" );
           	pFrame->SetIcon( hIcon, TRUE );

           	hIcon = (HICON)::LoadImage( AfxFindResourceHandle( "IDR_NETHELP_ICON", RT_GROUP_ICON ), "IDR_NETHELP_ICON", IMAGE_ICON,
                                        GetSystemMetrics( SM_CXSMICON ), GetSystemMetrics( SM_CYSMICON ), LR_SHARED );
           	pFrame->SetIcon( hIcon, FALSE );
        }
    }
   #endif // _WIN32
       
	//	Pay special attention to the chrome attributes specified, if specified.
	//	If they aren't, attempt to use attributes from the old context passed in.
	if(pChrome != NULL && pCX->IsFrameContext() == TRUE)	{
		CWinCX *pWinCX = VOID2CX(pCX, CWinCX);
		
		LPCHROME pIChrome = pWinCX->GetFrame()->GetChrome();
		if(pIChrome) {
    		//	Url bar and directory buttons.
    		// make it so that we don't save toolbar information
    		pIChrome->SetSaveToolbarInfo(FALSE);
    		pIChrome->ShowToolbar(ID_LOCATION_TOOLBAR, pChrome->show_url_bar);
    		pIChrome->ShowToolbar(ID_PERSONAL_TOOLBAR, pChrome->show_directory_buttons);
    		pIChrome->ShowToolbar(ID_NAVIGATION_TOOLBAR, pChrome->show_button_bar);

    		//	Status bar!
    		LPNSSTATUSBAR pIStatusBar = NULL;
    		pIChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
    		if ( pIStatusBar ) {
    			pIStatusBar->Show( pChrome->show_bottom_status_bar );
    			pIStatusBar->Release();
    		}
		}

		//	Show the menu?
		if(pChrome->show_menu == FALSE)	{
            CMenu *pMenu = pWinCX->GetFrame()->GetFrameWnd()->GetMenu();
            HMENU hMenu = pMenu ? pMenu->m_hMenu : NULL;                    
			pWinCX->GetFrame()->GetFrameWnd()->SetMenu(NULL);
            if( hMenu && (theApp.m_ViewTmplate->m_hMenuShared != hMenu) )
            {
                pMenu->DestroyMenu();
            }
		}

		//	Need to recalc.
		pWinCX->GetFrame()->GetFrameWnd()->RecalcLayout();

		//	Size, of all things, of the viewing area only....
		//	Doesn't account for menu wrap or unwrap when getting smaller/bigger.
		if((pChrome->outw_hint && pChrome->outh_hint) || 
			(pChrome->w_hint &&pChrome->h_hint)   ||
			pChrome->location_is_chrome ||
			pChrome->topmost || pChrome->bottommost) {

			int32 lLeft=0;
			int32 lTop=0;
			int32 lWidth=0;
			int32 lHeight=0;

			CRect crFrame;
			pWinCX->GetFrame()->GetFrameWnd()->GetWindowRect(crFrame);
			CRect crView;
			::GetWindowRect(pWinCX->GetPane(), crView);
			
			//	Acount for the view border style.
			crView.InflateRect(-1 * sysInfo.m_iBorderWidth, -1 * sysInfo.m_iBorderHeight);

			int32 lWAdjust = pChrome->w_hint - (int32)crView.Width();
			int32 lHAdjust = pChrome->h_hint - (int32)crView.Height();

			if (pChrome->outw_hint>0 && pChrome->outh_hint>0) {
				lWidth = pChrome->outw_hint;
				lHeight = pChrome->outh_hint;
			}
			else if (pChrome->w_hint>0 && pChrome->h_hint>0) {
				lWidth = (int32)crFrame.Width() + lWAdjust;
				lHeight = (int32)crFrame.Height() + lHAdjust;
			}
			else {
				lWidth = (int32)crFrame.Width();
				lHeight = (int32)crFrame.Height();
			}

			if (pChrome->location_is_chrome) {
				lLeft = pChrome->l_hint;
				lTop = pChrome->t_hint;
			}
			else {
				lLeft = (int32)crFrame.left;
				lTop = (int32)crFrame.top;
			}
  
			//There is a security problem where the window size buffer can be 
			//overridden on Win95.  Using arbitrary max window size of 10000
			lWidth = lWidth > 10000 ? 10000 : lWidth;
			lHeight = lHeight > 10000 ? 10000 : lHeight;

			if (pChrome->topmost)
				pWinCX->GetFrame()->GetFrameWnd()->SetWindowPos(&CWnd::wndTopMost, CASTINT(lLeft), CASTINT(lTop), CASTINT(lWidth), CASTINT(lHeight), NULL);
			else if (pChrome->bottommost)
				pWinCX->GetFrame()->GetFrameWnd()->SetWindowPos(&CWnd::wndBottom, CASTINT(lLeft), CASTINT(lTop), CASTINT(lWidth), CASTINT(lHeight), NULL);
			else
				pWinCX->GetFrame()->GetFrameWnd()->MoveWindow(CASTINT(lLeft), CASTINT(lTop), CASTINT(lWidth), CASTINT(lHeight));

		//  Specific width and height information means that we should not save the sizes when closing down, regardless of context type.
		pWinCX->m_bSizeIsChrome = TRUE;
		}

		//	Set windows always on bottom.
		pWinCX->SetZOrder(pChrome->z_lock, pChrome->bottommost);

		//	Modality.
		if(pChrome->is_modal)	{
			//	Inform the context of its special status.
			pWinCX->GoModal(pOldContext);
		}

		//	Scroll bars.
		if(pChrome->show_scrollbar == FALSE)	{
			//	Turn them off.
			pWinCX->SetDynamicScrollBars(FALSE);
			pWinCX->ShowScrollBars(SB_BOTH, FALSE);
			pWinCX->SetAlwaysShowScrollBars(FALSE);
		}

		//	Resize.
		if(pChrome->allow_resize == FALSE)	{
			//	Disable resize....
			//	This is a truly gross and unholy option.
			pWinCX->EnableResize(FALSE);
		}

		//	Hotkeys
		if (pChrome->disable_commands) {	
			//	Disable hotkeys
		    
			pWinCX->DisableHotkeys(TRUE);
		}

		//	Close.
		if(pChrome->allow_close == FALSE)	{
			//	Disable closing....
			//	This is a truly gross and unholy option.
			//	This is reenabled in FE_DestroyContext regardless (i.e.
			//		can't be closed unless someone calls that API).
			pWinCX->EnableClose(FALSE);
		}

		//	Don't allow restricted target windows to be targeted
		//	by mail links and other functions which just grab 
		//	the nearest Browser context.
		if(pChrome->restricted_target)
		    pCX->GetContext()->restricted_target = TRUE;

		//	Close callback.
		//	Callback when the context is destroyed.
		pWinCX->CloseCallback(pChrome->close_callback, pChrome->close_arg);

		//	History copy.
		if(pChrome->copy_history && pOldContext != NULL)	{
			//	Copy the old context's history.
			SHIST_CopySession(pWinCX->GetContext(), pOldContext);
		}
	}
	else if(cxType == MWContextBrowser && pCX->IsFrameContext() == TRUE)	{
		CWinCX *pWinCX = VOID2CX(pCX, CWinCX);

		//	See if the calling context is a window context, and if so, emulate its
		//		settings.
		//	Otherwise, just take what we have up....
		if(pOldContext != NULL && pOldContext->type == MWContextBrowser)	{
			CWinCX *pOldCX = WINCX(pOldContext);

			LPCHROME pIChrome = pWinCX->GetFrame()->GetChrome();
			if(pIChrome) {
    			// Directory buttons, location box and all
    			// make it so that we don't save toolbar information
    			pIChrome->SetSaveToolbarInfo(FALSE);
    			pIChrome->SetToolbarStyle( theApp.m_pToolbarStyle );

    			pIChrome->ShowToolbar( ID_LOCATION_TOOLBAR, pOldCX->GetFrame()->m_bLocationBar );
    			pIChrome->ShowToolbar(ID_PERSONAL_TOOLBAR, pOldCX->GetFrame()->m_bStarter);
    			pIChrome->ShowToolbar(ID_NAVIGATION_TOOLBAR, pOldCX->GetFrame()->m_bShowToolbar);
			}
		}
	}

	//	If a URL struct was passed in, load it as the last thing to do.
	if(pUrl != NULL)	{
		int iFormatOut = FO_CACHE_AND_PRESENT;
		switch(cxType)	{
			case MWContextBrowser:
			case MWContextDialog:
				iFormatOut = FO_CACHE_AND_PRESENT;
				break;
			default:
				//	May be a different format out depending on context type.
				//	Please set as appropriate!
				ASSERT(0);
				break;
		}
		pCX->GetUrl(pUrl, iFormatOut);
	}

    //  If we hid the window earlier due to sizing information, time to make it come back.
    if(pCX->IsFrameContext() == TRUE && !bMakeVisible)	{
		VOID2CX(pCX, CWinCX)->GetFrame()->GetFrameWnd()->ShowWindow(SW_SHOW);
    }
    
    //	Return the XP context.
	return(pCX->GetContext());
}

//  Update current chrome in a context.
void FE_UpdateChrome(MWContext *pContext, Chrome *pChrome)
{
	TRACE("FE_UpdateChrome(%p, %p)\n", pContext, pChrome);

    //  API fulfillments.
    if(!pContext || !pChrome || !ABSTRACTCX(pContext) || !ABSTRACTCX(pContext)->IsFrameContext())   {
        return;
    }

    CWinCX *pWinCX = WINCX(pContext);
    if(!pWinCX->GetPane() || !pWinCX->GetFrame() || !pWinCX->GetFrame()->GetFrameWnd())    {
        //  Context isn't anatomically correct.
        return;
    }

    LPCHROME pIChrome = pWinCX->GetFrame()->GetChrome();
    if(pIChrome) {
    	//	Url bar and directory buttons.
    	pIChrome->SetSaveToolbarInfo(FALSE);
    	pIChrome->ShowToolbar(ID_LOCATION_TOOLBAR,  pChrome->show_url_bar );
    	pIChrome->ShowToolbar(ID_PERSONAL_TOOLBAR,  pChrome->show_directory_buttons );
    	pIChrome->ShowToolbar(ID_NAVIGATION_TOOLBAR, pChrome->show_button_bar);

    	//	Status bar!
    	LPNSSTATUSBAR pIStatusBar = NULL;
    	pIChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
    	if ( pIStatusBar ) {
    		pIStatusBar->Show( pChrome->show_bottom_status_bar );
    		pIStatusBar->Release();
    	}
    }

	//	Show the menu?
	if(pChrome->show_menu == FALSE)	{
        CMenu *pMenu = pWinCX->GetFrame()->GetFrameWnd()->GetMenu();
        HMENU hMenu = pMenu ? pMenu->m_hMenu : NULL;        
 		pWinCX->GetFrame()->GetFrameWnd()->SetMenu(NULL);
        if( hMenu && (theApp.m_ViewTmplate->m_hMenuShared != hMenu) )
        {
            pMenu->DestroyMenu();
        }
	}
	else    {                                                            
	    //  Get the menu back out of the template.
        CMenu *pMenu = pWinCX->GetFrame()->GetFrameWnd()->GetMenu();
        HMENU hMenu = pMenu ? pMenu->m_hMenu : NULL;
	    ::SetMenu(pWinCX->GetFrame()->GetFrameWnd()->GetSafeHwnd(), theApp.m_ViewTmplate->m_hMenuShared);
        if( hMenu && (theApp.m_ViewTmplate->m_hMenuShared != hMenu) )
        {
            pMenu->DestroyMenu();
        }
	}

	//	Need to recalc.
	pWinCX->GetFrame()->GetFrameWnd()->RecalcLayout();

	//	Size, of all things, of the viewing area only....
	//	Doesn't account for menu wrap or unwrap when getting smaller/bigger.
	if((pChrome->outw_hint && pChrome->outh_hint) || 
		(pChrome->w_hint &&pChrome->h_hint)   ||
		pChrome->location_is_chrome ||
		pChrome->topmost || pChrome->bottommost) {

		int32 lLeft=0;
		int32 lTop=0;
		int32 lWidth=0;
		int32 lHeight=0;

		CRect crFrame;
		pWinCX->GetFrame()->GetFrameWnd()->GetWindowRect(crFrame);
		CRect crView;
		::GetWindowRect(pWinCX->GetPane(), crView);
		
		//	Acount for the view border style.
		crView.InflateRect(-1 * sysInfo.m_iBorderWidth, -1 * sysInfo.m_iBorderHeight);

		int32 lWAdjust = pChrome->w_hint - (int32)crView.Width();
		int32 lHAdjust = pChrome->h_hint - (int32)crView.Height();

		if (pChrome->outw_hint>0 && pChrome->outh_hint>0) {
			lWidth = pChrome->outw_hint;
			lHeight = pChrome->outh_hint;
		}
		else if (pChrome->w_hint>0 && pChrome->h_hint>0) {
			lWidth = (int32)crFrame.Width() + lWAdjust;
			lHeight = (int32)crFrame.Height() + lHAdjust;
		}
		else {
			lWidth = (int32)crFrame.Width();
			lHeight = (int32)crFrame.Height();
		}

		if (pChrome->location_is_chrome) {
			lLeft = pChrome->l_hint;
			lTop = pChrome->t_hint;
		}
		else {
			lLeft = (int32)crFrame.left;
			lTop = (int32)crFrame.top;
		}

		//There is a security problem where the window size buffer can be 
		//overridden on Win95.  Using arbitrary max window size of 10000
		lWidth = lWidth > 10000 ? 10000 : lWidth;
		lHeight = lHeight > 10000 ? 10000 : lHeight;

		if (pChrome->topmost)
			pWinCX->GetFrame()->GetFrameWnd()->SetWindowPos(&CWnd::wndTopMost, CASTINT(lLeft), CASTINT(lTop), CASTINT(lWidth), CASTINT(lHeight), NULL);
		else if (pChrome->bottommost)
			pWinCX->GetFrame()->GetFrameWnd()->SetWindowPos(&CWnd::wndBottom, CASTINT(lLeft), CASTINT(lTop), CASTINT(lWidth), CASTINT(lHeight), NULL);
		else
			pWinCX->GetFrame()->GetFrameWnd()->MoveWindow(CASTINT(lLeft), CASTINT(lTop), CASTINT(lWidth), CASTINT(lHeight));

		//  Specific width and height information means that we should not save the sizes when closing down, regardless of context type.
		pWinCX->m_bSizeIsChrome = TRUE;
	}

	//	Set windows always on bottom.
	pWinCX->SetZOrder(pChrome->z_lock, pChrome->bottommost);

	//	Hotkeys
	pWinCX->DisableHotkeys(pChrome->disable_commands);

	//	Scroll bars.
	if(pChrome->show_scrollbar == FALSE)	{
		//	Turn them off.
		pWinCX->SetDynamicScrollBars(FALSE);
		pWinCX->ShowScrollBars(SB_BOTH, FALSE);
		pWinCX->SetAlwaysShowScrollBars(FALSE);
	}
	else {
	    pWinCX->SetDynamicScrollBars(TRUE);
	    pWinCX->RealizeScrollBars();
	}

	//	Resize.
	//	This is a truly gross and unholy option.
	pWinCX->EnableResize(pChrome->allow_resize);

	//	Close.
	//	This is a truly gross and unholy option.
	//	This is reenabled in FE_DestroyContext regardless (i.e.
	//		can't be closed unless someone calls that API).
    //  This also sets the closing attributes of all child contexts.
	pWinCX->EnableClose(pChrome->allow_close);

	//	Close callback.
    //	Callback when the context is destroyed.
	pWinCX->CloseCallback(pChrome->close_callback, pChrome->close_arg);

    //  We don't handle modality, as there is no parent context passed in.
}

//  Query current chrome in a context.
void FE_QueryChrome(MWContext *pContext, Chrome *pChrome)
{
	TRACE("FE_QueryChrome(%p, %p)\n", pContext, pChrome);

    //  API fulfillments.
    if(!pContext || !pChrome || !ABSTRACTCX(pContext) || !ABSTRACTCX(pContext)->IsFrameContext())   {
        return;
    }

    CWinCX *pWinCX = WINCX(pContext);
    if(!pWinCX->GetPane() || !pWinCX->GetDocument() || !pWinCX->GetFrame() || !pWinCX->GetFrame()->GetFrameWnd())    {
        //  Context isn't anatomically correct.
        return;
    }

    //  Clear the chrome struct, we'll refill in the relevant parts.
    memset((void *)pChrome, 0, sizeof(Chrome));

    LPCHROME pIChrome = pWinCX->GetFrame()->GetChrome();
    if(pIChrome) {
        //	Toolbar on or off?
        pChrome->show_button_bar = pIChrome->GetToolbarVisible(ID_NAVIGATION_TOOLBAR);

        //	Url bar and directory buttons.
        pChrome->show_url_bar = pIChrome->GetToolbarVisible(ID_LOCATION_TOOLBAR);
        pChrome->show_directory_buttons = pIChrome->GetToolbarVisible(ID_PERSONAL_TOOLBAR);

        //	The security bar?
        //  TODO: Fix me!
        pChrome->show_security_bar = TRUE;

        //	Status bar!
        LPNSSTATUSBAR pIStatusBar = NULL;
        pIChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
        if ( pIStatusBar ) {
    	    HWND hWnd = pIStatusBar->GetHWnd();
    	    pIStatusBar->Release();
    	    pChrome->show_bottom_status_bar = IsWindowVisible( hWnd );
        }
    }

    //	Show the menu?
    pChrome->show_menu = pWinCX->GetFrame()->GetFrameWnd()->GetMenu() == NULL ? FALSE : TRUE;

    //	Size, of all things, of the viewing area only....
	//	Acount for the view border style, too.
	CRect crView;
	::GetWindowRect(pWinCX->GetPane(), crView);
	CRect crFrame;
	pWinCX->GetFrame()->GetFrameWnd()->GetWindowRect(crFrame);
	crView.InflateRect(-1 * sysInfo.m_iBorderWidth, -1 * sysInfo.m_iBorderHeight);
	pChrome->outw_hint = (int32)crFrame.Width();
	pChrome->outh_hint = (int32)crFrame.Height();
	pChrome->w_hint = (int32)crView.Width();
	pChrome->h_hint = (int32)crView.Height();
	pChrome->l_hint = (int32)crFrame.left;
	pChrome->t_hint = (int32)crFrame.top;

	//Set to make sure the coords given are used.
	pChrome->location_is_chrome= TRUE;

	//	Scroll bars.
	pChrome->show_scrollbar = pWinCX->DynamicScrollBars();

	//	Resize.
	//	This is a truly gross and unholy option.
	pChrome->allow_resize = ((CGenericFrame *)pWinCX->GetFrame()->GetFrameWnd())->CanResize();

	//	Hotkeys.
	pChrome->disable_commands = ((CGenericFrame *)pWinCX->GetFrame()->GetFrameWnd())->HotkeysDisabled();

	//	ZLocked.
	pChrome->z_lock = ((CGenericFrame *)pWinCX->GetFrame()->GetFrameWnd())->IsZOrderLocked();

	//	Bottommost.
	pChrome->bottommost = ((CGenericFrame *)pWinCX->GetFrame()->GetFrameWnd())->IsBottommost();

	//	Close.
	//	This is a truly gross and unholy option.
	//  This will only report if the current context and its children can close, not if
	//      the actual frame can close if this context has a parent.
	pChrome->allow_close = pWinCX->GetDocument()->CanClose();

	//	Close callback is not handled correctly, as there may be multiples.

    //  We don't handle modality, as there is no parent context passed in to check against.
}

//	Destroy a window/context.
void FE_DestroyWindow(MWContext *pContext)
{
	TRACE("FE_DestroyWindow(%p)\n", pContext);

	ASSERT(pContext);
	if(pContext != NULL)	{
		//	Make sure that this is a CWinCX.
		ASSERT(ABSTRACTCX(pContext)->IsFrameContext());
		if(ABSTRACTCX(pContext)->IsWindowContext())	{
			CWinCX *pCX = WINCX(pContext);

			//	Make sure there's a frame to close.
			if(pCX->GetFrame()->GetFrameWnd())	{
				//	Reenable closing.
				pCX->EnableClose(TRUE);

				//	Send it a close message.
				pCX->GetFrame()->GetFrameWnd()->SendMessage(WM_CLOSE);
			}
		}
	}
}

//  Fill in left top position of frame.
//  Breaks down on the minimized maximized scenario.
void FE_GetWindowPosition(MWContext *pContext, int32 *pX, int32 *pY)
{
	TRACE("FE_GetWindowPosition(%p, %p, %p)\n", pContext, pX, pY);

    //  Initialize.
    if(pX)  {
        *pX = 0;
    }
    if(pY)  {
        *pY = 0;
    }
    
    if(pContext && ABSTRACTCX(pContext) && ABSTRACTCX(pContext)->IsFrameContext() && (pX || pY))	{
		//	Make sure there's a frame.
		CWinCX *pCX = WINCX(pContext);
		if(pCX->GetFrame() && pCX->GetFrame()->GetFrameWnd())	{
            CRect cr;
            pCX->GetFrame()->GetFrameWnd()->GetWindowRect(cr);

            if(pX)  {
                *pX = cr.left;
            }
            if(pY)  {
                *pY = cr.top;
            }
		}
	}
}

//  Set left top position of frame.
//  Breaks down on the minimized maximized scenario.
void FE_SetWindowPosition(MWContext *pContext, int32 lX, int32 lY)
{
	TRACE("FE_SetWindowPosition(%p, %ld, %ld)\n", pContext, lX, lY);

    if(pContext && ABSTRACTCX(pContext) &&
        ABSTRACTCX(pContext)->IsFrameContext())	{
		//	Make sure there's a frame.
		CWinCX *pCX = WINCX(pContext);
		if(pCX->GetFrame() && pCX->GetFrame()->GetFrameWnd())	{
            pCX->GetFrame()->GetFrameWnd()->SetWindowPos(NULL, CASTINT(lX),
                CASTINT(lY), 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
		}
	}
}

//  Return screen dimensions.
void FE_GetScreenSize(MWContext *pContext, int32 *pX, int32 *pY)
{
    //  Initialize
    if(pX)  {
        *pX = sysInfo.m_iScreenWidth;
    }
    if(pY)  {
        *pY = sysInfo.m_iScreenHeight;
    }
}

//  Return available screen rectangle.
void FE_GetAvailScreenRect(MWContext *pContext, int32 *pX, int32 *pY, 
						int32 *pLeft, int32 *pTop)
{
    RECT *pRect = NULL;
#ifdef WIN32
    APPBARDATA abd;
    UINT state;

    XP_BZERO(&abd, sizeof abd);
    abd.cbSize = sizeof(abd);
    abd.hWnd = NULL;

    state = SHAppBarMessage(ABM_GETSTATE, &abd);

    if ((state & ABS_ALWAYSONTOP) && !(state & ABS_AUTOHIDE))
	if (!SHAppBarMessage(ABM_GETTASKBARPOS, &abd))
	    ASSERT(0);
    pRect = &abd.rc;
#endif
    //  Initialize
    if(pX)  {
        *pX = sysInfo.m_iScreenWidth;
	if (pRect != NULL) {
	    if (pRect->top < 0 && pRect->bottom > sysInfo.m_iScreenHeight) {
		//Docked left
		if (pRect->left < 0)
		    *pX -= pRect->right;
		//Docked right
		if (pRect->right > sysInfo.m_iScreenWidth)
		    *pX = pRect->left;
	    }
	}
    }
    if(pY)  {
        *pY = sysInfo.m_iScreenHeight;
	if (pRect != NULL) {
	    if (pRect->left < 0 && pRect->right > sysInfo.m_iScreenWidth) {
		//Docked top
		if (pRect->top < 0);
		    *pY -= pRect->bottom;
		//Docked bottom
		if (pRect->bottom > sysInfo.m_iScreenHeight)
		    *pY = pRect->top;
	    }
	}
    }
    if(pLeft)  {
        *pLeft = 0;
	if (pRect != NULL) {
	    if (pRect->top < 0 && pRect->left < 0) {
		//Docked left
		if (pRect->bottom > sysInfo.m_iScreenHeight)
		    *pLeft = pRect->right;
	    }
	}
    }
    if(pTop)  {
        *pTop = 0;
	if (pRect != NULL) {
	    if (pRect->top < 0 && pRect->left < 0) {
		//Docked top
		if (pRect->right > sysInfo.m_iScreenWidth)
		    *pTop = pRect->bottom;
	    }
	}
    }
}

//  Return color depth.
void FE_GetPixelAndColorDepth(MWContext *pContext, int32 *pixel, int32 *pallette)
{
    TRACE("FE_GetPixelAndColorDepth(%p, %p)\n", pixel, pallette);

    *pixel = *pallette = 0;

    if(ABSTRACTCX(pContext)->IsDestroyed() == FALSE) {

	CWinCX *pWinCX = WINCX(pContext);

	HDC pDC = pWinCX->GetContextDC();

	//  Figure out the depth of the CDC.
	*pixel = ::GetDeviceCaps(pDC, BITSPIXEL);

	//  Check for pallette existence
        if ((::GetDeviceCaps(pDC, RASTERCAPS) & RC_PALETTE) != 0)
	    *pallette = ::GetDeviceCaps(pDC, COLORRES);
	else
	    *pallette = *pixel;

	pWinCX->ReleaseContextDC(pDC);
    }

}








BOOL GetIntelCPUSpeed(int32 &processorSpeed );
BOOL GetIntelCPUInfoViaCPUID(char *systemArchitecture,
			int32 &processorType, int32 &processorFamily, 
			int32 &processorModel, int32 &processorStepping, int32 &processorFeatures,
			CString &vendor);
#ifndef _WIN32
unsigned short GetBaseMemC();
unsigned short GetExpMemC();
#endif
CString csUNKNOWN = "Unknown";
CString csINTEL = "Intel";
CString csMIPS = "Mips";
CString csALPHA = "Alpha";
CString csPPC = "PowerPC";

CString csARCHITECTURE = "ARCHITECTURE";
CString csFAMILY = "FAMILY";
CString csMODEL = "MODEL";
CString csSTEPPING = "STEPPING";
CString csTYPE = "TYPE";
CString csFEATURES = "FEATURES";
CString csPASS = "PASS";
CString csREVISION = "REVISION";
CString csVERSION = "VERSION";
CString csVENDOR = "VENDOR";

// VARIABLE STRUCTURE DEFINITIONS //////////////////////////////
struct FREQ_INFO
{
	unsigned long in_cycles;	// Internal clock cycles during
								//   test
								
	unsigned long ex_ticks;		// Microseconds elapsed during 
								//   test
								
	unsigned long raw_freq;		// Raw frequency of CPU in MHz
	
	unsigned long norm_freq;	// Normalized frequency of CPU
								//   in MHz.
};

struct FREQ_INFO cpuspeed(int clocks);
// Function Prototypes /////////////////////////////////////////

/***************************************************************
* WORD wincpuidsupport()
* =================================
* Wincpuidsupport() tells the caller whether the host processor
* supports the CPUID opcode or not.
*
* Inputs: none
*
* Returns:
*  1 = CPUID opcode is supported
*  0 = CPUID opcode is not supported
***************************************************************/

WORD wincpuidsupport();


/***************************************************************
* WORD wincpuid()
* ===============
* This routine uses the standard Intel assembly code to 
* determine what type of processor is in the computer, as
* described in application note AP-485 (Intel Order #241618).
* Wincpuid() returns the CPU type as an integer (that is, 
* 2 bytes, a WORD) in the AX register.
*
* Returns:
*  0 = 8086/88
*  2 = 80286
*  3 = 80386
*  4 = 80486
*  5 = Pentium(R) Processor
*  6 = PentiumPro(R) Processor
*  7 or higher = Processor beyond the PentiumPro6(R) Processor
*
*  Note: This function also sets the global variable clone_flag
***************************************************************/
WORD  wincpuid();


/***************************************************************
* WORD wincpuidext()
* ==================
* Similar to wincpuid(), but returns more data, in the order
* reflecting the actual output of a CPUID instruction execution:
*
* Returns:
* AX(15:14) = Reserved (mask these off in the calling code 
*				before using)
* AX(13:12) = Processor type (00=Standard OEM CPU, 01=OverDrive,
*				10=Dual CPU, 11=Reserved)
* AX(11:8)  = CPU Family (the same 4-bit quantity as wincpuid())
* AX(7:4)   = CPU Model, if the processor supports the CPUID 
*				opcode; zero otherwise
* AX(3:0)   = Stepping #, if the processor supports the CPUID 
*				opcode; zero otherwise
*
*  Note: This function also sets the global variable clone_flag
***************************************************************/
WORD wincpuidext(CString &vendorName);


/***************************************************************
* DWORD wincpufeatures()
* ======================
* Wincpufeatures() returns the CPU features flags as a DWORD 
*    (that is, 32 bits).
*
* Inputs: none
*
* Returns:
*   0 = Processor which does not execute the CPUID instruction.
*          This includes 8086, 8088, 80286, 80386, and some 
*		   older 80486 processors.                       
*
* Else
*   Feature Flags (refer to App Note AP-485 for description).
*      This DWORD was put into EDX by the CPUID instruction.
*
*	Current flag assignment is as follows:
*
*		bit31..10   reserved (=0)
*		bit9=1      CPU contains a local APIC (iPentium-3V)
*		bit8=1      CMPXCHG8B instruction supported
*		bit7=1      machine check exception supported
*		bit6=0      reserved (36bit-addressing & 2MB-paging)
*		bit5=1      iPentium-style MSRs supported
*		bit4=1      time stamp counter TSC supported
*		bit3=1      page size extensions supported
*		bit2=1      I/O breakpoints supported
*		bit1=1      enhanced virtual 8086 mode supported
*		bit0=1      CPU contains a floating-point unit (FPU)
*
*	Note: New bits will be assigned on future processors... see
*         processor data books for updated information
*
*	Note: This function also sets the global variable clone_flag
***************************************************************/
DWORD wincpufeatures(CString &vendorName);



#ifndef _WIN32
unsigned short GetBaseMemC()
{
	// get memory below 1-MB boundary
	// using run-time library functions
	unsigned short base;
	outp( 0x70, 0x15 );
	base = inp( 0x71 ); //retrieve low byte
	outp( 0x70, 0x16 ); 
	base += inp(0x71) << 8; //retieve high byte,
							//shift and add to base
	return base;  // return K's of base memory
}

unsigned short GetExpMemC()
{
	// get memory above 1-MB boundary
	// using run-time library functions
	unsigned short extend;
	outp( 0x70, 0x17 );
	base = inp( 0x71 ); //retrieve low byte
	outp( 0x70, 0x18 ); 
	base += inp(0x71) << 8; //retieve high byte,
							//shift and add to extend
	return extend;  // return K's of expansion memory		
}
#endif

int32 FE_SystemRAM( void )
{
	int32 systemRAM = -1;
#ifdef _WIN32
	MEMORYSTATUS MemoryStatus;

	memset( &MemoryStatus, 0, sizeof(MEMORYSTATUS));
	MemoryStatus.dwLength = sizeof(MEMORYSTATUS);

	GlobalMemoryStatus( &MemoryStatus );
	//return physical memory size in K
	systemRAM = MemoryStatus.dwTotalPhys / 1024 ;
#else
	//return physical memory size in K
	int32 expansionMem = GetExpMemC();
	int32 baseMem = GetBaseMemC();
	systemRAM = expansionMem + baseMem;
#endif
return systemRAM;
}

int32 FE_SystemClockSpeed( void )
{
	int32 clockSpeed = -1;
#ifdef _WIN32
	OSVERSIONINFO OSVersionInfo;
	memset( &OSVersionInfo, 0, sizeof(OSVERSIONINFO));
	OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if(GetVersionEx( &OSVersionInfo ))
	{
		if(OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT ||
			OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) //Windows 9x
		{
			SYSTEM_INFO SystemInfo;
			GetSystemInfo( &SystemInfo );
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL )
			{
				if(!GetIntelCPUSpeed(clockSpeed))
					clockSpeed = -1;
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ALPHA )
			{
				return -1;
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_MIPS )
			{
				return -1;
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_PPC )
			{
				return -1;
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_UNKNOWN )
				return -1;
			else
			{
				ASSERT(FALSE);
				return -1;
			}
		}
		else //if Win32s we must be running on Win3.x so Intel architecture
		if(	OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32s )
		{
			if(!GetIntelCPUSpeed(clockSpeed))
				clockSpeed = -1;
		}
		else
		{
			ASSERT(FALSE);
			return -1;
		}
	}
#else
	if(!GetIntelCPUSpeed(clockSpeed))
		clockSpeed = -1;
#endif
	return clockSpeed;
}

BOOL GetIntelCPUSpeed(int32 &processorSpeed )
{
	BOOL bRet = FALSE;
	// VARIABLE STRUCTURE DEFINITIONS //////////////////////////////
	struct FREQ_INFO cpu_speed;		// Return variable 
										//   structure for 
										//   cpuspeed 
										//   routine
			
			
	cpu_speed = cpuspeed(0);
				
	if ( cpu_speed.in_cycles == 0 && cpu_speed.ex_ticks  == 0 )
	{
		/*			 
		 sprintf(buf,
			 	"This processor cannot be accurately "
			 	"timed with this program.\n The "
			 	"processor could be"
			 	"below 80386 level.");
		 MessageBox(NULL,buf,"error", MB_ICONINFORMATION );
		 */
		bRet = FALSE;
	}
	else
	{
		/*
		sprintf(buf,
				"Clock Cycles: %lu cycles\n"
				"Elapsed Time: %luus\n"
				"Raw Clock Frequency: %luMHz\n"
				"Normalized Frequency: %luMHz",
				cpu_speed.in_cycles,
				cpu_speed.ex_ticks,
				cpu_speed.raw_freq,
				cpu_speed.norm_freq);          

		MessageBox(NULL,buf,"32-bit cpuspeed", 
					MB_ICONINFORMATION );
		*/
		processorSpeed = cpu_speed.norm_freq;
		bRet = TRUE;
	}
	return bRet;
}


BOOL GetIntelCPUInfoViaCPUID(CString &systemArchitecture,
			int32 &processorType, int32 &processorFamily, 
			int32 &processorModel, int32 &processorStepping, int32 &processorFeatures,
			CString &vendor)
{
	//Intel Only!!!
	systemArchitecture = csINTEL;
	int32 processorExtensions = 0;
	processorExtensions = wincpuidext(vendor);

	processorStepping = processorExtensions & 0x0f;		
	processorModel = (processorExtensions & 0x0f0) >> 4;
	processorFamily = (processorExtensions & 0x0f00) >> 8;
	processorType = (processorExtensions & 0x03000) >> 12;

	processorFeatures = wincpufeatures(vendor);

	return TRUE;
}

char *CreateNonIntelCPUInfoString(CString &systemArchitecture,
								  int32 &processorModel, int32 &processorStepping)
{
	CString tmpStr;
	CString CPUInfoString;
	ASSERT( !systemArchitecture.IsEmpty() );

	if( !systemArchitecture.IsEmpty() )
	{
		CPUInfoString = csARCHITECTURE + "=";
		CPUInfoString += systemArchitecture;
	}

	if( systemArchitecture == csALPHA )
	{
		if( processorModel > -1 )
		{
			if( !CPUInfoString.IsEmpty() )
				CPUInfoString += ";";
			CPUInfoString += csMODEL + "=A";
			tmpStr.Format("%.2X", processorModel );
			CPUInfoString += tmpStr;
		}
		if( processorStepping > -1 )
		{
			if( !CPUInfoString.IsEmpty() )
				CPUInfoString += ";"; 
			CPUInfoString += csPASS + "=";
			tmpStr.Format("%.2X", processorStepping);
			CPUInfoString += tmpStr;
		}
	}
	else
	if( systemArchitecture == csMIPS )
	{
		if( processorModel > -1 )
		{
			if( !CPUInfoString.IsEmpty() )
				CPUInfoString += ";";
			CPUInfoString += csREVISION + "=";
			tmpStr.Format("%.2X", processorModel);
			CPUInfoString += tmpStr;
		}
	}
	else
	if( systemArchitecture == csPPC )
	{
		if( processorModel > -1 )
		{
			if( !CPUInfoString.IsEmpty() )
				CPUInfoString += ";";
			CPUInfoString += csVERSION + "=";
			tmpStr.Format("%.2X", processorModel);
			CPUInfoString += tmpStr;
		}

		if( processorStepping > -1 )
		{		
			CPUInfoString += ".";
			tmpStr.Format("%.2X", processorStepping);
			CPUInfoString += tmpStr;
		}
	}
	char * pstr = NULL;
	int len = CPUInfoString.GetLength();
	if(len > 0 )
	{
		pstr = new char[ len + 1];
		if( pstr != NULL )
			strcpy( pstr, CPUInfoString );
	}	
	return pstr;
}
			
			
char *CreateIntelCPUInfoString(CString &systemArchitecture,
			int32 processorType, int32 processorFamily, 
			int32 processorModel, int32 processorStepping, int32 processorFeatures,
			CString &vendor)
{
	CString tmpStr;
	CString CPUInfoString;
	ASSERT( !systemArchitecture.IsEmpty() );

	if( !systemArchitecture.IsEmpty() )
	{
		CPUInfoString = csARCHITECTURE + "=";
		CPUInfoString += systemArchitecture;
	}
	if( processorType > -1 )
	{
		if( !CPUInfoString.IsEmpty() )
			CPUInfoString += ";";
		CPUInfoString += csTYPE + "=";
		tmpStr.Format("%d", processorType);
		CPUInfoString += tmpStr;
	}
	if( processorFamily > -1 )
	{
		if( !CPUInfoString.IsEmpty() )
			CPUInfoString += ";";
		CPUInfoString += csFAMILY + "=";
		tmpStr.Format("%d", processorFamily);
		CPUInfoString += tmpStr;
	}
	if( processorModel > -1 )
	{
		if( !CPUInfoString.IsEmpty() )
			CPUInfoString += ";";
		CPUInfoString += csMODEL + "=";
		tmpStr.Format("%d", processorModel);
		CPUInfoString += tmpStr;
	}
	if( processorStepping > -1 )
	{
		if( !CPUInfoString.IsEmpty() )
			CPUInfoString += ";";
		CPUInfoString += csSTEPPING + "=";
		tmpStr.Format("%d", processorStepping);
		CPUInfoString += tmpStr;
	}
	if( processorFeatures > -1 )
	{
		if( !CPUInfoString.IsEmpty() )
			CPUInfoString += ";";
		CPUInfoString += csFEATURES + "=";
		tmpStr.Format("0x%x", processorFeatures);
		CPUInfoString += tmpStr;
	}

	if( !CPUInfoString.IsEmpty() )
		CPUInfoString += ";";
	CPUInfoString += csVENDOR + "=";
	if( !vendor.IsEmpty() )
		CPUInfoString += vendor;
	else
		CPUInfoString += csUNKNOWN;

	char * pstr = NULL;
	int len = CPUInfoString.GetLength();
	if(len > 0 )
	{
		pstr = new char[ len + 1];
		if( pstr != NULL )
			strcpy( pstr, CPUInfoString );
	}
	
	return pstr;
}

char *FE_SystemCPUInfo(void)
{
	CString systemArchitecture;
	int32 processorType = -1; 
	int32 processorFamily = -1; 
	int32 processorModel = -1; 
	int32 processorStepping = -1; 
	int32 processorFeatures = -1;
	CString vendor;

#ifdef _WIN32
	OSVERSIONINFO OSVersionInfo;
	memset( &OSVersionInfo, 0, sizeof(OSVERSIONINFO));
	OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if(GetVersionEx( &OSVersionInfo ))
	{
		//if Win32s is the platform
		if(	OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32s )
		{
				if(GetIntelCPUInfoViaCPUID(systemArchitecture,	
							processorType, processorFamily, processorModel, 
							processorStepping, processorFeatures, vendor ))
					return CreateIntelCPUInfoString(systemArchitecture,	
							processorType, processorFamily, processorModel, 
							processorStepping, processorFeatures, vendor );
				else
					return NULL;
		}
		else
		if(OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT ||
			OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) //Windows 9x
		{
			SYSTEM_INFO SystemInfo;
			GetSystemInfo( &SystemInfo );
			processorFamily = SystemInfo.wProcessorLevel;
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL )
			{	
				if(OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ) 
				{	//Windows 9x
					if(GetIntelCPUInfoViaCPUID(systemArchitecture,	
							processorType, processorFamily, processorModel, 
							processorStepping, processorFeatures, vendor ))
						return CreateIntelCPUInfoString(systemArchitecture,	
							processorType, processorFamily, processorModel, 
							processorStepping, processorFeatures, vendor );
					else
						return NULL;
				}
				else //Windows NT on Intel
				{
					//Might as well use CPUID; it provides a little more information
					if(GetIntelCPUInfoViaCPUID(systemArchitecture,	
							processorType, processorFamily, processorModel, 
							processorStepping, processorFeatures, vendor ))
						return CreateIntelCPUInfoString(systemArchitecture,	
							processorType, processorFamily, processorModel, 
							processorStepping, processorFeatures, vendor );
					else
						return NULL;
					
	//				strcpy(systemArchitecture, INTEL);
	//				if( SystemInfo.dwProcessorType == PROCESSOR_INTEL_386 ||
	//					SystemInfo.dwProcessorType == PROCESSOR_INTEL_486 )
	//				{
	//					//wProcessor type of form xxyz; if xx != 0xFF
	//					if( ((SystemInfo.wProcessorRevision >> 8) & 0xff ) != 0xff )
	//					{
	//						//then xx + 'A' is the stepping letter						
	//						processorStepping = ((SystemInfo.wProcessorRevision >> 8) & 0xff) + 'A';
	//						//this is actually the minor stepping
	//						processorModel = (SystemInfo.wProcessorRevision & 0x0ff);
	//					}
	//					else
	//					if( ((SystemInfo.wProcessorRevision >> 8) & 0xff ) == 0xff )
	//					{
	//						processorModel = (SystemInfo.wProcessorRevision & 0x0f0) >> 4;
	//						processorStepping = (SystemInfo.wProcessorRevision & 0x0f);
	//					}
	//				}
	//				else  //processor must be > x486
	//				{
	//					processorModel = (SystemInfo.wProcessorRevision & 0x0ff00) >> 8;
	//					processorStepping = (SystemInfo.wProcessorRevision & 0x0ff);
	//				}
				}
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ALPHA )
			{
				systemArchitecture = csALPHA;
				processorModel = (SystemInfo.wProcessorRevision & 0x0ff00) >> 8;
				//"Pass" on Alpha; display as "Model 'A'+xx, Pass yy"
				processorStepping = (SystemInfo.wProcessorRevision & 0x0ff);
				return CreateNonIntelCPUInfoString(systemArchitecture, 
											processorModel, processorStepping);
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_MIPS )
			{
				systemArchitecture = csMIPS;
				//"Revision Number" on MIPS
				processorModel = (SystemInfo.wProcessorRevision & 0x0ff);
				return CreateNonIntelCPUInfoString(systemArchitecture, 
											processorModel, processorStepping);
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_PPC )
			{
				systemArchitecture = csPPC;
				//display as xx.yy on PPC
				processorModel = (SystemInfo.wProcessorRevision & 0x0ff00) >> 8;
				processorStepping = (SystemInfo.wProcessorRevision & 0x0ff);
				return CreateNonIntelCPUInfoString(systemArchitecture, 
											processorModel, processorStepping);
			}
			else
			if( SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_UNKNOWN )
			{
				systemArchitecture = csUNKNOWN;
				return CreateNonIntelCPUInfoString(systemArchitecture, 
											processorModel, processorStepping);
			}
			else
			{
				ASSERT(FALSE);
				systemArchitecture = csUNKNOWN;
				return CreateNonIntelCPUInfoString(systemArchitecture, 
											processorModel, processorStepping);
			}			
		}
		ASSERT(FALSE); //Unknown operating system
	}
	return NULL;
#else //WIN16
	if(GetIntelCPUInfoViaCPUID(systemArchitecture, processorType, 
							processorFamily, processorModel, processorStepping,
							processorFeatures, vendor ))
		return CreateIntelCPUInfoString(systemArchitecture,	
							processorType, processorFamily, processorModel, 
							processorStepping, processorFeatures, vendor );
	else
		return NULL;
#endif
}








// CONSTANT DEFINITIONS ////////////////////////////////////////
#define CLONE_MASK		0x8000	// Mask to be 'OR'ed with proc-
#define MAXCLOCKS		150		// Maximum number of cycles per
								//   BSF instruction
	// ACCURACY AFFECTING CONSTANTS ////////////////////////////
#define ITERATIONS		4000	// Number of times to repeat BSF
								//   instruction in samplings.
								//   Initially set to 4000.

#define MAX_TRIES		20		// Maximum number of samplings
								//   to allow before giving up
								//   and returning current 
								//   average. Initially set to
								//   20.
	
#define TOLERANCE		1		// Number of MHz to allow
								//   samplings to deviate from
								//   average of samplings.
								//   Initially set to 2.

#define	SAMPLINGS		10		// Number of BSF sequence 
								//   samplings to make.
								//   Initially set to 10.


typedef unsigned short ushort;
typedef unsigned long  ulong;

// Global Variable /////////////////////////////////////////////
int clone_flag;				// Flag to show whether processor
							//   is non-Intel




// OPCODE DEFINITIONS //////////////////////////////////////////
#define CPU_ID _asm _emit 0x0f _asm _emit 0xa2 	
										// CPUID instruction

#define RDTSC  _asm _emit 0x0f _asm _emit 0x31	
										// RDTSC instruction

// Private Function Declarations ///////////////////////////////

/***************************************************************
* static WORD check_clone()
*
* Inputs: none
*
* Returns:
*   1      if processor is clone (limited detection ability)
*   0      otherwise
***************************************************************/
static WORD check_clone();


/***************************************************************
* static WORD check_8086()
*
* Inputs: none
*
* Returns: 
*   0      if processor 8086
*   0xffff otherwise
***************************************************************/
static WORD check_8086();


/***************************************************************
* static WORD check_80286()
*
* Inputs: none
*
* Returns:
*   2      if processor 80286
*   0xffff otherwise
***************************************************************/
static WORD check_80286();


/***************************************************************
* static WORD check_80386()
*
* Inputs: none
*
* Returns:
*   3      if processor 80386
*   0xffff otherwise
***************************************************************/
static WORD check_80386();


/***************************************************************
* static WORD check_IDProc()
* ==========================
* Check_IDProc() uses the CPUID opcode to find the family type
* of the host processor.
*
* Inputs: none
*
* Returns:
*  CPU Family (i.e. 4 if Intel 486, 5 if Pentium(R) Processor)
*
*  Note: This function also sets the global variable clone_flag
***************************************************************/
static WORD check_IDProc();

//end of prototypes //////////////////////////////////////////////


#define ROUND_THRESHOLD		6

// Tabs set at 4
static struct FREQ_INFO GetRDTSCCpuSpeed();
static struct FREQ_INFO GetBSFCpuSpeed(ulong cycles);

// Number of cycles needed to execute a single BSF instruction.
//    Note that processors below i386(tm) are not supported.
static ulong processor_cycles[] = {
	00,  00,  00, 115, 47, 43, 
	38,  38,  38, 38,  38, 38, 
};

/***************************************************************
* wincpuidsupport()
*
* Inputs: none
*
* Returns:
*  1 = CPUID opcode is supported
*  0 = CPUID opcode is not supported
***************************************************************/

WORD wincpuidsupport() {
	int cpuid_support = 1;

	_asm {
        pushfd					// Get original EFLAGS
		pop		eax
		mov 	ecx, eax
        xor     eax, 200000h	// Flip ID bit in EFLAGS
        push    eax				// Save new EFLAGS value on
        						//   stack
        popfd					// Replace current EFLAGS value
        pushfd					// Get new EFLAGS
        pop     eax				// Store new EFLAGS in EAX
        xor     eax, ecx		// Can not toggle ID bit,
        jnz     support			// Processor=80486
		
		mov cpuid_support,0		// Clear support flag
support:
      }
	
	return cpuid_support;

} // wincpuidsupport()


/***************************************************************
* wincpuid()
*
* Inputs: none
*
* Returns:
*  0 = 8086/88
*  2 = 80286
*  3 = 80386
*  4 = 80486
*  5 = Pentium(R) Processor
*  6 = PentiumPro(R) Processor
*  7 or higher = Processor beyond the PentiumPro6(R) Processor
*
*  Note: This function also sets the global variable clone_flag
***************************************************************/

WORD wincpuid() {

	WORD cpuid;
	
	if ( wincpuidsupport() ) 	// Determine whether CPUID 
								//   opcode is supported
		cpuid=check_IDProc();

	else {
		
		clone_flag=check_clone();
	
		cpuid=check_8086();			// Will return FFFFh or 0
		if (cpuid == 0) goto end;
	
    	cpuid=check_80286();       	// Will return FFFFh or 2
		if (cpuid == 2) goto end;

    	cpuid=check_80386();       	// Will return FFFFh or 3
		if (cpuid == 3) goto end;    // temporarily commented out.
        
        cpuid=4;		// If the processor does not support CPUID,
        				//  is not an 8086, 80286, or 80386, assign
        				//  processor to be an 80486
	}

end:
	if (clone_flag)
		cpuid = cpuid | CLONE_MASK;	// Signify that a clone has been
									//   detected by setting MSB high 

   	return cpuid;

} // wincpuid ()


/***************************************************************
* wincpufeatures()
*
* Inputs: none
*
* Returns:
*   0 = Processor which does not execute the CPUID instruction.
*          This includes 8086, 8088, 80286, 80386, and some 
*		   older 80486 processors.                       
*
* Else
*   Feature Flags (refer to App Note AP-485 for description).
*      This DWORD was put into EDX by the CPUID instruction.
*
*	Current flag assignment is as follows:
*
*		bit31..10   reserved (=0)
*		bit9=1      CPU contains a local APIC (iPentium-3V)
*		bit8=1      CMPXCHG8B instruction supported
*		bit7=1      machine check exception supported
*		bit6=0      reserved (36bit-addressing & 2MB-paging)
*		bit5=1      iPentium-style MSRs supported
*		bit4=1      time stamp counter TSC supported
*		bit3=1      page size extensions supported
*		bit2=1      I/O breakpoints supported
*		bit1=1      enhanced virtual 8086 mode supported
*		bit0=1      CPU contains a floating-point unit (FPU)
*
*	Note: New bits will be assigned on future processors... see
*         processor data books for updated information
*
*	Note: This function also sets the global variable clone_flag
***************************************************************/

DWORD wincpufeatures(CString &vendorName) {

	int i=0;
	DWORD cpuff=0x00000000;
	BYTE vendor_id[]="------------";
	BYTE intel_id[]="GenuineIntel";

	if ( wincpuidsupport() ) {

_asm {      

		xor     eax, eax		// Set up for CPUID instruction
        
		CPU_ID                  // Get and save vendor ID

        mov     dword ptr vendor_id, ebx
        mov     dword ptr vendor_id[+4], edx
        mov     dword ptr vendor_id[+8], ecx
}
vendorName = vendor_id;

for (i=0;i<12;i++)
{
	if (!(vendor_id[i]==intel_id[i]))
		clone_flag = 1;    
}

_asm {
         
		cmp     eax, 1			// Make sure 1 is valid input 
        						//   for CPUID
        
        jl      end_cpuff		// If not, jump to end
        xor     eax, eax
        inc		eax
        CPU_ID					// Get family/model/stepping/
        						//   features

		mov		cpuff, edx

end_cpuff:
		mov		eax, cpuff
      }
	}

	return cpuff;

} // wincpufeatures()


/***************************************************************
* wincpuidext()
*
* Inputs: none
*
* Returns:
* AX(15:14) = Reserved (mask these off in the calling code 
*				before using)
* AX(13:12) = Processor type (00=Standard OEM CPU, 01=OverDrive,
*				10=Dual CPU, 11=Reserved)
* AX(11:8)  = CPU Family (the same 4-bit quantity as wincpuid())
* AX(7:4)   = CPU Model, if the processor supports the CPUID 
*				opcode; zero otherwise
* AX(3:0)   = Stepping #, if the processor supports the CPUID 
*				opcode; zero otherwise
*
*  Note: This function also sets the global variable clone_flag
***************************************************************/

WORD wincpuidext(CString &vendorName) {

		int i=0;
		WORD cpu_type=0x0000;
		WORD cpuidext=0x0000;
		BYTE vendor_id[]="------------";
		BYTE intel_id[]="GenuineIntel";

	if ( wincpuidsupport() ) {

_asm {      

		xor     eax, eax		// Set up for CPUID instruction
        
		CPU_ID                  // Get and save vendor ID

		mov     dword ptr vendor_id, ebx
		mov     dword ptr vendor_id[+4], edx
		mov     dword ptr vendor_id[+8], ecx
}
vendorName = vendor_id;

for (i=0;i<12;i++)
{
	if (!(vendor_id[i]==intel_id[i]))
		clone_flag = 1;    
}

_asm {
        
		cmp     eax, 1			// Make sure 1 is valid input 
        						//   for CPUID
        
        jl      end_cpuidext	// If not, jump to end
        xor     eax, eax
        inc		eax
        CPU_ID					// Get family/model/stepping/
        						//   features

		mov		cpuidext, ax

end_cpuidext:
		mov		ax, cpuidext
    	}
	}
	else {

	cpu_type = wincpuid();		// If CPUID opcode is not
	cpuidext = cpu_type << 8;	//   supported, put family
								//   value in extensions and
	}							//   return
	
	return cpuidext;

} // wincpuidext()


static struct FREQ_INFO GetBSFCpuSpeed(ulong cycles)
{
	// If processor does not support time 
	//   stamp reading, but is at least a 
	//   386 or above, utilize method of 
	//   timing a loop of BSF instructions 
	//   which take a known number of cycles
	//   to run on i386(tm), i486(tm), and
	//   Pentium(R) processors.
	LARGE_INTEGER t0,t1;			// Variables for High-
									//   Resolution Performance
									//   Counter reads

	ulong freq  =0;			// Most current frequ. calculation

	ulong  ticks;					// Microseconds elapsed 
									//   during test
	
	LARGE_INTEGER count_freq;		// High Resolution 
									//   Performance Counter 
									//   frequency

	int i;						// Temporary Variable

	ulong current = 0;          // Variable to store time
									//   elapsed during loop of
									//   of BSF instructions

	ulong lowest  = ULONG_MAX;	// Since algorithm finds 
									//   the lowest value out of
									//   a set of samplings, 
									//   this variable is set 
									//   intially to the max 
									//   unsigned long value). 
									//   This guarantees that 
									//   the initialized value 
									//   is not later used as 
									//   the least time through 
									//   the loop.

	struct FREQ_INFO cpu_speed;

	memset(&cpu_speed, 0x00, sizeof(cpu_speed));

	if ( !QueryPerformanceFrequency ( &count_freq ) )
		return cpu_speed;

	for ( i = 0; i < SAMPLINGS; i++ ) { // Sample Ten times. Can
									 //   be increased or 
									 //   decreased depending
									 //   on accuracy vs. time
									 //   requirements

		QueryPerformanceCounter(&t0);	// Get start time

			_asm 
			{
				
					mov eax, 80000000h	
					mov bx, ITERATIONS		
								// Number of consecutive BSF 
								//   instructions to execute. 
								//   Set identical to 
								//   nIterations constant in
								//   speed.h
	           
				loop1:	bsf ecx,eax
       
	       				dec	bx
						jnz	loop1
			}
							 
		QueryPerformanceCounter(&t1);	// Get end time
		current = (ulong) t1.LowPart - (ulong) t0.LowPart;	
								// Number of external ticks is
								//   difference between two
								//   hi-res counter reads.

		if ( current < lowest )		// Take lowest elapsed
			lowest = current;		//   time to account
	}								//   for some samplings
										//   being interrupted
										//   by other operations 
		 
	ticks = lowest;

	// Note that some seemingly arbitrary mulitplies and
	//   divides are done below. This is to maintain a 
	//   high level of precision without truncating the 
	//   most significant data. According to what value 
	//   ITERATIIONS is set to, these multiplies and
	//   divides might need to be shifted for optimal
	//   precision.

	ticks = ticks * 100000;	
						// Convert ticks to hundred
						//   thousandths of a tick
			
	ticks = ticks / ( count_freq.LowPart/10 );		
						// Hundred Thousandths of a 
						//   Ticks / ( 10 ticks/second )
						//   = microseconds (us)
		
	if ( ticks%count_freq.LowPart > count_freq.LowPart/2 )	
		ticks++;				// Round up if necessary
			
	freq = cycles/ticks;		// Cycles / us  = MHz

	cpu_speed.raw_freq  = freq;
    if ( cycles%ticks > ticks/2 )
   		freq++;					// Round up if necessary	

	cpu_speed.in_cycles = cycles;	// Return variable structure
	cpu_speed.ex_ticks  = ticks;	//   determined by one of 
	cpu_speed.norm_freq = freq;

	return cpu_speed;
}	

static struct FREQ_INFO GetRDTSCCpuSpeed()
{
	struct FREQ_INFO cpu_speed;
	LARGE_INTEGER t0,t1;			// Variables for High-
									//   Resolution Performance
									//   Counter reads

	ulong freq  =0;			// Most current frequ. calculation
	ulong freq2 =0;			// 2nd most current frequ. calc.
	ulong freq3 =0;			// 3rd most current frequ. calc.
	
	ulong total;			// Sum of previous three frequency
							//   calculations

	int tries=0;			// Number of times a calculation has
							//   been made on this call to 
							//   cpuspeed

	ulong  total_cycles=0, cycles;	// Clock cycles elapsed 
									//   during test
	
	ulong  stamp0, stamp1;			// Time Stamp Variable 
									//   for beginning and end 
									//   of test

	ulong  total_ticks=0, ticks;	// Microseconds elapsed 
									//   during test
	
	LARGE_INTEGER count_freq;		// High Resolution 
									//   Performance Counter 
									//   frequency

#ifdef WIN32
	int iPriority;
	HANDLE hThread = GetCurrentThread();
#endif // WIN32;

	memset(&cpu_speed, 0x00, sizeof(cpu_speed));

	if ( !QueryPerformanceFrequency ( &count_freq ) )
		return cpu_speed;

	// On processors supporting the Read 
	//   Time Stamp opcode, compare elapsed
	//   time on the High-Resolution Counter
	//   with elapsed cycles on the Time 
	//   Stamp Register.
	
	do {			// This do loop runs up to 20 times or
	   				//   until the average of the previous 
	   				//   three calculated frequencies is 
	   				//   within 1 MHz of each of the 
	   				//   individual calculated frequencies. 
					//   This resampling increases the 
					//   accuracy of the results since
					//   outside factors could affect this
					//   calculation
			
		tries++;		// Increment number of times sampled
						//   on this call to cpuspeed
			
		freq3 = freq2;	// Shift frequencies back to make
		freq2 = freq;	//   room for new frequency 
						//   measurement

    	QueryPerformanceCounter(&t0);	
    					// Get high-resolution performance 
    					//   counter time
			
		t1.LowPart = t0.LowPart;		// Set Initial time
		t1.HighPart = t0.HighPart;

#ifdef WIN32
		iPriority = GetThreadPriority(hThread);
		if ( iPriority != THREAD_PRIORITY_ERROR_RETURN )
		{
			SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
		}
#endif // WIN32

   		while ( (ulong)t1.LowPart - (ulong)t0.LowPart<50) {	  
   						// Loop until 50 ticks have 
   						//   passed	since last read of hi-
						//	 res counter. This accounts for
						//   overhead later.

			QueryPerformanceCounter(&t1);

			RDTSC;						// Read Time Stamp
			_asm {
				MOV stamp0, EAX
			}
		}
			
			
		t0.LowPart = t1.LowPart;		// Reset Initial 
		t0.HighPart = t1.HighPart;		//   Time

   		while ((ulong)t1.LowPart-(ulong)t0.LowPart<1000 ) {
   						// Loop until 1000 ticks have 
   						//   passed	since last read of hi-
   						//   res counter. This allows for
						//   elapsed time for sampling.
   			
				
   			QueryPerformanceCounter(&t1);
   			

			RDTSC;						// Read Time Stamp
			__asm {
				MOV stamp1, EAX
			}
		}

			

#ifdef WIN32
		// Reset priority
		if ( iPriority != THREAD_PRIORITY_ERROR_RETURN )
		{
			SetThreadPriority(hThread, iPriority);
		}
#endif // WIN32

       	cycles = stamp1 - stamp0;	// Number of internal 
        							//   clock cycles is 
        							//   difference between 
        							//   two time stamp 
        							//   readings.

    	ticks = (ulong) t1.LowPart - (ulong) t0.LowPart;	
								// Number of external ticks is
								//   difference between two
								//   hi-res counter reads.
	

		// Note that some seemingly arbitrary multiplies and
		//   divides are done below. This is to maintain a 
		//   high level of precision without truncating the 
		//   most significant data. According to what value 
		//   ITERATIONS is set to, these multiplies and
		//   divides might need to be shifted for optimal
		//   precision.

		ticks = ticks * 100000;	
							// Convert ticks to hundred
							//   thousandths of a tick
			
		ticks = ticks / ( count_freq.LowPart/10 );		
							// Hundred Thousandths of a 
							//   Ticks / ( 10 ticks/second )
							//   = microseconds (us)

		total_ticks += ticks;
		total_cycles += cycles;

		if ( ticks%count_freq.LowPart > count_freq.LowPart/2 )
			ticks++;			// Round up if necessary
			
		freq = cycles/ticks;	// Cycles / us  = MHz
        										
     	if ( cycles%ticks > ticks/2 )
       		freq++;				// Round up if necessary
          	
		total = ( freq + freq2 + freq3 );
							// Total last three frequency 
							//   calculations

	} while ( (tries < 3 ) || 		
	          (tries < 20)&&
	          ((abs(int(3 * freq -total)) > 3*TOLERANCE )||
	           (abs(int(3 * freq2-total)) > 3*TOLERANCE )||
	           (abs(int(3 * freq3-total)) > 3*TOLERANCE )));	
					// Compare last three calculations to 
	          		//   average of last three calculations.		

	// Try one more significant digit.
	freq3 = ( total_cycles * 10 ) / total_ticks;
	freq2 = ( total_cycles * 100 ) / total_ticks;


	if ( freq2 - (freq3 * 10) >= ROUND_THRESHOLD )
		freq3++;

	cpu_speed.raw_freq = total_cycles / total_ticks;
	cpu_speed.norm_freq = cpu_speed.raw_freq;

	freq = cpu_speed.raw_freq * 10;
	if( (freq3 - freq) >= ROUND_THRESHOLD )
		cpu_speed.norm_freq++;

	cpu_speed.ex_ticks = total_ticks;
	cpu_speed.in_cycles = total_cycles;

	return cpu_speed;
}


//CPU Speed functions
/***************************************************************
* CpuSpeed() -- Return the raw clock rate of the host CPU.
*
* Inputs:
*	clocks:		0: Use default value for number of cycles
*				   per BSF instruction.
*               -1: Use CMos timer to get cpu speed.
*   			Positive Integer: Use clocks value for number
*				   of cycles per BSF instruction.
*
* Returns:
*		If error then return all zeroes in FREQ_INFO structure
*		Else return FREQ_INFO structure containing calculated 
*       clock frequency, normalized clock frequency, number of 
*       clock cycles during test sampling, and the number of 
*       microseconds elapsed during the sampling.
***************************************************************/

struct FREQ_INFO cpuspeed(int clocks) 
{
	ulong  cycles;					// Clock cycles elapsed 
									//   during test
	
	ushort processor = wincpuid();	// Family of processor


	CString vendor;
	DWORD features = wincpufeatures(vendor);	// Features of Processor

	int manual=0;			// Specifies whether the user 
							//   manually entered the number of
							//   cycles for the BSF instruction.

	struct FREQ_INFO cpu_speed;		// Return structure for
									//   cpuspeed

	memset(&cpu_speed, 0x00, sizeof(cpu_speed));
	
	// Check for manual BSF instruction clock count
	if (clocks <= 0) {
		cycles = ITERATIONS * processor_cycles[processor];
	}
	else if (0 < clocks && clocks <= MAXCLOCKS)  {
		cycles = ITERATIONS * clocks;
		manual = 1;			// Toggle manual control flag.
							//   Note that this mode will not
							// 	 work properly with processors
							//   which can process multiple
							//   BSF instructions at a time.
							//   For example, manual mode
							//   will not work on a 
							//   PentiumPro(R)
	}

	if ( ( features&0x00000010 ) && !(manual) ) {						
						// On processors supporting the Read 
						//   Time Stamp opcode, compare elapsed
						//   time on the High-Resolution Counter
						//   with elapsed cycles on the Time 
						//   Stamp Register.
		if ( clocks == 0 )
			return GetRDTSCCpuSpeed();
		else
			return cpu_speed;
    }
	else if ( processor >= 3 ) {
		return GetBSFCpuSpeed(cycles);
	}

	return cpu_speed;

} // cpuspeed()


// Internal Private Functions //////////////////////////////////

/***************************************************************
* check_clone()
*
* Inputs: none
*
* Returns:
*   1      if processor is clone (limited detection ability)
*   0      otherwise
***************************************************************/

static WORD check_clone()
{
	short cpu_type=0;

	_asm 
		{
  					MOV AX,5555h	// Check to make sure this
					XOR DX,DX		//   is a 32-bit processor
					MOV CX,2h
					DIV CX			// Perform Division
					CLC
					JNZ no_clone
					JMP clone
		no_clone:	STC
		clone:		PUSHF
					POP AX          // Get the flags
					AND AL,1
					XOR AL,1        // AL=0 is probably Intel,
									//   AL=1 is a Clone
					
					MOV cpu_type, ax
		}
	
    cpu_type = cpu_type & 0x0001;
    
	return cpu_type;
		
} // check_clone()

/***************************************************************
* check_8086()
*
* Inputs: none
*
* Returns: 
*   0      if processor 8086
*   0xffff otherwise
***************************************************************/

static WORD check_8086()
{

		WORD cpu_type=0xffff;

_asm {
        pushf                   // Push original FLAGS
        pop     ax              // Get original FLAGS
        mov     cx, ax          // Save original FLAGS
        and     ax, 0fffh       // Clear bits 12-15 in FLAGS
        push    ax              // Save new FLAGS value on stack
        popf                    // Replace current FLAGS value
        pushf                   // Get new FLAGS
        pop     ax              // Store new FLAGS in AX
        and     ax, 0f000h      // If bits 12-15 are set, then
        cmp     ax, 0f000h      //   processor is an 8086/8088
        mov     cpu_type, 0    	// Turn on 8086/8088 flag
        je      end_8086    	// Jump if processor is 8086/
        						//   8088
        mov		cpu_type, 0ffffh
end_8086:
		push 	cx
		popf
		mov		ax, cpu_type

      }
	
	return cpu_type;

} // check_8086()



/***************************************************************
* check_80286()
*
* Inputs: none
*
* Returns:
*   2      if processor 80286
*   0xffff otherwise
***************************************************************/

static WORD check_80286()
{

		WORD cpu_type=0xffff;

_asm {
		pushf
		pop		cx
		mov		bx, cx
        or      cx, 0f000h      // Try to set bits 12-15
        push    cx              // Save new FLAGS value on stack
        popf                    // Replace current FLAGS value
        pushf                   // Get new FLAGS
        pop     ax              // Store new FLAGS in AX
        and     ax, 0f000h      // If bits 12-15 are clear
        
        mov     cpu_type, 2     // Processor=80286, turn on 
        						//   80286 flag
        
        jz      end_80286       // If no bits set, processor is 
        						//   80286
		
		mov		cpu_type, 0ffffh
end_80286:
		push	bx
		popf
		mov		ax, cpu_type

      }
	
	return cpu_type;

} // check_80286()



/***************************************************************
* check_80386()
*
* Inputs: none
*
* Returns:
*   3      if processor 80386
*   0xffff otherwise
***************************************************************/

static WORD check_80386()
{

		WORD cpu_type=0xffff;

_asm {   
		mov 	bx, sp
		and		sp, not 3
        pushfd					// Push original EFLAGS 
        pop     eax				// Get original EFLAGS
        mov     ecx, eax		// Save original EFLAGS
        xor     eax, 40000h		// Flip AC bit in EFLAGS
        
        push    eax             // Save new EFLAGS value on
        						//   stack
        
        popfd                   // Replace current EFLAGS value
        pushfd					// Get new EFLAGS
        pop     eax             // Store new EFLAGS in EAX
        
        xor     eax, ecx        // Can't toggle AC bit, 
        						//   processor=80386
        
        mov     cpu_type, 3		// Turn on 80386 processor flag
        jz      end_80386		// Jump if 80386 processor
		mov		cpu_type, 0ffffh
end_80386:
		push	ecx
		popfd
		mov		sp, bx
		mov		ax, cpu_type
		and		eax, 0000ffffh
      }

	return cpu_type;

} // check_80386()



/***************************************************************
* check_IDProc()
*
* Inputs: none
*
* Returns:
*  CPU Family (i.e. 4 if Intel 486, 5 if Pentium(R) Processor)
*
*  Note: This function also sets the global variable clone_flag
***************************************************************/

static WORD check_IDProc() {

		int i=0;
		WORD cpu_type=0xffff;
		BYTE stepping=0;
		BYTE model=0;
		BYTE vendor_id[]="------------";
		BYTE intel_id[]="GenuineIntel";

_asm {      

        xor     eax, eax		// Set up for CPUID instruction
        
        CPU_ID                  // Get and save vendor ID

        mov     dword ptr vendor_id, ebx
        mov     dword ptr vendor_id[+4], edx
        mov     dword ptr vendor_id[+8], ecx
}

for (i=0;i<12;i++)
{
	if (!(vendor_id[i]==intel_id[i]))
		clone_flag = 1;    
}

_asm {

        cmp     eax, 1			// Make sure 1 is valid input 
        						//   for CPUID
        
        jl      end_IDProc		// If not, jump to end
        xor     eax, eax
        inc		eax
        CPU_ID					// Get family/model/stepping/
        						//   features

		mov 	stepping, al
		and		stepping, 0x0f //0fh
		
		and 	al, 0f0h
		shr		al, 4
		mov 	model, al
		
		and		eax, 0f00h
        shr     eax, 8			// Isolate family
		and		eax, 0fh
        mov     cpu_type, ax	// Set _cpu_type with family

end_IDProc:
		mov		ax, cpu_type
      }
	
	return cpu_type;

} // Check_IDProc()


