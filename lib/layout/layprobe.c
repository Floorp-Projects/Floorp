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
#include "prlink.h"

/* test dll init function prototype. */
typedef Bool TEST_LIB_INIT_PROC(void *);

typedef struct _lo_ProbeState {
	MWContext *context;
	lo_TopState *top_state;
	lo_DocState *doc_state;
	LO_Element *curr_ele;
	XP_List *table_stack;			/* Stack of pointers to parent tables */
	XP_List *cell_stack;			/* Stack of pointers to parent cells */
	XP_List *parent_type_stack;		/* Stack of type of parents seen till now */
} lo_ProbeState;


typedef struct _EnumInfo {
	int16 LAPI_Type;
	char* Name;
	int32 xPos;
	int32 yPos;
} stElementInfo;


/* Private variables */
static int32 LAPI_LastError = 0;
static XP_List* CallbackList[MAX_CALLBACKS];

/* Private types */
typedef Bool (LAPI_ELEMENTS_ENUM_FUNC)(MWContext* FrameID, LO_Element* ElementID, stElementInfo* ElementInfo, void** lppData);

/* Private function prototypes */
static Bool lo_QA_IsValidElement (LO_Element *ele);
static LO_Element * lo_QA_GetValidNextElement (LO_Element *ele);
static Bool lo_QA_ElementHasColor( LO_Element * ele);
static Bool lo_QA_GetColor( LO_Element *ele, LO_Color *color, ColorType type);
static long lo_QA_RGBColorToLong( LO_Color color );

static lo_ProbeState* GetProbe( MWContext *context );
static int16 GetElementLAPIType(LO_Element* ElementID);
static Bool IsLowLevelElementType(LO_Element* ElementID);
static Bool EnumElements(MWContext* FrameID, LO_Element *ElementID,
						 void ** lppData, stElementInfo* pElementInfo,
						 LAPI_ELEMENTS_ENUM_FUNC *EnumFunc);
/* Privat enum functions */
static Bool EnumGetElementList(MWContext* FrameID, LO_Element* ElementID, stElementInfo* pElementInfo, void** lppData);
static Bool EnumGetElementFromPoint(MWContext* FrameID, LO_Element *ElementID, stElementInfo* pElementInfo, void ** lppData);


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
			XP_ASSERT(XP_STRLEN((const char *) ps->curr_ele->lo_text.text) == length);
			strncpy(text, (const char *) ps->curr_ele->lo_text.text, length);
			success = TRUE;
		}
	}

	return success;
}

/* 
	Function to get text attributes (font, color, style ..) at once. Requires 
	preallocated LO_TextAttr_struct with size member set on. 
	
	char * font_face
		member points to allocated buffer. Caller responsibility to free buffer

    LO_TextAttr_struct * next 
		member ignored, set to 0.

    void * FE_Data	(recently cached font object)
		member ignored, set to 0;

     

*/

