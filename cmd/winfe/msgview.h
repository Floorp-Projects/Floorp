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
#ifndef _MSGVIEW_H
#define _MSGVIEW_H

#include "netsvw.h"

class CAttachmentTray: public CWnd {
protected:
	HFONT m_hFont;
	RECT m_rcClient;

	// Content data
	class AttachAux {
	public:
#ifdef _WIN32
		HIMAGELIST hImage;
#else
		HICON hIcon;
#endif
		BOOL bSelected;
		POINT ptPos;
	};

	int m_nAttachmentCount;
	MSG_AttachmentData *m_pAttachmentData;
	AttachAux *m_aAttachAux;

	// Layout stuff
	SIZE m_sizeSpacing, m_sizeIcon;

	// Scrolling stuff
	POINT m_ptOrigin;
	SIZE m_sizeMax;

public:
	CAttachmentTray();
	~CAttachmentTray();

	void SetAttachments(int nAttachmentCount, MSG_AttachmentData *pAttachmentData);
	void SetCSID(int16 doc_csid, XP_Bool updateWindow = TRUE);

protected:
	int Point2Index(POINT pt);
	void Index2Rect(int idx, RECT &rect);

	void ClearSelection();

	void Layout();
	BOOL WaitForDrag();
	HICON DredgeUpIcon(char *pszName);

	afx_msg void OnPaint();
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
	afx_msg void OnRButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	DECLARE_MESSAGE_MAP();
};

class CMessageBodyView: public CNetscapeView {

protected:

	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	DECLARE_MESSAGE_MAP();
    DECLARE_DYNCREATE(CMessageBodyView)
};

class CMessageView: public CView {
protected:
	CMessageBodyView *m_pViewMessage;
	CAttachmentTray *m_pWndAttachments;

	int m_iAttachmentsHeight;
	int m_iOldHeight;
	int m_nAttachmentCount;
	BOOL m_bAttachmentsVisible;
	int m_iSliderHeight;
	RECT m_rcSlider;
	POINT m_ptHit;
	RECT m_rcClient;

public:
	CMessageView();
	~CMessageView();

	void SetAttachments(int nAttachmentCount, MSG_AttachmentData *pAttachmentData);
	void ShowAttachments(BOOL bShow = TRUE);
	BOOL AttachmentsVisible() const;
	void UpdateFocusFrame();
	void SetCSID(int16 doc_csid);
	//Does this view or its children have focus
	BOOL HasFocus();
	CWnd* GetMessageBodyView() { return m_pViewMessage; }

protected:

	virtual BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	virtual BOOL OnCmdMsg( UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo );
	virtual void OnDraw( CDC* pDC );

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP();
    DECLARE_DYNCREATE(CMessageView)
};

#endif
