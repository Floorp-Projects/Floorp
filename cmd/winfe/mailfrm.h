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

#ifndef _MAILFRM_H
#define _MAILFRM_H
#include "mailmisc.h"
#include "mailpriv.h"

#ifndef _APIMSG_H
#include "apimsg.h"
#endif

#ifndef DYNAMIC_DOWNCAST
#define DYNAMIC_DOWNCAST( classname, ptr ) ((classname *)(ptr))
#endif

#ifndef DOWNCAST
#define DOWNCAST( classname, ptr ) ((classname *)(ptr))
#endif

// mailfrm.h : header file
//

#define	SORT_BYDATE		0		
#define	SORT_BYSUBJECT	1
#define	SORT_BYSENDER	2
#define	SORT_BYNUMBER	3
#define	SORT_BYPRIORITY	4
#define SORT_BYTHREAD	5
#define SORT_BYSTATUS	6
#define SORT_BYSIZE		7
#define SORT_BYFLAG		8
#define SORT_BYUNREAD	9

#define THREADS_NEW		0
#define THREADS_ALL		1
#define THREADS_WATCHED	2
#define THREADS_KILLED	3
#define THREADS_NEWONLY	4

#define IDW_FOLDER_PANE			AFX_IDW_PANE_FIRST
#define IDW_THREAD_PANE			(AFX_IDW_PANE_FIRST+1)
#define IDW_MESSAGE_PANE		(AFX_IDW_PANE_FIRST+2)
#define IDW_CATEGORY_PANE		(AFX_IDW_PANE_FIRST+3)
#define IDW_ATTACHMENTS_PANE	(AFX_IDW_PANE_FIRST+4)
#define IDW_MESSAGE_VIEW		(AFX_IDW_PANE_FIRST+5)

/////////////////////////////////////////////////////////////////////////////
// CMailNewsCX

class CMailNewsCX: public CStubsCX {
protected:
	LPCHROME m_pIChrome;

	CFrameWnd *m_pFrame;

	int32 m_lPercent;
	CString m_csProgress;
	BOOL m_bAnimated;

public:
	CMailNewsCX(ContextType ctMyType, CFrameWnd *pFrame);
	~CMailNewsCX();

	LPUNKNOWN GetChrome() const { return m_pIChrome; }
	void SetChrome( LPUNKNOWN pChrome );

public:
	int32 QueryProgressPercent();
	void SetProgressBarPercent(MWContext *pContext, int32 lPercent);

	void SetDocTitle( MWContext *pContext, char *pTitle );

	void StartAnimation();
	void StopAnimation();

	void Progress(MWContext *pContext, const char *pMessage);
	void AllConnectionsComplete(MWContext *pContext);

	void UpdateStopState( MWContext *pContext );
    
	CWnd *GetDialogOwner() const;
};

class C3PaneMailFrame;
class CFolderFrame;
class CMessageFrame;
class CMailInfoBar;
class CMessageView;
class CFolderPropertyPage;
class CDiskSpaceProptertyPage;

extern int g_iModalDelay;
#define MODAL_DELAY (g_iModalDelay)

/////////////////////////////////////////////////////////////////////////////
// CMailNewsFrame frame

class CMailNewsFrame : public CGenericFrame, public IMailFrame
{
	DECLARE_DYNCREATE(CMailNewsFrame)

protected:
	unsigned long m_ulRefCount;

	HACCEL m_hAccel;

public:
// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// Support for IMailFrame
	virtual CMailNewsFrame *GetMailNewsFrame();
	virtual MSG_Pane *GetPane();
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);
	virtual void AttachmentCount(MSG_Pane *messagepane, void* closure,
								 int32 numattachments, XP_Bool finishedloading);
	virtual void UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure);

protected:
	CMailNewsFrame();           // protected constructor used by dynamic creation

// Attributes

	MSG_Pane *m_pPane;
	MSG_Pane *m_pMessagePane;
	MSG_Master *m_pMaster;
	BOOL m_bNews; // Are we showing mail or news?
	BOOL m_bCategory;
	CMailNewsOutliner *m_pOutliner;
	CMailInfoBar *m_pInfoBar;


	MSG_AttachmentData *m_pAttachmentData;
	int32 m_nAttachmentCount;
	CMessageView *m_pMessageView;

protected:
    virtual ~CMailNewsFrame();

	BOOL LoadAccelerators( UINT nID );
	BOOL LoadAccelerators( LPCSTR lpszResource );

