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

// edlayout.cpp : implementation of Editor-only FE layout functions
//
// Created 9/16/96 by CLM
//
#include "stdafx.h"

#include "dialog.h"
#include "mainfrm.h"
#include "netsvw.h"
#include "edview.h"

// Utility function to check if it is OK to show the caret. It is used in the FE functions that
// are called by the back end to show caret.
static BOOL IsOkToShowCaret(CNetscapeEditView *pView, char *FE_FuncName)
{
    // Prevent setting caret when we have a modal (e.g. property) dialog over us
    CFrameWnd *pFrame = pView->GetFrame()->GetFrameWnd();
    if(pFrame && pFrame->GetLastActivePopup() != pFrame){
        return FALSE;
    }

    // This will allow us to use the regular caret for drop-point feedback
    //   even if window does not have focus
    // Don't display caret if the editor view does not have focus.
    //  unless we are dragging over our view
    if( !pView->m_bDragOver && pView->GetFocus() != pView ){
        return FALSE;
    }
    return TRUE;    // OK to show caret
}

// Caret used during Drag&Drop - more visible than regular cursor
extern CBitmap ed_DragCaretBitmap;

#define ED_DRAGCARET_HEIGHT 32

static void AdjustForDragCaret(CCaret * pCaret)
{            
    // NOTE: This assumes IDB_ED_DRAG bitmap is 3 by ED_DRAGCARET_HEIGHT
    //        and regular caret is 2 pixels wide
    
    // Back off one on X 
    pCaret->x--;
    
    // Only one size for drop caret, so reposition vertically
    pCaret->y -=  ED_DRAGCARET_HEIGHT - pCaret->height;
    
    pCaret->width = 3;
    pCaret->height = ED_DRAGCARET_HEIGHT;
}

//
// The text has been drawn.  This code will position the text caret in the
//  proper position and height.
//
PUBLIC void
FE_DisplayTextCaret(MWContext * context, int loc, LO_TextStruct * text_data, 
    int char_offset)
{
    if(!context || !EDT_IS_EDITOR(context) || !text_data || !text_data->text_attr)
        return;

    int32       xVal, yVal;
    
    CWinCX *pWinCX = VOID2CX(context->fe.cx, CWinCX);
    CNetscapeEditView * pView = (CNetscapeEditView *)pWinCX->GetView();

    HDC hDC = pView->GetContextDC();

    int32       xOrigin = WINCX(context)->GetOriginX();

    // Figure out X and Y coords of the text in the View's coordinate system
    xVal = text_data->x + text_data->x_offset - xOrigin;
    yVal = text_data->y + text_data->y_offset - WINCX(context)->GetOriginY();

    CPoint cDocPoint(CASTINT(text_data->x), CASTINT(text_data->y));
    if( pWinCX->PtInSelectedRegion(cDocPoint) && pView->m_caret.bEnabled ){
        DestroyCaret();
        pView->m_caret.cShown = 0;
        pView->m_caret.bEnabled = FALSE;
    }

    // Only draw text that falls within the curently viewed part of the frame
    if((yVal < pWinCX->GetHeight()) && (yVal < 32000)) {

		//	Get the font so we can get its height
		CyaFont	*pMyFont;
		pWinCX->SelectNetscapeFontWithCache( hDC, text_data->text_attr, pMyFont );

        // Calculate offset within text 
        CSize cSize(0,0);
        const char *pText = (const char*)text_data->text;
        // Win16 barfs on this if null string
        if( char_offset > 0 && pText && *pText != '\0' ){
            pWinCX->ResolveTextExtent(hDC, pText, char_offset, &cSize, pMyFont );
        }

		pWinCX->ReleaseNetscapeFontWithCache( hDC, pMyFont );
        //
        // The caret has changed position and size.  Buffer the size and position
        //  information        
        //

        // Place caret half way between characters,
        pView->m_caret.x = CASTINT(xVal + cSize.cx);
        pView->m_caret.y = CASTINT(yVal);

        pView->m_caret.x = CASTINT(xVal + cSize.cx);
        pView->m_caret.width = 2;
        pView->m_caret.height = CASTINT(text_data->height);

        if(pView->m_bDragOver)
            AdjustForDragCaret(&(pView->m_caret));

        // Don't show the caret if it is not ok to do so.
        if (!IsOkToShowCaret(pView, "FE_DisplayTextCaret"))
            return;

        if(pView->m_bDragOver)
            pView->CreateCaret(&ed_DragCaretBitmap);
        else
            pView->CreateSolidCaret(pView->m_caret.width, pView->m_caret.height);

        pView->SetCaretPos(CPoint(pView->m_caret.x, pView->m_caret.y));
        pView->m_caret.cShown = 1;
        pView->m_caret.bEnabled = TRUE;
        //TRACE0( "FE_DisplayTextCaret: Caret created and enabled\n");
        pView->ShowCaret();
        pView->SetEditChanged();

    } // end of if-then for when text was visible
}

