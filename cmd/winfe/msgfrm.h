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
// msgfrm.h : header file
//
#include "widgetry.h"
#include "statbar.h"
#include "apimsg.h"
#include "mailfrm.h"

/////////////////////////////////////////////////////////////////////////////
// CMessageFrame frame

class CMessageFrame : public CMailNewsFrame
{
DECLARE_DYNCREATE(CMessageFrame)
protected:
	CNetscapeStatusBar m_barStatus;
	BOOL m_bNavPending;
	MSG_MotionType m_navPending;

	CMessageFrame();           // protected constructor used by dynamic creation

// Operations
public:
	void SetIsNews( BOOL bNews );
	BOOL GetIsNews() const { return m_bNews; }
	void LoadMessage( MSG_FolderInfo *folderInfo, MessageKey id );

// Implementation
protected:

	void SwitchUI();

// IMailFrame override
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);
// Overrides
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
#ifndef ON_COMMAND_RANGE
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	afx_msg BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
#endif

	virtual ~CMessageFrame();

	virtual void DoUpdateNavigate( CCmdUI* pCmdUI, MSG_MotionType cmd );
	virtual void DoNavigate( MSG_MotionType msgCommand );

	// Generated message map functions
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	LRESULT CMessageFrame::OnFillInToolTip(WPARAM wParam, LPARAM lParam);

	// Edit Menu Items
	afx_msg void OnEditUndo();
	afx_msg void OnEditRedo();

	//Message Menu Items
	afx_msg void OnStop();
	afx_msg void OnUpdateStop( CCmdUI * pCmdUI );
	afx_msg void OnReuse();
	afx_msg void OnUpdateReuse(CCmdUI* pCmdUI);

	// Non Menu Items
	afx_msg void OnContainer();
	afx_msg void OnUpdateContainer( CCmdUI * pCmdUI );
	afx_msg void OnContinue();
	afx_msg void OnMove(UINT nID );
	afx_msg void OnCopy(UINT nID );
	afx_msg void OnUpdateFile( CCmdUI *pCmdUI );

	DECLARE_MESSAGE_MAP()

public:
	virtual MessageKey GetCurMessage() const;
	virtual MSG_FolderInfo *GetCurFolder() const;

	virtual LPCTSTR GetWindowMenuTitle();

	static CMessageFrame *FindFrame( MSG_FolderInfo *folderInfo, MessageKey key );
	static CMessageFrame *Open();
	static CMessageFrame *Open( MSG_FolderInfo *folderInfo, MessageKey key );
};
