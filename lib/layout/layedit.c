/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* Moved here so we can expose 2 functions outside of ifdef EDITOR */
#include "xp.h"
#include "layout.h"
#include "layers.h"

#ifdef EDITOR

/* define LAYEDIT so LO_RelayoutData is not defined. */
#define LAYEDIT		1
typedef struct LO_RelayoutData_struct LO_RelayoutData;
 
#include "pa_parse.h"
#include "edt.h"
#include "libi18n.h"

#ifdef max
#undef max
#endif
#define max(a, b) (((a) > (b)) ? (a) : (b))

struct LO_RelayoutData_struct {
	MWContext *context;
	ED_TagCursor* pTagCursor;
	lo_TopState* top_state;
	lo_DocState* old_state;
	lo_DocState* new_state;
	int32 iStartLine;
	int iStartEditOffset;
};

#define LINE_INC		100

#ifdef DEBUG

PRIVATE
Bool
lo_VerifyStateLayoutImplementation( MWContext *pContext, lo_TopState *top_state, lo_DocState *state, Bool print);

#endif

#ifdef XP_WIN16

#define SIZE_LIMIT		32000

void lo_GrowLineArrayByOneForWin16( lo_DocState *state, intn lineNum) 
{
    /* This code is a modified version of the lo_FlushLineList
     * that grows the line array to hold more lines.
    */

	intn a_size;
	intn a_indx;
	intn a_line;
	XP_Block *larray_array;
	LO_Element **line_array;

	a_size = SIZE_LIMIT / sizeof(LO_Element *);
	a_indx = (lineNum - 1) / a_size;
	a_line = lineNum - (a_indx * a_size);

	XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
	state->line_array = larray_array[a_indx];

	if (a_line == a_size)
	{
		state->line_array = XP_ALLOC_BLOCK(LINE_INC *
					sizeof(LO_Element *));
		if (state->line_array == NULL)
		{
			XP_UNLOCK_BLOCK(state->larray_array);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		line_array[0] = NULL;
		XP_UNLOCK_BLOCK(state->line_array);
		state->line_array_size = LINE_INC;

		state->larray_array_size++;
		XP_UNLOCK_BLOCK(state->larray_array);
		state->larray_array = XP_REALLOC_BLOCK(
			state->larray_array, (state->larray_array_size
			* sizeof(XP_Block)));
		if (state->larray_array == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}
		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		larray_array[state->larray_array_size - 1] = state->line_array;
		state->line_array = larray_array[a_indx];
	}
	else if (a_line >= state->line_array_size)
	{
		state->line_array_size += LINE_INC;
		if (state->line_array_size > a_size)
		{
			state->line_array_size = (intn)a_size;
		}
		state->line_array = XP_REALLOC_BLOCK(state->line_array,
		    (state->line_array_size * sizeof(LO_Element *)));
		if (state->line_array == NULL)
		{
			XP_UNLOCK_BLOCK(state->larray_array);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		larray_array[a_indx] = state->line_array;
	}

	/*
	 * Place this line of elements into the line array.
	 */
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[a_line - 1] = state->line_list;
	XP_UNLOCK_BLOCK(state->line_array);

	XP_UNLOCK_BLOCK(state->larray_array);
}

#endif

PRIVATE
Bool lo_GrowLineArray( lo_DocState *state, int32 iMaxLines )
{
    /* This code is a modified version of the lo_FlushLineList
     * that grows the line array to hold more lines.
    */

#ifdef XP_WIN16
    intn linenum = state->line_array_size;
    while ( iMaxLines > linenum){
        lo_GrowLineArrayByOneForWin16(state, linenum);
        if ( state->top_state->out_of_memory == TRUE)
        {
            return FALSE;
        }
        linenum++; 
    }
#else
	int32 line_inc = 0;

	while ( iMaxLines > state->line_array_size + line_inc)
	{

		if (state->line_array_size > (LINE_INC * 10))
		{
			line_inc += state->line_array_size / 10;
		}
		else
		{
			line_inc += LINE_INC;
		}
	}

	if ( line_inc != 0 )
	{
		state->line_array = XP_REALLOC_BLOCK(state->line_array,
			((state->line_array_size + line_inc) *
			sizeof(LO_Element *)));
		if (state->line_array == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return FALSE;
		}
		state->line_array_size += line_inc;
	}

#endif

	return TRUE;
}

void lo_MergeStateMoveElement(LO_Element* eptr, int32 yDelta, int32* pEle_id);

PRIVATE
void lo_MergeStateMoveCell(LO_CellStruct* cellPtr, int32 yDelta, int32* pEle_id)
{
    /* Like lo_ShiftCell, only we also renumber the element ids. */
	LO_Element* eptr;
    if (cellPtr->cell_bg_layer)
        CL_MoveLayer(cellPtr->cell_bg_layer,
                     cellPtr->x, cellPtr->y);
	for( eptr = cellPtr->cell_list;
        eptr;
        eptr = eptr->lo_any.next)
	{
		lo_MergeStateMoveElement(eptr, yDelta, pEle_id);
    }
	for( eptr = cellPtr->cell_float_list;
        eptr;
        eptr = eptr->lo_any.next)
	{
		lo_MergeStateMoveElement(eptr, yDelta, pEle_id);
    }
}

void lo_MergeStateMoveElement(LO_Element* eptr, int32 yDelta, int32* pEle_id)
{
    eptr->lo_any.ele_id = (*pEle_id)++;
	eptr->lo_any.y += yDelta;
    /* Descend into cells */
    if ( eptr->type == LO_CELL )
    {
        lo_MergeStateMoveCell(&eptr->lo_cell, yDelta, pEle_id);
    }
    else if (eptr->type == LO_IMAGE)
    {
        if (eptr->lo_image.layer)
        {
            CL_OffsetLayer(eptr->lo_image.layer, 0, yDelta);
        }
    }
}

PRIVATE
void lo_MergeState( MWContext *context, lo_DocState *old_state, int32 iStartLine, 
		int32 iEndLine, lo_DocState *new_state, int32* pRetY, int32* pRetHeight )
{
	LO_Element **old_line_array, **new_line_array;
	LO_Element *prev_element, *start_element, *eptr, *end_element, *new_end_element;
	int32 yDelta;
	int32 ele_id;
	int32 new_line_num;
	int32 new_changed_line_count;
	int32 old_changed_line_count;
	int32 old_delta;
	int32 old_lines_to_move;
	Bool relayout_to_end;
	Bool old_state_empty = (old_state->line_num < 2);

	new_changed_line_count = new_state->line_num-1;
	old_changed_line_count = iEndLine-iStartLine;

	new_line_num = old_state->line_num - old_changed_line_count + 
			new_changed_line_count;

	/*
	 *  Grow the line array if we need to.
	*/	
	if( !lo_GrowLineArray( old_state, new_line_num+1 ) )
	{
		return;		/* out of memory */
	}
		

	/* lock down the line array and copy the lines from the new_state into the
	 * old state.
	*/
	XP_LOCK_BLOCK(old_line_array, LO_Element **, old_state->line_array);
	XP_LOCK_BLOCK(new_line_array, LO_Element **, new_state->line_array);

	if( iEndLine == old_state->line_num-1 ){
		/* ERIC: help! */
		/* There is something very wrong here.  We the layout engine is 
		 * telling us the wrong line count.  Force the end by stuffing
		 * a zero
		*/

		/*XP_ASSERT( old_line_array[iEndLine] == 0 );*/
		old_line_array[iEndLine] = 0;
	}

	start_element = old_line_array[iStartLine];
	end_element = old_line_array[iEndLine];
	relayout_to_end = ( end_element == 0 );

	/*
	 * Break the chain so when we recycle these elements they don't continue
	 *  and deallocate the entire tree.
	*/
	if( end_element && end_element->lo_any.prev){
		end_element->lo_any.prev->lo_any.next = 0;
	}

	/*
	 * Shrink or grow the the old line array.  XP_BCOPY is supposed to handle
	 *  overlaps.
	*/
	old_delta = new_changed_line_count - old_changed_line_count;
	old_lines_to_move = (old_state->line_num-1) - iEndLine;

	XP_BCOPY( &old_line_array[iEndLine], &old_line_array[iEndLine+old_delta],
			sizeof(LO_Element*)*old_lines_to_move );

	old_state->line_num = new_line_num;

	/* if we shrunk the line array, make sure it is 0 filled
	*/
	if( old_delta < 0 ){
		XP_BZERO( &old_line_array[iEndLine+old_delta+old_lines_to_move],
			sizeof(LO_Element**)*(old_delta * -1) );
	}

	if( !old_state_empty ) {
		*pRetY = start_element->lo_any.y;
	}
	else {
		*pRetY = 0;
	}

	if( iStartLine != 0 )
	{
		prev_element = start_element->lo_any.prev;
	}
	else {
		prev_element = 0;
	}

	if( new_changed_line_count ){
		if( !old_state_empty )
		{
			yDelta = start_element->lo_any.y - new_line_array[0]->lo_any.y;
			ele_id = start_element->lo_any.ele_id;
		}
		else {
			yDelta = 0;
			ele_id = 0;
		}

		XP_BCOPY( &new_line_array[0], &old_line_array[iStartLine], 
				sizeof(LO_Element*)*new_changed_line_count );

		if( prev_element )
		{
			prev_element->lo_any.next = old_line_array[iStartLine];
			old_line_array[iStartLine]->lo_any.prev = prev_element;
		}

		eptr = old_line_array[iStartLine];
		while( eptr != NULL)
		{
			lo_MergeStateMoveElement(eptr, yDelta, &ele_id);
			new_end_element = eptr;
			eptr = eptr->lo_any.next;
		}
	}
	else {
		yDelta = 0;
		if( !old_state_empty )
		{
			ele_id = start_element->lo_any.ele_id;
		}
		else 
		{
			ele_id = 0;
		}
		new_end_element = prev_element;
	}

	/*
	 * We now have to patch the the bottom of the newly generated tags into
	 *  the old tags.
	 *
	*/
	if( relayout_to_end == FALSE )
	{
		end_element->lo_any.prev = new_end_element;
		new_end_element->lo_any.next = end_element;

		yDelta = (new_end_element->lo_any.y+new_end_element->lo_any.line_height)
		 			- end_element->lo_any.y;
		ele_id = new_end_element->lo_any.ele_id+1;

		eptr = end_element;
		while( eptr != NULL)
		{
			lo_MergeStateMoveElement(eptr, yDelta, &ele_id);
			eptr = eptr->lo_any.next;
		}
	}
	else
    {
        /* trivia: The -2 is because lines are 1 based, and new_line_num is one
         * more than the allocated number of lines. The array is zero based.
         * So subtract 1 to get the count zero based, and another 1 to get the
         * last allocated line rather than the first un-allocated line.
         */
		if( !old_state_empty )
		{
	        eptr = old_line_array[new_line_num - 2];
		}
		else
		{
			eptr = new_end_element;
		}
        /* move to the last element of the last line line */
		while( eptr != NULL && eptr->lo_any.next != NULL)
		{
            eptr = eptr->lo_any.next;
        }
	    old_state->end_last_line = eptr;
    }
	
	if( new_changed_line_count )
	{
		if( yDelta == 0 && !relayout_to_end )
		{
			eptr = new_end_element;
			*pRetHeight = eptr->lo_any.y - *pRetY + eptr->lo_any.line_height;
		}
		else
		{
			/* the last line in the document. */
			*pRetHeight = -1;
		}
		eptr = old_line_array[new_line_num-2];

        /* We need to add a few pixels to the new line width so that
         * there's always room for the cursor at the end of the line,
         * even if there's a scroll bar.
         *
         * JPNote: We never, ever, shrink the width of the document once
         * we've enlarged it. This is bad.
         */

#ifdef XP_MAC
#define INCREMENT_WIDTH_AMT	15
#else
#define INCREMENT_WIDTH_AMT	20
#endif

#define max(a, b) (((a) > (b)) ? (a) : (b))
		old_state->max_width = max( old_state->max_width, new_state->max_width + INCREMENT_WIDTH_AMT );
		old_state->y = eptr->lo_any.y +eptr->lo_any.line_height;
#undef max

        LO_SetDocumentDimensions(context, old_state->max_width, 
                                 eptr->lo_any.y +eptr->lo_any.line_height);
	}
	else
	{
		eptr = start_element;
		if( eptr )
		{
			*pRetHeight = eptr->lo_any.y - *pRetY + eptr->lo_any.line_height;
		}
		else 
		{
			*pRetHeight = -1;
		}
	}

#if 0
	XP_TRACE(( "line count = %d, end pointer = %d\n", 
			new_line_num, old_line_array[new_line_num-2] ));

	XP_ASSERT( old_line_array[new_line_num-2] != 0 );
#endif

	XP_UNLOCK_BLOCK(new_state->line_array);
	XP_UNLOCK_BLOCK(old_state->line_array);

	/* LTNOTE: need to recycle element chain from start_element
	*/
	if( start_element )
	{
		lo_relayout_recycle(context, old_state, start_element);
	}

    /* 
     * XXX BUGBUG We don't deal with layers yet, but we will eventually have
     * to worry about layers that are in the old state.
     */

#ifdef DEBUG
    lo_VerifyLayout(context);
#endif /* DEBUG */
}

PRIVATE
void lo_MergeElements( MWContext *context, lo_DocState *old_state, int32 iStartLine, 
		int32 iEndLine, lo_DocState *new_state, int32* pRetY, int32* pRetHeight )
{
	LO_Element **old_line_array, **new_line_array;
	LO_Element *prev_element, *eptr, *end_element, *new_end_element;
	int32 yDelta;
	int32 ele_id;
	int32 new_line_num;
	int32 new_changed_line_count;
	int32 old_changed_line_count;
	int32 old_delta;
	int32 old_lines_to_move;
	Bool relayout_to_end;
	Bool old_state_empty = (old_state->line_num < 2);

	new_changed_line_count = new_state->line_num-1;
	old_changed_line_count = iEndLine-iStartLine;

	new_line_num = old_state->line_num - old_changed_line_count + 
			new_changed_line_count;

	/*
	 *  Grow the line array if we need to.
	*/	
	if( !lo_GrowLineArray( old_state, new_line_num+1 ) )
	{
		return;		/* out of memory */
	}
		

	/* lock down the line array and copy the lines from the new_state into the
	 * old state.
	*/
	XP_LOCK_BLOCK(old_line_array, LO_Element **, old_state->line_array);
	XP_LOCK_BLOCK(new_line_array, LO_Element **, new_state->line_array);

	if( iEndLine == old_state->line_num-1 ){
		/* ERIC: help! */
		/* There is something very wrong here.  We the layout engine is 
		 * telling us the wrong line count.  Force the end by stuffing
		 * a zero
		*/

		/*XP_ASSERT( old_line_array[iEndLine] == 0 );*/
		old_line_array[iEndLine] = 0;
	}

	end_element = old_line_array[iEndLine];
	relayout_to_end = ( end_element == 0 );

	/*
	 * Break the chain so when we recycle these elements they don't continue
	 *  and deallocate the entire tree.
	*/
	if( end_element && end_element->lo_any.prev){
		end_element->lo_any.prev->lo_any.next = 0;
	}

	/*
	 * Shrink or grow the the old line array.  XP_BCOPY is supposed to handle
	 *  overlaps.
	*/
	old_delta = new_changed_line_count - old_changed_line_count;
	old_lines_to_move = (old_state->line_num-1) - iEndLine;

	XP_BCOPY( &old_line_array[iEndLine], &old_line_array[iEndLine+old_delta],
			sizeof(LO_Element*)*old_lines_to_move );

	old_state->line_num = new_line_num;

	/* if we shrunk the line array, make sure it is 0 filled
	*/
	if( old_delta < 0 ){
		XP_BZERO( &old_line_array[iEndLine+old_delta+old_lines_to_move],
			sizeof(LO_Element**)*(old_delta * -1) );
	}

	if( iStartLine != 0 )
	{
		/* find the last element on the previous line */
		prev_element = old_line_array[iStartLine - 1];
		while ( prev_element != NULL )
		{
			if ( prev_element->lo_any.next == NULL )
				break;
			
			prev_element = prev_element->lo_any.next;
		}
	}
	else {
		prev_element = 0;
	}
	
	if( old_state_empty ) {
		*pRetY = 0;
	}

	ele_id = 0;
	
	if( new_changed_line_count ){
		/* copy in new lines */
		XP_BCOPY( &new_line_array[0], &old_line_array[iStartLine], sizeof(LO_Element*)*new_changed_line_count );

		if( prev_element )
		{
			prev_element->lo_any.next = old_line_array[iStartLine];
			old_line_array[iStartLine]->lo_any.prev = prev_element;
			
			ele_id = prev_element->lo_any.ele_id + 1;
		}

		eptr = old_line_array[iStartLine];
		
		while( eptr != NULL)
		{
			lo_MergeStateMoveElement(eptr, 0, &ele_id);
			new_end_element = eptr;
			eptr = eptr->lo_any.next;
		}
	}
	else {
		/* no new lines to copy in, so get ready to delete the old ones */
		yDelta = 0;
		if( prev_element )
		{
			ele_id = prev_element->lo_any.ele_id + 1;
		}
		else 
		{
			ele_id = 0;
		}
		new_end_element = prev_element;
	}

	/*
	 * We now have to patch the the bottom of the newly generated tags into
	 *  the old tags.
	 *
	*/
	if( relayout_to_end == FALSE )
	{
		end_element->lo_any.prev = new_end_element;
		new_end_element->lo_any.next = end_element;

		yDelta = (new_end_element->lo_any.y+new_end_element->lo_any.line_height)
		 			- end_element->lo_any.y;
		ele_id = new_end_element->lo_any.ele_id+1;

		eptr = end_element;
		while( eptr != NULL)
		{
			lo_MergeStateMoveElement(eptr, yDelta, &ele_id);
			eptr = eptr->lo_any.next;
		}
	}
	else
    {
        /* trivia: The -2 is because lines are 1 based, and new_line_num is one
         * more than the allocated number of lines. The array is zero based.
         * So subtract 1 to get the count zero based, and another 1 to get the
         * last allocated line rather than the first un-allocated line.
         */
		if( !old_state_empty )
		{
	        eptr = old_line_array[new_line_num - 2];
		}
		else
		{
			eptr = new_end_element;
		}
        /* move to the last element of the last line line */
		while( eptr != NULL && eptr->lo_any.next != NULL)
		{
            eptr = eptr->lo_any.next;
        }
	    old_state->end_last_line = eptr;
    }
	
	if( new_changed_line_count )
	{
		if( yDelta == 0 && !relayout_to_end )
		{
			eptr = new_end_element;
			*pRetHeight = eptr->lo_any.y - *pRetY + eptr->lo_any.line_height;
		}
		else
		{
			/* the last line in the document. */
			*pRetHeight = -1;
		}
		eptr = old_line_array[new_line_num-2];

        /* We need to add a few pixels to the new line width so that
         * there's always room for the cursor at the end of the line,
         * even if there's a scroll bar.
         *
         * JPNote: We never, ever, shrink the width of the document once
         * we've enlarged it. This is bad.
         */

#ifdef XP_MAC
#define INCREMENT_WIDTH_AMT	15
#else
#define INCREMENT_WIDTH_AMT	20
#endif

#define max(a, b) (((a) > (b)) ? (a) : (b))
		old_state->max_width = max( old_state->max_width, new_state->max_width + INCREMENT_WIDTH_AMT );
		old_state->y = eptr->lo_any.y +eptr->lo_any.line_height;
#undef max

        LO_SetDocumentDimensions(context, old_state->max_width, 
                                 eptr->lo_any.y +eptr->lo_any.line_height);
	}
	else
	{
#if 0
		eptr = start_element;
		if( eptr )
		{
			*pRetHeight = eptr->lo_any.y - *pRetY + eptr->lo_any.line_height;
		}
		else 
		{
			*pRetHeight = -1;
		}
#else
		XP_ASSERT(0);
		*pRetHeight = -1;
#endif
	}

#if 0
	XP_TRACE(( "line count = %d, end pointer = %d\n", 
			new_line_num, old_line_array[new_line_num-2] ));

	XP_ASSERT( old_line_array[new_line_num-2] != 0 );
#endif

	XP_UNLOCK_BLOCK(new_state->line_array);
	XP_UNLOCK_BLOCK(old_state->line_array);

    /* 
     * XXX BUGBUG We don't deal with layers yet, but we will eventually have
     * to worry about layers that are in the old state.
     */

#ifdef DEBUG
    lo_VerifyLayout(context);
#endif /* DEBUG */
}

PRIVATE
LO_RelayoutData* lo_NewRelayoutData( MWContext* context, ED_TagCursor* pCursor, 
			int32 iStartLine, int iStartEditOffset )
{
	int32 doc_id;
	LO_RelayoutData *pRd;

	pRd = XP_NEW( LO_RelayoutData );
	if( pRd == 0 )
	{
		return 0;
	}

	pRd->pTagCursor = pCursor;
	pRd->context = context;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	pRd->top_state = lo_FetchTopState(doc_id);
	if ((pRd->top_state == NULL)||(pRd->top_state->doc_state == NULL))
	{
		return 0;
	}
	pRd->old_state = pRd->top_state->doc_state;

	pRd->iStartLine = iStartLine;
	pRd->iStartEditOffset = iStartEditOffset;

	pRd->new_state = lo_NewLayout( context, pRd->old_state->win_width, 
					pRd->old_state->win_height, pRd->old_state->win_left, 
					pRd->old_state->win_top, pRd->old_state );
	pRd->new_state->display_blocked = TRUE;
	pRd->new_state->edit_relayout_display_blocked = TRUE;
	
	return pRd;
}

PRIVATE
void lo_FreeRelayoutData( MWContext* context, LO_RelayoutData* pRd)
{
    if ( pRd ) {
        if ( pRd->new_state ) {
            lo_DocState* state = pRd->new_state;
			
            /*
             * If there is a blocking element, we'll get into trouble later.
             * Check if CEditImageElement::FinishedLoad is being called.
             */

            XP_ASSERT ( state->top_state->layout_blocking_element == NULL );

            lo_FreeLayoutData(context, state);
            lo_InternalDiscardDocument(context, state, NULL, FALSE);
        }
        XP_FREE(pRd);
    }
}

/* Remove all mquote bullets from elist, returning the list of mquote
   bullets. */ 
PRIVATE 
LO_Element *lo_strip_mquotes(LO_Element **elist)
{
  /* Return value. */
  LO_Element *mquotes = NULL;
  LO_Element *mquotesTail = NULL;

  /* For traversing the elist. */
  LO_Element *eptr = *elist;

  if (!elist || !*elist) {
    return NULL;
  }

  LO_LockLayout();

  while (eptr) {
    /* Found a mquote bullet. */
    if (eptr->type == LO_BULLET && 
        eptr->lo_bullet.bullet_type == BULLET_MQUOTE) {

      /* Delete from the passed-in element list. */
      if (eptr->lo_any.prev) {
        XP_ASSERT(eptr->lo_any.prev->lo_any.next == eptr);
        eptr->lo_any.prev->lo_any.next = eptr->lo_any.next;
      }
      else {
        /* beginning of list, change the head. */
        XP_ASSERT(*elist == eptr);
        *elist = eptr->lo_any.next;
      }
      if (eptr->lo_any.next) {
        eptr->lo_any.next->lo_any.prev = eptr->lo_any.prev;
      }
      eptr->lo_any.prev = mquotesTail;


      /* Add to mquotes list. */
      if (mquotes) {
        XP_ASSERT(mquotesTail);
        mquotesTail->lo_any.next = eptr;
      }
      else {
        /* Start the mquotes list. */
        mquotes = eptr;
      }
      mquotesTail = eptr;

      /* Move eptr to next element, eptrPrev stays the same. */
      eptr = eptr->lo_any.next;

      /* NULL-terminate mquotes list. */
      mquotesTail->lo_any.next = NULL;
    }

    /* Not a mquote bullet, just skip it. */
    else {
      eptr = eptr->lo_any.next;
    }
  }

  LO_UnlockLayout();
  return mquotes;
}

void LO_EditorReflow(MWContext *context, ED_TagCursor *pCursor, 
			int32 iStartLine, int iStartEditOffset, XP_Bool bDisplayTables)
{
	PA_Tag *pTag;
	PA_Tag *pNextTag = 0;
	LO_Element *eptr;
	LO_Element **line_array;
	int32 changedY, changedHeight;
	LO_RelayoutData* pRd;
	Bool bFoundBreak, bBreakIsEndTag;
	int32 iEndLine = -1;
    LO_Element *leadingMquotes = NULL;
    ED_Element *pTagElement;
    Bool bItemComplete;
	LO_Element * element;
    ED_ElementType	elementType;
	LO_Element * startElement;
	LO_Element * endElement;
	LO_Element * prevElement;
	LO_Element ** old_line_array;
    
    context->is_editor |= EDT_RELAYOUT_FLAG; /* Relayout flag */
	pRd = lo_NewRelayoutData( context, pCursor, iStartLine, iStartEditOffset );

	/* save the floating element list for later deletion. */
	if( pRd->old_state->float_list != 0 )
	{
		lo_relayout_recycle(context, pRd->new_state, pRd->old_state->float_list);
        pRd->old_state->float_list = NULL;
	}
	
	/* unhook the line array at our start line */
	XP_LOCK_BLOCK(old_line_array, LO_Element **, pRd->old_state->line_array);
	startElement = old_line_array[ iStartLine ];
#ifdef UNHOOK
	if ( ( startElement != NULL ) && ( startElement->lo_any.prev != NULL ) )
	{
		startElement->lo_any.prev->lo_any.next = NULL;
		startElement->lo_any.prev = NULL;
	}
#endif
	
	prevElement = NULL;
	
	if ( startElement != NULL  )
	{
		prevElement = startElement->lo_any.prev;
	}

	/* assume we'll layout to the end */
	endElement = NULL;
	
	if ( startElement != NULL )
	{
		changedY = startElement->lo_any.y;
	}
	else
	{
		changedY = 0;
	}

	/* set up our state's y position so we don't have to shift the new elements down */
	pRd->new_state->y = startElement->lo_any.y;
	pRd->top_state->element_id = startElement->lo_any.ele_id;
	
	/* set up any external state (such as lists) */
	while( (pTag = pNextTag) != 0 
			|| (pTag = EDT_TagCursorGetNextState(pCursor)) != 0 )
	{
		lo_LayoutTag(pRd->context, pRd->new_state, pTag);
		pNextTag = pTag->next;
		PA_FreeTag(pTag);
	}
	
	eptr = NULL;
	XP_LOCK_BLOCK(line_array, LO_Element **, pRd->new_state->line_array);
	eptr = line_array[0];
	if (eptr != NULL )
	{
		lo_relayout_recycle(context, pRd->new_state, eptr); 	
	}
	XP_UNLOCK_BLOCK(pRd->old_state->line_array);

  /* 
     Get list of any mailing quote bullets, removing them from line_list.
     These are the only elements that we want to preserve. 
     lo_strip_mquotes deals properly with a NULL input.
     Note that this can change the value of pRd->new_state->line_list.
  */
  leadingMquotes = lo_strip_mquotes(&pRd->new_state->line_list);
  if ( pRd->new_state->line_list ) {
		lo_relayout_recycle(context, pRd->new_state, pRd->new_state->line_list);
  }

  /* Only leave the leading mquote bullets, if any. */
	pRd->new_state->line_list = leadingMquotes; 
	pRd->new_state->line_num = 1;

	/* hack to find the endline */
	/* run through the tags with the tag cursor until we get to the end line */
	bFoundBreak = FALSE;
	pNextTag = NULL;
	while( !bFoundBreak )
	{
		if( pNextTag == NULL )
		{
			pTag = EDT_TagCursorGetNext(pRd->pTagCursor);
		}
		else 
		{
			pTag = pNextTag;
		}
		
		if( pTag == NULL )
		{
			break;
		}
		
		PA_FreeTag(pTag);

		if( pNextTag == 0 )
		{
			bFoundBreak = EDT_TagCursorAtBreak( pRd->pTagCursor, &bBreakIsEndTag );
		}
	}

	iEndLine = EDT_TagCursorCurrentLine( pRd->pTagCursor );
    if( iEndLine == -1 )
    {
    	/* BRAIN DAMAGE: figger this out - why do we need -2? */
    	iEndLine = pRd->old_state->line_num - 2;
    	if ( iEndLine < 0 )
    	{
    		iEndLine = pRd->old_state->line_num - 1;
    	}
	}
	
	endElement = old_line_array[ iEndLine ];
	
	if ( endElement != NULL )
	{
		if ( endElement->lo_any.prev != NULL )
		{
			endElement->lo_any.prev->lo_any.next = NULL;
			endElement->lo_any.prev = NULL;
		}
	}
	
	/* unhook our elements from the old state record */
	if ( pRd->old_state->line_list == startElement )
	{
		pRd->old_state->line_list = NULL;
	}
	
	if ( pRd->old_state->end_last_line == startElement )
	{
		pRd->old_state->end_last_line = NULL;
	}
		
	/* reflow from our start to our end element */
	LO_Reflow(pRd->context, pRd->new_state, startElement, endElement );
	
	/* layout any cleanup tags */
	if( bFoundBreak )
	{
		if( bBreakIsEndTag ){
			pTag = EDT_TagCursorGetNext(pRd->pTagCursor);
			lo_LayoutTag(pRd->context, pRd->new_state, pTag);
		}
		else {
			pTag = EDT_TagCursorGetNext(pRd->pTagCursor);
            if ( pTag->type == P_TABLE ) {
                lo_CloseOutLayout( pRd->context, pRd->new_state);
            }
            else {
			    lo_LayoutTag(pRd->context, pRd->new_state, pTag);
            }
		}
        EDT_DeleteTagChain(pTag);

		/* don't close layout.  We just flushed this line to the proper
		 *  height, there is a start of a new tag in the buffer.
		*/
	}
	
	/* cleanup the element list for the old elements before the reflow */
	if ( prevElement )
	{
		prevElement->lo_any.next = NULL;
	}
	
#if defined( DEBUG_shannon )
	XP_TRACE(("\n\nELEMENTS TO MERGE - new reflow"));
	lo_VerifyStateLayoutImplementation( context, pRd->top_state, pRd->new_state, TRUE);
#endif
	
	lo_MergeElements( context, pRd->old_state, iStartLine, iEndLine,
				pRd->new_state, &changedY, &changedHeight );

    /* Free the layout */
    lo_FreeRelayoutData(context, pRd);
    
    context->is_editor &= ~EDT_RELAYOUT_FLAG;
	FE_DocumentChanged( context, changedY, changedHeight );
}

void LO_Relayout(MWContext *context, ED_TagCursor *pCursor, 
			int32 iStartLine, int iStartEditOffset, XP_Bool bDisplayTables)
{
	PA_Tag *pTag;
	PA_Tag *pNextTag = 0;
	LO_Element *eptr;
	LO_Element **line_array;
	int32 changedY, changedHeight;
	LO_RelayoutData* pRd;
	Bool bFoundBreak, bBreakIsEndTag;
	int32 iEndLine = -1;
    LO_Element *leadingMquotes = NULL;
    context->is_editor |= EDT_RELAYOUT_FLAG; /* Relayout flag */
	pRd = lo_NewRelayoutData( context, pCursor, iStartLine, iStartEditOffset );

    /* We need to keep images loaded during relayout. Images are reference counted.
     * When the total number of layout elements that use an image drops to zero,
     * the image is kicked out of the cache. We have to have the images in the cache,
     * or else the layout engine will start an asynchronous load. The asynchronous load
     * is bad because we don't have any way of waiting for it to complete. And that in
     * turn means we would have to leave LO_Relayout with an inconsistent CEditElement /
     * LO_Element state.
     *
     * Here's how there might come to be elements on the float list: When the document has
     * right-aligned images, they are placed on the float list during the original load.
     *
     * Happily, there's a way around this problem: We make sure we don't delete the old
     * image tags until we've created the new image tags. That's why we use lo_relayout_recycle,
     * which puts the image elements on the "trash" list of pRd->new_state. Then we clean the
     * trash in lo_InternalDiscardDocument.
     *
     */

	/* save the floating element list for later deletion. */
	if( pRd->old_state->float_list != 0 )
	{
		lo_relayout_recycle(context, pRd->new_state, pRd->old_state->float_list);
        pRd->old_state->float_list = NULL;
	}

	while( (pTag = pNextTag) != 0 
			|| (pTag = EDT_TagCursorGetNextState(pCursor)) != 0 )
	{
		lo_LayoutTag(pRd->context, pRd->new_state, pTag);
		pNextTag = pTag->next;
		PA_FreeTag(pTag);
	}
	
	eptr = NULL;
	XP_LOCK_BLOCK(line_array, LO_Element **, pRd->new_state->line_array);
	eptr = line_array[0];
	if (eptr != NULL )
	{
		lo_relayout_recycle(context, pRd->new_state, eptr); 	
	}
	XP_UNLOCK_BLOCK(pRd->old_state->line_array);

  /* 
     Get list of any mailing quote bullets, removing them from line_list.
     These are the only elements that we want to preserve. 
     lo_strip_mquotes deals properly with a NULL input.
     Note that this can change the value of pRd->new_state->line_list.
  */
  leadingMquotes = lo_strip_mquotes(&pRd->new_state->line_list);
  if ( pRd->new_state->line_list ) {
		lo_relayout_recycle(context, pRd->new_state, pRd->new_state->line_list);
  }

  /* Only leave the leading mquote bullets, if any. */
	pRd->new_state->line_list = leadingMquotes; 
	pRd->new_state->line_num = 1;

	/* while there are tags to parse... */
	bFoundBreak = FALSE;
	pNextTag = NULL;
	while( !bFoundBreak )
	{
		if( pNextTag == NULL )
		{
			pTag = EDT_TagCursorGetNext(pRd->pTagCursor);
		}
		else 
		{
			pTag = pNextTag;
		}
		if( pTag == NULL ){
			break;
		}

		pRd->new_state->display_blocked = TRUE;
		pNextTag = pTag->next;
		if( iStartEditOffset && pTag->type == P_TEXT )
		{
			pRd->new_state->edit_current_offset = iStartEditOffset;
			pRd->new_state->edit_force_offset = TRUE;
			lo_LayoutTag(pRd->context, pRd->new_state, pTag);
			iStartEditOffset = 0;
			pRd->new_state->edit_force_offset = FALSE;
		    PA_FreeTag(pTag);
		}
		else
		{
/*			lo_LayoutTag(pRd->context, pRd->new_state, pTag);*/
            /* Matches code at end of LO_ProcessTag */
            lo_DocState *state = pRd->new_state;
	        lo_DocState *orig_state;
	        lo_DocState *up_state;
            PA_Tag* tag = pTag;

	        /*
	         * Divert all tags to the current sub-document if there is one.
	         */
	        up_state = NULL;
	        orig_state = state;
            if ( bDisplayTables ) {
                while (state->sub_state != NULL)
	            {
		            lo_DocState *new_state;

		            up_state = state;
		            new_state = state->sub_state;
		            state = new_state;
	            }
            }

	        /* orig_state->top_state->layout_status = status; */

	        {
		        lo_DocState *tmp_state;
		        Bool may_save;

		        if ((state->is_a_subdoc == SUBDOC_CELL)||
			        (state->is_a_subdoc == SUBDOC_CAPTION))
		        {
			        may_save = TRUE;
		        }
		        else
		        {
			        may_save = FALSE;
		        }

                /* Some table routines reach out to find the top doc state.
                 * So we replace it for the duration of this call.
                 */
                pRd->top_state->doc_state = pRd->new_state;
	            state->edit_relayout_display_blocked = TRUE;
	            state->display_blocked = TRUE;

		        lo_LayoutTag(context, state, tag);
                pRd->top_state->doc_state = pRd->old_state;
		        tmp_state = lo_CurrentSubState(orig_state);

		        if (may_save != FALSE)
		        {
			        /*
			         * That tag popped us up one state level.  If this new
			         * state is still a subdoc, save the tag there.
			         */
			        if (tmp_state == up_state)
			        {
				        if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				            (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				        {
				            lo_SaveSubdocTags(context, tmp_state, tag);
				        }
				        else
				        {
				            PA_FreeTag(tag);
				        }
			        }
			        /*
			         * Else that tag put us in a new subdoc on the same
			         * level.  It needs to be saved one level up,
			         * if the parent is also a subdoc.
			         */
			        else if ((up_state != NULL)&&
				        (tmp_state == up_state->sub_state)&&
				        (tmp_state != state))
			        {
				        if ((up_state->is_a_subdoc == SUBDOC_CELL)||
				             (up_state->is_a_subdoc == SUBDOC_CAPTION))
				        {
				            lo_SaveSubdocTags(context, up_state, tag);
				        }
				        else
				        {
				            PA_FreeTag(tag);
				        }
			        }
			        /*
			         * Else we are still in the same subdoc
			         */
			        else if (tmp_state == state)
			        {
				        lo_SaveSubdocTags(context, state, tag);
			        }
			        /*
			         * Else that tag started a new, nested subdoc.
			         * Add the starting tag to the parent.
			         */
			        else if (tmp_state == state->sub_state)
			        {
				        lo_SaveSubdocTags(context, state, tag);
				        /*
				         * Since we have extended the parent chain,
				         * we need to reset the child to the new
				         * parent end-chain.
				         */
				        if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				            (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				        {
					        tmp_state->subdoc_tags =
						        state->subdoc_tags_end;
				        }
			        }
			        /*
			         * This can never happen.
			         */
			        else
			        {
				        PA_FreeTag(tag);
			        }

			        state = tmp_state;
		        }
		        else
		        {
			        PA_FreeTag(tag);
		        }
	        }
		}
/*		pNextTag = pTag->next;
		PA_FreeTag(pTag);
*/		if( pNextTag == 0 ){
			bFoundBreak = EDT_TagCursorAtBreak( pRd->pTagCursor, &bBreakIsEndTag );
		}
	}

	if( bFoundBreak )
	{
		if( bBreakIsEndTag ){
			pTag = EDT_TagCursorGetNext(pRd->pTagCursor);
			lo_LayoutTag(pRd->context, pRd->new_state, pTag);
			iEndLine = EDT_TagCursorCurrentLine( pRd->pTagCursor );
		}
		else {
			iEndLine = EDT_TagCursorCurrentLine( pRd->pTagCursor );
			pTag = EDT_TagCursorGetNext(pRd->pTagCursor);
            if ( pTag->type == P_TABLE ) {
                lo_CloseOutLayout( pRd->context, pRd->new_state);
            }
            else {
			    lo_LayoutTag(pRd->context, pRd->new_state, pTag);
            }
		}
        EDT_DeleteTagChain(pTag);

		/* don't close layout.  We just flushed this line to the proper
		 *  height, there is a start of a new tag in the buffer.
		*/
	}
	else {
		lo_CloseOutLayout( pRd->context, pRd->new_state);
	}

	
    if( iEndLine == -1 ){
		iEndLine = pRd->old_state->line_num-1;
	}


    /* Go up the liststack, closing out any mquotes. */
    {
        lo_ListStack *lptr;
        lptr = pRd->new_state->list_stack;
        while (lptr->type != P_UNKNOWN && lptr->next != NULL)
        {
            if (lptr->quote_type == QUOTE_MQUOTE)
            {
                lo_add_leading_bullets(context,pRd->new_state,
                                       lptr->mquote_line_num - 1,
                                       pRd->new_state->line_num - 2,
                                       lptr->mquote_x);
            }
                    
            lptr = lptr->next;
        }
    }

#ifdef DEBUG_shannon
	XP_TRACE(("\n\nELEMENTS TO MERGE - old relayout"));
	lo_VerifyStateLayoutImplementation( context, pRd->top_state, pRd->new_state, TRUE);
#endif

	lo_MergeState( context, pRd->old_state, iStartLine, iEndLine,
				pRd->new_state, &changedY, &changedHeight );

	/* top_state->doc_state = new_state; */
	/* LTNOTE:need to destroy the new state		*/

	/* kill the current state */
	/*
	FE_ClearView( context, FE_VIEW );
	lo_RefreshDocumentArea( context, state, 0, 0,new_state->win_width, 
			new_state->win_height);
	*/

    /* Free the layout */
    lo_FreeRelayoutData(context, pRd);
    
    context->is_editor &= ~EDT_RELAYOUT_FLAG;
	FE_DocumentChanged( context, changedY, changedHeight );
}

PRIVATE
Bool lo_MovePosition( MWContext *pContext, 
			LO_Element *pElement, intn iOffset, 
			ED_Element **ppEdElement, intn* pOffset, Bool bForward)
{
    int32 iOffset32 = iOffset; /* Really should use a sythetic type for offsets. */
    Bool bResult = LO_ComputeNewPosition( pContext, LO_NA_CHARACTER,
        FALSE, FALSE, bForward, &pElement, &iOffset32);

    iOffset = (intn) iOffset32;

    if ( bResult ) {
		*ppEdElement = pElement->lo_any.edit_element;
		*pOffset = pElement->lo_any.edit_offset+iOffset;
	}
    return bResult;
}

Bool LO_PreviousPosition( MWContext *pContext, 
			LO_Element *pElement, intn iOffset, 
			ED_Element **ppEdElement, intn* pOffset)
{
    return lo_MovePosition(pContext, pElement, iOffset,
        ppEdElement, pOffset, FALSE);
}

Bool LO_NextPosition( MWContext *pContext, 
			LO_Element *pElement, intn iOffset, 
			ED_Element **ppEdElement, intn* pOffset)
{
    return lo_MovePosition( pContext, pElement, iOffset,
        ppEdElement, pOffset, TRUE);
}

/*
 * Find the first text element on a line.
 */
LO_Element* LO_FirstElementOnLine( MWContext *pContext, int32 x, int32 y, 
		int32* pLineNum )
{
	lo_TopState* top_state;
	lo_DocState *state;
	int32 iLine;
	LO_Element **line_array, *pElement;
	Bool bFound = FALSE;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(pContext));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return 0;
	}
	state = top_state->doc_state;


	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);

	/* find the line we are currently on */
	iLine = lo_PointToLine( pContext, state, x, y);
	if( pLineNum ){
		*pLineNum = iLine;
	}
	pElement = line_array[iLine];
	while( !bFound )
	{
		/* if we've found a text element.  We've for sure, found the line. */
		if( lo_EditableElement( pElement->type ) && pElement->lo_any.edit_element != 0 ){
			bFound = TRUE;
		}
		else {
			pElement = pElement->lo_any.next;
		}
	}

	XP_UNLOCK_BLOCK(state->line_array);
	return pElement;
}

/*
 * Find the first text element on a line.
 */
PRIVATE
LO_Element* LO_LastElementOnLine( MWContext *pContext, int32 x, int32 y, 
		int32* pLineNum )
{
	lo_TopState* top_state;
	lo_DocState *state;
	int32 iLine;
	LO_Element **line_array, *pElement, *pFoundElement, *pEnd;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(pContext));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return 0;
	}
	state = top_state->doc_state;


	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);

	/* find the line we are currently on */
	iLine = lo_PointToLine( pContext, state, x, y);
	if( pLineNum ){
		*pLineNum = iLine;
	}

	pElement = line_array[iLine];
	pFoundElement = 0;
	if( iLine == state->line_num-2 ){
		pEnd = NULL;
	}
	else {
		pEnd = line_array[iLine+1];
	}

	while( pElement != pEnd )
	{
		/* if we've found a text element.  We've for sure, found the line. */
		if( pElement->type == LO_TEXT ){
			pFoundElement = pElement;
		}
		pElement = pElement->lo_any.next;
	}

	XP_UNLOCK_BLOCK(state->line_array);
	return pFoundElement;
}