public:
	// Attributes
	CAbstractCX *GetContext() const { return GetMainContext(); }

	virtual LPCTSTR GetWindowMenuTitle() { return _T(""); };

	BOOL IsNews() const { return m_bNews; }
	virtual MessageKey GetCurMessage() const { return MSG_MESSAGEKEYNONE; }
	virtual MSG_FolderInfo *GetCurFolder() const { return NULL; }

	static BOOL CheckWizard( CWnd *pParent = NULL ); // See if prefs have been initialized

	static C3PaneMailFrame *GetLastThreadFrame(CFrameWnd *pExclude = NULL);
	static CMessageFrame *GetLastMessageFrame(CFrameWnd *pExclude = NULL);

	static void UpdateMenu( MSG_FolderInfo *mailRoot,
							HMENU hMenu, UINT &nID, int nStart = 0);

	void ActivateFrame( int nCmdShow = -1 );
	virtual void RefreshNewEncoding(int16 doc_csid, BOOL bSave = TRUE);
	static BOOL CleanupFolders(MSG_Pane *pPane);


protected:
	virtual void SetSort( int idSort ) {}
	virtual void SetCSID( int csid );

	virtual MSG_FolderInfo *FolderInfoFromMenuID( MSG_FolderInfo *mailRoot,
												  UINT &nBase, UINT nID );
	virtual MSG_FolderInfo *FolderInfoFromMenuID( UINT nID );

	int m_iMessageMenuPos;
	int m_iMoveMenuPos;
	int m_iCopyMenuPos;
	int m_iAddAddressBookMenuPos;

	int m_iFileMenuPos;
	int m_iAttachMenuPos;

	virtual void UpdateFileMenus();
	virtual void UpdateAddressBookMenus();
	virtual void UpdateAttachmentMenus();

	void ModalStatusBegin( int iModalDelay );
	void ModalStatusEnd();

	void DoUpdateCommand( CCmdUI* pCmdUI, MSG_CommandType cmd, BOOL bUseCheck = TRUE );
	void DoCommand(MSG_CommandType msgCommand, int iModalDelay = -1, BOOL bAsync = TRUE );
	void DoMessageCommand( MSG_CommandType );
	void DoUpdateMessageCommand( CCmdUI *, MSG_CommandType, BOOL = TRUE );
	virtual void DoUpdateNavigate( CCmdUI* pCmdUI, MSG_MotionType cmd ) {}
	virtual void DoNavigate( MSG_MotionType msgCommand ) {}

	void DoSubscribe(MSG_Host* pThisHost);

	// MFC Overrides
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage( MSG* pMsg );
	virtual void GetMessageString( UINT nID, CString& rMessage ) const;

	// Message Map
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint( );
	afx_msg	HCURSOR OnQueryDragIcon();
	afx_msg void OnClose( );
	afx_msg void OnDestroy( );
	afx_msg void OnInitMenuPopup( CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu );
	afx_msg LRESULT CMailNewsFrame::OnButtonMenuOpen(WPARAM wParam, LPARAM lParam);
  
#ifndef ON_COMMAND_RANGE
	afx_msg BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
