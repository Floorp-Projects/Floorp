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
/* Name:		<Xfe/DashBoard.c>										*/
/* Description:	XfeDashBoard widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/DashBoardP.h>
#include <Xfe/TaskBarP.h>

#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>

#include <Xm/MwmUtil.h>
#include <Xm/DialogS.h>

#define MESSAGE1 "Widget is not an XfeDashBoard."
#define MESSAGE2 "XfeDashBoard cannot have any additional children."
#define MESSAGE3 "XmNprogressBar is a read-only resource."
#define MESSAGE4 "XmNstatusBar is a read-only resource."
#define MESSAGE5 "XmNtaskBar is a read-only resource."
#define MESSAGE6 "XmNtoolBar is a read-only resource."
#define MESSAGE7 "The XmNfloatingShell must have a vali XfeTaskBar descendant."
#define MESSAGE8 "The class of XmNfloatingShell must be XmDialogShell."
#define MESSAGE9 "The XmNfloatingShell must have a single valid child."

#define SHOW_TOOL_BAR(dp)			(_XfeChildIsShown((dp)->tool_bar))
#define SHOW_STATUS_BAR(dp)			(_XfeChildIsShown((dp)->status_bar))
#define SHOW_PROGRESS_BAR(dp)		(_XfeChildIsShown((dp)->progress_bar))
#define SHOW_DOCKED_TASK_BAR(dp)	(_XfeChildIsShown((dp)->docked_task_bar))

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static void		LayoutComponents	(Widget);
static Boolean	AcceptChild			(Widget);
static Boolean	DeleteChild			(Widget);
static Boolean	InsertChild			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeDashBoard functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean		IsDocked				(Widget);
static Dimension	MaxComponentHeight		(Widget);
static void			ManageComponents		(Widget);
static void			LayoutProgressBar		(Widget);
static void			LayoutToolBar			(Widget);
static void			LayoutStatusBar			(Widget);
static void			LayoutDockedTaskBar		(Widget);

static void			UpdateUndockPixmap		(Widget);
static void			AddFloatingShell		(Widget,Widget);
static Boolean		CheckFloatingWidgets	(Widget,Widget);
static void			RemoveFloatingShell		(Widget);
static void			Undock					(Widget);
static void			Dock					(Widget);

static Boolean		ShowDockedTaskBar		(Widget);
static Boolean		DockedTaskBarEmpty		(Widget);

static Boolean		ChildIsProgressBar		(Widget);
static Boolean		ChildIsStatusBar		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* TaskBar callbacks													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	TaskBarActionCB			(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Floating shell event handlers and callbacks.							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		FloatingShellEH			(Widget,XtPointer,XEvent *,Boolean *);
static void		DestroyCB				(Widget,XtPointer,XtPointer);
static void		FloatingCloseCB			(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoard Resources                                               */
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Callback resources */     	
    { 
		XmNdockCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . dock_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNundockCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . undock_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNfloatingMapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . floating_map_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNfloatingUnmapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . floating_unmap_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },


    /* Components */
    { 
		XmNprogressBar,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . progress_bar),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNstatusBar,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . status_bar),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNdockedTaskBar,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . docked_task_bar),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNtoolBar,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . tool_bar),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNfloatingShell,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . floating_shell),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNfloatingTaskBar,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . floating_task_bar),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Boolean resources */
    { 
		XmNshowDockedTaskBar,
		XmCShowDockedTaskBar,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . show_docked_task_bar),
		XmRImmediate, 
		(XtPointer) True
    },
    { 
		XmNdocked,
		XmCDocked,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . docked),
		XmRImmediate, 
		(XtPointer) True
    },

    /* Misc resources */
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . spacing),
		XmRImmediate, 
		(XtPointer) 4
    },

    /* Pixmap resource */
    { 
		XmNundockPixmap,
		XmCUndockPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeDashBoardRec , xfe_dash_board . undock_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoard Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
   { 
       XmNspacing,
       sizeof(Dimension),
       XtOffsetOf(XfeDashBoardRec , xfe_dash_board . spacing),
       _XmFromHorizontalPixels,
       _XmToHorizontalPixels 
   },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(dashboard,DashBoard) =
{
	{
		(WidgetClass) &xfeManagerClassRec,		/* superclass       	*/
		"XfeDashBoard",							/* class_name       	*/
		sizeof(XfeDashBoardRec),				/* widget_size      	*/
		NULL,									/* class_initialize 	*/
		NULL,									/* class_part_initialize*/
		FALSE,                                  /* class_inited     	*/
		Initialize,                             /* initialize       	*/
		NULL,                                   /* initialize_hook  	*/
		XtInheritRealize,						/* realize          	*/
		NULL,									/* actions          	*/
		0,										/* num_actions			*/
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
		sizeof(XfeManagerConstraintRec),		/* constraint size     	*/
		NULL,									/* init proc           	*/
		NULL,                                   /* destroy proc        	*/
		NULL,									/* set values proc     	*/
		NULL,                                   /* extension           	*/
	},

	/* XmManager Part */
	{
		XtInheritTranslations,					/* tm_table				*/
		syn_resources,							/* syn resources       	*/
		XtNumber(syn_resources),				/* num syn_resources   	*/
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
		AcceptChild,							/* accept_child			*/
		InsertChild,							/* insert_child			*/
		DeleteChild,							/* delete_child			*/
		NULL,									/* change_managed		*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		NULL,									/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		NULL,									/* draw_components		*/
		NULL,									/* extension          	*/
	},

	/* XfeDashBoard Part */
	{
		NULL,									/* extension          	*/
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeDashBoardWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(dashboard,DashBoard);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(nw);

	if (CheckFloatingWidgets(nw,dp->floating_shell))
	{
		AddFloatingShell(nw,dp->floating_shell);
	}

	/* Update pixmaps */
	UpdateUndockPixmap(nw);
	
	dp->floating_target = NULL;

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeDashBoardWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeDashBoardPart * dp = _XfeDashBoardPart(w);

	/*RemoveFloatingShell(w);*/

	if (CheckFloatingWidgets(w,dp->floating_shell))
	{
		RemoveFloatingShell(w);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeDashBoardPart *		np = _XfeDashBoardPart(nw);
    XfeDashBoardPart *		op = _XfeDashBoardPart(ow);
	Boolean					update_docked = False;

	/* floating_shell */
    if (np->floating_shell != op->floating_shell)
    {
		if (CheckFloatingWidgets(ow,op->floating_shell))
		{
			RemoveFloatingShell(ow);
		}

		if (CheckFloatingWidgets(nw,np->floating_shell))
		{
			AddFloatingShell(nw,np->floating_shell);
		}
		else
		{
			np->docked = True;

			update_docked = True;
		}
		
		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

    /* docked */
    if (np->docked != op->docked)
    {
		update_docked = True;
    }

    /* progress_bar */
    if (np->progress_bar != op->progress_bar)
    {
		_XfeWarning(nw,MESSAGE3);
		np->progress_bar = op->progress_bar;
    }
    
    /* status_bar */
    if (np->status_bar != op->status_bar)
    {
		_XfeWarning(nw,MESSAGE4);
		np->status_bar = op->status_bar;
    }
    
    /* docked_task_bar */
    if (np->docked_task_bar != op->docked_task_bar)
    {
		_XfeWarning(nw,MESSAGE5);
		np->docked_task_bar = op->docked_task_bar;
    }
    
    /* tool_bar */
    if (np->tool_bar != op->tool_bar)
    {
		_XfeWarning(nw,MESSAGE6);
		np->tool_bar = op->tool_bar;
    }

	/* show_docked_task_bar */
	if (np->show_docked_task_bar != op->show_docked_task_bar)
	{
		_XfemConfigFlags(nw) |= XfeConfigGLE;
	}

	/* undock_pixmap */
	if (np->undock_pixmap != op->undock_pixmap)
	{
		/* Update undock pixmap */
		UpdateUndockPixmap(nw);
	}

	if (update_docked)
	{
		if (np->docked)
		{
			if (CheckFloatingWidgets(nw,np->floating_shell))
			{
				XtUnmanageChild(np->floating_target);
			}
			
			if (_XfeIsAlive(np->docked_task_bar))
			{
				XtManageChild(np->docked_task_bar);
			}
		}
		else
		{
			if (CheckFloatingWidgets(nw,np->floating_shell))
			{
				np->docked = False;
				
				XtManageChild(np->floating_target);
				
				if (_XfeIsAlive(np->docked_task_bar))
				{
					XtUnmanageChild(np->docked_task_bar);
				}
			}
		}
	}

    return _XfeManagerChainSetValues(ow,rw,nw,xfeDashBoardWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension * width,Dimension * height)
{
	XfeDashBoardPart *	dp = _XfeDashBoardPart(w);
	Cardinal			num_shown = 0;

	*width  = 
		_XfemOffsetLeft(w) + _XfemOffsetRight(w);

	if (SHOW_TOOL_BAR(dp))
	{
		*width += _XfeWidth(dp->tool_bar);

		num_shown++;
	}

	if (SHOW_PROGRESS_BAR(dp))
	{
		*width += _XfeWidth(dp->progress_bar);

		num_shown++;
	}

	if (SHOW_STATUS_BAR(dp))
	{
		*width += _XfeWidth(dp->status_bar);

		num_shown++;
	}

	if (ShowDockedTaskBar(w))
	{
		*width += _XfeWidth(dp->docked_task_bar);

		num_shown++;
	}

	if (num_shown)
	{
		*width += ((num_shown - 1) * dp->spacing);
	}
	
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w) + MaxComponentHeight(w);
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
	XfeDashBoardPart *		dp = _XfeDashBoardPart(w);

	/* Compute and store the max component height */
	dp->max_component_height = MaxComponentHeight(w);

	/* Manage the components */
	ManageComponents(w);

	if (SHOW_TOOL_BAR(dp))
	{
		LayoutToolBar(w);
	}

	if (SHOW_PROGRESS_BAR(dp))
	{
		LayoutProgressBar(w);
	}

    LayoutDockedTaskBar(w);

	if (SHOW_STATUS_BAR(dp))
	{
		LayoutStatusBar(w);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeDashBoardPart *		dp = _XfeDashBoardPart(w);
	Boolean					accept = False;
	
	/* Look for task bar */
	if (XfeIsTaskBar(child))
	{
		accept = !dp->docked_task_bar;
	}
	/* Look for button tool bar */
	else if (XfeIsToolBar(child))
	{
		accept = !dp->tool_bar;
	}
	/* Look for progress bar */
	else if (ChildIsProgressBar(child))
	{
		accept = !dp->progress_bar;
	}
	/* Look for status bar */
	else if (ChildIsStatusBar(child))
	{
		accept = !dp->status_bar;
	}

	return accept;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeDashBoardPart *		dp = _XfeDashBoardPart(w);
	Boolean					layout = False;

	/* Task bar */
	if (XfeIsTaskBar(child))
	{
		dp->docked_task_bar = child;

		/* Add undock callback to docked task bar */
		XtAddCallback(dp->docked_task_bar,XmNactionCallback,
					  TaskBarActionCB,(XtPointer) w);
		
		/* Make sure the action button does show */
		XtVaSetValues(dp->docked_task_bar,XmNshowActionButton,True,NULL);
		
		layout = True;
	}
	/* Button tool bar */
	else if (XfeIsToolBar(child))
	{
		dp->tool_bar = child;
		
		layout = True;
	}
	/* Progress bar */
	else if (ChildIsProgressBar(child))
	{
		dp->progress_bar = child;

		layout = True;	
	}
	/* Status bar */
	else if (ChildIsStatusBar(child))
	{
		dp->status_bar = child;

		layout = True;
	}

	return layout;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeDashBoardPart *		dp = _XfeDashBoardPart(w);

	/* Keep track of deleted widgets */
	if (child == dp->tool_bar)
	{
		dp->tool_bar = NULL;
	}
	else if (child == dp->progress_bar)
	{
		dp->progress_bar = NULL;
	}
	else if (child == dp->status_bar)
	{
		dp->status_bar = NULL;
	}
	else if (child == dp->docked_task_bar)
	{
		dp->docked_task_bar = NULL;
	}

	return True;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeDashBoard functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean
ChildIsProgressBar(Widget child)
{
	return (XmIsDrawingArea(child) || 
			XmIsForm(child) || 
			XmIsFrame(child) || 
			XfeIsProgressBar(child));
}
/*----------------------------------------------------------------------*/
static Boolean
ChildIsStatusBar(Widget child)
{
	return (XmIsLabel(child) || 
			XmIsLabelGadget(child) || 
			(XfeIsLabel(child) && !XfeIsButton(child)));
}
/*----------------------------------------------------------------------*/
static Boolean
ShowDockedTaskBar(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	return (dp->show_docked_task_bar && dp->docked_task_bar && IsDocked(w));
}
/*----------------------------------------------------------------------*/
static Boolean
DockedTaskBarEmpty(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	return (_XfeIsAlive(dp->docked_task_bar) &&
			(XfeChildrenGetNumManaged(dp->docked_task_bar) <= 1));
}
/*----------------------------------------------------------------------*/
static Boolean
CheckFloatingWidgets(Widget w,Widget shell)
{
	Boolean result = False;

	if (_XfeIsAlive(shell))
	{
		/* Make sure the class of the shell is XmDialogShell */
		if (XmIsDialogShell(shell))
		{
			Widget 	task_bar;
			Widget	target;
			
			/* The target widget is the first child of the floating shell */
			target = _XfemChildren(shell)[0];
			
			/* The docked taskbar is the first descendant of task bar class */
			task_bar = XfeDescendantFindByClass(shell,xfeTaskBarWidgetClass,
												XfeFIND_ALIVE,False);
			
			/* Make sure the target is valid */
			if (_XfeIsAlive(target))
			{
				result = True;
			}
			else
			{
				_XfeWarning(w,MESSAGE9);
			}
			
			/* Make sure the docked task bar is valid */
			if (_XfeIsAlive(task_bar) && XfeIsTaskBar(task_bar))
			{
				result = True;
			}
			else
			{
				_XfeWarning(w,MESSAGE7);
			}
		}
		else
		{
			_XfeWarning(w,MESSAGE8);
		}
		
	}

	return result;
}
/*----------------------------------------------------------------------*/
static void
AddFloatingShell(Widget w,Widget shell)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	/* Make sure the floating widgets are valid before adding them */
	if (!CheckFloatingWidgets(w,shell))
	{
		return;
	}

	/* Assign the global floating widget for our convenience */
	/* The target widget is the first child of the floating shell */
	/* The docked taskbar is the first descendant of task bar class */
	dp->floating_shell		= shell;
	dp->floating_target		= _XfemChildren(shell)[0];
	dp->floating_task_bar	= XfeDescendantFindByClass(shell,
													   xfeTaskBarWidgetClass,
													   XfeFIND_ALIVE,
													   False);

	/* Make sure the action button does not show */
	XtVaSetValues(dp->floating_task_bar,XmNshowActionButton,False,NULL);

	/* Handle changes in view window structure */
	XtAddEventHandler(dp->floating_shell,
					  StructureNotifyMask,
					  False,
					  FloatingShellEH,
					  (XtPointer) w);

	/* Handle floating window closing */
	XfeShellAddCloseCallback(dp->floating_shell,
							 FloatingCloseCB,
							 (XtPointer) w);

    XtAddCallback(dp->floating_shell,XmNdestroyCallback,
				  DestroyCB,(XtPointer) w);

    XtAddCallback(dp->floating_task_bar,XmNdestroyCallback,
				  DestroyCB,(XtPointer) w);

    XtAddCallback(dp->floating_target,XmNdestroyCallback,
				  DestroyCB,(XtPointer) w);

	if (IsDocked(w))
	{
		dp->docked = True;

		if (CheckFloatingWidgets(w,dp->floating_shell))
		{
			XtUnmanageChild(dp->floating_target);
		}
		
		if (_XfeIsAlive(dp->docked_task_bar))
		{
			XtManageChild(dp->docked_task_bar);
		}
	}
	else
	{
		dp->docked = False;

		if (CheckFloatingWidgets(w,dp->floating_shell))
		{
			dp->docked = False;
			
			XtManageChild(dp->floating_target);
			
			if (_XfeIsAlive(dp->docked_task_bar))
			{
				XtUnmanageChild(dp->docked_task_bar);
			}
		}
	}
}
/*----------------------------------------------------------------------*/
static void
RemoveFloatingShell(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	if (dp->floating_task_bar && dp->floating_shell)
	{
		/* Handle changes in view window structure */
		XtRemoveEventHandler(dp->floating_shell,
							 StructureNotifyMask,
							 False,
							 FloatingShellEH,
							 (XtPointer) w);

		/* Dont need to handle floating window closing any more */
		XfeShellRemoveCloseCallback(dp->floating_shell,
									FloatingCloseCB,
									(XtPointer) w);
	}

	dp->floating_task_bar = NULL;
	dp->floating_target = NULL;
	dp->floating_shell = NULL;
}
/*----------------------------------------------------------------------*/
static void
ManageComponents(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	_XfemIgnoreConfigure(w) = True;

	if (IsDocked(w))
	{
		if (dp->docked_task_bar)
		{
			if (dp->show_docked_task_bar)
			{
				XtManageChild(dp->docked_task_bar);
			}
			else
			{
				XtUnmanageChild(dp->docked_task_bar);
			}
		}
	}
	else
	{
		if (_XfeIsAlive(dp->floating_target))
		{
			XtManageChild(dp->floating_target);
		}

		if (dp->docked_task_bar)
		{
			XtUnmanageChild(dp->docked_task_bar);
		}
	}
	
	_XfemIgnoreConfigure(w) = False;
}
/*----------------------------------------------------------------------*/
static Dimension
MaxComponentHeight(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);
	Dimension			max_height = 0;

	if (SHOW_TOOL_BAR(dp))
	{
		max_height = _XfeHeight(dp->tool_bar);
	}

	if (SHOW_PROGRESS_BAR(dp))
	{
		max_height = XfeMax(max_height,_XfeHeight(dp->progress_bar));
	}

	if (SHOW_STATUS_BAR(dp))
	{
		max_height = XfeMax(max_height,_XfeHeight(dp->status_bar));
	}

	if (ShowDockedTaskBar(w))
	{
		max_height = XfeMax(max_height,_XfeHeight(dp->docked_task_bar));
	}
    
    return max_height;
}
/*----------------------------------------------------------------------*/
static void
LayoutToolBar(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	assert( SHOW_TOOL_BAR(dp) );

    /* Layout the tool bar all the way to the left */
	_XfeConfigureWidget(dp->tool_bar,
						
						_XfemOffsetLeft(w),
						
						_XfemOffsetTop(w),
						
						_XfeWidth(dp->tool_bar),
						
						dp->max_component_height);
}
/*----------------------------------------------------------------------*/
static void
LayoutProgressBar(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);
	Position			x;

	assert( SHOW_PROGRESS_BAR(dp) );

	if (SHOW_TOOL_BAR(dp))
	{
		x = _XfeX(dp->tool_bar) + 
			_XfeWidth(dp->tool_bar) +
			dp->spacing;
	}
	else
	{
		x = _XfemOffsetLeft(w);
	}
	
	if (_XfeWidth(dp->progress_bar) > 0)
	{
		_XfeConfigureWidget(dp->progress_bar,
							
							x,
							
							_XfemOffsetTop(w),
							
							_XfeWidth(dp->progress_bar),
							
							dp->max_component_height);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutDockedTaskBar(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	if (!_XfeIsAlive(dp->docked_task_bar))
	{
		return;
	}

	/* If the taskbar is empty, hide it */
	if (DockedTaskBarEmpty(w))
	{
		XtVaSetValues(dp->docked_task_bar,XmNmappedWhenManaged,False,NULL);
	}	
	else
	{
		XtVaSetValues(dp->docked_task_bar,XmNmappedWhenManaged,True,NULL);
	}

	/* If the taskbar is not mapped when managed, dont do layout for it */
	if (!_XfeMappedWhenManaged(dp->docked_task_bar))
	{
		return;
	}
	
    /* Layout the task bar and status bar according to docking state */
    if (ShowDockedTaskBar(w))
    {
		/* Layout the docked task bar to the far right */
		_XfeConfigureWidget(dp->docked_task_bar,
							
							_XfeWidth(w) - 
							_XfemOffsetRight(w) - 
							_XfeWidth(dp->docked_task_bar),

							_XfemOffsetTop(w),

							_XfeWidth(dp->docked_task_bar),
							
							dp->max_component_height);

		/*
		 * Raise the taskbar so that it will always show even when the
		 * dashboard display is tiny 
		 */
		if (XtIsRealized(dp->docked_task_bar))
		{
			XRaiseWindow(XtDisplay(dp->docked_task_bar),
						 _XfeWindow(dp->docked_task_bar));
		}
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutStatusBar(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);
	Position			x1;
	Position			x2;

	assert( SHOW_STATUS_BAR(dp) );

	/* Compute the right-most coordinate */
	if (ShowDockedTaskBar(w) && !DockedTaskBarEmpty(w))
		
	{
		x2 = _XfeX(dp->docked_task_bar) - dp->spacing;
	}
	else
	{
		x2 = _XfeWidth(w) - _XfemOffsetRight(w);
	}
	
	if (SHOW_PROGRESS_BAR(dp))
	{
		x1 = 
			_XfeX(dp->progress_bar) + 
			_XfeWidth(dp->progress_bar) + 
			dp->spacing;
	}
	else if (SHOW_TOOL_BAR(dp))
	{
		x1 = 
			_XfeX(dp->tool_bar) + 
			_XfeWidth(dp->tool_bar) + 
			dp->spacing;
	}
	else
	{
		x1 = _XfemOffsetLeft(w);
	}
	
	
	if (x2 > x1)
	{
		/* Use the full extent of the widget */
		_XfeConfigureWidget(dp->status_bar,
							
							x1,
							
							_XfemOffsetLeft(w),
							
							x2 - x1,
							
							dp->max_component_height);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
IsDocked(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	return !(_XfeIsAlive(dp->floating_target) && 
			 XtIsManaged(dp->floating_target));

	/* return !(dp->floating_task_bar && XtIsManaged(dp->floating_task_bar)); */
}
/*----------------------------------------------------------------------*/
static void
UpdateUndockPixmap(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	if (dp->docked_task_bar)
	{
		if (_XfePixmapGood(dp->undock_pixmap))
		{
			XtVaSetValues(dp->docked_task_bar,
						  XmNactionPixmap,			dp->undock_pixmap,
						  NULL);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
Undock(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);
	Boolean				invoke_callback = dp->docked;

	/* Undock only if a floating shell has been installed */
	if (CheckFloatingWidgets(w,dp->floating_shell))
	{
		dp->docked = False;
		
		/* XtManageChild(dp->floating_task_bar); */
		if (CheckFloatingWidgets(w,dp->floating_shell))
		{
			XtManageChild(dp->floating_target);
		}

		if (_XfeIsAlive(dp->docked_task_bar))
		{
			XtUnmanageChild(dp->docked_task_bar);
		}
	}

	if (invoke_callback)
	{
/* 		printf("Undock(%s,%p)\n",XtName(XtParent(XtParent(w))),XtParent(XtParent(w))); */

		_XfeInvokeCallbacks(w,dp->undock_callback,XmCR_UNDOCK,NULL,False);

	}
}
/*----------------------------------------------------------------------*/
static void
Dock(Widget w)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);
	Boolean				invoke_callback = !dp->docked;

	dp->docked = True;

	if (CheckFloatingWidgets(w,dp->floating_shell))
	{
		/* XtUnmanageChild(dp->floating_task_bar); */
		XtUnmanageChild(dp->floating_target);
	}

	if (_XfeIsAlive(dp->docked_task_bar))
	{
		XtManageChild(dp->docked_task_bar);
	}

	if (invoke_callback)
	{
/*  		printf("Dock(%s,%p)\n",XtName(XtParent(XtParent(w))),XtParent(XtParent(w))); */

		_XfeInvokeCallbacks(w,dp->dock_callback,XmCR_DOCK,NULL,False);
	}
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Floating shell / task bar event handlers and callbacks.				*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
FloatingShellEH(Widget		shell,
				XtPointer	client_data,
				XEvent *	event,
				Boolean *	cont)
{
    Widget				w = (Widget) client_data;
    XfeDashBoardPart *	dp = _XfeDashBoardPart(w);

	if (event)
	{
		switch(event->type)
		{
		case MapNotify:
			
			Undock(w);

			_XfeInvokeCallbacks(w,dp->floating_map_callback,
								XmCR_FLOATING_MAP,event,False);
			
			break;

		case UnmapNotify:
			
			Dock(w);

			_XfeInvokeCallbacks(w,dp->floating_unmap_callback,
								XmCR_FLOATING_UNMAP,event,False);

			break;
		}
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/
static void
DestroyCB(Widget child,XtPointer client_data,XtPointer call_data)
{
    Widget w = (Widget) client_data;
	
	RemoveFloatingShell(w);
}
/*----------------------------------------------------------------------*/
static void
FloatingCloseCB(Widget child,XtPointer client_data,XtPointer call_data)
{
	Widget w = (Widget) client_data;

	Dock(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TaskBar callbacks													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
TaskBarActionCB(Widget button,XtPointer client_data,XtPointer call_data)
{
    Widget w = (Widget) client_data;

	Undock(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoard Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateDashBoard(Widget parent,char *name,Arg *args,Cardinal count)
{
   return (XtCreateWidget(name,xfeDashBoardWidgetClass,parent,args,count));
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeDashBoardGetComponent(Widget dashboard,XfeDashBoardComponent component)
{
    XfeDashBoardPart *	dp = _XfeDashBoardPart(dashboard);
	Widget				cw = NULL;

	switch(component)
	{
	case XfeDASH_BOARD_TOOL_BAR:
		cw = dp->tool_bar;
		break;

	case XfeDASH_BOARD_DOCKED_TASK_BAR:
		cw = dp->docked_task_bar;
		break;

	case XfeDASH_BOARD_FLOATING_TASK_BAR:
		cw = dp->floating_task_bar;
		break;

	case XfeDASH_BOARD_FLOATING_SHELL:
		cw = dp->floating_shell;
		break;

	case XfeDASH_BOARD_PROGRESS_BAR:
		cw = dp->progress_bar;
		break;

	case XfeDASH_BOARD_STATUS_BAR:
		cw = dp->status_bar;
		break;
	}

	return cw;
}
/*----------------------------------------------------------------------*/
