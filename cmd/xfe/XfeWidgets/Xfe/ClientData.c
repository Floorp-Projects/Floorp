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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/ClientData.c>										*/
/* Description:	Manager of Arbitrary Client Data for widgets & gadget.	*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/ClientData.h>
#include <Xfe/Linked.h>
#include <Xfe/XfeP.h>

/*
 * How it works.
 *
 * The basic idea is to allocate a structure and associate it with the
 * hash key (the Widget pointer) using the client side X context manager.
 *
 * I originally used the item's Window as the hash key, but then I 
 * realized it would considerably simply things to use the Widget pointer
 * itself.  That is why there is all the parent_info hackery in the code
 * below.  Im #ifdef-ing it in in case we ever need to use the actual Window
 * XID as the has key.
 */


/*
 * HASH_KEY - use the widget itself for hashing.
 */

#define HASH_KEY(w) ((XID) w)

/*
 * Using the window for the has key would require the widget or gadget's
 * parent to be realized.  This would complicate the adding and changing
 * if tip/doc strings significantl.
 *
 * I looked at the Xlib source for the XSaveContext() and XFindContext().
 * There is nothing special about using an XID (a Window) as the hash 
 * key.  It is simply a key.
 *
 * Using the Widget pointer instead of the Window will work just as well
 * with the following 2 caveats:
 *
 * 1.  The has function might not be as effecient.  No big deal.
 *
 * 2.  If a random Window happens to be numerically identical to a Widget
 *     pointer, these will hash to the same value if-and-only-of the same
 *     unique context is used (XUniqueContext()).  
 *
 *     Since context management happens on the client side, this will not
 *     be a problem since the unique context (_xfe_tt_manager_context)
 *     used for the tool tip magic, is not available outside this module.
 *
 * One alternative solution would be to install realize (the first mapping)
 * event handlers for Widgets or gadget parents and install the tooltips
 * there.  However, this seems like too much work given the above solution
 * works well.  Also, this would not allow the caller to modify tooltips
 * until the widgets were realized - which would suck.
 */


#if 0
#define DEBUG_CLIENT_DATA yes
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* _XfeCDItemInfoRec													*/
/* 																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	/* Item */
	Widget							item;

	/* The item's key */
	XfeClientDataKey				item_key;


	/* The client's data */
	XtPointer						client_data;

	/* The client's free function */
	XfeClientDataFreeFunc			free_func;

	/* Used for gadgets only */
	XfeLinkNode						gadget_node;

#if PARENT_INFO
	/* Used for manager's only */
	XfeLinked						children_list;
#endif /* PARENT_INFO */

} _XfeCDItemInfoRec,*_XfeCDItemInfo;
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeCDItemInfo		WidgetGetInfo			(Widget,XfeClientDataKey);
static void					WidgetAddInfo			(Widget,XfeClientDataKey,
													 _XfeCDItemInfo);
static void					WidgetRemoveInfo		(Widget,_XfeCDItemInfo);

#if PARENT_INFO
/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeCDItemInfo		GadgetGetInfo			(Widget,XfeClientDataKey);
static XfeLinkNode			GadgetAddInfo			(Widget,
													 _XfeCDItemInfo,
													 _XfeCDItemInfo);
static void					GadgetRemoveInfo		(Widget,_XfeCDItemInfo);
#endif /* PARENT_INFO */