Bool LO_QA_GetTextAttributes( long probeID, void *attributes)
{
	lo_ProbeState *ps = (lo_ProbeState *) probeID;
	Bool success = FALSE;
	size_t len = 0;

	if (ps && ps->curr_ele) {
		if (ps->curr_ele->lo_any.type == LO_TEXT) {
			if (((LO_TextAttr *)attributes)->size == 
				ps->curr_ele->lo_text.text_attr->size) {

			((LO_TextAttr *)attributes)->fontmask =
				ps->curr_ele->lo_text.text_attr->fontmask;
			((LO_TextAttr *)attributes)->fg =
				ps->curr_ele->lo_text.text_attr->fg;
			((LO_TextAttr *)attributes)->bg =
				ps->curr_ele->lo_text.text_attr->bg;
			((LO_TextAttr *)attributes)->no_background =
				ps->curr_ele->lo_text.text_attr->no_background;
			((LO_TextAttr *)attributes)->attrmask =
				ps->curr_ele->lo_text.text_attr->attrmask;
			
			/* copy string of font faces */
			if (len = strlen(ps->curr_ele->lo_text.text_attr->font_face)) {
				((LO_TextAttr *)attributes)->font_face =
					(char *)XP_ALLOC(len + 1);
				strcpy(((LO_TextAttr *)attributes)->font_face,
						ps->curr_ele->lo_text.text_attr->font_face);
			}
			((LO_TextAttr *)attributes)->font_face =
				ps->curr_ele->lo_text.text_attr->font_face;
			((LO_TextAttr *)attributes)->point_size =
				ps->curr_ele->lo_text.text_attr->point_size;
			((LO_TextAttr *)attributes)->font_weight =
				ps->curr_ele->lo_text.text_attr->font_weight;
			((LO_TextAttr *)attributes)->FE_Data =	0;
			((LO_TextAttr *)attributes)->next 	  = 0;
			((LO_TextAttr *)attributes)->charset =
				ps->curr_ele->lo_text.text_attr->charset;

			success = TRUE;
			}
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


/* LAPIGetLastError
**
**	Description:	This function retrieves the error code of the last error
**					 encountered by an API call.
**
**	Parameters:		None.
**
**	Return Value:	The return value is the error code of the last error.
*/
int32 LAPIGetLastError()
{
	return LAPI_LastError;
}


/* DestroyProbe
**
**	Description:	This function deletes a probe struct, It should
**			`		only be used to clear memory allocated when a frame is destroyed.
**					Parameter should be the Probe member of MWContext.
**
**	Parameters:		ProbeID - the probe id to destroy.
**
**	Return Value:	None.
*/
void LAPIDestroyProbe(
	void* ProbeID
)
{
	LO_QA_DestroyProbe((long)ProbeID);
}


/**********************************************************
*********               Queries API                ********
**********************************************************/


/* LAPIGetFrames
**
**	Description:	This function retrieves a list of all frames.
**
**	Parameters:		None.
**
**	Return Value:	If the function succeeds, the return value is TRUE,
**					if there are no frames FALSE is returned and LastError will be LAPI_E_NOELEMENTS
*/
Bool LAPIGetFrames(XP_List** lppContextList)
{
	*lppContextList = XP_GetGlobalContextList();
	
	if (*lppContextList)
		return TRUE;

	LAPI_LastError = LAPI_E_NOELEMENTS;
	return FALSE;
}


/* LAPIFrameGetStringProperty
**
**	Description:	This function retrieves a property of a frame.
**
**	Parameters:		FrameID			- a frame id.
**					PropertyName	- a string identifing the property
**					lpszPropVal		- an allocated buffer to write the string to.
**
**	Return Value:	If the function succeeds, the return value is TRUE, 
**					otherwise the function returns FALSE and LastError will be:
**					LAPI_E_INVALIDFRAME if the given id doesn't identify an existing frame,
**					LAPI_E_INVALIDPROP if the given propery name isn't supported.
*/
Bool LAPIFrameGetStringProperty( 
	MWContext*		FrameID,
	char*			PropertyName,
	char**			lpszPropVal
)
{
	lo_ProbeState* ps = GetProbe(FrameID);

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return FALSE;
	}

	if (!XP_STRCASECMP(PropertyName, "title"))
	{
		*lpszPropVal = FrameID->title;
		return TRUE;
	}
	else if (!XP_STRCASECMP(PropertyName, "name"))
	{
		*lpszPropVal = FrameID->name;
		return TRUE;
	}
	else if (!XP_STRCASECMP(PropertyName, "url"))
	{
		*lpszPropVal = FrameID->url;
		return TRUE;
	}

	LAPI_LastError = LAPI_E_INVALIDPROP;
	return FALSE;
}
 

/* LAPIFrameGetNumProperty
**
**	Description:	This function retrieves a property of a frame.
**
**	Parameters:		FrameID			- a frame id.
**					PropertyName	- a string identifing the property
**					lpPropVal		- a pointer to a numeric var to write the prop' val into.
**
**	Return Value:	If the function succeeds, the return value is TRUE,
**					otherwise the function returns FALSE and LastError will be:
**					LAPI_E_INVALIDFRAME if the given id doesn't identify an existing frame,
**					LAPI_E_INVALIDPROP if the given propery name isn't supported,
*/
Bool LAPIFrameGetNumProperty( 
	MWContext*		FrameID,
	char*			PropertyName,
	int32*			lpPropVal			
)
{
	lo_ProbeState* ps = GetProbe(FrameID);

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return FALSE;
	}

	if (!XP_STRCASECMP(PropertyName, "width"))
	{
		*lpPropVal = (ps->doc_state)->win_width;
		return TRUE;
	}
	else if (!XP_STRCASECMP(PropertyName, "height"))
	{
		*lpPropVal = (ps->doc_state)->win_height;
		return TRUE;
	}

	LAPI_LastError = LAPI_E_INVALIDPROP;
	return FALSE;
}


/* LAPIFrameGetElements
**
**	Description:	This function retrieves a list of elements which match a gived type
**					and name.if not specified, all elements are retrieved..
**
**	Parameters:		lpList			- a list to be filled with all the elements
**					FrameID			- a frame id.
**					ElementLAPIType	- the element type/s to retrieve.
**					ElementName		- retrive elements with this name.
**
**	Return Value:	If the function succeeds, the return value is TRUE and the
**					list is populated with elementID's, otherewise return value is FALSE,
**					LastError will be:
**					LAPI_E_INVALIDFRAME if the frame id isn't valid,
**					LAPI_E_NOELEMENTS if there are no elements that fit the request.
*/
Bool LAPIFrameGetElements( 
	XP_List*	lpList,
	MWContext*	FrameID,
	int16		ElementLAPIType,
	char*		ElementName
) 	
{
	lo_ProbeState* ps = GetProbe(FrameID);
	stElementInfo ElementInfo;

	ElementInfo.LAPI_Type = ElementLAPIType;
	ElementInfo.Name = ElementName;

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return FALSE;
	}

	EnumElements(FrameID, LAPIGetFirstElement(FrameID), &lpList,
				&ElementInfo, &EnumGetElementList);

	if (lpList && lpList->next)
		return TRUE;
	else
	{	/* empty list */
		LAPI_LastError = LAPI_E_NOELEMENTS;
		return FALSE;
	}
}

 
/* LAPIFrameGetElementFromPoint
**
**	Description:	This function retrieves the lowest level
**					layout element from a given point in screen coordinates.
**
**	Parameters:		FrameID	- a frame id.
**					xPos	- the x coordinate of the point.	
**					yPos	- the y coordinate of the point.	
**
**	Return Value:	If the function succeeds, the return value an ElementID,
**					if the function fails the return value is NULL and LastError
**					gets set to LAPI_E_INVALIDFRAME if the frame parameter is invalid or to
**					LAPI_E_NOELEMENTS if no element is under this point.
*/
LO_Element* LAPIFrameGetElementFromPoint( 
	MWContext * FrameID,
	int xPos,
	int yPos
)
{
	lo_ProbeState* ps = GetProbe(FrameID);
	LO_Element* ElementID;
	stElementInfo ElementInfo;

	(ElementInfo).xPos = xPos;
	(ElementInfo).yPos = yPos;

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return NULL;
	}

	EnumElements(FrameID, LAPIGetFirstElement(FrameID), &ElementID,
				&ElementInfo, &EnumGetElementFromPoint);

	if (ElementID)
		return ElementID;
	else
	{
		LAPI_LastError = LAPI_E_NOELEMENTS;
		return NULL;
	}
}


