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
/* Name:		<Xfe/BypassShell.c>										*/
/* Description:	XfeBypassShell widget source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/BypassShellP.h>

#include <Xm/AtomMgr.h>
#include <Xm/Protocols.h>

#ifdef EDITRES
#include <X11/Xmu/Editres.h>
#endif

#define MESSAGE1 "Widget is not an XfeBypassShell."
#define MESSAGE2 "XfeBypassShell can only have one managed child."

#define STRUCTURE_EVENTS	StructureNotifyMask

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassInitialize	();
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void		Resize			(Widget);
static void		Redisplay		(Widget,XEvent *,Region);
static void		Realize			(Widget,XtValueMask *,XSetWindowAttributes *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				InsertChild		(Widget);
static void				DeleteChild		(Widget);
static void				ChangeManaged	(Widget);
static XtGeometryResult GeometryManager	(Widget,XtWidgetGeometry *,
										 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	BtnUp				(Widget,XEvent *,char **,Cardinal *);
static void 	BtnDown				(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeBypassShell functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DrawShadow			(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Shell structure event handler										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		MappingEH			(Widget,XtPointer,XEvent *,Boolean *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell resources												*/
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
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . realize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNbeforeRealizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . before_realize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* Mapping callback resources */
	{ 
		XmNmapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . map_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNunmapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . unmap_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	{ 
		XmNchangeManagedCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . change_managed_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* Shadow resources */
	{ 
		XmNbottomShadowColor,
		XmCBottomShadowColor,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . bottom_shadow_color),
		XmRCallProc, 
		(XtPointer) _XmBottomShadowColorDefault,
	},
	{ 
		XmNtopShadowColor,
		XmCTopShadowColor,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . top_shadow_color),
		XmRCallProc, 
		(XtPointer) _XmTopShadowColorDefault,
	},

	{ 
		XmNshadowThickness,
		XmCShadowThickness,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . shadow_thickness),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_SHADOW_THICKNESS 
	},
	{ 
		XmNshadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . shadow_type),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_SHADOW_TYPE
	},

	/* Cursor resources */
	{ 
		XmNcursor,
		XmCCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . cursor),
		XmRString, 
		"arrow"
	},

	/* Other resources */
	{ 
		XmNignoreExposures,
		XmCIgnoreExposures,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeBypassShellRec , xfe_bypass_shell . ignore_exposures),
		XmRImmediate, 
		(XtPointer) True
	},

	/* Override Shell resources */
	{ 
		XmNallowShellResize,
		XmCAllowShellResize,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeBypassShellRec , shell . allow_shell_resize),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNoverrideRedirect,
		XmCOverrideRedirect,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeBypassShellRec , shell . override_redirect),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNsaveUnder,
		XmCSaveUnder,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeBypassShellRec , shell . save_under),
		XmRImmediate, 
		(XtPointer) False
	},

	/* Override WmShell resources */
	{ 
		XmNtransient,
		XmCTransient,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeBypassShellRec , wm . transient),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNwaitForWm,
		XmCWaitForWm,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeBypassShellRec , wm . wait_for_wm),
		XmRImmediate, 
		(XtPointer) False
	},
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell actions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "BtnDown",			BtnDown				},
    { "BtnUp",				BtnUp				},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(bypassshell,BypassShell) =
{
    {
		(WidgetClass) &vendorShellClassRec,		/* superclass			*/
		"XfeBypassShell",						/* class_name			*/
		sizeof(XfeBypassShellRec),				/* widget_size			*/
		ClassInitialize,						/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		False,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		Realize,								/* realize				*/
#if 0
		actions,								/* actions            	*/
		XtNumber(actions),						/* num_actions        	*/
#else
		NULL,									/* actions            	*/
		0,										/* num_actions        	*/
#endif
		resources,                              /* resources			*/
		XtNumber(resources),                    /* num_resources		*/
		NULLQUARK,								/* xrm_class			*/
		True,									/* compress_motion		*/
		XtExposeCompressMaximal,				/* compress_exposure	*/
		True,									/* compress_enterleave	*/
		False,									/* visible_interest		*/
		Destroy,								/* destroy				*/
		Resize,									/* resize				*/
		Redisplay,								/* expose				*/
		SetValues,                              /* set_values			*/
		NULL,                                   /* set_values_hook		*/
		NULL,									/* set_values_almost	*/
		NULL,									/* get_values_hook		*/
		NULL,                                   /* access_focus			*/
		XtVersion,                              /* version				*/
		NULL,                                   /* callback_private		*/
		_XfeBypassShellDefaultTranslations,		/* tm_table				*/
		NULL,									/* query_geometry		*/
		NULL,									/* display accelerator	*/
		NULL,									/* extension			*/
    },
    
    /* Composite Part */
    {
		XtInheritGeometryManager,				/* geometry_manager		*/
		ChangeManaged,							/* change_managed		*/
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

    /* XfeBypassShell Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeBypassShellWidgetClass declaration.								*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(bypassshell,BypassShell);

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
    XfeBypassShellPart *		bp = _XfeBypassShellPart(nw);

	/* Make sure the shadow type is ok */
	XfeRepTypeCheck(nw,XmRShadowType,&bp->shadow_type,XfeDEFAULT_SHADOW_TYPE);

	/* Add mapping event handler */
	XtAddEventHandler(nw,StructureNotifyMask,True,MappingEH,nw);

    /* Allocate the shadow GCs */
    bp->top_shadow_GC = 
		XfeAllocateColorGc(nw,bp->top_shadow_color,None,True);

    bp->bottom_shadow_GC = 
		XfeAllocateColorGc(nw,bp->bottom_shadow_color,None,True);

    /* Initialize private members */
	bp->managed_child = False;
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeBypassShellPart *		bp = _XfeBypassShellPart(w);

    XtReleaseGC(w,bp->top_shadow_GC);
    XtReleaseGC(w,bp->bottom_shadow_GC);
}
/*----------------------------------------------------------------------*/
static void
Realize(Widget w,XtValueMask * mask,XSetWindowAttributes * wa)
{
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);

    /* Invoke before realize Callbacks */
    _XfeInvokeCallbacks(w,bp->before_realize_callback,
						XmCR_BEFORE_REALIZE,NULL,False);

    /* The actual realization is handled by the superclass */
	(*vendorShellWidgetClass->core_class.realize)(w,mask,wa);

    /* Invoke realize Callbacks */
    _XfeInvokeCallbacks(w,bp->realize_callback,XmCR_REALIZE,NULL,False);
}
/*----------------------------------------------------------------------*/
static void
Resize(Widget w)
{
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);

    /* The actual realization is handled by the superclass */
    (*vendorShellWidgetClass->core_class.resize)(w);
}
/*----------------------------------------------------------------------*/
static void
Redisplay(Widget w,XEvent *event,Region region)
{
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);

	if (!bp->ignore_exposures)
	{
		DrawShadow(w,event,region,NULL);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeBypassShellPart *		np = _XfeBypassShellPart(nw);
    XfeBypassShellPart *		op = _XfeBypassShellPart(ow);
	Boolean					redisplay = False;

    /* shadow_thickness */
    if (np->shadow_thickness != op->shadow_thickness)
	{
		redisplay = True;
	}

    /* shadow_type */
    if (np->shadow_type != op->shadow_type)
	{
		/* Make sure the new shadow type is ok */
		XfeRepTypeCheck(nw,XmRShadowType,&np->shadow_type,
						XfeDEFAULT_SHADOW_TYPE);

		redisplay = True;
	}

    /* bottom_shadow_color */
    if (np->bottom_shadow_color != op->bottom_shadow_color)
	{
		XtReleaseGC(nw,np->bottom_shadow_GC);

		np->bottom_shadow_GC = 
			XfeAllocateColorGc(nw,np->bottom_shadow_color,None,True);

		redisplay = True;
	}

    /* top_shadow_color */
    if (np->top_shadow_color != op->top_shadow_color)
	{
		XtReleaseGC(nw,np->top_shadow_GC);

		np->top_shadow_GC = 
			XfeAllocateColorGc(nw,np->top_shadow_color,None,True);

		redisplay = True;
	}
    
    return redisplay;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
InsertChild(Widget child)
{
}
/*----------------------------------------------------------------------*/
static void
DeleteChild(Widget child)
{
}
/*----------------------------------------------------------------------*/
static void
ChangeManaged(Widget w)
{
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);
	Cardinal				i;
	Widget					new_managed_child = NULL;
	Widget					old_managed_child = bp->managed_child;

	for(i = 0; i < _XfemNumChildren(w); i++)
	{
		Widget child = _XfeChildrenIndex(w,i);

		/* Look for the first child that is managed */
		if ((child != old_managed_child) && _XfeChildIsShown(child))
		{
			new_managed_child = child;
		}
	}

	printf("ChangeManaged(%s,last = %s, new = %s)\n",
		   XtName(w),
		   old_managed_child ? XtName(old_managed_child) : "NULL",
		   new_managed_child ? XtName(new_managed_child) : "NULL");

	if (new_managed_child)
	{
		/* Assign the new managed child */
		bp->managed_child = new_managed_child;
		
		/* Invoke before change managed Callbacks */
		_XfeInvokeCallbacks(w,bp->change_managed_callback,
							XmCR_CHANGE_MANAGED,NULL,False);
		
		_XfeConfigureWidget(bp->managed_child,
							bp->shadow_thickness,
							bp->shadow_thickness,
							_XfeWidth(w) - 2 * bp->shadow_thickness,
							_XfeHeight(w) - 2 * bp->shadow_thickness);

		if (_XfeIsRealized(bp->managed_child))
		{
			XRaiseWindow(XtDisplay(w),_XfeWindow(bp->managed_child));
		}
	}
}
/*----------------------------------------------------------------------*/
static XtGeometryResult
GeometryManager(Widget child,XtWidgetGeometry *request,XtWidgetGeometry *reply)
{
	XtGeometryResult	result;

	return result;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
BtnDown(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);

	printf("BtnDown(%s)\n",XtName(w));
}
/*----------------------------------------------------------------------*/
static void
BtnUp(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);

	printf("BtnUp(%s)\n",XtName(w));
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
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);

	/* Make sure the shell is still alive */
	if (_XfeIsAlive(w))
	{
		switch(event->type) 
		{
			/* Map */
		case MapNotify:

			DrawShadow(w,NULL,NULL,NULL);
			
			/* Invoke map callbacks */
			_XfeInvokeCallbacks(w,bp->map_callback,XmCR_MAP,NULL,False);

			break;
			
			/* Unmap */
		case UnmapNotify:
			
			/* Invoke unmap callbacks */
			_XfeInvokeCallbacks(w,bp->unmap_callback,XmCR_UNMAP,NULL,False);

			break;
		}
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeBypassShell functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DrawShadow(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);

	if (!bp->shadow_thickness)
	{
		return;

	}

    _XmDrawShadows(XtDisplay(w),_XfeWindow(w),
				   bp->top_shadow_GC,bp->bottom_shadow_GC,
				   0,0,_XfeWidth(w),_XfeHeight(w),
				   bp->shadow_thickness,bp->shadow_type);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell private methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget _bypass_shell_global = NULL;

/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeBypassShellGlobalIsAlive(void)
{
	return _XfeIsAlive(_bypass_shell_global);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
_XfeBypassShellGlobalAccess(void)
{
	assert( _XfeIsAlive(_bypass_shell_global) );

	return _bypass_shell_global;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
_XfeBypassShellGlobalInitialize(Widget pw,char * name,Arg * av,Cardinal ac)
{
	assert( ! _XfeIsAlive(_bypass_shell_global) );

	_bypass_shell_global = XfeCreateBypassShell(pw,name,av,ac);

	return _bypass_shell_global;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell public methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateBypassShell(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreatePopupShell(name,xfeBypassShellWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
