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


#include "xp.h"
#include "layout.h"
#include "laylayer.h"
#include "libi18n.h"
#include "edt.h"
#include "layers.h"
#include <ctype.h> /* For lo_CharacterClassOf */

#ifndef XP_TRACE
# define XP_TRACE(X) fprintf X
#endif

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

#ifdef XP_WIN16
#define SIZE_LIMIT              32000
#endif /* XP_WIN16 */

#ifdef PROFILE
#pragma profile on
#endif

#ifdef DEBUG
void lo_ValidatePosition(MWContext *context, LO_Position* position);
void lo_ValidateSelection(MWContext *context, LO_Selection* selection);
void lo_ValidatePosition2(MWContext *context, LO_Element* element, int32 position);
void lo_ValidateSelection2(MWContext *context, LO_Element* elementB, int32 positionB, LO_Element* elementE, int32 positionE);

#define LO_ASSERT_POSITION(context, p) lo_ValidatePosition(context, p)
#define LO_ASSERT_SELECTION(context, s) lo_ValidateSelection(context, s)
#define LO_ASSERT_POSITION2(context, e, p) lo_ValidatePosition2(context, e, p)
#define LO_ASSERT_SELECTION2(context, eb, pb, ee, pe) lo_ValidateSelection2(context, eb, pb, ee, pe)

#else

#define LO_ASSERT_POSITION(context, p) {}
#define LO_ASSERT_SELECTION(context, s) {}
#define LO_ASSERT_POSITION2(context, e, p) {}
#define LO_ASSERT_SELECTION2(context, eb, pb, ee, pe) {}

#endif

static void
lo_bump_position(MWContext *context, lo_DocState *state,
		LO_Element *sel, int32 pos,
		LO_Element **new_sel, int32 *new_pos, Bool forward);

/*
 * Utility functions to convert between insert point and selection end.
 * Returns FALSE if the conversion can't be done.
 * (Only happens when the insert point is off the beginning of the document, or the
 * selection end is off the end of the document.) In those cases, the input position is
 * returned unchanged.
 *
 */

Bool lo_ConvertInsertPointToSelectionEnd(MWContext* context, lo_DocState * state,
    LO_Element** element, int32* position);
Bool lo_ConvertSelectionEndToInsertPoint(MWContext* context, lo_DocState * state,
    LO_Element** element, int32* position);

void lo_HighlightSelect(MWContext *context, lo_DocState *state,
	LO_Element *start, int32 start_pos, LO_Element *end, int32 end_pos,
	Bool on);
void lo_SetSelect(MWContext *context, lo_DocState *state,
	LO_Element *start, int32 start_pos, LO_Element *end, int32 end_pos,
	Bool on);
void
lo_StartNewSelection(MWContext *context, lo_DocState * state,
    LO_Element* eptr, int32 position);

void
lo_ExtendSelectionToPosition2(MWContext *context, lo_TopState* top_state, lo_DocState *state, LO_Element* eptr, int32 position);

/* Makes sure *eptr points to an editable element. If *eptr doesn't, searches towards
 * the end of the document.
 * If no editiable element is found, the function
 * returns FALSE.
 */
Bool
lo_EnsureEditableSearchNext(MWContext *context, lo_DocState *state, LO_Element** eptr);

Bool
lo_EnsureEditableSearchNext2(MWContext *context, lo_DocState *state, LO_Element** eptr, int32* ePositionPtr);

/* Same as lo_EnsureEditableSearchNext, except searches towards the start of document.
 */

Bool
lo_EnsureEditableSearchPrev(MWContext *context, lo_DocState *state, LO_Element** eptr);

Bool
lo_EnsureEditableSearchPrev2(MWContext *context, lo_DocState *state, LO_Element** eptr, int32* ePositionPtr);

Bool
lo_EnsureEditableSearch(MWContext *context, lo_DocState* state, LO_Position* p, Bool forward);

Bool
lo_EnsureEditableSearch2(MWContext *context, lo_DocState* state, LO_Element** eptr, int32* ePositionPtr, Bool forward);

Bool
lo_IsValidEditableInsertPoint2(MWContext *context, lo_DocState* state, LO_Element* eptr, int32 ePositionPtr);


/* Normalizes the selection, if editing is turned on. Returns TRUE if the selection
 * is non empty.
 */

Bool lo_NormalizeSelection(MWContext *context);

/*
 * Return FALSE if the selection point can't be normalized.
 * (i.e. if it's at the end of the document.)
 */

Bool
lo_NormalizeSelectionPoint(MWContext *context, lo_DocState *state, LO_Element** pEptr, int32* pPosition);

void
lo_NormalizeSelectionEnd(MWContext *context, lo_DocState *state, LO_Element** pEptr, int32* pPosition);

int32
lo_GetTextAttrMask(LO_Element* eptr);

LO_AnchorData*
lo_GetAnchorData(LO_Element* eptr);

LO_Element*
lo_GetNeighbor(LO_Element* element, Bool forward);

int32
lo_GetElementEdge(LO_Element* element, Bool forward);

int32
lo_GetMaximumInsertPointPosition(LO_Element* eptr);

int32
lo_GetLastCharEndPosition(LO_Element* eptr);

int32
lo_GetLastCharBeginPosition(LO_Element* eptr);

int32
lo_IncrementPosition(LO_Element* eptr, int32 position);

int32
lo_DecrementPosition(LO_Element* eptr, int32 position);

Bool
lo_IsEndOfParagraph2(MWContext* context, LO_Element* element, int32 position);

Bool
lo_FindDocumentEdge(MWContext* context, lo_DocState *state, LO_Position* edge, Bool select, Bool forward);

Bool
lo_IsEdgeOfDocument2(MWContext* context, lo_DocState *state, LO_Element* element, int32 position, Bool forward);

Bool
lo_IsEdgeOfDocument(MWContext* context, lo_DocState *state, LO_Position* where, Bool forward);

void
lo_SetInsertPoint(MWContext *context, lo_TopState *top_state, LO_Element* eptr, int32 position, CL_Layer *layer);

/*
 * Implements the UI policy for a click on an element. Returns TRUE if the result is an insertion point.
 */

Bool lo_ProcessClick(MWContext *context, lo_TopState *top_state, lo_DocState *state, LO_HitResult* result, Bool requireCaret, CL_Layer *layer);
Bool lo_ProcessDoubleClick(MWContext *context, lo_TopState *top_state, lo_DocState *state, LO_HitResult* result, CL_Layer *layer);
Bool lo_ProcessAnchorClick(MWContext *context, lo_TopState *top_state, lo_DocState *state, LO_HitResult* result);

Bool
lo_SelectAnchor(MWContext* context, lo_DocState *state, LO_Element* eptr);

void
lo_SetSelection(MWContext *context, LO_Selection* selection, Bool extendingStart);

void
lo_FullSetSelection(MWContext *context, lo_DocState * state,
    LO_Element* start, int32 start_pos,
    LO_Element* end, int32 end_pos, Bool extendStart);

/* Get various selection parts, expressed as insert points. */

void
lo_GetAnchorPoint(MWContext *context, lo_DocState * state, LO_Element** pElement, int32* pPosition);
void
lo_GetExtensionPoint(MWContext *context, lo_DocState * state, LO_Element** pElement, int32* pPosition);

Bool lo_FindBestPositionInTable(MWContext *context, lo_DocState* state, LO_TableStruct* pTable, int32 iDesiredX, 
			int32 iDesiredY, Bool bForward, int32* pRetX, int32 *pRetY );
Bool lo_FindClosestUpDown_Cell(MWContext *pContext, lo_DocState* state, 
                               LO_CellStruct* cell, int32 x, int32 y,
                          Bool bForward, int32* ret_x, int32* ret_y);
LO_Element *
lo_search_element_list_WideMatch(MWContext *context, LO_Element *eptr, LO_Element* eEndPtr, int32 x, int32 y,
                                 Bool bForward);

Bool lo_EditableElement( int iType )
{
	if( iType == LO_TEXT
		|| iType == LO_TEXTBLOCK 
		|| iType == LO_IMAGE 
		|| iType == LO_HRULE
		|| iType == LO_LINEFEED
		|| iType == LO_FORM_ELE )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

PRIVATE
int32
lo_ElementToCharOffset2(MWContext *context, lo_DocState *state,
	LO_Element *element, int32 x, int32* returnCharStart, int32* returnCharEnd)
{

	LO_TextStruct *text_ele;
	LO_TextInfo text_info;
	int32 orig_len;
	int32 xpos;
	int32 cpos;
	int32 start, end;
	int16 charset;
    int32 startX;

	if ((element == NULL)||(element->type != LO_TEXT))
	{
		return(-1);
	}

	text_ele = (LO_TextStruct *)element;
	if ((text_ele->text == NULL)||(text_ele->text_len <= 0))
	{
		return -1;
	}

	startX = text_ele->x + text_ele->x_offset;
	xpos = x - startX;
	if (xpos < 0)
	{
		return -1;
	}

	if (xpos >= text_ele->width && returnCharStart == NULL)
	{
		return lo_GetLastCharEndPosition(element);
	}

	start = 0;
	end = lo_GetLastCharEndPosition(element);

    /* Make a guess as to where to start searching, assuming characters are
     * all roughly the same width.
     */

    if ( text_ele->width > 0 )
    {
	    cpos = xpos * text_ele->text_len / text_ele->width;
    }
    else
    {
        cpos = 0;
    }
	if (cpos > end)
	{
		cpos = end;
	}
	orig_len = text_ele->text_len;

	charset = text_ele->text_attr->charset;
	if (INTL_CharSetType(charset) == SINGLEBYTE)
	{	
		while (start < end)
		{
            XP_ASSERT(cpos >= 0);
			text_ele->text_len = (intn) cpos + 1;
			FE_GetTextInfo(context, text_ele, &text_info);
			if (xpos > text_info.max_width)
			{
				start = cpos + 1;
			}
			else
			{
				end = cpos;
			}
			cpos = (start + end) / 2;
		}

        /* Find character width by trickery */
        text_ele->text_len = (intn) cpos;
        FE_GetTextInfo(context, text_ele, &text_info);
        if ( returnCharStart != NULL ) *returnCharStart = startX + text_info.max_width;
        text_ele->text_len = (intn) cpos+1;
        FE_GetTextInfo(context, text_ele, &text_info);
        if ( returnCharEnd != NULL ) *returnCharEnd = startX + text_info.max_width;
   	}
	else  
	{	 		/* slow multibyte finding, I feel sad we can't use the
				   beautiful single byte algorithm above
				*/
		int32 prev_xpos, last_xpos, last_pos;
		char *tptr;

		PA_LOCK(tptr, char *, text_ele->text);
		cpos = 0;
		prev_xpos = 0;
		last_xpos = text_ele->width ;
		last_pos = end ;
		while ((start <= end) && (cpos <= end))
		{
			text_ele->text_len = (intn) cpos ;
			FE_GetTextInfo(context, text_ele, &text_info);
			if (xpos > text_info.max_width)
			{
				prev_xpos = text_info.max_width ;
				start = text_ele->text_len;
			}
			else	/* since it goes char by char, finding ends here */
			{
				last_xpos = text_info.max_width ;
				end = cpos;
				break ;
			}
			/* go to next char */
			cpos = start +
				(int32)INTL_CharLen(charset,
				(unsigned char *)(tptr + start));
		}
		cpos = start ;
		if (cpos > last_pos)
			cpos = last_pos ;

		PA_UNLOCK(text_ele->text);

        if ( returnCharStart != NULL ) *returnCharStart = startX + prev_xpos;
        if ( returnCharEnd != NULL) *returnCharEnd = startX + last_xpos;
	}

	text_ele->text_len = (intn) orig_len;

	return(cpos);
}

/*
 * For clients that don't care about the bounds of what they hit.
 */
PRIVATE
int32
lo_ElementToCharOffset(MWContext *context, lo_DocState *state,
	LO_Element *element, int32 x)
{
    return lo_ElementToCharOffset2(context, state, element, x, NULL, NULL);
}

void
LO_StartSelection(MWContext *context, int32 x, int32 y, CL_Layer *layer)
{
#if 0
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *eptr;
	int32 position;
	int32 ret_x, ret_y;

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

	LO_HighlightSelection(context, FALSE);
	state->selection_start = NULL;
	state->selection_start_pos = 0;
	state->selection_end = NULL;
	state->selection_end_pos = 0;

	position = 0;
	eptr = lo_XYToDocumentElement(context, state, x, y, FALSE, TRUE, TRUE,
		&ret_x, &ret_y);
	if (eptr != NULL)
	{
		position = lo_ElementToCharOffset(context, state, eptr, ret_x);
		if (position < 0)
		{
			position = 0;
		}
	}

	state->selection_new = eptr;
	state->selection_new_pos = position;
#endif

    LO_Click(context, x, y, ! EDT_IS_EDITOR(context),
             layer); /* Only allow selections in editor */
}

/*
 * Used for right-clicks.
 */
void LO_SelectObject( MWContext *context, int32 x, int32 y, CL_Layer *layer) 
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
    LO_HitResult result;

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

    LO_Hit(context, x, y, FALSE, &result, layer);

    if ( ! lo_ProcessAnchorClick(context, top_state, state, &result ) ) {
        lo_ProcessClick(context, top_state, state, &result, FALSE, layer );
    }
}


void LO_StartSelectionFromElement( MWContext *context, LO_Element *eptr, int32 new_pos, CL_Layer *layer )
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

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

    if ( eptr ) {
        if ( ! lo_EnsureEditableSearchPrev2(context, state, &eptr, &new_pos) )
        {
            XP_ASSERT(FALSE);
            return;
        }
        LO_ASSERT_POSITION2(context, eptr, new_pos);
    }

	LO_HighlightSelection(context, FALSE);

	state->selection_start = NULL;
	state->selection_start_pos = 0;
	state->selection_end = NULL;
	state->selection_end_pos = 0;

	state->selection_new = eptr;
	state->selection_new_pos = new_pos;
    state->selection_layer = layer;
}

#ifdef DEBUG

PRIVATE
void
LO_DUMP_SELECTIONSTATE(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

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

    XP_TRACE(("selection 0x%08x %d 0x%08x %d new 0x%08x %d extending_start %d\n",
        state->selection_start,
	    state->selection_start_pos,
	    state->selection_end,
	    state->selection_end_pos,
    	state->selection_new,
	    state->selection_new_pos,
        state->extending_start));
}

PRIVATE
void
LO_DUMP_INSERT_POINT(char* s, LO_Element* element, int32 position)
{
    if ( element )
    {
        XP_TRACE(("%s %d:%d.%d ", s, element->type, element->lo_any.ele_id, position));
    }
    else
    {
        XP_TRACE(("%s - NIL", s));
    }
}

PRIVATE
void
LO_DUMP_POSITION(char* s, LO_Position* position)
{
    LO_DUMP_INSERT_POINT(s, position->element, position->position);
}

PRIVATE
void
LO_DUMP_SELECTION2(char* s, LO_Element* a, int32 ap, LO_Element* b, int32 bp)
{
/*    const char* kElementCodes = "?tlhi?????????????????";*/
    XP_TRACE(("%s %d:%d.%d %d:%d.%d ", s,
        a->type, a->lo_any.ele_id, ap,
        b->type, b->lo_any.ele_id, bp));
}

PRIVATE
void
LO_DUMP_SELECTION(char* s, LO_Selection* selection)
{
    LO_DUMP_SELECTION2(s, selection->begin.element, selection->begin.position,
        selection->end.element, selection->end.position);
}

PRIVATE
void
LO_DUMP_HIT_RESULT(LO_HitResult* result)
{
    switch ( result->type )
    {
    case LO_HIT_UNKNOWN:
        XP_TRACE(("Unknown"));
        break;
    case LO_HIT_LINE:
        {
            char* s;
            switch ( result->lo_hitLine.region )
            {
            case LO_HIT_LINE_REGION_BEFORE:
                {
                    s = "before line";
                }
                break;
            case LO_HIT_LINE_REGION_AFTER:
                {
                    s = "after line";
                }
                break;
            default:
                    s = "LO_HIT_LINE Bad region";
                break;
            }
            LO_DUMP_SELECTION(s, & result->lo_hitLine.selection);
        }
        break;
    case LO_HIT_ELEMENT:
        {
            char* s;
            switch ( result->lo_hitElement.region )
            {
            case LO_HIT_ELEMENT_REGION_BEFORE:
                {
                   s = "before element";
                }
                break;
            case LO_HIT_ELEMENT_REGION_MIDDLE:
                {
                   s = "middle element";
                }
                break;
            case LO_HIT_ELEMENT_REGION_AFTER:
                {
                    s = "after element";
                }
                break;
            default:
                {
                    s = "LO_HIT_ELEMENT unknown region";
                }
                break;
            }
            LO_DUMP_POSITION(s, & result->lo_hitElement.position);
        }
        break;
    default:
        {
            XP_TRACE(("LO_HIT unknown result "));
        }
        break;
    }
}

#endif

Bool
LO_IsSelected(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}
	state = top_state->doc_state;

	return (state->selection_start != NULL);
}

Bool
LO_IsSelectionStarted(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}
	state = top_state->doc_state;

	return (state->selection_new != NULL);
}

PRIVATE
void lo_JiggleToMakeEditable( MWContext *context, 
                            lo_DocState *state,
                            LO_Element** ppElement, 
        					int32 *pPosition,
							Bool bForward)
{
   /* If it's not an editable element, move to find one that is.
     * We don't call the ensure routines because we don't want to
     * select end-of-paragraph linefeeds here.
     *
     */
    LO_Element* element = *ppElement;
    int32 position = *pPosition;

    if ( (! EDT_IS_EDITOR(context)) || (element && element->lo_any.edit_element) ) {
        return;
    }

    while( element && ! element->lo_any.edit_element ){
        LO_Element* newElement;
        int32 newPosition;
        lo_bump_position(context, state, element, position, &newElement, &newPosition, bForward);
        if ( element == newElement && position == newPosition ) {
            /* We got stuck */
            element = NULL;
            break;
        }
        element = newElement;
        position = newPosition;
    }
    if ( ! element ) {
        /* Ran off end of document */
        element = *ppElement;
        position = *pPosition;
        while( element && ! element->lo_any.edit_element ){
            LO_Element* newElement;
            int32 newPosition;
            lo_bump_position(context, state, element, position, &newElement, &newPosition, !bForward);
            if ( element == newElement && position == newPosition ) {
                /* We got stuck */
                element = NULL;
                break;
        }
        element = newElement;
        position = newPosition;
        }
    }
    if ( element ) {
        *ppElement = element;
        *pPosition = lo_GetElementEdge(element, !bForward);
    }
}

/* This is only called from the editor. As a convienience to the
 * editor, we return the selection as a 1/2 open selection. That
 * means we convert the selection end into an insert point.
 */

void LO_GetSelectionEndPoints( MWContext *context, 
							LO_Element** ppStart, 
        					intn *pStartOffset,
							LO_Element** ppEnd, 
        					intn *pEndOffset,
        					Bool* pbFromStart,
        					Bool* pbSingleItemSelection )
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
    LO_Element* startElement;
    int32 startPosition;
    LO_Element* endElement;
    int32 endPosition;
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

    startElement = state->selection_start;
    startPosition = state->selection_start_pos;
    /* If it's not an editable element, move backward to find one that is.
     * We don't call the ensure routines because we don't want to
     * select end-of-paragraph linefeeds here.
     *
     * But now that we have tables, we need to bump.
     */
    lo_JiggleToMakeEditable( context, state, &startElement, &startPosition, FALSE);
	if( ppStart ) *ppStart = startElement;
	if( pStartOffset ) *pStartOffset = startPosition;


    /* Convert the selection end into an insert point. */
    endElement = state->selection_end;
    endPosition = state->selection_end_pos;
    if ( endElement )
    {
        lo_ConvertSelectionEndToInsertPoint(context, state, &endElement, &endPosition);
    }

    /* If it's not an editable element, move forward to find one that is.
     * We don't call the ensure routines because we don't want to
     * select end-of-paragraph linefeeds here.
     */
    lo_JiggleToMakeEditable( context, state, &endElement, &endPosition, TRUE);

	if( ppEnd ) *ppEnd = endElement;
	if( pEndOffset ) *pEndOffset = endPosition;

	if( pbFromStart ) *pbFromStart = state->extending_start;

	/* 
	 * the editor needs to know if the select is just a single thing like a
	 *  image or a HRULE.
	*/
	if( pbSingleItemSelection ) *pbSingleItemSelection = (
			startElement
			&& startElement->type != LO_TEXT
			&& startElement->type != LO_TEXTBLOCK
			&& startElement == endElement );

	/*
	 * For lloyd's edification, if we have a single item seleciton, start_pos 
	 *  is 0 and end_pos is 0 or 1
	*/
	XP_ASSERT( pbSingleItemSelection && *pbSingleItemSelection 
			? 
				startPosition == 0
				&& ( endPosition == 0
					|| endPosition == 1 )
			:	
				TRUE);
}
    
void LO_GetSelectionNewPoint( MWContext *context, 
							LO_Element** ppNew, 
        					intn *pNewOffset)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

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

	if( ppNew ) *ppNew = state->selection_new;
	if( pNewOffset ) *pNewOffset = state->selection_new_pos;
}
 
static intn
lo_compare_selections(LO_Element *sel1, int32 pos1,
	LO_Element *sel2, int32 pos2)
{
	if ((sel1 == NULL)||(sel2 == NULL))
	{
		if (sel1 == sel2)
		{
			return(0);
		}
		else if (sel1 == NULL)
		{
			return(-1);
		}
		else
		{
			return(1);
		}
	}

	if (sel1->lo_any.ele_id < sel2->lo_any.ele_id)
	{
		return(-1);
	}
	else if (sel1->lo_any.ele_id > sel2->lo_any.ele_id)
	{
		return(1);
	}
	else if (sel1->lo_any.ele_id == sel2->lo_any.ele_id)
	{
		if (pos1 < pos2)
		{
			return(-1);
		}
		else if (pos1 > pos2)
		{
			return(1);
		}
		else if (pos1 == pos2)
		{
			return(0);
		}
	}
#ifdef DEBUG
	assert (FALSE);
#endif
	return(-1);
}

PRIVATE
intn
lo_ComparePositions(LO_Position* a, LO_Position* b)
{
    return lo_compare_selections( a->element, a->position, b->element, b->position);
}

PRIVATE
LO_Element*
lo_BoundaryJumpingNext(MWContext *context, lo_DocState *state, LO_Element *eptr)
{
    int32 last_id;
    LO_Element *cell_parent;
    /*
     * If no next element, see if we need to jump the cell wall.
     */
    Bool success=FALSE;
    while(!success)
    {
	    LO_Element* next = eptr->lo_any.next;
        last_id = eptr->lo_any.ele_id;
        cell_parent = NULL;
        while ( eptr && ! next  )
        {
            LO_Element* oldEptr = eptr;
            eptr = lo_JumpCellWall(context, state, eptr);
            if ( ! eptr || eptr == oldEptr )
                break; /* Ran off the end of the document */
            next = eptr->lo_any.next;
        }
	    eptr = next;
	    /*
	     * When we walk onto a cell,
	     * we need to walk into it if
	     * it isn't empty.
	     */
	    if ((eptr != NULL)&&
		    (eptr->type == LO_CELL)&&
		    (eptr->lo_cell.cell_list != NULL))
	    {
		    cell_parent = eptr;
		    eptr = eptr->lo_cell.cell_list;
	    }

	    /*
	     * Heuristic:  For some difficult tables, you may come back
	     * to the same cell you left instead of progressing.
	     * If so, try manually moving the cell forward.
	     */
	    if ((eptr != NULL)&&(eptr->lo_any.ele_id == last_id)&&
		    (cell_parent != NULL))
	    {
		    LO_Element *guess;

		    guess = cell_parent->lo_any.next;
		    /*
		     * If our guessed next element after the parent cell is
		     * a non-empty cell, make the first element in that
		     * cell our new element.
		     */
		    if ((guess != NULL)&&(guess->type == LO_CELL)&&
			    (guess->lo_cell.cell_list != NULL))
		    {
			    eptr = guess->lo_cell.cell_list;
		    }
	    }

        /*
         * We don't want infinite loops.
         * If the element ID hasn't progressed, something
         * serious is wrong, and we should punt.
         */
        if ((eptr != NULL)&&(eptr->lo_any.ele_id <= last_id))
        {
    #ifdef DEBUG
    XP_TRACE(("Cell Jump loop avoidance 1\n"));
    #endif /* DEBUG */
	    return NULL;
        }

        success=TRUE; /* unless we detect a LO_TEXTBLOCK */
        /* check textblock until not textblock leave prev as null if you hit
		   null */
		if (( next && eptr->type == LO_TEXTBLOCK ) || ( next && eptr->type == LO_DESCTITLE ))
		{
            success=FALSE;
		}
    
    }
    return eptr;
}


