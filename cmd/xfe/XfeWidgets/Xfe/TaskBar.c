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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/TaskBar.c>											*/
/* Description:	XfeTaskBar widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/TaskBarP.h>

#include <Xfe/Button.h>
#include <Xfe/Tab.h>

#include <Xm/AtomMgr.h>
#include <Xm/Protocols.h>

#define MESSAGE1 "Widget is not an XfeTaskBar."
#define MESSAGE2 "XmNactionButton is a read-only resource."
#define MESSAGE3 "XmNfirstWidget is a read-only resource."
#define MESSAGE4 "XmNrightWidget is a read-only resource."
#define MESSAGE5 "XmNmaxPosition is too big."
#define MESSAGE6 "XmNminPosition is too small."

#define ACTION_BUTTON_NAME			"ActionButton"

#define SHADOW_OFFSET 3

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class Methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,
								 Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static void		LayoutChildren		(Widget);
static void		LayoutComponents	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeTaskBar functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutComponentsVertical	(Widget);
static void		LayoutComponentsHorizontal	(Widget);
static void		UpdateActionPixmap			(Widget);
static void		UpdateActionCursor			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Action Button callbacks and event handlers							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	ActionCallback			(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBar Resources                                                 */
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Callback resources */     	
    { 
		XmNactionCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeTaskBarRec , xfe_task_bar . action_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },

    /* Resources */
    { 
		XmNactionButton,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeTaskBarRec , xfe_task_bar . action_button),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNactionCursor,
		XmCCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeTaskBarRec , xfe_task_bar . action_cursor),
		XmRImmediate, 
		(XtPointer) None
    },
    { 
		XmNactionPixmap,
		XmCActionPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTaskBarRec , xfe_task_bar . action_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNshowActionButton,
		XmCShowActionButton,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeTaskBarRec , xfe_task_bar . show_action_button),
		XmRImmediate, 
		(XtPointer) False
    },

    /* Force all the margins to 0 */
    { 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTaskBarRec , xfe_manager . margin_bottom),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTaskBarRec , xfe_manager . margin_left),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTaskBarRec , xfe_manager . margin_right),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTaskBarRec , xfe_manager . margin_top),
		XmRImmediate, 
		(XtPointer) 0
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(taskbar,TaskBar) =
{
    {
		(WidgetClass) &xfeToolBarClassRec,		/* superclass       	*/
		"XfeTaskBar",							/* class_name       	*/
		sizeof(XfeTaskBarRec),					/* widget_size      	*/
		NULL,									/* class_initialize 	*/
		NULL,									/* class_part_initiali	*/
		FALSE,                                  /* class_inited     	*/
		Initialize,                             /* initialize       	*/
		NULL,                                   /* initialize_hook  	*/
		XtInheritRealize,						/* realize          	*/
		NULL,									/* actions          	*/
		0,										/* num_actions      	*/
		resources,                              /* resources        	*/
		XtNumber(resources),                    /* num_resources    	*/
		NULLQUARK,                              /* xrm_class        	*/
		TRUE,                                   /* compress_motion  	*/
		XtExposeCompressMaximal,                /* compress_exposure	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest 	*/
		Destroy,								/* destroy          	*/
		XtInheritResize,                        /* resize           	*/
		XtInheritExpose,						/* expose           	*/
		SetValues,                              /* set_values       	*/
		NULL,                                   /* set_values_hook  	*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		NULL,									/* get_values_hook  	*/
		NULL,                                   /* accexfe_focus     	*/
		XtVersion,                              /* version          	*/
		NULL,                                   /* callback_private 	*/
		XtInheritTranslations,					/* tm_table				*/
		XtInheritQueryGeometry,					/* query_geometry   	*/
		XtInheritDisplayAccelerator,            /* display accel    	*/
		NULL,                                   /* extension        	*/
    },
    
    /* Composite Part */
    {
		XtInheritGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },

    /* Constraint Part */
    {
		NULL,									/* resource list       	*/
		0,										/* num resources       	*/
		sizeof(XfeToolBarConstraintRec),		/* constraint size		*/
		NULL,									/* init proc           	*/
		NULL,                                   /* destroy proc        	*/
		NULL,									/* set values proc     	*/
		NULL,                                   /* extension           	*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,
		NULL,									/* syn resources       	*/
		0,										/* num syn_resources   	*/
		NULL,                                   /* syn_cont_resources  	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process      	*/
		NULL,                                   /* extension           	*/
	},

    /* XfeManager Part 	*/
    {
		XfeInheritBitGravity,					/* bit_gravity			*/
		PreferredGeometry,						/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		XfeInheritAcceptChild,					/* accept_child			*/
		XfeInheritInsertChild,					/* insert_child			*/
		XfeInheritDeleteChild,					/* delete_child			*/
		XfeInheritChangeManaged,				/* change_managed		*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		LayoutChildren,							/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		XfeInheritDrawComponents,				/* draw_components		*/
		False,									/* count_layable_children*/
		NULL,									/* child_is_layable		*/
		NULL,									/* extension          	*/
    },

	/* XfeOriented Part */
	{
		NULL,									/* enter				*/
		NULL,									/* leave				*/
		NULL,									/* motion				*/
		NULL,									/* drag_start			*/
		NULL,									/* drag_end				*/
		NULL,									/* drag_motion			*/
		NULL,									/* des_enter			*/
		NULL,									/* des_leave			*/
		NULL,									/* des_motion			*/
		NULL,									/* des_drag_start		*/
		NULL,									/* des_drag_end			*/
		NULL,									/* des_drag_motion		*/
		NULL,									/* extension          	*/
	},


    /* XfeToolBar Part */
    {
		XfeInheritDrawRaiseBorder,				/* draw_raise_border	*/
		XfeInheritLayoutIndicator,				/* layout_indicator		*/
		NULL,									/* extension          	*/
    },

    /* XfeTaskBar Part */
    {
		NULL,									/* extension          	*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeTaskBarWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(taskbar,TaskBar);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class Methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeTaskBarPart *	tp = _XfeTaskBarPart(nw);

    /* Create the action button */
    tp->action_button = 
		XtVaCreateWidget(ACTION_BUTTON_NAME,
						 xfeTabWidgetClass,
						 nw,
						 XmNmarginLeft,			0,
						 XmNmarginRight,		0,
						 XmNmarginTop,			0,
						 XmNmarginBottom,		0,
						 XmNprivateComponent,	True,
						 XmNtraversalOn,		False,
						 XmNhighlightThickness,	0,
						 XmNarmOffset,			0,
						 XmNraiseOffset,		0,
						 XmNbackground,			_XfeBackgroundPixel(nw),
						 NULL);
	
	/* Update the pixmaps */
	UpdateActionPixmap(nw);

	/* Add callback to action button */
    XtAddCallback(tp->action_button,
				  XmNactivateCallback,
				  ActionCallback,
				  (XtPointer) nw);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeTaskBarWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfeTaskBarPart * tp = _XfeTaskBarPart(w); */

/*     XtRemoveCallback(tp->action_button, */
/* 					 XmNactivateCallback, */
/* 					 ActionCallback, */
/* 					 (XtPointer) nw); */

/* 	XtRemoveEventHandler(nw, */
/* 						 StructureNotifyMask, */
/* 						 True, */
/* 						 MappingEH, */
/* 						 (XtPointer) nw); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeTaskBarPart *		np = _XfeTaskBarPart(nw);
    XfeTaskBarPart *		op = _XfeTaskBarPart(ow);

    /* action_button */
    if (np->action_button != op->action_button)
    {
		np->action_button = op->action_button;
    }

    /* action_pixmap */
    if (np->action_pixmap != op->action_pixmap)
    {
		UpdateActionPixmap(nw);

		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

    /* action_cursor */
    if (np->action_cursor != op->action_cursor)
    {
		UpdateActionCursor(nw);
    }

    /* show_action_button */
    if (np->show_action_button != op->show_action_button)
    {
		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

    return _XfeManagerChainSetValues(ow,rw,nw,xfeTaskBarWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension * width,Dimension * height)
{
    XfeTaskBarPart *		tp = _XfeTaskBarPart(w);
    XfeToolBarWidgetClass	tbc = (XfeToolBarWidgetClass)xfeToolBarWidgetClass;

    (*tbc->xfe_manager_class.preferred_geometry)(w,width,height);

	if (tp->show_action_button)
	{
		switch (_XfeOrientedOrientation(w))
		{
		case XmHORIZONTAL:
			
			*width += _XfeWidth(tp->action_button);
			
			break;
			
		case XmVERTICAL:
			
			*height += _XfeHeight(tp->action_button);
			
			break;
		}
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutChildren(Widget w)
{
    XfeTaskBarPart *		tp = _XfeTaskBarPart(w);
    XfeToolBarWidgetClass	tbc = (XfeToolBarWidgetClass)xfeToolBarWidgetClass;
	Dimension				action_width;
	Dimension				action_height;

	if (tp->show_action_button && _XfeIsAlive(tp->action_button))
	{
		action_width  = _XfeWidth(tp->action_button);
		action_height = _XfeHeight(tp->action_button);
	}
	else
	{
		action_width  = 0;
		action_height = 0;
	}

	/* Add the action button's dimensions if needed */
    switch (_XfeOrientedOrientation(w))
	{
	case XmHORIZONTAL:

		_XfemMarginLeft(w) += action_width;
		
		(*tbc->xfe_manager_class.layout_children)(w);
		
		_XfemMarginLeft(w) -= action_width;

		break;
		
	case XmVERTICAL:

		_XfemMarginTop(w) += action_height;
		
		(*tbc->xfe_manager_class.layout_children)(w);
		
		_XfemMarginTop(w) -= action_height;

		break;
    }
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
    XfeTaskBarPart *	tp = _XfeTaskBarPart(w);

	/* Invoke layout_indicator method */
    _XfeToolBarLayoutIndicator(w);

	/* Make sure our one and only component alive and kicking */
	if (!_XfeIsAlive(tp->action_button))
	{
		return;
	}

	/* Do our layout */
    switch(_XfeOrientedOrientation(w))
	{
	case XmHORIZONTAL:
		LayoutComponentsHorizontal(w);
		break;

	case XmVERTICAL:
		LayoutComponentsVertical(w);
		break;
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Action Button callbacks												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ActionCallback(Widget child,XtPointer client_data,XtPointer call_data)
{
	Widget						w = XtParent(child);
    XfeTaskBarPart *			tp = _XfeTaskBarPart(w);
    XfeButtonCallbackStruct *	cbs = (XfeButtonCallbackStruct *) call_data;

	if (_XfeIsAlive(w))
	{
		/* Invoke the action callbacks */
		_XfeInvokeCallbacks(w,tp->action_callback,XmCR_ACTION,
							cbs->event,False);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeTaskBar functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutComponentsVertical(Widget w)
{
    XfeTaskBarPart *	tp = _XfeTaskBarPart(w);

	XtVaSetValues(tp->action_button,
				  XmNusePreferredWidth,		True,
				  NULL);
	
	/* Layout the action button */
	_XfeConfigureWidget(tp->action_button,
						   
						0,
						
						0,
						
						_XfeWidth(w),
						
						_XfeHeight(tp->action_button));

	/* Show the action button as needed */
	_XfemIgnoreConfigure(w) = True;

	if (tp->show_action_button)
	{
		XtManageChild(tp->action_button);
	}
	else
	{
		XtUnmanageChild(tp->action_button);
	}

	_XfemIgnoreConfigure(w) = False;
}
/*----------------------------------------------------------------------*/
static void
LayoutComponentsHorizontal(Widget w)
{
    XfeTaskBarPart *	tp = _XfeTaskBarPart(w);

	XtVaSetValues(tp->action_button,
				  XmNusePreferredWidth,		True,
				  NULL);
	
	/* Layout the action button */
	_XfeConfigureWidget(tp->action_button,
						   
						0,
						
						0,

						_XfeWidth(tp->action_button),

						_XfeHeight(w));
						
	/* Show the action button as needed */
	_XfemIgnoreConfigure(w) = True;

	if (tp->show_action_button)
	{
		XtManageChild(tp->action_button);
	}
	else
	{
		XtUnmanageChild(tp->action_button);
	}

	_XfemIgnoreConfigure(w) = False;
}
/*----------------------------------------------------------------------*/
static void
UpdateActionPixmap(Widget w)
{
    XfeTaskBarPart *	tp = _XfeTaskBarPart(w);

	if (_XfePixmapGood(tp->action_pixmap))
	{
		XtVaSetValues(tp->action_button,
					  XmNpixmap,				tp->action_pixmap,
					  NULL);
	}
	else
	{
		_XfeResizeWidget(tp->action_button,16,16);
	}
}
/*----------------------------------------------------------------------*/
static void
UpdateActionCursor(Widget w)
{
    XfeTaskBarPart *	tp = _XfeTaskBarPart(w);

	if (tp->action_pixmap != None)
	{
		XtVaSetValues(tp->action_button,
					  XmNcursor,				tp->action_cursor,
					  XmNcursorOn,				True,
					  NULL);
	}
	else
	{
		XtVaSetValues(tp->action_button,
					  XmNcursor,				None,
					  XmNcursorOn,				False,
					  NULL);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBar Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateTaskBar(Widget parent,char *name,Arg *args,Cardinal count)
{
	return (XtCreateWidget(name,xfeTaskBarWidgetClass,parent,args,count));
}
/*----------------------------------------------------------------------*/
