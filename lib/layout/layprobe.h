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

#ifndef layprobe_h__
#define layprobe_h__

#include "structs.h"

/* API Element types */
#define LAPI_UNKNOWN			-1
#define LAPI_TEXTLINK		0
#define LAPI_TEXT			1
#define LAPI_HRULE			2
#define LAPI_IMAGE			3
#define LAPI_BULLET			4
#define LAPI_TABLE			5
#define LAPI_CELL			6
#define LAPI_EMBED			7
#define LAPI_JAVA			8
#define LAPI_OBJECT			9

		
#define LAPI_FIRST_FE		30
#define	LAPI_FE_TEXT			30
#define LAPI_FE_RADIOBUTTON	31
#define LAPI_FE_CHECKBOX		32
#define LAPI_FE_BUTTON		33
#define LAPI_FE_COMBOBOX		34
#define LAPI_FE_LISTBOX		35
#define LAPI_LAST_FE			35

#define LAPI_FRAME			50

/* API Error codes */
#define LAPI_E_NOTIMPL				-1000
#define LAPI_E_UNEXPECTED			-1001
#define LAPI_E_INVALIDARG			-1002
#define LAPI_E_INVALIDBROWSER		-1003
#define LAPI_E_INVALIDFRAME			-1004
#define LAPI_E_INVALIDELEMENT		-1005
#define LAPI_E_NOTSUPPORTED			-1006
#define LAPI_E_NOELEMENTS			-1007	
#define LAPI_E_OUTOFBOUNDS			-1008
#define LAPI_E_INVALIDPROP			-1009
#define LAPI_E_LIB_LOAD_FAILED		-1010
#define LAPI_E_INIT_TEST_LIB_FAILED	-1011
#define	LAPI_E_GENERAL_ERROR		-1012

#define LAPI_INVALID_NUM			-1

/* Callback function types and constatnts */
typedef void (*ID_NOTIFY_PT)(void*);
typedef void (*ELEMENT_NOTIFY_PT)(MWContext* FrameID, void* ElementID,
								  int32 xPos, int32 yPos, int32 KeysState);

#define BROWSER_NAVIGATION_COMPLETE	0
#define BROWSER_ON_QUIT				1
#define FRAME_DOCUMENT_COMPLETE		2
#define FRAME_ON_UNLOAD				3

#define ELEMENT_MOUSE_DOWN			4
#define ELEMENT_MOUSE_UP			5
#define ELEMENT_MOUSE_OVER			6
#define ELEMENT_MOUSE_OUT			7
#define ELEMENT_CLICK				8
#define ELEMENT_KILL_FOCUS			9
#define ELEMENT_SET_FOCUS			10
#define ELEMENT_CHANGE				11

#define MAX_CALLBACKS				15

/* Notification Events (same as JSEvent values) */
#define EVENT_MOUSEDOWN 0x00000001
#define EVENT_MOUSEUP   0x00000002
#define EVENT_MOUSEOVER 0x00000004      /* user is mousing over a link */
#define EVENT_MOUSEOUT  0x00000008      /* user is mousing out of a link */
#define EVENT_MOUSEMOVE 0x00000010  
#define EVENT_MOUSEDRAG 0x00000020
#define EVENT_CLICK     0x00000040      /* input element click in progress */
#define EVENT_DBLCLICK  0x00000080
#define EVENT_KEYDOWN   0x00000100
#define EVENT_KEYUP     0x00000200
#define EVENT_KEYPRESS	0x00000400
#define EVENT_DRAGDROP  0x00000800      /* not yet implemented */
#define EVENT_FOCUS     0x00001000      /* input focus event in progress */
#define EVENT_BLUR      0x00002000      /* loss of focus event in progress */
#define EVENT_SELECT    0x00004000      /* input field selection in progress */
#define EVENT_CHANGE    0x00008000      /* field value change in progress */
#define EVENT_RESET     0x00010000      /* form submit in progress */
#define EVENT_SUBMIT    0x00020000      /* form submit in progress */
#define EVENT_SCROLL    0x00040000      /* window is being scrolled */
#define EVENT_LOAD      0x00080000      /* layout parsed a loaded document */
#define EVENT_UNLOAD    0x00100000
#define EVENT_XFER_DONE	0x00200000      /* document has loaded */
#define EVENT_ABORT     0x00400000
#define EVENT_ERROR     0x00800000
#define EVENT_LOCATE	0x01000000
#define EVENT_MOVE	0x02000000
#define EVENT_RESIZE    0x04000000
#define EVENT_FORWARD   0x08000000
#define EVENT_HELP	0x10000000		/* for handling of help events */
#define EVENT_BACK      0x20000000