/*
* We changed celljumping to boundary jumping to include type 19/LO_TEXTBLOCK as a type to skip past. anthonyd
*/

PRIVATE
LO_Element*
lo_BoundaryJumpingPrev(MWContext *context, lo_DocState *state, LO_Element *eptr)
{
    /*
     * If no previous element, see if we need to jump the cell wall.
     */
    LO_Element* prev;
	Bool success=FALSE;
	while(!success)
	{
        prev = eptr->lo_any.prev;
		while ( eptr && ! prev  )
		{
			LO_Element* oldEptr = eptr;
			eptr = lo_JumpCellWall(context, state, eptr);
			if ( ! eptr || eptr == oldEptr )
				break; /* Ran off the front of the document */
			prev = eptr->lo_any.prev;
			if ( ! prev )
				continue; /* Ran off the front of the cell/document */
			if ( prev->type == LO_TABLE )
			{
				/* That was the first cell of the table. */
				eptr = prev;
				prev = eptr->lo_any.prev;
				/* Repeat the check that was made above since we've moved eptr. */
				if ( eptr == oldEptr )
					break; 
			}
		}
		success=TRUE; /* successfull until textblock check */
		/*
		 * When we walk onto a cell,
		 * we need to walk into it if
		 * it isn't empty.
		 */
		if ((prev != NULL)&&
			(prev->type == LO_CELL)&&
			(prev->lo_cell.cell_list_end != NULL))
		{
			prev = prev->lo_cell.cell_list_end;
		}

        eptr = prev;

        /* check textblock until not textblock leave prev as null if you hit
		   null */
		if (( prev && eptr->type == LO_TEXTBLOCK ) || ( prev && eptr->type == LO_DESCTITLE ))
		{
            success=FALSE;
		}
	}
    return eptr;
}



static void
lo_bump_position(MWContext *context, lo_DocState *state,
		LO_Element *sel, int32 pos,
		LO_Element **new_sel, int32 *new_pos, Bool forward)
{
	LO_Element *eptr;
	int32 position;

	eptr = sel;
	position = pos;

	/*
	 * Moving forward one selection position
	 */
	if (forward != FALSE)
	{
		/*
		 * If it is not a text element, just move
		 * to the next element.
		 */
		if (eptr->lo_any.type != LO_TEXT)
		{
			eptr = lo_BoundaryJumpingNext(context, state, eptr);
            if (eptr == NULL)
			{
				eptr = sel;
			}
			/*
			 * Else start at the beginning of the next element
			 */
			else
			{
				position = 0;
			}
		}
		/*
		 * Else this is a text element, so check moving
		 * forward within the element.
		 */
		else
		{
            intn maxPosition = lo_GetLastCharBeginPosition(eptr);
			/*
			 * If we are already at the end of the text
			 * element we are back to moving on to the
			 * next element.
			 */
			if (position < maxPosition)
			{
 			    position = lo_IncrementPosition(eptr, position);
            }
            else
            {
			    eptr = lo_BoundaryJumpingNext(context, state, eptr);
                if (eptr == NULL)
			    {
				    eptr = sel;
			    }
			    /*
			     * Else start at the beginning of the next element
			     */
			    else
			    {
				    position = 0;
			    }
            }
		}
	}
	/*
	 * Else moving backward one selection position
	 */
	else
	{
		/*
		 * If it is not a text element, just move
		 * to the previous element.
		 */
		if (eptr->lo_any.type != LO_TEXT)
		{
			eptr = lo_BoundaryJumpingPrev(context, state, eptr);

			/*
			 * If no previous element, don't move it.
			 */
			if (eptr == NULL)
			{
				eptr = sel;
			}
			/*
			 * Else start at the end of the previous element.
			 * (only matters for text elements)
			 */
			else
			{
                position = lo_GetLastCharBeginPosition(eptr);
			}
		}
		/*
		 * Else this is a text element, so check moving
		 * backward within the element.
		 */
		else
		{
			/*
			 * If we are already at the beginning of the text
			 * element we are back to moving back to the
			 * previous element.
			 */
			if (position == 0)
			{
			    eptr = lo_BoundaryJumpingPrev(context, state, eptr);

				/*
				 * If no previous element, don't move it.
				 */
				if (eptr == NULL)
				{
					eptr = sel;
				}
				/*
				 * Else start at the end of the previous
				 * element.
				 * (only matters for text elements)
				 */
				else
				{
					position = lo_GetLastCharBeginPosition(eptr);
				}
			}
			/*
			 * This is the easy case, move one position
			 * backward in the selected text element.
			 */
			else
			{
			    position = lo_DecrementPosition(eptr, position);
			}
		}
	}

	*new_sel = eptr;
	*new_pos = position;
}

PRIVATE
Bool
lo_BumpPosition(MWContext *context, lo_DocState *state, LO_Position* position, Bool forward)
{
    LO_Element* newElement;
    int32 newPosition;
    lo_bump_position(context, state, position->element, position->position,
        &newElement, &newPosition, forward);
    if ( newElement == NULL
            || lo_compare_selections(newElement, newPosition, position->element, position->position) == 0)
    {
        /* We ran out of room. */
        return FALSE;
    }
    position->element = newElement;
    position->position = newPosition;
    return TRUE;
}

PRIVATE
Bool lo_ValidEditableElement(MWContext *context, LO_Element* eptr)
{
    return lo_EditableElement(eptr->type) &&
        ( (!EDT_IS_EDITOR(context) ) ||
            (eptr->lo_any.edit_element != 0 &&
             eptr->lo_any.edit_offset >= 0));
}

PRIVATE
Bool lo_ValidEditableElementIncludingParagraphMarks(MWContext *context, LO_Element* pElement)
{
    return lo_ValidEditableElement(context, pElement)
        || (pElement->type== LO_LINEFEED
	        && pElement->lo_linefeed.break_type == LO_LINEFEED_BREAK_PARAGRAPH);
}

/* Moves by one editable position forward or backward. Returns FALSE if it couldn't.
 * Assumes that position is normalized, doesn't denormalize it.
 */

PRIVATE
Bool
lo_BumpNormalizedEditablePosition(MWContext *context, lo_DocState *state,
    LO_Element **pEptr, int32 *pPosition, Bool direction)
{
    LO_Element* newEptr;
    int32 newPosition;
    if ( ! ( pEptr && *pEptr && pPosition ) )
    {
        XP_ASSERT(FALSE);
        return FALSE;
    }

    newEptr = *pEptr;
    newPosition = *pPosition;

    {
        int32 length = lo_GetElementLength(newEptr);
        if ( ! (newPosition >= 0 && ( newPosition < length || length == 0 ) ) )
        {
            XP_ASSERT(FALSE);
            return FALSE;
        }
    }

    do {
        LO_Element* oldEptr = newEptr;
        int32 oldPosition = newPosition;
        lo_bump_position(context, state, newEptr, newPosition,
            &newEptr, &newPosition, direction);
        if ( newEptr == NULL
            || lo_compare_selections(newEptr, newPosition, oldEptr, oldPosition) == 0)
        {
            /* We ran out of room. */
            return FALSE;
        }
    } while ( ! lo_ValidEditableElementIncludingParagraphMarks(context, newEptr) );
    *pEptr = newEptr;
    *pPosition = newPosition;
    return TRUE;
}

/* This function is only intended for the internal use of
 * lo_BumpEditablePosition. It handles the quick case where
 * the position doesn't cross over the object boundary.
 */

PRIVATE
Bool
lo_QuickBumpEditablePosition(MWContext *context, lo_DocState *state,
    LO_Element **pEptr, int32 *pPosition, Bool direction)
{
    if ( direction )
    {
        if ( *pPosition < lo_GetMaximumInsertPointPosition(*pEptr) )
        {
            /* Easy case: Moving forward within an element. */
			*pPosition = lo_IncrementPosition(*pEptr, *pPosition);

/*			XP_TRACE(("new position = %d", *pPosition)); */
            return TRUE;
        }
    }
    else
    {
        if ( *pPosition > 0 )
        {
            /* Easy case: Moving backward within an element. */
			*pPosition = lo_DecrementPosition(*pEptr, *pPosition);

/*			XP_TRACE(("new position = %d", *pPosition)); */
            return TRUE;
        }
    }
    return FALSE;
}

/* Moves by one editable position forward or backward. Returns FALSE if it couldn't.
 * position can be denormalized on entry and may be denormalized on exit.
 */

Bool
lo_BumpEditablePosition(MWContext *context, lo_DocState *state,
    LO_Element **pEptr, int32 *pPosition, Bool direction)
{
    LO_Element* newEptr;
    int32 newPosition;
    if ( ! ( pEptr && *pEptr && pPosition ) )
    {
        XP_ASSERT(FALSE);
        return FALSE;
    }
    newEptr = *pEptr;
    newPosition = *pPosition;

    if ( ! lo_QuickBumpEditablePosition(context, state, &newEptr, &newPosition, direction ) )
    {
        /* Normalize, and try again. */
        if ( ! lo_NormalizeSelectionPoint(context, state, &newEptr, &newPosition) )
            return FALSE;

        if ( ! lo_QuickBumpEditablePosition(context, state, &newEptr, &newPosition, direction ) )
        {
            /* Now that it's normalized, try the heavy-duty bump. */
            if ( ! lo_BumpNormalizedEditablePosition(context, state, &newEptr, &newPosition, direction) )
            {
                return FALSE;
            }
        }
    }

    *pEptr = newEptr;
    *pPosition = newPosition;
    return TRUE;
}

PRIVATE
Bool
lo_BumpEditablePositionForward(MWContext *context, lo_DocState *state,
    LO_Element **pEptr, int32 *pPosition)
{
    return lo_BumpEditablePosition(context, state, pEptr, pPosition, TRUE);
}


PRIVATE
Bool
lo_BumpEditablePositionBackward(MWContext *context, lo_DocState *state,
    LO_Element **pEptr, int32 *pPosition)
{
    return lo_BumpEditablePosition(context, state, pEptr, pPosition, FALSE);
}

PRIVATE
void
lo_ChangeSelection(MWContext *context, lo_DocState *state,
	LO_Element *changed_start, LO_Element *changed_end,
	int32 changed_start_pos, int32 changed_end_pos)
{
	LO_Element *eptr;
	int32 position;
	LO_Element *start, *end;
	int32 start_pos, end_pos;
	intn compare_start;
	intn compare_end;

    /*
     * Handle case where there's no existing selection
     */

    if ( state->selection_start == NULL )
    {
		lo_HighlightSelect(context, state,
			changed_start, changed_start_pos,
			changed_end, changed_end_pos, TRUE);
        return;
    }

	start = state->selection_start;
	start_pos = state->selection_start_pos;
	end = state->selection_end;
	end_pos = state->selection_end_pos;
    lo_NormalizeSelectionEnd(context, state, &end, &end_pos);

	/*
	 * If the end of the old selection is less than
	 * the start of the new one.  Or the end of the new one is
	 * less than the start of the old one.
	 * Erase the old one, draw the new one.
	 */
	if ((lo_compare_selections(end, end_pos, changed_start,
		changed_start_pos) < 0)||
	    (lo_compare_selections(changed_end, changed_end_pos, start,
		start_pos) < 0))
	{
		lo_HighlightSelect(context, state,
			start, start_pos, end, end_pos, FALSE);
		lo_HighlightSelect(context, state,
			changed_start, changed_start_pos,
			changed_end, changed_end_pos, TRUE);
	}
	else
	{
		compare_start = lo_compare_selections(changed_start,
			changed_start_pos, start, start_pos);
		compare_end = lo_compare_selections(changed_end,
			changed_end_pos, end, end_pos);
		/*
		 * If the start position has moved either draw more,
		 * or erase some of the old.
		 */
		if (compare_start < 0)
		{
			lo_HighlightSelect(context, state,
				changed_start, changed_start_pos,
				start, start_pos, TRUE);
		}
		else if (compare_start > 0)
		{
			lo_bump_position(context, state,
				changed_start, changed_start_pos,
				&eptr, &position, FALSE);
			lo_HighlightSelect(context, state,
				start, start_pos, eptr, position, FALSE);
		}

		/*
		 * If the end position has moved either draw more,
		 * or erase some of the old.
		 */
		if (compare_end > 0)
		{
			lo_HighlightSelect(context, state, end, end_pos,
				changed_end, changed_end_pos, TRUE);
		}
		else if (compare_end < 0)
		{
			lo_bump_position(context, state,
				changed_end, changed_end_pos,
				&eptr, &position, TRUE);
			lo_HighlightSelect(context, state,
				eptr, position, end, end_pos, FALSE);
		}
	}
}

Bool LO_SelectElement( MWContext *context, LO_Element *eptr, 
	int32 position, Bool bFromStart  )
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}
	state = top_state->doc_state;

    /* If we've been handed a non-editable item, go to the previous editable item */
    if ( ! lo_EnsureEditableSearchPrev2(context, state, &eptr, &position) )
    {
        return FALSE;
    }

    if ( lo_IsEdgeOfDocument2(context, state, eptr, position, ! bFromStart ) )
    {
        return FALSE;
    }

    /* If we're selecting backwards, actually select one lower */
    if ( bFromStart )
    {
        if ( ! lo_BumpEditablePositionBackward(context, state,
			&eptr, &position) ) return FALSE;
    }
    /* End is the same as the beginning. */
    lo_FullSetSelection(context, state, eptr, position, eptr, position, bFromStart);
    return TRUE;
}

/* Internal routine that should not be exported */
void LO_ExtendSelectionFromElement( MWContext *context, LO_Element *eptr, 
	int32 position, Bool bFromStart  )
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *changed_start, *changed_end;
	int32 changed_start_pos, changed_end_pos;

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

	/*
	 * If we have started a new selection, replace the old one with
	 * this new one before extending
	 */
	if (state->selection_new != NULL)
	{
        lo_StartNewSelection(context, state, eptr, position);
        return;
	}

	/*
	 * If we somehow got here without having any selection or started
	 * selection, start it now where we are.
	 */
	if (state->selection_start == NULL)
	{
		state->selection_start = eptr;
		state->selection_start_pos = position;
		state->selection_end = state->selection_start;
		state->selection_end_pos = state->selection_start_pos;
		state->extending_start = FALSE;
	}

	/*
	 * By some horrendous bug the start was set and the end was not,
	 * and there was no newly started selection.  Correct this now.
	 */
	if (state->selection_end == NULL)
	{
		state->selection_end = state->selection_start;
		state->selection_end_pos = state->selection_start_pos;
		state->extending_start = FALSE;
	}

	/*
	 * Extend the same side we extended last time.
	 */
	if (state->extending_start != FALSE)
	{
		changed_start = eptr;
		changed_start_pos = position;
		changed_end = state->selection_end;
		changed_end_pos = state->selection_end_pos;
	}
	else
	{
		changed_start = state->selection_start;
		changed_start_pos = state->selection_start_pos;
		changed_end = eptr;
		changed_end_pos = position;
	}

	/*
	 * If we crossed our start and end positions, switch
	 * them, and switch which end we are extending from.
	 */
	if (changed_start->lo_any.ele_id > changed_end->lo_any.ele_id)
	{
		eptr = changed_start;
		position = changed_start_pos;
		changed_start = changed_end;
		changed_start_pos = changed_end_pos;
		changed_end = eptr;
		changed_end_pos = position;
		if (state->extending_start != FALSE)
		{
			state->extending_start = FALSE;
		}
		else
		{
			state->extending_start = TRUE;
		}
	}
	else if ((changed_start->lo_any.ele_id == changed_end->lo_any.ele_id)&&
		 (changed_start_pos > changed_end_pos))
	{
		position = changed_start_pos;
		changed_start_pos = changed_end_pos;
		changed_end_pos = position;
		if (state->extending_start != FALSE)
		{
			state->extending_start = FALSE;
		}
		else
		{
			state->extending_start = TRUE;
		}
	}

	lo_ChangeSelection(context, state, changed_start, changed_end,
		changed_start_pos, changed_end_pos);

	lo_SetSelect(context, state, changed_start, changed_start_pos,
		changed_end, changed_end_pos, TRUE);

	state->selection_start = changed_start;
	state->selection_start_pos = changed_start_pos;
	state->selection_end = changed_end;
	state->selection_end_pos = changed_end_pos;

	/*
	 * Cannot have a new selection after an extend.
	 */
	state->selection_new = NULL;
	state->selection_new_pos = 0;

#if 0
	/*
	 * If the beginning and the endpoints were not the beginning and end points
	 *  then the editors notion of bFromStart is backwards.
	*/
	if( state->extending_start ){
		state->extending_start = !bFromStart;
	}
	else {
		state->extending_start = bFromStart;
	}
#endif
}

void LO_SelectRegion( MWContext *context, LO_Element *begin, int32 beginPosition, 
	LO_Element* end, int32 endPosition, Bool fromStart , Bool forward)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
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

    if ( ! begin || !end ) {
        XP_ASSERT(FALSE);
        return;
    }

#if 0
    lo_NormalizeSelectionPoint(context, state, &begin, &beginPosition);
    lo_NormalizeSelectionPoint(context, state, &end, &endPosition);
#endif

#if 0
    XP_TRACE(("begin %d %d %d end %d %d %d start %d forward %d",
        begin->type, begin->lo_any.ele_id, beginPosition,
        end->type, end->lo_any.ele_id, endPosition,
        fromStart, forward));
#endif
    /* Normalize end-points for end-of line conditions */
    if ( fromStart )
    {
        if ( !forward && begin->type == LO_LINEFEED
            && beginPosition == 0 )
        {
            lo_BumpEditablePositionBackward(context, state, &begin, &beginPosition);
        }
    }
    else
    {
        LO_Element* prev;
        if ( forward && endPosition == 0 &&
            ( end->type == LO_LINEFEED ||
                ((prev = lo_BoundaryJumpingPrev(context, state, end)) != NULL && prev->type == LO_LINEFEED ) ) )
        {
            lo_BumpEditablePositionForward(context, state, &end, &endPosition);
        }
    }
    LO_StartSelectionFromElement( context, begin, beginPosition, NULL );
    LO_ExtendSelectionFromElement( context, end, endPosition, fromStart );
    state->extending_start = fromStart;
}

void
lo_SetSelection(MWContext *context, LO_Selection* selection, Bool extendingStart)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
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

    lo_FullSetSelection( context, state,
        selection->begin.element,
        selection->begin.position,
        selection->end.element,
        selection->end.position, extendingStart);
}

void
lo_FullSetSelection(MWContext *context, lo_DocState * state,
    LO_Element* start, int32 start_pos,
    LO_Element* end, int32 end_pos,
    Bool extendFromStart)
{
    LO_Element* normalizedStart = start;
    int32 normalizedStartPosition = start_pos;
    LO_Element* normalizedEnd = end;
    int32 normalizedEndPosition = end_pos;

    LO_ASSERT_POSITION2(context, start, start_pos);
    LO_ASSERT_POSITION2(context, end, end_pos);

    /* The non-editor portions of layout can't deal with a
     * de-normalized positions. So we normalize it here.
     */

    /* LO_DUMP_SELECTION2("FullSetSelection", start, start_pos, end, end_pos); */
    #if 0
        lo_NormalizeSelectionPoint(context, state, &normalizedStart, &normalizedStartPosition);
    #endif

    if ( lo_GetElementLength(normalizedStart) <= normalizedStartPosition )
    {
        normalizedStart = lo_BoundaryJumpingNext(context, state, normalizedStart);
        normalizedStartPosition = 0;
    }
    lo_NormalizeSelectionEnd(context, state, &normalizedEnd, &normalizedEndPosition);

    /* If we've been handed a bad selection, just return. */
    if ( ! ( normalizedStart && normalizedEnd &&
        lo_compare_selections(normalizedStart, normalizedStartPosition,
            normalizedEnd, normalizedEndPosition) <= 0 ) )
    {
        XP_TRACE(("lo_FullSetSelection: bad selection."));
#ifdef DEBUG_EDIT_LAYOUT
        XP_ASSERT(FALSE);
#endif
        return;
    }

	lo_ChangeSelection(context, state, normalizedStart, normalizedEnd,
		normalizedStartPosition, normalizedEndPosition);

	lo_SetSelect(context, state, normalizedStart, normalizedStartPosition,
		normalizedEnd, normalizedEndPosition, TRUE);

	state->selection_start = start;
	state->selection_start_pos = start_pos;
	state->selection_end = end;
	state->selection_end_pos = end_pos;
    state->extending_start = extendFromStart;

	/*
	 * Cannot have a new selection after an extend.
	 */
	state->selection_new = NULL;
	state->selection_new_pos = 0;
}

/* Get the selection edges, expressed as insert points. */

PRIVATE
void
lo_GetSelectionEdge(MWContext *context, lo_DocState * state, LO_Element** pElement, int32* pPosition, Bool bForward)
{
    if(state->selection_start && state->selection_end )
    {
        if ( !bForward )
        {
            *pElement = state->selection_start;
            *pPosition = state->selection_start_pos;
        }
        else
        {
            *pElement = state->selection_end;
            *pPosition = state->selection_end_pos;
            lo_ConvertSelectionEndToInsertPoint(context, state, pElement, pPosition);
        }
    }
    else
    {
        /* Use insert point */
        *pElement = state->selection_new;
        *pPosition = state->selection_new_pos;
    }

}

void
lo_GetAnchorPoint(MWContext *context, lo_DocState * state, LO_Element** pElement, int32* pPosition)
{
    lo_GetSelectionEdge(context, state, pElement, pPosition, state->extending_start);
}
void
lo_GetExtensionPoint(MWContext *context, lo_DocState * state, LO_Element** pElement, int32* pPosition)
{
    lo_GetSelectionEdge(context, state, pElement, pPosition, !state->extending_start);
}

Bool
lo_IsEndOfParagraph2(MWContext *context, LO_Element* element, int32 position)
{
    if ( ! element )
    {
        XP_ASSERT( FALSE );
        return FALSE;
    }

    /* The browser doesn't set break_type, so every line is a paragraph to it. */
    if ( element->type == LO_LINEFEED && 
        ( ! EDT_IS_EDITOR(context)
            || element->lo_linefeed.break_type == LO_LINEFEED_BREAK_PARAGRAPH)) {
        return TRUE;
    }
    return FALSE;
}

PRIVATE
Bool
lo_IsEndOfParagraph(MWContext *context, LO_Position* position)
{
    XP_ASSERT( position );
    return lo_IsEndOfParagraph2(context, position->element, position->position);
}

PRIVATE
Bool
lo_IsHardBreak2(MWContext *context, LO_Element* element, int32 position)
{
    if ( ! element )
    {
        XP_ASSERT( FALSE );
        return FALSE;
    }

    /* The browser doesn't set break_type, so every line is a hard break to it. */
    if ( element->type == LO_LINEFEED && 
        ( ! EDT_IS_EDITOR(context)
            || element->lo_linefeed.break_type != LO_LINEFEED_BREAK_SOFT)) {
        return TRUE;
    }
    return FALSE;
}

PRIVATE
Bool
lo_ExtendToIncludeHardBreak(MWContext* context, lo_DocState *state, LO_Selection* selection)
{
    /* If this is a selection that ends in a hard break, extend the
     * selection to include the hard break.
     */
    Bool result = FALSE;
    if ( EDT_IS_EDITOR(context) )
    {
        LO_Position* end = &selection->end;
        if ( lo_IsHardBreak2(context, end->element,0) )
        {
            result = TRUE; /* Already at a break character. */
        }
        else
        {
            if ( end->position >= lo_GetLastCharEndPosition(end->element) )
            {
                LO_Element* eol = lo_BoundaryJumpingNext(context, state, end->element);
            	if ( lo_IsHardBreak2(context, eol,0) )
                {
                    /* Extend to include the paragraph end element */
                    end->element = eol;
                    end->position = 0;
                    result = TRUE;
                }
            }
        }
    }
    return result;
}

#ifdef DEBUG
void
lo_ValidatePosition(MWContext *context, LO_Position* position)
{
    XP_ASSERT( position );
    if ( position ) {
        lo_ValidatePosition2(context, position->element, position->position);
    }
}

