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
#include "winproto.h"
#include "template.h"
#include "netsvw.h"
#include "mainfrm.h"
#ifdef MOZ_MAIL_NEWS
#include "mailfrm.h"
#include "compfrm.h"
#endif /* MOZ_MAIL_NEWS */
#include "prefapi.h"
#include "cxwin.h"

#ifdef AFX_CORE2_SEG
#pragma code_seg(AFX_CORE2_SEG)
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CGenericDocTemplate, CDocTemplate)
IMPLEMENT_DYNAMIC(CNetscapeDocTemplate, CGenericDocTemplate)
#ifdef EDITOR
IMPLEMENT_DYNAMIC(CNetscapeEditTemplate, CGenericDocTemplate)
#endif // EDITOR
#ifdef MOZ_MAIL_NEWS
IMPLEMENT_DYNAMIC(CNetscapeComposeTemplate, CGenericDocTemplate)
IMPLEMENT_DYNAMIC(CNetscapeTextComposeTemplate, CGenericDocTemplate)
IMPLEMENT_DYNAMIC(CNetscapeAddrTemplate, CGenericDocTemplate)
#endif /* MOZ_MAIL_NEWS */
#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

//CLM:
//  Set TRUE when starting an Editor
//  with curent Browser document
//  (CGenframe::OnNavigateToEdit)
BOOL wfe_bUseLastFrameLocation = FALSE;
// The last active frame before creating a new one
CGenericFrame *wfe_pLastFrame = NULL;

/////////////////////////////////////////////////////////////////////////////
// construction/destruction
CGenericDocTemplate::CGenericDocTemplate(UINT nIDResource,
    CRuntimeClass* pDocClass,
    CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
	: CDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass)
{
    m_nUntitledCount = 0;
    m_hMenuShared = NULL;
    m_hAccelTable = NULL;
    m_bHideTitlebar = FALSE;
    m_bDependent = FALSE;
	m_bPopupWindow = FALSE;
	m_hPopupParent = NULL;
    m_bBorder = TRUE;
}

CGenericDocTemplate::~CGenericDocTemplate()
{
    //  Delete shared components
    if(m_hMenuShared != NULL)   {
	::DestroyMenu(m_hMenuShared);
    }
    if(m_hAccelTable != NULL)   {
	::FreeResource((HGLOBAL)m_hAccelTable);
    }
}


/////////////////////////////////////////////////////////////////////////////
//  CNetscapeDocTemplate attributes

POSITION CGenericDocTemplate::GetFirstDocPosition() const  {
    return m_docList.GetHeadPosition();
}

CDocument *CGenericDocTemplate::GetNextDoc(POSITION& rPos) const   {
    return(CDocument *)m_docList.GetNext(rPos);
}


/////////////////////////////////////////////////////////////////////////////
//  CNetscapeDocTemplate document management (list of open docs)

void CGenericDocTemplate::AddDocument(CDocument *pDoc) {
    CDocTemplate::AddDocument(pDoc);
    m_docList.AddTail(pDoc);
}

void CGenericDocTemplate::RemoveDocument(CDocument *pDoc)  {
    CDocTemplate::RemoveDocument(pDoc);
    m_docList.RemoveAt(m_docList.Find(pDoc));
}

/////////////////////////////////////////////////////////////////////////////
//  CNetscapeDocTemplate misc

BOOL CGenericDocTemplate::GetDocString(CString& rString, enum DocStringIndex index) const
{
    switch(index) {
    case CDocTemplate::windowTitle:
	rString = "Netscape Navigator";
	break;
    case CDocTemplate::docName:
	rString = "Netscape";
	break;
    case CDocTemplate::fileNewName:
	rString = "Hypertext";
	break;
    case CDocTemplate::filterName:
	rString = szLoadString(IDS_SOURCE_FILETYPE);
	break;
    case CDocTemplate::filterExt:
	rString = ".htm";
	break;
    case CDocTemplate::regFileTypeId:
	rString = "NetscapeMarkup";
	break;
    case CDocTemplate::regFileTypeName:
	rString = "Netscape Hypertext Document";
	break;
    default:
	return(FALSE);
    }

    return(TRUE);
}