//
// The text has been drawn.  This code will position the text carret in the
//  proper position and height.
//
PUBLIC void
FE_DisplayImageCaret(MWContext * context, LO_ImageStruct *pLoImage, 
        ED_CaretObjectPosition pos) 
{
    int32   xVal, yVal;

    if(!context || !pLoImage )
        return;

    CWinCX *pWinCX = VOID2CX(context->fe.cx, CWinCX);
    CNetscapeEditView * pView = (CNetscapeEditView *)pWinCX->GetView();
    xVal = pLoImage->x + pLoImage->x_offset - WINCX(context)->GetOriginX();
    yVal = pLoImage->y - WINCX(context)->GetOriginY();

    CPoint cDocPoint(CASTINT(pLoImage->x), CASTINT(pLoImage->y));
    if( pWinCX->PtInSelectedRegion(cDocPoint) && pView->m_caret.bEnabled ){
        DestroyCaret();
        pView->m_caret.cShown = 0;
        pView->m_caret.bEnabled = FALSE;
    }

    // Only draw text that falls within the curently viewed part of the frame
    if((yVal < pWinCX->GetHeight()) && (yVal < 32000)) {

        pView->m_caret.width = 2;

        // Constrain caret size to between 10 and 40 else it looks too weird
        if( pLoImage->line_height > 40 ){
            pView->m_caret.height = 40;
            yVal += (pLoImage->line_height - 45);
        } else if( pLoImage->line_height < 10 ){
            pView->m_caret.height = 10;
            yVal -= (10  - pLoImage->line_height);
        } else {
            pView->m_caret.height = CASTINT(pLoImage->line_height);
        }
        pView->m_caret.y = CASTINT(yVal);
        pView->m_caret.x = CASTINT(xVal);

        if( pos == ED_CARET_BEFORE ){
            pView->m_caret.x -= 1;
        }
        else if( pos == ED_CARET_AFTER ){
            pView->m_caret.x += CASTINT(pLoImage->width + 2 * pLoImage->border_width);
        }
        else {
            pView->m_caret.width = CASTINT(pLoImage->width+ 2 * pLoImage->border_width);
        }

        if(pView->m_bDragOver)
            AdjustForDragCaret(&(pView->m_caret));

        // Don't show the caret if it is not ok to do so.
        if (!IsOkToShowCaret(pView, "FE_DisplayImageCaret"))
            return;

        if(pView->m_bDragOver)
            pView->CreateCaret(&ed_DragCaretBitmap);
        else
            pView->CreateSolidCaret(pView->m_caret.width, pView->m_caret.height);

        pView->SetCaretPos(CPoint(pView->m_caret.x, pView->m_caret.y));
        pView->m_caret.cShown = 1;
        pView->m_caret.bEnabled = TRUE;
        pView->ShowCaret();
        ((CNetscapeEditView*)pView)->SetEditChanged();

    }
}

