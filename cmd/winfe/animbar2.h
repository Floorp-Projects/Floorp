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

#ifndef _ANIMBAR2_H
#define _ANIMBAR2_H

#include "apicx.h"

class CAnimation2: public CWnd
{
protected:
	LPMWCONTEXT m_pIMWContext;

	SIZE m_iconSize;
	int m_iFrameCount;

    HBITMAP m_hAnimBitmap;
    int m_iAnimationCount;
	UINT m_uAnimationClock;
	BOOL m_bInited;
	BOOL m_bCaptured, m_bDepressed;

    static HBITMAP m_hSmall;
    static HBITMAP m_hBig;

    static ULONG m_uRefCount;
    CNSToolTip2 m_ToolTip;
    BOOL m_bHaveFocus;
    UINT m_hFocusTimer;

protected:
    void AnimateIcon();

public:
	CAnimation2( CWnd *pParent, LPUNKNOWN pUnk = NULL);
    ~CAnimation2();
    
    void StopAnimation();
	void StartAnimation();

	void Initialize( LPUNKNOWN pUnk);
    
    void SetBitmap( BOOL bSmall = TRUE );
    HBITMAP GetBitmap( BOOL bSmall = TRUE );    

    void GetSize( CSize &size );
    void RemoveButtonFocus();

protected:
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
    afx_msg void OnPaint();
	afx_msg void OnTimer(UINT);
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnDestroy();
    afx_msg BOOL OnSetCursor( CWnd *, UINT, UINT );
    
    DECLARE_MESSAGE_MAP()
};

#define CAnimationBar2Parent CWnd

class CAnimationBar2: public CAnimationBar2Parent
{
protected:
	LPUNKNOWN m_pUnk;
	POINT m_iconPt;
	CAnimation2 *m_wndAnimation;

public:
    CAnimationBar2(LPUNKNOWN pUnk = NULL);
    ~CAnimationBar2();

    void StopAnimation();
	void StartAnimation();

	void Initialize( LPUNKNOWN pUnk);

    void SetBitmap( BOOL bSmall = TRUE ) { m_wndAnimation->SetBitmap( bSmall ); }
    
    void GetSize( CSize &size, BOOL bSmall = TRUE );
    
protected:
#ifdef XP_WIN32
// Attributes
//	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
#endif

	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
    DECLARE_MESSAGE_MAP()
};


#endif
