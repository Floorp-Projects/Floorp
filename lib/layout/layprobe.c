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

/* Created by: Nisheeth Ranjan (nisheeth@netscape.com), April 30, 1998 */

/* 
 * Implements the abstraction of a layout probe object that gives information about 
 * layout elements.  No mutilation of the layout elements is allowed by this
 * probe.  Each FE uses this API to satisfy requests for layout information. 
 *
 */

#include "layprobe.h"
#include "layout.h"
#include "xp.h"


typedef struct _lo_ProbeState {
	MWContext *context;
	lo_TopState *top_state;
	lo_DocState *doc_state;
	LO_Element *curr_ele;
	XP_List *table_stack;			/* Stack of pointers to parent tables */
	XP_List *cell_stack;			/* Stack of pointers to parent cells */
	XP_List *parent_type_stack;		/* Stack of type of parents seen till now */
} lo_ProbeState;

/* Private function prototypes */
static Bool lo_QA_IsValidElement (LO_Element *ele);
static LO_Element * lo_QA_GetValidNextElement (LO_Element *ele);
static Bool lo_QA_ElementHasColor( LO_Element * ele);
static Bool lo_QA_GetColor( LO_Element *ele, LO_Color *color, ColorType type);
static long lo_QA_RGBColorToLong( LO_Color color );

/* 
 * Constructor/Destructor for probe 
 *
 */

/* Probe state is created.  Probe position gets set to "nothing". */
long LO_QA_CreateProbe( MWContext *context )
{
	lo_ProbeState *ps = XP_NEW_ZAP( lo_ProbeState );

	if (ps && context) {
		ps->context = context;
		ps->curr_ele = NULL;
		ps->table_stack = XP_ListNew();
		ps->cell_stack = XP_ListNew();
		ps->parent_type_stack = XP_ListNew();
		ps->top_state = lo_FetchTopState( XP_DOCID(context) );
		if (ps->top_state)
			ps->doc_state = ps->top_state->doc_state;
	}	

	return (long) ps;
}

/* Probe state is destroyed. */
void LO_QA_DestroyProbe( long probeID )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;

	if (ps) {
		ps->context = NULL;
		ps->top_state = NULL;
		ps->doc_state = NULL;
		ps->curr_ele = NULL;
		if (ps->table_stack)
			XP_ListDestroy( ps->table_stack );
		if (ps->cell_stack)
			XP_ListDestroy( ps->cell_stack );
		if (ps->parent_type_stack)
			XP_ListDestroy( ps->parent_type_stack );
		XP_FREE(ps);
	}
}


/*
 * Functions to set position of layout probe.  
 *
 */

/* Sets probe position to first layout element in document. */
Bool LO_QA_GotoFirstElement( long probeID )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	LO_Element **line_array;
	Bool success = FALSE;

	if (ps && ps->doc_state) {
		XP_LOCK_BLOCK ( line_array, LO_Element **, ps->doc_state->line_array );
		if (line_array) {
			ps->curr_ele = line_array[0];
			success = TRUE;
		}
	}

	return success;
}

/* 
	If the probe has just been created, the next element is the first layout element in the document.
	If there is a next element, the function returns TRUE and sets the probe position to the next element.
	If there is no next element, the function returns FALSE and resets the probe position to nothing. 
*/
Bool LO_QA_GotoNextElement( long probeID )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;
	LO_Element *next_ele = NULL;

	if (ps) {
		if (!ps->curr_ele) {
			/* The probe has just been created, so goto first element */
			LO_QA_GotoFirstElement( (long) ps );
			success = TRUE;
		}
		else {
			/* The probe is currently pointing to curr_ele. */
			next_ele = lo_QA_GetValidNextElement (ps->curr_ele);
			if (next_ele) {
				/* A valid next element exists */
				ps->curr_ele = next_ele;
				success = TRUE;
			}
			else {
				/* No valid next element */
				ps->curr_ele = NULL;
			}
		}
	}

	return success;
}

