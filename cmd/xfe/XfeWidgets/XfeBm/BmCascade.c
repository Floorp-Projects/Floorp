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
/* Name:		<XfeBm/BmCascade.c>										*/
/* Description:	XfeBmCascade widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/BmCascadeP.h>
#include <Xfe/BmButtonP.h>
#include <Xfe/Cascade.h>
#include <Xfe/ManagerP.h>

#include <Xm/CascadeBGP.h>

#include <Xm/MenuShellP.h>
#include <Xm/RowColumnP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeBmCascade."
#define MESSAGE2 "XmNaccentType is a read-only reasource."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				ClassInitialize	();
static void				Initialize		(Widget,Widget,ArgList,Cardinal *);
static void				Destroy			(Widget);
static void				Resize			(Widget);
static void				Redisplay		(Widget,XEvent *,Region);
static Boolean			SetValues		(Widget,Widget,Widget,
										 ArgList,Cardinal *);

static XtGeometryResult	QueryGeometry	(Widget,XtWidgetGeometry *,
										 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	BorderHighlight			(Widget);
static void 	BorderUnhighlight		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	CheckDisarm				(Widget,XEvent *,char **,Cardinal *);
static void 	DelayedArm				(Widget,XEvent *,char **,Cardinal *);
static void 	DoSelect				(Widget,XEvent *,char **,Cardinal *);
static void 	Motion					(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeBmCascade functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DrawPixmap				(Widget,XEvent *,Region);

#ifdef notused
static Position	GetPointerY				(Widget);
#endif

static void		AccentUpdate			(Widget,Position);
static void		AccentDraw				(Widget);
static void		AccentErase				(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade Resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
    /* Pixmap resources */
    { 
		XmNaccentType,
		XmCAccentType,
		XmRAccentType,
		sizeof(unsigned char),
		XtOffsetOf(XfeBmCascadeRec , bm_cascade . accent_type),
		XmRImmediate, 
		(XtPointer) XmACCENT_NONE
    },
    { 
		XmNarmPixmap,
		XmCArmPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeBmCascadeRec , bm_cascade . arm_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNarmPixmapMask,
		XmCArmPixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeBmCascadeRec , bm_cascade . arm_pixmap_mask),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNlabelPixmapMask,
		XmCLabelPixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeBmCascadeRec , bm_cascade . label_pixmap_mask),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive extension record initialization							*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmPrimitiveClassExtRec xmPrimitiveClassExtRec = 
{
	NULL,									/* next_extension			*/
	NULLQUARK,								/* record_type				*/
	XmPrimitiveClassExtVersion,				/* version					*/
	sizeof(XmPrimitiveClassExtRec),			/* record_size				*/
	XmInheritBaselineProc,					/* widget_baseline			*/
	XmInheritDisplayRectProc,				/* widget_display_rect		*/
	XmInheritMarginsProc,					/* widget_margins			*/
};