PUBLIC void
FE_DisplayGenericCaret(MWContext * context, LO_Any *pLoAny, 
        ED_CaretObjectPosition pos) 
{
    int32       xVal, yVal;

    if(!context || !pLoAny )
        return;

    CWinCX *pWinCX = VOID2CX(context->fe.cx, CWinCX);
    CNetscapeEditView * pView = (CNetscapeEditView *)pWinCX->GetView();

    // Figure out X and Y coords of the text
    xVal = pLoAny->x + pLoAny->x_offset - WINCX(context)->GetOriginX();
    yVal = pLoAny->y - WINCX(context)->GetOriginY();

    CPoint cDocPoint(CASTINT(pLoAny->x), CASTINT(pLoAny->y));
    if( pWinCX->PtInSelectedRegion(cDocPoint) && pView->m_caret.bEnabled ){
        DestroyCaret();
        pView->m_caret.cShown = 0;
        pView->m_caret.bEnabled = FALSE;
    }

    // Only draw text that falls within the curently viewed part of the frame
    if((yVal < pWinCX->GetHeight()) && (yVal < 32000)) {

        pView->m_caret.width = 2;

        // Constrain caret size to between 10 and 40 else it looks too weird
        if( pLoAny->line_height > 40 ){
            pView->m_caret.height = 40;
            yVal += (pLoAny->line_height - 45);
        } else if( pLoAny->line_height < 10 ){
            pView->m_caret.height = 10;
            yVal -= (10 - pLoAny->line_height);
        } else {
            pView->m_caret.height = CASTINT(pLoAny->line_height);
        }
        pView->m_caret.y = CASTINT(yVal);
        pView->m_caret.x = CASTINT(xVal);

        if( pos == ED_CARET_BEFORE ){
            pView->m_caret.x -= 1;
        }
        else if( pos == ED_CARET_AFTER ){
            pView->m_caret.x += CASTINT(pLoAny->width);
        }
        else {
            pView->m_caret.width = CASTINT(pLoAny->width);
        }

        if(pView->m_bDragOver)
            AdjustForDragCaret(&(pView->m_caret));

        // Don't show the caret if it is not ok to do so.
        if (!IsOkToShowCaret(pView, "FE_DisplayGenericCaret"))
            return;

        if(pView->m_bDragOver)
            pView->CreateCaret(&ed_DragCaretBitmap);
        else
            pView->CreateSolidCaret(pView->m_caret.width, pView->m_caret.height);

        pView->SetCaretPos(CPoint(pView->m_caret.x, pView->m_caret.y));
        pView->m_caret.cShown = 1;
        pView->m_caret.bEnabled = TRUE;
        pView->ShowCaret();
        ((CNetscapeEditView*)pView)->SetEditChanged();

    }
}

// Note: The returned coordinates do NOT account for the current window
// scroll. So you can't use them to set the windows caret until after
// you subtract the current window WINCX(context)->GetOriginXXX

PUBLIC Bool
FE_GetCaretPosition(MWContext *context, LO_Position* where,
    int32* caretX, int32* caretYLow, int32* caretYHigh)
{
    if(!context || !EDT_IS_EDITOR(context) || !where->element )
        return FALSE;

    HDC hDC;

    CWinCX *pWinCX = VOID2CX(context->fe.cx, CWinCX);
    CNetscapeEditView * pView = (CNetscapeEditView *)pWinCX->GetView();
    int32 xVal = where->element->lo_any.x + where->element->lo_any.x_offset;
    int32 yVal = where->element->lo_any.y;
    int32 yValHigh = yVal + where->element->lo_any.height;

    switch ( where->element->type )
    {
    case LO_TEXT:
        {
            LO_TextStruct* text_data = & where->element->lo_text;
            if ( ! text_data->text_attr ) return FALSE;

            hDC = pView->GetContextDC();

			CyaFont	*pMyFont;
			pWinCX->SelectNetscapeFontWithCache( hDC, text_data->text_attr, pMyFont );
            SIZE cSize;
            pWinCX->ResolveTextExtent(hDC, (const char*)text_data->text, 
                    CASTINT(where->position), &cSize, pMyFont );
            xVal += cSize.cx - 1;
            // ??? Do we need to restore the old font ???
			pWinCX->ReleaseNetscapeFontWithCache( hDC, pMyFont );

        }
        break;
    case LO_IMAGE:
        {
            LO_ImageStruct *pLoImage = & where->element->lo_image;
            if( where->position == 0 ){
                xVal -= 1;
            }
            else {
                xVal += pLoImage->width + 2 * pLoImage->border_width;
            }
        }
        break;
    default:
        {
            LO_Any *any = & where->element->lo_any;
            if( where->position == 0 ){
                xVal -= 1;
            }
            else {
                xVal += any->width;
            }
        }
    }
    *caretX = xVal;
    *caretYLow = yVal;
    *caretYHigh = yValHigh;
   return TRUE;
}

