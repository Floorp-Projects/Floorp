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

#ifndef _MAILQF_H
#define _MAILQF_H

#include "dropmenu.h"
#include "msgcom.h"

// in dropmenu.h
class CDropMenu;

#define QF_DRAGGINGBOOKMARK WM_USER + 6

/////////////////////////////////////////////////////////////////////
// CMailQFDropTarget

#define CMailQFDropTargetBase	COleDropTarget

class CMailQFDropTarget : public CMailQFDropTargetBase
{
	public:
		CMailQFDropTarget(){}
		 
	protected:
		virtual DROPEFFECT OnDragEnter(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT OnDragOver(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point );
		virtual BOOL OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point);
}; // END OF CLASS CMailQFDropTarget()

/////////////////////////////////////////////////////////////////////
// CMailQFDropMenu

#define CMailQFDropMenuParent CDropMenu

class CMailQFDropMenu: public CMailQFDropMenuParent
{
protected:
	CDropMenuDropTarget *GetDropMenuDropTarget(CWnd *pOwner);

	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP();
};

/////////////////////////////////////////////////////////////////////
// CMailQFButton

#define CMailQFButtonParent	CDropMenuButton

class CMailQFButton : public CMailQFButtonParent
{
protected:
	CMailQFDropTarget m_DropTarget;
	CMailQFDropMenu *m_pDropMenu;
	HBITMAP m_hFolderBitmap;
	HBITMAP m_hSelectedFolderBitmap;
public:
	CMailQFButton();
	~CMailQFButton();

	BOOL Create(const CRect & rect, CWnd* pwndParent, UINT uID);
	virtual	 void OnAction();
protected:

	void BuildMenu();
	void BuildMenu( MSG_FolderInfo *, CDropMenu *pMenu, UINT &nID );
		
	afx_msg void OnBtnClicked(BOOL bDragging);
	afx_msg LRESULT OnQuickfileDrag(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDrop(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMoveOrCopy( UINT nID );
	afx_msg void OnSysColorChange();
	afx_msg void OnPaletteChanged( CWnd* pFocusWnd );
#ifndef ON_COMMAND_RANGE
	afx_msg BOOL OnCommand( WPARAM wParam, LPARAM lParam );
#endif
	DECLARE_MESSAGE_MAP();
}; // END OF CLASS CMailQFButton

#endif
