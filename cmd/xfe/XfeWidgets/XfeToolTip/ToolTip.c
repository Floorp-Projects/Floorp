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
/* Name:		<Xfe/ToolTip.c>											*/
/* Description:	XfeToolTip - TipString / DocString support.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/ToolTipP.h>
#include <Xfe/ToolTipShell.h>

#include <Xfe/Linked.h>

#include <Xm/LabelGP.h>
#include <Xm/PushBGP.h>

#define TOOL_TIP_SHELL_NAME		"ToolTipShell"
#define TOOL_TIP_INTERVAL		1000

#define TOOL_TIP_EVENT_MASK						\
( EnterWindowMask	|							\
  LeaveWindowMask	|							\
  ButtonPressMask	|							\
  ButtonReleaseMask	|							\
  KeyPressMask		| 							\
  KeyReleaseMask )


#if 0
#define DEBUG_TOOL_TIPS yes
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* _XfeTipItemInfoRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	Widget						item;

	/* TipString */
	Boolean						tip_string_enabled;
	XtCallbackRec				tip_string_callback;

	/* DocString */
	Boolean						doc_string_enabled;
	XtCallbackRec				doc_string_callback;

} _XfeTipItemInfoRec,*_XfeTipItemInfo;
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* _XfeTipManagerInfoRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	Widget		manager;
	XfeLinked	children_list;
} _XfeTipManagerInfoRec,*_XfeTipManagerInfo;
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget phony input functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		GadgetHackInputDispatch				(Widget);
static void		GadgetPhonyInputDispatch			(Widget,XEvent *,Mask);
static void		GadgetSaveRealInputDispatchMethod	(Widget);
static XmWidgetDispatchProc	GadgetGetRealInputDispatchMethod	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Manager functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeTipManagerInfo	ManagerGetInfo		(Widget);
static _XfeTipManagerInfo	ManagerAddInfo		(Widget);
static void					ManagerRemoveInfo	(Widget,_XfeTipManagerInfo);
static _XfeTipManagerInfo	ManagerAllocateInfo	(Widget);
static void					ManagerFreeInfo		(_XfeTipManagerInfo);
static void					ManagerDestroyCB	(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void					WidgetAdd			(Widget);
static void					WidgetRemove		(Widget);
static _XfeTipItemInfo		WidgetGetInfo		(Widget);
static _XfeTipItemInfo		WidgetAddInfo		(Widget);
static void					WidgetRemoveInfo	(Widget,_XfeTipItemInfo);

/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void					GadgetAdd			(Widget);
static void					GadgetRemove		(Widget);
static _XfeTipItemInfo		GadgetGetInfo		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Item (Widget or Gadget)  functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void					ItemDestroyCB		(Widget,XtPointer,XtPointer);
static void					ItemFreeInfo		(_XfeTipItemInfo);
static _XfeTipItemInfo		ItemAllocateInfo	(Widget);
static _XfeTipItemInfo		ItemGetInfo			(Widget);
static void					ItemGetTipString	(Widget,XmString *,Boolean *);
static void					ItemGetDocString	(Widget,XmString *,Boolean *);
static void					ItemPostToolTip		(Widget);
static void					ItemUnPostToolTip	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString enter / leave / cancel functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		TipStringEnterItem			(Widget,_XfeTipItemInfo);
static void		TipStringLeaveItem			(Widget,_XfeTipItemInfo);
static void		TipStringCancelItem			(Widget,_XfeTipItemInfo);

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString enter / leave / cancel functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DocStringEnterItem			(Widget,_XfeTipItemInfo);
static void		DocStringLeaveItem			(Widget,_XfeTipItemInfo);

/*----------------------------------------------------------------------*/
/*																		*/
/* Manager XfeLinked children list functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ChildrenListDestroyProc		(XtPointer,XtPointer);
static Boolean	ChildrenListTestFunc		(XtPointer,XtPointer);


/*----------------------------------------------------------------------*/
/*																		*/
/* Event handlers														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		WidgetStageOneEH		(Widget,XtPointer,XEvent *,Boolean *);

/*----------------------------------------------------------------------*/
/*																		*/
/* State Two timeout functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	StageTwoTimeout			(XtPointer,XtIntervalId *);
static void	StageTwoAddTimeout		(Widget);
static void	StageTwoRemoveTimeout	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget phony input functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
GadgetPhonyInputDispatch(Widget		w,
						 XEvent *	event,
						 Mask		event_mask)
{
	XmWidgetDispatchProc	input_dispatch = NULL;

	assert( XmIsGadget(w) );

	input_dispatch = GadgetGetRealInputDispatchMethod(w);

	assert( input_dispatch != NULL );

	/* Do the tip magic only if tip or doc strings are globally enabled */
	if (XfeToolTipGlobalGetTipStringEnabledState() ||
		XfeToolTipGlobalGetDocStringEnabledState())
	{
		_XfeTipManagerInfo	manager_info = NULL;
		Widget				pw = XtParent(w);

		/* Look for the manager info on the gadget's parent */
		manager_info = ManagerGetInfo(pw);

		/* Make sure the manager info exist */
		if (manager_info != NULL)
		{
			XfeLinkNode node = XfeLinkedFind(manager_info->children_list,
											 ChildrenListTestFunc,
											 (XtPointer) w);
			
			if (node != NULL)
			{
				_XfeTipItemInfo		item_info = 
					(_XfeTipItemInfo) XfeLinkNodeItem(node);

				assert( item_info != NULL );

				if (item_info != NULL)
				{
					Boolean do_tip_string = 
						XfeToolTipGlobalGetTipStringEnabledState() &&
						item_info->tip_string_enabled;

					Boolean do_doc_string = 
						XfeToolTipGlobalGetDocStringEnabledState() &&
						item_info->doc_string_enabled;

					/* Enter */
					if (event_mask & XmENTER_EVENT)
					{
						if (do_tip_string)
						{
							TipStringEnterItem(w,item_info);
						}
						
						if (do_doc_string)
						{
							DocStringEnterItem(w,item_info);
						}
					}
					/* Leave */
					else if (event_mask & XmLEAVE_EVENT)
					{
						if (do_tip_string)
						{
							TipStringLeaveItem(w,item_info);
						}
						
						if (do_doc_string)
						{
							DocStringLeaveItem(w,item_info);
						}
					}
					/*
					 * XmARM_EVENT
					 * XmMULTI_ARM_EVENT
					 * XmACTIVATE_EVENT
					 * XmMULTI_ACTIVATE_EVENT
					 * XmHELP_EVENT
					 * XmENTER_EVENT
					 * XmLEAVE_EVENT
					 * XmFOCUS_IN_EVENT
					 * XmFOCUS_OUT_EVENT
					 * XmBDRAG_EVENT
					 */
					else
					{
						if (do_tip_string)
						{
							TipStringCancelItem(w,item_info);
						}
					}
				} /* item_info != NULL */
			} /* node != NULL */
		} /* manager_info != NULL */
	} /* XfeToolTipGlobalGetTipStringEnabledState() == True */

	/* Invoke the original InputDispatch() method */
	(*input_dispatch)(w,event,event_mask);
}
/*----------------------------------------------------------------------*/
static void
GadgetHackInputDispatch(Widget g)
{
 	XmGadgetClass gadget_class = NULL;

	assert( XmIsGadget(g) );

	gadget_class = (XmGadgetClass) XtClass(g);

	/* Override the original input dispatch with the phony one */
	gadget_class->gadget_class.input_dispatch = GadgetPhonyInputDispatch;
}
/*----------------------------------------------------------------------*/