PUBLIC void
FE_DestroyCaret(MWContext * context)
{
    CNetscapeEditView * pView = (CNetscapeEditView *)WINCX(context)->GetView();

    if(!context || pView->GetFocus() != pView ){
//        TRACE0( "FE_DestroyCaret called, but CNetscapeEditView does not have focus\n");
        return;
    }

    DestroyCaret();

    pView->m_caret.cShown = 0;
    pView->m_caret.bEnabled = FALSE;
}

PUBLIC void
FE_ShowCaret(MWContext * context)
{
    CNetscapeEditView * pView = NULL;

    if(!context)
        return;

    pView = (CNetscapeEditView *)WINCX(context)->GetView();
    pView->m_caret.cShown = 1;
    pView->m_caret.bEnabled = TRUE;
    //TRACE0( "FE_ShowCaret: Caret created and enabled\n");
    pView->ShowCaret();
}


//
// FE_DocumentChanged.
//  The editor is telling the front end that the document has changed.  This 
//  occurs in the normal process of editing.  At any time, changeHeight can
//  be -1 which means grey to the end of the window.  This routine has to 
//  map document coordinates into window coordinates.
//
//  There are 4 possible cases here:
//
/*****

   case 0:

               0,0---------------------------------
                  |                               | 
                  |                               | 
                  |                               | 
                  |===============================| windowStart 
                  |                               |  |
    changeStartY  |-------------------------------|  |
             |    |///////////////////////////////|  |
changeHeight |    |///////////////////////////////|  | windowHeight
             |    |///////////////////////////////|  |
             V    |-------------------------------|  |
                  |                               |  |
                  |===============================|  V
                  |                               | 
                  |                               | 
                  |                               | 
                  --------------------------------- N,N


   case 1:
               0,0---------------------------------
                  |                               | 
                  |                               | 
                  |===============================| windowStart 
                  |                               |  |
    changeStartY  |-------------------------------|  |
             |    |///////////////////////////////|  |
changeHeight |    |///////////////////////////////|  | windowHeight
             |    |///////////////////////////////|  |
             |    |///////////////////////////////|  |
             |    |///////////////////////////////|  |
             |    |===============================|  V
             |    |                               | 
             V    |-------------------------------|  
                  |                               | 
                  |                               | 
                  --------------------------------- N,N



   case 2:
               0,0---------------------------------
                  |                               | 
                  |                               | 
    changeStartY  |-------------------------------|  
             |    |                               | 
             |    |                               | 
             |    |===============================| windowStart 
             |    |///////////////////////////////|  |
             |    |///////////////////////////////|  |
             |    |///////////////////////////////|  |
changeHeight |    |///////////////////////////////|  | windowHeight
             |    |///////////////////////////////|  |
             |    |///////////////////////////////|  |
             |    |///////////////////////////////|  |
             |    |===============================|  V
             |    |                               | 
             V    |-------------------------------|  
                  |                               | 
                  |                               | 
                  --------------------------------- N,N


   case 3:
               0,0---------------------------------
                  |                               | 
                  |                               | 
    changeStartY  |-------------------------------|  
             |    |                               | 
             |    |                               | 
             |    |===============================| windowStart 
             |    |///////////////////////////////|  |
             |    |///////////////////////////////|  |
changeHeight |    |///////////////////////////////|  | windowHeight
             |    |///////////////////////////////|  |
             V    |-------------------------------|  |
                  |                               |  |
                  |===============================|  V
                  |                               | 
                  |                               | 
                  |                               | 
                  --------------------------------- N,N



****/
PUBLIC void 
FE_DocumentChanged(MWContext * context, int32 changeStartY, int32 changeHeight )
{
    CNetscapeEditView * pView = NULL;
    int32 left,right , windowStartY, windowHeight, windowBottom, changeBottom;
    int32 top, bottom;

    CWinCX *pWinCX = VOID2CX(context->fe.cx, CWinCX);
    pView = (CNetscapeEditView *)pWinCX->GetView();
    HDC hDC = pView->GetContextDC();


    // Figure out X and Y coords of the text
    // Figure out X and Y coords of the text
    
    left = 0;
    right = pWinCX->GetWidth();

    windowStartY =  WINCX(context)->GetOriginY();
    windowHeight = pWinCX->GetHeight();
    windowBottom = windowStartY + pWinCX->GetHeight();

    // if the window is before the start of any change, then we don't need to
    //  redisplay anything.
    if( windowBottom < changeStartY ){
        return;
    }

    // make sure there is an overlap
    if( changeHeight == -1 ){
        changeBottom = windowBottom;
    }
    else {
        changeBottom = changeStartY + changeHeight;
    }

    if( changeBottom < windowStartY ){
        return;
    }

    // If window width is expanded past document width,
    //   the horizontal scroll bar disappears.
    // If X origin is not 0, then we have a bad view and
    //   you can't get to doc region to the left of window edge.
    // Detect this and reset X origin to 0 to reposition the view
    // Note: SetDocPosition,  Scroll, etc. all ignore this case
    //       (Browser works by always doing NET_GetURL, which resets origins)
    if( pWinCX->GetDocumentWidth() < pWinCX->GetWidth() ){
        pWinCX->m_lOrgX = 0;
    }
    
    //TODO: Is this the correct way to handle problem of not
    //       refreshing area below the doc?
    //      (Happens when doc shrinks (e.g., delete stuff) to less than window height)
    if( pWinCX->GetDocumentHeight() < pWinCX->GetHeight() ){
        bottom = windowBottom;
    }
    
    top =  max( windowStartY, changeStartY ) - windowStartY;
    bottom = min( windowBottom, changeBottom ) - windowStartY;

    RECT rect;
	::SetRect(&rect, CASTINT(left), CASTINT(top), CASTINT(right), CASTINT(bottom));

    ::LPtoDP(hDC, (POINT*)&rect, 2);
    pView->InvalidateRect( &rect, TRUE );
}


