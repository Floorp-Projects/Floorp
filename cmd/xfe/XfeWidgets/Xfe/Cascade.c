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
/* Name:		<Xfe/Cascade.c>											*/
/* Description:	XfeCascade widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/CascadeP.h>
#include <Xfe/ManagerP.h>

#include <Xm/RowColumnP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeButton."
#define MESSAGE2 "XmNsubMenuId is a read-only resource."
#define MESSAGE3 "XmNpoppedUp is a read-only resource."
#define MESSAGE4 "XmNtorn is a read-only resource."

#define SUB_MENU_ID_NAME		"PopupMenu"

/*
 * In order to get popup menus working as if they were pulldown menus,
 * we need to trick motif into ignoring the event up until the point when
 * we know it is ok to pop it up.  This will happen either right away,
 * or after the mapping delay timeout expires (based on the value of 
 * XmNmappingDelay). We accomplish this by swapping these two values for
 * the popup's XmNmenuPost resource.  This scheme seems pretty robust.  If 
 * a user happens to have a 7 button mouse and they hit Button7 on the 
 * widget, then a (harmless ?) passive grab will occur.  We make sure the
 * grab does not screw the user by installing a ButtonPress event handler
 * on the widget and ungrabing the pointer just in case.
 */
#define POPUP_IGNORE			"<Btn3Down>"
#define POPUP_ACCEPT			"<Btn1Down>"

/* Tear off model for cascade */
#define TEAR_MODEL(cp) \
((cp)->allow_tear_off ? XmTEAR_OFF_ENABLED : XmTEAR_OFF_DISABLED)