LO_Element* LO_BeginOfLine( MWContext *pContext, LO_Element *pElement){
	LO_Element *pFirst = LO_FirstElementOnLine( pContext, pElement->lo_any.x + pElement->lo_any.width / 2,
				pElement->lo_any.y + pElement->lo_any.height / 2, 0 );

	return pFirst;
}

LO_Element* LO_EndOfLine( MWContext *pContext, LO_Element *pElement ){
	LO_Element *pLast = LO_LastElementOnLine( pContext, pElement->lo_any.x,
				pElement->lo_any.y, 0 );
	return pLast;

}

void LO_PositionCaretBounded(MWContext *context, int32 x, int32 y,
			int32 minY, int32 maxY )
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	int32 retX, retY;
	int32 iLine;
	int dir = 0;
	Bool bFound = FALSE;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return;
	}
	state = top_state->doc_state;


	iLine = lo_PointToLine( context, state, x, y );

#define FORWARD 	1
#define BACKWARD	2

	while( iLine >= 0 && iLine < state->line_num -2 && !bFound)
	{
		bFound = lo_FindBestPositionOnLine( context, state, iLine, x, y, dir == FORWARD, &retX, &retY );
		if( bFound && dir != BACKWARD && retY < minY ){
			iLine++;
			dir = FORWARD;
			bFound = FALSE;
		}
		else if ( bFound && dir != FORWARD && retY > maxY ){
			iLine--;
			dir = BACKWARD;
			bFound = FALSE;
		}
	}
	if( bFound ){
		LO_PositionCaret( context, retX, retY, NULL );
	}
}