#endif

	// File Menu Items
	afx_msg BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	afx_msg void OnNew ();
	afx_msg void OnUpdateNew ( CCmdUI* pCmdUI );
	afx_msg void OnNewNewsgroup();
	afx_msg void OnUpdateNewNewsgroup( CCmdUI *pCmdUI );
	afx_msg void OnNewNewshost();
	afx_msg void OnUpdateNewNewshost( CCmdUI *pCmdUI );
	afx_msg void OnNewCategory();
	afx_msg void OnUpdateNewCategory( CCmdUI *pCmdUI );
	afx_msg void OnSave ();
	afx_msg void OnUpdateSave ( CCmdUI* pCmdUI );
	afx_msg void OnEditMessage ();
	afx_msg void OnUpdateEditMessage( CCmdUI* pCmdUI );
	afx_msg void OnAddNewsGroup();
	afx_msg void OnUpdateAddNewsGroup ( CCmdUI * pCmdUI );
	afx_msg void OnSubscribe ();
	afx_msg void OnUpdateSubscribe ( CCmdUI * pCmdUI );
	afx_msg void OnUnsubscribe ();
	afx_msg void OnUpdateUnsubscribe ( CCmdUI * pCmdUI );
	afx_msg void OnOpenAttach( UINT nID );
	afx_msg void OnUpdateOpenAttach( CCmdUI *pCmdUI );
	afx_msg void OnSaveAttach( UINT nID );
	afx_msg void OnUpdateSaveAttach( CCmdUI *pCmdUI );
	afx_msg void OnAttachProperties( UINT nID );
	afx_msg void OnUpdateAttachProperties( CCmdUI *pCmdUI );

	// Edit Menu Items
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo( CCmdUI* pCmdUI );
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo( CCmdUI* pCmdUI );
	afx_msg void OnDeleteMessage();
	afx_msg void OnReallyDeleteMessage();
	afx_msg void OnUpdateDeleteMessage( CCmdUI *pCmdUI );
	afx_msg void OnDeleteFolder();
	afx_msg void OnUpdateDeleteFolder( CCmdUI *pCmdUI );
	afx_msg void OnSelectAll();
	afx_msg void OnUpdateSelectAll ( CCmdUI * pCmdUI );
	afx_msg void OnSetupWizard ();
	afx_msg void OnUpdateSetupWizard( CCmdUI *pCmdUI );
	afx_msg void OnServerStuff();
	afx_msg void OnUpdateServerStuff( CCmdUI *pCmdUI );
	afx_msg void OnEditProperties();
	afx_msg void OnUpdateProperties( CCmdUI *pCmdUI );

	// View/Sort Menu Items
	afx_msg void OnSortAgain ();
	afx_msg void OnUpdateSortAgain ( CCmdUI* pCmdUI );
	afx_msg void OnThread ();
	afx_msg void OnUpdateThread ( CCmdUI* pCmdUI );
	afx_msg void OnAscending ();
	afx_msg void OnUpdateAscending ( CCmdUI* pCmdUI );
	afx_msg void OnDescending ();
	afx_msg void OnUpdateDescending ( CCmdUI* pCmdUI );
	afx_msg void OnSortNumber ();
	afx_msg void OnUpdateSortNumber ( CCmdUI* pCmdUI );
	afx_msg void OnSortDate ();
	afx_msg void OnUpdateSortDate ( CCmdUI* pCmdUI );
	afx_msg void OnSortSubject ();
	afx_msg void OnUpdateSortSubject ( CCmdUI* pCmdUI );
	afx_msg void OnSortSender ();
	afx_msg void OnUpdateSortSender ( CCmdUI* pCmdUI );
	afx_msg void OnSortPriority();
	afx_msg void OnUpdateSortPriority( CCmdUI *pCmdUI );
	afx_msg void OnSortStatus();
	afx_msg void OnUpdateSortStatus( CCmdUI *pCmdUI );
	afx_msg void OnSortSize();
	afx_msg void OnUpdateSortSize( CCmdUI *pCmdUI );
	afx_msg void OnSortFlag();
	afx_msg void OnUpdateSortFlag( CCmdUI *pCmdUI );
	afx_msg void OnSortUnread();
	afx_msg void OnUpdateSortUnread( CCmdUI *pCmdUI );
	afx_msg void OnViewAll();
	afx_msg void OnUpdateViewAll( CCmdUI * pCmdUI );
	afx_msg void OnViewKilled();
	afx_msg void OnUpdateViewKilled( CCmdUI * pCmdUI );
	afx_msg void OnViewWatched();
	afx_msg void OnUpdateViewWatched( CCmdUI * pCmdUI );
	afx_msg void OnViewNew();
	afx_msg void OnUpdateViewNew( CCmdUI * pCmdUI );
	afx_msg void OnViewNewOnly();
	afx_msg void OnUpdateViewNewOnly( CCmdUI * pCmdUI );
	afx_msg void OnHeadersMicro();
	afx_msg void OnUpdateHeadersMicro( CCmdUI *pCmdUI );
	afx_msg void OnHeadersShort();
	afx_msg void OnUpdateHeadersShort( CCmdUI *pCmdUI );
	afx_msg void OnHeadersAll();
	afx_msg void OnUpdateHeadersAll( CCmdUI *pCmdUI );
	afx_msg void OnMessageReuse();
	afx_msg void OnUpdateMessageReuse(CCmdUI* pCmdUI);
	afx_msg void OnViewInline();
	afx_msg void OnUpdateViewInline(CCmdUI*);
	afx_msg void OnViewAsLinks();
	afx_msg void OnUpdateViewAsLinks(CCmdUI*);

	// Navigate Menu Items
	afx_msg void OnPreviousUnread ();
	afx_msg void OnUpdatePreviousUnread ( CCmdUI* pCmdUI );
	afx_msg void OnPrevious ();
	afx_msg void OnUpdatePrevious ( CCmdUI* pCmdUI );
	afx_msg void OnNextUnread ();
	afx_msg void OnUpdateNextUnread ( CCmdUI* pCmdUI );
	afx_msg void OnFirstUnread();
	afx_msg void OnUpdateFirstUnread(CCmdUI *pCmdUI);
	afx_msg void OnNext();
	afx_msg void OnUpdateNext ( CCmdUI* pCmdUI );
	afx_msg void OnNextFolder();
	afx_msg void OnUpdateNextFolder( CCmdUI* pCmdUI );
	afx_msg void OnNextUnreadThread();
	afx_msg void OnUpdateNextUnreadThread(CCmdUI* pCmdUI);
	afx_msg void OnNextUnreadFolder();
	afx_msg void OnUpdateNextUnreadFolder(CCmdUI* pCmdUI);
	afx_msg void OnNextFlagged();
	afx_msg void OnUpdateNextFlagged(CCmdUI* pCmdUI);
	afx_msg void OnPreviousFlagged();
	afx_msg void OnUpdatePreviousFlagged(CCmdUI* pCmdUI);
	afx_msg void OnFirstFlagged();
	afx_msg void OnUpdateFirstFlagged(CCmdUI* pCmdUI);
	
	/* hack for message backtracking */
	afx_msg void OnGoBack();
	afx_msg void OnUpdateGoBack( CCmdUI* pCmdUI );
	afx_msg void OnGoForward();
	afx_msg void OnUpdateGoForward( CCmdUI* pCmdUI );

	// Message Menu Items
    afx_msg void OnGetMail ();
    afx_msg void OnUpdateGetMail ( CCmdUI* pCmdUI );
    afx_msg void OnGetNext();
    afx_msg void OnUpdateGetNext( CCmdUI* pCmdUI );
	afx_msg void OnDeliverNow();
	afx_msg void OnUpdateDeliverNow(CCmdUI*);
	afx_msg void OnReply ();
	afx_msg void OnUpdateReply ( CCmdUI* pCmdUI );
	afx_msg void OnReplyButton();
	afx_msg void OnReplyAll ();
	afx_msg void OnUpdateReplyAll ( CCmdUI* pCmdUI );
	afx_msg void OnPostNew ();
	afx_msg void OnUpdatePostNew ( CCmdUI* pCmdUI );
	afx_msg void OnPostAndMailReply ();
	afx_msg void OnUpdatePostAndMailReply ( CCmdUI * pCmdUI );
	afx_msg void OnPostAndMail ();
	afx_msg void OnUpdatePostAndMail ( CCmdUI * pCmdUI );
	afx_msg void OnPostReply ();
	afx_msg void OnUpdatePostReply ( CCmdUI* pCmdUI );
	afx_msg void OnForwardButton ();
	afx_msg void OnUpdateForwardButton  ( CCmdUI* pCmdUI );
	afx_msg void OnForward ();
	afx_msg void OnUpdateForward  ( CCmdUI* pCmdUI );
	afx_msg void OnForwardQuoted ();
	afx_msg void OnUpdateForwardQuoted ( CCmdUI * pCmdUI );
	afx_msg void OnForwardInline ();
	afx_msg void OnUpdateForwardInline ( CCmdUI * pCmdUI );
	afx_msg void OnKill();
	afx_msg void OnUpdateKill( CCmdUI *pCmdUI );
	afx_msg void OnWatch();
	afx_msg void OnUpdateWatch( CCmdUI *pCmdUI );
	afx_msg void OnCancelMessage();
	afx_msg void OnUpdateCancelMessage( CCmdUI * pCmdUI );
	afx_msg void OnRetrieveSelected();
	afx_msg void OnUpdateRetrieveSelected( CCmdUI *pCmdUI );
	afx_msg void OnMarkMessagesRead();
	afx_msg void OnUpdateMarkMessagesRead( CCmdUI *pCmdUI );
	afx_msg void OnMarkMessagesUnread();
	afx_msg void OnUpdateMarkMessagesUnread( CCmdUI *pCmdUI );
	afx_msg void OnMarkMessagesLater();
	afx_msg void OnUpdateMarkMessagesLater( CCmdUI *pCmdUI );
	afx_msg void OnMarkThreadRead();
	afx_msg void OnUpdateMarkThreadRead( CCmdUI *pCmdUI );
	afx_msg void OnMarkAllRead();
	afx_msg void OnUpdateMarkAllRead( CCmdUI *pCmdUI );
	afx_msg void OnCatchup();
	afx_msg void OnUpdateCatchup( CCmdUI *pCmdUI );
	afx_msg void OnRetrieveOffline();
	afx_msg void OnUpdateRetrieveOffline( CCmdUI *pCmdUI );
	afx_msg void OnMarkMessages();
	afx_msg void OnUpdateMarkMessages( CCmdUI *pCmdUI );
	afx_msg void OnUnmarkMessages();
	afx_msg void OnUpdateUnmarkMessages( CCmdUI *pCmdUI );
	afx_msg void OnWrapLongLines ( );
	afx_msg void OnUpdateWrapLongLines ( CCmdUI * pCmdUI );
	afx_msg void OnViewRot13 ( );
	afx_msg void OnUpdateViewRot13 ( CCmdUI * pCmdUI );

	// Organize Menu Items
	afx_msg void OnNewFolder ();
	afx_msg void OnUpdateNewFolder ( CCmdUI* pCmdUI );
	afx_msg void OnRenameFolder();
	afx_msg void OnUpdateRenameFolder( CCmdUI *pCmdUI );
	afx_msg void OnCompressFolder ();
	afx_msg void OnUpdateCompressFolder ( CCmdUI* pCmdUI );
	afx_msg void OnCompressAll ();
	afx_msg void OnUpdateCompressAll ( CCmdUI* pCmdUI );
	afx_msg void OnEmptyTrash ();
	afx_msg void OnUpdateEmptyTrash ( CCmdUI * pCmdUI );

	//old address book
	afx_msg void OnAddToAddressBook ();
	afx_msg void OnUpdateAddToAddressBook ( CCmdUI * pCmdUI );
	afx_msg void OnAddAllToAddressBook ();
	afx_msg void OnUpdateAddAllToAddressBook ( CCmdUI * pCmdUI );

	//new address book
	afx_msg void OnUpdateAddSenderAddressBook(CCmdUI *pCmdUI);
	afx_msg void OnUpdateAddAllAddressBook(CCmdUI *pCmdUI);
	afx_msg void OnAddSenderAddressBook(UINT nID);
	afx_msg void OnAddAllAddressBook(UINT nID);

	// Stop
	afx_msg void OnInterrupt();
	afx_msg void OnUpdateInterrupt( CCmdUI* pCmdUI );

	// Non menu Items
	afx_msg void OnTips();
	afx_msg void OnSearch();
	afx_msg void OnFilter();
	afx_msg void OnQuickFile();
	afx_msg void OnUpdateQuickFile( CCmdUI *pCmdUI );
	afx_msg void OnUpdateSecurity(CCmdUI *pCmdUI);
	afx_msg void OnUpdateSecureStatus(CCmdUI *pCmdUI);

	afx_msg void OnPriorityLowest();
	afx_msg void OnPriorityLow();
	afx_msg void OnPriorityNormal();
	afx_msg void OnPriorityHigh();
	afx_msg void OnPriorityHighest();
	afx_msg void OnUpdatePriority( CCmdUI *pCmdUI );

	afx_msg void OnToggleEncoding1();
    afx_msg void OnToggleEncoding2();
    afx_msg void OnToggleEncoding3();
    afx_msg void OnToggleEncoding4();
    afx_msg void OnToggleEncoding5();
    afx_msg void OnToggleEncoding6();
    afx_msg void OnToggleEncoding7();
    afx_msg void OnToggleEncoding8();
    afx_msg void OnToggleEncoding9();
    afx_msg void OnToggleEncoding10();
    afx_msg void OnToggleEncoding11();
    afx_msg void OnToggleEncoding12();
    afx_msg void OnToggleEncoding13();
    afx_msg void OnToggleEncoding14();
    afx_msg void OnToggleEncoding15();
    afx_msg void OnToggleEncoding16();
    afx_msg void OnToggleEncoding17();
    afx_msg void OnToggleEncoding18();
    afx_msg void OnToggleEncoding19();
    afx_msg void OnToggleEncoding20();
    afx_msg void OnUpdateEncoding(CCmdUI* pCmdUI);

	DECLARE_MESSAGE_MAP()

	void OnUpdateAddAddressBook(CCmdUI *pCmdUI, MSG_CommandType command);
	void OnAddAddressBook(UINT nID, MSG_CommandType command);

};

/////////////////////////////////////////////////////////////////////////////
// CMsgListFrame frame

class CMsgListFrame : public CMailNewsFrame, public IMsgList {
	DECLARE_DYNCREATE(CMsgListFrame)
public:
// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// Support for IMsgList Interface (Called by CMailMsgList)
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							   int *focus);
	virtual void SelectItem( MSG_Pane* pane, int item );

	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo);
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo);
protected:
	CMsgListFrame();           // protected constructor used by dynamic creation

	// Message Map
	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
};

extern MSG_MessagePaneCallbacks MsgPaneCB;
extern int iMailFrameCount;

/////////////////////////////////////////////////////////////////////////////
#endif