/* 	
	Sets probe position and returns TRUE or false based on the following table:

	Current Probe Position		New Probe Position					Return value
	----------------------		------------------					------------
	Table						First cell in table					TRUE
	Cell						First layout element in cell		TRUE
	Layer						First layout element in layer		TRUE
	Any other element			No change							FALSE	
 */

Bool LO_QA_GotoChildElement( long probeID )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps) {
		if (ps->curr_ele) {
			if (ps->curr_ele->lo_any.type == LO_TABLE) {
				/* Push the table layout element on the table stack */
				XP_ListAddObjectToEnd (ps->table_stack, (void *) ps->curr_ele);
				/* Push the table type on the parent type stack */
				XP_ListAddObjectToEnd (ps->parent_type_stack, (void *) LO_TABLE);
				ps->curr_ele = ps->curr_ele->lo_any.next;
				XP_ASSERT( ps->curr_ele->lo_any.type == LO_CELL);
				success = TRUE;
			}
			else if (ps->curr_ele->lo_any.type == LO_CELL) {
				/* Push the table layout element on the cell stack */
				XP_ListAddObjectToEnd (ps->cell_stack, (void *) ps->curr_ele);
				/* Push the cell type on the parent type stack */
				XP_ListAddObjectToEnd (ps->parent_type_stack, (void *) LO_CELL);
				ps->curr_ele = ps->curr_ele->lo_cell.cell_list;				
				success = TRUE;
			}
		}
	}

	return success;
}

/* 	
	Sets probe position and returns TRUE or false based on the following table:

	Current Probe Position		New Probe Position					Return value
	----------------------		------------------					------------	
	Cell within a table			Parent Table						TRUE
	Element within a cell		Parent Cell							TRUE
	Element within a layer		Parent Layer						TRUE
	Any other element			No change							FALSE	
 */

Bool LO_QA_GotoParentElement( long probeID )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;	
	Bool success = FALSE;

	if (ps) {
		if (ps->parent_type_stack) {
			/* Pop the parent type off of the parent type stack */
			uint8 parentType = (uint8) XP_ListRemoveEndObject( ps->parent_type_stack );
			switch (parentType) {
			case LO_TABLE: {
					if (ps->table_stack) {
						ps->curr_ele = (LO_Element *) XP_ListRemoveEndObject( ps->table_stack );
						success = TRUE;
					}
					break;
				}
			case LO_CELL: {
					if (ps->cell_stack) {
						ps->curr_ele = (LO_Element *) XP_ListRemoveEndObject( ps->cell_stack );
						success = TRUE;
					}
					break;
				}
			}
		}
	}

	return success;
}

/*
	Gets the layout element type that the probe is positioned on.
	Returns FALSE if the probe is not positioned on any element, else returns TRUE. 
*/
Bool LO_QA_GetElementType( long probeID, int *type )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		XP_ASSERT(lo_QA_IsValidElement( ps->curr_ele ));
		*type = ps->curr_ele->lo_any.type;
		success = TRUE;
	}

	return success;
}

/* 
 * Functions to return the current layout element's position and dimensions.
 *
 */

/* 
	Each of these functions return TRUE if the probe position is set to a layout element,
	otherwise, they return FALSE.
*/
Bool LO_QA_GetElementXPosition( long probeID, long *x )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		XP_ASSERT(lo_QA_IsValidElement( ps->curr_ele ));
		*x = ps->curr_ele->lo_any.x;
		success = TRUE;
	}

	return success;
}

Bool LO_QA_GetElementYPosition( long probeID, long *y )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		XP_ASSERT(lo_QA_IsValidElement( ps->curr_ele ));
		*y = ps->curr_ele->lo_any.y;
		success = TRUE;
	}

	return success;
}

Bool LO_QA_GetElementWidth( long probeID, long *width )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		XP_ASSERT(lo_QA_IsValidElement( ps->curr_ele ));
		*width = ps->curr_ele->lo_any.width;
		success = TRUE;
	}

	return success;
}