PRIVATE
void LO_Resize( MWContext *pContext ){
}

void LO_RefetchWindowDimensions( MWContext *pContext ){
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	int32 topX,topY, winWidth, winHeight;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(pContext);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return;
	}
	state = top_state->doc_state;
    FE_GetDocAndWindowPosition( pContext, &topX, &topY, &winWidth, &winHeight );
	state->win_width = winWidth;
	state->win_height = winHeight;
}


#ifdef DEBUG

PRIVATE
Bool
lo_PrintLayoutElement(MWContext *pContext, lo_TopState* top_state,
					  lo_DocState* state, LO_Element *eptr, int32 index);


Bool
lo_VerifyList( MWContext *pContext, lo_TopState* top_state,
              lo_DocState* state,
              LO_Element* start,
              LO_Element* end,
              LO_Element* floating,
              Bool print)
{
	Bool result = TRUE;
    LO_Element *eptr;
    LO_Element *prev;
    int32 index;
    int32 elementID;
    prev = NULL;
    index = 0;
    elementID = -1;

    for ( eptr = start; eptr; eptr = eptr->lo_any.next )
    {
        if ( print )
        {
			result = lo_PrintLayoutElement(pContext, top_state, state, eptr, index);        
	    }
        /*
         * Check next/prev pointer consistency
         */
        if ( eptr->lo_any.prev != prev )
        {
            XP_TRACE(("element %ld at address 0x%08x has a bad prev pointer.",
                index, eptr));
            XP_TRACE((" prev is 0x%08x , should be 0x%08x.",
                eptr->lo_any.prev, prev));
            result = FALSE;
        }

        /*
         * Does this element have a valid type?
         */
        if ( eptr->lo_any.type < 0 || eptr->lo_any.type > LO_SPACER ) {
            XP_TRACE(("element %ld at address 0x%08x has an unknown type %d.",
                index, eptr, eptr->lo_any.type));
            result = FALSE;
        }
        /*
         * Does this element have a valid element id?
         */
        if ( eptr->lo_any.ele_id <= elementID ) {
			XP_TRACE(("element %ld at address 0x%08x has an invalid ele_id %ld.",
                index, eptr, eptr->lo_any.ele_id));
			/* Exit because the pointers may be invalid. */
			return FALSE;
			
			/* Keep going for now, till you can because element ids might be
			   getting corrupted by relayout even though pointers are valid */
			/* result = FALSE; */
        }
        elementID = eptr->lo_any.ele_id;

		/*
		 *  Verify the edit element if it exists.
		 */
		if ( eptr->lo_any.edit_element != NULL ) 
		{
			/*EDT_VerifyLayoutElement( pContext, eptr, print );*/
		}
        /*
         * Update our loop variables
         */
        prev = eptr;
        index++;
    }
    

	/* If we just verified the float list, don't check for end because
	   end for float list is not stored in doc state */
	if (state->float_list != start)
	{
		/*
		 * Check that the last "next" pointer points to the end of the document.
		 */
		if ( prev != end ) {
			XP_TRACE(("end 0x%08x is not the same as the last linked-list element 0x%08x",
				end, prev));
			result = FALSE;
		}
	}

	/* Verify floating element list */
	if (floating != NULL)
	{
		lo_VerifyList(pContext, top_state, state, floating, NULL, NULL, print);
	}

    return result;
}