void WFE_HideEditCaret(MWContext * pMWContext)
{
    if( EDT_IS_EDITOR(pMWContext) ){
        CWinCX *pWinCX = VOID2CX(pMWContext->fe.cx, CWinCX);
        CNetscapeEditView * pView = (CNetscapeEditView *)pWinCX->GetView();
        if( !pView ) return;
        if( pView && pView->m_caret.bEnabled && pView->m_caret.cShown ) {
            pView->HideCaret();
            pView->m_caret.cShown = 0;
        } 
    }
}

void WFE_ShowEditCaret(MWContext * pMWContext)
{
    if( EDT_IS_EDITOR(pMWContext) ){
        CWinCX *pWinCX = VOID2CX(pMWContext->fe.cx, CWinCX);
        CNetscapeEditView * pView = (CNetscapeEditView *)pWinCX->GetView();
        if( !pView ) return;
        if( pView->m_caret.bEnabled ) {
            pView->ShowCaret();
            pView->m_caret.cShown = 1;
        }
    }
}

void FE_DisplayAddRowOrColBorder(MWContext * pMWContext, XP_Rect *pRect, XP_Bool bErase)
{
    CWinCX *pCX = VOID2CX(pMWContext->fe.cx, CWinCX);
    if( !pCX )
        return;
    if( bErase )
    {
        RECT rect = {pRect->left, pRect->top, pRect->right, pRect->bottom};
        // Adjust for line thickness
        if( pRect->left == pRect->right)
        {
            rect.right++;
        } else {
            rect.bottom++;
        }
        ::InvalidateRect(pCX->GetPane(), &rect, TRUE); 
        ::UpdateWindow(pCX->GetPane());
    } else {
	    HDC hdc = pCX->GetContextDC();
        COLORREF rgbColor = RGB(0,0,0);
//        Maybe draw in solid color?
//        wfe_GetSelectionColors(pDC->m_rgbBackgroundColor, NULL, &rgbColor);
        HPEN pPen = ::CreatePen(PS_DOT, 1, rgbColor);
	    HPEN pOldPen = (HPEN)::SelectObject(hdc, pPen);
        // Use reverse effect
        int OldRop = ::SetROP2( hdc, R2_NOT );
   
	    ::MoveToEx(hdc, CASTINT(pRect->left), CASTINT(pRect->top), NULL);
        ::LineTo(hdc, CASTINT(pRect->right), CASTINT(pRect->bottom));
	    ::SelectObject(hdc, pOldPen);
	    SetROP2(hdc, OldRop);
        VERIFY(::DeleteObject(pPen));
    }
}

