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

/* Created by: Nisheeth Ranjan (nisheeth@netscape.com), March 1998 */

/* 
 * Defines the abstraction of a layout probe object that gives information about 
 * layout elements.  No mutilation of the layout elements is allowed by this
 * probe.  Each FE uses this API to satisfy requests for layout information. 
 *
 */

#include "structs.h"

/* The different types of layout elements */
#define LO_TEXT         1
#define LO_HRULE        3
#define LO_IMAGE        4
#define LO_BULLET       5
#define LO_FORM_ELE     6
#define LO_TABLE        8
#define LO_CELL         9
#define LO_EMBED        10
#define LO_JAVA         12
#define LO_OBJECT       14

typedef enum _ColorType {
LO_QA_BACKGROUND_COLOR,
LO_QA_FOREGROUND_COLOR,
LO_QA_BORDER_COLOR
} ColorType;

/* 
 * Constructor/Destructor for probe 
 *
 */

/* Probe state is created.  Probe position gets set to "nothing". */
long LO_QA_CreateProbe( MWContext *context );

/* Probe state is destroyed. */
void LO_QA_DestroyProbe( long probeID );


/*
 * Functions to set position of layout probe.  
 *
 */

/* Sets probe position to first layout element in document. */
Bool LO_QA_GotoFirstElement( long probeID );

/* 
	If the probe has just been created, the next element is the first layout element in the document.
	If there is a next element, the function returns TRUE and sets the probe position to the next element.
	If there is no next element, the function returns FALSE and resets the probe position to nothing. 
*/
Bool LO_QA_GotoNextElement( long probeID );

/* 	
	Sets probe position and returns TRUE or false based on the following table:

	Current Probe Position		New Probe Position					Return value
	----------------------		------------------					------------
	Table						First cell in table					TRUE
	Cell						First layout element in cell		TRUE
	Layer						First layout element in layer		TRUE
	Any other element			No change							FALSE	
 */

Bool LO_QA_GotoChildElement( long probeID );

/* 	
	Sets probe position and returns TRUE or false based on the following table:

	Current Probe Position		New Probe Position					Return value
	----------------------		------------------					------------	
	Cell within a table			Parent Table						TRUE
	Element within a cell		Parent Cell							TRUE
	Element within a layer		Parent Layer						TRUE
	Any other element			No change							FALSE	
 */

Bool LO_QA_GotoParentElement( long probeID );

/*
	Gets the layout element type that the probe is positioned on.
	Returns FALSE if the probe is not positioned on any element, else returns TRUE. 
*/
Bool LO_QA_GetElementType( long probeID, int *type );

/* 
 * Functions to return the current layout element's position and dimensions.
 *
 */

/* 
	Each of these functions return TRUE if the probe position is set to a layout element,
	otherwise, they return FALSE.
*/
Bool LO_QA_GetElementXPosition( long probeID, long *x );
Bool LO_QA_GetElementYPosition( long probeID, long *y );
Bool LO_QA_GetElementWidth( long probeID, long *width );
Bool LO_QA_GetElementHeight( long probeID, long *height );

Bool LO_QA_HasURL( long probeID, Bool *hasURL );
Bool LO_QA_HasText( long probeID, Bool *hasText );
Bool LO_QA_HasColor( long probeID, Bool *hasColor );
Bool LO_QA_HasChild( long probeID, Bool *hasChild );
Bool LO_QA_HasParent( long probeID, Bool *hasParent );

Bool LO_QA_GetTextLength( long probeID, long *length);
Bool LO_QA_GetText( long probeID, char *text, long length);

/*   Color Type                       Elements containing the color type 
     ----------                       ----------------------------------
	 LO_QA_BACKGROUND_COLOR           LO_TEXT, LO_BULLET, LO_CELL
	 LO_QA_FOREGROUND_COLOR           LO_TEXT, LO_BULLET
	 LO_QA_BORDER_COLOR               LO_TABLE

     Based on the element type and the color type, gets the color of the element
	 at which the probe is currently positioned.  Returns TRUE if successful,
	 FALSE if not.
*/
Bool LO_QA_GetColor( long probeID, long *color, ColorType type);