void
lo_ValidatePosition2(MWContext *context, LO_Element* element, int32 position)
{
    int32 length;
    XP_ASSERT( element );
    XP_ASSERT( context );
    if ( context && element )
    {
        length = lo_GetElementLength(element);
        XP_ASSERT( 0 <= position );
        XP_ASSERT( position <= length );
        if ( EDT_IS_EDITOR(context) )
        {
            /* Either it's a line-feed with an eop, or it has an edit_element. */
            if ( ! element->lo_any.edit_element )
            {
                if (element->type == LO_LINEFEED)
                {
                    if ( ! lo_IsEndOfParagraph2(context, element, position) )
                    {
                        XP_TRACE(("lo_ValidatePosition2: illegal position: a line feed without an end-of-paragraph marker."));
#ifdef DEBUG_EDIT_LAYOUT
                            XP_ASSERT(FALSE);
#endif
                    }
                }
                else
                {
#ifdef DEBUG_EDIT_LAYOUT
                    XP_ASSERT(FALSE);
#endif
                }
            }
        }
    }
}

void
lo_ValidateSelection(MWContext *context, LO_Selection* selection)
{
    XP_ASSERT( selection );
    lo_ValidateSelection2( context, selection->begin.element, selection->begin.position,
        selection->end.element, selection->end.position );
}

void
lo_ValidateSelection2(MWContext *context, LO_Element* beginElement, int32 beginPosition, LO_Element* endElement, int32 endPosition)
{
    lo_ValidatePosition2( context, beginElement, beginPosition );
    lo_ValidatePosition2( context, endElement, endPosition );
    XP_ASSERT( lo_compare_selections(beginElement, beginPosition,
        endElement, endPosition) <= 0 );
}

#endif

/*
 * An insert point is one edit position greater than a selection end. But there are
 * two insert points for the edit position that falls between elements. One
 * insert point is for the end of the previous element. The other is for the
 * beginning of the next element. There are two corresponding selection ends.
 *
 *  end of previous element:
 *    insertion point: element = <previous>, position = <previous.length>
 *    selection end:   element = <previous>, position = <previous.length> - 1
 *
 *  start of next element:
 *    insertion point: element = <next>,     position = 0
 *    selection end:   element = <previous>, position = <previous.length>
 */

PRIVATE
Bool lo_IsAtStartOfLine(MWContext* context, lo_DocState* state,
    LO_Element* element, int32 position)
{
    LO_Element* prev;
    if ( position != 0 || ! element )
        return FALSE;
    prev = element->lo_any.prev;
    if ( ! prev )
        return TRUE;
    if ( prev->type == LO_LINEFEED ||
        prev->type == LO_BULLET )
        return TRUE;
    /* Numbered list test. */
    if ( EDT_IS_EDITOR(context) && prev->type == LO_TEXT && prev->lo_text.edit_element == 0 )
        return TRUE;
    return FALSE;
}

Bool lo_ConvertInsertPointToSelectionEnd(MWContext* context, lo_DocState * state,
    LO_Element** pElement, int32* pPosition)
{
    LO_Element* eptr = *pElement;
    int32 position = *pPosition;

    /* Special case for insert points at beginning of lines. */
    if ( lo_IsAtStartOfLine(context, state, eptr, position) )
    {
        /* Skip backwards to line feed that has end-of-paragraph set */
        while ( eptr && lo_BoundaryJumpingPrev(context, state, eptr) ) {
            lo_bump_position(context, state, eptr, position, &eptr, &position, FALSE);
            if ( ! eptr )
            {
                break;
            }

            if ( eptr->type == LO_LINEFEED )
            {
                if ( eptr->lo_linefeed.break_type != LO_LINEFEED_BREAK_SOFT )
                {
                    break;
                }
            }
            /* Linefeeds are editable, but the if test above filters them out. */
            else if (lo_EditableElement(eptr->type) )
            {
                break;
            }
        }
        position = lo_GetLastCharBeginPosition(eptr);
    }
    else
    if ( lo_BumpEditablePositionBackward(context, state, &eptr, &position) )
    {
        if ( *pPosition == 0 )
        {
            /* This is the "Start of next element" case.  */
            lo_BumpEditablePositionForward(context, state, &eptr, &position);
        }
    }
    else
    {
        /* If we can't go backwards, it means that the insert point was at the beginning of the document.
         * This is an error condition, because there is no legal selection end that corresponds
         * to this insert point.
         */
        return FALSE;
    }

    /* It turns out that the end is the address of the last byte selected, not just the position of the
     * first byte of the two-byte character. So we have to bump forward and subtract one.
     *
     * (If we're a denormalized selection end, we don't increment our position.
     *  that's what the "if" is for.)
     */
    if ( position < lo_GetElementLength(eptr) )
    {
        position = lo_IncrementPosition(eptr, position) - 1;
    }
    *pElement = eptr;
    *pPosition = position;
    return TRUE;
}

Bool lo_ConvertSelectionEndToInsertPoint(MWContext* context, lo_DocState * state,
    LO_Element** pElement, int32* pPosition)
{
    LO_Element* eptr = *pElement;
    int32 position = *pPosition;
    int32 length = lo_GetElementLength(*pElement);

    LO_ASSERT_POSITION2( context, eptr, position );
    /* It turns out that the end is the address of the last byte selected, not just the position of the
     * first byte of the two-byte character. So we have to add one and bump backward.
     */
    if ( position >= length )
    {
        /* this is the end-of-previous-element case. */
        XP_ASSERT( position == length );
        position = lo_GetLastCharBeginPosition(*pElement);

        /* If we can't go forward, it means that the selection end was beyond the end of the document.
         * This is an error condition.
         */
        if ( ! lo_BumpEditablePositionForward(context, state, &eptr, &position) )
        {
            return FALSE;
        }
    }
    else
    {
        position = lo_DecrementPosition(eptr, position + 1);
        /* This maps 0..length-1 into 1..length, which is OK. */
        position = lo_IncrementPosition(eptr, position);
    }
    *pElement = eptr;
    *pPosition = position;

    LO_ASSERT_POSITION2( context, eptr, position );
    return TRUE;
}

Bool
lo_NormalizeSelectionPoint(MWContext *context, lo_DocState * state, LO_Element** pEptr, int32* pPosition)
{
    /*
     * If the insert point is off the end of the element, make it the 0th selection of the next element.
     * If we can't normalize it, leave it alone.
     */
    Bool result = TRUE;
    if ( *pEptr != NULL )
    {
        int32 length = lo_GetElementLength(*pEptr);
        int32 newPosition = *pPosition;
        XP_ASSERT( 0 <= newPosition && newPosition <= length );
        if ( newPosition >= length )
        {
            if ( length <= 0 )
            {
                /* There are some zero-length text elements in the browser, when
                 * tables are around. Also, the editor uses zero-length text
                 * elements for empty paragraphs and empty lines.
                 */
                newPosition = 0;
            }
            else {
                /* Needs to be internationalized */
                newPosition = lo_GetLastCharBeginPosition(*pEptr);
                result = lo_BumpNormalizedEditablePosition(context, state, pEptr, &newPosition, TRUE);
            }
            if ( result ) {
                *pPosition = newPosition;
            }
        }
    }
    return result;
}

void
lo_NormalizeSelectionEnd(MWContext *context, lo_DocState * state, LO_Element** pEptr, int32* pPosition)
{
    /*
     * If the selection end point is off the end of the element, make it the last position of the element.
     */
    if ( *pEptr != NULL )
    {
        int32 length = lo_GetElementLength(*pEptr);
        XP_ASSERT( 0 <= *pPosition && *pPosition <= length );
        if ( *pPosition >= length )
        {
            *pPosition = lo_GetLastCharEndPosition(*pEptr);
        }
    }
}

PRIVATE
intn
lo_FancyHalfCompareSelections(LO_Element* start, int32 startPosition, LO_Element* end, int32 endPosition)
{
    int32 startID = start->lo_any.ele_id;
    int32 endID = end->lo_any.ele_id;
    if ( startID < endID )
    {
        if ( startID < (endID - 1) )
            return -1; /* Has to be less */
        if ( startPosition < lo_GetElementLength(start) )
            return -1;
        if ( endPosition > 0 )
            return -1;
        return 0;
    }
    XP_ASSERT(FALSE);
    return -1;
}

PRIVATE
intn
lo_FancyCompareSelections(MWContext *context, lo_DocState * state,
    LO_Element* start, int32 startPosition, LO_Element* end, int32 endPosition)
{
#if 0    /* We can't normalize past the end of the document. */
    Bool startIsEOD = ! lo_NormalizeSelectionPoint(context, state, &start, &startPosition);
    Bool endIsEOD = ! lo_NormalizeSelectionPoint(context, state, &end, &endPosition);
    if ( startIsEOD && endIsEOD ) return 0;
    else if ( startIsEOD ) return 1;
    else if ( endIsEOD ) return -1;

    return lo_compare_selections(start, startPosition, end, endPosition);
#endif
    int32 startID = 0;
    int32 endID = 0;

    /* Can not perform if no start or end element */
    if(!start || !end)
    {
        return 0;
    }

    /* Now safe to set IDs */
    startID = start->lo_any.ele_id;
    endID = end->lo_any.ele_id;
    
    if ( startID < endID )
    {
        return lo_FancyHalfCompareSelections(start, startPosition, end, endPosition);
    }
    else if ( startID == endID )
    {
        if ( startPosition < endPosition )
            return -1;
        else if ( startPosition == endPosition )
            return 0;
        return 1;
    }
    else /* startID > endID */
    {
        return - lo_FancyHalfCompareSelections(end, endPosition, start, startPosition);
    }
}


void
lo_StartNewSelection(MWContext *context, lo_DocState * state,
    LO_Element* eptr, int32 position)
{
    /*
     * If the user hasn't moved the insertion point yet, start a selection.
     */
    LO_Element* i_eptr = state->selection_new;
    int32 i_position = state->selection_new_pos;
    intn compare = lo_FancyCompareSelections(context, state, i_eptr, i_position, eptr, position);
    if ( compare < 0 )
    {
        /* old < now, so they're dragging towards the end of the document */
        lo_ConvertInsertPointToSelectionEnd(context, state, &eptr, &position);
		state->extending_start = FALSE;
        lo_FullSetSelection(context, state, i_eptr, i_position, eptr, position, FALSE);
    }
    else if ( compare == 0 )
    {
        /* The user hasn't moved the mouse far enough to select anything. */
        return;
    }
    else
    {
        /* Dragging towards start of document */
        lo_ConvertInsertPointToSelectionEnd(context, state, &i_eptr, &i_position);
		state->extending_start = TRUE;
        lo_FullSetSelection(context, state, eptr, position, i_eptr, i_position, TRUE);
    }
}