PRIVATE
Bool
lo_PrintLayoutElement(MWContext *pContext, lo_TopState* top_state,
					  lo_DocState* state, LO_Element *eptr, int32 index)
{
	int16 type;
	Bool result = TRUE;
    const char* typeString;
    static const char* kTypeStrings[] = {"LO_UNKNOWN",
        "LO_NONE",
        "LO_TEXT",
        "LO_LINEFEED",
        "LO_HRULE",
        "LO_IMAGE",
        "LO_BULLET",
        "LO_FORM_ELE",
        "LO_SUBDOC",
        "LO_TABLE",
        "LO_CELL",
        "LO_EMBED",
        "LO_EDGE",
        "LO_JAVA",
		"LO_SCRIPT",
		"LO_OBJECT",
		"LO_PARAGRAPH",
		"LO_CENTER",
		"LO_MULTICOLUMN",
		"LO_FLOAT",
		"LO_TEXTBLOCK",
		"LO_LIST",
		"LO_DESCTITLE",
		"LO_DESCTEXT",
		"LO_BLOCKQUOTE",
		"LO_LAYER",
		"LO_HEADING",
		"LO_SPAN",
		"LO_DIV",
		"LO_SPACER"
	};
    type = eptr->lo_any.type;
    if ( type < LO_UNKNOWN || type > LO_SPACER ) typeString  = "Illegal type";
    else typeString = kTypeStrings[type + 1];

    XP_TRACE(("[%d] 0x%08x type %s(%d) ele_id %d",
        index, eptr, typeString, type, eptr->lo_any.ele_id));
    XP_TRACE(("\tposition %d,%d size %d, %d offset %d, %d, lineheight %d",
        eptr->lo_any.x,
        eptr->lo_any.y,
        eptr->lo_any.width,
        eptr->lo_any.height,
        eptr->lo_any.x_offset,
        eptr->lo_any.y_offset,
		eptr->lo_any.line_height));
#ifdef EDITOR
    XP_TRACE(("\tedit_element 0x%08x edit_offset %d",
        eptr->lo_any.edit_element,
        eptr->lo_any.edit_offset));
#endif
    switch ( type )
    {
    case LO_TEXT:
        {
	        char* text;
			char saveChar;

	        XP_LOCK_BLOCK(text, char *, eptr->lo_text.text);
			saveChar = text[ eptr->lo_text.text_len ];
			text[ eptr->lo_text.text_len ] = '\0';
            XP_TRACE(("\ttext \"%s\"", text));
			text[ eptr->lo_text.text_len ] = saveChar;
			XP_TRACE(("block offset: %d, doc_width: %d, attr mask: %d",
				eptr->lo_text.block_offset,
				eptr->lo_text.doc_width,
				eptr->lo_text.ele_attrmask));
            XP_UNLOCK_BLOCK(eptr->lo_text.text);
        }
        break;
	case LO_LINEFEED:
		{
            XP_TRACE(("\tbreak_type: %d, baseline: %d, attr mask: %d", 
                eptr->lo_linefeed.break_type,
				eptr->lo_linefeed.baseline,
				eptr->lo_linefeed.ele_attrmask));
		}
		break;
	case LO_HRULE:
		{
			XP_TRACE(("\tend_x: %d, end_y: %d, attr mask: %d, alignment: %d, thickness: %d, %% width: %d", 
                eptr->lo_hrule.end_x,
				eptr->lo_hrule.end_y,
				eptr->lo_hrule.ele_attrmask,
				eptr->lo_hrule.alignment,
				eptr->lo_hrule.thickness,
				eptr->lo_hrule.percent_width));
		}
		break;
	case LO_IMAGE:
		{
			XP_TRACE(("\tlayer_id: %d, layer-x: %d, layer-y: %d, %% width: %d, %% height: %d, border(width: %d, vert_space: %d, horiz_space: %d)", 
                eptr->lo_image.layer_id,
                CL_GetLayerXOrigin(eptr->lo_image.layer),
                CL_GetLayerYOrigin(eptr->lo_image.layer),
				eptr->lo_image.percent_width,
				eptr->lo_image.percent_height,
				eptr->lo_image.border_width,
				eptr->lo_image.border_vert_space,
				eptr->lo_image.border_horiz_space));

			XP_TRACE(("\timage_url: %s, attr mask: %d, alt: %s, seq_num: %d",
				eptr->lo_image.image_url,
				eptr->lo_image.ele_attrmask,
				eptr->lo_image.alt,
				eptr->lo_image.seq_num));
		}
		break;
	case LO_BULLET:
		{
			XP_TRACE(("\tbullet(type: %d, size: %d, attr mask: %d, level: %d)",
				eptr->lo_bullet.bullet_type,
				eptr->lo_bullet.bullet_size,
				eptr->lo_bullet.ele_attrmask,
				eptr->lo_bullet.level));
		}
		break;
    case LO_FORM_ELE:
        {
            const struct LO_FormElementStruct_struct* form;
            static const char* kFormTypeNames[] = {
                  "FORM_TYPE_NONE",
                  "FORM_TYPE_TEXT",
                  "FORM_TYPE_RADIO",
                  "FORM_TYPE_CHECKBOX",
                  "FORM_TYPE_HIDDEN",
                  "FORM_TYPE_SUBMIT",
                  "FORM_TYPE_RESET",
                  "FORM_TYPE_PASSWORD",
                  "FORM_TYPE_BUTTON",
                  "FORM_TYPE_JOT",
                  "FORM_TYPE_SELECT_ONE",
                  "FORM_TYPE_SELECT_MULT",
                  "FORM_TYPE_TEXTAREA",
                  "FORM_TYPE_ISINDEX",
                  "FORM_TYPE_IMAGE",
                  "FORM_TYPE_FILE",
                  "FORM_TYPE_KEYGEN",
                  "FORM_TYPE_READONLY",
				  "FORM_TYPE_OBJECT"
                  };
            const char* formTypeName;
            int16 form_id;
            form = & eptr->lo_form;
            form_id = form->form_id;
            formTypeName = "Unknown";
            if ( form_id >= FORM_TYPE_NONE && form_id <= FORM_TYPE_OBJECT ) {
                formTypeName = kFormTypeNames[form_id];
            }

            XP_TRACE(("\tform_id %s(%d)", formTypeName, form_id));
            XP_TRACE(("\telement_index: %d, baseline: %d, sel(%d,%d), attr mask: %d, form_id: %d, border(horiz_space: %d, vert_space: %d)",
                form->element_index, form->baseline, form->sel_start,
                form->sel_end, form->ele_attrmask, form->form_id, 
				form->border_vert_space, form->border_horiz_space));
        }
		break;

	case LO_TABLE:
		{
			if (eptr->lo_table.table == NULL) 
			{
				XP_TRACE(("\tERROR: Table element contains a null pointer to its table state"));
				result = FALSE;
			}
			else
			{
				XP_TRACE(("\tlo_TableRec *: %p", eptr->lo_table.table));
			}
		}
		break;
    case LO_CELL:
        {
            const LO_CellStruct* cell
                = & eptr->lo_cell;
			XP_TRACE(("------------- Beginning of cell (ele_id = %d) contents --------------------",
				cell->ele_id));

			if (cell->table == NULL || cell->table_row == NULL || cell->table_cell == NULL)
			{
				XP_TRACE(("\tERROR: Cell element contains a null pointer to its table state"));
				result = FALSE;
			}
			XP_TRACE(("lo_TableRec ptr: %p, lo_TableRow ptr: lo_TableCell ptr:",
				cell->table,
				cell->table_row,
				cell->table_cell));
            lo_VerifyList(pContext, top_state, state,
                cell->cell_list,
                cell->cell_list_end,
                cell->cell_float_list, TRUE);
			XP_TRACE(("------------- End of cell (ele_id = %d) contents --------------------",
				cell->ele_id));
        }
		break;

	case LO_PARAGRAPH:				
		{
			XP_TRACE(("\tis_end: %d, blank_lines: %d, implicit_end: %d, align_set: %d, align: %d",
				eptr->lo_paragraph.is_end,
				eptr->lo_paragraph.blank_lines,
				eptr->lo_paragraph.implicit_end,
				eptr->lo_paragraph.alignment_set,
				eptr->lo_paragraph.alignment));
		}
		break;				

	case LO_CENTER:
		{
			XP_TRACE(("\tis_end: %d",
				eptr->lo_center.is_end));
		}
		break;

	case LO_MULTICOLUMN:
		{
			XP_TRACE(("\tis_end: %d",
				eptr->lo_multicolumn.is_end));
		}
		break;

	case LO_FLOAT:
		{
			XP_TRACE(("\tPoints to %s(%d), ele_id: %d",
				kTypeStrings[eptr->lo_float.float_ele->lo_any.type + 1],
				eptr->lo_float.float_ele->lo_any.type,
				eptr->lo_float.float_ele->lo_any.ele_id));
		}
		break;

   	case LO_TEXTBLOCK:
   		{
            const LO_TextBlock* block = & eptr->lo_textBlock;
            XP_TRACE(("      buffer_length %lu", block->buffer_length));
            XP_TRACE(("      buffer_write_index %lu", block->buffer_write_index));
            XP_TRACE(("      break_table %lx", block->break_table));
            XP_TRACE(("      startTextElement %lx", block->startTextElement));
            XP_TRACE(("      endTextElement %lx", block->endTextElement));
   		}

	case LO_LIST:
		{
			XP_TRACE(("\tbullet(type: %d, start: %d), quote_type: %d, compact: %d, is_end: %d",
				eptr->lo_list.bullet_type,
				eptr->lo_list.bullet_start,
				eptr->lo_list.quote_type,
				eptr->lo_list.compact,
				eptr->lo_list.is_end));
		}
		break;
	case LO_BLOCKQUOTE:
		{
			XP_TRACE(("\tblockquote : end: %s, quote_type: %d",
					  eptr->lo_blockquote.is_end ? "True" : "False",
					  eptr->lo_blockquote.quote_type));
		}
		break;
    default:
        break;
    }

	return result;
}