void CGenericDocTemplate::SetDefaultTitle(CDocument* pDocument)
{
    CString strDocName;
    if (!GetDocString(strDocName, CDocTemplate::docName) ||
	strDocName.IsEmpty())
    {
	// use generic 'untitled'
	VERIFY(strDocName.LoadString(AFX_IDS_UNTITLED));
    }
    pDocument->SetTitle(strDocName);
}

//
// Add the newly created frame to the end of theApp's m_pFrameList
void 
WFE_AddNewFrameToFrameList(CGenericFrame * pFrame)
{

    //  If the application doesn't have a main frame window yet,
    //      this frame is it.
    if(theApp.m_pFrameList == NULL) {
	theApp.m_pFrameList = pFrame;
	pFrame->m_pNext = NULL;
    }
    //  Otherwise, we need to add this to the list of frame windows
    else {
	CGenericFrame *CMFp = theApp.m_pFrameList;
	
	while(CMFp->m_pNext != NULL) {
	    CMFp = CMFp->m_pNext;
	}
	
	//  Our next is NULL.  Set it to the new frame.
	CMFp->m_pNext = pFrame;
	pFrame->m_pNext = NULL;
	
    }

}


CDocument *CGenericDocTemplate::OpenDocumentFile(const char *pszPathName, 
												 BOOL bMakeVisible)
{
    //CLM: Save last active frame for InitialUpdateFrame location mangling
    //     We want either browser or editor frame only
	CFrameWnd* pFrameWnd = FEU_GetLastActiveFrame(MWContextBrowser, TRUE);
	if (pFrameWnd && pFrameWnd->IsKindOf(RUNTIME_CLASS(CGenericFrame)))
		wfe_pLastFrame = (CGenericFrame*)pFrameWnd;
	else wfe_pLastFrame = NULL;
    //Don't use restricted target windows to keep them from
    //being used as size and position references.
    if (wfe_pLastFrame && wfe_pLastFrame->GetMainContext() && 
		    wfe_pLastFrame->GetMainContext()->GetContext() &&
		    wfe_pLastFrame->GetMainContext()->GetContext()->restricted_target)
		wfe_pLastFrame = NULL;

    //  Create the document
    CDocument *pDocument = CreateNewDocument();
    if(pDocument == NULL)   {
	return(NULL);
    }
    
    //  Create the frame, several steps
    BOOL bAutoDelete = pDocument->m_bAutoDelete;
    pDocument->m_bAutoDelete = FALSE;   //  don't destroy if something goes wrong
    
    //  We do a nasty manual frame creation technique here.
    //  Don't try this at home, kids.
    CCreateContext CCC;
    CCC.m_pCurrentFrame = NULL; //  nothing to base on
    CCC.m_pCurrentDoc = pDocument;  //  yes, we have doc
    CCC.m_pNewViewClass = m_pViewClass; //  whatever
    CCC.m_pNewDocTemplate = this;   //  we are the template, duh    
    
    CFrameWnd *pFrame = (CFrameWnd *)m_pFrameClass->CreateObject();
	CRuntimeClass* pRunTimeClass = GetRuntimeClass();

#ifdef MOZ_MAIL_NEWS
	ASSERT(pRunTimeClass);
	if (!stricmp("CNetscapeComposeTemplate",pRunTimeClass->m_lpszClassName))
  	    ((CComposeFrame *) pFrame)->SetHtmlMode(TRUE);
#endif /* MOZ_MAIL_NEWS */
    pDocument->m_bAutoDelete = bAutoDelete;
    if(pFrame == NULL)  {
	delete pDocument;   //  explicit delete upon error
	return(NULL);
    }

    //Keep dependent windows off of the Win95/NT taskbar
#ifdef WIN32    
    if (m_bDependent)
	((CGenericFrame*)pFrame)->SetExStyles(WS_EX_TOOLWINDOW, 0);
    if (m_bHideTitlebar || theApp.m_bSuperKioskMode)
	((CGenericFrame*)pFrame)->SetExStyles(0, WS_EX_CLIENTEDGE);
#endif
	
	if(m_bPopupWindow)
		((CGenericFrame*)pFrame)->SetAsPopup(m_hPopupParent);

	m_bPopupWindow = FALSE;
	m_hPopupParent = NULL;

    //  Create the new frame from some resources
	if (m_bHideTitlebar || theApp.m_bSuperKioskMode){	
		// ZZZ: Removed WS_CLIPCHILDREN because it was interfering with the frame
		// gride edge tracking code. Troy
		if(!pFrame->LoadFrame(m_nIDResource, WS_CLIPSIBLINGS |
		WS_OVERLAPPED, NULL, &CCC))   {
			delete pDocument;   //  explicit delete upon error
			return(NULL);
		}   
	}else{
		// ZZZ: Removed WS_CLIPCHILDREN because it was interfering with the frame
		// gride edge tracking code. Troy

		if(!pFrame->LoadFrame(m_nIDResource, WS_CLIPSIBLINGS |
		WS_OVERLAPPEDWINDOW, NULL, &CCC))   {
			delete pDocument;   //  explicit delete upon error
			return(NULL);
		}
	}

    if(m_bHideTitlebar || theApp.m_bSuperKioskMode) {
	long lStyles = GetWindowLong(pFrame->GetSafeHwnd(), GWL_STYLE); 
	lStyles |= WS_SYSMENU | WS_GROUP | WS_TABSTOP;
	lStyles &= ~WS_CAPTION;
	SetWindowLong(pFrame->GetSafeHwnd(), GWL_STYLE, lStyles);
    }

    if(theApp.m_ParentAppWindow) {
	SetParent(pFrame->GetSafeHwnd(), theApp.m_ParentAppWindow);
	theApp.m_bChildWindow = TRUE;
	theApp.m_ParentAppWindow = 0;
    }

    //  Here's some hack to get some context information, and activate
    //      a view
    if(pFrame->GetActiveView() == NULL) {
	CView *pView = (CView *)pFrame->
	    GetDescendantWindow(AFX_IDW_PANE_FIRST);
	if(pView != NULL && pView->IsKindOf(RUNTIME_CLASS(CView)))  {
	    pFrame->SetActiveView(pView);
	}
    }

    // add to the global frame list
	if (pFrame->IsKindOf(RUNTIME_CLASS(CGenericFrame))) {
	    WFE_AddNewFrameToFrameList((CGenericFrame *) pFrame);
    }
    //  Wether or not to create a new or existing document is determined
    //      via the path name passed in.
    if(pszPathName == NULL) {
	//  Create a new document - with default document name
	UINT nUntitled = m_nUntitledCount + 1;
	
	CString strDocName;
	if(GetDocString(strDocName, CDocTemplate::docName) &&
	    !strDocName.IsEmpty())  {
	    char szNum[16];
	    wsprintf(szNum, "%d", nUntitled);
	    strDocName += szNum;
	}
	else    {
	    //  use generic 'untitiled' - ignore untitled count
	    VERIFY(strDocName.LoadString(AFX_IDS_UNTITLED));
	}
	pDocument->SetTitle(strDocName);
	
	if(bMakeVisible && !pDocument->OnNewDocument()) {
	    //  user has been alerted to what failed in OnNewDocument
	    pFrame->DestroyWindow();
	    return(NULL);
	}
	
	//  it worked, bump up the untitiled count
	m_nUntitledCount++;
    }
    else    {
	//  Open an existing document
	BeginWaitCursor();
	if(!pDocument->OnOpenDocument(pszPathName)) {
	    //  user has been alerted to what failed in OnOpenDocument
	    pFrame->DestroyWindow();
	    EndWaitCursor();
	    return(NULL);
	}
	pDocument->SetPathName(pszPathName);
	EndWaitCursor();
    }

	InitialUpdateFrame(pFrame, pDocument, bMakeVisible);
	
	return pDocument;
}





