/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*---*- Mode: C++; tab-width: 4 -*--------------------------------------*/
/*																		*/
/*																		*/
/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		FrameListMenu.cpp										*/
/* Description:	XFE_FrameListMenu component implementation.				*/
/*																		*/
/* Created:		Ramiro Estrugo <ramiro@netscape.com>					*/
/* Date:		Fri Jan 30 01:13:11 PST 1998							*/
/*																		*/
/*----------------------------------------------------------------------*/

#include "RelatedLinksMenu.h"
#include "BookmarkBase.h"		// For pixmaps
#include "BrowserFrame.h"		// For fe_showBrowser()
#include "MozillaApp.h"
#include "View.h"
#include "felocale.h"
#include "intl_csi.h"

#if DO_IT_WITH_FANCY_PIXMAPS
#include <Xfe/BmButton.h>
#else
#include <Xm/PushB.h>
#endif

#include <Xfe/Cascade.h>

#include "xpgetstr.h"			// for XP_GetString()
#include "../../lib/xp/rl.h"		/* For related links */

#define MAX_MENU_ITEM_LENGTH				60
#define MAX_NUM_RELATED_LINKS				50

extern int XFE_UNTITLED;

//////////////////////////////////////////////////////////////////////////
//
// Used to pass data to the callbacks
//
//////////////////////////////////////////////////////////////////////////
typedef struct
{
	XFE_RelatedLinksMenu *	object;		// Object
	RL_Item 				rl_item;	// Related link item
	XP_Bool 				new_window;	// Open a new window ?
} _RL_ItemCallbackStruct;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
XFE_RelatedLinksMenu::XFE_RelatedLinksMenu(XFE_Frame *	frame,
										   Widget		cascade) :
	_frame(frame),
	_cascade(cascade),
	_rl_window(NULL)
{
	// Obtain the submenu
	XtVaGetValues(_cascade,XmNsubMenuId,&_submenu,NULL);

	XP_ASSERT( _submenu != NULL );
	XP_ASSERT( XfeIsCascade(_cascade) );

	XtAddCallback(_cascade,
				  XmNdestroyCallback,
				  &XFE_RelatedLinksMenu::cascadeDestroyCB,
				  (XtPointer) this);
	
	XtAddCallback(_cascade,
				  XmNcascadingCallback,
				  &XFE_RelatedLinksMenu::cascadingCB,
				  (XtPointer) this);
	
	// Make the delay 100
	XtVaSetValues(_cascade,XmNmappingDelay,100,NULL);

	_rl_window = RL_MakeRLWindow();
}
//////////////////////////////////////////////////////////////////////////
XFE_RelatedLinksMenu::~XFE_RelatedLinksMenu()
{
	if (_rl_window)
	{
		RL_DestroyRLWindow(_rl_window);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::generate(Widget		cascade,
							  XtPointer		/* data */,
							  XFE_Frame *	frame)
{
	XFE_RelatedLinksMenu * obj;
	
	obj = new XFE_RelatedLinksMenu(frame,cascade);

	// Store the BookmarkMenu instance in the quickfile button 
	// XmNinstancePointer.  This overrides the XmNinstancePointer 
	// installed by the XFE_Button class
	XtVaSetValues(cascade,XmNinstancePointer,(XtPointer) obj,NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::setRelatedLinksAddress(char * address)
{
	XP_ASSERT( address != NULL );

	if (!address)
	{
		return;
	}

	RL_SetRLWindowURL(_rl_window,address);

// #ifdef DEBUG_ramiro
// 	printf("RL_SetRLWindowURL(%p,%s)\n",_rl_window,address);
// #endif
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Cascade callbacks
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::cascadeDestroyCB(Widget,XtPointer clientData,XtPointer)
{
	XFE_RelatedLinksMenu * obj = (XFE_RelatedLinksMenu*)clientData;
	
	XP_ASSERT( obj != NULL );
	
	delete obj;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::cascadingCB(Widget,XtPointer clientData,XtPointer)
{
	XFE_RelatedLinksMenu *obj = (XFE_RelatedLinksMenu*)clientData;

	XP_ASSERT( obj != NULL );
	
	obj->cascading();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::cascading()
{
	destroyItems();
	
	XP_ASSERT( _frame != NULL );
	
	addItems(_submenu,_rl_window);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::destroyItems()
{
	XP_ASSERT( XfeIsAlive(_cascade) );

	XfeCascadeDestroyChildren(_cascade);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::addItems(Widget submenu,RL_Window rl_window)
{
	XP_ASSERT( XfeIsAlive(submenu) );
	XP_ASSERT( XmIsRowColumn(submenu) );
	XP_ASSERT( rl_window != NULL );

	if (!rl_window)
	{
		return;
	}

	RL_Item		rl_item;
	int			i = 0;

	for(rl_item = RL_WindowItems(rl_window), i = 0;
		rl_item != NULL; 
		rl_item = RL_NextItem(rl_item), i++)
	{
		XP_ASSERT( rl_item != NULL );

		if (rl_item && (i < MAX_NUM_RELATED_LINKS))
		{
			addItem(submenu,rl_item);
		}
	}
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Menu item callbacks
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::itemActivateCB(Widget,XtPointer clientData,XtPointer)
{
	_RL_ItemCallbackStruct * data = (_RL_ItemCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemActivate(data->rl_item,data->new_window);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::itemArmCB(Widget,XtPointer clientData,XtPointer)
{
	_RL_ItemCallbackStruct * data = (_RL_ItemCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemArm(data->rl_item);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::itemDisarmCB(Widget,XtPointer clientData,XtPointer)
{
	_RL_ItemCallbackStruct * data = (_RL_ItemCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemDisarm();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::itemDestroyCB(Widget,XtPointer clientData,XtPointer)
{
	_RL_ItemCallbackStruct * data = (_RL_ItemCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );

	XP_FREE(data);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::itemActivate(RL_Item rl_item,XP_Bool new_window)
{
	XP_ASSERT( _frame != NULL );
	XP_ASSERT( rl_item != NULL );

	if (!rl_item)
	{
		return;
	}
	
	char * address = RL_ItemUrl(rl_item);

	if (!address)
	{
		return;
	}
	
	URL_Struct * url = NET_CreateURLStruct(address,NET_DONT_RELOAD);

	if (new_window)
	{
		fe_showBrowser(FE_GetToplevelWidget(),NULL,NULL,url);
	}
	else
	{
		if (_frame->handlesCommand(xfeCmdOpenUrl,(void *) url))
		{
			_frame->doCommand(xfeCmdOpenUrl,(void *) url);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::itemArm(RL_Item rl_item)
{
	XP_ASSERT( _frame != NULL );
	XP_ASSERT( rl_item != NULL );

	if (!rl_item)
	{
		return;
	}
	
	char * address = RL_ItemUrl(rl_item);

	if (!address)
	{
		return;
	}

	_frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
							 (void*) address);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::itemDisarm()
{
	XP_ASSERT( _frame != NULL );

	_frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
							 (void*) "");
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::addItem(Widget submenu,RL_Item rl_item)
{
	XP_ASSERT( XfeIsAlive(submenu) );
	XP_ASSERT( XmIsRowColumn(submenu) );
	XP_ASSERT( rl_item != NULL );

	// Make sure the title is valid
	if (!rl_item)
	{
		return;
	}


	RL_ItemType		item_type = RL_GetItemType(rl_item);

// #ifdef DEBUG_ramiro
// 	printf("XFE_RelatedLinksMenu::addItem(%s,%s,%d)\n",
// 		   RL_ItemName(rl_item) ? RL_ItemName(rl_item) : "NULL",
// 		   RL_ItemUrl(rl_item) ? RL_ItemUrl(rl_item) : "NULL",
// 		   item_type);
// #endif

	if (item_type == RL_CONTAINER)
	{
		addContainer(submenu,rl_item);
	}
	else if(item_type == RL_SEPARATOR)
	{
		addSeparator(submenu,rl_item);
	}
	else if(item_type == RL_LINK)
	{
		addLink(submenu,rl_item,False);
	}
	else if(item_type == RL_LINK_NEW_WINDOW)
	{
		addLink(submenu,rl_item,True);
	}
	else
	{
		XP_ASSERT( 0 );
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::addSeparator(	Widget submenu,RL_Item /* rl_item */)
{
	XP_ASSERT( XfeIsAlive(submenu) );
	XP_ASSERT( XmIsRowColumn(submenu) );

	Widget separator;

	separator = XmCreateSeparator(submenu,"separator",NULL,0);

	XtVaSetValues(separator,XmNshadowThickness,2,NULL);

	XtManageChild(separator);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::addContainer(Widget submenu,RL_Item rl_item)
{
	XP_ASSERT( XfeIsAlive(submenu) );
	XP_ASSERT( XmIsRowColumn(submenu) );
	XP_ASSERT( rl_item != NULL );

	if (!rl_item)
	{
		return;
	}

	RL_Item		rl_child = NULL;
	int			i = 0;
	Widget		child_cascade = NULL;
	Widget		child_submenu = NULL;

	XfeCreateCascadeAndSubmenu(submenu,
							   "relatedCascade",
							   "relatedSubmenu",
							   xmCascadeButtonWidgetClass,
							   &child_cascade,
							   &child_submenu);

	for(rl_child = RL_ItemChild(rl_item), i = 0;
		rl_child != NULL; 
		rl_child = RL_NextItem(rl_child), i++)
	{
		XP_ASSERT( rl_child != NULL );

		if (rl_child && (i < MAX_NUM_RELATED_LINKS))
		{
			addItem(child_submenu,rl_child);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RelatedLinksMenu::addLink(Widget	submenu,
							  RL_Item	rl_item,
							  XP_Bool	new_window)
{
	XP_ASSERT( XfeIsAlive(submenu) );
	XP_ASSERT( XmIsRowColumn(submenu) );

	char *	item_name = RL_ConvertItemName(rl_item, fe_LocaleCharSetID);
	char *	item_address = RL_ItemUrl(rl_item);

	// If theres no name and item_address, whats the point ?
	if (!item_name && !item_address)
	{
		return;
	}
	
	// Make sure the item_name is something
	if (!item_name)
	{
		item_name = item_address;
	}

	_RL_ItemCallbackStruct * data;

	// Create a new bookmark data structure for the callbacks
	data = XP_NEW_ZAP(_RL_ItemCallbackStruct);

	data->object	 = this;
	data->rl_item	 = rl_item;
	data->new_window = new_window;

#if DO_IT_WITH_FANCY_PIXMAPS
	// Create the new item
	Widget item = XtVaCreateManagedWidget(xfeCmdOpenTargetUrl,
										  xfeBmButtonWidgetClass,
										  submenu,
										  NULL);

	// Set the pixmaps if needed
	if (fe_globalPrefs.toolbar_style != BROWSER_TOOLBAR_TEXT_ONLY)
	{
		Pixmap		bookmarkPixmap = XFE_BookmarkBase::getBookmarkPixmap();
		Pixmap		bookmarkMask = XFE_BookmarkBase::getBookmarkMask();

		if (XfePixmapGood(bookmarkPixmap) && XfePixmapGood(bookmarkMask))
		{
			XtVaSetValues(item,
						  XmNlabelPixmap,		bookmarkPixmap,
						  XmNlabelPixmapMask,	bookmarkMask,
						  NULL);
		}
	}
#else
	// Create the new item
	Widget item = XtVaCreateManagedWidget(xfeCmdOpenTargetUrl,
										  xmPushButtonWidgetClass,
										  submenu,
										  NULL);
#endif

	XtRealizeWidget(item);

	// Add callbacks to item
	XtAddCallback(item,
				  XmNactivateCallback,
				  &XFE_RelatedLinksMenu::itemActivateCB,
				  (XtPointer) data);

	XtAddCallback(item,
				  XmNarmCallback,
				  &XFE_RelatedLinksMenu::itemArmCB,
				  (XtPointer) data);

	XtAddCallback(item,
				  XmNdisarmCallback,
				  &XFE_RelatedLinksMenu::itemDisarmCB,
				  (XtPointer) data);

	// Format the title
	char * buffer = (String) XtNewString(item_name);

	XP_ASSERT( buffer != NULL );

	// Truncate the title and install it on the item
	if (buffer)
	{
//		MWContext *			context = _frame->getContext();
//		INTL_CharSetInfo	c = LO_GetDocumentCharacterSetInfo(context);
		XmFontList			font_list;

		INTL_MidTruncateString(fe_LocaleCharSetID,
							   buffer, 
							   buffer,
							   MAX_MENU_ITEM_LENGTH);

		
		XmString label = fe_ConvertToXmString((unsigned char *) buffer,
											  fe_LocaleCharSetID,
											  NULL, 
											  XmFONT_IS_FONT,
											  &font_list);

		if (label)
		{
			XtVaSetValues(item,XmNlabelString,label,NULL);
			
			XmStringFree(label);
		}

		// Free the allocated memory
		XtFree(buffer);
	}
}
//////////////////////////////////////////////////////////////////////////
