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

#ifndef APICHROM_H
#define APICHROM_H

#ifndef __APIAPI_H
	#include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
	#include "nsguids.h"
#endif

#define	APICLASS_CHROME	"Chrome"
#define APICLASS_NSTOOLBAR "NSToolBar"

class CToolbarButton;
class CCustToolbar;

class INSToolBar: public IUnknown {

public:
	// Initialization
	virtual int Create( CFrameWnd *pParent, 
						DWORD dwStyle = WS_CHILD|WS_VISIBLE|CBRS_TOP, 
						UINT nID = AFX_IDW_TOOLBAR ) = 0;

	virtual void SetSizes( SIZE sizeButton, 
						   SIZE sizeImage ) = 0;

	virtual void SetButtons( const UINT *lpIDArray,
							 int nIDCount, UINT nBitmapID = 0 ) = 0;
	
    virtual void SetButtonStyle( UINT nIDButtonCommand, DWORD dwButtonStyle ) = 0;
    virtual void GetButtonRect( UINT nIDButtonCommand, RECT * pRect ) = 0;

	virtual void AddButton( CToolbarButton *pButton, int index = -1 ) = 0;
	
	virtual void RemoveAllButtons() = 0;

	virtual CToolbarButton *RemoveButton( int index ) = 0;

	virtual BOOL LoadBitmap( LPCSTR lpszResourceName ) = 0;

	// State
	virtual void SetToolbarStyle( int nToolbarStyle ) = 0;

	virtual void Show( BOOL bShow = TRUE ) = 0;

	// should the buttons be the same width or their own size
	virtual void SetButtonsSameWidth(BOOL bSameWidth) = 0;
	// Data Access
	virtual HWND GetHWnd() = 0;
};

typedef INSToolBar *LPNSTOOLBAR;

class CNetscapeStatusBar;

class INSStatusBar: public IUnknown {
public:
	virtual int Create( CWnd* pParentWnd, 
						DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, 
						UINT nID = AFX_IDW_STATUS_BAR,
                        BOOL bSecurity = TRUE, BOOL bTaskbar = TRUE ) = 0;
	
	virtual void Attach( CNetscapeStatusBar *pBar ) = 0;

	// Messages
	virtual void SetStatusText(const char * = NULL) = 0;
	virtual const char *GetStatusText() = 0;

	// Progress
	virtual void SetProgress(int = 0) = 0;
	virtual int GetProgress() = 0;
	virtual void ProgressComplete() = 0;
        
	// Create/Remove a modal status window
	virtual void ModalStatus( BOOL bModal, UINT uDelay, char * szTitle = NULL ) = 0;

	virtual void Show( BOOL bShow = TRUE ) = 0;

	virtual HWND GetHWnd() = 0;

    // status mode cues    
    virtual void StartAnimation() = 0;
    virtual void StopAnimation() = 0;
};

typedef INSStatusBar *LPNSSTATUSBAR;

#define ApiToolBar(v,unk) APIPTRDEF(IID_INSToolBar,INSToolBar,v,unk)

class INSAnimation: public IUnknown {

public:
	virtual void StartAnimation() = 0;
	virtual void StopAnimation() = 0;
};

typedef INSAnimation *LPNSANIMATION;

#define ApiAnimation(v,unk) APIPTRDEF(IID_INSAnimation,INSAnimation,v,unk)

class IChrome: public IUnknown {

public:
// Initialization
	virtual void Initialize(CFrameWnd *pWnd) = 0;

	virtual BOOL procTabNavigation( UINT nChar, UINT forward, UINT controlKey ) = 0;

// Menu bar stuff
	virtual void SetMenu( UINT ) = 0;

// General Toolbar functionality
	virtual void ShowToolbar(UINT nToolbarID, BOOL bShow = TRUE) = 0;
	virtual BOOL GetToolbarVisible(UINT nToolbarID) = 0;
	virtual CWnd *GetToolbar(UINT nToolbarID) = 0;
	virtual void SetToolbarFocus(UINT nToolbarID) = 0;
	// nPos, bOpen, and bShowing are IN/OUT parameters. Values going in are default values and values
	// coming out are the values from the registry or default if not in the registry.
	virtual void LoadToolbarConfiguration(UINT nToolbarID, CString &csToolbarName, int32 & nPos, BOOL & bOpen, BOOL & bShowing) = 0;
	virtual void SaveToolbarConfiguration(UINT nToolbarID, CString &csToolbarName)  = 0;
	virtual void SetSaveToolbarInfo(BOOL bSaveToolbarInfo) = 0;

// Animation Stuff
	virtual void StartAnimation() = 0;
	virtual void StopAnimation() = 0;

// Configurable Toolbar Manager stuff
	virtual int CreateCustomizableToolbar(CString toolbarName, int nMaxToolbars, BOOL bHasAnimation)=0;
	virtual int CreateCustomizableToolbar(UINT nStringID, int nMaxToolbars, BOOL bHasAnimation) = 0;
	virtual CString GetCustToolbarString() = 0;
	virtual void RenameCustomizableToolbar(UINT nStringID) = 0;
	virtual void FinishedAddingBrowserToolbars()=0;
	virtual void SetToolbarStyle( int nToolbarStyle )=0;
	virtual BOOL CustToolbarShowing()=0;
	virtual void ViewCustToolbar(BOOL bShow) = 0;
	virtual void Customize()=0;
	virtual CCustToolbar * GetCustomizableToolbar() = 0;

//  MainFrame's Toolbar Stuff
	virtual void ImagesButton(BOOL) = 0;



//	Window Title Stuff
	virtual void SetWindowTitle(const char *) = 0;
	virtual void SetDocumentTitle(const char *) = 0;
};

typedef IChrome * LPCHROME;

#define ApiChrome(v,unk) APIPTRDEF(IID_IChrome,IChrome,v,unk)

#endif