/////////////////////////////////////////////////////////////////////////////
//  CNetscapeDocTemplate commands
CNetscapeDocTemplate::CNetscapeDocTemplate(UINT nIDResource,
    CRuntimeClass* pDocClass,
    CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
	: CGenericDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass)
{
    HINSTANCE hInst = AfxGetResourceHandle();
    m_hMenuShared = ::LoadMenu(hInst, MAKEINTRESOURCE(nIDResource));
    m_hAccelTable = ::LoadAccelerators(hInst, MAKEINTRESOURCE(nIDResource));
}

// Common stuff for CNetscapeDocTemplate and CNetscapeEditTemplate
void wfe_InitialUpdateFrame(CFrameWnd* pFrame, 
			    CDocument* pDocument, 
						BOOL bMakeVisible)      
{
	
    WINDOWPLACEMENT wp;
    FEU_InitWINDOWPLACEMENT(pFrame->GetSafeHwnd(), &wp);

    BOOL bSetLocation = FALSE;
    int screenX = sysInfo.m_iScreenWidth;
    int screenY = sysInfo.m_iScreenHeight;
	int16 iLeft,iRight,iTop,iBottom;
	PREF_GetRectPref("browser.window_rect", &iLeft, &iTop, &iRight, &iBottom);

    if ( wfe_pLastFrame ) {
        bSetLocation = TRUE;

        int captionY = ::GetSystemMetrics(SM_CYCAPTION);
        int titleY = ::GetSystemMetrics(SM_CYSIZE) + ::GetSystemMetrics(SM_CYFRAME);

        // Get placement and location of last frame
        wfe_pLastFrame->GetWindowPlacement(&wp);
        CRect rectLastFrame = wp.rcNormalPosition;

        // Impose lower and upper limits on frame size
        if( rectLastFrame.Width() < 300 ){
	        rectLastFrame.right = rectLastFrame.left + 300;
        }
        if( rectLastFrame.Height() < 300 ){
	        rectLastFrame.bottom = rectLastFrame.top + 300;
        }
        if( rectLastFrame.Width() > screenX ){
	        rectLastFrame.right = rectLastFrame.left + screenX;
        }
        if( rectLastFrame.Height() > screenY ){
	        rectLastFrame.bottom = rectLastFrame.top + screenY;
        }

        if ( wfe_bUseLastFrameLocation ) {
	        // Keep same location and size as last frame
	        // Good for just one new window!
	        wfe_bUseLastFrameLocation = FALSE;
        } else {
	        // Cascade: Start down and to the right of current frame
	        wp.rcNormalPosition.left += titleY;
	        wp.rcNormalPosition.top  += titleY;
        }

        // Set the right and bottom
        wp.rcNormalPosition.right = 
	        wp.rcNormalPosition.left + rectLastFrame.Width();
        wp.rcNormalPosition.bottom = 
	        wp.rcNormalPosition.top + rectLastFrame.Height();

        //  If we go off the screen to the right or to the bottom and
        //    pulling us back to 0,0 would make us fit on the screen then
        //    pull the origin back to 0,0
        //  But only if these coordinates are not the same as those on 
        //    the command line.
        if( wp.rcNormalPosition.right > screenX &&
	        rectLastFrame.Width() <= screenX     &&
	        wp.rcNormalPosition.left != theApp.m_iCmdLnX ) {

	        wp.rcNormalPosition.left = 0;
	        wp.rcNormalPosition.right = rectLastFrame.Width();
        }

        if( wp.rcNormalPosition.bottom > screenY &&
	        rectLastFrame.Height() <= screenY  &&
	        wp.rcNormalPosition.top != theApp.m_iCmdLnY ) {

	        wp.rcNormalPosition.top = 0;
	        wp.rcNormalPosition.bottom = rectLastFrame.Height();
        }
	        
        // Preserve International character set
        ((CGenericFrame*)pFrame)->m_iCSID = (wfe_pLastFrame)->m_iCSID;
    }
    else if (iLeft != -1) {
        bSetLocation = TRUE;
        int32 prefInt;
        PREF_GetIntPref("browser.wfe.show_value",&prefInt);
        wp.showCmd = CASTINT(prefInt);

        if(theApp.m_bInInitInstance)  {
	        //  Any frames created while in init instance must use the style passed therein.
	        //  See WinExec and how we don't pay attention to it....
	        //  We can get away with this only if we weren't told to show up normal.
	        if(theApp.m_iFrameCmdShow != SW_SHOWNORMAL) {
	        wp.showCmd = theApp.m_iFrameCmdShow;
	        }
        }

        // We don't have a "last frame" -- probably 1st time through
        // Still do range checking in case junk was saved in preferences
        wp.rcNormalPosition.left = max( 0, min(screenX-300, iLeft) );
        wp.rcNormalPosition.top  = max( 0, min(screenY-300, iTop) );
        wp.rcNormalPosition.right  = max( 300, min(screenX, iRight) );
        wp.rcNormalPosition.bottom = max( 300, min(screenY,  iBottom) );
    }
    else if( iLeft == -1) {
        // Bug 48772. Set window size if never set before
        bSetLocation = TRUE;
        wp.showCmd = SW_SHOWNORMAL;
        wp.rcNormalPosition.left = 0;
        wp.rcNormalPosition.top  = 0;
        wp.rcNormalPosition.right  = min(632, screenX);
        wp.rcNormalPosition.bottom = min(480, screenY);
    }

    if ( bSetLocation ) {
        // Use the modified position info for new window,
        //  but set other flags first
        if (wp.showCmd == SW_SHOWMINIMIZED) {
	        //  Can't do this conversion without first checking to see what is right and wrong
	        //      when paying attention to nCmdShow (winmain).
	        if(theApp.m_bInInitInstance == FALSE || theApp.m_iFrameCmdShow == SW_SHOWNORMAL)   {
	        wp.showCmd = SW_RESTORE;
	        }
        }

        //  We're not supposed to make visible, but is it visible (someone already handled up stream).
        if(bMakeVisible == FALSE && pFrame->IsWindowVisible() == FALSE) {
	        //      Don't show if told not to.
	        //      We do allow placement however, for size saving on close.
	        wp.showCmd = SW_HIDE;
        }
	if (theApp.m_bSuperKioskMode)
		wp.showCmd = SW_SHOWMAXIMIZED;

        // Note: if last frame was maximized, new frame will be also
        pFrame->SetWindowPlacement(&wp);
    }

	if (bMakeVisible && pDocument && pFrame) {
		if (theApp.m_pFrameList == pFrame) {
			switch (theApp.m_nCmdShow) {
			case SW_SHOWMINIMIZED:
			case SW_HIDE:
			case SW_SHOWMINNOACTIVE:
			case SW_MINIMIZE:
				pFrame->ActivateFrame(theApp.m_nCmdShow);
				// So InitialUpdateFrame doesn't do this again
				bMakeVisible = FALSE;
			default:
				break;
			}
		} else {
			pFrame->ActivateFrame(SW_SHOW);
			bMakeVisible = FALSE;
		}
	}
}

