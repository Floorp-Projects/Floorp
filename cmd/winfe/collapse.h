/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef __COLLAPSE_H
#define __COLLAPSE_H

typedef enum {
   collapse_open = 0,
   collapse_closed
} COLLAPSE_STATE;

class CNSCollapser 
{
protected:
   HBITMAP m_hVertSectionBitmap;
   HBITMAP m_hHTabBitmap;
   BOOL m_bHilite;
   CRect m_rect;
   CWnd * m_pWnd;
   COLLAPSE_STATE m_cState;
   int m_iSubtract;
	UINT m_nTimer;
   UINT m_cmdId;
public:
   CNSCollapser();
   ~CNSCollapser();
   void GetRect(CRect & rect) { rect = m_rect; }
   BOOL ButtonPress(POINT & point);
   void Initialize(CWnd * pWnd = NULL, UINT nCmd = 0);
   void TimerEvent(UINT nIDEvent);
   void DrawCollapseWidget(CDC &dc, COLLAPSE_STATE state = collapse_closed, BOOL bHilite = FALSE, int iSubtract = 0);
   void MouseAround(POINT & point);
};

#endif
