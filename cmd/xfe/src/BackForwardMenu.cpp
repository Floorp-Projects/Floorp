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
/*---------------------------------------*/
/*																		*/
/* Name:		FrameListMenu.cpp										*/
/* Description:	XFE_FrameListMenu component implementation.				*/
/*				These are the menu items that appear at the end of the	*/
/*																		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/


/* Date:		Sun Mar  2 01:34:13 PST 1997							*/
/*																		*/
/*----------------------------------------------------------------------*/

#include "BackForwardMenu.h"
#include "MozillaApp.h"
#include "View.h"
#include "Frame.h"
#include "felocale.h"
#include "intl_csi.h"

#include <Xm/PushBG.h>
#include <Xfe/Cascade.h>

#include "xpgetstr.h"			// for XP_GetString()

#define MAX_MENU_ITEM_LENGTH				40
#define MAX_NUM_HISTORY_MENU_ITEMS			15

extern int XFE_UNTITLED;

//////////////////////////////////////////////////////////////////////////
//
// Used to pass data to the callbacks
//
//////////////////////////////////////////////////////////////////////////
typedef struct
{
	XFE_BackForwardMenu *	object;		// Object
	History_entry *			entry;		// History entry
} _HistoryCallbackStruct;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
XFE_BackForwardMenu::XFE_BackForwardMenu(XFE_Frame *	frame,
										 Widget			cascade,
										 int			forward) :
	_frame(frame),
	_cascade(cascade),
	_forward(forward)
{
	// Obtain the submenu
	XtVaGetValues(_cascade,XmNsubMenuId,&_submenu,NULL);

	XP_ASSERT( _submenu != NULL );
	XP_ASSERT( XfeIsCascade(_cascade) );

	XtAddCallback(_cascade,
				  XmNdestroyCallback,
				  &XFE_BackForwardMenu::cascadeDestroyCB,
				  (XtPointer) this);
	
	XtAddCallback(_cascade,
				  XmNcascadingCallback,
				  &XFE_BackForwardMenu::cascadingCB,
				  (XtPointer) this);

	// Make the delay large to get the desired "hold" effect
	XtVaSetValues(_cascade,XmNmappingDelay,300,NULL);
}
//////////////////////////////////////////////////////////////////////////
XFE_BackForwardMenu::~XFE_BackForwardMenu()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::generate(Widget		cascade,
							  XtPointer		data,
							  XFE_Frame *	frame)
{
	XFE_BackForwardMenu * obj;
	
	obj = new XFE_BackForwardMenu(frame,cascade,(int) data);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Cascade callbacks
//
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::cascadeDestroyCB(Widget,XtPointer clientData,XtPointer)
{
	XFE_BackForwardMenu * obj = (XFE_BackForwardMenu*)clientData;
	
	XP_ASSERT( obj != NULL );
	
	delete obj;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::cascadingCB(Widget,XtPointer clientData,XtPointer)
{
	XFE_BackForwardMenu *obj = (XFE_BackForwardMenu*)clientData;

	XP_ASSERT( obj != NULL );
	
	obj->cascading();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::cascading()
{
	destroyItems();

	fillSubmenu(_forward);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::destroyItems()
{
	XP_ASSERT( XfeIsAlive(_cascade) );

	XfeCascadeDestroyChildren(_cascade);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::addItems(XP_List *			list,
							  XP_Bool			forward)
{
	int num_items = 0;

	while(list)
	{
		History_entry *  entry = (History_entry *) list->object;

		XP_ASSERT( entry != NULL );

		if (entry && (num_items < MAX_NUM_HISTORY_MENU_ITEMS))
		{
			addItem(num_items++,entry);
		}
	
		if(forward)
		{
			list = list->next;
		}
		else
		{
			list = list->prev;
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::fillSubmenu(XP_Bool forward)
{
	XP_ASSERT( _frame != NULL );

	MWContext * context = _frame->getContext();

	XP_ASSERT( context != NULL );

	if (!_frame || !context)
	{
		return;
	}
	
	// Get the session history list
	XP_List* list = SHIST_GetList(context);

	// Get the pointer to the current history entry
	History_entry * current_entry = context->hist.cur_doc_ptr;
	
	XP_List * current_node = XP_ListFindObject(list, current_entry);

	XP_ASSERT( list != NULL );

	// Make sure the history list is valid
	if (!list)
	{
		return;
	}
	
	list = list->prev;

	if (forward)
	{
		addItems(current_node->next,True);
	}
	else
	{
		addItems(current_node->prev,False);
	}
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Menu item callbacks
//
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::itemActivateCB(Widget,XtPointer clientData,XtPointer)
{
	_HistoryCallbackStruct * data = (_HistoryCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemActivate(data->entry);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::itemArmCB(Widget,XtPointer clientData,XtPointer)
{
	_HistoryCallbackStruct * data = (_HistoryCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemArm(data->entry);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::itemDisarmCB(Widget,XtPointer clientData,XtPointer)
{
	_HistoryCallbackStruct * data = (_HistoryCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemDisarm();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::itemDestroyCB(Widget,XtPointer clientData,XtPointer)
{
	_HistoryCallbackStruct * data = (_HistoryCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );

	XP_FREE(data);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::itemActivate(History_entry * entry)
{
	XP_ASSERT( _frame != NULL );

	MWContext *		context = _frame->getContext();
	URL_Struct *	url;

	url = SHIST_CreateURLStructFromHistoryEntry(context,entry);

	if (_frame->handlesCommand(xfeCmdOpenUrl,(void *) url))
	{
		_frame->doCommand(xfeCmdOpenUrl,(void *) url);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::itemArm(History_entry * entry)
{
	XP_ASSERT( _frame != NULL );

	_frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
							 (void*) entry->address);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::itemDisarm()
{
	XP_ASSERT( _frame != NULL );

	_frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
							 (void*) "");
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BackForwardMenu::addItem(int				position,
							 History_entry *	entry)
{
	XP_ASSERT( entry != NULL );

	// Make sure the title is valid
	if (!entry)
	{
		return;
	}

	char * title = entry->title ? entry->title : XP_GetString(XFE_UNTITLED);

	_HistoryCallbackStruct * data;

	// Create a new bookmark data structure for the callbacks
	data = XP_NEW_ZAP(_HistoryCallbackStruct);

	data->object	= this;
	data->entry		= entry;

	// Create the new item
	Widget item = XtVaCreateManagedWidget(xfeCmdOpenTargetUrl,
										  xmPushButtonGadgetClass,
										  _submenu,
										  NULL);
	// Add callbacks to item
	XtAddCallback(item,
				  XmNactivateCallback,
				  &XFE_BackForwardMenu::itemActivateCB,
				  (XtPointer) data);

	XtAddCallback(item,
				  XmNarmCallback,
				  &XFE_BackForwardMenu::itemArmCB,
				  (XtPointer) data);

	XtAddCallback(item,
				  XmNdisarmCallback,
				  &XFE_BackForwardMenu::itemDisarmCB,
				  (XtPointer) data);

	// Format the title
	char * buffer = PR_smprintf("%2d  %s",position + 1,title);

	XP_ASSERT( buffer != NULL );

	// Truncate the title and install it on the item
	if (buffer)
	{
		MWContext *			context = _frame->getContext();
		INTL_CharSetInfo	c = LO_GetDocumentCharacterSetInfo(context);
		XmFontList			font_list;
		
		INTL_MidTruncateString(INTL_GetCSIWinCSID(c), 
							   buffer, 
							   buffer,
							   MAX_MENU_ITEM_LENGTH);

		
		XmString label = fe_ConvertToXmString((unsigned char *) buffer,
											  INTL_GetCSIWinCSID(c), 
											  NULL, 
											  XmFONT_IS_FONT,
											  &font_list);

		if (label)
		{
			XtVaSetValues(item,XmNlabelString,label,NULL);
			
			XmStringFree(label);
		}

		// Free the allocated memory
		XP_FREE(buffer);
	}
}
//////////////////////////////////////////////////////////////////////////