static XmWidgetDispatchProc _xm_real_pbg_input_dispatch = NULL;
static XmWidgetDispatchProc _xm_real_lg_input_dispatch = NULL;

static XmWidgetDispatchProc
GadgetGetRealInputDispatchMethod(Widget g)
{
	XmWidgetDispatchProc id = NULL;

	assert( _XfeIsAlive(g) ) ;
	assert( XmIsGadget(g) ) ;
	
	if (XmIsPushButtonGadget(g))
	{
		assert( _xm_real_pbg_input_dispatch != NULL );

		id = _xm_real_pbg_input_dispatch;
	}
	else if (XmIsLabelGadget(g))
	{
		assert( _xm_real_lg_input_dispatch != NULL );
		
		id = _xm_real_lg_input_dispatch;
	}
#ifdef DEBUG_ramiro
	else
	{
		assert( 0 );
	}
#endif

	return id;
}
/*----------------------------------------------------------------------*/
static void
GadgetSaveRealInputDispatchMethod(Widget w)
{
	assert( _XfeIsAlive(w) ) ;
	assert( XmIsGadget(w) ) ;
	
	if (XmIsPushButtonGadget(w))
	{
		/* Save the original XmPushButtonGadget input dispatch method */
		if (_xm_real_pbg_input_dispatch == NULL)
		{
			_xm_real_pbg_input_dispatch = 
				xmPushButtonGadgetClassRec.gadget_class.input_dispatch;
			
		}
	}
	else if (XmIsLabelGadget(w))
	{
		/* Save the original XmLabelGadget input dispatch method */
		if (_xm_real_lg_input_dispatch == NULL)
		{
			_xm_real_lg_input_dispatch = 
				xmLabelGadgetClassRec.gadget_class.input_dispatch;
			
		}
	}
#ifdef DEBUG_ramiro
	else
	{
		assert( 0 );
	}
#endif
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Manager functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XContext _xfe_tt_item_context = None;
static XContext _xfe_tt_manager_context = None;

/*----------------------------------------------------------------------*/
static _XfeTipManagerInfo
ManagerGetInfo(Widget w)
{
	_XfeTipManagerInfo	manager_info = NULL;

	assert( _XfeIsAlive(w) );
	assert( _XfeIsRealized(w) );
	assert( XmIsManager(w) );

	/* Make sure the manager context has been allocated */
	if (_xfe_tt_manager_context != None)
	{
		int find_result = XFindContext(XtDisplay(w),
									   XtWindow(w),
									   _xfe_tt_manager_context,
									   (XPointer *) &manager_info);

		if (find_result != 0)
		{
			manager_info = NULL;
		}
	}

	return manager_info;
}
/*----------------------------------------------------------------------*/
static _XfeTipManagerInfo
ManagerAllocateInfo(Widget w)
{
	_XfeTipManagerInfo	manager_info = NULL;

	assert( _XfeIsAlive(w) );
	assert( _XfeIsRealized(w) );
	assert( XmIsManager(w) );

	manager_info = (_XfeTipManagerInfo) 
		XtMalloc(sizeof(_XfeTipManagerInfoRec) * 1);
	
	manager_info->manager		= w;
	manager_info->children_list	= XfeLinkedConstruct();
	
	XtAddCallback(w,
				  XmNdestroyCallback,
				  ManagerDestroyCB,
				  (XtPointer) manager_info);
	
	assert( manager_info != NULL );
	
	return manager_info;
}
/*----------------------------------------------------------------------*/
static void
ManagerFreeInfo(_XfeTipManagerInfo manager_info)
{
	assert( manager_info != NULL );

	/* Destroy the children list if needed */
	if (manager_info->children_list)
	{
		XfeLinkedDestroy(manager_info->children_list,
						 ChildrenListDestroyProc,
						 NULL);
	}

	XtFree((char *) manager_info);
}
/*----------------------------------------------------------------------*/
static _XfeTipManagerInfo
ManagerAddInfo(Widget w)
{
	_XfeTipManagerInfo	manager_info = NULL;
	int						save_result;

	assert( _XfeIsAlive(w) );
	assert( _XfeIsRealized(w) );
	assert( XmIsManager(w) );

	/* Allocate the gadget context only once */
	if (_xfe_tt_manager_context == None)
	{
		_xfe_tt_manager_context = XUniqueContext();
	}

	assert( XFindContext(XtDisplay(w),
						 XtWindow(w),
						 _xfe_tt_manager_context,
						 (XPointer *) &manager_info) != 0);

	/* Allocate the manager info */
	manager_info = ManagerAllocateInfo(w);
	
	save_result = XSaveContext(XtDisplay(w),
							   XtWindow(w),
							   _xfe_tt_manager_context,
							   (XPointer) manager_info);
	
	assert( save_result == 0 );
	
	assert( manager_info != NULL );
	
	return manager_info;
}
/*----------------------------------------------------------------------*/
static void
ManagerRemoveInfo(Widget w,_XfeTipManagerInfo manager_info)
{
	int delete_result;

	assert( manager_info != NULL );
	assert( _xfe_tt_manager_context != None );

	delete_result = XDeleteContext(XtDisplay(w),
								   XtWindow(w),
								   _xfe_tt_manager_context);

	assert( delete_result == 0 );
}
/*----------------------------------------------------------------------*/
static void
ManagerDestroyCB(Widget w,XtPointer client_data,XtPointer call_data)
{
	_XfeTipManagerInfo	manager_info = (_XfeTipManagerInfo) client_data;

	assert( manager_info != NULL );

	ManagerRemoveInfo(w,manager_info);

	ManagerFreeInfo(manager_info);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
WidgetAdd(Widget w)
{
	_XfeTipItemInfo		item_info = NULL;

	assert( _XfeIsAlive(w) );
	assert( XtIsWidget(w) );

	/* Make sure the item is not already on the list */
	assert( WidgetGetInfo(w) == NULL );

	/* Add the item info the the widget */
	item_info = WidgetAddInfo(w);

	assert( item_info != NULL );

	/* Remove the tool tip event handler */
    XtRemoveEventHandler(w,
						 TOOL_TIP_EVENT_MASK,
						 FALSE,
						 WidgetStageOneEH,
						 NULL);

	/* Insert the tool tip event handler */
    XtInsertEventHandler(w,
						 TOOL_TIP_EVENT_MASK,
						 FALSE,
						 WidgetStageOneEH,
						 NULL,
						 XtListHead);
}
/*----------------------------------------------------------------------*/
static void
WidgetRemove(Widget w)
{
	_XfeTipItemInfo		item_info = NULL;

	assert( _XfeIsAlive(w) );
	assert( XtIsWidget(w) );

	/* Add the item info the the widget */
	item_info = WidgetGetInfo(w);

	assert( item_info != NULL );

	/* Remove the tool tip info from the widget */
 	WidgetRemoveInfo(w,item_info);

	/* Free the tool tip info */
	ItemFreeInfo(item_info);

	/* Remove the tool tip event handler */
    XtRemoveEventHandler(w,
						 TOOL_TIP_EVENT_MASK,
						 FALSE,
						 WidgetStageOneEH,
						 NULL);
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
WidgetGetInfo(Widget w)
{
	_XfeTipItemInfo	item_info = NULL;

	assert( _XfeIsAlive(w) );
	assert( _XfeIsRealized(w) );
	assert( XtIsWidget(w) );

	/* Make sure the item context has been allocated */
	if (_xfe_tt_item_context != None)
	{
		int find_result = XFindContext(XtDisplay(w),
									   XtWindow(w),
									   _xfe_tt_item_context,
									   (XPointer *) &item_info);

		if (find_result != 0)
		{
			item_info = NULL;
		}
	}

	return item_info;
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
WidgetAddInfo(Widget w)
{
	_XfeTipItemInfo		item_info = NULL;
	int					save_result;

	assert( _XfeIsAlive(w) );
	assert( _XfeIsRealized(w) );
	assert( XtIsWidget(w) );

	/* Allocate the gadget context only once */
	if (_xfe_tt_item_context == None)
	{
		_xfe_tt_item_context = XUniqueContext();
	}

	assert( XFindContext(XtDisplay(w),
						 XtWindow(w),
						 _xfe_tt_item_context,
						 (XPointer *) &item_info) != 0);

	/* Allocate the item info */
	item_info = ItemAllocateInfo(w);

	assert( item_info != NULL );

#if 1 
	item_info->tip_string_enabled = True;
	item_info->doc_string_enabled = True;
#endif

	save_result = XSaveContext(XtDisplay(w),
							   XtWindow(w),
							   _xfe_tt_item_context,
							   (XPointer) item_info);
	
	assert( save_result == 0 );
	
	assert( item_info != NULL );
	
	return item_info;
}
/*----------------------------------------------------------------------*/
static void
WidgetRemoveInfo(Widget w,_XfeTipItemInfo item_info)
{
	int delete_result;

	assert( item_info != NULL );
	assert( _xfe_tt_item_context != None );

	delete_result = XDeleteContext(XtDisplay(w),
								   XtWindow(w),
								   _xfe_tt_item_context);

	assert( delete_result == 0 );
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
GadgetAdd(Widget w)
{
	Widget				pw = NULL;
	_XfeTipManagerInfo	manager_info = NULL;
	_XfeTipItemInfo		item_info = NULL;

	assert( _XfeIsAlive(w) );
	assert( XmIsGadget(w) );

	pw = XtParent(w);

	/* Look for the manager info */
	manager_info = ManagerGetInfo(pw);
	
	/* Add the manager info if needed */
	if (manager_info == NULL)
	{
		manager_info = ManagerAddInfo(pw);
	}

	assert( manager_info != NULL );

	/* Allocate the item info */
	item_info = ItemAllocateInfo(w);

	assert( item_info != NULL );

#if 1 
	item_info->tip_string_enabled = True;
	item_info->doc_string_enabled = True;
#endif

	/* Make sure the item is not already on the list */
	assert( XfeLinkedFind(manager_info->children_list,
						  ChildrenListTestFunc,
						  (XtPointer) w) == NULL);

	/* Add this gadget to the tool tip children list */
	XfeLinkedInsertAtTail(manager_info->children_list,item_info);

	GadgetSaveRealInputDispatchMethod(w);
	
	GadgetHackInputDispatch(w);
}
/*----------------------------------------------------------------------*/
static void
GadgetRemove(Widget w)
{
	Widget				pw = NULL;
	_XfeTipManagerInfo	manager_info = NULL;
	_XfeTipItemInfo		item_info = NULL;
	XfeLinkNode			node;

	assert( XmIsGadget(w) );

	pw = XtParent(w);

	/* Look for the manager info */
	manager_info = ManagerGetInfo(pw);
	
	assert( manager_info != NULL );

	node = XfeLinkedFind(manager_info->children_list,
						 ChildrenListTestFunc,
						 (XtPointer) w);
			
	assert( node != NULL );

	/* Remove the item info from the list */
	item_info = (_XfeTipItemInfo) 
		XfeLinkedRemoveNode(manager_info->children_list,node);

	assert( item_info != NULL );

	/* Free the item info */
	ItemFreeInfo(item_info);
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
GadgetGetInfo(Widget w)
{
	_XfeTipItemInfo		item_info = NULL;
	Widget				pw = NULL;
	_XfeTipManagerInfo	manager_info = NULL;

	assert( XmIsGadget(w) );

	pw = XtParent(w);

	/* Look for the manager info */
	manager_info = ManagerGetInfo(pw);

	if (manager_info != NULL)
	{
		XfeLinkNode node = XfeLinkedFind(manager_info->children_list,
										 ChildrenListTestFunc,
										 (XtPointer) w);

		if (node != NULL)
		{
			item_info = (_XfeTipItemInfo) XfeLinkNodeItem(node);
		}
	}

	return item_info;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Item (Widget or Gadget)  functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ItemDestroyCB(Widget w,XtPointer client_data,XtPointer call_data)
{
	if (XmIsGadget(w))
	{
		GadgetRemove(w);
	}
	else
	{
		WidgetRemove(w);
	}
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
ItemAllocateInfo(Widget w)
{
	_XfeTipItemInfo	item_info = NULL;

	assert( _XfeIsAlive(w) );
	assert( _XfeIsRealized(w) );

	item_info = (_XfeTipItemInfo) XtMalloc(sizeof(_XfeTipItemInfoRec) * 1);

	/* Initialize the members */
	item_info->item							= w;

	item_info->tip_string_enabled			= False;
 	item_info->doc_string_enabled			= False;

	item_info->tip_string_callback.callback	= NULL;
	item_info->tip_string_callback.closure	= NULL;

	item_info->doc_string_callback.callback	= NULL;
	item_info->doc_string_callback.closure	= NULL;

	XtAddCallback(w,
				  XmNdestroyCallback,
				  ItemDestroyCB,
				  (XtPointer) item_info);
	
	return item_info;
}
/*----------------------------------------------------------------------*/
static void
ItemFreeInfo(_XfeTipItemInfo item_info)
{
	assert( item_info != NULL );

	XtFree((char *) item_info);
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
ItemGetInfo(Widget w)
{
	_XfeTipItemInfo item_info = NULL;
	
	assert( _XfeIsAlive(w) );

	if (XmIsGadget(w))
	{
		item_info = GadgetGetInfo(w);
	}
	else
	{
		item_info = WidgetGetInfo(w);
	}

	return item_info;
}
/*----------------------------------------------------------------------*/
static void
ItemGetTipString(Widget w,XmString * xmstr_out,Boolean * need_to_free_out)
{
	_XfeTipItemInfo	item_info;

	assert( xmstr_out != NULL );
	assert( need_to_free_out != NULL );

	*xmstr_out = NULL;
	*need_to_free_out = False;

	assert( _XfeIsAlive(w) );
	
	item_info = ItemGetInfo(w);
	
	assert( item_info != NULL );

	/* Invoke the tool tip callback if present */
	if (item_info->tip_string_callback.callback != NULL)
	{
		XfeToolTipLabelCallbackStruct cbs;

		cbs.reason = 0;
		cbs.event = NULL;
		cbs.label_return = NULL;
		cbs.need_to_free_return = False;
		
		(*item_info->tip_string_callback.callback)(w,
												 item_info->tip_string_callback.closure,
												 &cbs);
		
		*xmstr_out = cbs.label_return;
		*need_to_free_out = cbs.need_to_free_return;
	}
	/* Check resources directly */
	else
	{
		*xmstr_out = XfeSubResourceGetWidgetXmStringValue(w, 
														  XmNtipString, 
														  XmCTipString);
		
		/*
		 * No need to free this string.  The Xt resource destructor
		 * should take care of freeing this memory.
		 */
		*need_to_free_out = False;
	}
}
/*----------------------------------------------------------------------*/
static void
ItemGetDocString(Widget w,XmString * xmstr_out,Boolean * need_to_free_out)
{
	_XfeTipItemInfo	item_info;

	assert( xmstr_out != NULL );
	assert( need_to_free_out != NULL );

	*xmstr_out = NULL;
	*need_to_free_out = False;

	assert( _XfeIsAlive(w) );
	
	item_info = ItemGetInfo(w);
	
	assert( item_info != NULL );

	/* Invoke the doc string callback if present */
	if (item_info->doc_string_callback.callback != NULL)
	{
		XfeToolTipLabelCallbackStruct cbs;
		
		cbs.reason = 0;
		cbs.event = NULL;
		cbs.label_return = NULL;
		cbs.need_to_free_return = False;
		
		(*item_info->doc_string_callback.callback)(w,
												   item_info->doc_string_callback.closure,
												   &cbs);
		
		*xmstr_out = cbs.label_return;
		*need_to_free_out = cbs.need_to_free_return;
	}
	/* Check resources directly */
	else
	{
		*xmstr_out = XfeSubResourceGetWidgetXmStringValue(w, 
														  XmNdocumentationString, 
														  XmCDocumentationString);
		
		/*
		 * No need to free this string.  The Xt resource destructor
		 * should take care of freeing this memory.
		 */
		*need_to_free_out = False;
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString enter / leave / cancel functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget		_xfe_tt_stage_one_target = NULL;
static Widget		_xfe_tt_stage_two_target = NULL;
static XtIntervalId	_xfe_tt_timer_id = 0;

static void
TipStringEnterItem(Widget w,_XfeTipItemInfo item_info)
{
	assert( _XfeIsAlive(w) );

	if (_XfeToolTipIsLocked())
	{
#ifdef DEBUG_TOOL_TIPS
		printf("TipStringEnterItem(%s): tool tips are locked - abort\n",
			   XtName(w));
#endif
	}

	_XfeToolTipLock();

	_xfe_tt_stage_one_target = w;

	StageTwoAddTimeout(w);

#ifdef DEBUG_TOOL_TIPS
	printf("TipStringEnterItem(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/
static void
TipStringLeaveItem(Widget w,_XfeTipItemInfo item_info)
{
	assert( _XfeIsAlive(w) );
	
	/* Check for stage one */

	/* If a target exists, reset it so that stage two aborts */
	if (_XfeIsAlive(_xfe_tt_stage_one_target))
	{
		_xfe_tt_stage_one_target = NULL;
	}

	/* Remove the timeout if its still active */
	StageTwoRemoveTimeout(w);

	/* Check for stage two */
	if (_XfeIsAlive(_xfe_tt_stage_two_target))
	{
		/* Set the stage two target */
		_xfe_tt_stage_two_target = NULL;

		ItemUnPostToolTip(w);
	}

	_XfeToolTipUnlock();

#ifdef DEBUG_TOOL_TIPS
	printf("TipStringLeaveItem(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/
static void
TipStringCancelItem(Widget w,_XfeTipItemInfo item_info)
{
#ifdef DEBUG_TOOL_TIPS
	printf("TipStringCancelItem(%s)\n",XtName(w));
#endif

	TipStringLeaveItem(w,item_info);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString enter / leave / cancel functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
/* static Widget		_xfe_tt_stage_one_target = NULL; */
/* static Widget		_xfe_tt_stage_two_target = NULL; */
/* static XtIntervalId	_xfe_tt_timer_id = 0; */

static void
DocStringEnterItem(Widget w,_XfeTipItemInfo item_info)
{
	assert( _XfeIsAlive(w) );

#ifdef DEBUG_TOOL_TIPS
	printf("DocStringEnterItem(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/
static void
DocStringLeaveItem(Widget w,_XfeTipItemInfo item_info)
{
	assert( _XfeIsAlive(w) );
	
#ifdef DEBUG_TOOL_TIPS
	printf("DocStringLeaveItem(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Item Post / UnPost ToolTip functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean		_xfe_tt_posted = False;

static void
ItemPostToolTip(Widget w)
{
	Widget		shell = NULL;
	Widget		label = NULL;

	XmString	xmstr = NULL;
	Boolean		need_to_free = False;

	assert( _XfeIsAlive(w) );

	shell =_XfeToolTipGetShell(w);
	
	assert( _XfeIsAlive(shell) );

	label = _XfeToolTipGetLabel(w);

	assert( _XfeIsAlive(label) );

	ItemGetTipString(w,&xmstr,&need_to_free);

#if DEBUG
	if (xmstr == NULL)
	{
		xmstr = XmStringCreateLocalized("YOU SUCK!");
		
		need_to_free = True;
	}
#endif
 
	if (xmstr != NULL)
	{
 		XfeLabelSetString(label,xmstr);

		/* Free the string if needed */
		if (need_to_free)
		{
			XmStringFree(xmstr);
		}
		
		XfeBypassShellUpdateSize(shell);

		_xfe_tt_posted = True;

		XtPopup(shell,XtGrabNone);
	}

#ifdef DEBUG_TOOL_TIPS
	printf("ItemPostToolTip(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/
static void
ItemUnPostToolTip(Widget w)
{
	if (_xfe_tt_posted)
	{
		Widget shell = _XfeToolTipGetShell(w);

		_xfe_tt_posted = True;

		if (_XfeIsAlive(shell))
		{
			XtPopdown(shell);
		}
	}

#ifdef DEBUG_TOOL_TIPS
	printf("ItemUnPostToolTip(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Manager XfeLinked children list functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ChildrenListDestroyProc(XtPointer item,XtPointer client_data)
{
	_XfeTipItemInfo	item_info = (_XfeTipItemInfo) item;

	assert( item_info != NULL );

	ItemFreeInfo(item_info);
}
/*----------------------------------------------------------------------*/
static Boolean
ChildrenListTestFunc(XtPointer item,XtPointer client_data)
{
	_XfeTipItemInfo	item_info = (_XfeTipItemInfo) item;
	Widget			w = (Widget) client_data;

	assert( item_info != NULL );

	return (item_info->item == w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* State Two timeout functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
StageTwoTimeout(XtPointer closure,XtIntervalId * id)
{
    Widget 		w = (Widget) closure;
	
	/*
	 * Always clear timer_id, so we don't kill someone else's timer
	 * by accident later.
	 */
	_xfe_tt_timer_id = 0;

	/*
	 * Post the tooltip if a target is still alive.
	 *
	 * A dead or missing target means the tooltip operation was aborted.
	 * The user could have boinked or left the target.
	 */
	if (_XfeIsAlive(_xfe_tt_stage_one_target))
	{
		assert( w == _xfe_tt_stage_one_target);
		
		/* Reset the stage one target */
		_xfe_tt_stage_one_target = NULL;
		
		/* Set the stage two target */
		_xfe_tt_stage_two_target = w;

		ItemPostToolTip(w);
	}
}
/*----------------------------------------------------------------------*/
static void
StageTwoAddTimeout(Widget w)
{
	assert( _XfeIsAlive(w) );

	_xfe_tt_timer_id = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
									   TOOL_TIP_INTERVAL,
									   StageTwoTimeout,
									   (XtPointer) w);
}
/*----------------------------------------------------------------------*/
static void
StageTwoRemoveTimeout(Widget w)
{
	if (_xfe_tt_timer_id != 0)
	{
		XtRemoveTimeOut(_xfe_tt_timer_id);

		_xfe_tt_timer_id = 0;
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Event handlers														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
WidgetStageOneEH(Widget		w,
				 XtPointer	client_data,
				 XEvent *	event,
				 Boolean *	cont)
{
	/* Do the tip magic only if tip or doc strings are globally enabled */
	if (XfeToolTipGlobalGetTipStringEnabledState() ||
		XfeToolTipGlobalGetDocStringEnabledState())
	{
		_XfeTipItemInfo item_info = WidgetGetInfo(w);

		assert( item_info != NULL );

		if (item_info != NULL)
		{
			Boolean do_tip_string = 
				XfeToolTipGlobalGetTipStringEnabledState() &&
				item_info->tip_string_enabled;
			
			Boolean do_doc_string = 
				XfeToolTipGlobalGetDocStringEnabledState() &&
				item_info->doc_string_enabled;

			/* Enter */
			if (event->type == EnterNotify)
			{
				if (do_tip_string)
				{
					TipStringEnterItem(w,item_info);
				}
				
				if (do_doc_string)
				{
					DocStringEnterItem(w,item_info);
				}
			}
			/* Leave */
			else if (event->type == LeaveNotify)
			{
				if (do_tip_string)
				{
					TipStringLeaveItem(w,item_info);
				}
				
				if (do_doc_string)
				{
					DocStringLeaveItem(w,item_info);
				}
			}
			/* Cancel */
			else
			{
				if (do_tip_string)
				{
					TipStringCancelItem(w,item_info);
				}
			}
		} /* item_info != NULL */
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTip Private Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Tool tip shell access												*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget _xfe_tt_shell = NULL;

/* extern */ Widget
_XfeToolTipGetShell(Widget w)
{
	/* Create to global tool tip shell if needed */
	if (!_xfe_tt_shell)
	{
		Widget aw = XfeAncestorFindApplicationShell(w);

		assert( _XfeIsAlive(aw) );

		_xfe_tt_shell = XfeCreateToolTipShell(aw,
											  TOOL_TIP_SHELL_NAME,
											  NULL,
											  0);
	}

	return _xfe_tt_shell;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
_XfeToolTipGetLabel(Widget w)
{
	Widget shell = _XfeToolTipGetShell(w);

	assert( _XfeIsAlive(shell) );
	assert( _XfeIsAlive(XfeToolTipShellGetLabel(shell)) );

	return XfeToolTipShellGetLabel(shell);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Lock / Unlock functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean _xfe_tt_locked = False;

/* extern */ void
_XfeToolTipLock(void)
{
	_xfe_tt_locked = True;
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeToolTipUnlock(void)
{
	_xfe_tt_locked = False;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeToolTipIsLocked(void)
{
	return _xfe_tt_locked;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipAddTipString(Widget w)
{
	assert( _XfeIsAlive(w) );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeToolTipAddTipString(%s)\n",XtName(w));
#endif

	if (XmIsGadget(w))
	{
		GadgetAdd(w);
	}
	else
	{
		WidgetAdd(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipRemoveTipString(Widget w)
{
	assert( _XfeIsAlive(w) );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeToolTipRemoveTipString(%s)\n",XtName(w));
#endif

	if (XmIsGadget(w))
	{
		GadgetRemove(w);
	}
	else
	{
		WidgetRemove(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolTipGetTipStringEnabledState(Widget w)
{
	_XfeTipItemInfo item_info;
	
	assert( _XfeIsAlive(w) );

	item_info = ItemGetInfo(w);

	if (item_info != NULL)
	{
		return item_info->tip_string_enabled;
	}

	return False;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipSetTipStringEnabledState(Widget w,Boolean state)
{
	_XfeTipItemInfo item_info;
	
	assert( _XfeIsAlive(w) );

	item_info = ItemGetInfo(w);

	assert( item_info != NULL );
	
	if (item_info != NULL)
	{
		item_info->tip_string_enabled = state;
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipSetTipStringCallback(Widget			w,
							   XtCallbackProc	callback,
							   XtPointer		client_data)
{
	_XfeTipItemInfo item_info;
	
	assert( _XfeIsAlive(w) );

	item_info = ItemGetInfo(w);

	if (item_info != NULL)
	{
		item_info->tip_string_callback.callback = callback;
		item_info->tip_string_callback.closure = client_data;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipClearTipStringCallback(Widget w)
{
	XfeToolTipSetTipStringCallback(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString global enabled / disable functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean _xfe_tip_string_global_enabled = False;

/* extern */ void
XfeToolTipGlobalSetTipStringEnabledState(Boolean state)
{
	_xfe_tip_string_global_enabled = state;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolTipGlobalGetTipStringEnabledState(void)
{
	return _xfe_tip_string_global_enabled;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Check whether the global tooltip is showing							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipIsTipStringShowing(void)
{
	/* writeme */
	assert( 0 );
	return False;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipAddDocString(Widget w)
{
	assert( _XfeIsAlive(w) );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeToolTipAddDocString(%s)\n",XtName(w));
#endif

	if (XmIsGadget(w))
	{
		GadgetAdd(w);
	}
	else
	{
		WidgetAdd(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipRemoveDocString(Widget w)
{
	assert( _XfeIsAlive(w) );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeToolTipRemoveDocString(%s)\n",XtName(w));
#endif

	if (XmIsGadget(w))
	{
		GadgetRemove(w);
	}
	else
	{
		WidgetRemove(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolTipGetDocStringEnabledState(Widget w)
{
	_XfeTipItemInfo item_info;
	
	assert( _XfeIsAlive(w) );

	item_info = ItemGetInfo(w);

	if (item_info != NULL)
	{
		return item_info->doc_string_enabled;
	}

	return False;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipSetDocStringEnabledState(Widget w,Boolean state)
{
	_XfeTipItemInfo item_info;
	
	assert( _XfeIsAlive(w) );

	item_info = ItemGetInfo(w);

	assert( item_info != NULL );
	
	if (item_info != NULL)
	{
		item_info->doc_string_enabled = state;
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipSetDocStringCallback(Widget			w,
							   XtCallbackProc	callback,
							   XtPointer		client_data)
{
	_XfeTipItemInfo item_info;
	
	assert( _XfeIsAlive(w) );

	item_info = ItemGetInfo(w);

	if (item_info != NULL)
	{
		item_info->doc_string_callback.callback = callback;
		item_info->doc_string_callback.closure = client_data;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipClearDocStringCallback(Widget w)
{
	XfeToolTipSetDocStringCallback(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString global enabled / disable functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean _xfe_doc_string_global_enabled = False;

/* extern */ void
XfeToolTipGlobalSetDocStringEnabledState(Boolean state)
{
	_xfe_doc_string_global_enabled = state;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolTipGlobalGetDocStringEnabledState(void)
{
	return _xfe_doc_string_global_enabled;
}
/*----------------------------------------------------------------------*/

