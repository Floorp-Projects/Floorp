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
// srchfrm.h : header file
//
#ifndef SEARCHFRM_H
#define SEARCHFRM_H
#include "outliner.h"
#include "mailmisc.h"
#include "statbar.h"
#include "msg_srch.h"
#include "srchobj.h"
#ifndef _WIN32
#include "ctl3d.h"
#endif


/////////////////////////////////////////////////////////////////////////////
// CSearchBar

class CSearchBar : public CDialogBar
{
public:
    CSearchBar ( );
    ~CSearchBar ( );

// Attributes
public:
	CMailFolderCombo m_wndScopes;
	CSearchObject m_searchObj;
	int m_iMoreCount;
	BOOL m_bLogicType;
	BOOL m_bLDAP;
	int m_iOrigFrameHeight;

	MSG_ScopeAttribute DetermineScope( DWORD dwData );

	void UpdateAttribList();
	void UpdateOpList();
	void UpdateOpList(int);
	int More();
	int Fewer();
	void OnAndOr();
	void Advanced();
	int ChangeLogicText();
	void InitializeAttributes (MSG_SearchValueWidget widgetValue, MSG_SearchAttribute attribValue);
	void BuildQuery (MSG_Pane* searchPane);

	int ClearSearch(BOOL bIsLDAPSearch);

protected:
	int m_iWidth, m_iHeight;
	int GetHeightNeeded();

#ifdef XP_WIN16
	CSize m_sizeDefault;
#endif

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSearchBar)
public:
	BOOL Create( CWnd*, UINT, UINT, UINT );
	CSize CalcFixedLayout( BOOL, BOOL );
	//}}AFX_VIRTUAL

public:	
	// Generated message map functions
	//{{AFX_MSG(CSearchBar)
	afx_msg int OnCreate( LPCREATESTRUCT );
	afx_msg void OnSize ( UINT, int , int );
	//}}AFX_MSG
#ifndef _WIN32
	afx_msg LRESULT OnDlgSubclass(WPARAM wParam, LPARAM lParam);

#endif
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSearchFrame frame

class CSearchOutliner;
class IMsgList;
class CMailQFButton;

class CSearchFrame : public CFrameWnd, public CStubsCX
{
	friend class CSearchResultsList;

// Attributes
public:
	CNetscapeStatusBar m_barStatus;
protected:
	CSearchBar m_barSearch;
	CDialogBar m_barAction;
	XP_List *m_listSearch;
	XP_List *m_listResult;

	BOOL m_bResultsShowing, m_bSearching, m_bIsLDAPSearch;
	int m_iHeight, m_iWidth;
	BOOL m_bDragCopying;
	int m_iRowSelected;
	CSearchOutliner *m_pOutliner;
	MSG_Pane *m_pSearchPane;
	IMsgList *m_pIMsgList;
	MSG_Master *m_pMaster;
	
	int m_iOrigFrameHeight;
	char * m_helpString;

	void ShowResults( BOOL );

	// From CStubsCX
	virtual void Progress(MWContext *pContext, const char *pMessage);
	virtual void SetProgressBarPercent(MWContext *pContext, int32 lPercent);
	virtual void AllConnectionsComplete(MWContext *pContext);
	virtual CWnd *GetDialogOwner() const { return (CWnd *) this; }


// Operations
	void Create();

	void UpdateScopes( CMailNewsFrame *pFrame );

public:
	CSearchFrame();
	void ModalStatusBegin( int iModalDelay );
	void ModalStatusEnd();
#ifndef _WIN32
	CWnd *CreateView(CCreateContext* pContext, UINT nID = AFX_IDW_PANE_FIRST);
#endif
	static void Open();
	static void Open( CMailNewsFrame *pFrame );
	static void Close();

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext );
	virtual void PostNcDestroy() {}; // Overridden to prevent auto-delete on destroy

	//Taken from MailFrm.h :  required to get FolderInfo from a Menu ID
	//Thanks Will!!
	virtual MSG_FolderInfo *FolderInfoFromMenuID( MSG_FolderInfo *mailRoot,
												  UINT &nBase, UINT nID );
	virtual MSG_FolderInfo *FolderInfoFromMenuID( UINT nID );



	// IMsgList implementation
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus) {}
	virtual void SelectItem( MSG_Pane* pane, int item ) {}

// Implementation
protected:
	virtual ~CSearchFrame();

#ifndef ON_COMMAND_RANGE
	BOOL OnCommand( WPARAM wParam, LPARAM lParam );
#endif

#ifndef ON_UPDATE_COMMAND_UI_RANGE
	virtual BOOL OnCmdMsg( UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo );  
