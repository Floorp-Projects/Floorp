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
/* Name:		<Xfe/Button.c>											*/
/* Description:	XfeButton widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/ButtonP.h>
#include <Xfe/ManagerP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeButton."


#define MESSAGE2  "XmNpixmap needs to have the same depth as the XfeButton."
#define MESSAGE3  "Cannot obtain geometry info for XmNpixmap."
#define MESSAGE4  "Cannot obtain geometry info for XmNarmedPixmap."
#define MESSAGE5  "Cannot obtain geometry info for XmNraisedPixmap."
#define MESSAGE6  "Cannot obtain geometry info for XmNinsensitivePixmap."
#define MESSAGE7  "Cannot use XmNarmedPixmap since XmNpixmap is bad."
#define MESSAGE8  "Cannot use XmNraisedPixmap since XmNpixmap is bad."
#define MESSAGE9  "Cannot use XmNinsensitivePixmap since XmNpixmap is bad."
#define MESSAGE10 "XmNarmedPixmap dimensions do not match XmNpixmap."
#define MESSAGE11 "XmNraisedPixmap dimensions do not match XmNpixmap."
#define MESSAGE12 "XmNinsensitivePixmap dimensions do not match XmNpixmap."
#define MESSAGE13 "XmNarmedPixmap needs to have same depth as the XfeButton."
#define MESSAGE14 "XmNraisedPixmap needs to have same depth as the XfeButton."
#define MESSAGE15 "XmNinsensitivePixmap needs to have same depth as the XfeButton."
#define MESSAGE16 "XfeButtonPreferredGeometry: The given layout is invalid."
#define MESSAGE17 "XmNarmed can only be set for XmBUTTON_TOGGLE XmNbuttonType."
#define MESSAGE18 "XmNraised can only be set when XmNraiseOnEnter is True."

#define RAISE_OFFSET(bp) \
(bp->raise_on_enter ? bp->raise_border_thickness : 0)

#define XY_IN_LABEL(_lp,_x,_y) XfePointInRect(&(_lp)->label_rect,(_x),(_y))
#define XY_IN_PIXMAP(_bp,_x,_y) XfePointInRect(&(_bp)->pixmap_rect,(_x),(_y))

#define MIN_COMP_WIDTH(_lp,_bp) \
XfeMin((_bp)->pixmap_rect.width,(_lp)->label_rect.width)

#define MIN_COMP_HEIGHT(_lp,_bp) \
XfeMin((bp)->pixmap_rect.height,(_lp)->label_rect.height)

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassPartInit	(WidgetClass);
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	PreferredGeometry	(Widget,Dimension *,Dimension *);
static void	PrepareComponents	(Widget,int);
static void	LayoutComponents	(Widget);
static void	DrawComponents		(Widget,XEvent *,Region,XRectangle *);
static void	DrawBackground		(Widget,XEvent *,Region,XRectangle *);
static void	DrawShadow			(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	LayoutString			(Widget);
static void	DrawString			(Widget,XEvent *,Region,XRectangle *);
static GC	GetLabelGC			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	LayoutPixmap		(Widget);
static void	DrawPixmap			(Widget,XEvent *,Region,XRectangle *);
static void	DrawRaiseBorder		(Widget,XEvent *,Region,XRectangle *);
static void	ArmTimeout			(XtPointer,XtIntervalId *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeButton functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PixmapPrepare				(Widget);
static void		PixmapArmedPrepare			(Widget);
static void		PixmapRaisedPrepare			(Widget);
static void		PixmapInsensPrepare			(Widget);

static void		StringLayoutLabelOnBottom	(Widget);
static void		StringLayoutLabelOnLeft		(Widget);
static void		StringLayoutLabelOnRight	(Widget);
static void		StringLayoutLabelOnTop		(Widget);
static void		StringLayoutLabelOnly		(Widget);

static void		InvokeCallback			(Widget,XtCallbackList,int,XEvent *);

static Boolean	AcceptEvent				(Widget,XEvent *);
static Boolean	SpacingContainsXY		(Widget,int,int);

static void		TransparentCursorSetState		(Widget,Boolean);

/*----------------------------------------------------------------------*/
/*																		*/
/* Resource Callprocs													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	DefaultArmBackground	(Widget,int,XrmValue *);
static void	DefaultRaiseBackground	(Widget,int,XrmValue *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton Resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
    /* Callback resources */     	
    { 
		XmNactivateCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeButtonRec , xfe_button . activate_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNarmCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeButtonRec , xfe_button . arm_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNbutton3DownCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeButtonRec , xfe_button . button_3_down_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNbutton3UpCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeButtonRec , xfe_button . button_3_up_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNdisarmCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeButtonRec , xfe_button . disarm_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    
    /* Label resources */
    { 
		XmNraiseForeground,
		XmCRaiseForeground,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeButtonRec , xfe_button . raise_foreground),
		XmRCallProc, 
		(XtPointer) _XfeCallProcCopyForeground
    },

    /* Pixmap resources */
    { 
		XmNarmedPixmap,
		XmCArmedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . armed_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNinsensitivePixmap,
		XmCInsensitivePixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . insensitive_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNpixmap,
		XmCPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNraisedPixmap,
		XmCRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNarmedPixmapMask,
		XmCArmedPixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . armed_pixmap_mask),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNinsensitivePixmapMask,
		XmCInsensitivePixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . insensitive_pixmap_mask),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNpixmapMask,
		XmCPixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . pixmap_mask),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNraisedPixmapMask,
		XmCRaisedPixmapMask,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeButtonRec , xfe_button . raised_pixmap_mask),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },

    /* Raise resources */
    { 
		XmNfillOnEnter,
		XmCFillOnEnter,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeButtonRec , xfe_button . fill_on_enter),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNraised,
		XmCRaised,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeButtonRec , xfe_button . raised),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNraiseBackground,
		XmCRaiseBackground,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeButtonRec , xfe_button . raise_background),
		XmRCallProc, 
		(XtPointer) DefaultRaiseBackground
    },
    { 
		XmNraiseBorderThickness,
		XmCRaiseBorderThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_button . raise_border_thickness),
		XmRImmediate, 
		(XtPointer) 1
    },
    { 
		XmNraiseOnEnter,
		XmCRaiseOnEnter,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeButtonRec , xfe_button . raise_on_enter),
		XmRImmediate, 
		(XtPointer) True
    },

    /* Arm resources */
    { 
		XmNfillOnArm,
		XmCFillOnArm,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeButtonRec , xfe_button . fill_on_arm),
		XmRImmediate, 
		(XtPointer) True
    },
    { 
		XmNarmBackground,
		XmCArmBackground,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeButtonRec , xfe_button . arm_background),
		XmRCallProc, 
		(XtPointer) DefaultArmBackground
    },
    { 
		XmNarmOffset,
		XmCArmOffset,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_button . arm_offset),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_ARM_OFFSET
    },
    { 
		XmNarmed,
		XmCArmed,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeButtonRec , xfe_button . armed),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNdeterminate,
		XmCDeterminate,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeButtonRec , xfe_button . determinate),
		XmRImmediate, 
		(XtPointer) True
    },


    /* Misc resources */
    { 
		XmNbuttonLayout,
		XmCButtonLayout,
		XmRButtonLayout,
		sizeof(unsigned char),
		XtOffsetOf(XfeButtonRec , xfe_button . button_layout),
		XmRImmediate, 
		(XtPointer) XmBUTTON_LABEL_ON_BOTTOM
    },
    { 
		XmNbuttonTrigger,
		XmCButtonTrigger,
		XmRButtonTrigger,
		sizeof(unsigned char),
		XtOffsetOf(XfeButtonRec , xfe_button . button_trigger),
		XmRImmediate, 
		(XtPointer) XmBUTTON_TRIGGER_ANYWHERE
    },
    { 
		XmNbuttonType,
		XmCButtonType,
		XmRButtonType,
		sizeof(unsigned char),
		XtOffsetOf(XfeButtonRec , xfe_button . button_type),
		XmRImmediate, 
		(XtPointer) XmBUTTON_PUSH
    },
    { 
		XmNemulateMotif,
		XmCEmulateMotif,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeButtonRec , xfe_button . emulate_motif),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_EMULATE_MOTIF
    },
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_button . spacing),
		XmRImmediate, 
		(XtPointer) 4
    },

	/* Cursor resources */
    { 
		XmNtransparentCursor,
		XmCCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeButtonRec , xfe_button . transparent_cursor),
		XmRImmediate, 
		(XtPointer) None
    },
    
    
	/* Force margins to 0 */
	{ 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_primitive . margin_bottom),
		XmRImmediate, 
		(XtPointer) 0
	},
	{ 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_primitive . margin_left),
		XmRImmediate, 
		(XtPointer) 0
	},
	{ 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_primitive . margin_right),
		XmRImmediate, 
		(XtPointer) 0
	},
	{ 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_primitive . margin_top),
		XmRImmediate, 
		(XtPointer) 0
	},

    /* Force buffer type to XmBUFFER_SHARED */
    { 
		XmNbufferType,
		XmCBufferType,
		XmRBufferType,
		sizeof(unsigned char),
		XtOffsetOf(XfeButtonRec , xfe_primitive . buffer_type),
		XmRImmediate, 
		(XtPointer) XmBUFFER_SHARED
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
    { 
		XmNarmOffset,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_button . arm_offset),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNraiseBorderThickness,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_button . raise_border_thickness),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNspacing,
		sizeof(Dimension),
		XtOffsetOf(XfeButtonRec , xfe_button . spacing),
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
    { "Enter",				_XfeButtonEnter				},
    { "Leave",				_XfeButtonLeave				},
    { "Motion",				_XfeButtonMotion			},
    { "Activate",			_XfeButtonActivate			},
    { "Arm",				_XfeButtonArm				},
    { "Disarm",				_XfeButtonDisarm			},
    { "ArmAndActivate",		_XfeButtonArmAndActivate	},
    { "Btn3Down",			_XfeButtonBtn3Down			},
    { "Btn3Up",				_XfeButtonBtn3Up			},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton widget class record initialization							*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(button,Button) =
{
    {
		/* Core Part */
		(WidgetClass) &xfeLabelClassRec,		/* superclass         	*/
		"XfeButton",							/* class_name         	*/
		sizeof(XfeButtonRec),					/* widget_size        	*/
		NULL,									/* class_initialize   	*/
		ClassPartInit,							/* class_part_initialize*/
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
		_XfeButtonDefaultTranslations,			/* tm_table           	*/
		XtInheritQueryGeometry,					/* query_geometry     	*/
		XtInheritDisplayAccelerator,            /* display accel      	*/
		NULL,                                   /* extension          	*/
    },

    /* XmPrimitive Part */
    {
		XmInheritBorderHighlight,				/* border_highlight		*/
		XmInheritBorderUnhighlight,				/* border_unhighlight 	*/
		XtInheritTranslations,                  /* translations       	*/
		_XfeButtonArmAndActivate,				/* arm_and_activate   	*/
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
		PrepareComponents,						/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		DrawBackground,							/* draw_background		*/
		DrawShadow,								/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		NULL,									/* extension            */
    },

    /* XfeLabel Part */
    {
		LayoutString,							/* layout_string		*/
		DrawString,								/* draw_string			*/
		XfeInheritDrawSelection,				/* draw_selection		*/
		GetLabelGC,								/* get_label_gc			*/
		XfeInheritGetSelectionGC,				/* get_selection_gc		*/
		NULL,									/* extension            */
    },

    /* XfeButton Part */
    {
		LayoutPixmap,							/* layout_pixmap		*/
		DrawPixmap,								/* draw_pixmap			*/
		DrawRaiseBorder,						/* draw_raise_border	*/
		ArmTimeout,								/* arm_timeout			*/
		NULL,									/* extension            */
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeButtonWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(button,Button);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton resource call procedures									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DefaultArmBackground(Widget w,int offset,XrmValue * value)
{
    XfeButtonPart *		bp = _XfeButtonPart(w);
    static Pixel		arm_background;

	if (bp->fill_on_arm)
	{
		arm_background = XfeSelectPixel(w,bp->raise_background);
	}
	else
	{
		arm_background = _XfeBackgroundPixel(w);
	}
   
    value->addr = (XPointer) &arm_background;
    value->size = sizeof(arm_background);
}
/*----------------------------------------------------------------------*/
static void
DefaultRaiseBackground(Widget w,int offset,XrmValue * value)
{
    XfeButtonPart *		bp = _XfeButtonPart(w);
    static Pixel		raise_background;

	if (bp->fill_on_enter)
	{
		raise_background = XfeSelectPixel(w,_XfeBackgroundPixel(w));
	}
	else
	{
		raise_background = _XfeBackgroundPixel(w);
	}
   
    value->addr = (XPointer) &raise_background;
    value->size = sizeof(raise_background);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassPartInit(WidgetClass wc)
{
	XfeButtonWidgetClass cc = (XfeButtonWidgetClass) wc;
	XfeButtonWidgetClass sc = (XfeButtonWidgetClass) wc->core_class.superclass;

	/* Resolve inheritance of all XfeButton class methods */
	_XfeResolve(cc,sc,xfe_button_class,layout_pixmap,XfeInheritLayoutPixmap);
	_XfeResolve(cc,sc,xfe_button_class,draw_pixmap,XfeInheritDrawPixmap);

	_XfeResolve(cc,sc,xfe_button_class,draw_raise_border,
				XfeInheritDrawRaiseBorder);

	_XfeResolve(cc,sc,xfe_button_class,arm_timeout,XfeInheritArmTimeout);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeButtonPart * bp = _XfeButtonPart(nw);
    XfeLabelPart *	lp = _XfeLabelPart(nw);

    /* Make sure rep types are ok */
    XfeRepTypeCheck(nw,XmRButtonType,&bp->button_type,
					XmBUTTON_PUSH);
	
    XfeRepTypeCheck(nw,XmRButtonLayout,&bp->button_layout,
					XmBUTTON_LABEL_ON_BOTTOM);
	
    XfeRepTypeCheck(nw,XmRButtonTrigger,&bp->button_trigger,
					XmBUTTON_TRIGGER_ANYWHERE);

    /* Allocate the label raised GCs */
    bp->label_raised_GC = XfeAllocateStringGc(nw,lp->font_list,
											  bp->raise_foreground,
											  None,True);

    /* Allocate the armed GCs */
    bp->armed_GC = XfeAllocateColorGc(nw,bp->arm_background,None,True);
    
    /* Allocate the raised GCs */
    bp->raised_GC = XfeAllocateColorGc(nw,bp->raise_background,None,True);

    /* Allocate the pixmap GC */
    bp->pixmap_GC = XfeAllocateTransparentGc(nw);

    /* Initialize other private members */
    bp->clicking					= False;
	bp->transparent_cursor_state	= False;

	/* Make sure the background gc is allocated if needed */
	if (_XfePixmapGood(bp->insensitive_pixmap))
	{
		_XfePrimitiveAllocateBackgroundGC(nw);
	}

	/* Need to track motion if trigger is not anywhere */
	if (bp->button_trigger != XmBUTTON_TRIGGER_ANYWHERE)
	{
		XfeOverrideTranslations(nw,_XfeButtonMotionAddTranslations);
	}

    /* Finish of initialization */
    _XfePrimitiveChainInitialize(rw,nw,xfeButtonWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeButtonPart * bp = _XfeButtonPart(w);

    /* Release GCs */
    XtReleaseGC(w,bp->armed_GC);
    XtReleaseGC(w,bp->label_raised_GC);
    XtReleaseGC(w,bp->pixmap_GC);
    XtReleaseGC(w,bp->raised_GC);

    /* Remove all CallBacks */
    /* XtRemoveAllCallbacks(w,XmNactivateCallback); */
    /* XtRemoveAllCallbacks(w,XmNarmCallback); */
    /* XtRemoveAllCallbacks(w,XmNdisarmCallback); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeButtonPart *	np = _XfeButtonPart(nw);
    XfeButtonPart *	op = _XfeButtonPart(ow);
    XfeLabelPart *	nlp = _XfeLabelPart(nw);
    Boolean			label_raised_gc_flag = False;
    Boolean			armed_gc_flag = False;
    Boolean			raised_gc_flag = False;

    /* button_type */
    if (np->button_type != op->button_type)
    {
		/* Make sure the new button type is ok */
		XfeRepTypeCheck(nw,XmRButtonType,&np->button_type,
						XmBUTTON_PUSH);

#if 0
		/* If the new button_type is not toggle, make sure we are not armed */
		if (np->button_type != XmBUTTON_TOGGLE)
		{
			np->armed = False;
		}
#endif

		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* button_layout */
    if (np->button_layout != op->button_layout)
    {
		/* Make sure the new layout type is ok */
		XfeRepTypeCheck(nw,XmRButtonLayout,&np->button_layout,
						XmBUTTON_LABEL_ON_BOTTOM);

		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    /* button_trigger */
    if (np->button_trigger != op->button_trigger)
    {
		/* Make sure the new trigger type is ok */
		XfeRepTypeCheck(nw,XmRButtonTrigger,&np->button_trigger,
						XmBUTTON_TRIGGER_ANYWHERE);

		/* No motion tracking */
		if (np->button_trigger == XmBUTTON_TRIGGER_ANYWHERE)
		{
			XfeOverrideTranslations(nw,_XfeButtonMotionRemoveTranslations);
		}
		/* Yes motion tracking */
		else
		{
			XfeOverrideTranslations(nw,_XfeButtonMotionAddTranslations);
		}
    }

    /* raise_border_thickness */
    if (np->raise_border_thickness != op->raise_border_thickness)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    /* arm_offset */
    if (np->arm_offset != op->arm_offset)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }


    /* background */
    if (_XfeBackgroundPixel(nw) != _XfeBackgroundPixel(ow))
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    if (np->determinate != op->determinate)
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* spacing */
    if (np->spacing != op->spacing)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    /* pixmap */
    if (np->pixmap != op->pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_BUTTON_PIXMAP;
    }

    /* armed_pixmap */
    if (np->armed_pixmap != op->armed_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_BUTTON_ARMED_PIXMAP;
    }

    /* raised_pixmap */
    if (np->raised_pixmap != op->raised_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_BUTTON_RAISED_PIXMAP;
    }

    /* insensitive_pixmap */
    if (np->insensitive_pixmap != op->insensitive_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_BUTTON_INSENSITIVE_PIXMAP;

		/* Make sure the background gc is allocated if needed */
		if (_XfePixmapGood(np->insensitive_pixmap))
		{
			_XfePrimitiveAllocateBackgroundGC(nw);
		}
    }

    /* armed */
    if (np->armed != op->armed)
    {
		if (np->button_type == XmBUTTON_TOGGLE)
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
		else
		{
			_XfeWarning(nw,MESSAGE17);
			
			np->armed = op->armed;
		}
    }

    /* raise_on_enter */
    if (np->raise_on_enter != op->raise_on_enter)
	{
		/* Make sure we are not raised if raise_on_enter changes */
		np->raised = False;

		_XfeConfigFlags(nw) |= XfeConfigExpose;
	}

    /* raised */
    if (np->raised != op->raised)
    {
		if (!np->raise_on_enter)
		{
			_XfeWarning(nw,MESSAGE18);
			
			np->raised = op->raised;
		}
		else
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
	}

	/* sensitive */
    if (_XfeSensitive(nw) != _XfeSensitive(ow))
    {
		/* If button is now insensitive make sure old states are cleared */
		if (!_XfeSensitive(nw))
		{
			np->armed = False;
			np->clicking = False;
			np->raised = False;
		}
		else
		{
			/* Button is now sensitive and pointer is inside */
			if (_XfePointerInside(nw) && np->raise_on_enter)
			{
				/* The button is raised if the pointer is still inside */
				np->raised = True;
			}
		}
    }

	/* pretend_sensitive */
    if (_XfePretendSensitive(nw) != _XfePretendSensitive(ow))
    {
		/* 
		 * If button is now pretending to be insensitive, make sure 
		 * old states are cleared 
		 */
		if (!_XfePretendSensitive(nw))
		{
			np->armed = False;
			np->clicking = False;
			np->raised = False;
		}
		else
		{
			/* Button is now sensitive and pointer is inside */
			if (_XfePointerInside(nw) && np->raise_on_enter)
			{
				/* The button is raised if the pointer is still inside */
				np->raised = True;
			}
		}
    }

    /* raise_foreground */
    if (np->raise_foreground != op->raise_foreground)
    {
		label_raised_gc_flag = True;   

		if (np->raised)
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
    }

    /* fill_on_arm */
    if (np->fill_on_arm != op->fill_on_arm)
    {
		if (np->armed)
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
    }

    /* fill_on_enter */
    if (np->fill_on_enter != op->fill_on_enter)
    {
		if (np->raised)
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
    }

    /* arm_background */
    if (np->arm_background != op->arm_background)
    {
		armed_gc_flag = True;   

		if (np->armed && np->fill_on_arm)
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
    }

    /* raise_background */
    if (np->raise_background != op->raise_background)
    {
		raised_gc_flag = True;   

		if (np->raised && np->fill_on_enter)
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
    }


    /* Update the label raised GC */
    if (label_raised_gc_flag)
    {
		/* Release the old label GC */
		XtReleaseGC(nw,np->label_raised_GC);
	
		/* Allocate the new label GC */
		np->label_raised_GC = XfeAllocateStringGc(nw,nlp->font_list,
												  np->raise_foreground,
												  None,True);
    }

    /* Update the armed GC */
    if (armed_gc_flag)
    {
		/* Release the old armed GC */
		XtReleaseGC(nw,np->armed_GC);
	
		/* Allocate the new armed GC */
		np->armed_GC = XfeAllocateColorGc(nw,np->arm_background,None,True);
    }

    /* Update the raised GC */
    if (raised_gc_flag)
    {
		/* Release the old raised GC */
		XtReleaseGC(nw,np->raised_GC);
		
		/* Allocate the new raised GC */
		np->raised_GC = XfeAllocateColorGc(nw,np->raise_background,None,True);
    }

    return _XfePrimitiveChainSetValues(ow,rw,nw,xfeButtonWidgetClass);
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
    XfeButtonPart * bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

    *width  = _XfeOffsetLeft(w) + _XfeOffsetRight(w);
    *height = _XfeOffsetTop(w)  + _XfeOffsetBottom(w);

	/* Include the arm_offset if needed */
	if (bp->button_type != XmBUTTON_NONE)
	{
		*width  += (2 * bp->arm_offset);
		*height += (2 * bp->arm_offset);
	}

	/* Include the raise_border_thickenss if needed */
	if (bp->raise_on_enter)
	{
		*width  += (2 * bp->raise_border_thickness);
		*height += (2 * bp->raise_border_thickness);
	}
	
    switch(bp->button_layout)
    {
    case XmBUTTON_LABEL_ONLY:
		
		*width  += lp->label_rect.width;
		*height += lp->label_rect.height;
		
		break;
		
    case XmBUTTON_LABEL_ON_BOTTOM:
    case XmBUTTON_LABEL_ON_TOP:
		
		if (bp->pixmap_rect.width && bp->pixmap_rect.height)
		{
			*width  += 
				XfeMax(bp->pixmap_rect.width,lp->label_rect.width);
			
			*height += 
				(bp->pixmap_rect.height + lp->label_rect.height + bp->spacing);
		}
		else
		{
			*width  += lp->label_rect.width;
			*height += lp->label_rect.height;
		}
		
		break;
		
    case XmBUTTON_LABEL_ON_LEFT:
    case XmBUTTON_LABEL_ON_RIGHT:
		
		if (bp->pixmap_rect.width && bp->pixmap_rect.height)
		{
			*height += 
				XfeMax(bp->pixmap_rect.height,lp->label_rect.height);
			
			*width  += 
				(bp->pixmap_rect.width + lp->label_rect.width + bp->spacing);
		}
		else
		{
			*width  += lp->label_rect.width;
			*height += lp->label_rect.height;
		}
		
		
		break;
		
    case XmBUTTON_PIXMAP_ONLY:
		
		*width  += bp->pixmap_rect.width;
		*height += bp->pixmap_rect.height;
		
		break;
    }
}
/*----------------------------------------------------------------------*/
static void
PrepareComponents(Widget w,int flags)
{
    if (flags & _XFE_PREPARE_BUTTON_PIXMAP)
    {
		PixmapPrepare(w);
    }

    if (flags & _XFE_PREPARE_BUTTON_ARMED_PIXMAP)
    {
		PixmapArmedPrepare(w);
    }

    if (flags & _XFE_PREPARE_BUTTON_RAISED_PIXMAP)
    {
		PixmapRaisedPrepare(w);
    }

    if (flags & _XFE_PREPARE_BUTTON_INSENSITIVE_PIXMAP)
    {
		PixmapInsensPrepare(w);
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
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
	/* Invoke draw_selection method */
	_XfeLabelDrawSelection(w,event,region,clip_rect);

	/* Invoke draw_string method */
	_XfeLabelDrawString(w,event,region,clip_rect);

	/* Invoke draw_pixmap method */
	_XfeButtonDrawPixmap(w,event,region,clip_rect);

	/* Invoke draw_raise_border method */
	_XfeButtonDrawRaiseBorder(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/
static void
DrawShadow(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    unsigned char	shadow_type = _XfeShadowType(w);
    Boolean			need_shadow = True;

	if (!_XfeShadowThickness(w))
	{
		return;
	}

	if (bp->raise_on_enter)
	{
		if (bp->armed)
		{
			shadow_type = XmSHADOW_IN;
		}
		else if (bp->raised)
		{
			shadow_type = XmSHADOW_OUT;
		}
		else
		{
			need_shadow = False;
		}
	}
	else
	{
		/* For both toggle and push the shadow depends on the arming state */
		if (bp->button_type != XmBUTTON_NONE)
		{
			shadow_type = bp->armed ? XmSHADOW_IN : XmSHADOW_OUT;
		}
	}

    /* Draw the shadow only if needed */
    if (need_shadow)
    {
		Dimension raise_offset = RAISE_OFFSET(bp);
		
		_XmDrawShadows(XtDisplay(w),

					   _XfePrimitiveDrawable(w),

					   _XfeTopShadowGC(w),_XfeBottomShadowGC(w),

					   _XfeHighlightThickness(w) + raise_offset,

					   _XfeHighlightThickness(w) + raise_offset,

					   _XfeWidth(w) - 
					   2 * _XfeHighlightThickness(w) -
					   2 * raise_offset,

					   _XfeHeight(w) - 
					   2 * _XfeHighlightThickness(w) - 
					   2 * raise_offset,

					   _XfeShadowThickness(w),

					   shadow_type);
    }
}
/*----------------------------------------------------------------------*/
static void
DrawBackground(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

    /* Fill the background if needed */
    if (bp->fill_on_arm && bp->armed)
    {
		XFillRectangle(XtDisplay(w),
					   _XfePrimitiveDrawable(w),
					   bp->armed_GC,
					   0,0,
					   _XfeWidth(w),_XfeHeight(w));
    }
    else if (bp->fill_on_enter && (bp->raised || _XfePointerInside(w)))
    {
		XFillRectangle(XtDisplay(w),
					   _XfePrimitiveDrawable(w),
					   bp->raised_GC,
					   0,0,
					   _XfeWidth(w),_XfeHeight(w));
    }
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel methods														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutString(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

    if ((bp->button_layout == XmBUTTON_LABEL_ONLY) ||
		!bp->pixmap_rect.width || !bp->pixmap_rect.height)
    {
		StringLayoutLabelOnly(w);
    }
    else
    {
		/* Layout the label according to the button's layout */
		switch(bp->button_layout)
		{
		case XmBUTTON_LABEL_ON_BOTTOM:

			StringLayoutLabelOnBottom(w);

			break;
	    
		case XmBUTTON_LABEL_ON_TOP:
	    
			StringLayoutLabelOnTop(w);

			break;
	    
		case XmBUTTON_LABEL_ON_LEFT:

			StringLayoutLabelOnLeft(w);
	    
			break;
	    
		case XmBUTTON_LABEL_ON_RIGHT:
	    
			StringLayoutLabelOnRight(w);
	    
			break;
		}
    }	    
}
/*----------------------------------------------------------------------*/
static void
DrawString(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeButtonPart *			bp = _XfeButtonPart(w);
    XfeLabelPart *			lp = _XfeLabelPart(w);
	XfeLabelWidgetClass		lwc = (XfeLabelWidgetClass) xfeLabelWidgetClass;

    /* Make sure the label needs to be drawn */
    if (bp->button_layout == XmBUTTON_PIXMAP_ONLY)
    {
		return;
    }

	/* Set the label's misc offset to the arm offset if needed */
	lp->misc_offset = bp->armed ? bp->arm_offset : 0;


	/* Explicit invoke of XfeLabel's draw_string() method */
	(*lwc->xfe_label_class.draw_string)(w,event,region,clip_rect);

	/* Restore the label's misc offset */
	lp->misc_offset = 0;
}
/*----------------------------------------------------------------------*/
static GC
GetLabelGC(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

	return bp->raised ? bp->label_raised_GC : lp->label_GC;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutPixmap(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

    if ((bp->button_layout == XmBUTTON_PIXMAP_ONLY) ||
		!lp->label_rect.width || !lp->label_rect.height)
    {
		bp->pixmap_rect.x = (_XfeWidth(w) - bp->pixmap_rect.width) / 2;
		bp->pixmap_rect.y = (_XfeHeight(w) - bp->pixmap_rect.height) / 2;
    }
    else
    {
		Dimension raise_offset = RAISE_OFFSET(bp);

		/* Layout the label according to the button's layout */
		switch(bp->button_layout)
		{
		case XmBUTTON_LABEL_ON_BOTTOM:
	    
			bp->pixmap_rect.x = (_XfeWidth(w) - bp->pixmap_rect.width) / 2;
	    
			bp->pixmap_rect.y = _XfeOffsetTop(w) + raise_offset;
	    
			break;
	    
		case XmBUTTON_LABEL_ON_TOP:
	    
			bp->pixmap_rect.x = (_XfeWidth(w) - bp->pixmap_rect.width) / 2;
	    
			bp->pixmap_rect.y = 
				_XfeHeight(w) - 
				_XfeOffsetBottom(w) - 
				bp->pixmap_rect.height -
				raise_offset;
	    
			break;
	    
		case XmBUTTON_LABEL_ON_LEFT:
	    
			bp->pixmap_rect.y = (_XfeHeight(w) - bp->pixmap_rect.height) / 2;
	    
			bp->pixmap_rect.x = 
				_XfeWidth(w) - 
				_XfeOffsetRight(w) - 
				bp->pixmap_rect.width -
				raise_offset;
			
			break;
	    
		case XmBUTTON_LABEL_ON_RIGHT:
	    
			bp->pixmap_rect.y = (_XfeHeight(w) - bp->pixmap_rect.height) / 2;
	    
			bp->pixmap_rect.x = _XfeOffsetLeft(w) + raise_offset;
	    
			break;
		}
    }
}
/*----------------------------------------------------------------------*/
static void
DrawPixmap(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    Pixmap			pixmap = XmUNSPECIFIED_PIXMAP;
    Pixmap			mask = XmUNSPECIFIED_PIXMAP;
	Boolean			insensitive = False;

    /* Make sure the pixmap needs to be drawn */
    if (bp->button_layout == XmBUTTON_LABEL_ONLY)
    {
		return;
    }

    /* Determine if the button is insensitive */
	insensitive = (!_XfeIsSensitive(w) || !bp->determinate);

    /* Determine which pixmap to use */
	/* for indeterminate state, use insensitive pixmap */
    if (insensitive && _XfePixmapGood(bp->insensitive_pixmap))
    {
		pixmap = bp->insensitive_pixmap;
		mask = bp->insensitive_pixmap_mask;
    }
    else if (bp->armed && _XfePixmapGood(bp->armed_pixmap))
    {
		pixmap = bp->armed_pixmap;
		mask = bp->armed_pixmap_mask;
    }
    else if (bp->raised && _XfePixmapGood(bp->raised_pixmap))
    {
		pixmap = bp->raised_pixmap;
		mask = bp->raised_pixmap_mask;
    }
    else
    {
		pixmap = bp->pixmap;
		mask = bp->pixmap_mask;
    }

    /* Make sure the pixmap is good before rendering it */
    if (_XfePixmapGood(pixmap) && 
		bp->pixmap_rect.width && 
		bp->pixmap_rect.height)
    {
		Dimension	offset = bp->armed ? bp->arm_offset : 0;
		Position	x = bp->pixmap_rect.x + offset;
		Position	y = bp->pixmap_rect.y + offset;

		if (_XfePixmapGood(mask))
		{
			XSetClipOrigin(XtDisplay(w),bp->pixmap_GC,x,y);
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,mask);
		}
		else
		{
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,None);
		}

		XCopyArea(XtDisplay(w),
				  pixmap,
				  _XfePrimitiveDrawable(w),
				  bp->pixmap_GC,
				  0,0,
				  bp->pixmap_rect.width,
				  bp->pixmap_rect.height,
				  x,
				  y);

		/*
		 * If the button is insensitive and the pixmap we just
		 * rendered is not the XmNinsensitivePixmap, then we 
		 * need to also render the insensitive effect.
		 */
		if (insensitive && (pixmap != bp->insensitive_pixmap))
		{
			assert( _XfeBackgroundGC(w) != NULL );

			XfeStippleRectangle(XtDisplay(w),
								_XfePrimitiveDrawable(w),
 								_XfeBackgroundGC(w),
								x,
								y,
								bp->pixmap_rect.width,
								bp->pixmap_rect.height,
								2);
		}
    }
}
/*----------------------------------------------------------------------*/
static void
DrawRaiseBorder(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

	/* Make sure the thickness of the raise border ain't */
	if (bp->raise_border_thickness == 0)
	{
		return;
	}
	
	/*
	 * The border is only needed if raise_on_enter is true or we are in
	 * a special clicking state (between Arm() and Disarm())
	 */
	if (!bp->raise_on_enter && !bp->armed)
	{
		return;
	}

 	if (bp->raised || (bp->raise_on_enter && bp->armed && bp->clicking))
	{
		XfeDrawRectangle(XtDisplay(w),
						 
						 _XfePrimitiveDrawable(w),
							 
						 _XfeHighlightGC(w),
							 
						 _XfeHighlightThickness(w),
							 
						 _XfeHighlightThickness(w),
							 
						 _XfeWidth(w) - 2 * _XfeHighlightThickness(w),
							 
						 _XfeHeight(w) - 2 * _XfeHighlightThickness(w),
							 
						 bp->raise_border_thickness);
	}
	else
	{
		XfeDrawRectangle(XtDisplay(w),
							 
						 _XfePrimitiveDrawable(w),
							 
						 _XfemBackgroundGC(XtParent(w)),
							 
						 _XfeHighlightThickness(w),
							 
						 _XfeHighlightThickness(w),
							 
						 _XfeWidth(w) - 2 * _XfeHighlightThickness(w),
							 
						 _XfeHeight(w) - 2 * _XfeHighlightThickness(w),
							 
						 bp->raise_border_thickness);
	}
}
/*----------------------------------------------------------------------*/
static void
ArmTimeout(XtPointer client_data,XtIntervalId * id)
{
	/* Write me */
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeButton functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PixmapPrepare(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

	_XfePixmapPrepare(w,
					  &bp->pixmap,
					  &bp->pixmap_rect.width,
					  &bp->pixmap_rect.height,
					  XmNpixmap);
}
/*----------------------------------------------------------------------*/
static void
PixmapArmedPrepare(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XRectangle		rect;

	_XfePixmapPrepare(w,
					  &bp->armed_pixmap,
					  &rect.width,
					  &rect.height,
					  XmNarmedPixmap);

    if (_XfePixmapGood(bp->armed_pixmap))
	{
		/* Make sure the normal pixmap is defined */
		if (_XfePixmapGood(bp->pixmap))
		{
			/* Make sure the geometry matches the main pixmap's */
			if ((rect.width  != bp->pixmap_rect.width) ||
				(rect.height != bp->pixmap_rect.height))
			{
				bp->armed_pixmap = XmUNSPECIFIED_PIXMAP;
				
				_XfeWarning(w,MESSAGE10);
			}
		}
		else
		{
			bp->armed_pixmap = XmUNSPECIFIED_PIXMAP;

			_XfeWarning(w,MESSAGE7);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
PixmapRaisedPrepare(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XRectangle		rect;

	_XfePixmapPrepare(w,
					  &bp->raised_pixmap,
					  &rect.width,
					  &rect.height,
					  XmNraisedPixmap);

    if (_XfePixmapGood(bp->raised_pixmap))
	{
		/* Make sure the normal pixmap is defined */
		if (_XfePixmapGood(bp->pixmap))
		{
			/* Make sure the geometry matches the main pixmap's */
			if ((rect.width  != bp->pixmap_rect.width) ||
				(rect.height != bp->pixmap_rect.height))
			{
				bp->raised_pixmap = XmUNSPECIFIED_PIXMAP;
				
				_XfeWarning(w,MESSAGE11);
			}
		}
		else
		{
			bp->raised_pixmap = XmUNSPECIFIED_PIXMAP;

			_XfeWarning(w,MESSAGE8);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
PixmapInsensPrepare(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XRectangle		rect;

	_XfePixmapPrepare(w,
					  &bp->insensitive_pixmap,
					  &rect.width,
					  &rect.height,
					  XmNinsensitivePixmap);

    if (_XfePixmapGood(bp->insensitive_pixmap))
	{
		/* Make sure the normal pixmap is defined */
		if (_XfePixmapGood(bp->pixmap))
		{
			/* Make sure the geometry matches the main pixmap's */
			if ((rect.width  != bp->pixmap_rect.width) ||
				(rect.height != bp->pixmap_rect.height))
			{
				bp->insensitive_pixmap = XmUNSPECIFIED_PIXMAP;
				
				_XfeWarning(w,MESSAGE12);
			}
		}
		else
		{
			bp->insensitive_pixmap = XmUNSPECIFIED_PIXMAP;

			_XfeWarning(w,MESSAGE9);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
StringLayoutLabelOnly(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

	/* Layout horizontally according to alignment */
	switch(lp->label_alignment)
	{
	case XmALIGNMENT_BEGINNING:
		
		lp->label_rect.x = _XfeRectX(w) + RAISE_OFFSET(bp);
		
		break;
		
	case XmALIGNMENT_CENTER:
		
		lp->label_rect.x = (_XfeWidth(w) - lp->label_rect.width) / 2;
		
		break;
		
	case XmALIGNMENT_END:
		
		lp->label_rect.x = 
			XfeWidth(w) - 
			lp->label_rect.width - 
			_XfeOffsetRight(w) -
			RAISE_OFFSET(bp);
		
		break;
	}
	
	/* Layout vertically centred */
	lp->label_rect.y = (_XfeHeight(w) - lp->label_rect.height) / 2;
}
/*----------------------------------------------------------------------*/
static void
StringLayoutLabelOnBottom(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

	/* Layout horizontally according to alignment */
	switch(lp->label_alignment)
	{
	case XmALIGNMENT_BEGINNING:
		
		lp->label_rect.x = _XfeRectX(w) + RAISE_OFFSET(bp);
		
		break;
		
	case XmALIGNMENT_CENTER:
		
		lp->label_rect.x = (_XfeWidth(w) - lp->label_rect.width) / 2;
		
		break;
		
	case XmALIGNMENT_END:
		
		lp->label_rect.x = 
			XfeWidth(w) - 
			lp->label_rect.width - 
			_XfeOffsetRight(w) -
			RAISE_OFFSET(bp);
		
		break;
	}
	
	/* Layout horizontally on bottom */
	lp->label_rect.y = 
		_XfeHeight(w) - 
		_XfeOffsetBottom(w) - 
		lp->label_rect.height -
		RAISE_OFFSET(bp);
}
/*----------------------------------------------------------------------*/
static void
StringLayoutLabelOnTop(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

	/* Layout horizontally according to alignment */
	switch(lp->label_alignment)
	{
	case XmALIGNMENT_BEGINNING:
		
		lp->label_rect.x = _XfeRectX(w) + RAISE_OFFSET(bp);
		
		break;
		
	case XmALIGNMENT_CENTER:
		
		lp->label_rect.x = (_XfeWidth(w) - lp->label_rect.width) / 2;
		
		break;
		
	case XmALIGNMENT_END:
		
		lp->label_rect.x = 
			XfeWidth(w) - 
			lp->label_rect.width - 
			_XfeOffsetRight(w) -
			RAISE_OFFSET(bp);
		
		break;
	}

	/* Layout horizontally on top */
	lp->label_rect.y = _XfeOffsetTop(w) + RAISE_OFFSET(bp);
}
/*----------------------------------------------------------------------*/
static void
StringLayoutLabelOnLeft(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

	lp->label_rect.x = _XfeOffsetLeft(w) + RAISE_OFFSET(bp);

	lp->label_rect.y = (_XfeHeight(w) - lp->label_rect.height) / 2;
	
}
/*----------------------------------------------------------------------*/
static void
StringLayoutLabelOnRight(Widget w)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

	lp->label_rect.x = 
		_XfeWidth(w) - 
		_XfeOffsetRight(w) - 
		lp->label_rect.width -
		RAISE_OFFSET(bp);

	lp->label_rect.y = (_XfeHeight(w) - lp->label_rect.height) / 2;
}
/*----------------------------------------------------------------------*/
static void
InvokeCallback(Widget w,XtCallbackList list,int reason,XEvent * event)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

    /* Invoke the callbacks only if needed */
	if (list)
    {
		XfeButtonCallbackStruct cbs;
	
		cbs.event	= event;
		cbs.reason	= reason;
		cbs.armed	= bp->armed;
		cbs.raised	= bp->raised;

		/* Flush the display */
		XFlush(XtDisplay(w));
	
		/* Invoke the Callback List */
		XtCallCallbackList(w,list,&cbs);
    }
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptEvent(Widget w,XEvent * event)
{
	int		x;
	int		y;

	/* Obtain the event coords */
	if (XfeEventGetXY(event,&x,&y))
	{
		return XfeButtonAcceptXY(w,x,y);
	}

	return False;
}
/*----------------------------------------------------------------------*/
static Boolean
SpacingContainsXY(Widget w,int x,int y)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);

	/* Check whether the coords are inside the spacing only if its > 0 */
	if (bp->spacing > 0)
	{
		XRectangle rect;
		
		if (bp->button_layout == XmBUTTON_LABEL_ON_BOTTOM)
		{
			XfeRectSet(&rect,
					   (_XfeWidth(w) - MIN_COMP_WIDTH(lp,bp)) / 2,
					   lp->label_rect.y - bp->spacing,
					   MIN_COMP_WIDTH(lp,bp),
					   bp->spacing);
		}
		else if (bp->button_layout == XmBUTTON_LABEL_ON_TOP)
		{
			XfeRectSet(&rect,
					   (_XfeWidth(w) - MIN_COMP_WIDTH(lp,bp)) / 2,
					   bp->pixmap_rect.y - bp->spacing,
					   MIN_COMP_WIDTH(lp,bp),
					   bp->spacing);
		}
		else if (bp->button_layout == XmBUTTON_LABEL_ON_LEFT)
		{
			XfeRectSet(&rect,
					   bp->pixmap_rect.x - bp->spacing,
					   (_XfeHeight(w) - MIN_COMP_HEIGHT(lp,bp)) / 2,
					   bp->spacing,
					   MIN_COMP_HEIGHT(lp,bp));
		}
		else if (bp->button_layout == XmBUTTON_LABEL_ON_RIGHT)
		{
			XfeRectSet(&rect,
					   lp->label_rect.x - bp->spacing,
					   (_XfeHeight(w) - MIN_COMP_HEIGHT(lp,bp)) / 2,
					   bp->spacing,
					   MIN_COMP_HEIGHT(lp,bp));
		}
		
		return XfePointInRect(&rect,x,y);
	}
	
	return False;
}
/*----------------------------------------------------------------------*/
static void
TransparentCursorSetState(Widget w,Boolean state)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

	/* Make sure the transparent cursor is good and we are sensitive */
	if (!_XfeCursorGood(bp->transparent_cursor) || !_XfeIsSensitive(w))
	{
		return;
	}

	/* Make sure the state has changed */
	if (state == bp->transparent_cursor_state)
	{
		return;
	}

	/* Update the state */
	bp->transparent_cursor_state = state;

	if(state)
	{
		XfeCursorDefine(w,bp->transparent_cursor);
	}
	else
	{
		XfeCursorUndefine(w);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonLayoutPixmap(Widget w)
{
	XfeButtonWidgetClass bc = (XfeButtonWidgetClass) XtClass(w);

	if (bc->xfe_button_class.layout_pixmap)
	{
		(*bc->xfe_button_class.layout_pixmap)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonDrawPixmap(Widget			w,
					 XEvent *		event,
					 Region			region,
					 XRectangle *	clip_rect)
{
	XfeButtonWidgetClass bc = (XfeButtonWidgetClass) XtClass(w);

	if (bc->xfe_button_class.draw_pixmap)
	{
		(*bc->xfe_button_class.draw_pixmap)(w,event,region,clip_rect);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonDrawRaiseBorder(Widget		w,
						  XEvent *		event,
						  Region		region,
						  XRectangle *	clip_rect)
{
	XfeButtonWidgetClass bc = (XfeButtonWidgetClass) XtClass(w);

	if (bc->xfe_button_class.draw_raise_border)
	{
		(*bc->xfe_button_class.draw_raise_border)(w,event,region,clip_rect);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonArmTimeout(Widget w,XtPointer client_data,XtIntervalId * id)
{
	XfeButtonWidgetClass bc = (XfeButtonWidgetClass) XtClass(w);
	
	if (bc->xfe_button_class.arm_timeout)
	{
		(*bc->xfe_button_class.arm_timeout)(client_data,id);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */  void
_XfeButtonEnter(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    Boolean			redisplay = False;

	/* Accept the event based on the button_trigger resource.  This needs to
	 * happen before we call _XfePrimitiveEnter() so that enter callbacks 
	 * don't get called.
	 */
	if (!AcceptEvent(w,event))
	{
		/* Turn the transparent cursor on */
		TransparentCursorSetState(w,True);
		
		return;
	}

	if (XfeEventGetModifiers(event) & Button1Mask)
	{
		if (bp->transparent_cursor_state)
		{
			return;
		}
	}

    /* First, invoke the XfePrimitive's enter action */
    _XfePrimitiveEnter(w,event,params,nparams);

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

    /* Adjust armed state if needed */
    if (bp->clicking && !bp->emulate_motif)
    {
		bp->armed = True;

		redisplay = True;
    }

    /* Adjust raised state if needed */
    if (bp->raise_on_enter)
    {
		if (!bp->clicking)
		{
			bp->raised = True;
			
			redisplay = True;
		}
    }
	else if (bp->fill_on_enter)
	{
		redisplay = True;
	}

    if (redisplay)
    {
		/* Erase the old stuff */
		_XfePrimitiveClearBackground(w);
	
		/* Redraw the buffer */
		_XfePrimitiveDrawEverything(w,NULL,NULL);
	
		/* Draw the buffer onto the window */
		XfeExpose(w,NULL,NULL);
    }
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonLeave(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    Boolean			redisplay = False;

	/* Turn the transparent cursor off if needed */
	if (bp->button_trigger != XmBUTTON_TRIGGER_ANYWHERE)
	{
		TransparentCursorSetState(w,False);
	}

    /* First, invoke the XfePrimitive's leave action */
    _XfePrimitiveLeave(w,event,params,nparams);

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

    /* Adjust armed state if needed */
    if (bp->armed && !bp->emulate_motif)
    {
		bp->armed = False;

		redisplay = True;
    }

    /* Adjust raised state if needed */
    if (bp->raise_on_enter)
    {
		if (!bp->clicking)
		{
			bp->raised = False;
			
			redisplay = True;
		}
    }
	else if (bp->fill_on_enter)
	{
		redisplay = True;
	}

    if (redisplay)
    {
		/* Erase the old stuff */
		_XfePrimitiveClearBackground(w);
	
		/* Redraw the buffer */
		_XfePrimitiveDrawEverything(w,NULL,NULL);
	
		/* Draw the buffer onto the window */
		XfeExpose(w,NULL,NULL);
    }
}
/*----------------------------------------------------------------------*/
/* extern */  void
_XfeButtonMotion(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	/* Make sure a button is not being pressed */ 
	if (XfeEventGetModifiers(event) & Button1Mask)
	{
		return;
	}

	/*
	 * If the event is accepted, pretend we got an enter event.  If not,
	 * pretend we got a leave event.
	 */
	if (AcceptEvent(w,event))
	{
		if (!_XfePointerInside(w))
		{
			/* Turn the transparent cursor off */
			TransparentCursorSetState(w,False);

			_XfeButtonEnter(w,event,params,nparams);
		}
	}
	else
	{
		if (_XfePointerInside(w))
		{
			_XfeButtonLeave(w,event,params,nparams);

			/* Turn the transparent cursor on */
			TransparentCursorSetState(w,True);
		}

	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonActivate(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

	/* Make sure we are not insenitsive (or pretending to be insensitive) */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

    /* Make sure we are indeed clicking */
    if (!bp->clicking)
    {
		return;
    }

    /* Invoke callbacks only if pointer is still inside button */
    if (_XfePointerInside(w))
    {
		InvokeCallback(w,bp->activate_callback,XmCR_ACTIVATE,event);
    }
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonArm(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);
	Boolean			accept_event;

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

    _XfePrimitiveFocus(w,event,params,nparams);

	/* Determine whether this event is acceptable */
	accept_event = AcceptEvent(w,event);

	/* Look for a Select() action */
	if (_XfeLabelAcceptSelectionEvent(w,event,!accept_event))
	{
		_XfeLabelSetSelected(w,event,!lp->selected,True);

		return;
	}

	/* Look for a Edit() action */
	if (_XfeLabelAcceptEditEvent(w,event,!accept_event))
	{
		_XfeLabelEdit(w,event,params,nparams);

		return;
	}

	/* Make sure the event is accepted */
	if (!accept_event)
	{
		return;
	}

	/* Make sure we are not already clicking */
	if (bp->clicking)
	{
		return;
	}

    /* We are now clicking */
    bp->clicking = True;

	/* If we are in none mode, do nothing more */
	if (bp->button_type == XmBUTTON_NONE)
	{
		return;
	}

	/* Being simultaneously armed and raised is confusing, so its out */
	if (bp->raise_on_enter)
	{
		bp->raised = False;
	}

	/* If we are in toggle mode, toggle the armed state */
	if (bp->button_type == XmBUTTON_TOGGLE)
	{
		bp->armed = !bp->armed;
	}
	/* Otherwise turn it on */
	else
	{
		bp->armed = True;
	}
    
    /* Erase the old stuff */
    _XfePrimitiveClearBackground(w);
    
    /* Redraw the buffer */
    _XfePrimitiveDrawEverything(w,NULL,NULL);
    
    /* Draw the buffer onto the window */
    XfeExpose(w,NULL,NULL);

	/* If we are in toggle mode, invoke the appropiate callback */
	if (bp->button_type == XmBUTTON_TOGGLE)
	{
		if (bp->armed)
		{
			InvokeCallback(w,bp->arm_callback,XmCR_ARM,event);
		}
		else
		{
			InvokeCallback(w,bp->disarm_callback,XmCR_DISARM,event);
		}
	}
	/* Otherwise invoke the arm callback */
	else
	{
		InvokeCallback(w,bp->arm_callback,XmCR_ARM,event);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonDisarm(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	/* Make sure we are indeed clicking */
	if (!bp->clicking)
	{
		return;
	}

	/* If we are in none mode, do nothing more */
	if (bp->button_type == XmBUTTON_NONE)
	{
		return;
	}

    /* We are now no longer clicking */
    bp->clicking = False;

	/* Raise the button if needed and the pointer is still inside */
	if (bp->raise_on_enter && _XfePointerInside(w))
	{
		bp->raised = True;
	}

	if (bp->button_type != XmBUTTON_TOGGLE)
	{
		bp->armed = False;
		
		/* Erase the old stuff */
		_XfePrimitiveClearBackground(w);
		
		/* Redraw the buffer */
		_XfePrimitiveDrawEverything(w,NULL,NULL);
		
		/* Draw the buffer onto the window */
		XfeExpose(w,NULL,NULL);

		InvokeCallback(w,bp->disarm_callback,XmCR_DISARM,event);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonArmAndActivate(Widget		w,
						 XEvent *	event,
						 char **	params,
						 Cardinal *	nparams)
{
	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	/* Accept the event based on the button_trigger resource */
	if (!AcceptEvent(w,event))
	{
		return;
	}

    _XfeButtonArm(w,event,params,nparams);

    _XfeButtonActivate(w,event,params,nparams);

    _XfeButtonDisarm(w,event,params,nparams);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonBtn3Down(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	InvokeCallback(w,bp->button_3_down_callback,XmCR_BUTTON_3_DOWN,event);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeButtonBtn3Up(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	InvokeCallback(w,bp->button_3_up_callback,XmCR_BUTTON_3_UP,event);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton Public Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateButton(Widget parent,char *name,Arg *args,Cardinal count)
{
    return (XtCreateWidget(name,xfeButtonWidgetClass,parent,args,count));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeButtonPreferredGeometry(Widget			w,
						   unsigned char	layout,
						   Dimension *		width_out,
						   Dimension *		height_out)
{
    XfeButtonPart *		bp;
	Dimension			width;
	Dimension			height;
	unsigned char		old_layout;

	assert( _XfeIsAlive(w) );
	assert( XfeIsButton(w) );

    bp = _XfeButtonPart(w);

	/* Make sure the new layout is valid */
    if (!XfeRepTypeCheck(w,XmRButtonLayout,&layout,XmBUTTON_LABEL_ON_BOTTOM))
	{
		_XfeWarning(w,MESSAGE16);

		return;
	}

	/* Remember the old layout */
	old_layout = bp->button_layout;

	bp->button_layout = layout;
	
    _XfePrimitivePreferredGeometry(w,&width,&height);

	bp->button_layout = old_layout;

	if (width_out)
	{
		*width_out = width;
	}

	if (height_out)
	{
		*height_out = height;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeButtonAcceptXY(Widget w,int x,int y)
{
    XfeButtonPart *	bp = _XfeButtonPart(w);
    XfeLabelPart *	lp = _XfeLabelPart(w);
	Boolean			result = True;

	/* Anywhere */
	if (bp->button_trigger == XmBUTTON_TRIGGER_ANYWHERE)
	{
 		result = True;
	}
	/* Label */
	else if (bp->button_trigger == XmBUTTON_TRIGGER_LABEL)
	{
		if (bp->button_layout != XmBUTTON_PIXMAP_ONLY)
		{
			result = XY_IN_LABEL(lp,x,y);
		}
	}
	/* Pixmap */
	else if (bp->button_trigger == XmBUTTON_TRIGGER_PIXMAP)
	{
		if (bp->button_layout != XmBUTTON_LABEL_ONLY)
		{
			result = XY_IN_PIXMAP(bp,x,y);
		}
	}
	/* Neither */
	else if(bp->button_trigger == XmBUTTON_TRIGGER_NEITHER)
	{
		result = (!XY_IN_LABEL(lp,x,y) && !XY_IN_PIXMAP(bp,x,y));
	}
	/* Either */
	else if (bp->button_trigger == XmBUTTON_TRIGGER_EITHER)
	{
		if (bp->button_layout == XmBUTTON_LABEL_ONLY)
		{
			result = XY_IN_LABEL(lp,x,y);
		}
		else if (bp->button_layout == XmBUTTON_PIXMAP_ONLY)
		{
			result = XY_IN_PIXMAP(bp,x,y);
		}
		else 
		{
			result = (XY_IN_LABEL(lp,x,y) || 
					  XY_IN_PIXMAP(bp,x,y) || 
					  SpacingContainsXY(w,x,y));
		}
	}

	return result;
}
/*----------------------------------------------------------------------*/