/*----------------------------------------------------------------------*/
/*																		*/
/* Item (Widget or Gadget)  functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				ItemDestroyCB			(Widget,XtPointer,XtPointer);
static void				ItemFreeInfo			(_XfeCDItemInfo);
static _XfeCDItemInfo	ItemAllocateInfo		(Widget);
static _XfeCDItemInfo	ItemGetInfo				(Widget,XfeClientDataKey);

#if PARENT_INFO
/*----------------------------------------------------------------------*/
/*																		*/
/* Manager XfeLinked children list functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean	ChildrenListTestFunc		(XtPointer,XtPointer);
#endif /* PARENT_INFO */

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeCDItemInfo
WidgetGetInfo(Widget w,XfeClientDataKey key)
{
	_XfeCDItemInfo	info = NULL;
	int				find_result;

 	assert( w != NULL );
 	assert( key != 0 );
/* 	assert( XtIsWidget(w) ); */

	find_result = XFindContext(XtDisplay(w),
							   HASH_KEY(w),
							   (XContext) key,
							   (XPointer *) &info);
	

	if (find_result != 0)
	{
		info = NULL;
	}

	return info;
}
/*----------------------------------------------------------------------*/
static void
WidgetAddInfo(Widget w,XfeClientDataKey key,_XfeCDItemInfo info)
{
	int save_result;

	assert( w != NULL );
	assert( key != 0 );
	assert( info != NULL );
	assert( _XfeIsAlive(w) );
/* 	assert( XtIsWidget(w) ); */

	/* Make sure the info is not already installed */
	assert( XFindContext(XtDisplay(w),
						 HASH_KEY(w),
						 (XContext) key,
						 (XPointer *) &info) != 0);
	
	save_result = XSaveContext(XtDisplay(w),
							   HASH_KEY(w),
							   (XContext) key,
							   (XPointer) info);
	
	assert( save_result == 0 );
}
/*----------------------------------------------------------------------*/
static void
WidgetRemoveInfo(Widget w,_XfeCDItemInfo info)
{
	int delete_result;

	assert( w != NULL );
	assert( info != NULL );
	assert( info->item_key != 0 );

#ifdef DEBUG_CLIENT_DATA
	printf("WidgetRemoveInfo(%s,window = %p)\n",XtName(w),HASH_KEY(w));
#endif

	delete_result = XDeleteContext(XtDisplay(w),
								   HASH_KEY(w),
								   (XContext) info->item_key);

	assert( delete_result == 0 );
}
/*----------------------------------------------------------------------*/

#if PARENT_INFO
/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeCDItemInfo
GadgetGetInfo(Widget w,XfeClientDataKey key)
{
	_XfeCDItemInfo		info = NULL;
	_XfeCDItemInfo		parent_info = NULL;

	assert( w != NULL );
	assert( key != 0 );
	assert( XmIsGadget(w) );

	/* Look for the parent info */
	parent_info = WidgetGetInfo(_XfeParent(w),key);

	if (parent_info != NULL)
	{
		XfeLinkNode node = XfeLinkedFind(parent_info->children_list,
										 ChildrenListTestFunc,
										 (XtPointer) w);

		if (node != NULL)
		{
			info = (_XfeCDItemInfo) XfeLinkNodeItem(node);
		}
	}

	return info;
}
/*----------------------------------------------------------------------*/
static XfeLinkNode
GadgetAddInfo(Widget w,_XfeCDItemInfo parent_info,_XfeCDItemInfo info)
{
	assert( w != NULL );
	assert( _XfeIsAlive(w) );
	assert( XmIsGadget(w) );
	assert( parent_info != NULL );
	assert( info != NULL );

	assert( parent_info->children_list != NULL );

	/* Make sure the item is not already on the list */
	assert( XfeLinkedFind(parent_info->children_list,
						  ChildrenListTestFunc,
						  (XtPointer) w) == NULL);

	/* Add this gadget to the tool tip children list */
	return XfeLinkedInsertAtTail(parent_info->children_list,info);
}
/*----------------------------------------------------------------------*/
static void
GadgetRemoveInfo(Widget w,_XfeCDItemInfo info)
{
	_XfeCDItemInfo		parent_info = NULL;

	assert( w != NULL );
	assert( XmIsGadget(w) );
	assert( info != NULL );
	assert( info->item_key != 0 );

	/* Look for the parent info */
	parent_info = WidgetGetInfo(_XfeParent(w),info->item_key);
	
	assert( parent_info != NULL );
	
	assert( info->gadget_node != NULL );

	assert( XfeLinkedFind(parent_info->children_list,
						  ChildrenListTestFunc,
						  (XtPointer) w) == info->gadget_node);
			
	/* Remove the item info from the list */
	XfeLinkedRemoveNode(parent_info->children_list,
						info->gadget_node);
}
/*----------------------------------------------------------------------*/
#endif /* PARENT_INFO */

