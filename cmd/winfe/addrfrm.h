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

#ifndef _addrfrm_h_
#define _addrfrm_h_

// ADDRFRM.H
//
// DESCRIPTION:
//		This file contains the declarations of the various address book related
//		classes.
//

#include "outliner.h"
#include "apimsg.h"
#include "xp_core.h"
#include "addrbook.h"
#include "abmldlg.h"
#include "mailfrm.h"
#include "mnrccln.h"

// above the range for normal EN_ messages
#define PEN_ILLEGALCHAR     0x8000
			// sent to parent when illegal character hit
			// return 0 if you want parsed edit to beep

#define NETSCAPE_ADDRESS_FORMAT      "Netscape Address Book Format"
#define ADDRESSBOOK_SOURCETARGET_FORMAT "Netscape Address Book source-target"

#define IDW_ADDRESS_SLIDER			AFX_IDW_PANE_FIRST
#define IDW_DIRECTORY_PANE			(AFX_IDW_PANE_FIRST+1)
#define IDW_RESULTS_PANE			(AFX_IDW_PANE_FIRST+2)

// Definitions for column headings in the outliner control

#define DEF_VISIBLE_COLUMNS			5
#define ID_COLADDR_TYPE				1
#define ID_COLADDR_NAME				2
#define ID_COLADDR_EMAIL			3
#define ID_COLADDR_COMPANY			4
#define ID_COLADDR_PHONE			5
#define ID_COLADDR_LOCALITY			6
#define ID_COLADDR_NICKNAME			7

#define DEF_DIRVISIBLE_COLUMNS		2
#define ID_COLDIR_TYPE				1
#define ID_COLDIR_NAME				2

// array of indexes for IDB_ADDRESSBOOK bitmap
#define IDX_ADDRESSBOOKPERSON   0
#define IDX_ADDRESSBOOKLIST		1
#define IDX_ADDRESSBOOKPERCARD  2

// array of indexes for IDB_DIRLIST bitmap
#define IDX_DIRLDAPAB			0
#define IDX_DIRPERSONALAB		1

// call to build a menu of PABs
void WFE_MSGBuildAddressBookPopup( HMENU hmenu, BOOL bInHeaders = FALSE );

class CAddrCX: public CStubsCX {

public:
	CAddrCX() : CStubsCX (AddressCX, MWContextAddressBook) {}
	void DestroyContext();
};

/****************************************************************************
*
*	Class: CAddrBar
*
*	DESCRIPTION:
*		This class is used to process the dialog bar at the top of the address
*		book.  Mainly used for typedown.
*
****************************************************************************/
class CAddrBar : public CDialogBar
{
public:
    CAddrBar ( );
    virtual ~CAddrBar ( );
	void	UpdateDirectories();

// Attributes
public:
	int				m_iWidth;
	UINT			m_uTypedownClock;
	CString			m_save;
	int				m_savedir;
	HFONT			m_pFont;
  	XP_Bool			m_bRemoveLDAPDir;

#ifdef XP_WIN16
	CSize m_sizeDefault;
#endif

protected:
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddrBar)
public:
	void SetDirectoryIndex (int directory);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	//{{AFX_DATA(CAddrBar)
	CString	m_name;
	int		m_directory;
	//}}AFX_DATA

public:	
	BOOL Create( CWnd*, UINT, UINT, UINT );
	// Generated message map functions
	//{{AFX_MSG(CAddrBar)
	afx_msg int OnCreate( LPCREATESTRUCT );
	afx_msg void OnSize( UINT, int, int);
	afx_msg void OnTimer( UINT );
	afx_msg void OnChangeName();
	afx_msg void OnSetFocusName();
	afx_msg void OnChangeDirectory();
	afx_msg void OnExtDirectorySearch();
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/****************************************************************************
*
*	Class: CAddrFrame
*
*	DESCRIPTION:
*		This class is used to as the main frame window for the address book.
*
****************************************************************************/
class COutlinerView;
class CAddrOutliner;
class CAddrOutlinerParent;
class CDirOutliner;
class CDirOutlinerParent;
class CAddrEditProperties;
class CAddrEntryList;

class CAddrFrame : public CGenericFrame, public CStubsCX, public IMailFrame {
	DECLARE_DYNCREATE(CAddrFrame)

// Attributes
protected:

