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
#ifndef _ANIMBAR_H
#define _ANIMBAR_H

#include "toolbar2.h"

class CAnimation: public CWnd
{
protected:
	SIZE m_iconSize;
	int m_iFrameCount;

    HBITMAP m_hAnimBitmap;
    int m_iAnimationCount;
	UINT m_uAnimationClock;
	BOOL m_bInited;
	BOOL m_bCaptured, m_bDepressed;

    void AnimateIcon();

public:
	CAnimation( CWnd *pParent );

    void StopAnimation();
	void StartAnimation();

protected:
    afx_msg void OnPaint();
	afx_msg void OnTimer(UINT);
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnDestroy();

    DECLARE_MESSAGE_MAP()
};

#define CAnimationBarParent CToolbarControlBar

class CAnimationBar: public CAnimationBarParent
{
protected:
	POINT m_iconPt;
	CAnimation *m_wndAnimation;

public:
    CAnimationBar();
    ~CAnimationBar();

    void StopAnimation();
	void StartAnimation();
	void PlaceToolbar(void);

protected:
#ifdef XP_WIN32
// Attributes
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode );

#endif

	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnSize( UINT nType, int cx, int cy );
    DECLARE_MESSAGE_MAP()
};

#endif