void FE_DisplayEntireTableOrCell(MWContext * pMWContext, LO_Element * pElement)
{
    if( pMWContext && pElement )
    {
        int32 iWidth, iHeight;
        if( pElement->type == LO_TABLE )
        {
            iWidth = pElement->lo_table.width;
            iHeight = pElement->lo_table.height;
        }
        else if ( pElement->type == LO_CELL )
        {
            iWidth = pElement->lo_cell.width;
            iHeight = pElement->lo_cell.height;
        }
        else
            return; // Only Table and Cell types allowed
            
        CWinCX *pCX = VOID2CX(pMWContext->fe.cx, CWinCX);
        RECT rect;
        rect.left = pElement->lo_any.x - pCX->GetOriginX();
        rect.top = pElement->lo_any.y - pCX->GetOriginY();
        rect.right = rect.left + iWidth;
        rect.bottom = rect.top + iHeight;

        if( pElement->type == LO_TABLE && 
            pElement->lo_table.border_left_width == 0 &&
            pElement->lo_table.border_right_width == 0 &&
            pElement->lo_table.border_top_width == 0 &&
            pElement->lo_table.border_bottom_width == 0 )
        {
            // We are displaying a "zero-width" table,
            //  increase rect by 1 pixel 'cause thats
            //  where we drew the table's dotted-line border 
            ::InflateRect(&rect, 1, 1);
        }
        // Include the inter-cell spacing area also used for highlighting a cell
        int32 iExtraSpace;
        if( pElement->type == LO_CELL && pElement->lo_cell.border_width < ED_SELECTION_BORDER &&
            0 < (iExtraSpace = pElement->lo_cell.inter_cell_space / 2) )
        {
            ::InflateRect(&rect, iExtraSpace, iExtraSpace);
        }

        InvalidateRect(pCX->GetPane(), &rect, TRUE);
    }
}

void FE_DisplayDropTableFeedback(MWContext * pMWContext, EDT_DragTableData *pDragData)
{
    if(!pMWContext || !EDT_IS_EDITOR(pMWContext) || !pDragData)
        return;

    CWinCX *pWinCX = VOID2CX(pMWContext->fe.cx, CWinCX);
    CNetscapeEditView * pView = (CNetscapeEditView *)pWinCX->GetView();
    if( pView )
    {
        if( pDragData->iDropType == ED_DROP_REPLACE_CELL )
        {
            //TODO: FEEDBACK WHEN REPLACING
            // THIS MAY BE DONE IN XP CODE BY SETTING LO_ELE_SELECTED_SPECIAL
            // THEN JUST REDRAWING THE TABLE
        } else {
            // Use caret to show inserting between cells
            pView->CreateSolidCaret(pDragData->iWidth, pDragData->iHeight);
            pView->SetCaretPos(CPoint(pDragData->X, pDragData->Y));
            pView->m_caret.cShown = 1;
            pView->m_caret.bEnabled = TRUE;
            pView->ShowCaret();
        }
    }
}
