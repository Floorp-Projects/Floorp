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

/* COMPMISC.CPP - Contains controls which are utilized in the compose
 * window address/attachment block.  Also contains the resizer control
 * which resizes the text edit control
 */
 
#include "stdafx.h"
#include "compmisc.h"
#include "compfrm.h"
#include "compbar.h"
#include "findrepl.h"
#include "spellcli.h"       // spell checker client
#include "prefapi.h"

#ifdef XP_WIN32
#define EDIT_CONTROL_BUFFER_SIZE    UINT_MAX
#else
#define EDIT_CONTROL_BUFFER_SIZE    ((UINT)(32*1024))
#endif

/////////////////////////////////////////////////////////////////////////////
// CBlankWnd - resizes the edit control and restores focus to the appropriate
//             field.

BEGIN_MESSAGE_MAP(CBlankWnd,CWnd)
    ON_EN_CHANGE(IDC_COMPOSEEDITOR,OnChange)
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
    ON_WM_PAINT()
    ON_WM_CREATE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

#define INDICATOR_WIDTH 5
#define INDICATOR_COLOR RGB(113,113,255)

PR_CALLBACK PrefSetWrapCol(const char *pPref, void *pData)
{
    int32 iCol;
	PREF_GetIntPref("mailnews.wraplength", &iCol);

	CBlankWnd * pEditorWindow = (CBlankWnd *)pData;
	pEditorWindow->SetWrapCol(iCol);
	pEditorWindow->Invalidate(TRUE);// must pass TRUE
}

int CBlankWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_lWrapCol = 0L;

	//register call back for wrap long line
	PREF_RegisterCallback("mailnews.wraplength", PrefSetWrapCol, (void *)this);
    PREF_GetIntPref("mailnews.wraplength",&m_lWrapCol);
	return 0;
}

void CBlankWnd::OnDestroy()
{
	PREF_UnregisterCallback("mailnews.wraplength", PrefSetWrapCol, this);
	CWnd::OnDestroy();
}

void CBlankWnd::OnPaint()
{
   CPaintDC dc(this);
   CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
   if (pFrame->GetWrapLongLines())
   {
       CRect rect;
       GetClientRect(rect);
       int iPos = ((int)GetWrapCol()) * pFrame->GetCharWidth();
       if (iPos < rect.Width())
       {
           CPen pen(PS_SOLID,1,INDICATOR_COLOR);
           CPen * pOldPen = dc.SelectObject(&pen);
           iPos -= (INDICATOR_WIDTH/2);
           dc.MoveTo(iPos,1);
           dc.LineTo(iPos+INDICATOR_WIDTH,1);
           dc.MoveTo(iPos+1,2);
           dc.LineTo((iPos+INDICATOR_WIDTH)-1,2);
           dc.SetPixel(iPos+2,3,INDICATOR_COLOR);
           dc.SelectObject(pOldPen);
       }
   }
}

void CBlankWnd::OnSetFocus(CWnd *)
{
    // when focus is given to this window... focus is passed off to the
    // last field which had focus.
    CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
    if (pFrame->GetFocusField() && IsWindow(pFrame->GetFocusField()->m_hWnd))
        pFrame->GetFocusField()->SetFocus();
}

void CBlankWnd::OnChange(void)
{   
    // the backend should be notified that the text has been 
    // changed  whener an edit occurs
    CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
    m_bModified = TRUE;
}

BOOL CBlankWnd::OnEraseBkgnd (CDC * pdc)
{
    // erase the background with the default brush
    CRect rect;
    CBrush Brush(GetSysColor(COLOR_WINDOW));
    GetClientRect (&rect);
    pdc->FillRect (rect, &Brush);
    return TRUE;
}

void CBlankWnd::OnSize ( UINT nType, int cx, int cy )
{
    // resize the editor whenver this window is resized.
    // a border is given on the left so that the cursor doesn't
    // but right up against the edge (looks bad)
    CWnd::OnSize ( nType, cx, cy );    
    CRect rect ( EDIT_MARGIN_OFFSET, EDIT_TOP_MARGIN, cx, cy ); 
    CComposeFrame * pFrame = (CComposeFrame*)GetParent()->GetParent();
    if (IsWindow(pFrame->GetEditor()->m_hWnd))
        pFrame->GetEditor()->MoveWindow ( rect );
}