/* LAPIGetFirstElement
**
**	Description:	The GetFirstElement function retrieves the first element in the frame.
**
**	Parameters:		FrameID	- a frame id.
**
**	Return Value:	If the function succeeds, the return value an ElementID,
**					if the function fails the return value is NULL and LastError
**					gets set to LAPI_E_INVALIDFRAME if the frame parameter is invalid or to
**					LAPI_E_NOELEMENTS if the frame contains no elements.
*/
LO_Element* LAPIGetFirstElement ( 
	MWContext * FrameID
) 	
{
	lo_ProbeState* ps = GetProbe(FrameID);

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return NULL;
	}
	
	if (LO_QA_GotoFirstElement((long)ps))
		return ps->curr_ele;
	else
	{	/* no elements */
		LAPI_LastError = LAPI_E_NOELEMENTS;
		return NULL;
	}
}

 
/* LAPIGetNextElement
**
**	Description:	Given an element, this function retrieves the next element.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**
**	Return Value:	If the function succeeds, the return value an ElementID,
**					if the function fails the return value is NULL and LastError
**					gets set to LAPI_E_INVALIDFRAME if the frame parameter is invalid or to
**					LAPI_E_INVALIDELEMENT if the given element is invalid.
**					LAPI_E_NOELEMENTS if there's no next element.
*/
LO_Element* LAPIGetNextElement ( 
	MWContext * FrameID,
	LO_Element* ElementID
)
{
	lo_ProbeState* ps = GetProbe(FrameID);
	LO_Element **line_array;
	LO_Element* RefElement = ElementID;

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return NULL;
	}

	XP_LOCK_BLOCK ( line_array, LO_Element **, ps->doc_state->line_array );
	if (ElementID) 
	{
		if (ElementID->type == LO_TABLE)
		{
			/* for a table we have to skip all the cells
				and return the first element after */
			while(RefElement = (RefElement->lo_any).next)
			{
				if (RefElement->type != LO_CELL)
					return RefElement;
			}
		}
		else
		{
			if ((RefElement->lo_any).next)
				return ((RefElement->lo_any).next);
		}/* TODO - check for last cell in a table */

		/* no next element */
		LAPI_LastError = LAPI_E_NOELEMENTS;
		return NULL;
	}
	else
	{	/* invalid element */
		LAPI_LastError = LAPI_E_INVALIDELEMENT;
		return NULL;
	}
}

 


/* LAPIGetPrevElement
**
**	Description:	Given an element, this function retrieves the previous element.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**
**	Return Value:	If the function succeeds, the return value an ElementID,
**					if the function fails the return value is NULL and LastError
**					gets set to LAPI_E_INVALIDFRAME if the frame parameter is invalid or to
**					LAPI_E_NOELEMENTS if there's no previous element.
*/
LO_Element* LAPIGetPrevElement ( 
	MWContext * FrameID,
	LO_Element* ElementID
)
{
	lo_ProbeState* ps = GetProbe(FrameID);
	LO_Element **line_array;
	LO_Element* RefElement = ElementID;

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return NULL;
	}

	XP_LOCK_BLOCK ( line_array, LO_Element **, ps->doc_state->line_array );
	if (ElementID) 
	{
		if (RefElement = (RefElement->lo_any).prev)
		{
			if (RefElement->type != LO_TABLE || ElementID->type != LO_CELL)
			{	/* not a first cell inside a table */
				if (RefElement->type == LO_CELL && ElementID->type != LO_CELL)
				{	/* a prev element can't be a cell unless the given element is a cell */
					while(RefElement = (RefElement->lo_any).prev)
					{
						if (RefElement->type == LO_TABLE)
							return RefElement;
					}
				}
				else
					return ((RefElement->lo_any).prev);
			}
		}

		/* no prev element */
		LAPI_LastError = LAPI_E_NOELEMENTS;
		return NULL;
	}
	else
	{	/* invalid element */
		LAPI_LastError = LAPI_E_INVALIDELEMENT;
		return NULL;
	}
}


/* LAPIGetChildElement
**
**	Description:	Given an element, this function retrieves its first child element.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**
**	Return Value:	If the function succeeds, the return value an ElementID,
**					if the function fails the return value is NULL and LastError
**					gets set to LAPI_E_INVALIDFRAME if the frame parameter is invalid or to
**					LAPI_E_NOELEMENTS if the element has no child elements.
*/
LO_Element* LAPIGetChildElement ( 
	MWContext * FrameID,
	LO_Element* ElementID
)
{
	lo_ProbeState* ps = GetProbe(FrameID);
	LO_Element **line_array;
	LO_Element* ret, *RefElement = (LO_Element*)ElementID;

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return NULL;
	}

	XP_LOCK_BLOCK ( line_array, LO_Element **, ps->doc_state->line_array );
	if (ElementID)
	{
		switch ((RefElement->lo_any).type)
		{
			case LO_TABLE:
			{
				if (ret = (RefElement->lo_any).next)
				{
					XP_ASSERT(ret->lo_any.type == LO_CELL);	
					return ret;
				}

				LAPI_LastError = LAPI_E_NOELEMENTS;	
				return NULL;
			}
			case LO_CELL:
			{
				if (ret = (RefElement->lo_cell).cell_list)
					return ret;
				else if (ret = (RefElement->lo_cell).cell_float_list)
					return ret;

				LAPI_LastError = LAPI_E_NOELEMENTS;	
				return NULL;
			}
			default:
			{
				LAPI_LastError = LAPI_E_NOELEMENTS;	
				return NULL;
			}
	
		}
	}
	else
	{	/* invalid element */
		LAPI_LastError = LAPI_E_INVALIDELEMENT;
		return NULL;
	}
}

 