void CNetscapeDocTemplate::InitialUpdateFrame(CFrameWnd* pFrame, 
											  CDocument* pDocument, 
											  BOOL bMakeVisible)    
{
    // Window positioning code shared by Editor and Browser
    wfe_InitialUpdateFrame( pFrame, pDocument, bMakeVisible );
	//      Call the base.
	CDocTemplate::InitialUpdateFrame(pFrame, pDocument, bMakeVisible);
}


#ifdef EDITOR
// CLM: Copy of CNetscapeDocTemplate -- used by CNetscapeEditView
// MUST BE MAINTAINED TO MATCH ABOVE FUNCTIONS!
////////////////////////////////////////////////////////////////////////////////
//  CNetscapeEditTemplate commands
CNetscapeEditTemplate::CNetscapeEditTemplate(UINT nIDResource,
    CRuntimeClass* pDocClass,
    CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
	: CGenericDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass)
{
}

void CNetscapeEditTemplate::InitialUpdateFrame(CFrameWnd *pFrame, 
											   CDocument *pDocument, 
											   BOOL bMakeVisible)
{
    // Window positioning code shared by Editor and Browser
    wfe_InitialUpdateFrame( pFrame, pDocument, bMakeVisible );
	//      Call the base.
	CDocTemplate::InitialUpdateFrame(pFrame, pDocument, /*bMakeVisible */ TRUE );
}
#endif // EDITOR