/*----------------------------------------------------------------------*/
/*																		*/
/* XmCascadeButton extension record initialization						*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmBaseClassExtRec xmCascadeButtonClassExtRec = 
{
    NULL,                                   /* Next extension           */
    NULLQUARK,                              /* record type XmQmotif     */
    XmBaseClassExtVersion,                  /* version                  */
    sizeof(XmBaseClassExtRec),              /* size                     */
    XmInheritInitializePrehook,             /* initialize prehook       */
    XmInheritSetValuesPrehook,              /* set_values prehook       */
    XmInheritInitializePosthook,            /* initialize posthook      */
    XmInheritSetValuesPosthook,             /* set_values posthook      */
    XmInheritClass,                         /* secondary class          */
    XmInheritSecObjectCreate,               /* creation proc            */
    XmInheritGetSecResData,                 /* getSecResData            */
    {0},                                    /* fast subclass            */
    XmInheritGetValuesPrehook,              /* get_values prehook       */
    XmInheritGetValuesPosthook,             /* get_values posthook      */
    XmInheritClassPartInitPrehook,          /* classPartInitPrehook     */
    XmInheritClassPartInitPosthook,         /* classPartInitPosthook    */
    NULL,                                   /* ext_resources            */
    NULL,                                   /* compiled_ext_resources   */
    0,                                      /* num_ext_resources        */
    FALSE,                                  /* use_sub_resources        */
    XmInheritWidgetNavigable,               /* widgetNavigable          */
    XmInheritFocusChange,                   /* focusChange              */
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
	{ "CheckDisarm",			CheckDisarm			},
    { "DelayedArm",				DelayedArm			},
    { "DoSelect",				DoSelect			},
    { "Motion",					Motion				},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade widget class record initialization						*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(bmcascade,BmCascade) =
{
    {
		/* Core Part */
		(WidgetClass) &xmCascadeButtonClassRec,	/* superclass         	*/
		"XfeBmCascade",							/* class_name			*/
		sizeof(XfeBmCascadeRec),				/* widget_size        	*/
		ClassInitialize,						/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		FALSE,                                  /* class_inited       	*/
		Initialize,								/* initialize			*/
		NULL,                                   /* initialize_hook    	*/
		XtInheritRealize,                       /* realize            	*/
		actions,								/* actions            	*/
		XtNumber(actions),						/* num_actions        	*/
		resources,                              /* resources          	*/
		XtNumber(resources),                    /* num_resources      	*/
		NULLQUARK,                              /* xrm_class          	*/
		TRUE,                                   /* compress_motion    	*/
		XtExposeCompressMaximal,                /* compress_exposure  	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest   	*/
		Destroy,								/* destroy				*/
		Resize,									/* resize             	*/
		Redisplay,								/* expose             	*/
		SetValues,                              /* set_values         	*/
		NULL,                                   /* set_values_hook    	*/
		XtInheritSetValuesAlmost,				/* set_values_almost  	*/
		NULL,									/* get_values_hook		*/
		NULL,                                   /* accept_focus       	*/
		XtVersion,                              /* version            	*/
		NULL,                                   /* callback_private   	*/
		XtInheritTranslations,					/* tm_table           	*/
		QueryGeometry,							/* query_geometry     	*/
		XtInheritDisplayAccelerator,            /* display accel      	*/
		(XtPointer) &xmCascadeButtonClassExtRec	/* extension          	*/
    },

    /* XmPrimitive Part */
    {
		BorderHighlight,						/* border_highlight 	*/
		BorderUnhighlight,						/* border_unhighlight 	*/
		XtInheritTranslations,                  /* translations       	*/
		XmInheritArmAndActivate,				/* arm_and_activate   	*/
		NULL,									/* syn resources      	*/
		0,										/* num syn_resources  	*/
		(XtPointer) &xmPrimitiveClassExtRec,	/* extension          	*/
    },

    /* XmLabel Part */
	{
		XmInheritWidgetProc,					/* setOverrideCallback	*/
		XmInheritMenuProc,						/* menu procedures		*/
		XtInheritTranslations,					/* menu traversal xlat	*/
		(XtPointer) NULL,						/* extension			*/
	},

    /* XmCascadeButton Part */
	{
		(XtPointer) NULL,						/* extension			*/
	},

	/* XfeBmCascade Part */
	{
		(XtPointer) NULL,						/* extension			*/
	}
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeBmCascadeWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(bmcascade,BmCascade);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassInitialize()
{
	/* Register Bm Representation Types */
	XfeBmRegisterRepresentationTypes();
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeBmCascadePart *		bmc = _XfeBmCascadePart(nw);
	XmLabelPart *			lp = _XfeXmLabelPart(nw);

	/* Force the type to be string always */
	lp->label_type = XmSTRING;

	bmc->pixmap_GC = XfeAllocateTransparentGc(nw);

	_XfePixmapPrepare(nw,
					  &lp->pixmap,
					  &bmc->pixmap_width,
					  &bmc->pixmap_height,
					  XmNlabelPixmap);

	XfeOverrideTranslations(nw,_XfeBmButtonExtraTranslations);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeBmCascadePart * bmc = _XfeBmCascadePart(w);

	XtReleaseGC(w,bmc->pixmap_GC);
}
/*----------------------------------------------------------------------*/
static XtGeometryResult
QueryGeometry(Widget w,XtWidgetGeometry	*req,XtWidgetGeometry *reply)
{
    XfeBmCascadePart *		bmc = _XfeBmCascadePart(w);
	XmLabelPart *			lp = _XfeXmLabelPart(w);
	XtGeometryResult		result;
	XmCascadeButtonWidgetClass cbc = 
		(XmCascadeButtonWidgetClass) xmCascadeButtonWidgetClass;
	
	result = (*cbc->core_class.query_geometry)(w,req,reply);

	if (bmc->pixmap_width && bmc->pixmap_height)
	{
		Dimension offset;
		Dimension label_height;

		reply->width += (bmc->pixmap_width + XfeBmPixmapGetOffset());

		offset = 
			lp->margin_top +
			lp->margin_bottom +
			_XfePrimitiveOffset(w);

		label_height = reply->height - offset;

		reply->height = 
			XfeMax(label_height,bmc->pixmap_height) +
			offset;
    }

	return result;
}
/*----------------------------------------------------------------------*/
static void
Resize(Widget w)
{
    XfeBmCascadePart *			bmc = _XfeBmCascadePart(w);
	XmLabelPart *				lp = _XfeXmLabelPart(w);
	XmCascadeButtonWidgetClass	cbc = 
		(XmCascadeButtonWidgetClass) xmCascadeButtonWidgetClass;

	(*cbc->core_class.resize)(w);

	/* Layout the pixmap if needed */
	if (bmc->pixmap_width && bmc->pixmap_height)
	{
		/* Move the label outta the way to the right */
		lp->TextRect.x += (bmc->pixmap_width + XfeBmPixmapGetOffset());
	}
}
/*----------------------------------------------------------------------*/
static void
Redisplay(Widget w,XEvent *event,Region region)
{
	XmCascadeButtonWidgetClass		pwc = 
		(XmCascadeButtonWidgetClass) xmCascadeButtonWidgetClass;

	if (XfeBmAccentIsEnabled())
	{
		Dimension shadow_thickness = _XfeShadowThickness(w);

		/*
		 * We change the shadow thickness to 0, so that the real Expose() 
		 * method not draw anything the shadow so that the accent lines
		 * are not disturbed.
		 */
		_XfeShadowThickness(w)		= 0;
		_XfeHighlightThickness(w)	= shadow_thickness;
		
		(*pwc->core_class.expose)(w,event,region);
		
		_XfeShadowThickness(w)		= shadow_thickness;
		_XfeHighlightThickness(w)	= 0;
	}
	else
	{
		(*pwc->core_class.expose)(w,event,region);
	}
	
	DrawPixmap(w,event,region);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
	Boolean					redisplay = False;
	Boolean					resize = False;
/*  	XmCascadeButtonPart *	old_cp = _XfeXmCascadeButtonPart(ow); */
/*  	XmCascadeButtonPart *	new_cp = _XfeXmCascadeButtonPart(nw); */
	XmLabelPart *			old_lp = _XfeXmLabelPart(ow);
	XmLabelPart *			new_lp = _XfeXmLabelPart(nw);
    XfeBmCascadePart *		new_bmc = _XfeBmCascadePart(nw);
    XfeBmCascadePart *		old_bmc = _XfeBmCascadePart(ow);

	/* accent_type */
	if (new_bmc->accent_type != old_bmc->accent_type)
	{
		new_bmc->accent_type = old_bmc->accent_type;

		_XfeWarning(nw,MESSAGE2);
	}

	/* label_pixmap */
	if (new_lp->pixmap != old_lp->pixmap)
	{
		_XfePixmapPrepare(nw,&new_lp->pixmap,
						  &new_bmc->pixmap_width,
						  &new_bmc->pixmap_height,
						  XmNlabelPixmap);

		redisplay = True;
		resize = True;
	}

	/* arm_pixmap */
	if (new_bmc->arm_pixmap != old_bmc->arm_pixmap)
	{
		Dimension width;
		Dimension height;

		assert( _XfePixmapGood(new_lp->pixmap) );

		_XfePixmapPrepare(nw,&new_bmc->arm_pixmap,
						  &width,
						  &height,
						  XmNarmPixmap);

		assert( width == new_bmc->pixmap_width );
		assert( height == new_bmc->pixmap_height );

		redisplay = True;
		resize = True;
	}

	if (resize)
	{
		Resize(nw);
	}

    return redisplay;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
BorderHighlight(Widget w)
{
/*     XfeBmCascadePart *			bmc = _XfeBmCascadePart(w); */
	XmCascadeButtonWidgetClass		cwc = 
		(XmCascadeButtonWidgetClass) xmCascadeButtonWidgetClass;

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmProcWithoutDrawing(w,cwc->primitive_class.border_highlight);
	}
	else
	{
		(*cwc->primitive_class.border_highlight)(w);
	}

	DrawPixmap(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
static void
BorderUnhighlight(Widget w)
{
/*     XfeBmCascadePart *			bmc = _XfeBmCascadePart(w); */
	XmCascadeButtonWidgetClass		cwc = 
		(XmCascadeButtonWidgetClass) xmCascadeButtonWidgetClass;

	if (XfeBmAccentIsEnabled())
	{
 		_XfeBmProcWithoutDrawing(w,cwc->primitive_class.border_unhighlight);
	}
	else
	{
		(*cwc->primitive_class.border_unhighlight)(w);
	}

	DrawPixmap(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CheckDisarm(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	static XtActionProc		check_disarm_action = NULL;
/*     XfeBmCascadePart *		bmc = _XfeBmCascadePart(w); */

	if (!check_disarm_action)
	{
		check_disarm_action = _XfeGetActionProc(xmCascadeButtonWidgetClass,
											  "CheckDisarm");
	}

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmActionWithoutDrawing(w,check_disarm_action,event,params,nparams);

		AccentUpdate(w,XfeACCENT_OUTSIDE);
	}
	else
	{
		check_disarm_action(w,event,params,nparams);
	}

	/*
	 * Redraw the pixmap since fancy motif menus such as those in 
	 * irix or cde will fill the whole background and erase the 
	 * previous menu.  This should be smarter, and redraw the pixmap
	 * only if needed, but the extra waste in time is smaller than
	 * the effort needed to implement it.
	 */
	DrawPixmap(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
static void
DelayedArm(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	static XtActionProc		delayed_arm_action = NULL;
/*     XfeBmCascadePart *		bmc = _XfeBmCascadePart(w); */

	if (!delayed_arm_action)
	{
		delayed_arm_action = _XfeGetActionProc(xmCascadeButtonWidgetClass,
											  "DelayedArm");
	}

	if (XfeBmAccentIsEnabled())
	{
		/*
		 * There is a very nasty motif bug that causes the cascading shell
		 * to appear much to the right of what it is supposed to be.
		 * 
		 * The reason for this is very complicated, but fortunately the
		 * solution is pretty simple.  By moving the parent row column
		 * to (0,0) every time before the cascading shell is posted, the
		 * problem is fixed.
		 */
		_XfeMoveWidget(XtParent(w),0,0);

		_XfeBmActionWithoutDrawing(w,delayed_arm_action,event,params,nparams);
		
		AccentUpdate(w,event->xcrossing.y);
	}
	else
	{
		delayed_arm_action(w,event,params,nparams);
	}


	/*
	 * Redraw the pixmap since fancy motif menus such as those in 
	 * irix or cde will fill the whole background and erase the 
	 * previous menu.  This should be smarter, and redraw the pixmap
	 * only if needed, but the extra waste in time is smaller than
	 * the effort needed to implement it.
	 */
	DrawPixmap(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
static void
DoSelect(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	static XtActionProc do_select_action = NULL;


	if (!do_select_action)
	{
		do_select_action = _XfeGetActionProc(xmCascadeButtonWidgetClass,
											 "DoSelect");
	}

	if (XfeBmAccentIsEnabled())
	{
		XmCascadeButtonPart * cp = _XfeXmCascadeButtonPart(w);

  		_XmMenuPopDown(XtParent(w),event,NULL);

		_XfeInvokeCallbacks(w,cp->activate_callback,XmCR_ACTIVATE,event,False);
		
		AccentUpdate(w,XfeACCENT_OUTSIDE);
	}
	else
	{
		do_select_action(w,event,params,nparams);
	}
}
/*----------------------------------------------------------------------*/
static void
Motion(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
/*     XfeBmCascadePart *		bmc = _XfeBmCascadePart(w); */

	if (!XfeBmAccentIsEnabled())
	{
		return;
	}

	AccentUpdate(w,event->xmotion.y);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeBmCascade functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DrawPixmap(Widget w,XEvent *event,Region region)
{
    XfeBmCascadePart *		bmc = _XfeBmCascadePart(w);
	XmLabelPart *			lp = _XfeXmLabelPart(w);
	Pixmap					pixmap = XmUNSPECIFIED_PIXMAP;
	Pixmap					mask = XmUNSPECIFIED_PIXMAP;
	Boolean					armed = CB_IsArmed(w);

	if (armed && _XfePixmapGood(bmc->arm_pixmap))
	{
		pixmap = bmc->arm_pixmap;

		if (_XfePixmapGood(bmc->arm_pixmap_mask))
		{
			mask = bmc->arm_pixmap_mask;
		}
	}
	else if (_XfePixmapGood(lp->pixmap) && 
			 bmc->pixmap_width && bmc->pixmap_height)
	{
		pixmap = lp->pixmap;

		if (_XfePixmapGood(bmc->label_pixmap_mask))
		{
			mask = bmc->label_pixmap_mask;
		}
	}

	if (_XfePixmapGood(pixmap))
	{
		Position x = _XfePrimitiveOffset(w) + lp->margin_width;
		Position y = (_XfeHeight(w) - bmc->pixmap_height) / 2;

		if (_XfePixmapGood(mask))
		{
			XSetClipOrigin(XtDisplay(w),bmc->pixmap_GC,x,y);
			XSetClipMask(XtDisplay(w),bmc->pixmap_GC,mask);
		}
		else
		{
			XSetClipMask(XtDisplay(w),bmc->pixmap_GC,None);
		}

		XCopyArea(XtDisplay(w),
				  pixmap,
				  _XfeWindow(w),
				  bmc->pixmap_GC,
				  0,0,
				  bmc->pixmap_width,
				  bmc->pixmap_height,
				  x,
				  y);
	}

}
/*----------------------------------------------------------------------*/
#ifdef notused
static Position
GetPointerY(Widget w)
{
	Window			root;
	Window			child;
	int				rx;
	int				ry;
	int				cx;
	int				cy;
	unsigned int	mask;

	if (XQueryPointer(XtDisplay(w),_XfeWindow(w),&root,&child,
					  &rx,&ry,&cx,&cy,&mask))
	{
		cy -= _XfeY(w);

		if (cy < 0)
		{
			cy = 0;
		}
		
		if (cy > _XfeHeight(w))
		{
			cy = _XfeHeight(w);
		}

		return cy;
	}

	return 0;
}
/*----------------------------------------------------------------------*/
#endif
static void
AccentUpdate(Widget w,Position y)
{
	unsigned char 		new_accent_type = XmACCENT_NONE;
	XfeBmCascadePart * 	bmc = _XfeBmCascadePart(w);
	Dimension			one_fourth = _XfeHeight(w) / 4;

	/* Determine the new accent */
	if (XfeBmAccentGetFileMode() == XmACCENT_FILE_ANYWHERE)
	{
		if ( (y <= one_fourth) && (y >= 0) )
		{
			new_accent_type = XmACCENT_TOP;
		}
		else if ( (y > (_XfeHeight(w) - one_fourth)) && (y <= _XfeHeight(w)) )
		{
			new_accent_type = XmACCENT_BOTTOM;
		}
		else if ( (y > one_fourth) && (y <= (_XfeHeight(w) - one_fourth)))
		{
			new_accent_type = XmACCENT_ALL;
		}
	}
	else
	{
		if ( (y >= 0) && (y <= _XfeHeight(w)) )
		{
			new_accent_type = XmACCENT_ALL;
		}
	}

	/* Make sure the new accent has changed */
	if (new_accent_type == bmc->accent_type)
	{
 		return;
	}

	AccentErase(w);

	bmc->accent_type = new_accent_type;

	AccentDraw(w);
}
/*----------------------------------------------------------------------*/
static void
AccentDraw(Widget w)
{
	XfeBmCascadePart * 	bmc = _XfeBmCascadePart(w);

	if (bmc->accent_type == XmACCENT_NONE)
	{
		return;
	}

	XfeMenuItemDrawAccent(w,
						  bmc->accent_type,
						  XfeBmAccentGetOffsetLeft(),
						  XfeBmAccentGetOffsetRight(),
						  XfeBmAccentGetShadowThickness(),
						  XfeBmAccentGetThickness());
}
/*----------------------------------------------------------------------*/
static void
AccentErase(Widget w)
{
	XfeBmCascadePart * 	bmc = _XfeBmCascadePart(w);

	if (bmc->accent_type == XmACCENT_NONE)
	{
		return;
	}

	XfeMenuItemEraseAccent(w,
						   bmc->accent_type,
						   XfeBmAccentGetOffsetLeft(),
						   XfeBmAccentGetOffsetRight(),
						   XfeBmAccentGetShadowThickness(),
						   XfeBmAccentGetThickness());
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade public functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
/*extern*/ Widget
XfeCreateBmCascade(Widget pw,char * name,Arg * av,Cardinal ac)
{
    return XtCreateWidget(name,xfeBmCascadeWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/