/*
** LAPIGetParentElement
**
**	Description:	Given an element, this function retrieves its parenet element.
**					This function is time consuming.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**
**	Return Value:	If the function succeeds, the return value an ElementID,
**					if the function fails the return value is NULL and LastError
**					gets set to LAPI_E_INVALIDFRAME if the frame parameter is invalid or to
**					LAPI_E_NOELEMENTS if the element has no parent elements.
*/
LO_Element* LAPIGetParentElement (
	MWContext*	FrameID,
	LO_Element*	ElementID
)
{
	lo_ProbeState* ps = GetProbe(FrameID); 

	if (ps == NULL)
	{	
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return NULL;
	}

	if (ElementID->type == LO_CELL)
		return ((ElementID->lo_any).prev);
		
	if (LO_QA_GotoFirstElement((long)ps))
	{
		/* do a DFS tree search for the element */
		do
		{
			/* goto the deepest child */
			while (LO_QA_GotoChildElement((long)ps));

			if (ps->curr_ele == ElementID && ps->parent_type_stack)
			{
				/* Pop the parent type off of the parent type stack */
				uint8 parentType = (uint8) XP_ListRemoveEndObject( ps->parent_type_stack );
				switch (parentType) 
				{
					case LO_TABLE:
					{
						if (ps->table_stack)
							return (LO_Element *) XP_ListRemoveEndObject( ps->table_stack );
					}
					case LO_CELL:
					{
						if (ps->cell_stack)
							return (LO_Element *) XP_ListRemoveEndObject( ps->cell_stack );
					}
					default:
					{
						LAPI_LastError = LAPI_E_UNEXPECTED;
						return NULL;
					}
				}
			}
		}
		while (LO_QA_GotoNextElement((long)ps));
	}
	else
	{
		LAPI_LastError = LAPI_E_UNEXPECTED;
		return NULL;
	}
}


/* LAPIElementGetStringProperty
**
**	Description:	This function retrieves a property of an element.
**
**	Parameters:		FrameID			- a frame id.
**					ElementID		- Identifies the reference element.
**					PropertyName	- a string identifing the property
**					lpszPropVal		- an allocated buffer to write the string to.
**
**	Return Value:	If the function succeeds, the return value is TRUE, otherwise FALSE is returned.
**					otherwise the function returns LAPI_INVALID_NUM and LastError will be:
**					LAPI_E_INVALIDFRAME if the given id doesn't identify an existing frame,
**					LAPI_E_INVALIDPROP if the given propery name isn't supported,
*/
Bool LAPIElementGetStringProperty( 
	MWContext*		FrameID,
	LO_Element*		ElementID,
	char*			PropertyName,
	char**			lpszPropVal
)
{
	lo_ProbeState* ps = GetProbe(FrameID);

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return FALSE;
	}

	switch (ElementID->type)
	{
	case LO_TEXT:
		if (!XP_STRCASECMP(PropertyName, "text") || !XP_STRCASECMP(PropertyName, "name"))
		{
			/* beard: I detest platform-specific fields in the LO_TextStruct_struct. */
			*lpszPropVal = (const char *)(ElementID->lo_text).text;
			return TRUE;
		}
		if (!XP_STRCASECMP(PropertyName, "href"))
		{
			LO_AnchorData* pAnchorData = (ElementID->lo_text).anchor_href;
			if (pAnchorData && pAnchorData->anchor &&
				(*((char*)(pAnchorData->anchor)) != '\0'))

			*lpszPropVal = (char*)pAnchorData->anchor;
			return TRUE;
		}
		break;

	case LO_HRULE:
	break;
	case LO_IMAGE:
		if (!XP_STRCASECMP(PropertyName, "alt"))
		{
			*lpszPropVal = (char*)((ElementID->lo_image).alt);
			return TRUE;
		}
		else if (!XP_STRCASECMP(PropertyName, "url"))
		{
			*lpszPropVal = (char*)((ElementID->lo_image).image_url);
			return TRUE;
		}
		if (!XP_STRCASECMP(PropertyName, "href"))
		{
			LO_AnchorData* pAnchorData = (ElementID->lo_image).anchor_href;
			if (pAnchorData && pAnchorData->anchor &&
				(*((char*)(pAnchorData->anchor)) != '\0'))

			*lpszPropVal = (char*)pAnchorData->anchor;
			return TRUE;
		}
		break;
	case LO_BULLET:
	break;
	case LO_FORM_ELE:
		switch (((ElementID->lo_form).element_data)->type)
		{
		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_READONLY:
			if (!XP_STRCASECMP(PropertyName, "name"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_text).name);
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "defaulttext"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_text).default_text);
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "currenttext"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_text).current_text);
				return TRUE;
			}
			break;

		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
			if (!XP_STRCASECMP(PropertyName, "name"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_minimal).name);
				return TRUE;
			}
			break;

		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:
			if (!XP_STRCASECMP(PropertyName, "name"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_toggle).name);
				return TRUE;
			}
			break;

		case FORM_TYPE_SELECT_ONE:
		case FORM_TYPE_SELECT_MULT:
			if (!XP_STRCASECMP(PropertyName, "name"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_select).name);
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "options"))
			{	/* actually returns a pointer to the option list */
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_select).options);
				return TRUE;
			}

		case FORM_TYPE_TEXTAREA:
			if (!XP_STRCASECMP(PropertyName, "name"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_textarea).name);
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "defaulttext"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_textarea).default_text);
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "currenttext"))
			{
				*lpszPropVal = (char*)((((ElementID->lo_form).element_data)->ele_textarea).current_text);
				return TRUE;
			}
			break;

		case FORM_TYPE_IMAGE: /* TODO - what data structure?*/
			break;
		
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_JOT:
		case FORM_TYPE_ISINDEX:
		case FORM_TYPE_KEYGEN:
		case FORM_TYPE_OBJECT:
			break;
		}
		break;
	
	case LO_TABLE:
	case LO_CELL:
	case LO_EMBED:
	case LO_JAVA:
	case LO_OBJECT:
		break;
	}

	LAPI_LastError = LAPI_E_INVALIDPROP;
	return FALSE;
}
 