/*----------------------------------------------------------------------*/
/*																		*/
/* Item (Widget or Gadget)  functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ItemDestroyCB(Widget w,XtPointer client_data,XtPointer call_data)
{
 	_XfeCDItemInfo	info = (_XfeCDItemInfo) client_data;

	assert( info != NULL );

	/* Invoke the client's free function if needed */
	if (info->free_func != NULL)
	{
		(*info->free_func)(w,info->client_data);
	}

#if PARENT_INFO
 	if (XmIsGadget(w))
	{
		/* Remove the info */
		GadgetRemoveInfo(w,info);

		/* Free the item info */
		ItemFreeInfo(info);
	}
	else
	{
		/* Remove the info */
		WidgetRemoveInfo(w,info);

		/* Free the info */
		ItemFreeInfo(info);
	}
#else
	/* Remove the info */
	WidgetRemoveInfo(w,info);
	
	/* Free the info */
	ItemFreeInfo(info);
#endif /* PARENT_INFO */
}
/*----------------------------------------------------------------------*/
static _XfeCDItemInfo
ItemAllocateInfo(Widget w)
{
	_XfeCDItemInfo	info = NULL;

	assert( w != NULL );
	assert( _XfeIsAlive(w) );

	info = (_XfeCDItemInfo) XtMalloc(sizeof(_XfeCDItemInfoRec) * 1);

	/* Initialize the members */
	info->item			= w;
	info->item_key		= 0;
	info->client_data	= NULL;
	info->free_func		= NULL;

	info->gadget_node	= NULL;

#if PARENT_INFO
	info->children_list	= NULL;
#endif /* PARENT_INFO */

	/* Garbage collection */
	XtAddCallback(w,XmNdestroyCallback,ItemDestroyCB,(XtPointer) info);
	
	return info;
}
/*----------------------------------------------------------------------*/
static void
ItemFreeInfo(_XfeCDItemInfo info)
{
	assert( info != NULL );

#if PARENT_INFO
	/* Destroy the children list if needed */
	if (info->children_list != NULL)
	{
		XfeLinkedDestroy(info->children_list,NULL,NULL);
	}
#endif /* PARENT_INFO */

	XtFree((char *) info);
}
/*----------------------------------------------------------------------*/
static _XfeCDItemInfo
ItemGetInfo(Widget w,XfeClientDataKey key)
{
	_XfeCDItemInfo info = NULL;
	
	assert( w != NULL );
	assert( key != 0 );

#if PARENT_INFO
 	if (XmIsGadget(w))
	{
		info = GadgetGetInfo(w,key);
	}
	else
	{
		info = WidgetGetInfo(w,key);
	}
#else
	info = WidgetGetInfo(w,key);
#endif /* PARENT_INFO */

	return info;
}
/*----------------------------------------------------------------------*/

