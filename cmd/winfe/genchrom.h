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

#ifndef GENCHROM_H
#define GENCHROM_H

#include "apichrom.h"
#include "statbar.h"
#include "animbar.h"
#include "urlbar.h"
#include "csttlbr2.h"
#include "toolbar2.h"
#include "usertlbr.h"

class CGenericToolBar: public INSToolBar, public INSAnimation {

protected:
	CCommandToolbar *m_pCommandToolbar;
	UINT	      m_nBitmapID;
	LPUNKNOWN m_pOuterUnk;
	ULONG m_ulRefCount;

public:
	CGenericToolBar( LPUNKNOWN pOuterUnk = NULL );
	~CGenericToolBar();

// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef();
	STDMETHODIMP_(ULONG)	Release();

// INSToolbar Interface
	virtual int Create( CFrameWnd *pParent, 
						DWORD dwStyle = WS_CHILD|WS_VISIBLE|CBRS_TOP, 
						UINT nID = AFX_IDW_TOOLBAR );

	virtual void SetSizes( SIZE sizeButton, 
						   SIZE sizeImage );

	virtual void SetButtons( const UINT *lpIDArray,
							int nIDCount, UINT nBitmapID = 0 );
	
    //cmanske: Added functions convenient adding of popup menu styles to specific buttons
    //  and getting rect (used for CDropDownToolbar that appears below button)
    virtual void SetButtonStyle( UINT nIDButtonCommand, DWORD dwButtonStyle );
    virtual void GetButtonRect( UINT nIDButtonCommand, RECT * pRect );
	
    virtual void AddButton( CToolbarButton *pButton, int index = -1 );

	virtual void RemoveAllButtons();

	virtual CToolbarButton *RemoveButton( int index );
	virtual CToolbarButton *GetButtonByIndex(int index);
	virtual CToolbarButton *GetButtonByID(int nCmd);

	virtual BOOL LoadBitmap( LPCSTR lpszResourceName );

	virtual void SetToolbarStyle( int nToolbarStyle );

	virtual void Show( BOOL bShow = TRUE );

	// should the buttons be the same width or their own size
	virtual void SetButtonsSameWidth(BOOL bSameWidth);

	virtual HWND GetHWnd();

// INSAnimation Interface
	virtual void StartAnimation();
	virtual void StopAnimation();
};

class CModalStatus;

class CGenericStatusBar: public INSStatusBar {
protected:
	CNetscapeStatusBar *m_pStatusBar;
	CNetscapeStatusBar *m_pCreatedBar;

	LPUNKNOWN m_pOuterUnk;
	ULONG m_ulRefCount;

	int m_iProg;
	CString m_csStatus;

	BOOL m_bModal;
	CModalStatus *m_pModalStatus;

public:
	
	CGenericStatusBar( LPUNKNOWN );
	~CGenericStatusBar();

// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef();
	STDMETHODIMP_(ULONG)	Release();

// INSStatusBar Interface
	virtual BOOL Create( CWnd* pParentWnd, 
						DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, 
						UINT nID = AFX_IDW_STATUS_BAR,
                        BOOL bxxx = TRUE, BOOL bTaskbar = TRUE );

	virtual void Attach( CNetscapeStatusBar *pBar );

	// Messages
	virtual void SetStatusText(const char * = NULL);
	virtual const char *GetStatusText();

	// Progress
	virtual void SetProgress(int = 0);
	virtual int GetProgress();
	virtual void ProgressComplete();

	// Create/Remove a modal status window
	virtual void ModalStatus( BOOL bModal, UINT uDelay, char * pszTitle );

	virtual void Show( BOOL bShow = TRUE );

	virtual HWND GetHWnd();

    // "Cylon" mode cues    
    virtual void StartAnimation();
    virtual void StopAnimation();
    
	CNetscapeStatusBar *GetNetscapeStatusBar();
};

//Begin JOKI
//	Purpose:	Provide a registry to monitor when a status bar msg. text is changed
class CStatusBarChangeRegistry	{
protected:
	static CPtrList m_Registry;
	POSITION m_rIndex;

	CStatusBarChangeRegistry()	{}
	~CStatusBarChangeRegistry()	{m_Registry.RemoveAt(m_rIndex);	}
};

class CStatusBarChangeItem : public CStatusBarChangeRegistry	{	
protected:
	DWORD m_dwWindowID;