/* LAPIElementGetNumProperty
**
**	Description:	This function retrieves a property of a frame.
**
**	Parameters:		FrameID			- a frame id.
**					ElementID		- Identifies the reference element.
**					PropertyName	- a string identifing the property
**					lpPropVal		- a pointer to a numeric var to write the prop' val into.
**
**	Return Value:	If the function succeeds, the return value is TRUE,
**					otherwise the function returns FALSE and LastError will be:
**					LAPI_E_INVALIDFRAME if the given id doesn't identify an existing frame,
**					LAPI_E_INVALIDPROP if the given propery name isn't supported,
*/
Bool LAPIElementGetNumProperty( 
	MWContext*		FrameID,
	LO_Element*		ElementID,
	char*			PropertyName,
	int32*			lpPropVal			
)
{
	lo_ProbeState* ps = GetProbe(FrameID);

	if (ps == NULL)
	{	/* invalid frame id */
		LAPI_LastError = LAPI_E_INVALIDFRAME;
		return FALSE;
	}

	/* TODO Add bullet type */
	
	if (!XP_STRCASECMP(PropertyName, "type"))
	{
		*lpPropVal = GetElementLAPIType(ElementID);
		return TRUE;
	}
	else if (!XP_STRCASECMP(PropertyName, "xpos"))
	{
		*lpPropVal = (ElementID->lo_any).x + (ElementID->lo_any).x_offset;
		return TRUE;
	}
	else if (!XP_STRCASECMP(PropertyName, "ypos"))
	{
		*lpPropVal = (ElementID->lo_any).y + (ElementID->lo_any).y_offset;
		return TRUE;
	}
	else if (!XP_STRCASECMP(PropertyName, "width"))
	{
		*lpPropVal = (ElementID->lo_any).width;
		return TRUE;
	}
	else if (!XP_STRCASECMP(PropertyName, "height"))
	{
		*lpPropVal = (ElementID->lo_any).height;
		return TRUE;
	}

	if (ElementID->type == LO_FORM_ELE)
	{
		switch (((ElementID->lo_form).element_data)->type)
		{
		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_READONLY:
			if (!XP_STRCASECMP(PropertyName, "disabled"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_text).disabled;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "readonly"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_text).read_only;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "size"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_text).size;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "maxsize"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_text).max_size;
				return TRUE;
			}
			break;

		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
			if (!XP_STRCASECMP(PropertyName, "disabled"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_minimal).disabled;
				return TRUE;
			}
			break;

		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:
			if (!XP_STRCASECMP(PropertyName, "disabled"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_toggle).disabled;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "toggled"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_toggle).toggled;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "default_toggle"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_toggle).default_toggle;
				return TRUE;
			}
			break;

		case FORM_TYPE_SELECT_ONE:
		case FORM_TYPE_SELECT_MULT:
			if (!XP_STRCASECMP(PropertyName, "disabled"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_select).disabled;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "multiple"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_select).multiple;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "multiple"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_select).multiple;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "option_cnt"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_select).option_cnt;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "options"))
			{	/* actually returns a pointer to the option list */
				*lpPropVal = (int32)((((ElementID->lo_form).element_data)->ele_select).options);
				return TRUE;
			}

		case FORM_TYPE_TEXTAREA:
			if (!XP_STRCASECMP(PropertyName, "disabled"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_textarea).disabled;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "readonly"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_textarea).read_only;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "rows"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_textarea).rows;
				return TRUE;
			}
			else if (!XP_STRCASECMP(PropertyName, "cols"))
			{
				*lpPropVal = (((ElementID->lo_form).element_data)->ele_textarea).cols;
				return TRUE;
			}

		case FORM_TYPE_IMAGE: /* TODO - what data structure?*/
			break;
		
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_JOT:
		case FORM_TYPE_ISINDEX:
		case FORM_TYPE_KEYGEN:
		case FORM_TYPE_OBJECT:
			break;
		}
	}

	LAPI_LastError = LAPI_E_INVALIDPROP;
	return FALSE;
}




/**********************************************************
*********             Manipulation API             ********
**********************************************************/


/* LAPIElementClick
**
**	Description:	This function simulates a mouse button click
**					operation on an element.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**					MouseButton	- The mouse button to 'use'.
**
**	Return Value:	If the function succeeds, the return value is TRUE,
**					if the function fails the return value is NULL and LastError will be:
**					E_INVALIDFRAME if the frame parameter is invalid,
**					E_INVALIDELEMENT if the element parameter is invalid,
**					E_NOTSUPPORTED if the element does not support this operation.
*/
Bool LAPIElementClick( 
	MWContext*	FrameID,
	void*		ElementID,
	int16		MouseButton
)
{
	return FALSE;
}


/* LAPIElementSetText
**
**	Description:	This function sets the text value for edits,
**					lists and combo box's.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**					Text		- A text buffer.
**
**	Return Value:	If the function succeeds, the return value is TRUE,
**					if the function fails the return value is NULL and LastError will be:
**					LAPI_E_INVALIDFRAME if the frame parameter is invalid,
**					LAPI_E_INVALIDELEMENT if the element parameter is invalid,
**					LAPI_E_NOTSUPPORTED if the element does not support this operation.
**					For lists and combo boxes if the text isn't one of the existing items,
**					LastError will be set to LAPI_E_NOSUCHITEM.
*/
Bool LAPIElementSetText ( 
	MWContext*	FrameID,
	void*		ElementID,
	char*		Text
)
{
	return FALSE;
}



/* LAPIElementSetState
**
**	Description:	This function sets the value for radio buttons
**					and checkboxes.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**					Value		- The new state.
**
**	Return Value:	If the function succeeds, the return value is TRUE, if the function fails the return value is NULL and LastError will be:
**					E_INVALIDFRAME if the frame parameter is invalid,
**					E_INVALIDELEMENT if the element parameter is invalid, or to
**					E_NOTSUPPORTED if the element does not support this operation.
*/
Bool LAPIElementSetState ( 
	MWContext*	FrameID,
	void*		ElementID,
	int8		Value
)
{
	return FALSE;
}