#ifdef MOZ_MAIL_NEWS
/////////////////////////////////////////////////////////////////////////////
//  CNetscapeComposeTemplate commands
CNetscapeComposeTemplate::CNetscapeComposeTemplate(UINT nIDResource,
    CRuntimeClass* pDocClass,
    CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
	: CGenericDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass)
{
    // for the compose window we will use shared menus and accelerators but
    //  we will not actually load them until the first compose window is created
}

void CNetscapeComposeTemplate::InitialUpdateFrame(CFrameWnd *pFrame, 
												  CDocument *pDocument, 
												  BOOL bMakeVisible)    
{
	if (pDocument && pFrame) {
	    // tell the frame what type of widgets to create
		((CComposeFrame *) pFrame)->SetType(MWContextMessageComposition);

		// Pass win_csid to ComposeFrame, eventually ComposeEdit will use
		// it to select font

		((CComposeFrame *) pFrame)->SetCSID(win_csid);      
		//  Activate the frame but do not show it until we get a call to 
		//    FE_RaiseMailCompositionWindow() --- this allows us to use
		//    the composition frame as a context to quote messages with and
		//    such before loading exchange without flashing the netscape compose
		//    window up
		pFrame->ActivateFrame(SW_RESTORE);
    }
	CDocTemplate::InitialUpdateFrame(pFrame, pDocument, FALSE);
}