#endif
	BOOL PreTranslateMessage( MSG* pMsg );

	void AdjustHeight(int dy);

	afx_msg void OnSetFocus(CWnd * pOldWnd);
	afx_msg int OnCreate(LPCREATESTRUCT);
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT, int, int);
	afx_msg void OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI );
	afx_msg void OnMore();
	afx_msg void OnUpdateMore( CCmdUI *pCmdUI );
	afx_msg void OnFewer();
	afx_msg void OnUpdateFewer( CCmdUI *pCmdUI );
	afx_msg void OnFind();
	afx_msg void OnUpdateFind( CCmdUI *pCmdUI );
	afx_msg void OnUpdateQuery( CCmdUI *pCmdUI );
	afx_msg void OnScope();
	afx_msg void OnSave();
	afx_msg void OnTo();
	afx_msg void OnUpdateTo( CCmdUI *pCmdUI );
	afx_msg void OnHelp();
	afx_msg void OnUpdateSave( CCmdUI *pCmdUI );
	afx_msg void OnUpdateHelp( CCmdUI *pCmdUI );
	afx_msg void OnNew();
  afx_msg void OnUpdateClearSearch(CCmdUI *pCmdUI);
	afx_msg void OnAdvanced();
	afx_msg void OnUpdateAdvanced(CCmdUI *pCmdUI );
  afx_msg void OnAttrib1();
	afx_msg void OnAttrib2();
	afx_msg void OnAttrib3();
	afx_msg void OnAttrib4();
	afx_msg void OnAttrib5();

  BOOL readyToSearch();

  afx_msg	void OnAndOr();
	afx_msg void OnUpdateAndOr(CCmdUI *pCmdUI );
  afx_msg void OnFileButton();
	afx_msg void OnUpdateFileButton(CCmdUI *pCmdUI );
	afx_msg void OnUpdateDeleteButton(CCmdUI *pCmdUI );
	afx_msg LONG OnFinishedAdvanced( WPARAM wParam, LPARAM lParam );
	afx_msg LONG OnFinishedHeaders(WPARAM wParam, LPARAM lParam );

	//context menu handlers
	afx_msg void OnUpdateDeleteMessage(CCmdUI* pCmdUI);
	afx_msg void OnDeleteMessage();
	afx_msg void OnFileMessage(UINT nID );
	afx_msg void OnUpdateFile( CCmdUI *pCmdUI );
	afx_msg void OnOpenMessage();
	afx_msg void OnUpdateOpenMessage(CCmdUI *pCmdUI);
	afx_msg void OnUpdateOnlineStatus(CCmdUI *pCmdUI);
	

	DECLARE_MESSAGE_MAP()

};


/////////////////////////////////////////////////////////////////////////////
// CSearchOutliner

class CSearchOutlinerParent;

class CSearchOutliner : public CMSelectOutliner
{
friend class CSearchOutlinerParent;

protected:
	MWContext           *m_pContext;
	MSG_SearchAttribute m_attribSortBy;
	XP_Bool	            m_bSortDescending;
	MSG_Pane            *m_pSearchPane;
	HFONT               m_hFont;
	int					m_iMysticPlane;
	CLIPFORMAT          m_cfSearchMessages;

	virtual void OnSelChanged();
	virtual void OnSelDblClk();

public:
    CSearchOutliner ( );
    ~CSearchOutliner ( );

	void ChangeResults (int num);

    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
	virtual HFONT GetLineFont( void *pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num);
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num);


	virtual void PropertyMenu ( int iSel, UINT flags );
    virtual BOOL DeleteItem ( int iLine );

	MWContext *GetContext() { return m_pContext; }
	void SetContext( MWContext *pContext ) { m_pContext = pContext; }
	void SetPane (MSG_Pane *pPane) { m_pSearchPane = pPane; }
	
	POINT GetHit() const {return m_ptHit;}

	//Drad and Drop
	virtual void InitializeClipFormats(void);
	CLIPFORMAT *GetClipFormatList(void);
	virtual COleDataSource * GetDataSource(void);

  void deleteMessages(BOOL bNoTrash);

  afx_msg int OnCreate(LPCREATESTRUCT);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnSetFocus(CWnd* pOldWnd);
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
// CSearchOutlinerParent

class CSearchOutlinerParent : public COutlinerParent 
{
public:
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);
	virtual BOOL ColumnCommand ( int idColumn );
};

/////////////////////////////////////////////////////////////////////////////
// CSearchView

class CSearchView: public COutlinerView {
    DECLARE_DYNCREATE(CSearchView)
public:
    CSearchView ( ) : COutlinerView ( )
    {
        m_pOutlinerParent = new CSearchOutlinerParent;
    }
};

#if 0

/////////////////////////////////////////////////////////////////////////////
// CLDAPSearchFrame

class CLDAPSearchFrame : public CSearchFrame
{
protected:
// Operations
	void Create();

public:
	static void Open();
	static void Close();

// Implementation
protected:
	virtual BOOL OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext );
	
	afx_msg int OnCreate(LPCREATESTRUCT);
	afx_msg void OnClose();
	afx_msg void OnScope();
	afx_msg void OnFind();
	afx_msg void OnAdd();
	afx_msg void OnUpdateAdd( CCmdUI *pCmdUI );
	afx_msg void OnTo();
	afx_msg void OnUpdateTo( CCmdUI *pCmdUI );
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CLDAPSearchOutliner

class CLDAPSearchOutliner: public CSearchOutliner
{
friend class CLDAPSearchOutlinerParent;

public:
	virtual int TranslateIcon ( void *);
	virtual HFONT GetLineFont( void *);

	//Drag and Drop
	virtual void InitializeClipFormats(void);
	CLIPFORMAT *GetClipFormatList(void);
	virtual COleDataSource * GetDataSource(void);


protected:

    CLIPFORMAT	m_cfAddresses;
	CLIPFORMAT	m_cfSourceTarget;

	// Generated message map functions
	//{{AFX_MSG(CLDAPSearchOutliner)
	afx_msg int OnCreate ( LPCREATESTRUCT );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CLPADSearchOutlinerParent

class CLDAPSearchOutlinerParent : public CSearchOutlinerParent 
{
public:
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
};

/////////////////////////////////////////////////////////////////////////////
// CLDAPSearchView

class CLDAPSearchView: public COutlinerView {
    DECLARE_DYNCREATE(CLDAPSearchView)
public:
    CLDAPSearchView ( ) : COutlinerView ( )
    {
        m_pOutlinerParent = new CLDAPSearchOutlinerParent;
    }
};

////////////////////////////////////////////////////////////////////////////
#endif

#endif