/* LAPIElementSetState
**
**	Description:	This function makes the top left (or a relative specified point)
**					corner of an element visible, and retrieve it's properties.
**
**	Parameters:		FrameID		- Identifies the relevant frame.
**					ElementID	- Identifies the reference element.
**					xPos		- horizontal offset relative to top left corner.
**					yPos		- vertical offset relative to top left corner.
**
**	Return Value:	If the function succeeds, the return value is TRUE, if the function fails the return value is NULL and LastError will be:
**					E_INVALIDFRAME if the frame parameter is invalid,
**					E_INVALIDELEMENT if the element parameter is invalid,
**					E_OUTOFBOUNDS if the point specified is not within the element's rectangle,
**					E_NOTSUPPORTED if the element does not support this operation.
*/
Bool LAPIElementScrollIntoView ( 
	MWContext*		FrameID,
	void*			ElementID,
	int xPos,
	int yPos
)
{
	return FALSE;
}



/**********************************************************
*********         Callback regisration API         ********
**********************************************************/



/* LAPIRegisterNotifyCallback
**
**	Description:	This function is used to register a callback function
**					that will be called by Netscape each time a
**					specified event occurs.
**
**	Parameters:		lpFunc	- A callback function pointer
**					EventID	- An event identifier.
**
**	Return Value:	If the function succeeds, the return value is
**					a callbackID, otherwise the return value is NULL.
*/
static int32 LAPIRegisterNotifyCallback(
	ID_NOTIFY_PT*	lpFunc,
	int32			EventID
)
{
	if (!lpFunc)
		return 0;
	
	if (!CallbackList[EventID])
		CallbackList[EventID] = XP_ListNew();

	XP_ListAddObjectToEnd(CallbackList[EventID], (void*)lpFunc);

	return (int32)lpFunc;
}



/* LAPIRegisterElementCallback
**
**	Description:	This function is used to register a callback function
**					that will be called by Netscape each time a
**					specified mouse event occurs.
**
**	Parameters:		lpFunc	- A callback function pointer
**					EventID	- An event identifier.
**
**	Return Value:	If the function succeeds, the return value is
**					a callbackID, otherwise the return value is NULL.
*/
static int32 LAPIRegisterElementCallback(
	ELEMENT_NOTIFY_PT*	lpFunc,
	int32				EventID
)
{
	if (!lpFunc)
		return 0;
	
	if (!CallbackList[EventID])
		CallbackList[EventID] = XP_ListNew();

	XP_ListAddObjectToEnd(CallbackList[EventID], (void*)lpFunc);

	return (int32)lpFunc;
}


/* LAPIGetCallbackFuncList
**
**	Description:	This function retrieves the list of callback
**					funcions registered on a specific event.
**
**	Parameters:		EventID	- An event identifier.
**
**	Return Value:	If the function succeeds, the return value is
**					a pointer to the list of registerd callbacks, otherwise
**					the function returns NULL.
*/
XP_List* LAPIGetCallbackFuncList(
	int32	EventID
)
{
	if (EventID <0 || EventID >= MAX_CALLBACKS)
		return NULL;
	else
		return CallbackList[EventID];
}


/* LAPIUnregisterCallbackFunction
**
**	Description:	This function is used to remove a previously
**					registered callback function identified by
**					it's CallbackID.
**
**	Parameters:		CalbackID	-  The callbackID to unregister.
**
**	Return Value:	If the function succeeds, the return value is a TRUE,
**					otherwise the return value is FALSE.
*/
static Bool LAPIUnregisterCallbackFunction (
	void* CallbackID
)
{
	int i;
	for(i = 0; i< MAX_CALLBACKS;i++)
	{
		if (CallbackList[i] != NULL)
		{
			if (XP_ListRemoveObject(CallbackList[i], CallbackID))
				return TRUE;
		}
	}
	return FALSE;
}


/* LAPINotificationHandler
**
**	Description:	This function should be called before ET_SendEvent
**					and notify events before they are sent to the JS engine
**
**	Parameters:		pEvent		-  The event with all the info.
**
**	Return Value:	If the function succeeds, the return value is a TRUE,
**					otherwise the return value is FALSE.
*/
Bool LAPINotificationHandler(LAPIEventInfo* pEvent)
{
	int32 LAPIEvent = LAPI_INVALID_NUM;
	XP_List *pList;

	if (!pEvent)
		return FALSE;
	
	switch (pEvent->type)
	{
	case EVENT_MOUSEDOWN:
		LAPIEvent = ELEMENT_MOUSE_DOWN;
		break;
	case EVENT_MOUSEUP:
		LAPIEvent = ELEMENT_MOUSE_UP;
		break;
	case EVENT_MOUSEOVER:      /* user is mousing over a link */
		LAPIEvent = ELEMENT_MOUSE_OVER;
		break;
	case EVENT_MOUSEOUT:       /* user is mousing out of a link */
		LAPIEvent = ELEMENT_MOUSE_OUT;
		break;
	case EVENT_CLICK:          /* input element click in progress */
		LAPIEvent = ELEMENT_CLICK;
		break;
	case EVENT_FOCUS:          /* input focus event in progress */
		LAPIEvent = ELEMENT_SET_FOCUS;
		break;
	case EVENT_BLUR:           /* loss of focus event in progress */
		LAPIEvent = ELEMENT_KILL_FOCUS;
		break;
	case EVENT_CHANGE:         /* field value change in progress */
		LAPIEvent = ELEMENT_CHANGE;
		break;
	}
	
	pList = LAPIGetCallbackFuncList(LAPIEvent);
	
	if (pList)
	{
		while (pList = pList->next)
		{
			if (pList->object)
				(*((ELEMENT_NOTIFY_PT)pList->object))((void*)(pEvent->Context),
					(void*)pEvent->lo_element, pEvent->docx, pEvent->docy, LAPI_INVALID_NUM);
		}
	}
}
 