/////////////////////////////////////////////////////////////////////////////
// CComposeEdit - This is the regular text edit control. This is probably
//

BEGIN_MESSAGE_MAP(CComposeEdit, CGenericEdit)
   ON_WM_KEYDOWN()
   ON_WM_CREATE()
   ON_WM_SETFOCUS()
   ON_COMMAND(ID_CHECK_SPELLING, OnCheckSpelling)
   // edit menu
   ON_COMMAND(ID_EDIT_CUT,OnCut)
   ON_COMMAND(ID_EDIT_COPY,OnCopy)
   ON_COMMAND(ID_EDIT_PASTE,OnPaste)
   ON_COMMAND(ID_EDIT_UNDO,OnUndo)
   ON_COMMAND(idm_redo,OnRedo)
   ON_COMMAND(ID_EDIT_DELETE,OnDelete)
   ON_COMMAND(IDM_SELECTALL,OnSelectAll)
   ON_COMMAND(IDM_FINDAGAIN,OnFindAgain)
   ON_COMMAND(IDM_FINDINMESSAGE,OnFindInMessage)
   ON_REGISTERED_MESSAGE(WM_FINDREPLACE, OnFindReplace)
   ON_UPDATE_COMMAND_UI(ID_EDIT_CUT,OnUpdateCut)
   ON_UPDATE_COMMAND_UI(ID_EDIT_COPY,OnUpdateCopy)
   ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE,OnUpdatePaste)
   ON_UPDATE_COMMAND_UI(IDM_PASTEASQUOTE,OnUpdatePasteAsQuote)
   ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO,OnUpdateUndo)
   ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE,OnUpdateDelete)
   ON_UPDATE_COMMAND_UI(IDM_SELECTALL,OnUpdateSelectAll)
   ON_UPDATE_COMMAND_UI(IDM_FINDINMESSAGE,OnUpdateFindInMessage)
   ON_UPDATE_COMMAND_UI(IDM_FINDAGAIN,OnUpdateFindAgain)
END_MESSAGE_MAP()

CComposeEdit::CComposeEdit(CComposeFrame * pFrame)
{
    m_pComposeFrame = pFrame;
    m_cxChar = 1;
#ifdef XP_WIN16
    // win16 edit control's memory is by default, allocated from the heap.. which is
    // usually not enough.... so create a 32K message buffer
    m_hTextElementSegment = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, EDIT_CONTROL_BUFFER_SIZE); 
#endif
}

CComposeEdit::~CComposeEdit()
{
#ifdef XP_WIN16
    // free up the 16bit memory buffer
    if (m_hTextElementSegment)
        GlobalFree(m_hTextElementSegment);
#endif
}

#ifdef XP_WIN16
//
// The creator of this form element should have created a segment for
// us to live in before we got here.  Tell Windows to use that 
// segment rather than the application's so that we don't run out of
// DGROUP space
//
BOOL CComposeEdit::PreCreateWindow( CREATESTRUCT& cs )
{
    // during a precreate in happy 16bit land, lock the edit data
    // buffer and tell the control to use it.
    if (CGenericEdit::PreCreateWindow(cs)) {

        ASSERT(m_hTextElementSegment);

        if(!m_hTextElementSegment)
            return TRUE;

        LPVOID lpEditDC = (LPVOID) GlobalLock(m_hTextElementSegment);

        if (lpEditDC) {
            UINT uSegment = HIWORD((LONG) lpEditDC);
            if (LocalInit((UINT)uSegment,0,(WORD)(GlobalSize(m_hTextElementSegment)-16))) {
                cs.hInstance = (HINSTANCE) uSegment;
                UnlockSegment(uSegment);
            }
            else 
                GlobalUnlock(m_hTextElementSegment);

        }
        return TRUE;
    }

    return FALSE;   
}
#endif  

void CComposeEdit::OnSetFocus(CWnd * pWnd)
{
    // When focus is set to the edit field, tell the parent
    CGenericEdit::OnSetFocus(pWnd);
    CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
    pFrame->SetFocusField(this);
}

