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

#ifndef _THRDFRM_H
#define _THRDFRM_H

#include "mailfrm.h"

/////////////////////////////////////////////////////////////////////////////
// C3PaneMailFrame frame

class C3PaneMailFrame : public CMsgListFrame
{
	DECLARE_DYNCREATE(C3PaneMailFrame)
protected:
	C3PaneMailFrame();

	enum { actionNone, actionSelectFirst, actionNavigation, actionSelectKey };

	int m_actionOnLoad;
	MSG_MotionType m_navPending;
	MessageKey m_selPending;

	int m_nLoadingFolder;
	BOOL m_bDragCopying;

	BOOL m_bNoScrollHack;

// Attibutes
	BOOL m_bWantToGetMail;
	
	COutlinerParent *m_pOutlinerParent;
	MSG_Pane *m_pFolderPane;
	CFolderOutliner *m_pFolderOutliner;
	COutlinerParent *m_pFolderOutlinerParent;
	CMailNewsSplitter *m_pFolderSplitter;
	CMailNewsSplitter *m_pThreadSplitter;
	CThreadStatusBar m_barStatus;

	void UIForFolder( MSG_FolderInfo *folderInfo );
	void DoOpenMessage(BOOL bReuse);
	void BlankOutThreadPane();
	void BlankOutMessagePane(MSG_FolderInfo *folderInfo = NULL);

// IMailFrame override
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);

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

// Overrides
	virtual BOOL PreTranslateMessage( MSG* pMsg );
	virtual BOOL OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext );

	void SwitchUI();
	void SetIsNews( BOOL bNews );

	void CreateFolderOutliner();
	void CreateThreadPane();

	virtual void SetSort( int idSort );

	BOOL IsThreadFocus() const { return m_pOutliner && m_pOutliner == GetFocus(); }
	BOOL IsMessageFocus() const;

	virtual void DoUpdateNavigate( CCmdUI* pCmdUI, MSG_MotionType cmd );
	virtual void DoNavigate( MSG_MotionType msgCommand );
	virtual void DoPriority( MSG_PRIORITY priority );

	virtual void GetMessageString( UINT nID, CString& rMessage ) const;

	// Helper
	void DoUndoNavigate( MSG_MotionType msgCommand );

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
#ifndef ON_COMMAND_RANGE
	afx_msg BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	afx_msg BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
#endif
	afx_msg void OnClose();
	afx_msg void OnDestroy();

	// Edit Menu Items
	afx_msg void OnEditUndo();
	afx_msg void OnEditRedo();
	afx_msg void OnSelectThread ();
	afx_msg void OnUpdateSelectThread ( CCmdUI * pCmdUI );
	afx_msg void OnSelectFlagged();
	afx_msg void OnUpdateSelectFlagged( CCmdUI *pCmdUI );
	afx_msg void OnSelectAll();
	afx_msg void OnUpdateSelectAll( CCmdUI *pCmdUI );
	afx_msg void OnEditProperties();
	afx_msg void OnUpdateProperties( CCmdUI *pCmdUI );

	// View Menu Items
	afx_msg void OnViewMessage();
	afx_msg void OnUpdateViewMessage( CCmdUI *pCmdUI );
	afx_msg void OnViewCategories();
	afx_msg void OnUpdateViewCategories( CCmdUI *pCmdUI );
	afx_msg void OnViewFolder();

	// Message Menu Items
	afx_msg void OnMove( UINT nID );
	afx_msg void OnCopy( UINT nID );
	afx_msg void OnUpdateFile( CCmdUI *pCmdUI );
	afx_msg void OnIgnore();
	afx_msg void OnUpdateIgnore(CCmdUI *pCmdUI);

	// Window menu items
	afx_msg void OnFileBookmark( );
	afx_msg void OnUpdateFileBookmark( CCmdUI *pCmdUI );

	// Non-menu Items
	afx_msg void OnSelect();
	afx_msg void OnSelectFolder();
	afx_msg void OnOpen();
	afx_msg void OnOpenNew();
	afx_msg void OnOpenReuse();
	afx_msg void OnUpdateOpen(CCmdUI *pCmdUI);
	afx_msg void OnContinue();
	afx_msg void OnContainer();
	afx_msg void OnOpenNewFrame();

	afx_msg void OnPriorityLowest();
	afx_msg void OnPriorityLow();
	afx_msg void OnPriorityNormal();
	afx_msg void OnPriorityHigh();
	afx_msg void OnPriorityHighest();
	afx_msg void OnUpdatePriority( CCmdUI *pCmdUI );

	afx_msg void OnDoneGettingMail();
	DECLARE_MESSAGE_MAP()

	void SelectMessage( MessageKey key );

public:
	~C3PaneMailFrame();

	CMailNewsOutliner* GetFolderOutliner() { return m_pFolderOutliner; }
	void LoadFolder( MSG_FolderInfo *folderInfo, 
					 MessageKey key = MSG_MESSAGEKEYNONE,
					 int action = actionSelectFirst);
	void UpdateFolderPane(MSG_FolderInfo *pFolderInfo);
	BOOL GetSelectedFolder(MSG_FolderLine* pFolderLine);

	virtual MessageKey GetCurMessage() const;
	virtual MSG_FolderInfo *GetCurFolder() const;

	virtual LPCTSTR GetWindowMenuTitle();
	virtual BOOL FileBookmark();

	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo);
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo);

	static C3PaneMailFrame *FindFrame( MSG_FolderInfo *folderInfo );
	static C3PaneMailFrame *Open( );
	static C3PaneMailFrame *Open( MSG_FolderInfo *folderInfo, 
							   MessageKey key = MSG_MESSAGEKEYNONE,
							   BOOL* pContinue = FALSE);
	static C3PaneMailFrame *OpenInbox( BOOL bGetNew = FALSE );

#ifdef DEBUG_WHITEBOX
	afx_msg void WhiteBox_OnDeleteMessage() { OnDeleteMessage(); }
 	afx_msg void WhiteBox_OnSelect() { OnSelect(); }
 	void WhiteBox_GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, int *focus){ 
		GetSelection(pane,indices,count,focus);
 	}
 	BOOL WhiteBox_DoesMessageExist( MessageKey key );
#endif
};

#endif