/**********************************************************
*  Private methods.  Only called from within this module. *
**********************************************************/

/* GetProbe
**
**	Description:	This function retrieves a frame's probe struct,
**					if there isn't one, a probe is created.
**
**	Parameters:		FrameID - id of the frame to get the probe for.
**
**	Return Value:	a pointer to the probe, or NULL if FrameID not valid.
*/
static lo_ProbeState* GetProbe(
	MWContext* FrameID
)
{
 	lo_TopState* top_state = NULL;
		if (!FrameID)
		return NULL;
	
	top_state = lo_FetchTopState( XP_DOCID(FrameID) );

	if (!top_state)
		return NULL;
	
	/* if a probe already exists return a pointer to it */
	if (top_state->LAPIprobe)
		return (lo_ProbeState*)top_state->LAPIprobe;

	top_state->LAPIprobe = (void*)LO_QA_CreateProbe(FrameID);
	if (top_state->LAPIprobe && ((lo_ProbeState*)top_state->LAPIprobe)->doc_state)
		return (lo_ProbeState*)top_state->LAPIprobe;

	return NULL;
}

/* GetElementLAPIType
**
**	Description:	An enumeration function going over all elements and for each of them
**					performing an operation given as a parameter.
**
**	Parameters:		ElementID	- An element to begin enumeration from.
**					lppData		- A data pointer for the use of the enum function.
**					pElementInfo- A pointer to the information of the object/s we're looking for.
**					EnumFunc	- A pointer to the enumerator function to be called for each element
**
**	Return Value:	LAPI type if successful, LAPI_INVALID_NUM otherwise (error code in LAPI_LastError).
*/
static int16 GetElementLAPIType(
	LO_Element* ElementID
)
{
	switch (ElementID->type)
	{
		case LO_TEXT:
		{
			LO_AnchorData* pAnchorData = (ElementID->lo_text).anchor_href;
			if (pAnchorData && pAnchorData->anchor &&
				(*((char*)(pAnchorData->anchor)) != '\0'))
			{
				return LAPI_TEXTLINK;
			}
			else
				return LAPI_TEXT;
		}
		case LO_HRULE:
			return LAPI_HRULE;

		case LO_IMAGE:
			return LAPI_IMAGE;
		
		case LO_BULLET:
			return LAPI_BULLET;

		case LO_TABLE:
		    return LAPI_TABLE;

		case LO_CELL:
			return LAPI_CELL;

		case LO_FORM_ELE:
		{
			switch (((ElementID->lo_form).element_data)->type)
			{
				case FORM_TYPE_TEXT:
				case FORM_TYPE_PASSWORD:
				case FORM_TYPE_TEXTAREA:
					return LAPI_FE_TEXT;

				case FORM_TYPE_RADIO:
					return LAPI_FE_RADIOBUTTON;

				case FORM_TYPE_CHECKBOX:
					return LAPI_FE_CHECKBOX;

				case FORM_TYPE_BUTTON:
				case FORM_TYPE_SUBMIT:
				case FORM_TYPE_RESET:
					return LAPI_FE_BUTTON;

				case FORM_TYPE_JOT:	
				case FORM_TYPE_SELECT_ONE:
					return LAPI_FE_COMBOBOX;

				case FORM_TYPE_SELECT_MULT:
					return LAPI_FE_LISTBOX;

				case FORM_TYPE_ISINDEX:
				case FORM_TYPE_IMAGE:
					return LAPI_IMAGE;

				case FORM_TYPE_NONE:
				case FORM_TYPE_HIDDEN:
				case FORM_TYPE_FILE:
				case FORM_TYPE_KEYGEN:
				case FORM_TYPE_READONLY:
				case FORM_TYPE_OBJECT:
					return LAPI_UNKNOWN;
			}
		}
		case LO_EMBED:
			return LAPI_EMBED;

		case LO_JAVA:
			return LAPI_JAVA;

		case LO_OBJECT:
			return LAPI_OBJECT;

		default:
			return LAPI_UNKNOWN;
	}
}

/* EnumElements
**
**	Description:	An enumeration function going over all elements and for each of them
**					performing an operation given as a parameter.
**
**	Parameters:		FrameID		- the id of the frame being queried.
**					ElementID	- An element to begin enumeration from.
**					lppData		- A data pointer for the use of the enum function.
**					pElementInfo- A pointer to the information of the object/s we're looking for.
**					EnumFunc	- A pointer to the enumerator function to be called for each element
**
**	Return Value:	TRUE if enumeration stopped, FALSE otherwise.
*/
static Bool EnumElements(
	MWContext* FrameID,
	LO_Element *ElementID,
	void ** lppData,
	stElementInfo* pElementInfo,
	LAPI_ELEMENTS_ENUM_FUNC *EnumFunc
)
{
	Bool bStopEnum	= FALSE;

	if (!ElementID)
		return FALSE;
	
	do
	{
		if (ElementID->type == LO_CELL)
		{
			bStopEnum = EnumElements(FrameID, ((LO_CellStruct*)ElementID)->cell_list, lppData, pElementInfo, EnumFunc);
			
			if (!bStopEnum)
				bStopEnum = EnumElements(FrameID, ((LO_CellStruct*)ElementID)->cell_float_list, lppData, pElementInfo, EnumFunc);
		}
		else if (ElementID->type == LO_FLOAT)
		{
			bStopEnum = EnumElements(FrameID, ((LO_FloatStruct*)ElementID)->float_ele, lppData, pElementInfo, EnumFunc);
		}
		else
		{
			bStopEnum = (*EnumFunc)(FrameID, ElementID, pElementInfo, lppData);
		}
	}
	while (!bStopEnum && ((ElementID = (ElementID->lo_any).next)));

	return bStopEnum;
}