int CComposeEdit::OnCreate ( LPCREATESTRUCT lpcs )
{
    // set up the fonts, get the character size
    int iRet = CGenericEdit::OnCreate ( lpcs );

    LimitText(EDIT_CONTROL_BUFFER_SIZE);
    
    CClientDC dc ( this );
    LOGFONT lf;
    XP_MEMSET(&lf,0,sizeof(LOGFONT));
    CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
    lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
        strcpy(lf.lfFaceName, IntlGetUIFixFaceName(pFrame->m_iCSID));
	lf.lfCharSet = IntlGetLfCharset(pFrame->m_iCSID); 
    lf.lfHeight = -MulDiv(9,dc.GetDeviceCaps(LOGPIXELSY), 72);
    lf.lfQuality = PROOF_QUALITY;
    
	if (m_cfRegFont) {
		theApp.ReleaseAppFont(m_cfRegFont);
	}
	m_cfRegFont = theApp.CreateAppFont( lf );

    TEXTMETRIC tm;
    CFont * pOldFont = dc.SelectObject(CFont::FromHandle(m_cfRegFont));
    dc.GetTextMetrics(&tm);
    dc.SelectObject(pOldFont);
    m_cxChar = tm.tmAveCharWidth;
	::SendMessage(GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfRegFont, FALSE);

    return iRet;
}

void CComposeEdit::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
    // process the tab key so that it properly tabs between the
    // address block and the editor.
    if ((nChar==VK_TAB)&&(GetKeyState(VK_SHIFT)&0x8000)) {
        CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
        MSG msg;
        while (PeekMessage(&msg,m_hWnd,WM_KEYFIRST,WM_KEYLAST,PM_REMOVE))
            ;
        CComposeBar * pBar = pFrame->GetComposeBar();
        ASSERT(pBar);        
        if (pBar)
            pBar->m_pSubjectEdit->SetFocus();
    }
    else
    {
        CGenericEdit::OnKeyDown(nChar,nRepCnt,nFlags);
    }
}

void CComposeEdit::OnCheckSpelling()
{
    CPlainTextSpellChecker SpellChecker(this);

    if (SpellChecker.ProcessDocument() != 0)
        ASSERT(FALSE);
}

void CComposeEdit::OnCut()
{
    Cut();
}

void CComposeEdit::OnCopy()
{
    Copy();
}

void CComposeEdit::OnPaste()
{
   Paste();
}

void CComposeEdit::OnUndo()
{
    Undo();
}

void CComposeEdit::OnRedo()
{
}

void CComposeEdit::OnDelete()
{
	if (GetFocus() == this)
	    Clear();
}

void CComposeEdit::OnFindInMessage()
{
    CNetscapeFindReplaceDialog *dlg;
    dlg = new CNetscapeFindReplaceDialog();
    dlg->Create(TRUE, 
        theApp.m_csFindString, 
        NULL, 
        FR_DOWN | FR_NOWHOLEWORD | FR_HIDEWHOLEWORD | FR_NOUPDOWN, 
        GetParentFrame());
}

void CComposeEdit::OnFindAgain()
{
   FindText();
}

void CComposeEdit::OnUpdateCut(CCmdUI * pCmdUI)
{
   if (GetFocus() == this)
      pCmdUI->Enable(IsSelection());
}

void CComposeEdit::OnUpdateCopy(CCmdUI * pCmdUI)
{
   if (GetFocus() == this)
       pCmdUI->Enable(IsSelection());
}

void CComposeEdit::OnUpdatePaste(CCmdUI * pCmdUI)
{
   if (GetFocus() == this)
      pCmdUI->Enable(IsClipboardData());
}

BOOL CComposeEdit::IsSelection()
{
    int nStart, nEnd;
    GetSel(nStart,nEnd);
    return ((nEnd-nStart)>0);
}

BOOL CComposeEdit::IsClipboardData()
{
    BOOL retVal = FALSE;
    OpenClipboard();
			
    if (GetClipboardData(CF_TEXT)) {
        retVal = TRUE;
    }
    CloseClipboard();
    return retVal;
}

void CComposeEdit::OnUpdatePasteAsQuote(CCmdUI * pCmdUI)
{
    if (GetFocus() == this)
       pCmdUI->Enable(IsClipboardData());
}

void CComposeEdit::OnUpdateUndo(CCmdUI * pCmdUI)
{
    if (GetFocus() == this)
       pCmdUI->Enable(CanUndo());
}

void CComposeEdit::OnUpdateDelete(CCmdUI * pCmdUI)
{
	if (GetFocus() == this)
	    pCmdUI->Enable(IsSelection());
}

