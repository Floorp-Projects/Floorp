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
/* Name:		<Xfe/TempShell.c>										*/
/* Description:	XfeTempShell widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/FrameShellP.h>

#include <Xm/AtomMgr.h>
#include <Xm/Protocols.h>

#define BYPASS_SHELL_NAME "SharedBypassShell"

#define EDITRES 1

#ifdef EDITRES
#include <X11/Xmu/Editres.h>
#endif

#define MESSAGE1 "Widget is not an XfeFrameShell."
#define MESSAGE2 "XmNhasBeenMapped is a read-only resource."

#define STRUCTURE_EVENTS	StructureNotifyMask

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassInitialize	();
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void		Resize			(Widget);
static void		Realize			(Widget,XtValueMask *,XSetWindowAttributes *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);
static void		GetValuesHook	(Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Tracking update functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		SetTrackEditres			(Widget,Boolean);
static void		SetTrackMapping			(Widget,Boolean);
static void		SetTrackSaveYourself	(Widget,Boolean);
static void		SetTrackDeleteWindow	(Widget,Boolean);

/*----------------------------------------------------------------------*/
/*																		*/
/* Window manager protocol callbacks									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DeleteWindowCB		(Widget,XtPointer,XtPointer);
static void		SaveYourselfCB		(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Shell structure event handler										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		StructureEH			(Widget,XtPointer,XEvent *,Boolean *);
static void		MappingEH			(Widget,XtPointer,XEvent *,Boolean *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShell resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
	/* Realization callback resources */
	{ 
		XmNrealizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.realize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNbeforeRealizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.before_realize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* Configure callback resources */
	{ 
		XmNresizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.resize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNbeforeResizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.before_resize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNmoveCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.move_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* WM Protocol callback resources */
	{ 
		XmNdeleteWindowCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.delete_window_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNsaveYourselfCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.save_yourself_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* Mapping callbacks */
	{ 
		XmNfirstMapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.first_map_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNmapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.map_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNunmapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.unmap_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* Title changed callback */
	{ 
		XmNtitleChangedCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeFrameShellRec,xfe_frame_shell.title_changed_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* Tracking resources */
	{ 
		XmNtrackEditres,
		XmCTrackEditres,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . track_editres),
		XmRImmediate, 
		(XtPointer) False
	},
	{ 
		XmNtrackMapping,
		XmCTrackMapping,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . track_mapping),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNtrackPosition,
		XmCTrackPosition,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . track_position),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNtrackSaveYourself,
		XmCTrackSaveYourself,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . track_save_yourself),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNtrackSize,
		XmCTrackSize,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . track_size),
		XmRImmediate, 
		(XtPointer) True
	},


	/* Other resources */
	{ 
		XmNhasBeenMapped,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . has_been_mapped),
		XmRImmediate, 
		(XtPointer) False
	},
	{ 
		XmNstartIconic,
		XmCStartIconic,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . start_iconic),
		XmRImmediate, 
		(XtPointer) True
	},

	{ 
		XmNbypassShell,
		XmCBypassShell,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeFrameShellRec , xfe_frame_shell . bypass_shell),
		XmRImmediate, 
		(XtPointer) NULL
	},
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(frameshell,FrameShell) =
{
    {
		(WidgetClass) &topLevelShellClassRec,	/* superclass			*/
		"XfeFrameShell",						/* class_name			*/
		sizeof(XfeFrameShellRec),				/* widget_size			*/
		ClassInitialize,						/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		Realize,								/* realize				*/
		NULL,									/* actions				*/
		0,										/* num_actions			*/
		resources,                              /* resources			*/
		XtNumber(resources),                    /* num_resources		*/
		NULLQUARK,								/* xrm_class			*/
		FALSE,									/* compress_motion		*/
		TRUE,									/* compress_exposure	*/
		FALSE,									/* compress_enterleave	*/
		FALSE,									/* visible_interest		*/
		Destroy,								/* destroy				*/
		Resize,									/* resize				*/
		XtInheritExpose,						/* expose				*/
		SetValues,                              /* set_values			*/
		NULL,                                   /* set_values_hook		*/
		NULL,									/* set_values_almost	*/
		GetValuesHook,							/* get_values_hook		*/
		NULL,                                   /* access_focus			*/
		XtVersion,                              /* version				*/
		NULL,                                   /* callback_private		*/
		XtInheritTranslations,					/* tm_table				*/
		NULL,									/* query_geometry		*/
		NULL,									/* display accelerator	*/
		NULL,									/* extension			*/
    },
    
    /* Composite Part */
    {
		XtInheritGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },

    /* Shell */
    {
		NULL,									/* extension			*/
    },

    /* WMShell */
    {
		NULL,									/* extension			*/
    },

    /* VendorShell */
    {
		NULL,									/* extension			*/
    },

    /* TopShell */
    {
		NULL,									/* extension			*/
    },

    /* XfeFrameShell Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeFrameShellWidgetClass declaration.								*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(frameshell,FrameShell);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassInitialize()
{
	/* Register Xfe Converters */
    /*XfeRegisterConverters();*/

    /* Register Representation Types */
    XfeRegisterRepresentationTypes();
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeFrameShellPart *		fp = _XfeFrameShellPart(nw);

	/* Check read only resources */
	if (fp->has_been_mapped)
	{
		fp->has_been_mapped = False;

		_XfeWarning(nw,MESSAGE2);
	}

	/* Initialize private members */

	/* Update tracking */
/* 	SetTrackEditres(nw,fp->track_editres); */
	SetTrackMapping(nw,fp->track_mapping);
	SetTrackSaveYourself(nw,fp->track_save_yourself);
	SetTrackDeleteWindow(nw,fp->track_delete_window);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfeFrameShellPart *		fp = _XfeFrameShellPart(w); */

#if 0
	SetTrackEditres(w,False);
	SetTrackMapping(w,False);
	SetTrackSaveYourself(w,False);
	SetTrackDeleteWindow(w,False);
#endif
}
/*----------------------------------------------------------------------*/
static void
Realize(Widget w,XtValueMask * mask,XSetWindowAttributes * wa)
{
    XfeFrameShellPart *	fp = _XfeFrameShellPart(w);

    /* Invoke before realize Callbacks */
    _XfeInvokeCallbacks(w,fp->before_realize_callback,
						XmCR_BEFORE_REALIZE,NULL,False);

    /*
     * HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY
     *
     * This is a complete HACK.  Hardcode the dimensions to 640x480
     * until I write some clever code to compute dimensions from
     * resources, command line, children preferred geometries, and
     * other magical things.
     *
     * HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY
     */
    if (_XfeWidth(w) <= 2)
    {
      _XfeWidth(w) = 600;
    }

    if (_XfeHeight(w) <= 2)
    {
      _XfeHeight(w) = 480;
    }

    /* The actual realization is handled by the superclass */
	(*topLevelShellWidgetClass->core_class.realize)(w,mask,wa);

    /* Invoke realize Callbacks */
    _XfeInvokeCallbacks(w,fp->realize_callback,XmCR_REALIZE,NULL,False);
}
/*----------------------------------------------------------------------*/
static void
Resize(Widget w)
{
    XfeFrameShellPart *	fp = _XfeFrameShellPart(w);

    /* Invoke before resize Callbacks */
    _XfeInvokeCallbacks(w,fp->before_resize_callback,
						XmCR_BEFORE_RESIZE,NULL,False);

    /* The actual realization is handled by the superclass */
    (*topLevelShellWidgetClass->core_class.resize)(w);
    
    /* Invoke resize Callbacks */
    _XfeInvokeCallbacks(w,fp->resize_callback,XmCR_RESIZE,NULL,False);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeFrameShellPart *		np = _XfeFrameShellPart(nw);
    XfeFrameShellPart *		op = _XfeFrameShellPart(ow);

    WMShellPart *			wm_np = _WMShellPart(nw);
    WMShellPart *			wm_op = _WMShellPart(ow);

	/* has_been_mapped */
	if (np->has_been_mapped != op->has_been_mapped)
	{
		np->has_been_mapped = op->has_been_mapped;

		_XfeWarning(nw,MESSAGE2);
	}

	/* track_editres */
	if (np->track_editres != op->track_editres)
	{
		SetTrackEditres(nw,np->track_editres);
	}

	/* track_mapping */
	if (np->track_mapping != op->track_mapping)
	{
		SetTrackMapping(nw,np->track_mapping);
	}

	/* track_save_yourself */
	if (np->track_save_yourself != op->track_save_yourself)
	{
		SetTrackSaveYourself(nw,np->track_save_yourself);
	}

	/* track_delete_window */
	if (np->track_delete_window != op->track_delete_window)
	{
		SetTrackDeleteWindow(nw,np->track_delete_window);
	}

	/* title */
	if (wm_np->title != wm_op->title)
	{
		/* Invoke title changed Callbacks */
		_XfeInvokeCallbacks(nw,np->title_changed_callback,
							XmCR_TITLE_CHANGED,NULL,False);
	}

    return False;
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList av,Cardinal * pac)
{
    XfeFrameShellPart *	fp = _XfeFrameShellPart(w);
    Cardinal			i;
    
    for (i = 0; i < *pac; i++)
    {
		/* bypass_shell */
		if (strcmp(av[i].name,XmNbypassShell) == 0)
		{
			*((XtArgVal *) av[i].value) = 
				(XtArgVal) XfeFrameShellGetBypassShell(w);
		}
    }
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Tracking update functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
SetTrackEditres(Widget w,Boolean state)
{
#ifdef EDITRES
	/* Remove any editres event handler that might have been added */
 	XtRemoveEventHandler(w,0,True,_XEditResCheckMessages,NULL);

	/* Add the editres event handler if needed */
	if (state)
	{
 		XtAddEventHandler(w,0,True,_XEditResCheckMessages,NULL);
	}
#endif
}
/*----------------------------------------------------------------------*/
static void
SetTrackMapping(Widget w,Boolean state)
{
	/* Remove any mapping event handler that might have been added */
	XtRemoveEventHandler(w,StructureNotifyMask,True,MappingEH,w);

	/* Add the mapping event handler if needed */
	if (state)
	{
		XtAddEventHandler(w,StructureNotifyMask,True,MappingEH,w);
	}
}
/*----------------------------------------------------------------------*/
static void
SetTrackDeleteWindow(Widget w,Boolean state)
{
	static Atom WM_DELETE_WINDOW = None;

	/* Dont do nothing if the state is False and no Atom has been created */
	if ((WM_DELETE_WINDOW == None) && !state)
	{
		return;
	}

	/* Create the Atom for the first time */
	if (WM_DELETE_WINDOW == None)
	{
		WM_DELETE_WINDOW = XmInternAtom(XtDisplay(w),"WM_DELETE_WINDOW",False);

		XmAddWMProtocols(w,&WM_DELETE_WINDOW,1);
	}
	
	/* Remove any delete callback that might have been added */
	XmRemoveWMProtocolCallback(w,WM_DELETE_WINDOW,DeleteWindowCB,w);

	/* Add the delete callback if needed */
	if (state)
	{
		XmAddWMProtocolCallback(w,WM_DELETE_WINDOW,DeleteWindowCB,w);
	}
}
/*----------------------------------------------------------------------*/
static void
SetTrackSaveYourself(Widget w,Boolean state)
{
	static Atom WM_SAVE_YOURSELF = None;

	/* Dont do nothing if the state is False and no Atom has been created */
	if ((WM_SAVE_YOURSELF == None) && !state)
	{
		return;
	}

	/* Create the Atom for the first time */
	if (WM_SAVE_YOURSELF == None)
	{
		WM_SAVE_YOURSELF = XmInternAtom(XtDisplay(w),"WM_SAVE_YOURSELF",False);

		XmAddWMProtocols(w,&WM_SAVE_YOURSELF,1);
	}
	
	/* Remove any save yourself callback that might have been added */
	XmRemoveWMProtocolCallback(w,WM_SAVE_YOURSELF,SaveYourselfCB,w);

	/* Add the save yourself callback if needed */
	if (state)
	{
		XmAddWMProtocolCallback(w,WM_SAVE_YOURSELF,SaveYourselfCB,w);
	}

}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Window manager protocol callbacks									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DeleteWindowCB(Widget shell,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
    XfeFrameShellPart *		fp = _XfeFrameShellPart(w);
/* 	XmVendorShellExtPart *	vp = _XfeVendorShellPart(w); */

	if (_XfeIsAlive(w) && fp->track_delete_window)
	{
		/* Invoke delete window callbacks */
		_XfeInvokeCallbacks(w,fp->delete_window_callback,
							XmCR_DELETE_WINDOW,NULL,False);
	}
}
/*----------------------------------------------------------------------*/
static void
SaveYourselfCB(Widget shell,XtPointer client_data,XtPointer call_data)
{
	Widget				w = (Widget) client_data;
    XfeFrameShellPart *	fp = _XfeFrameShellPart(w);

	if (_XfeIsAlive(w) && fp->track_save_yourself)
	{
		/* Invoke save yourself callbacks */
		_XfeInvokeCallbacks(w,fp->save_yourself_callback,
							XmCR_SAVE_YOURSELF,NULL,False);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Shell structure event handler										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
MappingEH(Widget		shell,
		  XtPointer		client_data,
		  XEvent *		event,
		  Boolean *		cont)
{
	Widget				w = (Widget) client_data;
    XfeFrameShellPart *	fp = _XfeFrameShellPart(w);

	/* Make sure the shell is still alive */
	if (_XfeIsAlive(w) && fp->track_mapping)
	{
		switch(event->type) 
		{
			/* Map */
		case MapNotify:

			if (XfeIsViewable(shell))
			{
				/* Invoke first map callbacks */
				if (!fp->has_been_mapped)
				{
					fp->has_been_mapped = True;
					
					_XfeInvokeCallbacks(w,fp->first_map_callback,
										XmCR_FIRST_MAP,NULL,False);
				}
				/* Invoke map callbacks */
				else
				{
					_XfeInvokeCallbacks(w,fp->map_callback,
										XmCR_MAP,NULL,False);
				}
			}
			
			break;
			
			/* Unmap */
		case UnmapNotify:
			
			/* Invoke unmap callbacks */
			_XfeInvokeCallbacks(w,fp->unmap_callback,
							XmCR_UNMAP,NULL,False);

			break;
		}
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/
static void
StructureEH(Widget			shell,
			XtPointer		client_data,
			XEvent *		event,
			Boolean *		cont)
{
	Widget				w = (Widget) client_data;
    XfeFrameShellPart *	fp = _XfeFrameShellPart(w);

	/* Make sure the shell is still alive */
	if (!_XfeIsAlive(w))
	{
		return;
	}

	switch(event->type) 
	{
	case MapNotify:

		/* Invoke first map callbacks */
		if (!fp->has_been_mapped)
		{
			fp->has_been_mapped = True;

			_XfeInvokeCallbacks(w,fp->first_map_callback,
								XmCR_FIRST_MAP,NULL,False);
			
		}
		/* Invoke map callbacks */
		else
		{
			_XfeInvokeCallbacks(w,fp->map_callback,
								XmCR_MAP,NULL,False);
		}

		break;

	case UnmapNotify:

		/* Invoke unmap callbacks */
		_XfeInvokeCallbacks(w,fp->unmap_callback,
							XmCR_UNMAP,NULL,False);

		break;
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShell Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateFrameShell(Widget pw,char * name,ArgList av,Cardinal ac)
{
	return XtCreatePopupShell(name,xfeFrameShellWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeFrameShellGetBypassShell(Widget w)
{
    XfeFrameShellPart *	fp = _XfeFrameShellPart(w);
	Arg					av[10];
	Cardinal			ac = 0;

	assert( XfeIsFrameShell(w) );

#if 0
	assert( _XfeIsRealized(w) );

	if (!_XfeIsRealized(w))
	{
		return False;
	}
#endif

#if 1
	/*
	 * Make sure the frame shell is realized or else the colormap and visual
	 * stuff below will fail.
	 */
	if (!_XfeIsRealized(w))
	{
		XtRealizeWidget(w);
	}
#endif

	/* Make sure a shared bypass shell does not already exist */
	if (_XfeIsAlive(fp->bypass_shell))
	{
		return fp->bypass_shell;
	}

	XtSetArg(av[ac],XmNcolormap,	XfeColormap(w));	ac++;
	XtSetArg(av[ac],XmNvisual,		XfeVisual(w));		ac++;
	XtSetArg(av[ac],XmNdepth,		XfeDepth(w));		ac++;

	fp->bypass_shell = XfeCreateBypassShell(w,BYPASS_SHELL_NAME,av,ac);

	/* Make sure its realized */
 	XtRealizeWidget(fp->bypass_shell);

	return fp->bypass_shell;
}
/*----------------------------------------------------------------------*/