	CStatusBarChangeItem(DWORD dwWindowID) : CStatusBarChangeRegistry()	{
		m_dwWindowID = dwWindowID;
	}
	
	virtual void StatusChanging(LPCSTR lpStatusMsg) = 0;

public:
	DWORD GetWindowID()	{
		return(m_dwWindowID);
	}

	static void Changing(DWORD dwWindowID, LPCSTR lpStatusMsg);
};

class CDDEStatusBarChangeItem : public CStatusBarChangeItem	{
	CString m_csServiceName;
	CString m_csLastMsgSent;

protected:
	CDDEStatusBarChangeItem(CString& csServiceName, DWORD dwWindowID) : CStatusBarChangeItem(dwWindowID)	{
		m_rIndex = m_Registry.AddTail(this);
		m_csServiceName = csServiceName;
	}

	//	Must override.
	void StatusChanging(LPCSTR lpStatusMsg);
	
public:
	CString GetServiceName()	{
		return(m_csServiceName);
	}

	BOOL IsSameAsLastMsgSent(LPCSTR lpCurMsg);

	//	Consider these the constructor, destructor.
	static BOOL DDERegister(CString &csServiceName, DWORD dwWindowID);
	static BOOL DDEUnRegister(CString &csServiceName, DWORD dwWindowID);
};

//End JOKI

class CGenericChrome: public IChrome {
protected:
	ULONG m_ulRefCount;
	CFrameWnd *m_pParent;
	LPUNKNOWN m_pOuterUnk;
	CGenericToolBar *m_pToolBar;
	CGenericStatusBar *m_pStatusBar;

	CString m_csWindowTitle, m_csDocTitle;

	BOOL m_bHasStatus;

	CCustToolbar *m_pCustToolbar;
	CString m_toolbarName;
	LPNSTOOLBAR m_pMainToolBar;

//#ifndef	NO_TAB_NAVIGATION 
	virtual BOOL CGenericChrome::procTabNavigation( UINT nChar, UINT forward, UINT controlKey );
	UINT	m_tabFocusInChrom; 
	enum  { TAB_FOCUS_IN_NULL, TAB_FOCUS_IN_LOCATION_BAR };
//#endif	/* NO_TAB_NAVIGATION  */


public:
	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);
   
	virtual void Initialize( CFrameWnd *pWnd );

// Menu bar stuff
	virtual void SetMenu( UINT );

// General Toolbar functionality
	virtual void ShowToolbar(UINT nToolbarID, BOOL bShow = TRUE);
	virtual BOOL GetToolbarVisible(UINT nToolbarID);
	virtual CWnd *GetToolbar(UINT nToolbarID);
	virtual void SetToolbarFocus(UINT nToolbarID);
	// nPos, bOpen, and bShowing are IN/OUT parameters. Values going in are default values and values
	// coming out are the values from the registry or default if not in the registry.
	virtual void LoadToolbarConfiguration(UINT nToolbarID, CString &csToolbarName, int32 & nPos, BOOL & bOpen, BOOL & bShowing);
	virtual void SaveToolbarConfiguration(UINT nToolbarID, CString &csToolbarName);
	virtual void SetSaveToolbarInfo(BOOL bSaveToolbarInfo);

// Animation Stuff
	virtual void StartAnimation();
	virtual void StopAnimation();

//	Window Title Stuff
	virtual void SetWindowTitle(const char *);
	virtual void SetDocumentTitle(const char*);

//  ToolbarManager Stuff
	virtual int CreateCustomizableToolbar(CString toolbarName, int nMaxToolbars, BOOL bHasAnimation);
	virtual int CreateCustomizableToolbar(UINT nStringID, int nMaxToolbars, BOOL bHasAnimation);
	virtual CString GetCustToolbarString();
	virtual void RenameCustomizableToolbar(UINT nStringID);
	virtual void FinishedAddingBrowserToolbars();
	virtual void SetToolbarStyle( int nToolbarStyle );
	virtual BOOL CustToolbarShowing();
	virtual void ViewCustToolbar(BOOL bShow);
	virtual void Customize();
	virtual CCustToolbar * GetCustomizableToolbar();


//  MainFrame's Toolbar Stuff
	virtual void ImagesButton(BOOL);

// Constructor and Destructor
	CGenericChrome( LPUNKNOWN pOuterUnk );
	virtual ~CGenericChrome();
};


#endif