void
LO_ExtendSelection(MWContext *context, int32 x, int32 y)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *eptr;
	int32 position;
	LO_HitResult result;

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

	LO_Hit(context, x, y, FALSE, &result, state->selection_layer);

	position = 0;

	eptr = NULL;
    switch ( result.type )
    {
    case LO_HIT_LINE:
        {
            switch ( result.lo_hitLine.region )
            {
            case LO_HIT_LINE_REGION_BEFORE:
                {
                    /* Insertion point before first element of line */
                    eptr = result.lo_hitLine.selection.begin.element;
                    position = result.lo_hitLine.selection.begin.position;
               }
                break;
            case LO_HIT_LINE_REGION_AFTER:
                {
                    /* Insertion point after last element of line */
 		            eptr = result.lo_hitLine.selection.end.element;
                    position = result.lo_hitLine.selection.end.position;
                    lo_ConvertSelectionEndToInsertPoint(context, state, &eptr, &position);
                    if ( eptr->type == LO_LINEFEED ) position = 0;  /* For editable line-feeds */
               }
                break;
            default:
                break;
            }
        }
        break;
    case LO_HIT_ELEMENT:
        {
		    eptr = result.lo_hitElement.position.element;
            position  = result.lo_hitElement.position.position;
            switch ( result.lo_hitElement.region )
            {
            case LO_HIT_ELEMENT_REGION_BEFORE:
                break;
            case LO_HIT_ELEMENT_REGION_MIDDLE:
                {
                    /* The user wants this item selected.
                     *
                     * If the anchor point is before or at this item, the insert point should be
                     * after this item.
                     *
                     */
                    LO_Element* anchorElement;
                    int32 anchorPosition;
                    lo_GetAnchorPoint(context, state, &anchorElement, &anchorPosition);
                    if ( lo_FancyCompareSelections(context, state, anchorElement, anchorPosition, eptr, position) <= 0 )
                    {
		                lo_BumpEditablePositionForward(context, state, &eptr, &position);
                    }
                }
                break;
            case LO_HIT_ELEMENT_REGION_AFTER:
                {
		            lo_BumpEditablePositionForward(context, state, &eptr, &position);
                }
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
	if (eptr == NULL)
	{
		return;
	}

    lo_ExtendSelectionToPosition2(context, top_state, state, eptr, position);
}

void
lo_ExtendSelectionToPosition2(MWContext *context, lo_TopState* top_state, lo_DocState *state, LO_Element* eptr, int32 position)
{
	/*
	 * Are we starting a new selection?
	 */
	if (state->selection_new != NULL)
	{
        lo_StartNewSelection(context, state, eptr, position);
        return;
	}

	/*
	 * If we somehow got here without having any selection or started
	 * selection, start it now where we are.
	 */
	if (state->selection_start == NULL)
	{
		state->selection_start = eptr;
		state->selection_start_pos = position;
		state->selection_end = state->selection_start;
		state->selection_end_pos = state->selection_start_pos;
		state->extending_start = FALSE;
	}

	/*
	 * By some horrendous bug the start was set and the end was not,
	 * and there was no newly started selection.  Correct this now.
	 */
	if (state->selection_end == NULL)
	{
		state->selection_end = state->selection_start;
		state->selection_end_pos = state->selection_start_pos;
		state->extending_start = FALSE;
	}

    {
        LO_Element* anchorElement;
        int32 anchorPosition;
        intn compare;

        lo_GetAnchorPoint(context, state, &anchorElement, &anchorPosition);
        compare = lo_FancyCompareSelections(context, state, eptr, position, anchorElement, anchorPosition );

        if ( compare == 0 )
        {
            /* The anchor point might not be editable -- it might be the start of a line feed.
             * So Jiggle it.
             */
            lo_JiggleToMakeEditable(context, state, &anchorElement, &anchorPosition, FALSE);
            lo_SetInsertPoint(context, top_state, anchorElement, anchorPosition, state->selection_layer);
        }
        else
        {
	        LO_Element *changed_start, *changed_end;
	        int32 changed_start_pos, changed_end_pos;
            Bool extendingStart;
            if ( compare < 0 )
            {
                /* new position is less than anchor. So it's the start */
                changed_start = eptr;
                changed_start_pos = position;
                changed_end = anchorElement;
                changed_end_pos = anchorPosition;
                extendingStart = TRUE;
            }
            else
            {
                /* new position is greater than anchor. So it's the end */
                changed_start = anchorElement;
                changed_start_pos = anchorPosition;
                changed_end = eptr;
                changed_end_pos = position;
                extendingStart = FALSE;
            }
            lo_ConvertInsertPointToSelectionEnd(context, state, &changed_end, &changed_end_pos);

            lo_FullSetSelection(context, state, changed_start, changed_start_pos,
                changed_end, changed_end_pos, extendingStart);
        }
    }
}


void
LO_EndSelection(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

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
}


#ifdef NOT_USED
static void
lo_copy_color(LO_Color *from_color, LO_Color *to_color)
{
	to_color->red = from_color->red;
	to_color->green = from_color->green;
	to_color->blue = from_color->blue;
}

#endif /* NOT_USED */


/*
 * If this element is part of a cell, return that cell.
 */
LO_Element *
lo_JumpCellWall(MWContext *context, lo_DocState *state, LO_Element *eptr)
{
	LO_Element *parent;
	LO_Element *cell = NULL;
	LO_Element *prev_eptr;
	LO_Element *stop_eptr;
	int32 x, y;
	int32 ret_x, ret_y;
	Bool guess_mode;
	CL_Layer *layer;

	/*
	 * We only turn this on if we are desparate and think
	 * we are in a cell with NO selectable elements!
	 */
	guess_mode = FALSE;

	prev_eptr = NULL;
	x = eptr->lo_any.x + eptr->lo_any.x_offset;
	y = eptr->lo_any.y + eptr->lo_any.y_offset;

	/*
	 * If this element is a zero width linefeed, it is unselectable, so
	 * we need to special case this to work off the previous element
	 * in the cell (if any).
	 */
	if (eptr->lo_any.width <= 0)
	{
		prev_eptr = eptr->lo_any.prev;
		/*
		 * Gads, you have have zero width text followed by a
		 * zero width linefeed.  Move back until we get a real
		 * width or can't go any furthur.
		 */
		while ((prev_eptr != NULL)&&(prev_eptr->lo_any.width <= 0))
		{
			prev_eptr = prev_eptr->lo_any.prev;
		}

		if (prev_eptr != NULL)
		{
			x = prev_eptr->lo_any.x + prev_eptr->lo_any.x_offset;
			y = prev_eptr->lo_any.y + prev_eptr->lo_any.y_offset;
		}
		/*
		 * If there is no previous element to the zero-width
		 * one, guess in desperation by moving back one.
		 * Probably there are no other elements in this
		 * cell, go into guess_mode.
		 */
		else
		{
			guess_mode = TRUE;
            /* 
             * We used to subtract 1 from the x pixel position, but
             * that didn't work in all cases and had the potential to
             * cause infinite loops. Now we don't and I feel a lot 
             * happier.
             */
		}
	}

	/*
	 * figure out when to stop traversing inward.
	 * If we have a prev_eptr then that is where we stop,
	 * otherwise stop if we hit the starting element.
	 */
	if (prev_eptr != NULL)
	{
		stop_eptr = prev_eptr;
	}
	else
	{
		stop_eptr = eptr;
	}

	parent = NULL;
    /* This deals with the case where the table is in a layer.
     * We want to search within the corresponding block and
     * not in the base document for the enclosing cell. 
     * BUGBUG This makes the assumption that if this table is
     * in a layer, that layer is part of the current selection
     * state. I don't know if this assumption is always true
     * for all invocations of this function.
     * joki: changing function to work based on layer key focus
     * instead of selected layer so that form elements will be
     * tabable in tables. 5/25/97.
     */
     if (context->compositor && !(CL_IsKeyEventGrabber(context->compositor, NULL))) {
	layer = CL_GetKeyEventGrabber(context->compositor); 
        
	if (layer && LO_GetIdFromLayer(context, layer) != LO_DOCUMENT_LAYER_ID) {
	    LO_CellStruct *layer_cell = lo_GetCellFromLayer(context, layer);
	    if (layer_cell)
		cell = lo_XYToCellElement(context,
					  state,
					  layer_cell,
					  x, y, TRUE, FALSE, FALSE);
	}
    }

    if (cell == NULL)
        cell = lo_XYToDocumentElement(context, state,
                                      x, y, TRUE, FALSE, FALSE, 
                                      &ret_x, &ret_y);
	while ((cell != NULL)&&(cell != stop_eptr)&&(cell->type == LO_CELL))
	{
		parent = cell;
		cell = lo_XYToCellElement(context, state, (LO_CellStruct *)cell,
			x, y, TRUE, FALSE, FALSE);
	}

	/*
	 * If we've traversed to our stopping point.
	 */
	if (cell == stop_eptr)
	{
		/*
		 * If the stopping point has a cell parent, return it.
		 */
		if ((parent != NULL)&&(parent->type == LO_CELL))
		{
			return(parent);
		}
		/*
		 * Else we aren't in a cell, return the original element.
		 */
		else
		{
			return(eptr);
		}
	}
	/*
	 * Else if we are in guess mode, we found a valid parent
	 * cell that appears to have no children, then we need to check
	 * if the first child of the parent is our stop_eptr, and if
	 * so, return the cell parent.
	 */
	else if ((guess_mode != FALSE)&&(cell == NULL)&&(parent != NULL)&&
		(parent->type == LO_CELL))
	{
		if (parent->lo_cell.cell_list == stop_eptr)
		{
			return(parent);
		}

		/*
		 * If we get here we don't know what we found, so just
		 * return.
		 */
		return(eptr);
	}
    /* If we found a parent that's a cell, then return it.
     * I think this case happens when we hit an element
     * that has the same coordinates are the element we
     * were given. An example would be a zero-length
     * text element before a linefeed. Maybe we should
     * change the search algorithm to skip zero-lenght elements?
     *
     */
    else if ( (parent != NULL) && (parent->type == LO_CELL))
    {
        return parent;
    }
	/*
	 * Else, we've missed the stopping point for some reason.
	 * Assume we aren't in a cell.
	 */
	else
	{
		return(eptr);
	}
}


void
lo_SetSelect(MWContext *context, lo_DocState *state,
	LO_Element *start, int32 start_pos, LO_Element *end, int32 end_pos,
	Bool on)
{
        LO_Element *eptr;

	if ((start == NULL)||(end == NULL))
	{
		return;
	}

	eptr = start;
        while ((eptr != NULL)&&(eptr->lo_any.ele_id <= end->lo_any.ele_id))
        {
		int32 last_id;

                switch (eptr->type)
                {
		    case LO_TEXT:
			if (eptr->lo_text.text != NULL)
			{
			    int32 p1, p2;

			    if (eptr == start)
			    {
			        p1 = start_pos;
			    }
			    else
			    {
			        p1 = 0;
			    }

			    if (eptr == end)
			    {
			        p2 = end_pos;
			    }
			    else
			    {
			        p2 = lo_GetLastCharEndPosition(eptr);
			    }
			    if (p2 < p1)
			    {
			        p2 = p1;
			    }

			    if (on != FALSE)
			    {
				eptr->lo_text.ele_attrmask |= LO_ELE_SELECTED;
				eptr->lo_text.sel_start = (intn) p1;
				eptr->lo_text.sel_end = (intn) p2;
			    }
			    else
			    {
				eptr->lo_text.ele_attrmask &= (~(LO_ELE_SELECTED));
				eptr->lo_text.sel_start = -1;
				eptr->lo_text.sel_end = -1;
			    }
			}
			break;
		    case LO_LINEFEED:
			if (on != FALSE)
			{
				eptr->lo_linefeed.ele_attrmask |= LO_ELE_SELECTED;
				eptr->lo_linefeed.sel_start = 0;
				eptr->lo_linefeed.sel_end = 0;
			}
			else
			{
				eptr->lo_linefeed.ele_attrmask &= (~(LO_ELE_SELECTED));
				eptr->lo_linefeed.sel_start = -1;
				eptr->lo_linefeed.sel_end = -1;
			}
			break;
            case LO_IMAGE:
#ifdef EDITOR
            if ( EDT_IS_EDITOR(context) )
            {
        	    if (on != FALSE)
        		{
        			eptr->lo_image.ele_attrmask |= LO_ELE_SELECTED;
			    	eptr->lo_image.sel_start = 0;
			    	eptr->lo_image.sel_end = 0;
        		}
        		else
        		{
        			eptr->lo_image.ele_attrmask &= (~(LO_ELE_SELECTED));
			    	eptr->lo_image.sel_start = -1;
			    	eptr->lo_image.sel_end = -1;
          		}
           }
            break;
#endif /* EDITOR */
		    case LO_HRULE:
#ifdef EDITOR
            if ( EDT_IS_EDITOR(context) )
            {
        	    if (on != FALSE)
        		{
        			eptr->lo_hrule.ele_attrmask |= LO_ELE_SELECTED;
 			    	eptr->lo_hrule.sel_start = 0;
			    	eptr->lo_hrule.sel_end = 0;
       		    }
        		else
        		{
        			eptr->lo_hrule.ele_attrmask &= (~(LO_ELE_SELECTED));
 			    	eptr->lo_hrule.sel_start = -1;
			    	eptr->lo_hrule.sel_end = -1;
         		}
           }
            break;
#endif /* EDITOR */
		    case LO_FORM_ELE:
#ifdef EDITOR
            if ( EDT_IS_EDITOR(context) )
            {
        	    if (on != FALSE)
        		{
        			eptr->lo_form.ele_attrmask |= LO_ELE_SELECTED;
			    	eptr->lo_form.sel_start = 0;
			    	eptr->lo_form.sel_end = 0;
        		}
        		else
        		{
        			eptr->lo_form.ele_attrmask &= (~(LO_ELE_SELECTED));
 			    	eptr->lo_form.sel_start = -1;
			    	eptr->lo_form.sel_end = -1;
         		}
           }
            break;
#endif /* EDITOR */
		    case LO_BULLET:
		    case LO_SUBDOC:
		    case LO_TABLE:
		    default:
 			break;
                }

		last_id = eptr->lo_any.ele_id;

		/*
		 * Jump cell boundries if there is one between start
		 * and end.
		 */
		if ((eptr->lo_any.next == NULL)&&(eptr != end))
		{
            eptr = lo_JumpCellWall(context, state, eptr);
		}

                eptr = eptr->lo_any.next;

		/*
		 * When we walk onto a cell, we need to walk into
		 * it if it isn't empty.
		 */
		if ((eptr != NULL)&&(eptr->type == LO_CELL)&&
			(eptr->lo_cell.cell_list != NULL))
		{
			eptr = eptr->lo_cell.cell_list;
		}

		/*
		 * We don't want infinite loops.
		 * If the element ID hasn't progressed, something
		 * serious is wrong, and we should punt.
		 */
		if ((eptr != NULL)&&(eptr->lo_any.ele_id <= last_id))
		{
#ifdef DEBUG
XP_TRACE(("Selection loop avoidance 1\n"));
#endif /* DEBUG */
			break;
		}
        }
}

typedef enum {
    HL_STARTED,
    HL_NOT_STARTED,
    HL_COMPLETED
} LO_HighlightState;

/* Highlight a range of elements from within a list of layout
   elements.  Return values:

   HL_STARTED      At least one element highlighted from list, but
                   last element in range was not encountered on list.

   HL_NOT_STARTED  Specified range did not contain any of the range
                   of elements specified in the arguments.

   HL_COMPLETED    Specified range of elements has been highlighted. */
static LO_HighlightState
lo_HighlightElementList(MWContext *context,
                        int32 base_x, int32 base_y,
                        LO_Element *list,
                        LO_Element *start, int32 start_pos,
                        LO_Element *end, int32 end_pos,
                        CL_Layer *layer,
                        Bool on)
{
    LO_Element *eptr;
    int16 charset;
    int32 p1, p2;
    LO_HighlightState highlight_state;

    if (list == NULL)
        return HL_COMPLETED;

    /* Locate first element to be highlighted in list */
    eptr = list;
    while (eptr && eptr != start) {
        if (eptr->type == LO_CELL) {
            int32 x_offset = 0;
            int32 y_offset = 0;
            CL_Layer *cell_layer = layer;

            /* LO_CELLs can be either ordinary table cells or
               containers for in-flow layers */
            if (eptr->lo_cell.cell_inflow_layer) {
                cell_layer = eptr->lo_cell.cell_inflow_layer;
                lo_GetLayerXYShift(cell_layer, &x_offset, &y_offset);
            }

            if (eptr->lo_cell.cell_list) {
                highlight_state=lo_HighlightElementList(context, 
                                                        base_x - x_offset,
                                                        base_y - y_offset,
                                                        eptr->lo_cell.cell_list,
                                                        start, start_pos,
                                                        end, end_pos, 
                                                        cell_layer, on);
                if (highlight_state == HL_COMPLETED)
                    return HL_COMPLETED;
                if (highlight_state == HL_STARTED) {
                    eptr = eptr->lo_any.next;
                    break;
                }
            }
        }
        eptr = eptr->lo_any.next;
    }
    if (!eptr)
        return HL_NOT_STARTED;

    /* Paint highlighted elements */
    while ((eptr != NULL) && (eptr->lo_any.ele_id <= end->lo_any.ele_id))
    {
        int32 last_id;

        switch (eptr->type)
        {
        case LO_TEXT:
            if (eptr->lo_text.text == NULL)
                break;
            
            if (eptr == start)
                p1 = start_pos;
            else
                p1 = 0;

            if (eptr == end)
                p2 = end_pos;
            else
                p2 = lo_GetLastCharEndPosition(eptr);

            if (p2 < p1)
                p2 = p1;

            if ( p2 >= eptr->lo_text.text_len )
                p2 = lo_GetLastCharEndPosition(eptr);

            if (on != FALSE)
            {
                eptr->lo_text.ele_attrmask |= LO_ELE_SELECTED;
                eptr->lo_text.sel_start = (intn) p1;
                eptr->lo_text.sel_end = (intn) p2;
            }
            else
            {
                eptr->lo_text.ele_attrmask &= (~(LO_ELE_SELECTED));
                eptr->lo_text.sel_start = -1;
                eptr->lo_text.sel_end = -1;
            }

            charset = ((LO_TextStruct *) eptr)->text_attr->charset;
            if ((eptr == start) || ((eptr == end) &&
									INTL_CharSetType(charset) != SINGLEBYTE))
            {
                /* ugly processing for multibyte here	*/
                char *string;
                int n;

                PA_LOCK(string, char *, eptr->lo_text.text);

                /*
                 * find beginning of first character
                 */
                switch (n = INTL_NthByteOfChar(charset, string, (int)(p1+1)))
                {
                case 0:
                case 1:
                    break;
                default:
                    p1 -= (n - 1);
                    if (p1 < 0)
                        p1 = 0;

                    break;
                }

                /*
                 * find end of last character
                 */
                switch (n = INTL_NthByteOfChar(charset, string, (int)(p2+1)))
                {
                case 0:
                    break;
                default:
                    p2 -= (n - 1);
                    if (p2 < 0)
                        p2 = 0;

                    /* FALL THROUGH */
                case 1:
                    p2 += INTL_IsLeadByte(charset, string[p2]);
                    if (p2 > lo_GetLastCharEndPosition(eptr))
                    {
                        p2 = lo_GetLastCharEndPosition(eptr);
                    }
                    break;
                }

                PA_UNLOCK(eptr->lo_text.text);
            }
            
            if (p1 <= p2)
            {
                lo_DisplaySubtext(context,
                                  (LO_TextStruct *)eptr, p1, p2, 
                                  !on,
                                  layer);
            }
            break;

        case LO_LINEFEED:
            if (on != FALSE)
            {
                eptr->lo_linefeed.ele_attrmask |= LO_ELE_SELECTED;
                eptr->lo_linefeed.sel_start = 0;
                eptr->lo_linefeed.sel_end = 0;
            }
            else
            {
                eptr->lo_linefeed.ele_attrmask &= (~(LO_ELE_SELECTED));
                eptr->lo_linefeed.sel_start = -1;
                eptr->lo_linefeed.sel_end = -1;
            }

            lo_RefreshElement(eptr, layer, FALSE);
            break;

        case LO_HRULE:
#ifdef EDITOR
            if ( EDT_IS_EDITOR(context) )
            {
                if (on != FALSE)
                {
                    eptr->lo_hrule.ele_attrmask |= LO_ELE_SELECTED;
                    eptr->lo_hrule.sel_start = 0;
                    eptr->lo_hrule.sel_end = 0;
                }
                else
                {
                    eptr->lo_hrule.ele_attrmask &= (~(LO_ELE_SELECTED));
                    eptr->lo_hrule.sel_start = -1;
                    eptr->lo_hrule.sel_end = -1;
                }

                lo_RefreshElement(eptr, layer, FALSE);
            }
            break;
#endif
        case LO_FORM_ELE:
#ifdef EDITOR
            if ( EDT_IS_EDITOR(context) )
            {
                if (on != FALSE)
                {
                    eptr->lo_form.ele_attrmask |= LO_ELE_SELECTED;
                    eptr->lo_form.sel_start = 0;
                    eptr->lo_form.sel_end = 0;
                }
                else
                {
                    eptr->lo_form.ele_attrmask &= (~(LO_ELE_SELECTED));
                    eptr->lo_form.sel_start = -1;
                    eptr->lo_form.sel_end = -1;
                }

                lo_DisplayFormElement(context, (LO_FormElementStruct *)eptr);
            }
            break;
#endif
        case LO_IMAGE:
#ifdef EDITOR
            if ( EDT_IS_EDITOR(context) )
            {
                if (on != FALSE)
                {
                    eptr->lo_image.ele_attrmask |= LO_ELE_SELECTED;
                    eptr->lo_image.sel_start = 0;
                    eptr->lo_image.sel_end = 0;
                }
                else
                {
                    eptr->lo_image.ele_attrmask &= (~(LO_ELE_SELECTED));
                    eptr->lo_image.sel_start = -1;
                    eptr->lo_image.sel_end = -1;
                }

                {
                    LO_ImageStruct *lo_image = (LO_ImageStruct*)eptr;
                    XP_Rect rect;
                    
                    rect.left = 0;
                    rect.top = 0;
                    rect.right = lo_image->width;
                    rect.bottom = lo_image->height;

                    CL_UpdateLayerRect(context->compositor, lo_image->layer,
                                       &rect, (PRBool)FALSE);
                }
            }
#endif
            break;

        case LO_CELL:
        {
            LO_CellStruct *cell = &eptr->lo_cell;
            int32 x_offset = 0;
            int32 y_offset = 0;
            CL_Layer *cell_layer = layer;

            /* LO_CELLs can be either ordinary table cells or
               containers for in-flow layers */
            if (cell->cell_inflow_layer) {
                cell_layer = cell->cell_inflow_layer;
                lo_GetLayerXYShift(cell_layer, &x_offset, &y_offset);
            }
            
            if (!eptr->lo_cell.cell_list)
                break;
            
            highlight_state = lo_HighlightElementList(context,
                                                      base_x - x_offset,
                                                      base_y - y_offset,
                                                      eptr->lo_cell.cell_list,
                                                      eptr->lo_cell.cell_list, 0,
                                                      end, end_pos, 
                                                      cell_layer, on);
            if (highlight_state == HL_COMPLETED) {
                return HL_COMPLETED;
            }
            break;
        }

        case LO_SUBDOC:
        case LO_TABLE:
        case LO_BULLET:
        default:
            break;
        }

        last_id = eptr->lo_any.ele_id;

        eptr = eptr->lo_any.next;

        /*
         * We don't want infinite loops.
         * If the element ID hasn't progressed, something
         * serious is wrong, and we should punt.
         */
        if ((eptr != NULL)&&(eptr->lo_any.ele_id <= last_id))
        {
#ifdef DEBUG
            XP_TRACE(("Selection loop avoidance 2\n"));
#endif /* DEBUG */
            break;
        }
    }

    /* We ran off the list without reaching the last element in the
       selection range, so tell our caller that there's more work to do. */
    if (!eptr)
        return HL_STARTED;

    /* We reached the last element in the selection range.  We're done. */
    return HL_COMPLETED;
}


void
lo_HighlightSelect(MWContext *context, lo_DocState *state,
                   LO_Element *start, int32 start_pos,
                   LO_Element *end, int32 end_pos,
                   Bool on)
{
    LO_Element **line_array, *list;
    LO_CellStruct *layer_cell;

    if ((start == NULL) || (end == NULL))
        return;

    /* 
     * Do a synchronous composite to flush any other drawing 
     * request.  Since we're going to replace the painter_func
     * of state->selection_layer, we don't want to ignore other
     * requests that might require this layer to draw.  This call
     * must happen before the text element is modified in any way.
     */
    if (context->compositor)
        CL_CompositeNow(context->compositor);

    /* The stinkin' editor doesn't set the selection layer yet. */
    if (!state->selection_layer) 
        state->selection_layer = CL_FindLayer(context->compositor, LO_BODY_LAYER_NAME);

    layer_cell = lo_GetCellFromLayer(context, state->selection_layer);
    if (layer_cell == NULL) {
        XP_LOCK_BLOCK(line_array, LO_Element**, state->line_array );
        if (line_array)
            list = line_array[0];
        else
            list = NULL;
        XP_UNLOCK_BLOCK( state->line_array );
    } else 
        list = layer_cell->cell_list;

    lo_HighlightElementList(context, state->base_x, state->base_y, list,
                            start, start_pos, end, end_pos, 
                            state->selection_layer, on);

    /* On the Mac, we don't run timeouts during a selection, so the
       compositor won't run.  Therefore we need to force any new
       selection to paint now. */
    if (context->compositor)
        CL_CompositeNow(context->compositor);
}

void
LO_HighlightSelection(MWContext *context, Bool on)
{
        int32 doc_id;
	lo_TopState *top_state;
        lo_DocState *state;
        LO_Element *start, *end;
	int32 start_pos, end_pos;

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

        if ((state->selection_start == NULL)||(state->selection_end == NULL))
        {
                return;
        }

        start = state->selection_start;
	start_pos = state->selection_start_pos;
        end = state->selection_end;
	end_pos = state->selection_end_pos;

	lo_HighlightSelect(context, state, start, start_pos, end, end_pos,
					on);
}


void
LO_ClearSelection(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

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

	LO_HighlightSelection(context, FALSE);
	state->selection_start = NULL;
	state->selection_start_pos = 0;
	state->selection_end = NULL;
	state->selection_end_pos = 0;
    state->selection_layer = NULL;
}


PRIVATE
XP_Block
lo_SelectionToText(MWContext *context, lo_DocState *state,
	LO_Element *start, int32 start_pos, LO_Element *end, int32 end_pos)
{
        LO_Element *eptr;
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	LO_TextAttr tmp_attr;
	int32 space_width;
	int32 length;
	Bool indent_counts;
	PA_Block sbuff;
	XP_Block buff;
	char *str;
	char *tptr;
/*	int16 charset;*/

        if ((start == NULL)||(end == NULL))
        {
                return(NULL);
        }

	/*
	 * All this work is to get the width of a " " in the default
	 * fixed font.
	 */
	memset (&tmp_text, 0, sizeof (tmp_text));
	sbuff = PA_ALLOC(1);
	if (sbuff == NULL)
	{
		return(NULL);
	}
	PA_LOCK(str, char *, sbuff);
	str[0] = ' ';
	PA_UNLOCK(sbuff);
	tmp_text.text = sbuff;
	tmp_text.text_len = 1;
	/*
	 * Fill in default font information.
	 */
	lo_SetDefaultFontAttr(state, &tmp_attr, context);
	tmp_attr.fontmask |= LO_FONT_FIXED;
	tmp_text.text_attr = lo_FetchTextAttr(state, &tmp_attr);
	FE_GetTextInfo(context, &tmp_text, &text_info);
	PA_FREE(sbuff);
	space_width = text_info.max_width;
	if (space_width <= 0)
	{
		space_width = 2;
	}

	indent_counts = FALSE;
	length = 0;
	eptr = start;
        while ((eptr != NULL)&&(eptr->lo_any.ele_id <= end->lo_any.ele_id))
        {
		int32 last_id;

                switch (eptr->type)
                {
		    case LO_TEXT:
			if (eptr->lo_text.text != NULL)
			{
			    if (indent_counts != FALSE)
			    {
				int32 width;

				width = eptr->lo_text.x +
					eptr->lo_text.x_offset -
					state->win_left;
				width = (width + space_width - 1) / space_width;
				if (width < 0)
				{
					width = 0;
				}
				length += width;
				indent_counts = FALSE;
			    }

			    if ((eptr == start)||(eptr == end))
			    {
			        int32 p1, p2;

			        if (eptr == start)
			        {
				    p1 = start_pos;
			        }
			        else
			        {
				    p1 = 0;
			        }

			        if (eptr == end)
			        {
				    char *string;

				    PA_LOCK(string, char *, eptr->lo_text.text);
					/*  charset = eptr->lo_text.text_attr->charset;
				     *	p2 = end_pos + INTL_IsLeadByte(charset, string[end_pos]);
				     *	
				     *	p2 already point to the end of the selection
				     *	calling INTL_IsLeadByte(charset, string[end_pos]) is wrong here.
				     */
				    p2 = end_pos;
				    PA_UNLOCK(eptr->lo_text.text);
				    if (p2 > lo_GetLastCharEndPosition(eptr))
				    {
					p2 = lo_GetLastCharEndPosition(eptr);
				    }
			        }
			        else
			        {
				    p2 = lo_GetLastCharEndPosition(eptr);
			        }
			        if (p2 < p1)
			        {
				    p2 = p1;
			        }

				length += (p2 - p1 + 1);
			    }
			    else
			    {
				length += eptr->lo_text.text_len;
			    }
			}
			break;
		    case LO_LINEFEED:
			length += LINEBREAK_LEN;
			indent_counts = TRUE;
			break;
		    case LO_HRULE:
		    case LO_FORM_ELE:
		    case LO_BULLET:
		    case LO_IMAGE:
		    case LO_SUBDOC:
		    case LO_TABLE:
		    default:
			break;
                }

		last_id = eptr->lo_any.ele_id;

		/*
		 * Jump cell boundries if there is one between start
		 * and end.
		 */
		if ((eptr->lo_any.next == NULL)&&(eptr != end))
		{
			eptr = lo_JumpCellWall(context, state, eptr);
		}

                eptr = eptr->lo_any.next;

		/*
		 * When we walk onto a cell, we need to walk into
		 * it if it isn't empty.
		 */
		if ((eptr != NULL)&&(eptr->type == LO_CELL)&&
			(eptr->lo_cell.cell_list != NULL))
		{
			eptr = eptr->lo_cell.cell_list;
		}

		/*
		 * We don't want infinite loops.
		 * If the element ID hasn't progressed, something
		 * serious is wrong, and we should punt.
		 */
		if ((eptr != NULL)&&(eptr->lo_any.ele_id <= last_id))
		{
#ifdef DEBUG
XP_TRACE(("Selection loop avoidance 3\n"));
#endif /* DEBUG */
			break;
		}
        }
	length++;

#ifdef XP_WIN16
	if (length > SIZE_LIMIT)
	{
		length = SIZE_LIMIT;
	}
#endif /* XP_WIN16 */

	buff = XP_ALLOC_BLOCK(length * sizeof(char));
	if (buff == NULL)
	{
		return(NULL);
	}
	XP_LOCK_BLOCK(str, char *, buff);

	tptr = str;
	indent_counts = FALSE;
	length = 0;
	eptr = start;
        while ((eptr != NULL)&&(eptr->lo_any.ele_id <= end->lo_any.ele_id))
        {
		int32 last_id;

                switch (eptr->type)
                {
		    case LO_TEXT:
			if (eptr->lo_text.text != NULL)
			{
			    char *text;

			    if (indent_counts != FALSE)
			    {
				int32 width;
				int32 i;

				width = eptr->lo_text.x +
					eptr->lo_text.x_offset -
					state->win_left;
				width = (width + space_width - 1) / space_width;
				if (width < 0)
				{
					width = 0;
				}
				length += width;
#ifdef XP_WIN16
				if (length > SIZE_LIMIT)
				{
					length -= width;
					break;
				}
#endif /* XP_WIN16 */
				for (i=0; i<width; i++)
				{
					XP_BCOPY(" ", tptr, 1);
					tptr++;
				}
				indent_counts = FALSE;
			    }

			    if ((eptr == start)||(eptr == end))
			    {
			        int32 p1, p2, len;
                    int32 maxPos = lo_GetLastCharEndPosition(eptr);

			        if (eptr == start)
			        {
				        p1 = start_pos;
			        }
			        else
			        {
				        p1 = 0;
			        }

			        if (eptr == end)
			        {
    				    char *string;

    				    PA_LOCK(string, char *, eptr->lo_text.text);
				    /*  charset = eptr->lo_text.text_attr->charset;
				     *	p2 = end_pos + INTL_IsLeadByte(charset, string[end_pos]);
				     *	
				     *	p2 already point to the end of the selection
				     *	calling INTL_IsLeadByte(charset, string[end_pos]) is wrong here.
				     */
				    	p2 = end_pos;
    				    PA_UNLOCK(eptr->lo_text.text);
    				    if (p2 > maxPos)
    				    {
    					    p2 = maxPos;
    				    }
			        }
			        else
			        {
				        p2 = maxPos;
			        }
			        if (p2 < p1)
			        {
				        p2 = p1;
			        }

				len = p2 - p1 + 1;
				length += len;
#ifdef XP_WIN16
				if (length > SIZE_LIMIT)
				{
					length -= len;
					break;
				}
#endif /* XP_WIN16 */
				PA_LOCK(text, char *, eptr->lo_text.text);
				XP_BCOPY((char *)(text + p1), tptr, len);
				tptr = (char *)(tptr + len);
				PA_UNLOCK(eptr->lo_text.text);
			    }
			    else
			    {
				length += eptr->lo_text.text_len;
#ifdef XP_WIN16
				if (length > SIZE_LIMIT)
				{
					length -= eptr->lo_text.text_len;
					break;
				}
#endif /* XP_WIN16 */
				PA_LOCK(text, char *, eptr->lo_text.text);
				XP_BCOPY(text, tptr, eptr->lo_text.text_len);
				tptr = (char *)(tptr + eptr->lo_text.text_len);
				PA_UNLOCK(eptr->lo_text.text);
			    }
			}
			break;
		    case LO_LINEFEED:
			length += LINEBREAK_LEN;
#ifdef XP_WIN16
			if (length > SIZE_LIMIT)
			{
				length -= LINEBREAK_LEN;
				break;
			}
#endif /* XP_WIN16 */
			XP_BCOPY(LINEBREAK, tptr, LINEBREAK_LEN);
			tptr = (char *)(tptr + LINEBREAK_LEN);
			indent_counts = TRUE;
			break;
		    case LO_HRULE:
		    case LO_FORM_ELE:
		    case LO_BULLET:
		    case LO_IMAGE:
		    case LO_SUBDOC:
		    case LO_TABLE:
		    default:
			break;
                }

		last_id = eptr->lo_any.ele_id;

		/*
		 * Jump cell boundries if there is one between start
		 * and end.
		 */
		if ((eptr->lo_any.next == NULL)&&(eptr != end))
		{
			eptr = lo_JumpCellWall(context, state, eptr);
		}

                eptr = eptr->lo_any.next;

		/*
		 * When we walk onto a cell, we need to walk into
		 * it if it isn't empty.
		 */
		if ((eptr != NULL)&&(eptr->type == LO_CELL)&&
			(eptr->lo_cell.cell_list != NULL))
		{
			eptr = eptr->lo_cell.cell_list;
		}

		/*
		 * We don't want infinite loops.
		 * If the element ID hasn't progressed, something
		 * serious is wrong, and we should punt.
		 */
		if ((eptr != NULL)&&(eptr->lo_any.ele_id <= last_id))
		{
#ifdef DEBUG
XP_TRACE(("Selection loop avoidance 4\n"));
#endif /* DEBUG */
			break;
		}
        }
	str[length] = '\0';
	length++;
	XP_UNLOCK_BLOCK(buff);

	return(buff);
}


XP_Block
LO_GetSelectionText(MWContext *context)
{
        int32 doc_id;
	lo_TopState *top_state;
        lo_DocState *state;
        LO_Element *start, *end;
	int32 start_pos, end_pos;
	XP_Block buff;

        /*
         * Get the unique document ID, and retreive this
         * documents layout state.
         */
        doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
        {
                return(NULL);
        }
	state = top_state->doc_state;

        if ((state->selection_start == NULL)||(state->selection_end == NULL))
        {
                return(NULL);
        }

        start = state->selection_start;
	start_pos = state->selection_start_pos;
        end = state->selection_end;
	end_pos = state->selection_end_pos;

    /* Make sure the selection is normalized */
    lo_NormalizeSelectionPoint(context, state, &start, &start_pos);
    lo_NormalizeSelectionEnd(context, state, &end, &end_pos);

	buff = lo_SelectionToText(context, state, start, start_pos,
			end, end_pos);

	return(buff);
}


Bool
LO_HaveSelection(MWContext *context)
{
        int32 doc_id;
	lo_TopState *top_state;
        lo_DocState *state;

        /*
         * Get the unique document ID, and retreive this
         * documents layout state.
         */
        doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
        {
                return(FALSE);
        }
	state = top_state->doc_state;

        if ((state->selection_start == NULL)||(state->selection_end == NULL))
        {
                return(FALSE);
        }
	else
        {
                return(TRUE);
        }
}

/* return true if there is a current selection */

PRIVATE
Bool
lo_GetSelection(MWContext *context, LO_Selection* selection)
{
    int32 beginPosition;
    int32 endPosition;
    CL_Layer *sel_layer;
    /* LO_GetSelectionEndPoints uses int32. LO_Selection uses intn. So we need to
     * do the conversion here.
     */

    LO_GetSelectionEndpoints(context, &selection->begin.element, &selection->end.element,
        &beginPosition, &endPosition, &sel_layer);
    selection->begin.position = (intn) beginPosition;
    selection->end.position = (intn) endPosition;
    return selection->begin.element != NULL && selection->end.element != NULL;
}

void
LO_GetSelectionEndpoints(MWContext *context, LO_Element **start, LO_Element **end,
	int32 *start_pos, int32 *end_pos, CL_Layer **sel_layer)
{
        int32 doc_id;
	lo_TopState *top_state;
        lo_DocState *state;

	*start = NULL;
	*end = NULL;
	*start_pos = 0;
	*end_pos = 0;
        *sel_layer = NULL;

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

        if ((state->selection_start == NULL)||(state->selection_end == NULL))
        {
                return;
        }

        *start = state->selection_start;
	*start_pos = state->selection_start_pos;
        *end = state->selection_end;
	*end_pos = state->selection_end_pos;
        *sel_layer = state->selection_layer;
}

/* Returns TRUE if there was anything to select. If there is no data, or if
 * we're in editor mode and there is no editable data, then FALSE is returned.
 */

Bool
LO_SelectAll(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
    LO_Selection selection;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}
	state = top_state->doc_state;

	LO_HighlightSelection(context, FALSE);
	selection.begin.element = NULL;
	selection.begin.position = 0;
	selection.end.element = NULL;
	selection.end.position = 0;

    if ( ! ( lo_FindDocumentEdge(context, state, &selection.begin, TRUE, FALSE)
        && lo_FindDocumentEdge(context, state, &selection.end, TRUE, TRUE) ) )
    {
        return FALSE;
    }
#if 0
    lo_ConvertInsertPointToSelectionEnd(context, state, &selection.end.element, &selection.end.position);
#endif
    LO_ASSERT_SELECTION(context, &selection);
	/*
	 * Nothing to select.
	 */
	if (selection.begin.element == NULL)
	{
		return FALSE;
	}

	state->selection_start = selection.begin.element;
	state->selection_start_pos = selection.begin.position;
	state->selection_end = selection.end.element;
	state->selection_end_pos = selection.end.position;

#if 0
    if ( ! lo_NormalizeSelection(context) ){
        return FALSE;
    }
#endif
	LO_HighlightSelection(context, TRUE);
    return TRUE;
}

#ifdef EDITOR 

/* This routine normalizes a selection so that it only selects editable elements.
 * It returns TRUE if the resulting selection is not empty.
 */
Bool
lo_NormalizeSelection(MWContext *context)
{
	Bool result;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
    intn comparisonResult;

#ifdef DEBUG
    lo_VerifyLayout(context);
#endif /* DEBUG */
    result = FALSE;
	/*
	 * Get the unique document ID, and retreive this
	 * document's layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}
	state = top_state->doc_state;
    if ( ! ( lo_EnsureEditableSearchNext2(context, state, &state->selection_start, & state->selection_start_pos)
        && lo_EnsureEditableSearchPrev2(context, state, &state->selection_end, & state->selection_end_pos) ) )
    {
        XP_ASSERT(FALSE);
        return FALSE;
    }
    /*
     * Ensure that the start position is before the end position
     */
    comparisonResult = lo_compare_selections(
        state->selection_start, state->selection_start_pos,
        state->selection_end, state->selection_end_pos );
    result = comparisonResult <= 0;
    if ( comparisonResult > 0 )
    {
        /*
         * It's not a legal selection. This can happen if we're editing and
         * the selected element is not editable. In this case we null out the selection
         */
         state->selection_start = NULL;
         state->selection_end = NULL;
         state->selection_start_pos = 0;
         state->selection_end_pos = 0;
    }
    return result;
}
#endif

Bool
lo_EnsureEditableSearchNext(MWContext *context, lo_DocState* state, LO_Element** eptr)
{
    int32 position = 0;
    return lo_EnsureEditableSearch2(context, state, eptr, &position, TRUE);
}

Bool
lo_EnsureEditableSearchNext2(MWContext *context, lo_DocState* state, LO_Element** eptr,
    int32* ePositionPtr)
{
    return lo_EnsureEditableSearch2(context, state, eptr, ePositionPtr, TRUE);
}

Bool
lo_EnsureEditableSearchPrev(MWContext *context, lo_DocState* state, LO_Element** eptr)
{
    int32 position = lo_GetMaximumInsertPointPosition(*eptr);
    return lo_EnsureEditableSearch2(context, state, eptr, &position, FALSE);
}

Bool
lo_EnsureEditableSearchPrev2(MWContext *context, lo_DocState* state, LO_Element** eptr, int32* ePositionPtr)
{
    return lo_EnsureEditableSearch2(context, state, eptr, ePositionPtr, FALSE);
}

/* Returns TRUE if the result is editable. Doesn't change position if can't find editable. */

Bool
lo_EnsureEditableSearch(MWContext *context, lo_DocState* state, LO_Position* p, Bool forward)
{
    return lo_EnsureEditableSearch2(context, state, &p->element, &p->position, forward);
}

PRIVATE
Bool
lo_BumpToNextElement(MWContext *context, lo_DocState* state, LO_Element** eptr, int32* ePositionPtr, Bool forward)
{
    LO_Element* element = *eptr;
    int32 position = *ePositionPtr;
    if ( forward )
    {
        element = lo_BoundaryJumpingNext(context, state, element);
    }
    else
    {
        element = lo_BoundaryJumpingPrev(context, state, element);
    }

    if ( ! element ) return FALSE;

    if ( forward )
    {
        position = 0;
    }
    else
    {
        position = lo_GetMaximumInsertPointPosition(element);
    }

    *eptr = element;
    *ePositionPtr = position;
    return TRUE;
}

Bool
lo_EnsureEditableSearch2(MWContext *context, lo_DocState* state, LO_Element** eptr, int32* ePositionPtr, Bool forward)
{
    Bool moved = FALSE;
    LO_Element* element = *eptr;
    int32 position = *ePositionPtr;

    if ( ! element ) return FALSE;

    while ( ! lo_IsValidEditableInsertPoint2(context, state, element, position ) )
    {
        if ( ! lo_BumpToNextElement(context, state, &element, &position, forward ) )
        {
            return FALSE;
        }
        moved = TRUE;
    }

    if ( moved )
    {
        *eptr = element;
        *ePositionPtr = position;
    }
    return TRUE;
}

Bool
lo_IsValidEditableInsertPoint2(MWContext *context, lo_DocState* state, LO_Element* eptr, int32 position)
{
    return lo_ValidEditableElement(context, eptr)
        && position >= 0 && position <= lo_GetMaximumInsertPointPosition(eptr);
}

#ifdef EDITOR
/*
 *  This routine has too much knowledge of the layout engine.  Need to revisit
*/
void LO_PositionCaret(MWContext *context, int32 x, int32 y, CL_Layer *layer)
{
    LO_Click(context, x, y, TRUE, layer);
}

void LO_DoubleClick(MWContext *context, int32 x, int32 y, CL_Layer *layer)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
    LO_HitResult result;

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

    LO_Hit(context, x, y, FALSE, &result, layer);

    lo_ProcessDoubleClick(context, top_state, state, &result, layer);
}