Bool LO_QA_GetElementHeight( long probeID, long *height )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		XP_ASSERT(lo_QA_IsValidElement( ps->curr_ele ));
		*height = ps->curr_ele->lo_any.height;
		success = TRUE;
	}

	return success;
}


Bool LO_QA_HasURL( long probeID, Bool *hasURL )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		if (ps->curr_ele->lo_any.type == LO_TEXT) {
			if (ps->curr_ele->lo_text.text_attr->attrmask & LO_ATTR_ANCHOR) {
				*hasURL = TRUE;
			}
		}
		else {
			*hasURL = FALSE;
		}
		
		success = TRUE;
	}

	return success;
}

Bool LO_QA_HasText( long probeID, Bool *hasText )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		if (ps->curr_ele->lo_any.type == LO_TEXT)
			*hasText = TRUE;
		else
			*hasText = FALSE;
		success = TRUE;
	}

	return success;
}

Bool LO_QA_HasColor( long probeID, Bool *hasColor )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		XP_ASSERT(lo_QA_IsValidElement( ps->curr_ele ));
		*hasColor = lo_QA_ElementHasColor( ps->curr_ele );
		success = TRUE;
	}

	return success;
}

Bool LO_QA_HasChild( long probeID, Bool *hasChild )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		if (ps->curr_ele->lo_any.type == LO_TABLE ||
			ps->curr_ele->lo_any.type == LO_CELL)
			*hasChild = TRUE;
		else
			*hasChild = FALSE;
		success = TRUE;
	}

	return success;
}

Bool LO_QA_HasParent( long probeID, Bool *hasParent )
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->parent_type_stack) {		
		if (XP_ListGetEndObject(ps->parent_type_stack)) {
			/* There is stuff pushed on the parent type stack.  So, this element has a parent */
			*hasParent = TRUE;
		}
		else {
			*hasParent = FALSE;
		}
		success = TRUE;
	}

	return success;
}

Bool LO_QA_GetTextLength( long probeID, long *length)
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		if (ps->curr_ele->lo_any.type == LO_TEXT) {
			*length = ps->curr_ele->lo_text.text_len;
			success = TRUE;
		}
	}

	return success;
}

Bool LO_QA_GetText( long probeID, char *text, long length)
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		if (ps->curr_ele->lo_any.type == LO_TEXT) {
			XP_ASSERT(XP_STRLEN(ps->curr_ele->lo_text.pText) == length);
			strncpy(text, ps->curr_ele->lo_text.pText, length);
			success = TRUE;
		}
	}

	return success;
}

/*   Color Type                       Elements containing the color type 
     ----------                       ----------------------------------
	 LO_QA_BACKGROUND_COLOR           LO_TEXT, LO_BULLET, LO_CELL
	 LO_QA_FOREGROUND_COLOR           LO_TEXT, LO_BULLET
	 LO_QA_BORDER_COLOR               LO_TABLE

     Based on the element type and the color type, gets the color of the element
	 at which the probe is currently positioned.  Returns TRUE if successful,
	 FALSE if not.
*/
Bool LO_QA_GetColor( long probeID, long *color, ColorType type)
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;

	if (ps && ps->curr_ele) {
		if (lo_QA_ElementHasColor(ps->curr_ele)) {
			LO_Color clr;
			if (lo_QA_GetColor(ps->curr_ele, &clr, type)) {
				*color = lo_QA_RGBColorToLong( clr );
				success = TRUE;
			}
		}
	}

	return success;
}



/**
  Private methods.  Only called from within this module.
**/

/* Gets a "valid" next element in the layout element list.
   "Valid" elements are all the element types that we want layout outsiders to know about.
   Note:  An LO_CELL is NOT a valid next element even though outsiders know about it.  This
   is because we want tables to contain cells.  Cells can only be gotten at by making a
   LO_QA_GotoChildElement() on a probe that is positioned on a table element.
 */