typedef struct {
    int32	    type;
    MWContext*	Context;
	LO_Element* lo_element;
    int32	    x,y;
    int32	    docx,docy;
    int32	    screenx,screeny;
} LAPIEventInfo;


#define LAPI_COPY_JS2API_EVENT(ei,jsevent)		\
		(ei)->type = (jsevent)->type;				\
		(ei)->lo_element = (jsevent)->lo_element;	\
		(ei)->x = (jsevent)->x;						\
		(ei)->y = (jsevent)->y;						\
		(ei)->docx = (jsevent)->docx;				\
		(ei)->docy = (jsevent)->docy;				\
		(ei)->screenx = (jsevent)->screenx;			\
		(ei)->screeny = (jsevent)->screeny;


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
#define LO_FLOAT		18

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
Bool LO_QA_GetTextAttributes( long probeID, void *attributes);

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

/**********************************************************
*********               Queries API                ********
**********************************************************/

Bool LAPILoadTestLib(char* szLibName);

int32 LAPIGetLastError();

Bool LAPIGetFrames(
	XP_List** lppContextList
); 	


Bool LAPIFrameGetStringProperty( 
	MWContext*		FrameID,
	char*			PropertyName,
	char**			lpszPropVal
);

Bool LAPIFrameGetNumProperty( 
	MWContext*		FrameID,
	char*			PropertyName,
	int32*			lpPropVal			
);


Bool LAPIFrameGetElements( 
	XP_List*	lpList,
	MWContext*	FrameID,
	int16		ElementLAPIType,
	char*		ElementName
);


LO_Element* LAPIFrameGetElementFromPoint( 
	MWContext * FrameID,
	int xPos,
	int yPos
);


LO_Element* LAPIGetFirstElement ( 
	MWContext * FrameID
);


LO_Element* LAPIGetNextElement	( 
	MWContext * FrameID,
	LO_Element* ElementID
);

  
LO_Element* LAPIGetPrevElement ( 
	MWContext * FrameID,
	LO_Element* ElementID
);

  
LO_Element* LAPIGetChildElement ( 
	MWContext * FrameID,
	LO_Element* ElementID
);


LO_Element* LAPIGetParentElement ( 
	MWContext * FrameID,
	LO_Element* ElementID
);


Bool LAPIElementGetStringProperty( 
	MWContext*		FrameID,
	LO_Element*		ElementID,
	char*			PropertyName,
	char**			lpszPropVal
);


Bool LAPIElementGetNumProperty( 
	MWContext*		FrameID,
	LO_Element*		ElementID,
	char*			PropertyName,
	int32*			lpPropVal			
);



void LAPIDestroyProbe(
	void* ProbeID
);

/**********************************************************
*********             Manipulation API             ********
**********************************************************/


Bool LAPIElementClick( 
	MWContext*	FrameID,
	void*		ElementID,
	int16		MouseButton
);


Bool LAPIElementSetText ( 
	MWContext*	FrameID,
	void*		ElementID,
	char*		Text
);


Bool LAPIElementSetState ( 
	MWContext*	FrameID,
	void*		ElementID,
	int8		Value
);


Bool LAPIElementScrollIntoView ( 
	MWContext*		FrameID,
	void*			ElementID,
	int xPos,
	int yPos
);


/**********************************************************
*********         API Callback regisration         ********
**********************************************************/

int32 LAPIRegisterNotifyCallback(
	ID_NOTIFY_PT*	lpFunc,
	int32			EventID
);

int32 LAPIRegisterElementCallback(
	ELEMENT_NOTIFY_PT*	lpFunc,
	int32				EventID
);

Bool LAPIUnregisterCallbackFunction(
	void* CallbackID
);

XP_List* LAPIGetCallbackFuncList(
	int32	EventID
);

Bool LAPINotificationHandler(
	LAPIEventInfo*	pEvent
);

#endif /* layprobe_h__ */