	friend class CAddrEntryList;

	CAddrBar	m_barAddr;
	int			m_iWidth;

	CMailNewsSplitter	*m_pSplitter;
	CAddrOutliner		*m_pOutliner;
	CAddrOutlinerParent *m_pOutlinerParent;
	CDirOutliner		*m_pDirOutliner;
	CDirOutlinerParent	*m_pDirOutlinerParent;
	CObArray			m_userPropList;			// Contains user property pointers
	CObArray			m_listPropList;			// Contains mailing list property pointers

	LPMSGLIST			m_pIAddrList;
	ABPane				*m_addrBookPane;
	int32				m_iProgress;
	BOOL				m_bSearching;

//	CAddrFrame(); // protected constructor used by dynamic creation
	virtual ~CAddrFrame();

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

	// From CStubsCX
	virtual void Progress(MWContext *pContext, const char *pMessage);
	virtual void SetProgressBarPercent(MWContext *pContext, int32 lPercent);
	virtual void AllConnectionsComplete(MWContext *pContext);
	virtual CWnd *GetDialogOwner() const { return (CWnd *) this; }

	// Support for IMsgList Interface (Called by CAddrEntryList)
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);

// Operations
public:
	CAddrFrame();
	static CAddrFrame *Open();
	static void UpdateMenu(HMENU hMenu, UINT &nID);

	// Support for IMailFrame
	virtual CMailNewsFrame *GetMailNewsFrame();
	virtual MSG_Pane *GetPane();
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);
	virtual void AttachmentCount(MSG_Pane *messagepane, void* closure,
								 int32 numattachments, XP_Bool finishedloading) {};
	virtual void UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure) {};

	enum { ToolInvalid = -1, ToolText = 0, ToolPictures = 1, ToolBoth = 2 };

	// Callback from LDAP search
	void SetSearchResults(MSG_ViewIndex index, int32 num);
	void CloseUserProperties (CAddrEditProperties* wnd, ABID entryID);
	void CloseListProperties (CABMLDialog* wnd, ABID entryID);
	XP_Bool IsSearching () { return m_bSearching; }

	void OnTypedown (char* name);
	void OnChangeDirectory (int dirIndex);
	void OnUpdateDirectorySelection (int dirIndex);
	void OnExtendedDirectorySearch();
	void OnDirectoryList(char* searchString);
	static void HandleErrorReturn(int XPErrorID, CWnd* parent = NULL, int errorID = 0);
	static void Close();
	void UpdateDirectories();
	DIR_Server * GetCurrentDirectoryServer ();

#ifndef _WIN32
	CWnd *CreateView(CCreateContext* pContext, UINT nID = AFX_IDW_PANE_FIRST);
