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

#ifndef __COMPMISC_H
#define __COMPMISC_H

#include "compstd.h"
#include "mainfrm.h"
#include "edframe.h"

class CComposeSubjectEdit : public CGenericEdit {
public:
   virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
   afx_msg void OnSetFocus(CWnd * pWnd);
   afx_msg void OnKillFocus(CWnd*);

   DECLARE_MESSAGE_MAP();
};

class CBlankWnd :   public CWnd
{
public:
    CBlankWnd () : CWnd ( ) 
    { 
        m_bModified = FALSE; 
    }
    BOOL m_bModified;
    void SetModified(BOOL bValue) { m_bModified = bValue; }
	void SetWrapCol(int32 lCol)   { m_lWrapCol = lCol; }
	int32 GetWrapCol()			  { return m_lWrapCol; }

protected:

	int32 m_lWrapCol;

    afx_msg BOOL OnEraseBkgnd( CDC* pDC );
    afx_msg void OnSize ( UINT nType, int cx, int cy );
    afx_msg void OnChange(void);
    afx_msg void OnSetFocus(CWnd*);
    afx_msg void OnPaint(void);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP();

};

class CComposeFrame;

class CComposeEdit : public CGenericEdit
{
protected:
   CComposeFrame * m_pComposeFrame;
   HFONT m_cfRegFont;
   int m_cxChar;
#ifdef XP_WIN16
   HGLOBAL m_hTextElementSegment;
   BOOL PreCreateWindow( CREATESTRUCT& cs );
#endif

public:
   CComposeEdit(CComposeFrame * pFrame = NULL);
   void SetComposeFrame(CComposeFrame * pFrame)
   {
       m_pComposeFrame = pFrame;
   }
   ~CComposeEdit();

   BOOL FindText();
   BOOL IsClipboardData();
   BOOL IsSelection();
   int GetCharWidth() { return m_cxChar; }
   afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
   afx_msg int OnCreate ( LPCREATESTRUCT );
   afx_msg void OnSetFocus ( CWnd * );
   afx_msg void OnCheckSpelling();

    // edit menu
   afx_msg void OnCut();
   afx_msg void OnCopy();
   afx_msg void OnPaste();
   afx_msg void OnUndo();
   afx_msg void OnRedo();
   afx_msg void OnDelete();
   afx_msg void OnFindInMessage();
   afx_msg void OnFindAgain();
   afx_msg void OnUpdateCut(CCmdUI * pCmdUI);
   afx_msg void OnUpdateCopy(CCmdUI * pCmdUI);
   afx_msg void OnUpdatePaste(CCmdUI * pCmdUI);
   afx_msg void OnUpdatePasteAsQuote(CCmdUI * pCmdUI);
   afx_msg void OnUpdateUndo(CCmdUI * pCmdUI);
   afx_msg void OnUpdateRedo(CCmdUI * pCmdUI);
   afx_msg void OnUpdateDelete(CCmdUI * pCmdUI);
   afx_msg void OnUpdateFindInMessage(CCmdUI * pCmdUI);
   afx_msg void OnUpdateFindAgain(CCmdUI * pCmdUI);
   afx_msg void OnSelectAll();
   afx_msg void OnUpdateSelectAll(CCmdUI *pCmdUI);
   afx_msg LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam);

   DECLARE_MESSAGE_MAP()
};

#endif