PRIVATE
Bool
lo_VerifyStateLayoutImplementation( MWContext *pContext, lo_TopState *top_state, lo_DocState *state, Bool print) {
    /* Debugging aid. Checks consistency of the layout for this context.
     * returns TRUE if the layout is valid.
     * Prints information to stderr if the layout is invalid.
     */
 	Bool result;
	int32 doc_id;
	LO_Element **array;
	LO_Element *eptr;
    LO_Element *start;
    LO_Element *end;
#ifdef XP_WIN16
	XP_Block *larray_array;
#endif /* XP_WIN16 */

    result = TRUE;
    if ( ! pContext ) {
        XP_TRACE(("context is NULL."));
        return FALSE;
    }

	/*
	 * Get first element in doc.
	 */
#ifdef XP_WIN16
	XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
	state->line_array = larray_array[0];
	XP_UNLOCK_BLOCK(state->larray_array);
#endif /* XP_WIN16 */

	XP_LOCK_BLOCK(array, LO_Element **, state->line_array);
	eptr = array[0];
	XP_UNLOCK_BLOCK(state->line_array);

	if (eptr == NULL)
	{
		XP_TRACE(("No first element."));
		return FALSE;
	}

	start = eptr;

	/*
	 * Get last element in doc.
	 */
	end = state->end_last_line;

	if (end == NULL)
	{
		XP_TRACE(("No last element."));
		return FALSE;
	}

    return lo_VerifyList(pContext, top_state, state, start, end, state->float_list, print);
}