void CComposeEdit::OnUpdateFindInMessage(CCmdUI * pCmdUI)
{
   if ( GetWindowTextLength() > 0 && (GetFocus() == this))
      pCmdUI->Enable(TRUE);
   else
      pCmdUI->Enable(FALSE);
}

void CComposeEdit::OnUpdateFindAgain(CCmdUI * pCmdUI)
{
   if ((theApp.m_csFindString && strlen(theApp.m_csFindString)) && (GetFocus() == this))
      pCmdUI->Enable(TRUE);
   else
      pCmdUI->Enable(FALSE);
}

void CComposeEdit::OnSelectAll()
{
    SetSel(0,-1);
    SetFocus();
}

void CComposeEdit::OnUpdateSelectAll(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(TRUE);
}

BEGIN_MESSAGE_MAP(CComposeSubjectEdit, CGenericEdit)
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

void CComposeSubjectEdit::OnSetFocus(CWnd * pWnd)
{
    // When focus is set to the edit field, tell the parent
    CGenericEdit::OnSetFocus(pWnd);
    CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
    pFrame->SetFocusField(this);
}

void CComposeSubjectEdit::OnKillFocus(CWnd * pWnd)
{
   CGenericEdit::OnKillFocus(pWnd);
   CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
   CString cs;
   GetWindowText(cs);
   if (cs.GetLength())
       pFrame->GetChrome()->SetDocumentTitle(cs);
}

BOOL CComposeSubjectEdit::PreTranslateMessage(MSG* pMsg)
{
   if (pMsg->message == WM_KEYDOWN)
   {
      if (pMsg->wParam == VK_RETURN)
		{
         CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
         pFrame->GetComposeBar()->TabControl(FALSE,FALSE,this);
         return TRUE;
      }

      if (pMsg->wParam == 'v' || pMsg->wParam == 'V')
      {
         if (GetKeyState(VK_CONTROL)&0x8000)
         {
            Paste();
            return TRUE;
         }
      } 
      else if (pMsg->wParam == VK_INSERT)
      {
         if (GetKeyState(VK_SHIFT)&0x8000)
         {
            Paste();
            return TRUE;
         }
         else if (GetKeyState(VK_CONTROL)&0x8000)
         {
            Copy();
            return TRUE;
         }
      }
      else if (pMsg->wParam == 'x' || pMsg->wParam == 'X')
      {
         if (GetKeyState(VK_CONTROL)&0x8000)
         {
            Cut();
            return TRUE;
         }
      } 
      else if (pMsg->wParam == 'c' || pMsg->wParam == 'C')
      {
         if (GetKeyState(VK_CONTROL)&0x8000)
         {
            Copy();
            return TRUE;
         }
      } 
   }
   return CEdit::PreTranslateMessage(pMsg);
}

LRESULT CComposeEdit::OnFindReplace(WPARAM wParam, LPARAM lParam) 
{
	CFindReplaceDialog * dlg = ::CFindReplaceDialog::GetNotifier(lParam);
	if (!dlg) 
		return NULL;
	  
	FINDREPLACE findstruct = dlg->m_fr;
	    
	if (dlg->IsTerminating()) {
		return NULL;
	}     

	// Something wrong or user cancelled dialog box
	if(!(findstruct.Flags & FR_FINDNEXT))
		return NULL;
	      
	// remember this string for next time
	theApp.m_csFindString = findstruct.lpstrFindWhat;
	theApp.m_csReplaceString = findstruct.lpstrReplaceWith;
	theApp.m_bMatchCase   = dlg->MatchCase();
	theApp.m_bSearchDown  = dlg->SearchDown(); 

   return (LRESULT) FindText();
}

BOOL CComposeEdit::FindText()
{
   CString cs, csSearch = theApp.m_csFindString;
   GetWindowText(cs);

   int nStart, nEnd;
   GetSel(nStart,nEnd);

   if (!theApp.m_bMatchCase)
   {
      cs.MakeLower();
      csSearch.MakeLower();
   }
   if (!cs.IsEmpty())
   {
      int ipos = nEnd;
      int ipos2;
      LPCSTR lpcstr = cs;
      if ((ipos2 = CString(&lpcstr[ipos]).Find(csSearch)) != -1)
      {
         SetSel(ipos + ipos2, ipos + ipos2 + strlen(csSearch));
      	return(TRUE);
      }
   }
   return FALSE;
}

