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

#ifndef SUBNEWS_H
#define SUBNEWS_H

#include "property.h"
#include "outliner.h"
#include "mailmisc.h"
#include "statbar.h"
#include "apimsg.h"

// Definitions for column headings in the outliner control
#define ID_COLNEWS_NAME			1
#define ID_COLNEWS_SUBSCRIBE	2
#define ID_COLNEWS_POSTINGS		3

class CSubscribePropertySheet;
class CNewsgroupsOutliner;
class CSubscribeList;

/////////////////////////////////////////////////////////////////////////////
//	Class: CServersCombo

class CServersCombo: public CComboBox
{
public:
	CServersCombo();
	~CServersCombo();

protected:

	BOOL m_bStaticCtl;
	HFONT m_hFont, m_hBoldFont;
	LPIMAGEMAP m_pIImageMap;
	LPUNKNOWN m_pIImageUnk;

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );
	
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
//	Class: CSubscribeCX
class CSubscribeCX: public CStubsCX
{
protected:
	CSubscribePropertySheet* m_pSheet;

 	int32 m_lPercent;
	CString m_csProgress;
	BOOL m_bAnimated;

public:
	CSubscribeCX(CNetscapePropertySheet* pSheet);

	virtual CWnd *GetDialogOwner() const { return (CWnd*)m_pSheet;	}

	int32 QueryProgressPercent();
	void SetProgressBarPercent(MWContext *pContext, int32 lPercent);

	void Progress(MWContext *pContext, const char *pMessage);
	void AllConnectionsComplete(MWContext *pContext);

	void UpdateStopState( MWContext *pContext );

	CSubscribePropertySheet* GetPropertySheet() {return m_pSheet;}
};

/////////////////////////////////////////////////////////////////////////////
//	Class: CNewsgroupsOutlinerParent
class CNewsgroupsOutlinerParent : public COutlinerParent 
{
public:
	CNewsgroupsOutlinerParent();
	virtual ~CNewsgroupsOutlinerParent();
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);

	void SetPropertySheet(CSubscribePropertySheet* pSheet) { m_pSheet = pSheet; }

// Implementation
protected:

	CSubscribePropertySheet* m_pSheet;

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSubscribePropertyPage dialog
class CSubscribePropertyPage : public CNetscapePropertyPage
{
public:

    CSubscribePropertyPage(CWnd *pParent, MWContext * pContext,
 		                   MSG_SubscribeMode nMode, UINT nID);
   ~CSubscribePropertyPage();
	
    BOOL m_bFromTyping;
	BOOL m_bProcessGetDeletion;
	BOOL m_bListChangeStarting;

    CNewsgroupsOutliner * GetOutliner() { return m_pOutliner; }

	CServersCombo * GetServerCombo() { return &m_ServerCombo; }

	MSG_SubscribeMode GetMode() { return m_nMode; }


    MWContext* GetContext();
    CSubscribeCX* GetSubscribeContext();
    MSG_Pane * GetPane(); 
    CSubscribeList*  GetList();  
    CSubscribeList**  GetListHandle();  
    void SetSubscribeContext(CSubscribeCX* pCX);
    void SetPane(MSG_Pane *pPane); 
    void SetList(CSubscribeList* pList); 

 	void DoSelChanged(MSG_GroupNameLine* pGroup);
	void CheckSubscribeButton(MSG_GroupNameLine* pGroup);
	Bool IsOutlinerHasFocus();
	void EnableAllControls(BOOL bEnable);
	void DoStopListChange();
	void ClearNewsgroupSelection();

	virtual void ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									int32 num);

	virtual BOOL OnInitDialog();
    virtual BOOL OnSetActive();
	virtual BOOL OnKillActive( );

	//{{AFX_VIRTUAL(CSubscribePropertyPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