/* EnumGetElementList
**
**	Description:	An enumerator function which creates and fils an element list
**					with all elements matching a given template.
**					* currently supports 'Type' and 'Name' only.
**
**	Parameters:		FrameID		- the id of the frame being queried.
**					ElementID	- An element to begin enumeration from.
**					lppData		- the address where the list pointer will be written to.
**					pElementInfo- A poiner to a template of the requested objects.
**
**	Return Value:	allways returns FALSE : continue enumeration
*/
static Bool EnumGetElementList(
	MWContext* FrameID,
	LO_Element *ElementID,
	stElementInfo* pElementInfo,
	void ** lppData
)
{
	int32 LAPI_Type;

	/* stop enumeration if list is null */
	if (!(*lppData))
		return TRUE;
	
	if ((LAPI_Type = GetElementLAPIType(ElementID)) == LAPI_UNKNOWN)
		return FALSE;  /* continue enumeration */
	
	/* compare element type */
	if (pElementInfo->LAPI_Type != LAPI_INVALID_NUM)
	{	/* request is for an exact type */
		if (pElementInfo->LAPI_Type != LAPI_Type)
			return FALSE;  /* continue enumeration */
		
		if (pElementInfo->Name != NULL)
		{
			char* szName;
			if (LAPIElementGetStringProperty(FrameID, ElementID, "name", &szName) == FALSE)
			{
				return FALSE; /* continue enumeration */
			}
			
			if (strcmp(pElementInfo->Name, szName))
				return FALSE; /* continue enumeration */
		}
	}

	/* Add the current element to the list */
	XP_ListAddObjectToEnd ((XP_List*)(*lppData), (void *) ElementID);
	
	return FALSE; /* continue enumeration */
}


/* EnumGetElementFromPoint
**
**	Description:	An enumerator function which finds the 
**					Lowset level layout element under a given point.
**					The point is passed in Screen_xPos and Screen_yPos of
**					pElementInfo.
**
**	Parameters:		FrameID		- the id of the frame being queried.
**					ElementID	- An element to begin enumeration from.
**					lppData		- an address of a ElementID.
**					pElementInfo- A poiner to a template of the requested objects.
**
**	Return Value:	TRUE if a low level layout object found, 
**					FALSE otherwise (continue enumeration).
*/
static Bool EnumGetElementFromPoint(
	MWContext*		FrameID,
	LO_Element		*ElementID,
	stElementInfo*	pElementInfo,
	void ** lppData
)
{
	int32	ele_xPos, ele_yPos, ele_width, ele_height;
	
	if (!LAPIElementGetNumProperty(FrameID, ElementID, "xpos", &ele_xPos) ||
		!LAPIElementGetNumProperty(FrameID, ElementID, "ypos", &ele_yPos) ||
		!LAPIElementGetNumProperty(FrameID, ElementID, "width", &ele_width) ||
		!LAPIElementGetNumProperty(FrameID, ElementID, "height", &ele_height))
		return FALSE; /* continue enumeration */


	if ((pElementInfo->xPos >= ele_xPos) &&
		(pElementInfo->xPos <= (ele_xPos + ele_width)) &&
		(pElementInfo->yPos >= ele_yPos) &&
		(pElementInfo->yPos <= (ele_yPos + ele_height)))
	{
		if (IsLowLevelElementType(ElementID))
		{
			*lppData = ElementID;
			return TRUE; /* object found, stop enumeration */
		}
	}
		
	return FALSE;  /* continue enumeration */
}


static Bool IsLowLevelElementType(
	LO_Element* ElementID
)
{
	if (ElementID->type == LO_TABLE || ElementID->type == LO_CELL)
		return FALSE;
	else
		return TRUE;
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

/* LAPILoadTestLib
**
**	Description:	This function loads the testing library and calls
**					"Initialize" in the lib.
**
**	Parameters:		szLibName	-  The Full path to the library (null terminated).
**
**	Return Value:	If the function succeeds, the return value is a TRUE,
**					otherwise the return value is FALSE and LAPI_LastError will be set to:
**.					LAPI_E_INVALIDARG		- if the szLibName is null.
**					LAPI_E_LIB_LOAD_FAILED	- if library dosen't exist/ library dosen't have the "Initialize" function.
**					LAPI_E_INIT_TEST_LIB_FAILED - if the Initialize function indicates a faiure to initialie.
*/
Bool LAPILoadTestLib(char* szLibName)
{
	TEST_LIB_INIT_PROC *proc = NULL;
	PRLibrary* hLib = NULL;
	Bool bInit = FALSE;
#ifdef XP_MAC
	const char *libPath = PR_GetLibraryPath();
	char *libDir = XP_STRDUP(szLibName);
	char *libName = strrchr(libDir, '/');
#endif

	if (!szLibName)
	{
		LAPI_LastError = LAPI_E_INVALIDARG;
		return FALSE;
	}

#ifdef XP_MAC
		
		if (libName != NULL)
		{
			libName[1] = '\0';
			PR_SetLibraryPath(libDir);
		}
		
		hLib = PR_LoadLibrary(szLibName);
		
		if (libName != NULL)
			PR_SetLibraryPath(libPath);
		
		XP_FREE(libDir);

#else
		hLib = PR_LoadLibrary(szLibName);
#endif
	if (!hLib)
	{
		LAPI_LastError = LAPI_E_LIB_LOAD_FAILED;
		return FALSE;
	}
	*proc =
	  (TEST_LIB_INIT_PROC *)PR_FindSymbol(hLib, "Initialize");
	if (!proc)
	{
		PR_UnloadLibrary(hLib);
		LAPI_LastError = LAPI_E_LIB_LOAD_FAILED;
		return FALSE;
	}
	
	bInit = (*proc)(NULL);
	if (!bInit)
	{
		PR_UnloadLibrary(hLib);
		LAPI_LastError = LAPI_E_INIT_TEST_LIB_FAILED;
		return FALSE;
	}

	return TRUE;
}

