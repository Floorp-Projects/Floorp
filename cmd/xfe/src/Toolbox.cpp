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
/* Name:		Toolbox.cpp												*/
/* Description:	XFE_Toolbox component source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include "Toolbox.h"
#include "IconGroup.h"

#include "ToolboxItem.h"

#include <Xfe/ToolBox.h>

#define BOTTOM_ICON		DTB_bottom_group
#define HORIZONTAL_ICON DTB_horizontal_group
#define LEFT_ICON		DTB_left_group
#define RIGHT_ICON		DTB_right_group
#define TOP_ICON		DTB_top_group
#define VERTICAL_ICON	DTB_vertical_group

// Invoked when one of the toolbars is closed
const char * XFE_Toolbox::toolbarClose = "XFE_Toolbox::toolbarClose";

// Invoked when one of the toolbars is opened
const char * XFE_Toolbox::toolbarOpen = "XFE_Toolbox::toolbarOpen";

// Invoked when one of the toolbars is snapped
const char * XFE_Toolbox::toolbarSnap = "XFE_Toolbox::toolbarSnap";

// Invoked when one of the toolbars is swapped
const char * XFE_Toolbox::toolbarSwap = "XFE_Toolbox::toolbarSwap";

//////////////////////////////////////////////////////////////////////////
XFE_Toolbox::XFE_Toolbox(XFE_Component *	top_level,
						 Widget				parent)
	: XFE_Component(top_level)
{
	m_toolbox = NULL;

	createMain(parent);
}
//////////////////////////////////////////////////////////////////////////
XFE_Toolbox::~XFE_Toolbox()
{
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbox::getToolBoxWidget()
{
	return m_toolbox;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbox::getLogoWidget()
{
	// write me
    return NULL;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbox::addDragDescendant(Widget descendant)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XfeToolBoxAddDragDescendant(m_widget,descendant);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbox::removeDragDescendant(Widget descendant)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XfeToolBoxRemoveDragDescendant(m_widget,descendant);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbox::toggleItemState(Widget item)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XfeToolBoxItemToggleOpen(m_widget,item);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbox::setItemPosition(Widget item,int position)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsAlive(item) );

	XfeToolBoxItemSetPosition(m_widget,item,position);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbox::setItemOpen(Widget item,XP_Bool open)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsAlive(item) );

	XfeToolBoxItemSetOpen(m_widget,item,open);
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_Toolbox::stateOfItem(Widget item)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsAlive(item) );

	return (XP_Bool) XfeToolBoxItemGetOpen(m_widget,item);
}
//////////////////////////////////////////////////////////////////////////
int
XFE_Toolbox::positionOfItem(Widget item)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsAlive(item) );

//	return XfeToolBoxItemGetPosition(m_widget,item);
	return XfeToolBoxItemGetIndex(m_widget,item);
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbox::tabOfItem(Widget item,XP_Bool opened)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsAlive(item) );

	return XfeToolBoxItemGetTab(m_widget,item,(Boolean) opened);
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//
// Item methods
//
//////////////////////////////////////////////////////////////////////////
Cardinal
XFE_Toolbox::getNumItems()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	Cardinal		item_count;

	XtVaGetValues(m_widget,XmNitemCount,&item_count,NULL);

	return item_count;
}
//////////////////////////////////////////////////////////////////////////
WidgetList
XFE_Toolbox::getItems()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	WidgetList		items;
	
	XtVaGetValues(m_widget,XmNitems,&items,NULL);

	return items;
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolboxItem *	
XFE_Toolbox::getItemAtIndex(Cardinal index)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( index < getNumItems() );

	XP_ASSERT( XfeIsAlive(m_widget) );

	Widget item = XfeToolBoxItemGetByIndex(m_widget,index);

	XP_ASSERT( XfeIsAlive(item) );

	return (XFE_ToolboxItem *) XfeUserData(item);
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolboxItem *
XFE_Toolbox::firstOpenItem()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	Cardinal	num_items = getNumItems();
	Cardinal	i;

	// Make sure toolbox has items
	if (!num_items)
	{
		return NULL;
	}

	for (i = 0; i < num_items; i++)
	{
		Widget item = XfeToolBoxItemGetByIndex(m_widget,i);

		if (XfeIsAlive(item) && XfeToolBoxItemGetOpen(m_widget,item))
		{
			return (XFE_ToolboxItem *) XfeUserData(item);
		}
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolboxItem *
XFE_Toolbox::firstManagedItem()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	Cardinal	num_items = getNumItems();
	Cardinal	i;

	// Make sure toolbox has items
	if (!num_items)
	{
		return NULL;
	}

	for (i = 0; i < num_items; i++)
	{
		Widget item = XfeToolBoxItemGetByIndex(m_widget,i);

		if (XfeIsAlive(item) && XtIsManaged(item))
		{
			return (XFE_ToolboxItem *) XfeUserData(item);
		}
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolboxItem *
XFE_Toolbox::firstOpenAndManagedItem()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	Cardinal	num_items = getNumItems();
	Cardinal	i;

	// Make sure toolbox has items
	if (!num_items)
	{
		return NULL;
	}

	for (i = 0; i < num_items; i++)
	{
		Widget item = XfeToolBoxItemGetByIndex(m_widget,i);

		if (XfeIsAlive(item) && XtIsManaged(item) &&
			XfeToolBoxItemGetOpen(m_widget,item))
		{
			return (XFE_ToolboxItem *) XfeUserData(item);
		}
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbox::createMain(Widget parent)
{
	XP_ASSERT( XfeIsAlive(parent) );

	// The main frame
	m_widget = XtVaCreateWidget("toolBox",
								xfeToolBoxWidgetClass,
								parent,
								XmNusePreferredWidth,		False,
//								XmNusePreferredHeight,		True,
								XmNusePreferredHeight,		False,
								XmNheight,					200,
								NULL);

	// Create all the tab icons
	Pixel	fg = XfeForeground(m_widget);
	Pixel	bg = XfeBackground(m_widget);
	Widget	shell = XfeAncestorFindByClass(parent,
										   shellWidgetClass,
										   XfeFIND_ANY);

	IconGroup_createAllIcons(&BOTTOM_ICON,shell,fg,bg);
	IconGroup_createAllIcons(&HORIZONTAL_ICON,shell,fg,bg);
	IconGroup_createAllIcons(&LEFT_ICON,shell,fg,bg);
	IconGroup_createAllIcons(&RIGHT_ICON,shell,fg,bg);
	IconGroup_createAllIcons(&TOP_ICON,shell,fg,bg);
	IconGroup_createAllIcons(&VERTICAL_ICON,shell,fg,bg);

	XtVaSetValues(
		m_widget,
		XmNbottomPixmap,			BOTTOM_ICON.pixmap_icon.pixmap,
		XmNbottomRaisedPixmap,		BOTTOM_ICON.pixmap_mo_icon.pixmap,
		XmNtopPixmap,				TOP_ICON.pixmap_icon.pixmap,
		XmNtopRaisedPixmap,			TOP_ICON.pixmap_mo_icon.pixmap,
		XmNleftPixmap,				LEFT_ICON.pixmap_icon.pixmap,
		XmNleftRaisedPixmap,		LEFT_ICON.pixmap_mo_icon.pixmap,
		XmNrightPixmap,				RIGHT_ICON.pixmap_icon.pixmap,
		XmNrightRaisedPixmap,		RIGHT_ICON.pixmap_mo_icon.pixmap,
		XmNverticalPixmap,			VERTICAL_ICON.pixmap_icon.pixmap,
		XmNverticalRaisedPixmap, 	VERTICAL_ICON.pixmap_mo_icon.pixmap,
		XmNhorizontalPixmap,		HORIZONTAL_ICON.pixmap_icon.pixmap,
		XmNhorizontalRaisedPixmap,	HORIZONTAL_ICON.pixmap_mo_icon.pixmap,
		NULL);

#if notyet
	// Set the hand cursor
	Cursor hand_cursor = XfeCursorGetDragHand(parent);

	if (hand_cursor != None)
	{
		XtVaSetValues(m_widget,XmNdragCursor,hand_cursor,NULL);
	}
#endif

	// Add new item callback
	XtAddCallback(m_widget,
				  XmNnewItemCallback,
				  &XFE_Toolbox::newItemCallback,
				  (XtPointer) this);

	// Add swap callback
	XtAddCallback(m_widget,
				  XmNswapCallback,
				  &XFE_Toolbox::swapCallback,
				  (XtPointer) this);


	// Add snap callback
	XtAddCallback(m_widget,
				  XmNsnapCallback,
				  &XFE_Toolbox::snapCallback,
				  (XtPointer) this);

	// Add close callback
	XtAddCallback(m_widget,
				  XmNcloseCallback,
				  &XFE_Toolbox::closeCallback,
				  (XtPointer) this);

	// Add open callback
	XtAddCallback(m_widget,
				  XmNopenCallback,
				  &XFE_Toolbox::openCallback,
				  (XtPointer) this);


	installDestroyHandler();
}
//////////////////////////////////////////////////////////////////////////

//
// Returns True if the toolbox is needed.  The toolbox is only needed if 
// at least 1 item is managed or 1 tab is managed.
//
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_Toolbox::isNeeded()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	return XfeToolBoxIsNeeded(m_widget);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Toolbox::newItemCallback(Widget		/* w */,
							 XtPointer	clientData,
							 XtPointer	callData)
{
	XFE_Toolbox *				tb = (XFE_Toolbox *) clientData;
	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) callData;

	XP_ASSERT( tb != NULL );

	// Add a tool tip to the tab
	fe_WidgetAddToolTips(cbs->closed_tab);
	fe_WidgetAddToolTips(cbs->opened_tab);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Toolbox::closeCallback(Widget		/* w */,
						   XtPointer	clientData,
						   XtPointer	callData)
{
	XFE_Toolbox *				tb = (XFE_Toolbox *) clientData;

	XP_ASSERT( tb != NULL );

	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) callData;

	XP_ASSERT( cbs != NULL );

	XFE_ToolboxItem *		item = tb->getItemAtIndex(cbs->index);

	XP_ASSERT( item != NULL );

	// Send a message that will perform an action.
	tb->notifyInterested(XFE_Toolbox::toolbarClose,(void *) item);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Toolbox::openCallback(Widget		/* w */,
						  XtPointer		clientData,
						  XtPointer		callData)
{
	XFE_Toolbox *				tb = (XFE_Toolbox *) clientData;

	XP_ASSERT( tb != NULL );

	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) callData;

	XP_ASSERT( cbs != NULL );

	XFE_ToolboxItem *		item = tb->getItemAtIndex(cbs->index);

	XP_ASSERT( item != NULL );

	// Send a message that will perform an action.
	tb->notifyInterested(XFE_Toolbox::toolbarOpen,(void *) item);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Toolbox::swapCallback(Widget	/* w */,
						  XtPointer clientData,
						  XtPointer	callData)
{
	XFE_Toolbox *					tb = (XFE_Toolbox *) clientData;
	
	XfeToolBoxSwapCallbackStruct *	cbs = 
		(XfeToolBoxSwapCallbackStruct *) callData;

	XFE_ToolboxItem *		item = 
		(XFE_ToolboxItem *) XfeUserData(cbs->swapped);

	XP_ASSERT( item != NULL );

	// Send a message that will perform an action.
	tb->notifyInterested(XFE_Toolbox::toolbarSwap,(void *) item);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Toolbox::snapCallback(Widget	/* w */,
						  XtPointer clientData,
						  XtPointer	callData)
{
	XFE_Toolbox *				tb = (XFE_Toolbox *) clientData;

	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) callData;

	XFE_ToolboxItem *		item = 
		(XFE_ToolboxItem *) XfeUserData(cbs->item);

	XP_ASSERT( item != NULL );

	// Send a message that will perform an action.
	tb->notifyInterested(XFE_Toolbox::toolbarSnap,(void *) item);
}
//////////////////////////////////////////////////////////////////////////