#endif

PRIVATE
void lo_GetHorizontalBounds(LO_Element* eptr, int32* returnBegin, int32* returnEnd)
{
    int32 width = eptr->lo_any.width;
	/*
	 * Images need to account for border width
	 */
	if (eptr->type == LO_IMAGE)
	{
		width = width + (2 * eptr->lo_image.border_width);
	}
	if (width <= 0)
	{
		width = 1;
	}
    *returnBegin = eptr->lo_any.x + eptr->lo_any.x_offset;
    *returnEnd = *returnBegin + width;
}

int32
lo_GetTextAttrMask(LO_Element* eptr)
{
    int32 mask = 0;
    LO_TextAttr* textAttr = 0;
    if ( ! eptr )
    {
        XP_ASSERT(FALSE);
        return mask;
    }
    switch ( eptr->type ) {
    case LO_TEXT:
       textAttr = eptr->lo_text.text_attr;
       break;
    case LO_IMAGE:
       textAttr = eptr->lo_image.text_attr;
       break;
    case LO_LINEFEED:
       textAttr = eptr->lo_linefeed.text_attr;
       break;
    default:
        break;
    }
    if ( textAttr ) {
        mask = textAttr->attrmask;
    }
    return mask;
}

int32
lo_GetElementLength(LO_Element* eptr)
{
    int32 length;
    if ( ! eptr )
    {
        XP_ASSERT(FALSE);
        return 1;
    }
    switch ( eptr->type ) {
    case LO_TEXT:
       length = eptr->lo_text.text_len;
       break;
	case LO_TEXTBLOCK:
	case LO_DESCTITLE:
		length = 0; /*MJUDGE 2-5-98*/
		break;
    case LO_HRULE:
    case LO_IMAGE:
    case LO_LINEFEED:
    default:
        length = 1;
        break;
    }
    return length;
}

LO_AnchorData*
lo_GetAnchorData(LO_Element* eptr)
{
    LO_AnchorData* result = 0;
    if ( ! eptr )
    {
        XP_ASSERT(FALSE);
        return result;
    }
    switch ( eptr->type ) {
    case LO_TEXT:
        result = eptr->lo_text.anchor_href;
        break;
    case LO_IMAGE:
        result = eptr->lo_image.anchor_href;
        break;
    case LO_LINEFEED:
        result = eptr->lo_linefeed.anchor_href;
        break;
   default:
        break;
    }
    return result;
}

LO_Element*
lo_GetNeighbor(LO_Element* element, Bool forward)
{
    LO_Element* result = NULL;
    if ( element ) {
        if ( forward )
            result = element->lo_any.next;
        else
            result = element->lo_any.prev;
    }
    return result;
}

int32
lo_GetElementEdge(LO_Element* element, Bool forward)
{
    int32 result = 0;
    if ( element ) {
        if ( forward )
            result = lo_GetElementLength(element);
    }
    return result;
}

int32
lo_GetMaximumInsertPointPosition(LO_Element* eptr)
{
    if ( ! eptr )
    {
        XP_ASSERT(FALSE);
        return 0;
    }
#if 0
    if ( eptr->type == LO_LINEFEED )
    {
        return 0;
    }
    else
    {
        return lo_GetElementLength(eptr);
    }
#endif
    return lo_GetElementLength(eptr);
}

int32
lo_IncrementPosition(LO_Element* eptr, int32 position)
{
   int32 length = lo_GetElementLength(eptr);
   if ( position < length )
   {
       if (eptr->type == LO_TEXT)
        {
            unsigned char *string;

            PA_LOCK(string, unsigned char *, eptr->lo_text.text);
    		position = INTL_NextCharIdx(eptr->lo_text.text_attr->charset, 
                string, position);
    		PA_UNLOCK(eptr->lo_text.text);
        }
        else
            position++;
    }
    if ( position > length ) {
        position = length;
    }
    return position;
}

int32
lo_DecrementPosition(LO_Element* eptr, int32 position)
{
   if ( position > 0 )
   {
       if (eptr->type == LO_TEXT)
        {
            unsigned char *string;

            PA_LOCK(string, unsigned char *, eptr->lo_text.text);
    		position = INTL_PrevCharIdx(eptr->lo_text.text_attr->charset, 
                string, position);
    		PA_UNLOCK(eptr->lo_text.text);
        }
        else
            position--;
    }
    if ( position < 0 ) {
        position = 0;
    }
    return position;
}

int32
lo_GetLastCharBeginPosition(LO_Element* eptr)
{
    return lo_DecrementPosition(eptr,
        lo_GetElementLength(eptr));
}

int32
lo_GetLastCharEndPosition(LO_Element* eptr)
{
    return lo_GetElementLength(eptr) - 1;
}

void lo_HitLine(MWContext *context, lo_DocState *state, int32 x, int32 y, Bool requireCaret,
                LO_HitResult* result);
void lo_HitLine2(MWContext *context, lo_DocState *state, LO_Element* element, int32 position,
                 int32 x, LO_HitResult* result);
Bool lo_PositionIsOffEndOfLine(LO_HitElementResult* elementResult);

void lo_FullHitElement(MWContext *context, lo_DocState* state, int32 x, int32 y,
    Bool requireCaret,
    LO_Element* eptr, int32 ret_x, int32 ret_y, LO_HitResult* result);

PRIVATE
void lo_HitElement(MWContext *context, lo_DocState* state, int32 x, int32 y,
    Bool requireCaret,
    LO_Element* eptr, int32 ret_x, int32 ret_y, LO_HitResult* result)
{
    /* We hit something */
    result->type = LO_HIT_ELEMENT;
    result->lo_hitElement.position.element = eptr;
	switch ( eptr->lo_any.type )
	    {
	case LO_TEXT:
	    {
        	int32 charStart;
        	int32 charEnd;
        	result->lo_hitElement.position.position = lo_ElementToCharOffset2(context, state, eptr, ret_x,
        	    &charStart, &charEnd);
    		if (result->lo_hitElement.position.position < 0)
    		{
    			result->lo_hitElement.position.position = 0;
        	    result->lo_hitElement.region = LO_HIT_ELEMENT_REGION_BEFORE;
    		}
            else {
        		/*
        		 * eptr points to the element 
        		 * region is the part of the element
        		*/
        		if( x > eptr->lo_any.x + eptr->lo_text.width )
        		{
            	    result->lo_hitElement.region = LO_HIT_ELEMENT_REGION_AFTER;
        		}
        		else
        		{
                    int32 charMiddle = (charStart + charEnd) / 2;
                    if ( ret_x < charMiddle ) {
            	        result->lo_hitElement.region = LO_HIT_ELEMENT_REGION_BEFORE;
                    } else {
            	        result->lo_hitElement.region = LO_HIT_ELEMENT_REGION_AFTER;
                    }
                }
            }
	    }
	    break;
	default:
	    {
            int32 begin;
            int32 end;
            result->lo_hitElement.position.position = 0;
            lo_GetHorizontalBounds(eptr, &begin, &end);
            if ( requireCaret )
            {
                int32 middle = (begin + end) / 2;
                result->lo_hitElement.region =
                    ( x < middle ) ? LO_HIT_ELEMENT_REGION_BEFORE
                        : LO_HIT_ELEMENT_REGION_AFTER;
            }
            else
            {
                int32 midBegin;
                int32 midEnd;
                midBegin = begin + 3;
                midEnd = end - 3;
                if ( midEnd - midBegin < 6 ) {
                    if ( end - begin <= 6 ) {
                        /* Very narrow picture. 0..6 pixels */
                        midBegin = begin;
                        midEnd = end;
                    } else {
                        /* Narrow picture. 6+..12 pixels */
                        int32 margin = end - begin - 6;
                        int32 halfMargin = margin / 2;
                        midBegin = begin + halfMargin;
                        midEnd = end - (margin - halfMargin);
                    }
                }
                if ( x < midBegin ) {
                    result->lo_hitElement.region = LO_HIT_ELEMENT_REGION_BEFORE;
                } else if ( x < midEnd ) {
                    result->lo_hitElement.region = LO_HIT_ELEMENT_REGION_MIDDLE;
                } else {
                    result->lo_hitElement.region = LO_HIT_ELEMENT_REGION_AFTER;
                }
            }
	    }
	    break;
    }

}

void lo_FullHitElement(MWContext *context, lo_DocState* state, int32 x, int32 y,
    Bool requireCaret,
    LO_Element* eptr, int32 ret_x, int32 ret_y, LO_HitResult* result)
{
    if ( eptr->type != LO_LINEFEED ) {
        /* Seek forward to find an editable element */
        if ( ! lo_EnsureEditableSearchNext(context, state, &eptr) )
        {
            lo_EnsureEditableSearchPrev(context, state, &eptr);
        }
        lo_HitElement(context, state, x, y, requireCaret, eptr, ret_x, ret_y, result);
        /* Check if we ran off the end of the line */
        if ( result->type == LO_HIT_UNKNOWN )

#if 0 /* leads to infinite recursion  when draging around tables. */
        if ( result->type == LO_HIT_UNKNOWN 
            || result->type == LO_HIT_ELEMENT
                && lo_PositionIsOffEndOfLine(& result->lo_hitElement) )
#endif

        {
            /* XP_TRACE(("Element off end of line.")); */
            lo_HitLine(context, state, x, y, requireCaret, result);
        }
    }
    else
    {
        /* There's a bug selecting the last character of text in a table
         * that bites us if we use lo_HitLine. So rather than starting the
         * search from the beginning, we start from the linefeed.
         */
        lo_HitLine2(context, state, eptr, 0, x, result);
    }
}

PRIVATE
void lo_HitCellWideMatch(MWContext *context, lo_DocState *state, LO_CellStruct* cellPtr, int32 x, int32 y, Bool requireCaret, LO_HitResult* result)
{
#if 0
    LO_Element* eptr = lo_XYToNearestCellElement(context, state, cellPtr, x, y);
#endif
    /*
     * The last argument is FALSE so that we search upwards. This helps us in tables
     * in the editor.
     */

    LO_Element* eptr = lo_search_element_list_WideMatch(context, cellPtr->cell_list, NULL, x, y, FALSE);
    if ( eptr )
    {
        lo_FullHitElement(context, state, x, y, requireCaret, eptr, x, y, result);
    }
}

PRIVATE
void lo_HitCell(MWContext *context, lo_DocState *state, LO_CellStruct* cellPtr, int32 x, int32 y, Bool requireCaret, LO_HitResult* result)
{
    LO_Element* eptr = lo_XYToNearestCellElement(context, state, cellPtr, x, y);
    if ( eptr )
    {
        lo_FullHitElement(context, state, x, y, requireCaret, eptr, x, y, result);
    }
}

void lo_HitLine(MWContext *context, lo_DocState *state, int32 x, int32 y, Bool requireCaret,
                LO_HitResult* result)
{
    int32 line;
    result->type = LO_HIT_UNKNOWN;
    
    /*
     * Search from current line backwards to find something to edit.
     */
    for ( line = lo_PointToLine(context, state, x, y);
        line >= 0;
        line-- )
    {
        LO_Element* begin;
        LO_Element* end;
        LO_Element* tptr;
        lo_GetLineEnds(context, state, line, & begin, & end);
        /* lo_GetLineEnds returns the start of the next line for 'end' */
        if ( end ) {
            end = end->lo_any.prev;
        } else {
            /* Last line. We know that the last line only has one element. */
            end = begin;
        }
        /* Except for cases where the entire line is a line feed, don't select the end line-feed. */
        if ( begin->type != LO_LINEFEED && end->type == LO_LINEFEED ) {
            end = end->lo_any.prev;
        }

        if ( begin->type == LO_TABLE )
        {
            /* Search inside the table to find which cell/caption we hit */
            LO_Element* tptr;
			tptr = begin->lo_any.next;
			while ((tptr != NULL)&&(tptr->type == LO_CELL))
			{
                if ( tptr->lo_any.x <= x &&
                    x < tptr->lo_any.x + tptr->lo_any.width &&
                    tptr->lo_any.y <= y &&
                    y < tptr->lo_any.y + tptr->lo_any.height ) {
                    /* We hit this cell */
                    /* lo_HitCellWideMatch(context, state, (LO_CellStruct*) tptr, x, y, requireCaret, result); */
					/* Replacing call to lo_HitCellWideMatch with lo_HitCell because the former function does
					   not drill down into cell lists to find the closest document element to x,y.  This fixes
					   selection in tables.  */
					lo_HitCell(context, state, (LO_CellStruct*) tptr, x, y, requireCaret, result);
                    break;
                }
                tptr = tptr->lo_any.next;
            }
            return;
        }

        /* 
         * Loop through the elements in the line and, if any are inflow
         * layers, go into them. We bail out if we've reached the end of
         * the line (or null for the last line).
         */
        for( tptr = begin; tptr && ((tptr != end) || (begin == end)); 
             tptr = tptr->lo_any.next) {
            if ( tptr->type == LO_CELL && tptr->lo_cell.cell_inflow_layer &&
                 tptr->lo_any.x <= x &&
                 x < tptr->lo_any.x + tptr->lo_any.width &&
                 tptr->lo_any.y <= y &&
                 y < tptr->lo_any.y + tptr->lo_any.height ) {
                lo_HitCell(context, state, (LO_CellStruct *)tptr,
                           x, y, requireCaret, result);
                return;
            }
        }
        
        /* Make the end-points editable */
        if ( ! lo_EnsureEditableSearchNext(context, state, &begin) )
            continue;
        if ( ! lo_EnsureEditableSearchPrev(context, state, &end) )
            return;
        if ( begin && end && begin->lo_any.ele_id <= end->lo_any.ele_id )
        {
            result->type = LO_HIT_LINE;
            if ( x < begin->lo_any.x ) {
                result->lo_hitLine.region = LO_HIT_LINE_REGION_BEFORE;
            } else {
                result->lo_hitLine.region = LO_HIT_LINE_REGION_AFTER;
            }
            result->lo_hitLine.selection.begin.element = begin;
            result->lo_hitLine.selection.begin.position = 0;
            result->lo_hitLine.selection.end.element = end;
            if ( end->type == LO_LINEFEED )
            {
                result->lo_hitLine.selection.end.position = 0;
            }
            else
            {
                result->lo_hitLine.selection.end.position = lo_GetMaximumInsertPointPosition(end);
            }
#if 0
            XP_TRACE(("b %d e %d\n", result->lo_hitLine.begin->lo_any.ele_id,
                 result->lo_hitLine.end->lo_any.ele_id));
#endif
            break;
        }
    }
}


#ifdef MQUOTE
/* Returns true if the only elements between begin and end are bullets, not including endpoints. */
Bool lo_OnlyBulletsBetween(LO_Element *begin,LO_Element *end)
{
    /* Both begin and end should exist, and begin should be strictly before end. */
    if (!begin || !end || (begin->lo_any.ele_id >= end->lo_any.ele_id)) 
    {
        XP_ASSERT(FALSE);
        return FALSE;
    }

    /* We should never get into an infinite loop because we made sure begin was before end. */
    do {
        if (begin->lo_any.next == end)
        {
            return TRUE;
        }

        begin = begin->lo_any.next;
    } while (begin->type == LO_BULLET);

    /* Hit something that's not a bullet between begin and end. */
    return FALSE;
}
#endif

void lo_HitLine2(MWContext *context, lo_DocState *state, LO_Element* element,
                 int32 position, int32 x, LO_HitResult* result)
{
    LO_Element* begin;
    LO_Element* end;

    result->type = LO_HIT_UNKNOWN;

    end = element;

    for(;;)
    {
        /* If this is a non-editable line feed, go backwards to the previous editable line. */
        while  ( end && end->type == LO_LINEFEED
            && end->lo_linefeed.break_type == LO_LINEFEED_BREAK_SOFT)
        {
            end = end->lo_any.prev;
        }
        if ( ! end )
            return;

        /* Search forward to find the end of line */
        for ( ;
            end;
            end = end->lo_any.next)
        {
            if ( end->type == LO_LINEFEED ) break;
        }

        if ( ! end )
        {
            return;
        }
       /* Search backwards to find the beginning of the line. */
        for ( begin = end->lo_any.prev;
            begin;
            begin = begin->lo_any.prev)
        {
            if ( begin->type == LO_LINEFEED )
            {
#ifdef MQUOTE
                /* We have the case of an editable linefeed on a line by itself when the only
                   thing on the line are bullets.  I.e. from a <mquote> tag. */
                if ( lo_OnlyBulletsBetween(begin,end) )
#else
                if ( begin->lo_any.next == end )
#endif
                {
                    /* editable linefeed on a line by itself. */
                    result->type = LO_HIT_LINE;
                    if ( x < end->lo_any.x ) {
                        result->lo_hitLine.region = LO_HIT_LINE_REGION_BEFORE;
                    } else {
                        result->lo_hitLine.region = LO_HIT_LINE_REGION_AFTER;
                    }
                    result->lo_hitLine.selection.begin.element = end;
                    result->lo_hitLine.selection.begin.position = 0;
                    result->lo_hitLine.selection.end = result->lo_hitLine.selection.begin;
                    return;
                }
                begin = begin->lo_any.next;
                break;
            }
            if ( ! begin->lo_any.prev )
                break;  /* Start of document */
        }
    
        if ( ! begin )
        {
            /* Must be a line-feed at the beginning of the document */
            begin = end;
        }

    
        /* Except for cases where the entire line is a line feed, don't select the end line-feed. */
        if ( begin->type != LO_LINEFEED && end->type == LO_LINEFEED ) {
            end = end->lo_any.prev;
        }

        if ( ( begin->type == LO_TABLE ) || 
             (begin->type == LO_CELL && begin->lo_cell.cell_inflow_layer) )
        {
            /* If this is a table or an inflow layer, give up. 
               We don't understand them, yet. */
            return;
        }

        /* Make the end-points editable */
        if ( ! lo_EnsureEditableSearchNext(context, state, & begin) ) {
            /* This is the last, unselectable line of a document in edit mode.
             * Select the previous line.
             */
            if ( lo_EnsureEditableSearchPrev(context, state, & begin) ){
                lo_HitLine2(context, state, begin, 0, 0, result);
            }
            return;
        }
        if ( ! lo_EnsureEditableSearchPrev(context, state, & end) )
            return;
    
        if ( begin && end && begin->lo_any.ele_id <= end->lo_any.ele_id )
        {
            /* This is a good line */
            break;
        }
        /* At this point "end" points to the previous editable line. So try again. */
    }

    result->type = LO_HIT_LINE;
    if ( x < begin->lo_any.x ) {
        result->lo_hitLine.region = LO_HIT_LINE_REGION_BEFORE;
    } else {
        result->lo_hitLine.region = LO_HIT_LINE_REGION_AFTER;
    }
    result->lo_hitLine.selection.begin.element = begin;
    result->lo_hitLine.selection.begin.position = 0;
    result->lo_hitLine.selection.end.element = end;
    if ( end->type == LO_LINEFEED )
    {
        result->lo_hitLine.selection.end.position = 0;
    }
    else
    {
        result->lo_hitLine.selection.end.position = lo_GetMaximumInsertPointPosition(end);
    }
}


