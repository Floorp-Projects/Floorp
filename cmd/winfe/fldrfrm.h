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

#ifndef _FLDRFRM_H
#define _FLDRFRM_H

#ifndef _MAILFRM_H
#include "mailfrm.h"
#endif

/////////////////////////////////////////////////////////////////////////////
// CFolderFrame frame

class CFolderFrame : public CMsgListFrame
{
	DECLARE_DYNCREATE(CFolderFrame)
protected:
	CNetscapeStatusBar m_barStatus;

	CFolderFrame();

	void SelectFolder( MSG_FolderInfo *folderInfo );
	void DoOpenFolder( BOOL bReuse);

	virtual void GetMessageString( UINT nID, CString& rMessage ) const;
	virtual BOOL OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext );
	virtual BOOL FileBookmark();

#ifndef ON_COMMAND_RANGE
	afx_msg BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	afx_msg BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
#endif

	afx_msg void OnSetFocus( CWnd * pOld );
	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnClose( );
	afx_msg void OnDestroy();

	// File Menu Items
	afx_msg void OnOpen();
	afx_msg void OnOpenNew();
	afx_msg void OnOpenReuse();
	afx_msg void OnUpdateOpen( CCmdUI *pCmdUI );
	afx_msg void OnNew();
	afx_msg void OnUpdateNew(CCmdUI *pCmdUI);

	// View Menu Items
	afx_msg void OnUpdateView();
	afx_msg void OnUpdateUpdateView( CCmdUI *pCmdUI );
	afx_msg void OnViewProperties();
	afx_msg void OnUpdateViewProperties( CCmdUI *pCmdUI );

	// Message Menu Items
	afx_msg void OnMove( UINT nID );
	afx_msg void OnCopy( UINT nID );
	afx_msg void OnUpdateFile( CCmdUI *pCmdUI );

	// Window Menu Items
	afx_msg void OnFileBookmark();
	afx_msg void OnUpdateFileBookmark( CCmdUI *pCmdUI );

	// Non menu items
	afx_msg void OnSelect();
	DECLARE_MESSAGE_MAP()

public:
// Support for IMailFrame
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);

	virtual LPCTSTR GetWindowMenuTitle();

	virtual MSG_FolderInfo *GetCurFolder() const;

	static MSG_FolderInfo *FindFolder( LPCSTR lpszName, MSG_FolderInfo *root = NULL );

	static CFolderFrame *Open();
	static CFolderFrame *Open( MSG_FolderInfo *folderInfo );
	static CFolderFrame *OpenNews( );

	static void SetFolderPref( MSG_FolderInfo *, uint32 );
	static void ClearFolderPref( MSG_FolderInfo *, uint32 );
	static BOOL IsFolderPrefSet( MSG_FolderInfo *, uint32 );
};

void _ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure);

	
extern CFolderFrame *g_pFolderFrame;

#endif