/////////////////////////////////////////////////////////////////////////////
//  CNetscapeTextComposeTemplate commands
CNetscapeTextComposeTemplate::CNetscapeTextComposeTemplate(UINT nIDResource,
    CRuntimeClass* pDocClass,
    CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
	: CGenericDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass)
{
    // for the compose window we will use shared menus and accelerators but
    //  we will not actually load them until the first compose window is created
}

void CNetscapeTextComposeTemplate::InitialUpdateFrame(
    CFrameWnd *pFrame, CDocument *pDocument, BOOL bMakeVisible)    
{
	if (pDocument && pFrame) {
	    // tell the frame what type of widgets to create
		((CComposeFrame *) pFrame)->SetType(MWContextMessageComposition);

		// Pass win_csid to ComposeFrame, eventually ComposeEdit will use
		// it to select font

		((CComposeFrame *) pFrame)->SetCSID(win_csid);
      
		//  Activate the frame but do not show it until we get a call to 
		//    FE_RaiseMailCompositionWindow() --- this allows us to use
		//    the composition frame as a context to quote messages with and
		//    such before loading exchange without flashing the netscape compose
		//    window up
		if(bMakeVisible)// SW_RESTORE i.e. show frame only if bMakeVisible. 
			pFrame->ActivateFrame(SW_RESTORE);
    }
	CDocTemplate::InitialUpdateFrame(pFrame, pDocument, FALSE);
}



/////////////////////////////////////////////////////////////////////////////
//  CNetscapeAddrTemplate commands
CNetscapeAddrTemplate::CNetscapeAddrTemplate(UINT nIDResource,
    CRuntimeClass* pDocClass,
    CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
	: CGenericDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass)
{
}

void CNetscapeAddrTemplate::InitialUpdateFrame(CFrameWnd *pFrame, 
											   CDocument *pDocument, 
											   BOOL bMakeVisible)    
{
	if (pDocument && pFrame) {
		 //  Show it 
		int16 iLeft,iRight,iTop,iBottom;
		PREF_GetRectPref("mail.addr_book_window_rect", &iLeft, &iTop, &iRight, &iBottom);
		if (iLeft != -1) {
			int32 showCmd; // = *theApp.m_pAddrShow;
			PREF_GetIntPref("mail.wfe.addr_book.show_value",&showCmd);

			if (showCmd == SW_SHOWMINIMIZED)
				showCmd = SW_RESTORE;
			pFrame->ActivateFrame(CASTINT(showCmd));
		}
    }    
	CDocTemplate::InitialUpdateFrame(pFrame, pDocument, bMakeVisible);
}

#endif // MOZ_MAIL_NEWS