Bool
lo_PositionIsOffEndOfLine(LO_HitElementResult* elementResult)
{
    if ( elementResult->region == LO_HIT_ELEMENT_REGION_AFTER )
    {
        LO_Element* eptr = elementResult->position.element;
        int32 position = elementResult->position.position;
        if ( eptr && eptr->lo_any.next && eptr->lo_any.next->type == LO_LINEFEED )
        {
            return position >= lo_GetLastCharEndPosition(eptr);
        }
    }
    return FALSE;
}

/*
 * LO_Hit
 * Determines what semantic part of the layout was hit for a given x/y position. 
 * This is intended to be called by cursor tracking, drag & drop, etc.
 *
 */

void LO_Hit(MWContext *context, int32 x, int32 y, Bool requireCaret,
    LO_HitResult* result, CL_Layer *layer)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *eptr;
	int32 position;
	int32 ret_x, ret_y;
    LO_CellStruct *layer_cell;

#if 0
    lo_PrintLayout(context);
#endif /* DEBUG */

    result->type = LO_HIT_UNKNOWN;
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL)) {
	    return;
	}
	state = top_state->doc_state;

    layer_cell = lo_GetCellFromLayer(context, layer);
    if (layer_cell != NULL) {
        lo_HitCell(context,
                   state,
                   layer_cell,
                   x, y, requireCaret, result);
    }
    else {

    /* Clip Y to the last line of the document */
    {
        int32 endY;
	LO_Element *last_eptr;
	last_eptr = LO_getFirstLastElement(context, FALSE);
        if (last_eptr == NULL)
        {
            return;
        }
        endY = last_eptr->lo_any.y
            + last_eptr->lo_any.y_offset
            + last_eptr->lo_any.height;
        if ( y >= endY ){
            y = endY - 1;
        }
    }

    /* Clip Y to the first line of the document */
    {
        int32 startY;
	LO_Element *first_eptr;
	first_eptr = LO_getFirstLastElement(context, TRUE);
        if (first_eptr == NULL)
        {
            return;
        }
        /* Curiously, tables have an offset which excludes their captions. So don't
         * add in the offset, or else you won't be able to select the caption of a
         * table at the start of a document.
         */
        startY = first_eptr->lo_any.y;
        if ( y < startY ){
            y = startY;
        }
    }

    /* Note: Setting the first Boolean to TRUE allows you to hit-select into floating elements,
     * which basicly means into floating tables. Unfortunately, floating element ids are not numbered
     * consecutively with respect to the main document flow, which means that selections that
     * cross from below the floating element into the floating element will compare and draw wrong.
     */

	position = 0;
	eptr = lo_XYToDocumentElement2(context, state, x, y, FALSE, TRUE, TRUE, 
        TRUE, &ret_x, &ret_y);

    /* LO_DUMP_INSERT_POINT("lo_XYToDocumentElement2", eptr, 0); */

    if ( eptr ) {
        lo_FullHitElement(context, state, x, y, requireCaret, eptr, ret_x, ret_y, result);
    }
    else {
        lo_HitLine(context, state, x, y, requireCaret, result);
   }

    }

#ifdef DEBUG
    {
/*        const char* kTypes[] = { "LO_HIT_UNKNOWN", "LO_HIT_LINE", "LO_HIT_ELEMENT"};*/
        XP_ASSERT ( result->type >= LO_HIT_UNKNOWN &&  result->type <= LO_HIT_ELEMENT );
    }

#if 0
    /* Useful for debugging mouse selection, */ 
    LO_DUMP_HIT_RESULT(result);
#endif

#endif
}

void
lo_SetInsertPoint(MWContext *context, lo_TopState *top_state, LO_Element* eptr, int32 position, CL_Layer *layer)
{
    if ( EDT_IS_EDITOR(context) )
    {
#ifdef EDITOR
        LO_ASSERT_POSITION2(context, eptr, position);
        if ( ! lo_IsValidEditableInsertPoint2(context, top_state->doc_state, eptr, position) )
        {
            XP_ASSERT(FALSE);
        }
        else
        {
            EDT_SetInsertPoint( top_state->edit_buffer,
                eptr->lo_any.edit_element, 
            	eptr->lo_any.edit_offset + position,
            	position == 0);
        }
#endif
    }
    LO_StartSelectionFromElement( context, eptr, position, layer );
}

/*
 * Like LO_PositionCaret, except it selects non-text items as well.
 * Returns TRUE if the result is a selection, FALSE if the result is
 * an insertion point. (The layout level
 * does not understand the concept of insertion point.) 
 */

Bool LO_Click(MWContext *context, int32 x, int32 y, Bool requireCaret,
              CL_Layer *layer)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
    LO_HitResult result;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return FALSE;
	}
	state = top_state->doc_state;

    LO_Hit(context, x, y, requireCaret, &result, layer);

    return lo_ProcessClick(context, top_state, state, &result, requireCaret,
                           layer);
}