#endif

	void DoUpdateAddressBook( CCmdUI* pCmdUI, AB_CommandType cmd, BOOL bUseCheck = TRUE );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddrFrame)
	public:
	protected:
	virtual BOOL PreTranslateMessage( MSG* pMsg );
	virtual void RecalcLayout( BOOL bNotify = TRUE ) { CFrameWnd::RecalcLayout( bNotify ); }
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL LoadFrame( UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, 
		CWnd* pParentWnd = NULL, CCreateContext* pContext = NULL );  
	virtual BOOL OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext );
	virtual void PostNcDestroy( ) {}; // Overridden to prevent auto-delete on destroy
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrFrame)
	afx_msg void OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI );
	afx_msg int OnCreate(LPCREATESTRUCT);
	afx_msg void OnUpdateSecureStatus(CCmdUI *pCmdUI);
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg void OnClose();
	afx_msg void OnFileClose();
	afx_msg void OnSize(UINT, int, int);
	afx_msg void OnExtDirectorySearch();
	afx_msg void OnUpdateSearch( CCmdUI *pCmdUI );
	afx_msg void OnImportFile();
	afx_msg void OnUpdateImport( CCmdUI *pCmdUI );
	afx_msg void OnExportFile();
	afx_msg void OnUpdateExport ( CCmdUI *pCmdUI );
	afx_msg void OnStopSearch();
	afx_msg void OnUpdateStopSearch ( CCmdUI *pCmdUI );
	afx_msg void OnViewCommandToolbar();
	afx_msg void OnUpdateViewCommandToolbar(CCmdUI* pCmdUI);
	afx_msg void OnLDAPSearch();
	afx_msg void OnComposeMsg(void);
	afx_msg void OnUpdateComposeMsg(CCmdUI*);
	afx_msg void OnCall(void);
	afx_msg void OnUpdateCall(CCmdUI*);
	afx_msg void OnAddUser(void);
	afx_msg void OnUpdateAddUser(CCmdUI*);
	afx_msg void OnAddAB(void);
	afx_msg void OnUpdateAddAB(CCmdUI*);
	afx_msg void OnAddDir(void);
	afx_msg void OnUpdateAddDir(CCmdUI*);
	afx_msg void OnHTMLDomains(void);
	afx_msg void OnUpdateHTMLDomains(CCmdUI*);
	afx_msg void OnAddList(void);
	afx_msg void OnUpdateAddList(CCmdUI*);
	afx_msg void OnSwitchSortFirstLast(void);
	afx_msg void OnUpdateSwitchSort(CCmdUI*);
	afx_msg void OnDeleteItem(void);
	afx_msg void OnUpdateDeleteItem(CCmdUI*);
	afx_msg void OnUndo(void);
	afx_msg void OnUpdateUndo(CCmdUI*);
	afx_msg void OnRedo(void);
	afx_msg void OnUpdateRedo(CCmdUI*);
	afx_msg	void OnItemProperties(void);
	afx_msg void OnUpdateItemProperties(CCmdUI*);
	afx_msg	void OnSortType(void);
	afx_msg void OnUpdateSortType(CCmdUI*);
	afx_msg	void OnSortName(void);
	afx_msg void OnUpdateSortName(CCmdUI*);
	afx_msg	void OnSortNickName(void);
	afx_msg void OnUpdateSortNickName(CCmdUI*);
	afx_msg	void OnSortEmailAddress(void);
	afx_msg void OnUpdateSortEmailAddress(CCmdUI*); 
	afx_msg	void OnSortCompany(void);
	afx_msg void OnUpdateSortCompany(CCmdUI*);
	afx_msg	void OnSortLocality(void);
	afx_msg void OnUpdateSortLocality(CCmdUI*);
	afx_msg	void OnSortAscending(void);
	afx_msg void OnUpdateSortAscending(CCmdUI*);
	afx_msg	void OnSortDescending(void);
	afx_msg void OnUpdateSortDescending(CCmdUI*);
	afx_msg void OnSave();
	afx_msg void OnDestroy();
	afx_msg void OnHelpMenu();
	afx_msg void OnUpdateShowAddressBookWindow(CCmdUI*);
	afx_msg void OnCreateCard();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
}; // END OF CLASS CAddrFrame()


/****************************************************************************
*
*	Class: CAddrOutliner
*
*	DESCRIPTION:
*		This class is the column/list object in the address book
*
****************************************************************************/
class CAddrOutliner : public CMSelectOutliner
{
friend class CAddrOutlinerParent;

protected:
	int					m_attribSortBy;
	int					m_lineindex;
	char*				m_pszExtraText;
    CLIPFORMAT			m_cfAddresses;
	CLIPFORMAT			m_cfSourceTarget;
	ABPane*				m_pane;
	int					m_iMysticPlane;
	BOOL				m_bSortAscending;
	MWContext*			m_pContext;
	int					m_dirIndex;
    AB_EntryLine		m_EntryLine;
	HFONT				m_hFont;
	CString				m_psTypedown;
	UINT				m_uTypedownClock;

public:
    CAddrOutliner ( );
    virtual ~CAddrOutliner ( );

	void	UpdateCount();
	void	OnTypedown (char* name);
	void	OnChangeDirectory(int dirIndex);
	int		GetSortBy() { return m_attribSortBy; }
	void	SetDirectoryIndex (int dirIndex);
	int		GetDirectoryIndex () { return m_dirIndex; }
	BOOL	GetSortAscending() { return m_bSortAscending; }

	virtual void OnSelChanged();
	virtual void OnSelDblClk();