protected: 
	
	CSubscribePropertySheet* m_pParent;
    BOOL m_bActivated;
    BOOL m_bSelChanged;
    BOOL m_bNotifyAll;  //MAG_NotifyALl when outliner is not visible
    BOOL m_bInitDialog;
	BOOL m_bDoShowWindow;
    UINT m_uTimer;  
	XP_Bool m_bAsynchronous;

 	MSG_Host** m_hNewsHost;
    MSG_SubscribeMode m_nMode;

	CNewsgroupsOutlinerParent	m_OutlinerParent;
	CNewsgroupsOutliner			*m_pOutliner;

	CServersCombo m_ServerCombo;

	BOOL InitSubscribePage();
	BOOL CreateSubscribePage();
	void SetNewsHosts(MSG_Master* pMaster);
	void CleanupOnClose();

	afx_msg void OnAddServer();
	afx_msg void OnSubscribeNewsgroup();
	afx_msg void OnUnsubscribeNewsgroup();
	afx_msg void OnChangeServer();
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
   DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
//	Class: CNewsgroupsOutliner

class CNewsgroupsOutliner : public CMailNewsOutliner
{
friend class CNewsgroupsOutlinerParent;

protected:
	int					m_attribSortBy;
	BOOL				m_bSortAscending;
	char*				m_pszExtraText;
    OutlinerAncestorInfo * m_pAncestor;
    MSG_GroupNameLine	m_GroupLine;
 	BOOL				m_bSelChanged;

	CSubscribePropertyPage* m_pPage;

public:
    CNewsgroupsOutliner ( );
    virtual ~CNewsgroupsOutliner ( );

	void SetPage(CSubscribePropertyPage *pPage) { m_pPage = pPage; }
	CSubscribePropertyPage * GetPage() { return m_pPage; }

	void DeselectItem();
	BOOL SelectInitialItem();

	virtual void OnSelChanged();
	virtual void OnSelDblClk();
    virtual int ToggleExpansion ( int iLine );

	virtual int GetDepth( int iLine );
	virtual int GetNumChildren( int iLine );
	virtual BOOL IsCollapsed( int iLine );
	virtual BOOL ColumnCommand(int iColumn, int iLine);

    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual BOOL RenderData ( UINT idColumn, CRect & rect, CDC & dc, const char * text);
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);
};

/////////////////////////////////////////////////////////////////////////////
//	Class: CSubscribeList
class CSubscribeList: public IMsgList 
{