Bool lo_ProcessClick(MWContext *context, lo_TopState *top_state, lo_DocState *state, LO_HitResult* result, Bool requireCaret, CL_Layer *layer)
{
    switch ( result->type )
    {
    case LO_HIT_LINE:
        {
            switch ( result->lo_hitLine.region )
            {
            case LO_HIT_LINE_REGION_BEFORE:
                {
                    if ( requireCaret )
                    /* Insertion point before first element of line */
                    {
     		            LO_Element* eptr = result->lo_hitLine.selection.begin.element;
                        int32 position = result->lo_hitLine.selection.begin.position;
                        lo_SetInsertPoint(context, top_state, eptr, position, layer);
                        return FALSE;
                    }
                    else
                    {
                        /* Select the line */
                        lo_ExtendToIncludeHardBreak(context, state, & result->lo_hitLine.selection);
                        lo_SetSelection(context, & result->lo_hitLine.selection, FALSE);
                        return TRUE;
                    }
               }
                break;
            case LO_HIT_LINE_REGION_AFTER:
                {
                    /* Set the insertion point after the last element of line */
 		            LO_Element* eptr = result->lo_hitLine.selection.end.element;
                    int32 position = result->lo_hitLine.selection.end.position;
                    lo_EnsureEditableSearchPrev2(context, state, &eptr, &position);
                    lo_SetInsertPoint(context, top_state, eptr, 
                                      position, layer);
                    return FALSE;
               }
                break;
            default:
                break;
            }
        }
        break;
    case LO_HIT_ELEMENT:
        {
		    LO_Element* eptr = result->lo_hitElement.position.element;
            int32 position  = result->lo_hitElement.position.position;
            switch ( result->lo_hitElement.region )
            {
            case LO_HIT_ELEMENT_REGION_BEFORE:
                {
                    lo_SetInsertPoint(context, top_state, eptr, position, 
                                      layer);
                    return FALSE;
                }
                break;
            case LO_HIT_ELEMENT_REGION_MIDDLE:
                {
                    /* Select the element */
                    LO_StartSelectionFromElement( context, eptr, position, 
                                                  layer );
                    lo_BumpEditablePositionForward(context, state, &eptr, &position);
                    LO_ExtendSelectionFromElement( context, eptr, position, FALSE );
    	            LO_HighlightSelection(context, TRUE);
                    return TRUE;
                }
                break;
            case LO_HIT_ELEMENT_REGION_AFTER:
                {
                    lo_BumpEditablePositionForward(context, state,
		                &eptr, &position);
                    lo_SetInsertPoint(context, top_state, eptr, 
                                      position, layer);
                    return FALSE;
                }
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
    return FALSE;
}

/* Shares much functionality with lo_ProcessClick
 * If any changes made in logic above, please check
 *  this and make corresponding changes if relevant
 *  Returns the element that we will drop at and position 
 *   within this element for text data
*/
LO_Element * lo_PositionDropCaret(MWContext *pContext, int32 x, int32 y, int32 * pPosition) 
{
    LO_Element* eptr = NULL;
#ifdef EDITOR
    lo_TopState *top_state = lo_FetchTopState(XP_DOCID(pContext));
    int32 position;
    LO_HitResult result;
    lo_DocState *state;

	if ((top_state == NULL)||(top_state->doc_state == NULL)) {
	    return NULL;
	}
    state =  top_state->doc_state;

    LO_Hit(pContext, x, y, FALSE /*requireCaret*/, &result, 0);

    /* This was copied from lo_ProcessClick above
     * We want to execute most of the same logic to locate the caret position
     *  without calling lo_SetInsertPoint, which sets the regular caret
     *  and is incompatable with a selection
    */

    switch ( result.type )
    {
        case LO_HIT_LINE:
            switch ( result.lo_hitLine.region )
            {
                case LO_HIT_LINE_REGION_BEFORE:
                    /* Drop point before first element of line */
                    eptr = result.lo_hitLine.selection.begin.element;
                    position = result.lo_hitLine.selection.begin.position;
                    break;
                case LO_HIT_LINE_REGION_AFTER:
                    /* Set the drop point after the last element of line */
 		            eptr = result.lo_hitLine.selection.end.element;
                    position = result.lo_hitLine.selection.end.position;
                    lo_EnsureEditableSearchPrev2(pContext, state, &eptr, &position);
                    break;
                default:
                    break;
            }
            break;
        case LO_HIT_ELEMENT:
	        eptr = result.lo_hitElement.position.element;
            position  = result.lo_hitElement.position.position;
            
            /* Move to just past the element if after it */
            if( result.lo_hitElement.region == LO_HIT_ELEMENT_REGION_AFTER ){
                lo_BumpEditablePositionForward(pContext, state, &eptr, &position);
            }
            break;
        default:
            break;
    }

    if( eptr )
    {
        LO_ASSERT_POSITION2(pContext, eptr, position);
        if ( lo_IsValidEditableInsertPoint2(pContext, state, eptr, position) )
        {
            if( EDT_IsSelected(pContext) )
            {
                FE_DestroyCaret(pContext);
            }

            switch ( eptr->type )
            {
                case LO_TEXT:
                    FE_DisplayTextCaret( pContext, FE_VIEW,
                                &eptr->lo_text,
                                (ED_CaretObjectPosition)position );
                    break;
                case LO_IMAGE:
                    FE_DisplayImageCaret( pContext,
                                &eptr->lo_image,
                                (ED_CaretObjectPosition)position );
                    break;
                default:
                    FE_DisplayGenericCaret( pContext,
                                &eptr->lo_any,
                                (ED_CaretObjectPosition)position );
                    break;
            }
        }
        else
        {
            XP_ASSERT(FALSE);
        }
    }
    if( pPosition ){
        *pPosition = position;
    }
#endif /* EDITOR */
    return eptr;
}


/* Returns TRUE if the result is an anchor. Also selects the entire anchor. */
Bool lo_ProcessAnchorClick(MWContext *context, lo_TopState *top_state, lo_DocState *state, LO_HitResult* result)
{
    switch ( result->type )
    {
    case LO_HIT_LINE:
        {
            return FALSE;
        }
        break;
    case LO_HIT_ELEMENT:
        {
		    LO_Element* eptr = result->lo_hitElement.position.element;
            switch ( result->lo_hitElement.region )
            {
            case LO_HIT_ELEMENT_REGION_BEFORE:
            case LO_HIT_ELEMENT_REGION_AFTER:
                {
                    return lo_SelectAnchor(context, state, eptr);
                }
                break;
            case LO_HIT_ELEMENT_REGION_MIDDLE:
                {
                    return FALSE;
                }
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
    return FALSE;
}

PRIVATE
void
lo_FindStartOfParagraph(MWContext* context, lo_DocState * state, LO_Position* where, LO_Position* paragraphStart)
{
    /*
     * Search backwards to find the beginning of the paragraph. A paragraph starts
     * with the beginning of the document, or after a forced break.
     */
    LO_Element* element;
    if ( ! (where && where->element ) )
    {
        XP_ASSERT(FALSE);
        return;
    }
    
    element = where->element;

    for(;;)
    {
        LO_Element* prev = element->lo_any.prev;
        if ( ! prev ) break;
        if ( lo_IsEndOfParagraph2(context, prev, 0) )
        {
            break;
        }
        element = prev;
    }

    paragraphStart->element = element;
    paragraphStart->position = 0;
    lo_EnsureEditableSearchNext2(context, state, &paragraphStart->element, & paragraphStart->position);
}

PRIVATE
void
lo_FindEndOfParagraph(MWContext* context, lo_DocState * state, LO_Position* where, LO_Position* paragraphEnd)
{
    /*
     * Search forward to find the end of the paragraph. A paragraph ends
     * at the end of the document, or before a forced break.
     */
    LO_Element* element;
    if ( ! (where && where->element ) )
    {
        XP_ASSERT(FALSE);
        return;
    }

    element = where->element;
    for(;;) {
        LO_Element* p = element->lo_any.next;
        if ( ! p ) break;
        element = p;
        if ( lo_IsEndOfParagraph2(context, element, 0) ) break;
    }
    paragraphEnd->element = element;
    paragraphEnd->position = lo_GetElementLength(element);
    if ( paragraphEnd->element->type == LO_LINEFEED )
    {
        paragraphEnd->position = 1;
    }
    /* lo_EnsureEditableSearchPrev2(context, state, &paragraphEnd->element, & paragraphEnd->position); */
}

PRIVATE
void
lo_FindParagraph(MWContext* context, lo_DocState *state, LO_Position* where, LO_Selection* paragraph)
{
    lo_FindStartOfParagraph(context, state, where, & paragraph->begin);
    lo_FindEndOfParagraph(context, state, where, &paragraph->end);
    lo_ConvertInsertPointToSelectionEnd(context, state, &paragraph->end.element, &paragraph->end.position);
    LO_ASSERT_SELECTION(context, paragraph);
}

PRIVATE
void
lo_SelectParagraph(MWContext* context, lo_DocState *state, LO_Position* where)
{
    LO_Selection selection;
    lo_FindParagraph(context, state, where, &selection);
    lo_SetSelection(context, &selection, FALSE);
}

PRIVATE
intn pa_strcmp( PA_Block p1, PA_Block p2 )
{
	char *s1, *s2;
	intn ret;

	PA_LOCK( s1, char*, p1 );
	PA_LOCK( s2, char*, p2 );
	ret = XP_STRCMP( s1, s2 );
	PA_UNLOCK( p1 );
	PA_UNLOCK( p2 );
	return ret;
}

PRIVATE
Bool lo_AnchorsEqual( LO_AnchorData *p1, LO_AnchorData *p2 )
{
	if( pa_strcmp( p1->anchor, p2->anchor ) != 0 )
	{
		return FALSE;
	}

	if( p1->target == 0 && p2->target == 0 )
	{
		return TRUE;
	}

	if( p1->target && p2->target && pa_strcmp( p1->target, p2->target ) == 0 )
	{
		return TRUE;
	}
		
	return FALSE;
}

PRIVATE
Bool
lo_FindAnchorEdge(MWContext* context, lo_DocState *state, LO_Position* where, LO_Position* result, Bool forward)
{
    LO_Element* element = where->element;
    LO_AnchorData* goal = lo_GetAnchorData(element);
    if ( ! goal ) return FALSE;
    while(1) {
        LO_Element* next = lo_GetNeighbor(element, forward);
	    LO_AnchorData* next_anchor;
        if ( next == 0
	        	|| (next_anchor = lo_GetAnchorData(next)) == 0
    	    	|| !lo_AnchorsEqual( next_anchor,goal )) 
		{
	    	break;
		}
        element = next;
    }
    /* LINEFEED elements can have anchor information. Backup.
     */
    while ( element && element->type == LO_LINEFEED ) {
        element = lo_GetNeighbor(element, !forward);
    }
    result->element = element;
    result->position = lo_GetElementEdge(element, forward);
    return TRUE;
}

PRIVATE
Bool
lo_FindAnchor(MWContext* context, lo_DocState *state, LO_Position* where, LO_Selection* anchor)
{
    LO_Selection selection;
    Bool result = lo_FindAnchorEdge(context, state, where, & selection.begin, FALSE) &&
        lo_FindAnchorEdge(context, state, where, &selection.end, TRUE);
    if ( result ) {
        lo_ConvertInsertPointToSelectionEnd(context, state, &selection.end.element, &selection.end.position);
        LO_ASSERT_SELECTION(context, &selection);
        *anchor = selection;
    }
    return result;
}

Bool
lo_SelectAnchor(MWContext* context, lo_DocState *state, LO_Element* eptr)
{
    /* Select the entire anchor associated with eptr */
    Bool result = FALSE;
    LO_Selection selection;
    LO_Position where;
    where.element = eptr;
    where.position = 0;
    result = lo_FindAnchor(context, state, &where, &selection);
    if ( result ) {
        lo_SetSelection(context, &selection, FALSE);
    }
    return result;
}

/*
 * This should move to a public header so that the routine can be internationalized more easily.
 *
 * The word-finding algorithm assumes that characters can be divided into different classes.
 * The classes are: single, space, and grouping. There are currently only two grouping
 * classes, because that's enough for english text. Some languages (e.g. Japanese, which has
 * romanji, katakana and hirigana, might encourage us to define more grouping classes.)

 */

#define LO_CC_SINGLE 0
#define LO_CC_SPACE 1
#define LO_CC_ALPHA 2
#define LO_CC_PUNCT 3
#define LO_CC_KANJI 4
#define LO_CC_KANA  5
#define LO_CC_OTHERS 6


PRIVATE
intn
lo_CharacterClassOf(MWContext* context, lo_DocState *state, LO_Position* where)
{
    LO_Position position = *where;
    if ( ! lo_NormalizeSelectionPoint(context, state, &position.element, &position.position) )
    {
        /* We've gone off the end of the world. */
        return LO_CC_SINGLE;
    }
    switch ( position.element->type )
    {
    case LO_LINEFEED:
        return LO_CC_SPACE;
    case LO_TEXT:
        {
	    	int16 charset;
            if ( position.element->lo_text.text_len <= 0 ) {
                return LO_CC_SPACE;
            }
	    	charset = position.element->lo_text.text_attr->charset;
            XP_ASSERT(position.position < position.element->lo_text.text_len);
            if (INTL_CharSetType(charset) == SINGLEBYTE)
            {
                /* Need to handle this on a character-set basis. */
 		        char *tptr;
                char c;

		        /* To do: The lock should be outside of the inner loop. */
		        PA_LOCK(tptr, char *, position.element->lo_text.text);
                if ( tptr )
                {
                    c = tptr[position.position];
                }
                else
                {
                    XP_ASSERT(FALSE);
                    c = '\0';
                }
                PA_UNLOCK(where->element->lo_text.text);
                if ( XP_IS_SPACE(c) ) return LO_CC_SPACE;
                else if ( isalnum(c) || ((unsigned char)c > 0x7F)) return LO_CC_ALPHA;
                else return LO_CC_PUNCT;
            }
            else { 
                /* Do something smarter here. */
 		        char *tptr;
                char c;
                intn iRet;

		        /* To do: The lock should be outside of the inner loop. */
		        PA_LOCK(tptr, char *, position.element->lo_text.text);
                if ( tptr == NULL )
                {
                    XP_ASSERT(FALSE);
                    c = '\0';
                    iRet = LO_CC_SINGLE;
                }
                else
                {
                    c = tptr[position.position];
                    /* Don't know how much detail we want to do for multibyte, 
                       right now it divide into pronounce character and kanji character
                    */
                    switch (INTL_CharClass(charset, (unsigned char *)(tptr+position.position)))
                    {
                        case SEVEN_BIT_CHAR:
                            if ( XP_IS_SPACE(c) ) iRet = LO_CC_SPACE;
                            else if ( isalnum(c) ) iRet = LO_CC_ALPHA;
                            else iRet = LO_CC_PUNCT;
                            break;
                        case HALFWIDTH_PRONOUNCE_CHAR:
                        case FULLWIDTH_PRONOUNCE_CHAR:
                            iRet = LO_CC_KANA;
                            break;
                        case FULLWIDTH_ASCII_CHAR:
                            iRet = LO_CC_ALPHA;
                            break;
                        case KANJI_CHAR:
                            iRet = LO_CC_KANJI;
                            break;
                        case UNCLASSIFIED_CHAR:
                            iRet = LO_CC_OTHERS;
                            break;
                        default:
                            iRet = LO_CC_PUNCT;
                    } 
                }

                PA_UNLOCK(where->element->lo_text.text);

                return iRet;

            }
        }
        
    default:
        return LO_CC_SINGLE;
    }
}

Bool
lo_IsEdgeOfDocument2(MWContext* context, lo_DocState *state, LO_Element* element, int32 position, Bool forward)
{
    /* If we can bump the pointer, then we're not at the end of the document. */
    if ( forward )
    {
        LO_Element* next;
        if ( element->type != LO_LINEFEED
            && position < lo_GetElementLength(element) )
            return FALSE;
        next = lo_BoundaryJumpingNext(context, state, element);
        if ( next == NULL ||
            lo_BoundaryJumpingNext(context, state, next) == NULL )
            return TRUE;
    }
    else
    {
        if ( position > 0 )
            return FALSE;
    }

    {
        return ! lo_BumpEditablePosition(context, state, &element, &position, forward);
    }
}

Bool
lo_IsEdgeOfDocument(MWContext* context, lo_DocState *state, LO_Position* where, Bool forward)
{
    return lo_IsEdgeOfDocument2(context, state, where->element, where->position, forward);
}

PRIVATE
Bool
lo_TraverseElement(MWContext* context, lo_DocState *state, LO_Element** element, Bool forward)
{
    if ( ! element || ! *element ) return FALSE;
    if ( forward )
    {
        *element = lo_BoundaryJumpingNext(context, state, *element);
    }
    else
    {
        *element = lo_BoundaryJumpingPrev(context, state, *element);
    }
    return *element != NULL;
}

/* Stop for linefeeds */
PRIVATE
Bool
lo_IsLineEdge(MWContext* context, lo_DocState *state, LO_Element* element, int32 position,
    Bool skipSoftBreaks, Bool forward)
{
    LO_Element* next = element;
    int32 nextPosition  = position;
    if ( ! lo_BumpEditablePosition(context, state, &next, &nextPosition, forward) )
    {
        /* Hit end of document. That counts as a linefeed. */
        return TRUE;
    }

    /* Increment element towards next, looking for a linefeed */

    if ( next == element )
    {
        return element->type == LO_LINEFEED;
    }

    if ( next->type == LO_LINEFEED )
    {
        return TRUE;
    }

    if ( ! lo_TraverseElement(context, state, &element, forward) ) return TRUE;
    while ( next != element )
    {
        if ( element->type == LO_LINEFEED &&
            (forward ? position <= 0 : position > 0)
            && ! lo_ValidEditableElement(context, element) )
        {
            Bool hardBreak = element->lo_linefeed.break_type != LO_LINEFEED_BREAK_SOFT;
            if ( !skipSoftBreaks || hardBreak )
                return TRUE;
        }

        /* Go on to next element */
        if ( ! lo_TraverseElement(context, state, &element, forward) ) return TRUE;
    }

    /* Didn't find a linefeed yet. */
    return FALSE;
}


/* As long as there's room and where is of the target class, move forward.
 * returns with where not equal to the character class, unless edge of line or document.
 * returns TRUE if not edge of document, else false.
 */

#define LO_SO_DOCUMENT_EDGE 0
#define LO_SO_LINEFEED 1
#define LO_SO_NEW_CHARACTER_CLASS 2

PRIVATE
intn
lo_SkipOver(MWContext* context, lo_DocState *state, LO_Position* where, intn targetCharacterClass, Bool forward)
{
    while ( lo_CharacterClassOf(context, state, where) == targetCharacterClass )
    {
        /* Stop for linefeeds */
        if ( lo_IsLineEdge(context, state, where->element, where->position,
            targetCharacterClass == LO_CC_SPACE, forward) )
        {
            /* Skip to the next character */
            return LO_SO_LINEFEED;
        }
        if ( ! lo_BumpEditablePosition(context, state, &where->element, &where->position, forward) ) return LO_SO_DOCUMENT_EDGE;
    }
    return LO_SO_NEW_CHARACTER_CLASS;
}

PRIVATE
Bool
lo_MoveToNearestEdgeOfNextLine(MWContext* context, lo_DocState *state, LO_Position* where, Bool forward)
{
    LO_Position caret = *where;
    Bool bHardBreak;
    while ( ! lo_IsLineEdge(context, state, caret.element, caret.position, FALSE, forward) )
    {
        if (! lo_BumpEditablePosition ( context, state, &caret.element, &caret.position, forward ) )
            return FALSE;
    }
    bHardBreak = caret.element && caret.element->type == LO_LINEFEED
        && caret.element->lo_linefeed.break_type == LO_LINEFEED_BREAK_HARD;

    if ( bHardBreak && ! forward && caret.position == 1 )
    {
        /* This is the after-a-hard-break-and-before-a-paragraph-end case. */
        caret.position = 0;
    }
    else if ( bHardBreak && forward && caret.position == 0 && caret.element->lo_any.next
        && caret.element->lo_any.next->type == LO_LINEFEED
        && caret.element->lo_any.next->lo_linefeed.break_type == LO_LINEFEED_BREAK_PARAGRAPH )
    {
        /* This is the before-a-hard-break-before-a-paragraph-end case. */
        caret.position = 1;
    }
    else
    {
        do {
            lo_TraverseElement(context, state, &caret.element, forward);
#ifdef MQUOTE          
          } while ( caret.element && 
            ((caret.element->type == LO_LINEFEED && 
              caret.element->lo_linefeed.break_type == LO_LINEFEED_BREAK_SOFT) ||
             caret.element->type == LO_BULLET));
        /* Added code above to skip over leading bullets resulting from <mquote> */
#else       
       } while ( caret.element && caret.element->type == LO_LINEFEED
             && caret.element->lo_linefeed.break_type == LO_LINEFEED_BREAK_SOFT);
#endif


        if ( ! caret.element )
            return FALSE;
        caret.position = 0;
        if ( ! forward && caret.element ){
            if ( ! (caret.element->type == LO_LINEFEED
                && caret.element->lo_linefeed.break_type == LO_LINEFEED_BREAK_HARD ) ){
                caret.position = lo_GetMaximumInsertPointPosition(caret.element);
            }
        }
        {
            /* Bump by one position if crossing from one line of wrapped text to another.
             * This is because the editor can't represent the difference between one edge
             * and the next for wrapped text. *sigh*
             */
            if ( caret.element && where->element
                && caret.element->type == LO_TEXT && where->element->type == LO_TEXT
                && caret.element->lo_any.edit_element == where->element->lo_any.edit_element )
            {
                if (! lo_BumpEditablePosition ( context, state, &caret.element, &caret.position, forward ) )
                    return FALSE;
            }
        }
    }
    /* Skip over junk at edge of line. (For example, bullets.) */
    if ( ! lo_EnsureEditableSearch2(context, state, &caret.element, &caret.position, forward) )
    {
        /* Either the end of the document, or the end of a cell. */
#if 0
        if (! lo_BumpEditablePosition ( context, state, &caret.element, &caret.position, forward ) )
            return FALSE;
#endif
        return FALSE;
    }
    XP_ASSERT(caret.element != NULL);
    *where = caret;
    return TRUE;
}

/* Return TRUE if we actually found an edge. FALSE if we're off the start or end of the document. */

PRIVATE
Bool
lo_FindEdgeOfWord(MWContext* context, lo_DocState *state, LO_Position* where, LO_Position* wordEdge, 
				Bool bSelect, Bool forward, Bool bIncludeSpacesAtEndOfWord)
{
    /*
     * Search to find the edge of the word.
     * word = { single_word | alpha_word | punct_word | space_word }
     * single_word = LO_CC_SINGLE (e.g. chinese character for the season "autumn".)
     * alpha_word = LO_CC_ALPHA + LO_CC_SPACE * (e.g. "foo10   ")
     * punct_word = LO_CC_PUNCT + LO_CC_SPACE * (e.g. "---()   ")
     * space_word = LO_CC_SPACE + (e.g. "   ")
     */

    intn characterClass;
    *wordEdge = *where;

    /* Check for end of document */

    if ( lo_IsEdgeOfDocument(context, state, wordEdge, forward) )
    {
        lo_FindDocumentEdge(context, state, wordEdge, bSelect, forward);
        return TRUE;
    }

    if ( lo_IsLineEdge(context, state, where->element, where->position, FALSE, forward) )
    {
        return TRUE;
    }

    if ( !forward && lo_IsLineEdge(context, state, where->element, where->position, FALSE, !forward) )
    {
        /* Starting at end of line. Move backward. */
        lo_BumpEditablePosition(context, state, &wordEdge->element, &wordEdge->position, forward);
    }

    characterClass = lo_CharacterClassOf(context, state, wordEdge);
    if ( characterClass == LO_CC_SINGLE ) {
        if ( forward )
        {
            lo_BumpEditablePositionForward(context, state, &wordEdge->element, &wordEdge->position);
        }
    }
    else {
        /* Match more of this class */
        if ( ! forward )
        {
            /* Skip over spaces */
            if ( lo_SkipOver(context, state, wordEdge, LO_CC_SPACE, forward ) != LO_SO_NEW_CHARACTER_CLASS )
            {
                goto exit;
            }
            characterClass = lo_CharacterClassOf(context, state, wordEdge);
        }
        if ( characterClass != LO_CC_SPACE )
        {
            if ( lo_SkipOver(context, state, wordEdge, characterClass, forward) != LO_SO_NEW_CHARACTER_CLASS )
                goto exit;
        }
        if ( forward )
        {
        	if ( bIncludeSpacesAtEndOfWord )
        	{
	            /* Skip over spaces */
    	        if ( lo_SkipOver(context, state, wordEdge, LO_CC_SPACE, forward ) != LO_SO_NEW_CHARACTER_CLASS )
        	        goto exit;
        	}
        }
        else
        {
            /* If we didn't hit the edge of the document, we are now one element further than we want to be. So back up one position. */
            lo_BumpEditablePositionForward(context, state, &wordEdge->element, &wordEdge->position);
        }
    }

exit:
    /* The edge might be a line feed. If so, move to an editable position. */
    lo_EnsureEditableSearch(context, state, wordEdge, forward);
    return TRUE;
}

PRIVATE
Bool
lo_IsEmptyText(LO_Element* eptr)
{
    if ( eptr && eptr->type == LO_TEXT &&
        lo_GetElementLength(eptr) == 0 )
    {
        return TRUE;
    }
    return FALSE;
}

PRIVATE
Bool
lo_FindLineEdge(MWContext* context, lo_DocState *state, LO_Position* where, LO_Position* lineEdge, Bool select, Bool forward)
{
    /* Special case for hard breaks at end of paragraph */

    Bool bHardBreakAtEndOfParagraph;

    if ( lo_IsEdgeOfDocument(context, state, lineEdge, forward) )
    {
        lo_FindDocumentEdge(context, state, lineEdge, select, forward);
        return TRUE;
    }

    bHardBreakAtEndOfParagraph = lo_IsHardBreak2(context, where->element, 0)
        || (lo_IsEmptyText(where->element) && lo_IsHardBreak2(context, where->element->lo_any.next, 0));

    if ( bHardBreakAtEndOfParagraph && forward) {
        /* "where" is the correct edge */
        *lineEdge = *where;
    }
    else {
        LO_HitResult result;
        lo_HitLine2(context, state, where->element, where->position, where->element->lo_any.x, &result);
        if ( result.type != LO_HIT_LINE )
        {
            XP_ASSERT(FALSE);
            return FALSE;
        }

        if ( forward )
        {
            if ( select )
            {
                lo_ExtendToIncludeHardBreak(context, state, & result.lo_hitLine.selection);
            }
            *lineEdge = result.lo_hitLine.selection.end;
            lo_ConvertSelectionEndToInsertPoint(context, state, &lineEdge->element, &lineEdge->position);
        }
        else
        {
            *lineEdge = result.lo_hitLine.selection.begin;
        }
    }
    return TRUE;
}

Bool
lo_FindDocumentEdge(MWContext* context, lo_DocState *state, LO_Position* edge, Bool select, Bool forward)
{
	LO_Element **array;
    LO_Element* eptr = NULL;
#ifdef XP_WIN16
	XP_Block *larray_array;
#endif /* XP_WIN16 */
	/*
	 * Nothing to select.
	 */
	if (state->line_num <= 1)
	{
		return FALSE;
	}

    if ( forward )
    {
        /*
         * Here we deal with the case where the current selection is within
         * a layer. In that case, we need to find the edge of the layer.
         */
        if (state->selection_layer) {
            LO_CellStruct *layer_cell = lo_GetCellFromLayer(context,
                                                       state->selection_layer);
            
            if (layer_cell)
                eptr = layer_cell->cell_list_end;
        }

        if (!eptr)
        /*
    	 * Get last element in doc.
    	 */
            eptr = state->end_last_line;
    	if (eptr == NULL)
    	{
    		eptr = state->selection_start;
    	}

    	/*
    	 * Since the last element is always a
    	 * linefeed, it is safe to set selection_end_pos to 0.
    	 */
    	edge->element = eptr;
    	edge->position = 0;
        if ( EDT_IS_EDITOR(context) ) {
            /* Skip over end-of-document elements. */
            int32 position = 0;
            while ( eptr && eptr->type == LO_LINEFEED
                && eptr->lo_linefeed.break_type == LO_LINEFEED_BREAK_PARAGRAPH ) {
                if ( ! lo_BumpEditablePositionBackward(context, state, &eptr, &position) ) break;
            }
        	edge->element = eptr;
        	edge->position = position;
            /* Skip forward to end of paragraph. */
            while ( eptr && ! lo_IsEndOfParagraph2(context, eptr,0) ) {
                eptr = eptr->lo_any.next;
            }
            if ( eptr ) {
                edge->element = eptr;
                edge->position = select ? 1 : 0;
            }
            else {
                XP_ASSERT(FALSE);
            }
        }

    }
    else
    {
        /*
         * Here we deal with the case where the current selection is within
         * a layer. In that case, we need to find the edge of the layer.
         */
        if (state->selection_layer) {
            LO_CellStruct *layer_cell = lo_GetCellFromLayer(context,
                                                       state->selection_layer);
            
            if (layer_cell)
                eptr = layer_cell->cell_list;
        }

        if (!eptr)
        {
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
        }
         
            /*
    	 * No elements.
    	 */
    	if (eptr == NULL)
    	{
    		return FALSE;
    	}

    	edge->element = eptr;
    	edge->position = 0;
    }
    /* Backtrack to ensure editability. */
    lo_EnsureEditableSearch(context, state, edge, !forward);
    /* However, for selecting forward, we need to go over the edge. */
    if ( select && forward ) {
        if ( edge->element->lo_any.next && edge->element->lo_any.next->type == LO_LINEFEED) {
            edge->element = edge->element->lo_any.next;
            edge->position = 1;
        }
    }
    return TRUE;
}

Bool
lo_FindWord(MWContext* context, lo_DocState *state, LO_Position* where, LO_Selection* word)
{
    lo_FindEdgeOfWord(context, state, where, &word->begin, TRUE, FALSE, TRUE);
    LO_ASSERT_POSITION(context, &word->begin);
    lo_FindEdgeOfWord(context, state, where, &word->end, TRUE, TRUE, FALSE);
    if ( word->begin.element == word->end.element && word->begin.position == word->end.position ) {
        /* For some reason the result is an insert point. Perhaps there are no words in
         * this document.
         */
        return FALSE;
    }
    lo_ConvertInsertPointToSelectionEnd(context, state, &word->end.element, &word->end.position);
    LO_ASSERT_SELECTION(context, word);
    return TRUE;
}

PRIVATE
void
lo_SelectWord(MWContext* context, lo_DocState *state, LO_Position* where)
{
    LO_Selection selection;
    Bool success = lo_FindWord(context, state, where, &selection);
    if ( success ) {
        lo_SetSelection(context, &selection, FALSE);
    }
    else {
        /* No word available. */
        lo_SelectParagraph(context, state, where );
    }
}

Bool lo_ProcessDoubleClick(MWContext *context, lo_TopState *top_state, lo_DocState *state, LO_HitResult* result, CL_Layer *layer)
{
    switch ( result->type )
    {
    case LO_HIT_LINE:
        {
            switch ( result->lo_hitLine.region )
            {
            case LO_HIT_LINE_REGION_BEFORE:
                {
                    lo_SelectParagraph(context, state, & result->lo_hitLine.selection.begin );
                    return TRUE;
               }
                break;
            case LO_HIT_LINE_REGION_AFTER:
                {
                    LO_Selection selection = result->lo_hitLine.selection;
                    /* If they double clicked after the end of the document, back them up. */
                    lo_EnsureEditableSearchPrev2(context, state, &selection.end.element, &selection.end.position);
                    if ( lo_ExtendToIncludeHardBreak(context, state, &selection) )
                    {
                        /* Select the paragraph end */
                        selection.begin = selection.end;
                        lo_SetSelection(context, &selection, FALSE);
                    }
                    else
                    {
                        lo_SelectWord(context, state, &selection.end );
                    }

                    return FALSE;
               }
                break;
            default:
                break;
            }
        }
        break;
    case LO_HIT_ELEMENT:
        {
            switch ( result->lo_hitElement.region )
            {
            case LO_HIT_ELEMENT_REGION_BEFORE:
            case LO_HIT_ELEMENT_REGION_AFTER:
               {
                    if ( ! lo_SelectAnchor(context, state, result->lo_hitElement.position.element ) )
                        lo_SelectWord(context, state, &result->lo_hitElement.position );
                    return FALSE;
                }
                break;
            case LO_HIT_ELEMENT_REGION_MIDDLE:
                {
                    /* To do: Open editor on image. For now, act like click */
                    return lo_ProcessClick( context, top_state, state, result, FALSE, layer);
                }
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
    return FALSE;
}

PRIVATE
Bool lo_FindCharacterEdge(MWContext* context, lo_DocState *state, LO_Position* where, LO_Position* edge, Bool bSelect, Bool forward)
{
    *edge = *where;
    if ( forward )
    {
        if ( lo_IsEdgeOfDocument(context, state, edge, forward) )
        {
            lo_FindDocumentEdge(context, state, edge, bSelect, forward);
            return where->element != edge->element && where->position != edge->position;
        }
        return lo_BumpEditablePosition(context, state, &edge->element, &edge->position, forward);
    }
    else
    {
        /* We're already at the edge of the character. */
        return TRUE;
    }
}

PRIVATE
Bool
lo_FindChunkEdge(MWContext* context, lo_DocState *state, LO_Position* where, LO_Position* wordEdge, intn chunkType, Bool select, Bool forward)
{
    Bool result;
    *wordEdge = *where;
    switch ( chunkType )
    {
    case LO_NA_CHARACTER:
        result = lo_FindCharacterEdge(context, state, where, wordEdge, select, forward);
        break;
    case LO_NA_WORD:
        result = lo_FindEdgeOfWord(context, state, where, wordEdge, select, forward, TRUE);
        break;
    case LO_NA_LINEEDGE:
        result = lo_FindLineEdge(context, state, where, wordEdge, select, forward);
        break;
    case LO_NA_DOCUMENT:
        /* Doesn't care where we start. */
        result = lo_FindDocumentEdge(context, state, wordEdge, select, forward);
        break;
    default:
        XP_ASSERT(FALSE);
        return FALSE;
    }
    /* Did we go off the end of the world? */
    /* if ( result && ! lo_EnsureEditableSearch(context, state, wordEdge, forward) ) result = FALSE; */
    return result;
}

PRIVATE
Bool
lo_GapWithBothSidesAllowed( MWContext *context, lo_DocState *state, LO_Position* caret)
{
    LO_Position prev = *caret;
    LO_Position next = *caret;
    Bool result = FALSE;
    if ( caret->position == 0 )
    {
        result = lo_BumpEditablePosition(context, state, &prev.element, &prev.position, FALSE);
    }
    else if ( caret->position == lo_GetElementLength(caret->element) )
    {
        result = lo_BumpEditablePosition(context, state, &next.element, &next.position, TRUE);
    } 
    if ( ! result )
        return FALSE;

    /* OK, we're in the gap. */
    if ( prev.element->type == LO_HRULE 
        || next.element->type == LO_HRULE )
    {
        /* It's a gap with hrules. Now, if there isn't a hard break, we're all set. */
        LO_Element* element;
        for ( element = prev.element;
            element && element != next.element;
            element = element->lo_any.next)
        {
            if ( lo_IsHardBreak2(context, element, 0) )
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

/* For logical navigation (e.g. next / prev word, character, paragraph) this function
 * performs all the book keeping to implement extension vs. moving the cursor.
 *
 * target - the unit of navigation -- the word / character that is selected.
 * bSelect - true if the current selection is to be extended.
 * bDeselecting - true if there used to be a selection, but it's going away.
 * bForward - true if the end of the target is to be used.
 *
  */

/* Compute new position
 */

Bool
LO_ComputeNewPosition( MWContext *context, intn chunkType,
    Bool bSelect, Bool bDeselecting, Bool bForward,
    LO_Element** pElement, int32* pPosition )
{
	lo_TopState* top_state;
	lo_DocState *state;
    LO_Position wordEdge;
    LO_Position caret;
    Bool result = TRUE;

    /* LO_DUMP_SELECTIONSTATE(context); */

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}
	state = top_state->doc_state;

    if ( ! pElement || ! pPosition )
        return FALSE;

    caret.element = *pElement;
    caret.position = *pPosition;
    wordEdge = caret;

    switch ( chunkType )
    {
    case LO_NA_WORD:
        {
            Bool isLineEdge = lo_IsLineEdge(context, state, caret.element, caret.position, FALSE, bForward);
            Bool isHardBreak = lo_IsLineEdge(context, state, caret.element, caret.position, TRUE, bForward);
            if ( isLineEdge && bDeselecting ) break;

            if ( lo_IsEdgeOfDocument(context, state, &caret, bForward) )
            {
                lo_FindDocumentEdge(context, state, &caret, bSelect, bForward);
                wordEdge = caret;
            }
            else
            {
                if ( isLineEdge )
                {
                    Bool bOverEdge = caret.element->type == LO_LINEFEED && caret.position > 0;
                    lo_MoveToNearestEdgeOfNextLine(context, state, &caret, bForward);
                    wordEdge = caret;
                    if ( isHardBreak )
                    {
                        if ( bSelect && bOverEdge && bForward )
                        {
                            /* If the next line is a single line feed. */
                            if ( lo_GetElementLength(caret.element) == 0 )
                            {
                                lo_BumpEditablePosition(context, state, &caret.element, &caret.position, bForward);
                                wordEdge = caret;
                            }
                            else
                            {
                                lo_BumpEditablePosition(context, state, &caret.element, &caret.position, bForward);
                                lo_FindChunkEdge(context, state, &caret, &wordEdge, chunkType, bSelect, bForward);
                            }
                        }
                    }
                    else
                    {
                        lo_FindChunkEdge(context, state, &caret, &wordEdge, chunkType, bSelect, bForward);
                    }
                }
                else {
                    if ( isLineEdge && ! bSelect )
                    {
                        if ( isHardBreak )
                        {
                            wordEdge = caret;
                        }
                        else
                        {
                            lo_FindChunkEdge(context, state, &caret, &wordEdge, chunkType, bSelect, bForward);
                        }
                    }
                    else
                    {
                        /* If we're at the beginning of a word, and we are searching backwards,
                         * we want the word before the word we're currently on.
                         */
                        if ( ! bDeselecting && !isLineEdge && !bForward )
                        {
                            lo_BumpEditablePosition(context, state, &caret.element, &caret.position, bForward);
                        }
                        lo_FindChunkEdge(context, state, &caret, &wordEdge, chunkType, bSelect, bForward);
                    }
                }
            }
        }
        break;
    case LO_NA_CHARACTER:
        {
            Bool isLineEdge;
            /* Special rule: If navigating by unselected character, and there is a selection, then the
             * result is the edge in the direction of travel.
             */
            if ( bDeselecting )
            {
                break;
            }

           isLineEdge = lo_IsLineEdge(context, state, caret.element, caret.position, FALSE, bForward);
           if ( isLineEdge )
           {
                Bool bDoubleGap = lo_GapWithBothSidesAllowed(context, state, &caret);
                result = lo_MoveToNearestEdgeOfNextLine(context, state, &caret, bForward);
                if ( ! result && bSelect && bForward )
                {
                    /* End of document case. Allowed to select the paragraph. */
                    lo_FindDocumentEdge(context, state, &caret, TRUE, TRUE);
                    result = TRUE;
                }
                else if ( ! result )
                {
                    return FALSE;
                }
                else
                {
                  /* If we are shift-selecting an hrule, bump an extra position,
                     because the pseudo-position before and after an hrule doesn't
                     really exist. */
                    if ( result && bSelect && bDoubleGap )
                    {
                        result = lo_BumpEditablePosition(context, state, &caret.element, &caret.position, bForward);
                        if ( ! result )
                        {
                            /* End of document */
                            lo_FindDocumentEdge(context, state, &caret, bSelect, bForward);
                            result = TRUE;
                        }
                    }
                }

                result = TRUE;    /* Bogus -- should remove. */
                wordEdge = caret;
           }
           else
           {
                /* If we are searching backwards,
                 * we want the character before the character we're currently on.
                 */
                result = TRUE;
                if ( !bForward )
                {
                    result = lo_BumpEditablePosition(context, state, &caret.element, &caret.position, bForward);
                }
                if ( result )
                {
                    result = lo_FindChunkEdge(context, state, &caret, &wordEdge, chunkType, bSelect, bForward);
                }
            }
        }
        break;

    default:
        {
            result = lo_FindChunkEdge(context, state, &caret, &wordEdge, chunkType, bSelect, bForward);
        }
        break;
    }

    result = result && (bDeselecting || wordEdge.element != *pElement || wordEdge.position != *pPosition);
    if ( result ){
        *pElement = wordEdge.element;
        *pPosition = wordEdge.position;
    }
    return result;
}

/*
 * Find the next or previous word and possibly extend the selection.
 */
Bool
LO_NavigateChunk( MWContext *context, intn chunkType, Bool bSelect, Bool bForward )
{
	lo_TopState* top_state;
	lo_DocState *state;
    LO_Position caret;
    LO_Element *element;
    int32 position;
    Bool bSelectionExists;
    Bool bDeselecting;
    Bool bResult;

    /* LO_DUMP_SELECTIONSTATE(context); */

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}
	state = top_state->doc_state;

    bSelectionExists = state->selection_new == NULL;
    bDeselecting = bSelectionExists && ! bSelect;
    if ( bSelectionExists )
    {
        /* There is no insert point. Use the extension point. */
        if ( bSelect )
        {
            lo_GetExtensionPoint(context, state, &element, &position);
        }
        else
        {
            lo_GetSelectionEdge(context, state, &element, &position, bForward);
        }
    }
    else
    {
        element = state->selection_new;
        position = state->selection_new_pos;
    }

    bResult = LO_ComputeNewPosition(context, chunkType, bSelect, bDeselecting, bForward, &element, &position);
    if ( bResult ) {
        caret.element = element;
        caret.position = position;
        if ( bSelect )
        {
            lo_ExtendSelectionToPosition2(context, top_state, state, caret.element, caret.position);
        }
        else
        {
            if ( ! lo_EnsureEditableSearch(context, state, &caret, bForward) ) {
                /* Off the edge of the world. */
                lo_EnsureEditableSearch(context, state, &caret, !bForward);
            }
            lo_SetInsertPoint(context, top_state, caret.element, caret.position, state->selection_layer);
        }
    }

    return bResult;
}

void LO_GetEffectiveCoordinates( MWContext *pContext, LO_Element *pElement, int32 position,
     int32* pX, int32* pY, int32* pWidth, int32* pHeight )
{
    *pY = pElement->lo_any.y + pElement->lo_any.y_offset;
    *pX = pElement->lo_any.x + pElement->lo_any.x_offset;
    *pWidth = pElement->lo_any.width;
    *pHeight = pElement->lo_any.height;
    switch ( pElement->type )
    {
    case LO_IMAGE:
        *pWidth += 2 * pElement->lo_image.border_width;
        *pHeight += 2 * pElement->lo_image.border_width;
        if ( position > 0 ) {
            *pX = *pX + *pWidth;
        }
        break;
    case LO_TEXT:
        {
            int32 start = LO_TextElementWidth( pContext, (LO_TextStruct*)pElement, 
                            position);
            *pX += start;
        }
        break;
    case LO_HRULE:
        /* The effective y and height is the height of the following linefeed. */
        {
            LO_Element* pNext = pElement->lo_any.next;
            if ( pNext && pNext->type == LO_LINEFEED )
            {
                int32 dummyWidth;
                int32 dummyX;
                LO_GetEffectiveCoordinates(pContext, pNext, 0, &dummyX, pY, &dummyWidth, pHeight);
            }
            if ( position > 0 ) {
                *pX = *pX + *pWidth;
            }
        }
        break;
    case LO_LINEFEED:
        {
            if ( position > 0 )
            {
                /* At start of next element. */
                LO_Element* pNext = pElement->lo_any.next;
                if ( pNext )
                { /* Just one level of recursion, since position = 0 */
                    LO_GetEffectiveCoordinates(pContext, pNext, 0, pX, pY, pWidth, pHeight);
                }
            }
        }
        break;
    default:
        break;
    }
}

PRIVATE
Bool lo_FindClosestUpDown(MWContext *pContext, lo_DocState* state, int32 x, int32 y,
                          Bool bForward, int32* ret_x, int32* ret_y)
{
    int32 iLine;
    Bool bFound = FALSE;
	lo_TopState* top_state = lo_FetchTopState(XP_DOCID(pContext));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return FALSE;
	}

    /* find the line we are currently on */
    iLine = lo_PointToLine( pContext, state, x, y );

    if ( bForward )
    {
        int32 maxLine;
        /* Down. MaxLine is - 2 for the lines data structure */
        maxLine = state->line_num - 2;
        if ( EDT_IS_EDITOR(pContext) && top_state->doc_state == state )
        {
            maxLine -= 2; /* and -2 for the phantom end-of-doc hrule*/
        }
    	for( ;
            iLine <= maxLine  && !bFound;
            iLine++)
    	{
    		bFound = lo_FindBestPositionOnLine( pContext, state, iLine, x, y, bForward, ret_x, ret_y );
    	}
    }
    else
    {
        /* Up */ /*changed iline comparisson to include 0.  is this correct?  uparrow was not working*/
    	for( ;
            iLine >= 0 && !bFound;
            --iLine)
    	{
    		bFound = lo_FindBestPositionOnLine( pContext, state, iLine, x, y, bForward, ret_x, ret_y );
    	}
    }
    return bFound;
}


/*
 * Find a element up from this and set the cursor there.
 */
void LO_UpDown( MWContext *pContext, LO_Element *pElement, int32 position, int32 iDesiredX, Bool bSelect, Bool bForward )
{
#ifdef EDITOR
	lo_TopState* top_state;
	lo_DocState *state;
	int32 x, y, width, height;
	Bool bFound = FALSE;
    LO_Element *pNext;/*used to look ahead for correct y*/
    /*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(pContext));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

    /* Special case. If the element is a linefeed and the position is 1,
     * we have to move the cursor to the next element.
     */
    if ( pElement && pElement->type == LO_LINEFEED && position > 0 ) {
        LO_Element* pNext = pElement->lo_linefeed.next;
        if ( pNext ) {
            pElement = pNext;
            position = 0;
        }
    }

    {
        LO_Position p;
        p.element = pElement;
        p.position = position;
        if ( lo_IsEdgeOfDocument(pContext, state, &p, bForward) )
        {
            LO_NavigateChunk( pContext, LO_NA_DOCUMENT, bSelect, bForward);
            return;
        }
    }

    /* Find the effective coordinate of our current position */
    LO_GetEffectiveCoordinates(pContext, pElement, position, &x, &y, &width, &height);

    /*  Skip over this element */
    pNext=NULL; /*if not forward, let the old way handle it.*/
    if (bForward)
    {
        pNext=pElement->lo_any.next;
        while (pNext && (pNext->type!=LO_CELL) && (!lo_ValidEditableElement(pContext,pNext) || (pNext->lo_any.y == pElement->lo_any.y)) )
            pNext=pNext->lo_any.next;
        /*if we find a cell, we will need more work.*/
        if (pNext)
            y=pNext->lo_any.y;
    }
    if (!pNext)
    {
        if ( bForward) {
            y = pElement->lo_any.y + pElement->lo_any.line_height;
        }
        else {
            y = pElement->lo_any.y - 1;
        }
    }

    bFound = lo_FindClosestUpDown(pContext, state, iDesiredX, y, bForward, &x, &y);

	/* We found a text element on this line.  Now, lets re-scan the line for 
	 *	the best fit.
	*/
	if( bFound ){
		if( bSelect ){
			EDT_ExtendSelection( pContext, x, y );
			EDT_EndSelection( pContext, x, y );
		}
		else {
			LO_PositionCaret( pContext, x, y, NULL );
		}
	}
    else {
        /* No more elements. */
        /* Navigate to the edge of the document. */
        LO_NavigateChunk( pContext, LO_NA_DOCUMENT, bSelect, bForward);
    }
#endif
}

/* like lo_search_element_list, but matches nearest X */
LO_Element *
lo_search_element_list_WideMatch(MWContext *context, LO_Element *eptr, LO_Element* eEndPtr, int32 x, int32 y,
                                 Bool bForward)
{
    LO_Element* pFound = NULL;
    int32 bestDistanceX = 2000000;
    int32 bestDistanceY = 2000000;
    int32 distanceY;

	for ( ;
        eptr != eEndPtr && eptr != NULL;
		eptr = eptr->lo_any.next
        )
	{
		int32 width, height;

		if( eptr->type != LO_CELL
            && eptr->type != LO_TABLE
            && ! lo_ValidEditableElementIncludingParagraphMarks(context, eptr)){
            continue;
        }
        width = eptr->lo_any.width;
		/*
		 * Images need to account for border width
		 */
		if (eptr->type == LO_IMAGE)
		{
			width = width + (2 * eptr->lo_image.border_width);
		}
		if (width <= 0)
		{
			width = 1;
		}

		height = eptr->lo_any.height;
		/*
		 * Images need to account for border height
		 */
		if (eptr->type == LO_IMAGE)
		{
			height = height + (2 * eptr->lo_image.border_width);
		}

        distanceY = bestDistanceY + 1;
        if ( y < eptr->lo_any.y ) {
            /* This element is below the target. Consider it if we're searching forward. */
            if ( bForward ) {
                distanceY = eptr->lo_any.y - y;
            }
        }
        else if ( eptr->lo_any.y + eptr->lo_any.y_offset + height <= y ) {
            /* This element is above the target. Consider it if we're searching backwards. */
            if ( ! bForward ) {
                distanceY = y - (eptr->lo_any.y + eptr->lo_any.y_offset + height) + 1;
            }
        }
        else {
            distanceY = 0;
        }
        if ( distanceY <= bestDistanceY ) {
                if ( distanceY < bestDistanceY ) {
                    /* We have a new winner in Y, so forget our X */
                    bestDistanceX = 2000000;
                    bestDistanceY = distanceY;
                }
                
                
			/* We're looking for the closest match */
            if ((x < (eptr->lo_any.x + eptr->lo_any.x_offset +
				width))&&(x >= eptr->lo_any.x))
			{
                /* Can't get closer than containing. */
                pFound = eptr;
                bestDistanceX = 0;
                if ( distanceY == 0 ) {
                    break;
                }
			}
            else {
                int32 distance;
                if ( x < eptr->lo_any.x ) {
                    distance = eptr->lo_any.x - x;
                }
                else {
                    distance = x - (eptr->lo_any.x + eptr->lo_any.x_offset +
				        width) + 1;
                }
                if ( distance < bestDistanceX ) {
                    pFound = eptr;
                    bestDistanceX = distance;
                }
            }
		}
        /* Skip over cells while searchig for the closest item */
        if (eptr->type == LO_TABLE) {
            while ( eptr->lo_any.next && eptr->lo_any.next->type == LO_CELL )
            {
                eptr = eptr->lo_any.next;
            }
        }
	}
	return pFound;
}

PRIVATE
Bool lo_FindClosestUpDown_SubDoc(MWContext *pContext, lo_DocState* state,
                                 LO_SubDocStruct* pSubdoc, int32 x, int32 y,
                          Bool bForward, int32* ret_x, int32* ret_y)
{
    /* Cribbed from the end of lo_XYToDocumentElement2 */
	int32 new_x, new_y;

	new_x = x - (pSubdoc->x + pSubdoc->x_offset +
		pSubdoc->border_width);
	new_y = y - (pSubdoc->y + pSubdoc->y_offset +
		pSubdoc->border_width);
    return lo_FindClosestUpDown(pContext, (lo_DocState *)pSubdoc->state, new_x, new_y, bForward, ret_x, ret_y);
}

Bool lo_FindClosestUpDown_Cell(MWContext *pContext, lo_DocState* state, 
                               LO_CellStruct* cell, int32 x, int32 y,
                          Bool bForward, int32* ret_x, int32* ret_y)
{
    /* Cells don't have line arrays. So we linearly search for our element.
     * This code was inspired by the code in lo_search_element_list and
     * lo_XYToCellElement
     */
	LO_Element *eptr;
	LO_Element *element;

	eptr = cell->cell_list;
	element = lo_search_element_list_WideMatch(pContext, eptr, NULL, x, y, bForward);
	if (element == NULL)
	{
		eptr = cell->cell_float_list;
		element = lo_search_element_list_WideMatch(pContext, eptr, NULL, x, y, bForward);
	}
	if ((element != NULL)&&(element->type == LO_SUBDOC))
	{
		return lo_FindClosestUpDown_SubDoc(pContext, state, (LO_SubDocStruct *)element,
            x, y, bForward, ret_x, ret_y);
	}
	else if ((element != NULL)&&(element->type == LO_CELL))
	{
		return lo_FindClosestUpDown_Cell(pContext, state,
			(LO_CellStruct *)element, x, y,
			bForward, ret_x, ret_y);
	}
	else if ((element != NULL)&&(element->type == LO_TABLE))
	{
		if ( lo_FindBestPositionInTable(pContext, state,
			(LO_TableStruct *)element, x, y,
			bForward, ret_x, ret_y) )
            return TRUE;
        else {
            /* no more stuff in the table. Skip out of the table and try again. */
            if ( bForward ) {
                y = element->lo_table.y + element->lo_table.y_offset + element->lo_table.height;
            }
            else {
                y = element->lo_table.y - 1;
            }
            /* Tail recursion. Maybe we should use goto? */
            return lo_FindClosestUpDown_Cell(pContext, state, cell, x, y, bForward, ret_x, ret_y);
        }
	}
    else if (element != NULL) {
        /* Return this element's Y. */
        int32 eHeight;
        int32 eWidth;
        int32 eX;
        int32 eY;
        LO_GetEffectiveCoordinates(pContext, element, 0, &eX, &eY, & eWidth, &eHeight);
        /* Clip x to be inside the bounds of the element */
        if ( x >= eX + eWidth ) x = eX + eWidth - 1;
        if ( x < eX ) x = eX;
        *ret_x = x;
        *ret_y = eY;
        return TRUE;
    }
    else
        return FALSE;
}

PRIVATE
LO_CellStruct* lo_FindBestCellInTable(MWContext *context, lo_DocState* state, LO_TableStruct* pTable, int32 iDesiredX, 
			int32 iDesiredY, Bool bForward )
{
    /* Find end of table */
    LO_Element* pClosest = NULL;
    LO_CellStruct* result = NULL;
    if ( pTable ) {
        LO_Element* pStart = pTable->next;
        LO_Element* pEnd = pStart;
        while ( pEnd && pEnd->type == LO_CELL ) {
            pEnd = pEnd->lo_any.next;
        }

        if ( pStart ) {
            pClosest = lo_search_element_list_WideMatch(context, pStart, pEnd, iDesiredX, iDesiredY, bForward);

            if ( pClosest && pClosest->type == LO_CELL ) {
                return result = &pClosest->lo_cell;
            }
        }
    }
    return result;
}


Bool lo_FindBestPositionInTable(MWContext *context, lo_DocState* state, LO_TableStruct* pTable, int32 iDesiredX, 
			int32 iDesiredY, Bool bForward, int32* pRetX, int32 *pRetY )
{
    /* Up/Down can skip over cell boundaries. So we may have to search in two cells, the one
     * we started in, and the next one in the direction we're searching.
     * For simplicity, we search a cell, and if we don't hit anything, we bump the
     * y coordinate by the cell height, and search again.
     */

    while(1) {
        LO_CellStruct* pCell = lo_FindBestCellInTable(context, state, pTable, iDesiredX, iDesiredY, bForward);
        if ( ! pCell ) return FALSE;
        if ( lo_FindClosestUpDown_Cell(context, state, pCell, iDesiredX, iDesiredY, bForward, pRetX, pRetY) ) {
            return TRUE;
        }
        /* Search the next cell */
        if ( ! bForward ) {
            iDesiredY = pCell->y - 1;
        }
        else {
            iDesiredY = pCell->y + pCell->y_offset + pCell->height;
        }
    }
}

Bool lo_FindBestPositionOnLine(MWContext *context, lo_DocState* state, int32 iLine, int32 iDesiredX, 
			int32 iDesiredY, Bool bForward, int32* pRetX, int32 *pRetY )
{
	LO_Element **line_array;
	LO_Element *pElement, *pEnd;
    LO_Element *pFound = NULL;
	Bool bDone = FALSE;
	
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);

	if( iLine == state->line_num - 2 ){
		pEnd = 0;
	}
	else {
		pEnd = line_array[iLine+1];
	}

    pElement = line_array[iLine];

	XP_UNLOCK_BLOCK(state->line_array);

    if ( pElement->type == LO_TABLE ) {
        return lo_FindBestPositionInTable(context, state, & pElement->lo_table, iDesiredX, iDesiredY, bForward, pRetX, pRetY);
    }

	for( ;
        pElement != pEnd && !bDone;
        pElement = pElement->lo_any.next){

		if( lo_ValidEditableElementIncludingParagraphMarks(context, pElement)){
			if( pFound ){
				/* what is desired is before the element, take the absolute
				 *  value of the distance between the two elements
				 */
				if( iDesiredX < pElement->lo_any.x ){
					if( pElement->lo_any.x - iDesiredX < iDesiredX - (*pRetX) ){
						*pRetX = pElement->lo_any.x;
						*pRetY = pElement->lo_any.y;
                        pFound = pElement;
					}
					bDone = TRUE;
				}
				else if( iDesiredX < pElement->lo_any.x + pElement->lo_any.width ) {
					*pRetY = pElement->lo_any.y;
					*pRetX = iDesiredX;
                    pFound = pElement;
					bDone = TRUE;
				}
				else {
					*pRetX = iDesiredX;
					/* *pRetX = pElement->lo_any.x + pElement->lo_any.width; */
					*pRetY = pElement->lo_any.y;
                    pFound = pElement;
				}
			}
			else {
				if( iDesiredX < pElement->lo_any.x ){
					*pRetX = pElement->lo_any.x;
					*pRetY = pElement->lo_any.y;
					bDone = TRUE;
				}
				else if( iDesiredX < pElement->lo_any.x + pElement->lo_any.width ) {
					*pRetY = pElement->lo_any.y;
					*pRetX = iDesiredX;
					bDone = TRUE;
				}
				else {
					*pRetX = iDesiredX;
					/* *pRetX = pElement->lo_any.x + pElement->lo_any.width; */
					*pRetY = pElement->lo_any.y;
				}
                pFound = pElement;
			}
		}
	}

    if ( pFound && pFound->type == LO_CELL ) {
        return lo_FindClosestUpDown_Cell(context, state, &pFound->lo_cell, iDesiredX, iDesiredY, bForward, pRetX, pRetY);
    }
    else if ( pFound && pFound->type == LO_SUBDOC ) {
        return lo_FindClosestUpDown_SubDoc(context, state, &pFound->lo_subdoc, iDesiredX, iDesiredY, bForward, pRetX, pRetY);
    }
	return pFound != NULL;
}

ED_Buffer* LO_GetEDBuffer( MWContext *context)
{
#ifdef EDITOR
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
		return NULL;
	}
	return top_state->edit_buffer;
#else
	return 0;
#endif
}

/* #endif */

/* 
 * Given a Text element and a character offset, find the width of upto that
 *	offset.
*/
int32
LO_TextElementWidth(MWContext *context, LO_TextStruct *text_ele, int charOffset)
{
	LO_TextInfo text_info;
	int orig_len;
	int32 retval;

	if ((text_ele->text == NULL)||(text_ele->text_len <= 0))
	{
		return(0);
	}

	orig_len = text_ele->text_len;
	text_ele->text_len = charOffset;
	FE_GetTextInfo(context, text_ele, &text_info);
	retval =  text_info.max_width;
	text_ele->text_len = orig_len;
	return retval;
}

#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

#ifdef PROFILE
#pragma profile off
#endif

/* #ifndef NO_TAB_NAVIGATION  */

/* 
Arthur Liu, 9/13/96
  Reading through this file, I found LO_getDocState() should
  be a separate function.

  TODO, check this file(may be also other files), to replace
  the duplicated code with this function.
*/
PRIVATE
lo_DocState *
LO_getDocState(MWContext *context)
{
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
		return NULL ;
	}
	return( top_state->doc_state );
}

/* 
Arthur Liu, 9/13/96
  LO_getFirstLastElement() was created for Keyboard Navigation. 
  
	TODO:  Its function is a subset of lo_FindDocumentEdge(..), and the 
	latter may call LO_getFirstLastElement() to avoid code duplication.
*/

LO_Element * 
LO_getFirstLastElement(MWContext *context, int forward )
{
	lo_DocState *state;
	LO_Element **array;
    LO_Element* eptr;
#ifdef XP_WIN16
	XP_Block *larray_array;
#endif /* XP_WIN16 */
    
	if( NULL == ( state = LO_getDocState(context) ) )
		return( NULL );

	/*
	 * Nothing to select.
	 */
	if (state->line_num <= 1)
	{
		return NULL ;
	}

    if ( ! forward )
    {
        /*
    	 * Get last element in doc.
    	 */
    	eptr = state->end_last_line;
    	if (eptr == NULL)
    	{
    		eptr = state->selection_start;		/* ??? */
    	}
    }
    else	/* wantFirst */
    {
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
    	/*
    	 * No elements.
    	 */
    	if (eptr == NULL)
    	{
    		return NULL ;
    	}

    }
    return( eptr );
}	/* LO_getFirstLastElement() */

/*
 return TRUE if pCurrentFocus->pElement is a tabable element,
 may also fill in pCurrentFocus->pAnchor, and pCurrentFocus->mapAreaIndex
*/
Bool 
LO_isTabableElement(MWContext *context, LO_TabFocusData *pCurrentFocus  )
{
	/*
		For all LO_ types which update status bar, see CWinCX::OnMouseMoveForLayerCX(UINT uFlags, CPoint& cpPoint,
		for all links see CWinCX::OnLButtonUpForLayerCX(UINT uFlags, CPoint& cpPoint, XY& Point,
	*/
	Bool					bIsTabable;
	LO_TextStruct			*pText;
	LO_ImageStruct			*pImage;
	lo_MapAreaRec			*tempArea;
	LO_Element				*pElement;
	LO_FormElementStruct	*formEleStruct;

	if( pCurrentFocus == NULL )
		return( FALSE );

	pCurrentFocus->mapAreaIndex = 0;			/* no area */
	pCurrentFocus->pAnchor		= NULL;

	pElement = pCurrentFocus->pElement;
	if( pElement == NULL )
		return( FALSE );

	bIsTabable = FALSE;

    switch( pElement->type ) {
	
	case LO_NONE :		/*     0  */
		break;
	case LO_TEXT :		/*     1  */
		pText = (LO_TextStruct *) pElement;
		if( pText )
			bIsTabable = (pText->anchor_href && pText->anchor_href->anchor);
		if( bIsTabable )
			pCurrentFocus->pAnchor = pText->anchor_href;
		break;

	case LO_LINEFEED :	/*     2  */
		break;
	case LO_HRULE :		/*     3  */
		break;
	case LO_IMAGE :		/*     4  */
		pImage = ( LO_ImageStruct *) pElement;
		
		/* first test image with a link */
		bIsTabable  = (pImage->anchor_href != NULL) && (pImage->anchor_href->anchor != NULL);
		if( bIsTabable ) {
			pCurrentFocus->pAnchor = pImage->anchor_href;
			break;
		}

		/* test use map */
		tempArea = LO_getTabableMapArea(context, pImage, 1 ); /* 1 means get the first */
		if( tempArea != NULL ) {
			pCurrentFocus->mapAreaIndex = 1;			/* the first area */
			pCurrentFocus->pAnchor		= tempArea->anchor;
			bIsTabable = TRUE;
		}
		break;
	case LO_BULLET :	/*     5 */
		break;
	case LO_FORM_ELE :	/*     6 */
		/*TODO get anchor for buttons...*/
		formEleStruct = (LO_FormElementStruct *) pElement;
		if( formEleStruct )
			bIsTabable = LO_isTabableFormElement( formEleStruct );
		break;

	case LO_SUBDOC :	/*     7  */
		break;
	case LO_TABLE :		/*     8  */
		break;
	case LO_CELL :		/*     9  */
		break;
	case LO_EMBED :		/*    10  */
		break;
	case LO_EDGE :		/*    11  */
		break;
#ifdef JAVA
	case LO_JAVA :		/*    12  */
		break;
#endif
	case LO_SCRIPT :	/*    13  */
		break;
	default :			/* something wrong!!  */
			bIsTabable = FALSE;
		break;
	}
	return( bIsTabable );
}


/*
	get first(last) if current element is NULL.
*/
Bool
LO_getNextTabableElement( MWContext *context, LO_TabFocusData *pCurrentFocus, int forward )
{

	lo_DocState		*state;
	LO_Element		*currentElement;			
	lo_MapAreaRec	*tempArea;
	LO_AnchorData	*pLastAnchorData;

	if( NULL == ( state = LO_getDocState(context) ) )
		return( FALSE );

	/*
		A text may have been fragmented into multiple lines because the text folding.
		If last tab focus is a LO_Text, remember its anchor to skip continuious text fragments.
	
		When the focus is drawn, all continuious fragments will hav efocus, See
		CWinCX::setTextTabFocusDrawFlag() in cmd\winfe\cxwin.cpp file.
	*/

	pLastAnchorData = NULL;		/* assuming no need to skip continuious text fragments.  */
		
	currentElement = pCurrentFocus->pElement;
	/* find the first candidate,   */
	if( currentElement == NULL ) {
		/* find the first(last) element in the Doc as candidate,   */
		pCurrentFocus->pElement = LO_getFirstLastElement( context, forward );		/* forward is getting first  */

		/*TODO go down to cell_list if pElement has subtree  */

	} else {
		if( pCurrentFocus->pElement->lo_any.type == LO_TEXT )
			pLastAnchorData = pCurrentFocus->pAnchor;	/* need to skip continuious text fragments.  */
		
		/* if the last element is a image map, try go to next area in the map  */
		if( currentElement->lo_any.type == LO_IMAGE ) {
				if( forward ) {
				pCurrentFocus->mapAreaIndex++;
				tempArea = LO_getTabableMapArea(context, (LO_ImageStruct *)currentElement, pCurrentFocus->mapAreaIndex );
				if( tempArea ) {
					pCurrentFocus->pAnchor = tempArea->anchor;
					return( TRUE );
				}
			} else {	/* if( forward )   */
				pCurrentFocus->mapAreaIndex--;
				if( pCurrentFocus->mapAreaIndex > 0 ) {	/* otherwise no need to search.  */
					tempArea = LO_getTabableMapArea(context, (LO_ImageStruct *)currentElement, pCurrentFocus->mapAreaIndex );
					if( tempArea ) {
						pCurrentFocus->pAnchor = tempArea->anchor;
						return( TRUE );
					}
				}
			}
			
		}
		
		/* advance the pointer, use the next(previous) as first candidate.
			pElement = lo_GetNeighbor( currentElement, forward);
			pElement = currentElement;
		*/
		lo_TraverseElement(context, state, &(pCurrentFocus->pElement), forward);
	}

	/*todo if not forward, get last area.  */
	pCurrentFocus->mapAreaIndex = 0;			/* assumming no focus area  */
	pCurrentFocus->pAnchor		= NULL;
	while ( pCurrentFocus->pElement != NULL ) {
		if( LO_isTabableElement(context, pCurrentFocus) )  {

			if( pLastAnchorData == NULL		/* no need to skip continuious text fragments.  */
				|| pCurrentFocus->pElement->lo_any.type != LO_TEXT 
				|| pLastAnchorData != pCurrentFocus->pAnchor )
				return( TRUE );
			/* else it is a text fragment with the same anchor as the last tabFocus text.
			 In such case, continue search.
			*/
		}
		
		lo_TraverseElement(context, state, &(pCurrentFocus->pElement), forward);
	}

	return( FALSE );
}	/* LO_getNextTabableElement  */

/* NO_TAB_NAVIGATION */