#if PARENT_INFO
/*----------------------------------------------------------------------*/
/*																		*/
/* Manager XfeLinked children list functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean
ChildrenListTestFunc(XtPointer item,XtPointer client_data)
{
	_XfeCDItemInfo	info = (_XfeCDItemInfo) item;
	Widget			w = (Widget) client_data;

	assert( info != NULL );

	return (info->item == w);
}
/*----------------------------------------------------------------------*/
#endif /* PARENT_INFO */

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeClientData public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeClientDataAdd(Widget					w,
				 XfeClientDataKey		key,
				 XtPointer				client_data,
				 XfeClientDataFreeFunc	free_func)
{
	_XfeCDItemInfo	info = NULL;
	
	assert( w != NULL );
	assert( key != 0 );

#ifdef DEBUG_CLIENT_DATA
	printf("XfeClientDataAdd(%s)\n",XtName(w));
#endif

	/* Look for the item info */
	info = ItemGetInfo(w,key);

	/* Allocate the info if needed */
	if (info == NULL)
	{
		info = ItemAllocateInfo(w);
	}

	assert( info != NULL );

#if PARENT_INFO
	/* Allocate info according to item type */
 	if (XmIsGadget(w))
	{
		_XfeCDItemInfo		parent_info = NULL;
		XfeLinkNode			node = NULL;

		/* Look for the parent's info */
		parent_info = WidgetGetInfo(_XfeParent(w),key);

		/* Allocate the parent info if needed */
		if (parent_info == NULL)
		{
			parent_info = ItemAllocateInfo(_XfeParent(w));

			WidgetAddInfo(_XfeParent(w),key,parent_info);

			/* Construct the children list */
			parent_info->children_list = XfeLinkedConstruct();
		}

		assert( parent_info != NULL );

		/* Add info to gadget */
		node = GadgetAddInfo(w,parent_info,info);

		/* Assign the gadget node */
		info->gadget_node = node;
	}
	else
	{
		WidgetAddInfo(w,key,info);
	}
#else
	WidgetAddInfo(w,key,info);
#endif /* PARENT_INFO */

	/* Assign the item */
	info->item = w;

	/* Assign the item key */
	info->item_key = key;

	/* Assign the client data */
	info->client_data = client_data;

	/* Assign the client free function */
	info->free_func = free_func;
}
/*----------------------------------------------------------------------*/
/* extern */ XtPointer
XfeClientDataRemove(Widget w,XfeClientDataKey key)
{
	XtPointer		client_data = NULL;
	_XfeCDItemInfo	info = NULL;

	assert( w != NULL );
	assert( key != 0 );
	
#ifdef DEBUG_CLIENT_DATA
	printf("XfeClientDataGet(%s)\n",XtName(w));
#endif

	/* Obtain the item's info */
	info = ItemGetInfo(w,key);
	
	assert( info != NULL );

	/* Save the client data so we can return it later */
	client_data = info->client_data;

#if PARENT_INFO
	/* Remove info according to item type */
 	if (XmIsGadget(w))
	{
 		GadgetRemoveInfo(w,info);
	}
	else
	{
		WidgetRemoveInfo(w,info);
	}
#else
	WidgetRemoveInfo(w,info);
#endif /* PARENT_INFO */

	return client_data;
}
/*----------------------------------------------------------------------*/
/* extern */ XtPointer
XfeClientDataGet(Widget w,XfeClientDataKey key)
{
	_XfeCDItemInfo	info = NULL;
	
	assert( w != NULL );
	assert( key != 0 );

#ifdef DEBUG_CLIENT_DATA
	printf("XfeClientDataGet(%s)\n",XtName(w));
#endif

	/* Obtain the item's info */
	info = ItemGetInfo(w,key);

	if (!info)
	{
		return NULL;
	}
	
	assert( info->item == w );

	return info->client_data;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeClientDataSet(Widget					w,
				 XfeClientDataKey		key,
				 XtPointer				client_data,
				 XfeClientDataFreeFunc	free_func)
{
	_XfeCDItemInfo	info = NULL;
	
	assert( w != NULL );
	assert( key != 0 );

#ifdef DEBUG_CLIENT_DATA
	printf("XfeClientDataSet(%s)\n",XtName(w));
#endif

	/* Obtaon the item's info */
	info = ItemGetInfo(w,key);

	assert( info != NULL );

	assert( info->item == w );
	assert( info->item_key == key );

	/* Assign the client data */
	info->client_data = client_data;

	/* Assign the client free function */
	info->free_func = free_func;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeClientDataKey
XfeClientGetUniqueKey(void)
{
	return (XfeClientDataKey) XUniqueContext();
}
/*----------------------------------------------------------------------*/