PRIVATE
Bool
lo_VerifyLayoutImplementation( MWContext *pContext, Bool print) {
    /* Debugging aid. Checks consistency of the layout for this context.
     * returns TRUE if the layout is valid.
     * Prints information to stderr if the layout is invalid.
     */
 	Bool result;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element **array;
	LO_Element *eptr;
    LO_Element *start;
    LO_Element *end;
#ifdef XP_WIN16
	XP_Block *larray_array;
#endif /* XP_WIN16 */

    result = TRUE;
    if ( ! pContext ) {
        XP_TRACE(("context is NULL."));
        return FALSE;
    }
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(pContext);
	top_state = lo_FetchTopState(doc_id);
	if (top_state == NULL)
	{
        XP_TRACE(("top_state is NULL."));
		return FALSE;
	}
	state = top_state->doc_state;
	if (state == NULL)
	{
        XP_TRACE(("state is NULL."));
		return FALSE;
	}

    return lo_VerifyStateLayoutImplementation(pContext, top_state, state, print);
}

Bool
lo_VerifyLayout( MWContext *pContext) {
    return lo_VerifyLayoutImplementation(pContext, FALSE);
}

void
lo_PrintLayout( MWContext *pContext) {
    lo_VerifyLayoutImplementation(pContext, TRUE);
}
#endif /* DEBUG */

#endif /* editor */

void LO_SetBaseURL( MWContext *context, char *pURL ){
	int32 doc_id;
	lo_TopState *top_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return;
	}
	XP_FREE( top_state->base_url );
	top_state->base_url = XP_STRDUP( pURL );
}

char* LO_GetBaseURL( MWContext *context ){
	int32 doc_id;
	lo_TopState *top_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return 0;
	}
	return top_state->base_url;
}