static LO_Element * lo_QA_GetValidNextElement (LO_Element *ele)
{
	LO_Element *next_ele = NULL;

	if (ele) {
		Bool found = FALSE;
		next_ele = ele->lo_any.next;		
		while (next_ele && !found) {
			switch(next_ele->lo_any.type) 
			{
				case LO_TEXT:
				case LO_HRULE:
				case LO_IMAGE:
				case LO_BULLET:
				case LO_TABLE:
				case LO_FORM_ELE:
				case LO_EMBED:
				case LO_JAVA:
				case LO_OBJECT: {
					/* This is a valid element. */
					found = TRUE;
					break;
					}
				default: {
					/* Not a valid element. Try next element */
					next_ele = next_ele->lo_any.next;
					}
			}
		}
	}

	return next_ele;
}

/* Returns TRUE if ele's type is one of the types defined in layprobe.h */
static Bool lo_QA_IsValidElement (LO_Element *ele)
{
	Bool isValid = FALSE;
	if (ele) {
		switch (ele->lo_any.type)
		{
			case LO_TEXT:
			case LO_HRULE:
			case LO_IMAGE:
			case LO_BULLET:
			case LO_TABLE:
			case LO_CELL:
			case LO_FORM_ELE:
			case LO_EMBED:
			case LO_JAVA:
			case LO_OBJECT: {
				isValid = TRUE;
				break;
			}										
		}
	}

	return isValid;
}

/* Figure out if ele is an element that has a color property */
static Bool lo_QA_ElementHasColor( LO_Element * ele)
{
	Bool hasColor = FALSE;
	if ( ele ) {
	   hasColor = (ele->lo_any.type == LO_TEXT ||				
				ele->lo_any.type == LO_BULLET ||
				ele->lo_any.type == LO_TABLE ||
				ele->lo_any.type == LO_CELL);
	}
	return hasColor;
}

/*   Color Type                       Elements containing the color type 
     ----------                       ----------------------------------
	 LO_QA_BACKGROUND_COLOR           LO_TEXT, LO_BULLET, LO_CELL
	 LO_QA_FOREGROUND_COLOR           LO_TEXT, LO_BULLET
	 LO_QA_BORDER_COLOR               LO_TABLE

     Based on the element type and the color type, gets the color of ele.
	 Returns TRUE if successful, FALSE if not.
*/
static Bool lo_QA_GetColor( LO_Element *ele, LO_Color *color, ColorType type )
{
	Bool success = FALSE;

	if (ele) {
		switch (ele->lo_any.type)
		{
		case LO_TEXT:
			if (type == LO_QA_FOREGROUND_COLOR) {
				if (ele->lo_text.text_attr) {
					*color = ele->lo_text.text_attr->fg;
					success = TRUE;
				}
			}
			else if (type == LO_QA_BACKGROUND_COLOR) {
				if (ele->lo_text.text_attr) {
					*color = ele->lo_text.text_attr->bg;
					success = TRUE;
				}
			}
			break;		
		case LO_BULLET:
			if (type == LO_QA_FOREGROUND_COLOR) {
				if (ele->lo_text.text_attr) {
					*color = ele->lo_bullet.text_attr->fg;
					success = TRUE;
				}
			}
			else if (type == LO_QA_BACKGROUND_COLOR) {
				if (ele->lo_text.text_attr) {
					*color = ele->lo_bullet.text_attr->bg;
					success = TRUE;
				}
			}
			break;
		case LO_TABLE:
			if (type == LO_QA_BORDER_COLOR) {
				*color = ele->lo_table.border_color;
				success = TRUE;
			}
			break;
		case LO_CELL:
			if (type == LO_QA_BACKGROUND_COLOR) {
				if (ele->lo_cell.backdrop.bg_color) {
					*color = *(ele->lo_cell.backdrop.bg_color);
					success = TRUE;
				}
			}
			break;
		}
	}

	return success;
}

/* Convert the RGB triplet into a long */
static long lo_QA_RGBColorToLong( LO_Color color )
{
	long c = color.red;
	
	c = c << 8;
	c += color.green;
	c = c << 8;
	c += color.blue;

	return c;
}