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

#include "stdafx.h"

#include "cxwin.h"
#include "cntritem.h"
#include "medit.h"
#include "button.h"
#include "fmbutton.h"
#include "gridedge.h"
#include "cxsave.h"
#include "netsvw.h"
#include "feembed.h"
#include "libevent.h"
#include "np.h"
#include "winclose.h"
#ifdef EDITOR
#include "edview.h"
#endif /* EDITOR */

void CWinCX::DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool iClear)  
{
#ifdef EDITOR
    CNetscapeEditView * pView;
#endif // EDITOR
    BOOL bHideCaret = FALSE;

    //  If we're erasing a selection, do so now.
    if(iClear)  {
        LTRB Rect;
        if(ResolveElement(Rect, pText->x, pText->y, pText->x_offset, pText->y_offset,
							pText->width, pText->height))  {
			SafeSixteen(Rect);

            RECT crRect;
			::SetRect(&crRect,CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
            HDC pDC = GetContextDC();
			EraseTextBkgnd(pDC, crRect, pText);
            ReleaseContextDC(pDC);
        }
    } else {
        // Hide the editor's caret during drawing -- needed to avoid
        //   erasing character pixels by the caret
#ifdef EDITOR
        pView = (CNetscapeEditView *)GetView();
        if( pView && pView->IsEditor() && pView->m_caret.bEnabled && pView->m_caret.cShown ) {
            pView->HideCaret();
            pView->m_caret.cShown = 0;
            bHideCaret = TRUE;
        }
#endif // EDITOR
    }

    //  Call the base.
//#ifndef NO_TAB_NAVIGATION 
	uint32 flag = 0;
	setTextTabFocusDrawFlag( pText, &flag );
	CDCCX::DisplayText(pContext, iLocation, pText, iClear, flag);
//#endif /* NO_TAB_NAVIGATION */
 
#ifdef EDITOR
    if( bHideCaret ) {
        pView->ShowCaret();
        pView->m_caret.cShown = 1;
    }
#endif // EDITOR
}

void CWinCX::DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool iClear)   {
#ifdef EDITOR
    CNetscapeEditView * pView;
#endif // EDITOR
    BOOL bHideCaret = FALSE;

    //  If we're erasing a selection, do so now.
    if(iClear)  {
        LTRB Rect;
        if(ResolveElement(Rect, pText, iLocation, lStartPos, lEndPos, iClear))  {
			SafeSixteen(Rect);

            RECT crRect;
			::SetRect(&crRect, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
            HDC pDC = GetContextDC();
			EraseTextBkgnd(pDC, crRect, pText);
            ReleaseContextDC(pDC);
        }
    } else {
        // Hide the editor's caret during drawing
#ifdef EDITOR
        pView = (CNetscapeEditView *)GetView();
        if( pView && pView->IsEditor() && pView->m_caret.bEnabled && pView->m_caret.cShown ) {
            pView->HideCaret();
            pView->m_caret.cShown = 0;
            bHideCaret = TRUE;
        }
#endif // EDITOR
    }

    //  Call the base.
    CDCCX::DisplaySubtext(pContext, iLocation, pText, lStartPos, lEndPos, iClear);
#ifdef EDITOR
    if( bHideCaret ) {
        pView->ShowCaret();
        pView->m_caret.cShown = 1;
    }
#endif // EDITOR
}