	CSubscribePropertyPage *m_pSubscribePage;
	unsigned long m_ulRefCount;

public:
// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// IMsgList Interface
	virtual void ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									int32 num);
	virtual void GetSelection(MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus);
	virtual void SelectItem(MSG_Pane* pane, int item);
	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}

	void SetSubscribePage(CSubscribePropertyPage * pPage) 
		{ m_pSubscribePage = pPage; } 

	CSubscribeList(CSubscribePropertyPage *pPage) 
	{
		m_ulRefCount = 0;
		m_pSubscribePage = pPage;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CAllNewsgroupsPage dialog
class CAllNewsgroupsPage : public CSubscribePropertyPage
{
public:

    CAllNewsgroupsPage(CWnd *pParent, MWContext * pContext = NULL,
 		               MSG_SubscribeMode nMode = MSG_SubscribeAll);
	
	enum { IDD = IDD_NEWSGROUP_ALL };

	afx_msg void OnGetDeletions();



	//{{AFX_VIRTUAL(CAllNewsgroupsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();
	virtual BOOL OnKillActive( );

protected: 

	afx_msg void OnChangeNewsgroup();
	afx_msg void OnExpandAll();
	afx_msg void OnCollapseAll();
	afx_msg void OnStop();
   DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSearchNewsgroupPage dialog
class CSearchNewsgroupPage : public CSubscribePropertyPage
{
public:
    CSearchNewsgroupPage(CWnd *pParent, MWContext * pContext = NULL,
 		                 MSG_SubscribeMode nMode = MSG_SubscribeSearch);
	
	enum { IDD = IDD_NEWSGROUP_SEARCH };

	//{{AFX_VIRTUAL(CSearchNewsgroupPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

protected:              

	afx_msg void OnSearchNow();
	afx_msg void OnStop();
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CNewNewsgroupsPage dialog
class CNewNewsgroupsPage : public CSubscribePropertyPage
{
public:
    CNewNewsgroupsPage(CWnd *pParent, MWContext * pContext = NULL,
 		               MSG_SubscribeMode nMode = MSG_SubscribeNew);
	
	enum { IDD = IDD_NEWSGROUP_NEW };

	BOOL	m_bGetNew;

	//{{AFX_VIRTUAL(CNewNewsgroupsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();
	virtual BOOL OnKillActive( );

protected:              

	afx_msg void OnGetNew();
	afx_msg void OnClearNew();
	afx_msg void OnStop();
    DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
// CSubscibePropertySheet
class CSubscribePropertySheet : public CNetscapePropertySheet
{
public:

    CSubscribePropertySheet(CWnd *pParent, MWContext * pContext = NULL, const char* pName = NULL);
    ~CSubscribePropertySheet();
	
    CSubscribeCX* GetSubscribeContext() { return m_pCX; }
    void SetSubscribeContext(CSubscribeCX* pCX) { m_pCX = pCX; } 
	MSG_Pane*  GetSubscribePane() { return m_pSubscribePane; }
    void SetSubscribePane(MSG_Pane *pPane) { m_pSubscribePane = pPane; }
    CSubscribeList*  GetSubscribeList() { return m_pSubscribeList; }
    CSubscribeList**  GetSubscribeHandle() { return &m_pSubscribeList; }
    void SetSubscribeList(CSubscribeList* pList) { m_pSubscribeList = pList; }
    MSG_Host* GetHost() { return m_pCurrentHost; }
    void SetHost(MSG_Host *pHost) { m_pCurrentHost = pHost; }

	void SetStatusText(const char* pMessage);
	void Progress(const char *pMessage);
	void SetProgressBarPercent(int32 lPercent);
	void StartAnimation();
	void StopAnimation();
	void AllConnectionsComplete(MWContext *pContext);
	void AddServer(MSG_Host* pHost);
	void EnableNonImapPages(BOOL bEnable);


	//In Win16, GetActivePage() is a protected
	CSubscribePropertyPage*  GetCurrentPage() 
		{ return (CSubscribePropertyPage*)GetActivePage();	}

	CAllNewsgroupsPage* GetAllGroupPage() {return m_pAllGroupPage;}

	virtual void OnHelp();

	int nameWidth;
	int subscribeWidth;
	int postWidth;
	int namePos;
	int subscribePos;
	int postPos;

protected:
	
	CNetscapeStatusBar	m_barStatus;
 	int				m_iProgress;

	MSG_Host*		m_pCurrentHost;

	CSubscribeCX*	m_pCX;
	MSG_Pane*		m_pSubscribePane;
	CSubscribeList*	m_pSubscribeList;
	CSubscribePropertyPage *m_pNewPage;

	CAllNewsgroupsPage *m_pAllGroupPage;
	CSearchNewsgroupPage *m_pSearchGroupPage;
	CNewNewsgroupsPage *m_pNewGroupPage;

	BOOL			m_bCommitingStart;


	void CreateProgressBar();

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
#ifdef _WIN32
	virtual BOOL OnInitDialog();
#else
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
#endif
    DECLARE_MESSAGE_MAP()
};

class CServerTypeDialog : public CDialog 
{
// Attributes
public:

	CServerTypeDialog(CWnd* pParent);

    enum { IDD = IDD_NEWSGROUP_SERVERTYPE };

	MSG_Host *GetNewHost() { return m_pHost; }

	//{{AFX_VIRTUAL(CNewNewsgroupsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

	// Implementation
protected:

	MSG_Host *m_pHost;
 
	afx_msg void OnOK();
	DECLARE_MESSAGE_MAP()
};


#endif SUBNEWS_H
