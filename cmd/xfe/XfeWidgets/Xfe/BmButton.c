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
/* Name:		<Xfe/BmButton.c>										*/
/* Description:	XfeBmButton widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/BmButtonP.h>
#include <Xfe/ManagerP.h>
#include <Xfe/BmCascade.h>
#include <Xfe/CascadeP.h>
#include <Xm/RowColumnP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeBmButton."
#define MESSAGE2 "XmNaccentType is a read-only reasource."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
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
/* XfeBmButton action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Enter			(Widget,XEvent *,char **,Cardinal *);
static void 	Leave			(Widget,XEvent *,char **,Cardinal *);
static void 	Motion			(Widget,XEvent *,char **,Cardinal *);
static void 	BtnDown			(Widget,XEvent *,char **,Cardinal *);
static void 	BtnUp			(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeBmButton functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PixmapDraw				(Widget,XEvent *,Region);

static void		AccentUpdate			(Widget,Position);
static void		AccentDraw				(Widget);
static void		AccentErase				(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton Resources												*/
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
		XtOffsetOf(XfeBmButtonRec , bm_button . accent_type),
		XmRImmediate, 
		(XtPointer) XmACCENT_NONE
    },
    { 
		XmNarmPixmapMask,
		XmCArmPixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeBmButtonRec , bm_button . arm_pixmap_mask),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNlabelPixmapMask,
		XmCLabelPixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeBmButtonRec , bm_button . label_pixmap_mask),
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
/* XmPushButton extension record initialization							*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmBaseClassExtRec xmPushButtonClassExtRec = 
{
    NULL,									/* Next extension			*/
    NULLQUARK,								/* record type XmQmotif		*/
    XmBaseClassExtVersion,					/* version					*/
    sizeof(XmBaseClassExtRec),				/* size						*/
    XmInheritInitializePrehook,				/* initialize prehook		*/
    XmInheritSetValuesPrehook,				/* set_values prehook		*/
    XmInheritInitializePosthook,			/* initialize posthook		*/
    XmInheritSetValuesPosthook,				/* set_values posthook		*/
    XmInheritClass,							/* secondary class			*/
    XmInheritSecObjectCreate,				/* creation proc			*/
    XmInheritGetSecResData,					/* getSecResData			*/
    {0},									/* fast subclass			*/
    XmInheritGetValuesPrehook,				/* get_values prehook		*/
    XmInheritGetValuesPosthook,				/* get_values posthook		*/
    XmInheritClassPartInitPrehook,			/* classPartInitPrehook		*/
    XmInheritClassPartInitPosthook,			/* classPartInitPosthook	*/
    NULL,									/* ext_resources			*/
    NULL,									/* compiled_ext_resources	*/
    0,										/* num_ext_resources		*/
    FALSE,									/* use_sub_resources		*/
    XmInheritWidgetNavigable,				/* widgetNavigable			*/
    XmInheritFocusChange,					/* focusChange				*/
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "BtnDown",			BtnDown			},
    { "BtnUp",				BtnUp			},
    { "Enter",				Enter			},
    { "Motion",				Motion			},
    { "Leave",				Leave			},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton widget class record initialization						*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(bmbutton,BmButton) =
{
    {
		/* Core Part */
		(WidgetClass) &xmPushButtonClassRec,	/* superclass         	*/
		"XfeBmButton",							/* class_name			*/
		sizeof(XfeBmButtonRec),					/* widget_size        	*/
		NULL,									/* class_initialize   	*/
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
		(XtPointer) &xmPushButtonClassExtRec	/* extension          	*/
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

    /* XmPushButton Part */
	{
		(XtPointer) NULL,						/* extension			*/
	},

	/* XfeBmButton Part */
	{
		(XtPointer) NULL,						/* extension			*/
	}
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeBmButtonWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(bmbutton,BmButton);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(nw);
	XmLabelPart *			lp = _XfeXmLabelPart(nw);

	assert( XmIsRowColumn(XtParent(nw)) );

	/* Force the type to be string always */
	lp->label_type = XmSTRING;

	bmp->pixmap_GC = XfeAllocateTransparentGc(nw);

	_XfePixmapPrepare(nw,
					  &lp->pixmap,
					  &bmp->pixmap_width,
					  &bmp->pixmap_height,
					  XmNlabelPixmap);

	XfeOverrideTranslations(nw,_XfeBmButtonExtraTranslations);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);
	
	XtReleaseGC(w,bmp->pixmap_GC);
}
/*----------------------------------------------------------------------*/
static XtGeometryResult
QueryGeometry(Widget w,XtWidgetGeometry	*req,XtWidgetGeometry *reply)
{
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);
	XmLabelPart *			lp = _XfeXmLabelPart(w);
	XtGeometryResult		result;
	XmPushButtonWidgetClass pwc = 
		(XmPushButtonWidgetClass) xmPushButtonWidgetClass;
	
	result = (*pwc->core_class.query_geometry)(w,req,reply);

	if (bmp->pixmap_width && bmp->pixmap_height)
	{
		Dimension offset;
		Dimension label_height;

		reply->width += (bmp->pixmap_width + XfeBmPixmapGetOffset());

		offset = 
			lp->margin_top +
			lp->margin_bottom +
			_XfePrimitiveOffset(w);

		label_height = reply->height - offset;

		reply->height = 
			XfeMax(label_height,bmp->pixmap_height) +
			offset;
	}

	return result;
}
/*----------------------------------------------------------------------*/
static void
Resize(Widget w)
{
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);
	XmLabelPart *			lp = _XfeXmLabelPart(w);
	XmPushButtonWidgetClass pwc = 
		(XmPushButtonWidgetClass) xmPushButtonWidgetClass;

	(*pwc->core_class.resize)(w);

	/* Layout the pixmap if needed */
	if (bmp->pixmap_width && bmp->pixmap_height)
	{
		/* Move the label outta the way to the right */
		lp->TextRect.x += (bmp->pixmap_width + XfeBmPixmapGetOffset());
	}
}
/*----------------------------------------------------------------------*/
static void
Redisplay(Widget w,XEvent *event,Region region)
{
	XmPushButtonWidgetClass		pwc = 
		(XmPushButtonWidgetClass) xmPushButtonWidgetClass;

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
	
	PixmapDraw(w,event,region);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
	Boolean					redisplay = False;
	Boolean					resize = False;
	XmPushButtonPart *		old_bp = _XfeXmPushButtonPart(ow);
	XmPushButtonPart *		new_bp = _XfeXmPushButtonPart(nw);
	XmLabelPart *			old_lp = _XfeXmLabelPart(ow);
	XmLabelPart *			new_lp = _XfeXmLabelPart(nw);
    XfeBmButtonPart *		new_bmp = _XfeBmButtonPart(nw);
    XfeBmButtonPart *		old_bmp = _XfeBmButtonPart(ow);

	/* accent_type */
	if (new_bmp->accent_type != old_bmp->accent_type)
	{
		new_bmp->accent_type = old_bmp->accent_type;

		_XfeWarning(nw,MESSAGE2);
	}

	/* label_pixmap */
	if (new_lp->pixmap != old_lp->pixmap)
	{
		_XfePixmapPrepare(nw,&new_lp->pixmap,
						  &new_bmp->pixmap_width,
						  &new_bmp->pixmap_height,
						  XmNlabelPixmap);

		redisplay = True;
		resize = True;
	}

	/* arm_pixmap */
	if (new_bp->arm_pixmap != old_bp->arm_pixmap)
	{
		Dimension width;
		Dimension height;

		assert( _XfePixmapGood(new_lp->pixmap) );

		_XfePixmapPrepare(nw,&new_bp->arm_pixmap,
						  &width,
						  &height,
						  XmNarmPixmap);

		assert( width == new_bmp->pixmap_width );
		assert( height == new_bmp->pixmap_height );

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
    XfeBmButtonPart *			bmp = _XfeBmButtonPart(w);
	XmPushButtonWidgetClass		pwc = 
		(XmPushButtonWidgetClass) xmPushButtonWidgetClass;

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmProcWithoutDrawing(w,pwc->primitive_class.border_highlight);
	}
	else
	{
		(*pwc->primitive_class.border_highlight)(w);
	}

	PixmapDraw(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
static void
BorderUnhighlight(Widget w)
{
    XfeBmButtonPart *			bmp = _XfeBmButtonPart(w);
	XmPushButtonWidgetClass		pwc = 
		(XmPushButtonWidgetClass) xmPushButtonWidgetClass;

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmProcWithoutDrawing(w,pwc->primitive_class.border_unhighlight);
	}
	else
	{
		(*pwc->primitive_class.border_unhighlight)(w);
	}

	PixmapDraw(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Enter(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	static XtActionProc		enter_action = NULL;
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);

	if (!enter_action)
	{
		enter_action = _XfeGetActionProc(xmPushButtonWidgetClass,"Enter");
	}

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmActionWithoutDrawing(w,enter_action,event,params,nparams);

		AccentUpdate(w,event->xcrossing.y);
	}
	else
	{
		enter_action(w,event,params,nparams);
	}

	/*
	 * Redraw the pixmap since fancy motif menus such as those in 
	 * irix or cde will fill the whole background and erase the 
	 * previous menu.  This should be smarter, and redraw the pixmap
	 * only if needed, but the extra waste in time is smaller than
	 * the effort needed to implement it.
	 */
	PixmapDraw(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
static void
Leave(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);
	static XtActionProc		leave_action = NULL;

	if (!leave_action)
	{
		leave_action = _XfeGetActionProc(xmPushButtonWidgetClass,"Leave");
	}

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmActionWithoutDrawing(w,leave_action,event,params,nparams);

		AccentUpdate(w,XfeACCENT_OUTSIDE);
	}
	else
	{
		leave_action(w,event,params,nparams);
	}

	/*
	 * Redraw the pixmap since fancy motif menus such as those in 
	 * irix or cde will fill the whole background and erase the 
	 * previous menu.  This should be smarter, and redraw the pixmap
	 * only if needed, but the extra waste in time is smaller than
	 * the effort needed to implement it.
	 */
	PixmapDraw(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
static void
Motion(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);

	if (!XfeBmAccentIsEnabled())
	{
		return;
	}

	AccentUpdate(w,event->xmotion.y);
}
/*----------------------------------------------------------------------*/
static void
BtnDown(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	static XtActionProc		btn_down_action = NULL;
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);

	if (!btn_down_action)
	{
		btn_down_action = _XfeGetActionProc(xmPushButtonWidgetClass,"BtnDown");
	}

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmActionWithoutDrawing(w,btn_down_action,event,params,nparams);

		AccentUpdate(w,event->xbutton.y);
	}
	else
	{
		btn_down_action(w,event,params,nparams);
	}
}
/*----------------------------------------------------------------------*/
static void
BtnUp(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	static XtActionProc		btn_up_action = NULL;
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);

	if (!btn_up_action)
	{
		btn_up_action = _XfeGetActionProc(xmPushButtonWidgetClass,"BtnUp");
	}

	if (XfeBmAccentIsEnabled())
	{
		_XfeBmActionWithoutDrawing(w,btn_up_action,event,params,nparams);

		AccentUpdate(w,XfeACCENT_OUTSIDE);
	}
	else
	{
		btn_up_action(w,event,params,nparams);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeBmButton functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PixmapDraw(Widget w,XEvent *event,Region region)
{
    XfeBmButtonPart *		bmp = _XfeBmButtonPart(w);
	XmLabelPart *			lp = _XfeXmLabelPart(w);
	XmPushButtonPart *		bp = _XfeXmPushButtonPart(w);
	Pixmap					pixmap = XmUNSPECIFIED_PIXMAP;
	Pixmap					mask = XmUNSPECIFIED_PIXMAP;

	if (bp->armed && _XfePixmapGood(bp->arm_pixmap))
	{
		pixmap = bp->arm_pixmap;

		if (_XfePixmapGood(bmp->arm_pixmap_mask))
		{
			mask = bmp->arm_pixmap_mask;
		}
	}
	else if (_XfePixmapGood(lp->pixmap) && 
			 bmp->pixmap_width && bmp->pixmap_height)
	{
		pixmap = lp->pixmap;

		if (_XfePixmapGood(bmp->label_pixmap_mask))
		{
			mask = bmp->label_pixmap_mask;
		}
	}

	if (_XfePixmapGood(pixmap))
	{
		Position x = _XfePrimitiveOffset(w) + lp->margin_width;
		Position y = (_XfeHeight(w) - bmp->pixmap_height) / 2;

		if (_XfePixmapGood(mask))
		{
			XSetClipOrigin(XtDisplay(w),bmp->pixmap_GC,x,y);
			XSetClipMask(XtDisplay(w),bmp->pixmap_GC,mask);
		}
		else
		{
			XSetClipMask(XtDisplay(w),bmp->pixmap_GC,None);
		}

		XCopyArea(XtDisplay(w),
				  pixmap,
				  _XfeWindow(w),
				  bmp->pixmap_GC,
				  0,0,
				  bmp->pixmap_width,
				  bmp->pixmap_height,
				  x,
				  y);
	}
}
/*----------------------------------------------------------------------*/
static void
AccentUpdate(Widget w,Position y)
{
	unsigned char 		new_accent_type = XmACCENT_NONE;
	XfeBmButtonPart * 	bmp = _XfeBmButtonPart(w);
	Dimension			half = _XfeHeight(w) / 2;

	/* Determine the new accent */
	if ( (y <= half) && (y >= 0) )
	{
		new_accent_type = XmACCENT_TOP;
	}
	else if ( (y > half) && (y <= _XfeHeight(w)) )
	{
		new_accent_type = XmACCENT_BOTTOM;
	}

	/* Make sure the new accent has changed */
	if (new_accent_type == bmp->accent_type)
	{
 		return;
	}

	AccentErase(w);

	bmp->accent_type = new_accent_type;

	AccentDraw(w);
}
/*----------------------------------------------------------------------*/
static void
AccentDraw(Widget w)
{
	XfeBmButtonPart * 	bmp = _XfeBmButtonPart(w);

	if (bmp->accent_type == XmACCENT_NONE || bmp->accent_type == XmACCENT_ALL)
	{
		return;
	}

	XfeMenuItemDrawAccent(w,
						  bmp->accent_type,
						  XfeBmAccentGetOffsetLeft(),
						  XfeBmAccentGetOffsetRight(),
						  XfeBmAccentGetShadowThickness(),
						  XfeBmAccentGetThickness());
}
/*----------------------------------------------------------------------*/
static void
AccentErase(Widget w)
{
	XfeBmButtonPart * 	bmp = _XfeBmButtonPart(w);

	if (bmp->accent_type == XmACCENT_NONE || bmp->accent_type == XmACCENT_ALL)
	{
		return;
	}

	XfeMenuItemEraseAccent(w,
						   bmp->accent_type,
						   XfeBmAccentGetOffsetLeft(),
						   XfeBmAccentGetOffsetRight(),
						   XfeBmAccentGetShadowThickness(),
						   XfeBmAccentGetThickness());
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeBmActionWithoutDrawing(Widget			w,
						   XtActionProc		proc,
						   XEvent *			event,
						   char **			params,
						   Cardinal *		nparams)
{
	Widget shell;
	Window window;

	assert( _XfeIsAlive(w) );
	assert( proc != NULL );

	shell = XfeAncestorFindByClass(w,shellWidgetClass,XfeFIND_ANY);

	window = _XfeWindow(w);

	/*
	 * We change the width to 0, so that the real Enter() 
	 * action does not draw anything in the cascade button so that
	 * we can draw the accent lines.
	 */
	_XfeWindow(w) = _XfeWindow(shell);
		
	(*proc)(w,event,params,nparams);
		
	_XfeWindow(w) = window;
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeBmProcWithoutDrawing(Widget w,XtWidgetProc proc)
{
	Widget shell;
	Window window;

	assert( _XfeIsAlive(w) );
	assert( proc != NULL );

	/*
	 * We pretend that the closest shell's window is the button's.  This is 
	 * a hack so that the widget procedure 'proc' does not draw anything.
	 */
	shell = XfeAncestorFindByClass(w,shellWidgetClass,XfeFIND_ANY);

	window = _XfeWindow(w);

	/*
	 * We change the width to 0, so that the real Enter() 
	 * action does not draw anything in the cascade button so that
	 * we can draw the accent lines.
	 */
	_XfeWindow(w) = _XfeWindow(shell);
		
	(*proc)(w);
		
	_XfeWindow(w) = window;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/*extern*/ Widget
XfeCreateBmButton(Widget pw,char * name,Arg * av,Cardinal ac)
{
    return XtCreateWidget(name,xfeBmButtonWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Accent enabled/disabled functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean _accent_is_enabled = False;

/* extern */ Boolean
XfeBmAccentIsEnabled(void)
{
	return _accent_is_enabled;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentDisable(void)
{
	_accent_is_enabled = False;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentEnable(void)
{
	_accent_is_enabled = True;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
static Dimension	_accent_offset_left			= 4;
static Dimension	_accent_offset_right		= 8;
static Dimension	_accent_shadow_thickness	= 1;
static Dimension	_accent_thickness			= 1;

/* extern */ void
XfeBmAccentSetOffsetLeft(Dimension offset_left)
{
	_accent_offset_left = offset_left;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentSetOffsetRight(Dimension offset_right)
{
	_accent_offset_right = offset_right;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentSetShadowThickness(Dimension shadow_thickness)
{
	_accent_shadow_thickness = shadow_thickness;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentSetThickness(Dimension thickness)
{
	_accent_thickness = thickness;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetOffsetLeft(void)
{
	return _accent_offset_left;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetOffsetRight(void)
{
	return _accent_offset_right;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetShadowThickness(void)
{
	return _accent_shadow_thickness;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetThickness(void)
{
	return _accent_thickness;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent filing mode.						*/
/*																		*/
/*----------------------------------------------------------------------*/
static XfeAccentFileMode	_accent_file_mode = XmACCENT_FILE_ANYWHERE;

/* extern */ void
XfeBmAccentSetFileMode(XfeAccentFileMode file_mode)
{
	_accent_file_mode = file_mode;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeAccentFileMode
XfeBmAccentGetFileMode(void)
{
	return _accent_file_mode;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade pixmap offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
static Dimension	_pixmap_offset = 4;

/* extern */ void
XfeBmPixmapSetOffset(Dimension offset)
{
	_pixmap_offset = offset;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmPixmapGetOffset(void)
{
	return _pixmap_offset;
}
/*----------------------------------------------------------------------*/