/* Popup shell */
#define POPUP_SHELL(cp) \
(XtParent((cp)->sub_menu_id))

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
/* XfePrimitive class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	PreferredGeometry	(Widget,Dimension *,Dimension *);
static void	DrawComponents		(Widget,XEvent *,Region,XRectangle *);
static void	LayoutComponents	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Arm				(Widget,XEvent *,char **,Cardinal *);
static void 	Activate		(Widget,XEvent *,char **,Cardinal *);
static void 	Disarm			(Widget,XEvent *,char **,Cardinal *);
static void 	Post			(Widget,XEvent *,char **,Cardinal *);
static void 	Unpost			(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeCascade functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DelayTimeout		(XtPointer,XtIntervalId *);
static void		DrawCascadeArrow	(Widget,XEvent *,Region,XRectangle *);
static void		LayoutCascadeArrow	(Widget);
static void		GetPostPosition		(Widget,Position *,Position *);
static void		InvokeTearCallback	(Widget,XEvent *,Boolean);

/*----------------------------------------------------------------------*/
/*																		*/
/* SubMenu event handlers and callbacks									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	SubMenuEH				(Widget,XtPointer,XEvent *,Boolean *);
static void	SubMenuTearCB			(Widget,XtPointer,XtPointer);
static void	UnGrabEH				(Widget,XtPointer,XEvent *,Boolean *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade Resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
    /* Callback resources */     	
    { 
		XmNcascadingCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeCascadeRec,xfe_cascade . cascading_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },

    { 
		XmNpopdownCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeCascadeRec,xfe_cascade . popdown_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNpopupCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeCascadeRec,xfe_cascade . popup_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNsubmenuTearCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeCascadeRec,xfe_cascade . submenu_tear_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },

    /* Sub menu resources */
    { 
		XmNmappingDelay,
		XmCMappingDelay,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . mapping_delay),
		XmRImmediate, 
 		(XtPointer) XfeDEFAULT_MAPPING_DELAY
    },
    { 
		XmNsubMenuId,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . sub_menu_id),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNsubMenuAlignment,
		XmCSubMenuAlignment,
		XmRAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . sub_menu_alignment),
		XmRImmediate, 
		(XtPointer) XmALIGNMENT_BEGINNING
    },
    { 
		XmNsubMenuLocation,
		XmCSubMenuLocation,
		XmRLocationType,
		sizeof(unsigned char),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . sub_menu_location),
		XmRImmediate, 
		(XtPointer) XmLOCATION_SOUTH
    },
    { 
		XmNpoppedUp,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . popped_up),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNmatchSubMenuWidth,
		XmCMatchSubMenuWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . match_sub_menu_width),
		XmRImmediate, 
		(XtPointer) False
    },

	
	/* Tear resources */
    { 
		XmNtorn,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . torn),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNallowTearOff,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . allow_tear_off),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNtornShellTitle,
		XmCTornShellTitle,
		XmRString,
		sizeof(String),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . torn_shell_title),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Cascade arrow resources */
    { 
		XmNcascadeArrowDirection,
		XmCCascadeArrowDirection,
		XmRArrowDirection,
		sizeof(unsigned char),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . cascade_arrow_direction),
		XmRImmediate, 
		(XtPointer) XmARROW_DOWN
    },
    { 
		XmNcascadeArrowLocation,
		XmCCascadeArrowLocation,
		XmRLocationType,
		sizeof(unsigned char),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . cascade_arrow_location),
		XmRImmediate, 
		(XtPointer) XmLOCATION_SOUTH_WEST
    },
    { 
		XmNcascadeArrowHeight,
		XmCCascadeArrowHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . cascade_arrow_rect . height),
		XmRImmediate, 
		(XtPointer) 3
    },
    { 
		XmNcascadeArrowWidth,
		XmCCascadeArrowWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . cascade_arrow_rect . width),
		XmRImmediate, 
		(XtPointer) 5
    },
    { 
		XmNdrawCascadeArrow,
		XmCDrawCascadeArrow,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . draw_cascade_arrow),
		XmRImmediate, 
		(XtPointer) False
    },

	/* Sub menu resources */
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
    {
		XmNcascadeArrowHeight,
		sizeof(Dimension),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . cascade_arrow_rect . height),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
    },
	{
		XmNcascadeArrowWidth,
		sizeof(Dimension),
		XtOffsetOf(XfeCascadeRec , xfe_cascade . cascade_arrow_rect . width),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "Arm",				Arm				},
    { "Activate",			Activate		},
    { "Disarm",				Disarm			},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade widget class record initialization						*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(cascade,Cascade) =
{
    {
		/* Core Part */
		(WidgetClass) &xfeButtonClassRec,		/* superclass         	*/
		"XfeCascade",							/* class_name         	*/
		sizeof(XfeCascadeRec),					/* widget_size        	*/
		NULL,									/* class_initialize   	*/
		NULL,									/* class_part_initialize*/
		FALSE,                                  /* class_inited       	*/
		Initialize,                             /* initialize         	*/
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
		Destroy,                                /* destroy            	*/
		XtInheritResize,						/* resize             	*/
		XtInheritExpose,						/* expose             	*/
		SetValues,                              /* set_values         	*/
		NULL,                                   /* set_values_hook    	*/
		XtInheritSetValuesAlmost,				/* set_values_almost  	*/
		NULL,									/* get_values_hook		*/
		NULL,                                   /* accept_focus       	*/
		XtVersion,                              /* version            	*/
		NULL,                                   /* callback_private   	*/
		XtInheritTranslations,					/* tm_table           	*/
		XtInheritQueryGeometry,					/* query_geometry     	*/
		XtInheritDisplayAccelerator,            /* display accel      	*/
		NULL,                                   /* extension          	*/
    },

    /* XmPrimitive Part */
    {
		XmInheritBorderHighlight,				/* border_highlight		*/
		XmInheritBorderUnhighlight,				/* border_unhighlight 	*/
		XtInheritTranslations,                  /* translations       	*/
		XmInheritArmAndActivate,				/* arm_and_activate   	*/
		syn_resources,							/* syn resources      	*/
		XtNumber(syn_resources),				/* num syn_resources  	*/
		NULL,									/* extension          	*/
    },
	
    /* XfePrimitive Part */
    {
		XfeInheritBitGravity,					/* bit_gravity			*/
		PreferredGeometry,						/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		XfeInheritDrawBackground,				/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		NULL,									/* extension            */
    },

    /* XfeLabel Part */
    {
		XfeInheritLayoutString,					/* layout_string		*/
		XfeInheritDrawString,					/* draw_string			*/
		XfeInheritDrawSelection,				/* draw_selection		*/
		XfeInheritGetLabelGC,					/* get_label_gc			*/
		XfeInheritGetSelectionGC,				/* get_selection_gc		*/
		NULL,									/* extension            */
    },

    /* XfeButton Part */
    {
		XfeInheritLayoutPixmap,					/* layout_pixmap		*/
		XfeInheritDrawPixmap,					/* draw_pixmap			*/
		XfeInheritDrawRaiseBorder,				/* draw_raise_border	*/
		XfeInheritArmTimeout,					/* arm_timeout			*/
		NULL,									/* extension            */
    },

    /* XfeCascade Part */
    {
		NULL,									/* extension            */
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeCascadeWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(cascade,Cascade);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeCascadePart *	cp = _XfeCascadePart(nw);
	Arg					xargs[3];
	Cardinal			n;

    /* Make sure rep types are ok */
    XfeRepTypeCheck(nw,XmRLocationType,&cp->cascade_arrow_location,
					XmLOCATION_SOUTH_WEST);

    XfeRepTypeCheck(nw,XmRArrowDirection,&cp->cascade_arrow_direction,
					XmARROW_DOWN);

    XfeRepTypeCheck(nw,XmRAlignment,&cp->sub_menu_alignment,
					XmALIGNMENT_BEGINNING);

    XfeRepTypeCheck(nw,XmRLocationType,&cp->sub_menu_location,
					XmLOCATION_SOUTH);

	/* Create popup using our probably non-standard visual/colormap/depth */
	n = 0;

	XtSetArg(xargs[n],XmNcolormap,	XfeColormap(nw));	n++;
	XtSetArg(xargs[n],XmNdepth,		XfeDepth(nw));		n++;
	XtSetArg(xargs[n],XmNvisual,	XfeVisual(nw));		n++;

	cp->sub_menu_id = XmCreatePopupMenu(nw,SUB_MENU_ID_NAME,xargs,n);

	XtVaSetValues(cp->sub_menu_id,XmNmenuPost,POPUP_IGNORE,NULL);

	/* Add ungrab event handler to cascade button */
	XtInsertEventHandler(nw,
						 ButtonPressMask,
						 True,
						 UnGrabEH,
						 (XtPointer) nw,
						 XtListTail);

	/* Add map/unmap event handler to sub menu's parent shell */
	XtAddEventHandler(POPUP_SHELL(cp),
					  StructureNotifyMask,
					  True,
					  SubMenuEH,
					  (XtPointer) nw);

	/* Add tear callbacks top submenu */
	XtAddCallback(cp->sub_menu_id,
				  XmNtearOffMenuActivateCallback,
				  SubMenuTearCB,
				  (XtPointer) nw);

	XtAddCallback(cp->sub_menu_id,
				  XmNtearOffMenuDeactivateCallback,
				  SubMenuTearCB,
				  (XtPointer) nw);

	/* Initialize private members */
	cp->delay_timer_id		= 0;
	cp->default_menu_cursor	= XmGetMenuCursor(XtDisplay(nw));

	/* Update the tear model */
	XtVaSetValues(cp->sub_menu_id,XmNtearOffModel,TEAR_MODEL(cp),NULL);

    /* Update the torn shell title */
	if (cp->torn_shell_title)
	{
		cp->torn_shell_title = (String) XtNewString(cp->torn_shell_title);
	}
	else
	{
		cp->torn_shell_title = (String) XtNewString(XtName(nw));
	}

	XtVaSetValues(POPUP_SHELL(cp),XmNtitle,cp->torn_shell_title,NULL);

    /* Finish of initialization */
    _XfePrimitiveChainInitialize(rw,nw,xfeCascadeWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	/* Free the torn shell title if needed */
	if (cp->torn_shell_title)
	{
		XtFree(cp->torn_shell_title);
	}

    /* Remove all CallBacks */
    /* XtRemoveAllCallbacks(w,XmNpopdownCallback); */
    /* XtRemoveAllCallbacks(w,XmNpopupCallback); */
    /* XtRemoveAllCallbacks(w,XmNcascadingCallback); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeCascadePart *	np = _XfeCascadePart(nw);
    XfeCascadePart *	op = _XfeCascadePart(ow);

    /* sub_menu_id */
    if (np->sub_menu_id != op->sub_menu_id)
    {
		np->sub_menu_id = op->sub_menu_id;

		_XfeWarning(nw,MESSAGE2);
    }

    /* popped_up */
    if (np->popped_up != op->popped_up)
    {
		np->popped_up = op->popped_up;

		_XfeWarning(nw,MESSAGE3);
    }

    /* torn */
    if (np->torn != op->torn)
    {
		np->torn = op->torn;

		_XfeWarning(nw,MESSAGE4);
    }

    /* allow_tear_off */
    if (np->allow_tear_off != op->allow_tear_off)
    {
		XtVaSetValues(np->sub_menu_id,XmNtearOffModel,TEAR_MODEL(np),NULL);
	}

    /* torn_shell_title */
    if (np->torn_shell_title != op->torn_shell_title)
	{
		if (op->torn_shell_title)
		{
			XtFree(op->torn_shell_title);
		}

		if (np->torn_shell_title)
		{
			np->torn_shell_title = (String) XtNewString(np->torn_shell_title);
		}
		else
		{
			np->torn_shell_title = (String) XtNewString(" ");
		}

		XtVaSetValues(POPUP_SHELL(np),XmNtitle,np->torn_shell_title,NULL);
	}

    /* sub_menu_alignment */
    if (np->sub_menu_alignment != op->sub_menu_alignment)
    {
		/* Make sure the new sub menu alignment is ok */
		XfeRepTypeCheck(nw,XmRAlignment,&np->sub_menu_alignment,
						XmALIGNMENT_BEGINNING);

		_XfeConfigFlags(nw) |= XfeConfigExpose;   
    }

    /* sub_menu_location */
    if (np->sub_menu_location != op->sub_menu_location)
    {
		/* Make sure the new sub menu location is ok */
		XfeRepTypeCheck(nw,XmRLocationType,&np->sub_menu_location,
						XmLOCATION_SOUTH);
    }

    /* cascade_arrow_location */
    if (np->cascade_arrow_location != op->cascade_arrow_location)
    {
		/* Make sure the new cascade arrow location is ok */
		XfeRepTypeCheck(nw,XmRLocationType,&np->cascade_arrow_location,
						XmLOCATION_SOUTH_WEST);
    }


    return _XfePrimitiveChainSetValues(ow,rw,nw,xfeCascadeWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
    XfeCascadePart *		cp = _XfeCascadePart(w);
    XfeButtonWidgetClass	bwc = (XfeButtonWidgetClass) xfeButtonWidgetClass;

	/* */
    (*bwc->xfe_primitive_class.preferred_geometry)(w,width,height);

	/* If the sub menu has children, use its width if needed */
	if (_XfeIsAlive(cp->sub_menu_id) && 
		_XfemNumChildren(cp->sub_menu_id) &&
		cp->match_sub_menu_width)
	{
		*width = _XfeWidth(cp->sub_menu_id);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
/*----------------------------------------------------------------------*/
{
	/* Invoke layout_string method */
	_XfeLabelLayoutString(w);

	/* Invoke layout_pixmap method */
	_XfeButtonLayoutPixmap(w);

	/* Layout the cascade arrow */
	LayoutCascadeArrow(w);
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
	/* Invoke draw_string method */
	_XfeLabelDrawString(w,event,region,clip_rect);

	/* Invoke draw_pixmap method */
	_XfeButtonDrawPixmap(w,event,region,clip_rect);

	/* Invoke draw_border method */
	_XfeButtonDrawRaiseBorder(w,event,region,clip_rect);

	/* Draw Cascade Arrow */
	DrawCascadeArrow(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeCascade functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DelayTimeout(XtPointer client_data,XtIntervalId * id)
{
	Widget					w = (Widget) client_data;
    XfeCascadePart *		cp = _XfeCascadePart(w);
    XfeButtonPart *			bp = _XfeButtonPart(w);

	/* If we are still armed and the pointer remains in the button */
	if (bp->armed && _XfePointerInside(w))
	{
		Post(w,NULL,NULL,NULL);
	}

	/* Reset the timer */
	cp->delay_timer_id = 0;
}
/*----------------------------------------------------------------------*/
static void
DrawCascadeArrow(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
    XfeCascadePart *		cp = _XfeCascadePart(w);
    XfeButtonPart *			bp = _XfeButtonPart(w);
	Dimension				offset;

	/* Make sure the cascade arrow is actually needed before drawing it */
	if (!cp->draw_cascade_arrow || 
		!cp->cascade_arrow_rect.height || 
		!cp->cascade_arrow_rect.width)
	{
		return;
	}

	/* Compute the arming offset */
	offset = bp->armed ? bp->arm_offset : 0;

	/* Draw the arrow as an XmArrowButton would */
	if (bp->emulate_motif)
	{
		XfeDrawMotifArrow(XtDisplay(w),
						  _XfePrimitiveDrawable(w), 
						  _XfeTopShadowGC(w),
						  _XfeTopShadowGC(w),
						  _XfeBackgroundGC(w),
						  cp->cascade_arrow_rect.x + offset,
						  cp->cascade_arrow_rect.y + offset, 
						  cp->cascade_arrow_rect.width,
						  cp->cascade_arrow_rect.height,
						  cp->cascade_arrow_direction,
						  2, /* hard coded to be compatible with Motif */
						  bp->armed);
	}
	/* Draw the arrow as an XfeButton would */
	else
	{
		XfeDrawArrow(XtDisplay(w),
					 _XfePrimitiveDrawable(w),
					 _XfeLabelGetLabelGC(w),
					 cp->cascade_arrow_rect.x + offset,
					 cp->cascade_arrow_rect.y + offset,
					 cp->cascade_arrow_rect.width,
					 cp->cascade_arrow_rect.height,
					 cp->cascade_arrow_direction);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutCascadeArrow(Widget w)
{
    XfeCascadePart *		cp = _XfeCascadePart(w);
    XfeButtonPart *			bp = _XfeButtonPart(w);
	XRectangle				bound;

	/* Make sure the cascade arrow has some dimensions */
	if (!cp->cascade_arrow_rect.height || !cp->cascade_arrow_rect.width)
	{
		return;
	}

	/* If the button layout has a pixmap, place the arrow there */
	if ((bp->button_layout != XmBUTTON_LABEL_ONLY) &&
		bp->pixmap_rect.width && bp->pixmap_rect.height)
    {
		XfeRectSet(&bound,
				   bp->pixmap_rect.x,
				   bp->pixmap_rect.y,
				   bp->pixmap_rect.width,
				   bp->pixmap_rect.height);
    }
	else
	{
		XfeRectSet(&bound,
				   _XfeRectX(w),
				   _XfeRectY(w),
				   _XfeRectWidth(w),
				   _XfeRectHeight(w));
	}

	/* Determine the horizontal layout of the arrow */
	switch(cp->cascade_arrow_location)
	{

	case XmLOCATION_NORTH_EAST:
	case XmLOCATION_SOUTH_EAST:
	case XmLOCATION_EAST:

		cp->cascade_arrow_rect.x = bound.x - cp->cascade_arrow_rect.width;

		break;

	case XmLOCATION_NORTH:
	case XmLOCATION_SOUTH:

		cp->cascade_arrow_rect.x = 
			(bound.width - cp->cascade_arrow_rect.width) / 2 +
			bound.x;

		break;

	case XmLOCATION_SOUTH_WEST:
	case XmLOCATION_WEST:
	case XmLOCATION_NORTH_WEST:

		cp->cascade_arrow_rect.x = 
			bound.width - cp->cascade_arrow_rect.width +
			bound.x;

		break;

	}

	/* Determine the vertical layout of the arrow */
	switch(cp->cascade_arrow_location)
	{
	case XmLOCATION_NORTH:
	case XmLOCATION_NORTH_EAST:
	case XmLOCATION_NORTH_WEST:

		cp->cascade_arrow_rect.y = 
			bound.y;

		break;

	case XmLOCATION_EAST:
	case XmLOCATION_WEST:

		cp->cascade_arrow_rect.y = 
			(bound.height - cp->cascade_arrow_rect.height) / 2 +
			bound.y;

		break;

	case XmLOCATION_SOUTH:
	case XmLOCATION_SOUTH_EAST:
	case XmLOCATION_SOUTH_WEST:

		cp->cascade_arrow_rect.y = 
			bound.height - cp->cascade_arrow_rect.height +
			bound.y;

		break;

	}
}
/*----------------------------------------------------------------------*/
static void
GetPostPosition(Widget w,Position * x_out,Position * y_out)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);
    XfeButtonPart *		bp = _XfeButtonPart(w);
	Dimension			deco_offset;
	Position			x;
	Position			y;
	Position			root_x = XfeRootX(w);
	Position			root_y = XfeRootY(w);

	/* Position the menu below the button (like pulldown menus do) */
	deco_offset = _XfeHighlightThickness(w);
	
	if (bp->raise_on_enter)
	{
		deco_offset += bp->raise_border_thickness;
	}

	switch(cp->sub_menu_alignment)
	{
	case XmALIGNMENT_BEGINNING:

		x = root_x + deco_offset;

		break;
		
	case XmALIGNMENT_CENTER:

		x = root_x + (_XfeWidth(w) - _XfeWidth(cp->sub_menu_id)) / 2;

		break;

	case XmALIGNMENT_END:

		x = root_x + _XfeWidth(w) - _XfeWidth(cp->sub_menu_id) - deco_offset;
			
		break;
	}
	
	y = root_y + _XfeHeight(w) - deco_offset;

	if (x_out)
	{
		*x_out = x;
	}

	if (y_out)
	{
		*y_out = y;
	}
}
/*----------------------------------------------------------------------*/
static void
InvokeTearCallback(Widget w,XEvent * event,Boolean torn)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

    /* Invoke the callbacks only if needed */
	if (cp->submenu_tear_callback)
    {
		XfeSubmenuTearCallbackStruct cbs;
		
		cbs.event	= event;
		cbs.reason	= XmCR_SUBMENU_TEAR;
		cbs.torn	= torn;
		
		/* Flush the display */
		XFlush(XtDisplay(w));
		
		/* Invoke the Callback List */
		XtCallCallbackList(w,cp->submenu_tear_callback,&cbs);
    }
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Arm(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	/* Make sure we are not insensitive (or pretending to be insensitive) */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	/* Make sure the submenu is not already posted */
	if (cp->popped_up)
	{
		return;
	}

	/* First, arm the button */
	_XfeButtonArm(w,event,params,nparams);
	
	/* Invoke the cascading callback so that client can do magic */
	_XfeInvokeCallbacks(w,cp->cascading_callback,XmCR_CASCADING,event,True);

	/* make sure the submenu has some children before posting */
	if (!_XfemNumChildren(cp->sub_menu_id))
	{
		return;
	}

	/* Make sure the submenu is not torn */
	if (cp->torn)
	{
		return;
	}

	/* Make sure no other timer is active.  If there is, remove it */
	if (cp->delay_timer_id)
	{
		XtRemoveTimeOut(cp->delay_timer_id);
		
		cp->delay_timer_id = 0;
	}

	/* Install the mapping delay timer if needed */
	if (cp->mapping_delay)
	{
		XtVaSetValues(cp->sub_menu_id,XmNmenuPost,POPUP_ACCEPT,NULL);
	
		cp->delay_timer_id = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
											 cp->mapping_delay,
											 DelayTimeout,
											 (XtPointer) w);
	}
	/* If no mapping delay is given, post the submenu right away */
	else
	{
		XtVaSetValues(cp->sub_menu_id,XmNmenuPost,POPUP_ACCEPT,NULL);
		Post(w,event,NULL,0);
	}
}
/*----------------------------------------------------------------------*/
static void
Activate(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	XtVaSetValues(cp->sub_menu_id,XmNmenuPost,POPUP_IGNORE,NULL);

	_XfeButtonActivate(w,event,params,nparams);
}
/*----------------------------------------------------------------------*/
static void
Disarm(Widget w,XEvent *event,char **params,Cardinal *nparams)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	/* Make sure we are not insensitive (or pretending to be insensitive) */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	_XfeButtonDisarm(w,event,params,nparams);

	XtVaSetValues(cp->sub_menu_id,XmNmenuPost,POPUP_IGNORE,NULL);
}
/*----------------------------------------------------------------------*/
static void
Post(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	if (_XfeIsAlive(POPUP_SHELL(cp)) && _XfeIsAlive(cp->sub_menu_id))
	{
		XEvent *		the_event = event;
		Position		x;
		Position		y;

		if (!the_event)
		{
			XButtonEvent	be;

			/*
			 * Hackery Hackery Hackery
			 *
			 * This crap occurs so that the menu can be posted programatically
			 * in any context and not only button press which the motif popup
			 * menus seem to expect.  I dont think all the given fields are
			 * needed, but they are ther anyway.
			 */
			be.type			= ButtonPress;
			be.serial		= 0;
			be.send_event	= 0;
			be.display		= XtDisplay(w);
 			be.window		= _XfeWindow(w);
			be.root			= DefaultRootWindow(XtDisplay(w));
			be.subwindow	= None;
 			be.time			= CurrentTime;
 			be.state		= AnyModifier;
 			be.button		= Button1;
			be.same_screen	= True;

			the_event = (XEvent *) &be;
		}

		assert( the_event != NULL );

		/*
		 * Force the modifiers to any.  The motif row column - and thus 
		 * popup menus, will not post correctly if either the ShiftMask
		 * or LockMask bit is set in the button event modifier state.
		 */
		if (the_event->type == ButtonPress)
		{
			the_event->xbutton.state = AnyModifier;
		}

		/* Find out where the sub menu is supposed to post */
		GetPostPosition(w,&x,&y);

		/* Position the sub menu */
		XfeMenuPositionXY(cp->sub_menu_id,x,y);

		/* Make sure the submenu is realized */
		if (!XtIsRealized(cp->sub_menu_id))
		{
			XtRealizeWidget(cp->sub_menu_id);
		}

		/* Synchronize and flush the display before popping up the submenu */
		XSync(XtDisplay(w),False);

		XmUpdateDisplay(w);

		/* Post the button */
		_XmPostPopupMenu(cp->sub_menu_id,the_event);
	}
}
/*----------------------------------------------------------------------*/
static void
Unpost(Widget w,XEvent *event,char **params,Cardinal *nparams)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	if (_XfeIsAlive(XtParent(cp->sub_menu_id)) && _XfeIsAlive(cp->sub_menu_id))
	{
		XtUnmanageChild(cp->sub_menu_id);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* SubMenu event handlers and callbacks									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
SubMenuEH(Widget		shell,
		  XtPointer		client_data,
		  XEvent *		event,
		  Boolean *		cont)
{
	Widget				w = (Widget) client_data;
    XfeCascadePart *	cp = _XfeCascadePart(w);

	switch (event->type) 
	{
	case MapNotify:

		XtVaSetValues(cp->sub_menu_id,XmNmenuPost,POPUP_IGNORE,NULL);

		/* Submenu is now popped up */
		cp->popped_up = True;

		/* Invoke the popup callbacks */
		_XfeInvokeCallbacks(w,cp->popup_callback,XmCR_POPUP,event,True);

		break;

	case UnmapNotify:

		Disarm(w,NULL,NULL,0);
		
		XtVaSetValues(cp->sub_menu_id,XmNmenuPost,POPUP_IGNORE,NULL);

		/* Submenu is now popped down */
		cp->popped_up = False;

		/* Invoke the popdown callbacks */
		_XfeInvokeCallbacks(w,cp->popdown_callback,XmCR_POPDOWN,event,True);

		break;
	}
}
/*----------------------------------------------------------------------*/
static void
SubMenuTearCB(Widget menu,XtPointer client_data,XtPointer call_data)
{
	XmRowColumnCallbackStruct * cbs = (XmRowColumnCallbackStruct *) call_data;
	Widget						w = (Widget) client_data;
    XfeCascadePart *			cp = _XfeCascadePart(w);

	if (cbs->reason == XmCR_TEAR_OFF_ACTIVATE)
	{
		cp->torn = True;
	}
	else if (cbs->reason == XmCR_TEAR_OFF_DEACTIVATE)
	{
		cp->torn = False;
	}

	InvokeTearCallback(w,cbs->event,cp->torn);
}
/*----------------------------------------------------------------------*/
static void
UnGrabEH(Widget			shell,
		 XtPointer		client_data,
		 XEvent *		event,
		 Boolean *		cont)
{
	Widget				w = (Widget) client_data;
    XfeCascadePart *	cp = _XfeCascadePart(w);
	
	if ((event->type == ButtonPress) && (event->xbutton.button >= Button2))
	{
		XtUngrabPointer(cp->sub_menu_id,CurrentTime);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateCascade(Widget parent,char *name,Arg *args,Cardinal count)
{
    return (XtCreateWidget(name,xfeCascadeWidgetClass,parent,args,count));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeCascadeDestroyChildren(Widget w)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsCascade(w) );
	assert( _XfeIsAlive(cp->sub_menu_id) );

	XfeChildrenDestroy(cp->sub_menu_id);
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeCascadeArmAndPost(Widget w,XEvent *	event)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsCascade(w) );
	assert( _XfeIsAlive(cp->sub_menu_id) );

	/* Make sure the subment is not already popped up or torn */
	if (!cp->popped_up && 
		!cp->torn && 
		_XfeIsAlive(POPUP_SHELL(cp)) && 
		_XfeIsAlive(cp->sub_menu_id))
	{
		Arm(w,event,NULL,0);
		Post(w,event,NULL,0);

		return True;
	}


	return False;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeCascadeDisarmAndUnpost(Widget w,XEvent *	event)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsCascade(w) );
	assert( _XfeIsAlive(cp->sub_menu_id) );

	/* Make sure the subment is not already popped up or torn */
	if (cp->popped_up)
	{
		Disarm(w,event,NULL,0);
		Unpost(w,event,NULL,0);
		
		return True;
	}
	
	return False;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeCascadeGetChildren(Widget w,WidgetList * children,Cardinal * num_children)
{
    XfeCascadePart *	cp = _XfeCascadePart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsCascade(w) );
	assert( _XfeIsAlive(cp->sub_menu_id) );
	assert( children != NULL || num_children != NULL );


	if (_XfeIsAlive(w) && _XfeIsAlive(cp->sub_menu_id))
	{
		*children = _XfemChildren(cp->sub_menu_id);
		*num_children = _XfemNumChildren(cp->sub_menu_id);
	}
	else
	{
		*children = NULL;
		*num_children = 0;
	}
}
/*----------------------------------------------------------------------*/
