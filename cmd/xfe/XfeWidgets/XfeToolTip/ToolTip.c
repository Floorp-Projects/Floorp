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

#include <Xfe/ClientData.h>

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
	/* Item */
	Widget							item;

	/* Does the item have tip string support ? */
	Boolean							tip_string_installed;

	/* Does the item have doc string support ? */
	Boolean							doc_string_installed;

	/* TipString */
	Boolean							tip_string_enabled;
	XfeTipStringObtainCallback		tip_string_obtain_callback;
	XtPointer						tip_string_obtain_data1;

	/* DocString */
	Boolean							doc_string_enabled;
	XfeTipStringObtainCallback		doc_string_obtain_callback;
	XtPointer						doc_string_obtain_data1;

	/* DocString callback */
	XfeDocStringCallback			doc_string_callback;
	XtPointer						doc_string_data1;

} _XfeTipItemInfoRec,*_XfeTipItemInfo;
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
/* Widget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void					WidgetAddStageOneEH		(Widget);
static void					WidgetRemoveStageOneEH	(Widget);

static _XfeTipItemInfo		WidgetAddTipString		(Widget);
static _XfeTipItemInfo		WidgetAddDocString		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo		GadgetAddTipString		(Widget);
static _XfeTipItemInfo		GadgetAddDocString		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Item (Widget or Gadget)  functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				ItemFreeInfo			(_XfeTipItemInfo);
static void				ClientDataFreeFunc		(Widget,XtPointer);
static _XfeTipItemInfo	ItemAllocateInfo		(Widget);
static void				ItemGetTipString		(Widget,XmString *,Boolean *);
static void				ItemGetDocString		(Widget,XmString *,Boolean *);
static void				ItemPostToolTip			(Widget);
static void				ItemUnPostToolTip		(Widget);
static void				ItemUninstallTipString	(Widget,_XfeTipItemInfo);
static void				ItemUninstallDocString	(Widget,_XfeTipItemInfo);

static _XfeTipItemInfo	ItemGetInfo				(Widget);
static void				ItemAddInfo				(Widget,_XfeTipItemInfo);
static void				ItemRemoveInfo			(Widget,_XfeTipItemInfo);
static _XfeTipItemInfo	ItemFindOrAddInfo		(Widget);

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
/* ClientData key functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XfeClientDataKey GetClientDataKey	(void);

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
	if (XfeTipStringGlobalGetEnabledState() ||
		XfeDocStringGlobalGetEnabledState())
	{
		_XfeTipItemInfo info = info = ItemGetInfo(w);

		if (info != NULL)
		{
			Boolean do_tip_string = 
				XfeTipStringGlobalGetEnabledState() &&
				info->tip_string_enabled;
			
			Boolean do_doc_string = 
				XfeDocStringGlobalGetEnabledState() &&
				info->doc_string_enabled;
			
			/* Enter */
			if (event_mask & XmENTER_EVENT)
			{
				if (do_tip_string)
				{
					TipStringEnterItem(w,info);
				}
				
				if (do_doc_string)
				{
					DocStringEnterItem(w,info);
				}
					}
			/* Leave */
			else if (event_mask & XmLEAVE_EVENT)
			{
				if (do_tip_string)
				{
							TipStringLeaveItem(w,info);
				}
				
				if (do_doc_string)
				{
					DocStringLeaveItem(w,info);
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
					TipStringCancelItem(w,info);
				}
			}
		} /* info != NULL */
	} /* XfeTipStringGlobalGetEnabledState() == True */

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
/* Widget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
WidgetAddStageOneEH(Widget w)
{
	assert( w != NULL );
	assert( _XfeIsAlive(w) );
	assert( XtIsWidget(w) );

	/* Remove the event handler so that we start with a clean item */
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
WidgetRemoveStageOneEH(Widget w)
{
 	assert( w != NULL );
	assert( XtIsWidget(w) );

	/* Remove the event handler so that we start with a clean item */
    XtRemoveEventHandler(w,
						 TOOL_TIP_EVENT_MASK,
						 FALSE,
						 WidgetStageOneEH,
						 NULL);
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
WidgetAddTipString(Widget w)
{
	_XfeTipItemInfo		info = NULL;

	assert( w != NULL );
	assert( _XfeIsAlive(w) );
	assert( XtIsWidget(w) );

	/* Find or add the info */
	info = ItemFindOrAddInfo(w);
	
	assert( info != NULL );

	assert( info->tip_string_installed == False );

	/* Mark the tip string info as installed */
	info->tip_string_installed = True;

	/* If the doc string info was already installed, we are done */
	if (info->doc_string_installed)
	{
		return info;
	}

	/* Add the stage one event handler */
	WidgetAddStageOneEH(w);

	return info;
}
/*----------------------------------------------------------------------*/
static void
ItemUninstallTipString(Widget w,_XfeTipItemInfo info)
{
	assert( info != NULL );

	/* Mark the tip string info as not installed */
	info->tip_string_installed = False;

	/* Reset all the tip string members */
	info->tip_string_enabled			= False;
	info->tip_string_obtain_callback	= NULL;
	info->tip_string_obtain_data1		= NULL;
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
WidgetAddDocString(Widget w)
{
	_XfeTipItemInfo		info = NULL;

	assert( w != NULL );
	assert( _XfeIsAlive(w) );
	assert( XtIsWidget(w) );

	/* Find or add the info */
	info = ItemFindOrAddInfo(w);

	assert( info != NULL );

	assert( info->doc_string_installed == False );

	/* Mark the doc string info as installed */
	info->doc_string_installed = True;

	/* If the tip string info was already installed, we are done */
	if (info->tip_string_installed)
	{
		return info;
	}

	/* Add the stage one event handler */
	WidgetAddStageOneEH(w);

	return info;
}
/*----------------------------------------------------------------------*/
static void
ItemUninstallDocString(Widget w,_XfeTipItemInfo info)
{
	assert( info != NULL );

	/* Mark the doc string info as not installed */
	info->doc_string_installed = False;

	/* Reset all the doc string members */
	info->doc_string_enabled			= False;
	info->doc_string_obtain_callback	= NULL;
	info->doc_string_obtain_data1		= NULL;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Gadget functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
GadgetAddTipString(Widget w)
{
	_XfeTipItemInfo		info = NULL;

	assert( _XfeIsAlive(w) );
	assert( XmIsGadget(w) );

	/* Find or add the info */
	info = ItemFindOrAddInfo(w);

	assert( info != NULL );

	assert( info->tip_string_installed == False );

	/* Mark the tip string info as installed */
	info->tip_string_installed = True;

	/* If the doc string was already installed, we are done */
	if (info->doc_string_installed)
	{
		return info;
	}

	GadgetSaveRealInputDispatchMethod(w);
	
	GadgetHackInputDispatch(w);

	return info;
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
GadgetAddDocString(Widget w)
{
	_XfeTipItemInfo		info = NULL;

	assert( _XfeIsAlive(w) );
	assert( XmIsGadget(w) );

	/* Find or add the info */
	info = ItemFindOrAddInfo(w);

	assert( info != NULL );

	assert( info->doc_string_installed == False );

	/* Mark the doc string info as installed */
	info->doc_string_installed = True;

	/* If the tip string was already installed, we are done */
	if (info->tip_string_installed)
	{
		return info;
	}

	GadgetSaveRealInputDispatchMethod(w);
	
	GadgetHackInputDispatch(w);

	return info;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Item (Widget or Gadget)  functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
ItemAllocateInfo(Widget w)
{
	_XfeTipItemInfo	info = NULL;

	assert( w != NULL );
	assert( _XfeIsAlive(w) );

	info = (_XfeTipItemInfo) XtMalloc(sizeof(_XfeTipItemInfoRec) * 1);

	/* Initialize the members */
	info->item							= w;

	info->tip_string_installed			= False;
	info->doc_string_installed			= False;

	/* TipString */
	info->tip_string_enabled			= False;
	info->tip_string_obtain_callback	= NULL;
	info->tip_string_obtain_data1		= NULL;

	/* DocString */
	info->doc_string_enabled			= False;
	info->doc_string_obtain_callback	= NULL;
	info->doc_string_obtain_data1		= NULL;

	/* DocString callback */
	info->doc_string_callback			= NULL;
	info->doc_string_data1				= NULL;

	return info;
}
/*----------------------------------------------------------------------*/
static void
ItemFreeInfo(_XfeTipItemInfo info)
{
	assert( info != NULL );

	XtFree((char *) info);
}
/*----------------------------------------------------------------------*/
static void
ClientDataFreeFunc(Widget w,XtPointer client_data)
{
	_XfeTipItemInfo info = (_XfeTipItemInfo) client_data;

	/* Free the info */
	ItemFreeInfo(info);
}
/*----------------------------------------------------------------------*/
static void
ItemGetTipString(Widget w,XmString * xmstr_out,Boolean * need_to_free_out)
{
	_XfeTipItemInfo	info;

	assert( xmstr_out != NULL );
	assert( need_to_free_out != NULL );

	*xmstr_out = NULL;
	*need_to_free_out = False;

	assert( _XfeIsAlive(w) );
	
	info = ItemGetInfo(w);
	
	assert( info != NULL );

	/* Invoke the tip string obtain callback if present */
	if (info->tip_string_obtain_callback != NULL)
	{
		(*info->tip_string_obtain_callback)(w,
											info->tip_string_obtain_data1,
											xmstr_out,
											need_to_free_out);
	}
}
/*----------------------------------------------------------------------*/
static void
ItemGetDocString(Widget w,XmString * xmstr_out,Boolean * need_to_free_out)
{
	_XfeTipItemInfo	info;

	assert( xmstr_out != NULL );
	assert( need_to_free_out != NULL );

	*xmstr_out = NULL;
	*need_to_free_out = False;

	assert( _XfeIsAlive(w) );
	
	info = ItemGetInfo(w);
	
	assert( info != NULL );

	/* Invoke the doc string obtain callback if present */
	if (info->doc_string_obtain_callback != NULL)
	{
		(*info->doc_string_obtain_callback)(w,
											info->doc_string_obtain_data1,
											xmstr_out,
											need_to_free_out);
	}
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
ItemGetInfo(Widget w)
{
 	assert( w != NULL );

	return (_XfeTipItemInfo) XfeClientDataGet(w,GetClientDataKey());
}
/*----------------------------------------------------------------------*/
static void
ItemAddInfo(Widget w,_XfeTipItemInfo info)
{
	assert( _XfeIsAlive(w) );
	assert( info != NULL );

	XfeClientDataAdd(w,
					 GetClientDataKey(),
					 (XtPointer) info,
					 ClientDataFreeFunc);
}
/*----------------------------------------------------------------------*/
static void
ItemRemoveInfo(Widget w,_XfeTipItemInfo info)
{
	assert( w != NULL );

	XfeClientDataRemove(w,GetClientDataKey());
}
/*----------------------------------------------------------------------*/
static _XfeTipItemInfo
ItemFindOrAddInfo(Widget w)
{
	_XfeTipItemInfo		info = NULL;

	assert( _XfeIsAlive(w) );

	/* Try to find the info */
	info = ItemGetInfo(w);
	
	/* If the info is not found, allocate it and add it to the item */
	if (info == NULL)
	{
		info = ItemAllocateInfo(w);

		ItemAddInfo(w,info);
	}

	assert( info != NULL );

	return info;
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
TipStringEnterItem(Widget w,_XfeTipItemInfo info)
{
	assert( _XfeIsAlive(w) );

	if (_XfeToolTipIsLocked())
	{
		_xfe_tt_stage_one_target = NULL;

		/* Remove the timeout if its still active */
		StageTwoRemoveTimeout(w);

		_xfe_tt_stage_two_target = NULL;

		_XfeToolTipUnlock();

/* 		printf("TipStringEnterItem(%s): tool tips are locked - abort\n", */
/* 			   XtName(w)); */

#ifdef DEBUG_TOOL_TIPS
		printf("TipStringEnterItem(%s): tool tips are locked - abort\n",
			   XtName(w));
#endif
	}

	_XfeToolTipLock();

#ifdef DEBUG_TOOL_TIPS
    printf("TipStringEnterItem: stage_one_target = %s\n",XtName(w));
#endif

	_xfe_tt_stage_one_target = w;

	StageTwoAddTimeout(w);

#ifdef DEBUG_TOOL_TIPS
	printf("TipStringEnterItem(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/
static void
TipStringLeaveItem(Widget w,_XfeTipItemInfo info)
{
	assert( _XfeIsAlive(w) );
	
	/* Check for stage one */

	/* If a target exists, reset it so that stage two aborts */
	if (_XfeIsAlive(_xfe_tt_stage_one_target))
	{
#ifdef DEBUG_TOOL_TIPS
		printf("TipStringLeaveItem: stage_one_target = NULL\n");
#endif

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
TipStringCancelItem(Widget w,_XfeTipItemInfo info)
{
#ifdef DEBUG_TOOL_TIPS
	printf("TipStringCancelItem(%s)\n",XtName(w));
#endif

	TipStringLeaveItem(w,info);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString enter / leave / cancel functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DocStringEnterItem(Widget w,_XfeTipItemInfo info)
{
	XmString	xmstr = NULL;
	Boolean		need_to_free = False;

	assert( _XfeIsAlive(w) );
	assert( info != NULL );

	ItemGetDocString(w,&xmstr,&need_to_free);

#if DEBUG
	if (xmstr == NULL)
	{
		xmstr = XmStringCreateLocalized("No doc string found.  You Suck!");
		
		need_to_free = True;
	}
#endif
 
	/* Invoke the doc string SET callback if present */
	if (info->doc_string_callback != NULL)
	{
		(*info->doc_string_callback)(w,
									 info->doc_string_data1,
									 XfeDOC_STRING_SET,
									 xmstr);
	}
#ifdef DEBUG
	else
	{
		printf("Widget '%s' has doc string support, but no callback\n",
			   XtName(w));
	}
#endif
	
	/* Free the string if needed */
	if (need_to_free)
	{
		XmStringFree(xmstr);
	}

#ifdef DEBUG_TOOL_TIPS
	printf("DocStringEnterItem(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/
static void
DocStringLeaveItem(Widget w,_XfeTipItemInfo info)
{
	assert( _XfeIsAlive(w) );
	assert( info != NULL );

	/* Invoke the doc string CLEAR callback if present */
	if (info->doc_string_callback != NULL)
	{
		(*info->doc_string_callback)(w,
									 info->doc_string_data1,
									 XfeDOC_STRING_CLEAR,
									 NULL);
	}
	
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
/* 	Widget		label = NULL; */

	XmString	xmstr = NULL;
	Boolean		need_to_free = False;

	assert( _XfeIsAlive(w) );

	shell = _XfeToolTipGetShell(w);
	
	assert( _XfeIsAlive(shell) );

/* 	label = _XfeToolTipGetLabel(w); */

/* 	assert( _XfeIsAlive(label) ); */

	ItemGetTipString(w,&xmstr,&need_to_free);

#if DEBUG
	if (xmstr == NULL)
	{
		xmstr = XmStringCreateLocalized("No tip string found.  You Suck!");
		
		need_to_free = True;
	}
#endif
 
	if (xmstr != NULL)
	{
		XfeToolTipShellSetString(shell,xmstr);

/*  		XfeLabelSetString(label,xmstr); */

		/* Free the string if needed */
		if (need_to_free)
		{
			XmStringFree(xmstr);
		}
		
/* 		XfeBypassShellUpdateSize(shell); */

		_xfe_tt_posted = True;

		_XfeMoveWidget(shell,
					   XfeRootX(w) + 10,
					   XfeRootY(w) + _XfeHeight(w) + 10);

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

#ifdef DEBUG_TOOL_TIPS
		printf("StageTwoTimeout: stage_one_target = NULL\n");
#endif
		
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
	if (XfeTipStringGlobalGetEnabledState() ||
		XfeDocStringGlobalGetEnabledState())
	{
		_XfeTipItemInfo info = ItemGetInfo(w);

		assert( info != NULL );

		if (info != NULL)
		{
			Boolean do_tip_string = 
				XfeTipStringGlobalGetEnabledState() &&
				info->tip_string_enabled;
			
			Boolean do_doc_string = 
				XfeDocStringGlobalGetEnabledState() &&
				info->doc_string_enabled;

			/* Enter */
			if (event->type == EnterNotify)
			{
				if (do_tip_string)
				{
					TipStringEnterItem(w,info);
				}
				
				if (do_doc_string)
				{
					DocStringEnterItem(w,info);
				}
			}
			/* Leave */
			else if (event->type == LeaveNotify)
			{
				if (do_tip_string)
				{
					TipStringLeaveItem(w,info);
				}
				
				if (do_doc_string)
				{
					DocStringLeaveItem(w,info);
				}
			}
			/* Cancel */
			else
			{
				if (do_tip_string)
				{
					TipStringCancelItem(w,info);
				}
			}
		} /* info != NULL */
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
/* extern */ Widget
_XfeToolTipGetShell(Widget w)
{
	static Widget _tool_tip_shell = NULL;

	/* Create to global tool tip shell if needed */
	if (!_tool_tip_shell)
	{
		/* Find the ancestor shell */
		Widget aw = XfeAncestorFindShell(w);
        
		assert( _XfeIsAlive(aw) );

		/* Create tool tip shell with the same visual, depth & cmap as aw */
		_tool_tip_shell = XtVaCreateWidget(TOOL_TIP_SHELL_NAME,
										   xfeToolTipShellWidgetClass,
										   aw,
										   XmNvisual,		XfeVisual(aw),
										   XmNcolormap,		XfeColormap(aw),
										   XmNdepth,		XfeDepth(aw),
										   NULL);
	}

	return _tool_tip_shell;
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
/* ClientData key functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XfeClientDataKey
GetClientDataKey(void)
{
	static XfeClientDataKey _client_data_key = 0;

	if (_client_data_key == 0)
	{
		_client_data_key = XfeClientGetUniqueKey();
	}

	return _client_data_key;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* TipString public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeTipStringAdd(Widget w)
{
	_XfeTipItemInfo	info = NULL;

	assert( _XfeIsAlive(w) );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeTipStringAdd(%s)\n",XtName(w));
#endif

	if (XmIsGadget(w))
	{
		info = GadgetAddTipString(w);
	}
	else
	{
		info = WidgetAddTipString(w);
	}

	assert( info != NULL );

	/* Enable the tip string by default */
	info->tip_string_enabled = True;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeTipStringRemove(Widget w)
{
	_XfeTipItemInfo info = NULL;

	assert( w != NULL );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeTipStringRemove(%s)\n",XtName(w));
#endif

	info = ItemGetInfo(w);

	assert( info != NULL );

	ItemUninstallTipString(w,info);
	
	if (!info->doc_string_installed)
	{
		/* Remove the info */
		ItemRemoveInfo(w,info);
		
		/* Free the item info */
		ItemFreeInfo(info);
	}

	/* Remove stage one event handler if needed */
	if (XtIsWidget(w))
	{
		WidgetRemoveStageOneEH(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeTipStringSetEnabledState(Widget w,Boolean state)
{
	_XfeTipItemInfo info;
	
	assert( _XfeIsAlive(w) );

	info = ItemGetInfo(w);

	assert( info != NULL );
	
	if (info != NULL)
	{
		info->tip_string_enabled = state;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeTipStringGetEnabledState(Widget w)
{
	_XfeTipItemInfo info;
	
	assert( _XfeIsAlive(w) );

	info = ItemGetInfo(w);

	assert( info != NULL );

	if (info != NULL)
	{
		return info->tip_string_enabled;
	}

	return False;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeTipStringSetObtainCallback(Widget						w,
							  XfeTipStringObtainCallback	callback,
							  XtPointer						client_data)
{
	_XfeTipItemInfo info;
	
	assert( _XfeIsAlive(w) );

	info = ItemGetInfo(w);

	assert( info != NULL );

	if (info != NULL)
	{
		info->tip_string_obtain_callback	= callback;
		info->tip_string_obtain_data1		= client_data;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeTipStringClearObtainCallback(Widget w)
{
	XfeTipStringSetObtainCallback(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString global enabled / disable functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean _xfe_tip_string_global_enabled = True;

/* extern */ void
XfeTipStringGlobalSetEnabledState(Boolean state)
{
	_xfe_tip_string_global_enabled = state;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeTipStringGlobalGetEnabledState(void)
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
XfeTipStringIsShowing(void)
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
XfeDocStringAdd(Widget w)
{
	_XfeTipItemInfo	info = NULL;

	assert( _XfeIsAlive(w) );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeDocStringAdd(%s)\n",XtName(w));
#endif

	if (XmIsGadget(w))
	{
		info = GadgetAddDocString(w);
	}
	else
	{
		info = WidgetAddDocString(w);
	}

	assert( info != NULL );

	/* Enable the doc string by default */
	info->doc_string_enabled = True;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDocStringRemove(Widget w)
{
	_XfeTipItemInfo info = NULL;

	assert( w != NULL );

#ifdef DEBUG_TOOL_TIPS
	printf("XfeDocStringRemove(%s)\n",XtName(w));
#endif

	info = ItemGetInfo(w);

	assert( info != NULL );

	ItemUninstallDocString(w,info);
	
	if (!info->tip_string_installed)
	{
		/* Remove the info */
		ItemRemoveInfo(w,info);
		
		/* Free the item info */
		ItemFreeInfo(info);
	}

	/* Remove stage one event handler if needed */
	if (XtIsWidget(w))
	{
		WidgetRemoveStageOneEH(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDocStringSetEnabledState(Widget w,Boolean state)
{
	_XfeTipItemInfo info;
	
	assert( _XfeIsAlive(w) );

	info = ItemGetInfo(w);

	assert( info != NULL );
	
	if (info != NULL)
	{
		info->doc_string_enabled = state;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeDocStringGetEnabledState(Widget w)
{
	_XfeTipItemInfo info;
	
	assert( _XfeIsAlive(w) );

	info = ItemGetInfo(w);

	assert( info != NULL );

	if (info != NULL)
	{
		return info->doc_string_enabled;
	}

	return False;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* DocString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDocStringSetObtainCallback(Widget						w,
							  XfeTipStringObtainCallback	callback,
							  XtPointer						client_data)
{
	_XfeTipItemInfo info;
	
	assert( _XfeIsAlive(w) );

	info = ItemGetInfo(w);

	assert( info != NULL );

	if (info != NULL)
	{
		info->doc_string_obtain_callback	= callback;
		info->doc_string_obtain_data1		= client_data;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDocStringClearObtainCallback(Widget w)
{
	XfeDocStringSetObtainCallback(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDocStringSetCallback(Widget					w,
						XfeDocStringCallback	callback,
						XtPointer				client_data)
{
	_XfeTipItemInfo info;
	
	assert( _XfeIsAlive(w) );

	info = ItemGetInfo(w);

	assert( info != NULL );

	if (info != NULL)
	{
		info->doc_string_callback	= callback;
		info->doc_string_data1		= client_data;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDocStringClearCallback(Widget w)
{
	XfeDocStringSetCallback(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString global enabled / disable functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean _xfe_doc_string_global_enabled = True;

/* extern */ void
XfeDocStringGlobalSetEnabledState(Boolean state)
{
	_xfe_doc_string_global_enabled = state;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeDocStringGlobalGetEnabledState(void)
{
	return _xfe_doc_string_global_enabled;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDocStringGetFromAppDefaults()										*/
/*																		*/
/* Obtain an XmString from application defaults for the resource named	*/
/* "documentationString"												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeDocStringGetFromAppDefaults(Widget w)
{
	return XfeSubResourceGetWidgetXmStringValue(w, 
												XmNdocumentationString, 
												XmCDocumentationString);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTipStringGetFromAppDefaults()										*/
/*																		*/
/* Obtain an XmString from application defaults for the resource named	*/
/* "tipString"															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeTipStringGetFromAppDefaults(Widget w)
{
	return XfeSubResourceGetWidgetXmStringValue(w, 
												XmNtipString, 
												XmCTipString);
}
/*----------------------------------------------------------------------*/