	virtual HFONT GetLineFont( void *pLineData );
    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);

	void SetPane( ABPane *pane );
	ABPane* GetPane() { return m_pane; }
	DIR_Server * GetCurrentDirectoryServer ();

	MWContext *GetContext() { return m_pContext; }
	void SetContext( MWContext *pContext ) { m_pContext = pContext; }

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num);
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num);

    virtual BOOL RenderData ( UINT iColumn, CRect & rect, CDC & pdc, const char * );
    virtual BOOL DeleteItem ( int iLine );

    virtual BOOL ColumnCommand ( int iColumn, int iLine );

	virtual void InitializeClipFormats(void);
    virtual CLIPFORMAT * GetClipFormatList(void);
    virtual void AcceptDrop(int iLineNo, COleDataObject *object, DROPEFFECT effect);
	virtual COleDataSource * GetDataSource(void);
    virtual void PropertyMenu ( int iSel, UINT flags );

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrOutliner)
	afx_msg void OnTimer( UINT );
	afx_msg void OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

}; // END OF CLASS CAddrOutliner()


/****************************************************************************
*
*	Class: CAddrOutlinerParent
*
*	DESCRIPTION:
*		This class is the window around the column/list object in the address 
*		book.  It is mainly purpose is to draw the column headings.
*
****************************************************************************/
class CAddrOutlinerParent : public COutlinerParent 
{
public:
	CAddrOutlinerParent();
	virtual ~CAddrOutlinerParent();
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);
	virtual BOOL ColumnCommand ( int idColumn );

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrOutlinerParent)
	afx_msg void OnDestroy();
	afx_msg void OnLButtonUp ( UINT nFlags, CPoint point );

	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CAddrOutliner
*
*	DESCRIPTION:
*		This class is the column/list object in the address book
*
****************************************************************************/
class CDirOutliner : public CMSelectOutliner
{
friend class CDirOutlinerParent;

protected:
	int					m_attribSortBy;
	int					m_lineindex;
	char*				m_pszExtraText;
    CLIPFORMAT			m_cfAddresses;
	CLIPFORMAT			m_cfSourceTarget;
	ABPane*				m_pane;
	int					m_iMysticPlane;
	BOOL				m_bSortAscending;
	MWContext*			m_pContext;
	DIR_Server*			m_pDirLine;
	int					m_dirIndex;
	HFONT				m_hFont;

public:
    CDirOutliner ( );
    virtual ~CDirOutliner ( );

	void	UpdateCount();
	void	OnChangeDirectory(int dirIndex);
	int		GetSortBy() { return m_attribSortBy; }
	void	SetDirectoryIndex (int dirIndex);
	int		GetDirectoryIndex () { return m_dirIndex; }
	BOOL	GetSortAscending() { return m_bSortAscending; }

	virtual void OnSelChanged();
	virtual void OnSelDblClk();

	virtual HFONT GetLineFont( void *pLineData );
    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);

	void SetPane( ABPane *pane );
	ABPane* GetPane() { return m_pane; }

	MWContext *GetContext() { return m_pContext; }
	void SetContext( MWContext *pContext ) { m_pContext = pContext; }

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num);
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num);

    virtual BOOL RenderData ( UINT iColumn, CRect & rect, CDC & pdc, const char * );
    virtual BOOL DeleteItem ( int iLine );

    virtual BOOL ColumnCommand ( int iColumn, int iLine );

	virtual void InitializeClipFormats(void);
    virtual CLIPFORMAT * GetClipFormatList(void);
    virtual void AcceptDrop(int iLineNo, COleDataObject *object, DROPEFFECT effect);
	virtual COleDataSource * GetDataSource(void);
    virtual void PropertyMenu ( int iSel, UINT flags );

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrOutliner)
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

}; // END OF CLASS CAddrOutliner()


/****************************************************************************
*
*	Class: CDirOutlinerParent
*
*	DESCRIPTION:
*		This class is the window around the list of directories in the address 
*		book.  It is mainly purpose is to draw the column headings.
*
****************************************************************************/
class CDirOutlinerParent : public COutlinerParent 
{
public:
	CDirOutlinerParent();
	virtual ~CDirOutlinerParent();
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);
	virtual BOOL ColumnCommand ( int idColumn );

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CDirOutlinerParent)
	afx_msg void OnDestroy();
	afx_msg void OnLButtonUp ( UINT nFlags, CPoint point );

	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


#endif
