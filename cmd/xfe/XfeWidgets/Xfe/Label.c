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
/* Name:		<Xfe/Label.c>											*/
/* Description:	XfeLabel widget source.									*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/LabelP.h>
#include <Xfe/ToolBar.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeLabel."
#define MESSAGE2 "If XmNtruncateLabel is true, XmNtruncateProc cannot be NULL."
#define MESSAGE3 "Cannot edit label because parent is not XfeToolBar."

#define INSENSITIVE_OFFSET 1

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassPartInit	(WidgetClass);
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);
static void		GetValuesHook	(Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	PreferredGeometry		(Widget,Dimension *,Dimension *);
static void	PrepareComponents		(Widget,int);
static void	LayoutComponents		(Widget);
static void	DrawComponents			(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	LayoutString			(Widget);
static void	DrawString				(Widget,XEvent *,Region,XRectangle *);
static void	DrawSelection			(Widget,XEvent *,Region,XRectangle *);
static GC	GetLabelGC				(Widget);
static GC	GetSelectionGC			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeLabel functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmString	CreateTruncatedString				(Widget);
static void		InvokeSelectionChangedCallback		(Widget,XEvent *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel Resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
    /* Callback resources */     	
    { 
		XmNselectionChangedCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeLabelRec , xfe_label . selection_changed_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },

    /* Label resources */
    { 
		XmNlabelAlignment,
		XmCLabelAlignment,
		XmRAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfeLabelRec , xfe_label . label_alignment),
		XmRImmediate, 
		(XtPointer) XmALIGNMENT_CENTER
    },
    { 
		XmNlabelDirection,
		XmCLabelDirection,
		XmRStringDirection,
		sizeof(unsigned char),
		XtOffsetOf(XfeLabelRec , xfe_label . label_direction),
		XmRImmediate, 
		(XtPointer) XmSTRING_DIRECTION_L_TO_R
    },
    { 
		XmNfontList,
		XmCFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeLabelRec , xfe_label . font_list),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNlabelString,
		XmCLabelString,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfeLabelRec , xfe_label . label_string),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Truncation resources */
    { 
		XmNtruncateLabel,
		XmCTruncateLabel,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeLabelRec , xfe_label  . truncate_label),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNtruncateProc,
		XmCTruncateProc,
		XmRProc,
		sizeof(XtPointer),
		XtOffsetOf(XfeLabelRec , xfe_label  . truncate_proc),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Selection resources */
    { 
		XmNselected,
		XmCSelected,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeLabelRec , xfe_label  . selected),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNselectionColor,
		XmCSelectionColor,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeLabelRec , xfe_label  . selection_color),
		XmRCallProc, 
		(XtPointer) _XfeCallProcSelectPixel
    },
    { 
		XmNselectionModifiers,
		XmCSelectionModifiers,
		XmRModifiers,
		sizeof(Modifiers),
		XtOffsetOf(XfeLabelRec , xfe_label  . selection_modifiers),
		XmRImmediate, 
		(XtPointer) None
    },

	/* Edit resources */
    { 
		XmNeditModifiers,
		XmCEditModifiers,
		XmRModifiers,
		sizeof(Modifiers),
		XtOffsetOf(XfeLabelRec , xfe_label  . edit_modifiers),
		XmRImmediate, 
		(XtPointer) None
    },

    /* Force XmNbufferType to XmBUFFER_SHARED */
    { 
		XmNbufferType,
		XmCBufferType,
		XmRBufferType,
		sizeof(unsigned char),
		XtOffsetOf(XfeLabelRec , xfe_primitive . buffer_type),
		XmRImmediate, 
		(XtPointer) XmBUFFER_SHARED
    },

    /* Force XmNtraversalOn to False */
    { 
		XmNtraversalOn,
		XmCTraversalOn,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeLabelRec , primitive . traversal_on),
		XmRImmediate, 
		(XtPointer) False
    },

    /* Force XmNhighlightThickness to 0 */
    { 
		XmNhighlightThickness,
		XmCHighlightThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeLabelRec , primitive . highlight_thickness),
		XmRImmediate, 
		(XtPointer) 0
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "Btn1Down",				_XfeLabelBtn1Down			},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel widget class record initialization							*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(label,Label) =
{
    {
		/* Core Part */
		(WidgetClass) &xfePrimitiveClassRec,	/* superclass         	*/
		"XfeLabel",								/* class_name         	*/
		sizeof(XfeLabelRec),					/* widget_size        	*/
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
		GetValuesHook,							/* get_values_hook		*/
		NULL,                                   /* accept_focus       	*/
		XtVersion,                              /* version            	*/
		NULL,                                   /* callback_private   	*/
		_XfeLabelDefaultTranslations,			/* tm_table           	*/
		XtInheritQueryGeometry,					/* query_geometry     	*/
		XtInheritDisplayAccelerator,            /* display accel      	*/
		NULL,                                   /* extension          	*/
    },

    /* XmPrimitive Part */
    {
		XmInheritBorderHighlight,				/* border_highlight		*/
		XmInheritBorderUnhighlight,				/* border_unhighlight 	*/
		XtInheritTranslations,                  /* translations       	*/
		NULL,									/* arm_and_activate   	*/
		NULL,									/* syn resources      	*/
		0,										/* num syn_resources  	*/
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
		XfeInheritDrawBackground,				/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		NULL,									/* extension            */
    },

    /* XfeLabel Part */
    {
		LayoutString,							/* layout_string		*/
		DrawString,								/* draw_string			*/
		DrawSelection,							/* draw_selection		*/
		GetLabelGC,								/* get_label_gc			*/
		GetSelectionGC,							/* get_selection_gc		*/
		NULL,									/* extension            */
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeLabelWidgetClass declaration.										*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(label,Label);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassPartInit(WidgetClass wc)
{
	XfeLabelWidgetClass cc = (XfeLabelWidgetClass) wc;
	XfeLabelWidgetClass sc = (XfeLabelWidgetClass) wc->core_class.superclass;

	/* Resolve inheritance of all XfeLabel class methods */
	_XfeResolve(cc,sc,xfe_label_class,layout_string,
				XfeInheritLayoutString);

	_XfeResolve(cc,sc,xfe_label_class,draw_string,
				XfeInheritDrawString);

	_XfeResolve(cc,sc,xfe_label_class,draw_selection,
				XfeInheritDrawSelection);

	_XfeResolve(cc,sc,xfe_label_class,get_label_gc,
				XfeInheritGetLabelGC);

	_XfeResolve(cc,sc,xfe_label_class,get_selection_gc,
				XfeInheritGetSelectionGC);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeLabelPart * lp = _XfeLabelPart(nw);

    /* Make sure rep types are ok */
    XfeRepTypeCheck(nw,XmRAlignment,&lp->label_alignment,
					XmALIGNMENT_CENTER);

    XfeRepTypeCheck(nw,XmRStringDirection,&lp->label_direction,
		   XmSTRING_DIRECTION_L_TO_R);

    /* Make sure the label is set up properly */
    lp->label_string = XfeXmStringCopy(nw,lp->label_string,XtName(nw));
	
    /* Make sure the font list is set up properly */
    lp->font_list = XfeXmFontListCopy(nw,lp->font_list,XmLABEL_FONTLIST);

    /* Allocate the label GCs */
    lp->label_GC = XfeAllocateStringGc(nw,lp->font_list,
									   _XfeForeground(nw),None,True);
    
    /* Allocate the label insensitive GCs */
    lp->insensitive_top_GC = XfeAllocateStringGc(nw,lp->font_list,
												 _XfeTopShadowColor(nw),
												 None,True);
	
    lp->insensitive_bottom_GC = XfeAllocateStringGc(nw,lp->font_list,
													_XfeBottomShadowColor(nw),
													None,True);

    /* Allocate the selection GCs */
    lp->selection_GC = XfeAllocateColorGc(nw,lp->selection_color,None,True);

	/* Initialize private members */
	lp->misc_offset = 0;
    
    /* Finish of initialization */
    _XfePrimitiveChainInitialize(rw,nw,xfeLabelWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeLabelPart * lp = _XfeLabelPart(w);

    /* Release GCs */
    XtReleaseGC(w,lp->label_GC);
    XtReleaseGC(w,lp->insensitive_top_GC);
    XtReleaseGC(w,lp->insensitive_bottom_GC);
    XtReleaseGC(w,lp->selection_GC);

	/* Free the label string */
	XmStringFree(lp->label_string);

	/* Free the font list */
	XmFontListFree(lp->font_list);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeLabelPart *	np = _XfeLabelPart(nw);
    XfeLabelPart *	op = _XfeLabelPart(ow);
    Boolean			label_gc_flag = False;
    Boolean			selection_gc_flag = False;
    Boolean			insensitive_top_gc_flag = False;
    Boolean			insensitive_bottom_gc_flag = False;

    /* font_list */
    if (np->font_list != op->font_list)
    {
		/* Make sure the new font is ok */
		np->font_list = XfeXmFontListCopy(nw,np->font_list,XmLABEL_FONTLIST);
      
		/* Free the old fontlist */
		if (op->font_list)
		{
			XmFontListFree(op->font_list);
		}
	
		label_gc_flag = True;   
		insensitive_bottom_gc_flag = True;
		insensitive_top_gc_flag = True;
	
		_XfeConfigFlags(nw) |= XfeConfigGLE;
	
		_XfePrepareFlags(nw) |= _XFE_PREPARE_LABEL_STRING;
    }

    /* label_string */
    if (np->label_string != op->label_string)
    {
		/* Free the old string if needed */
		if (op->label_string)
		{
			XmStringFree(op->label_string);
		}
	
		/* Make sure the new string is ok */
		np->label_string = XfeXmStringCopy(nw,np->label_string,XtName(nw));

		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_LABEL_STRING;
    }

    /* foreground */
    if (_XfeForeground(nw) != _XfeForeground(ow))
    {
		label_gc_flag = True;   

		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* top_shadow_color */
    if (_XfeTopShadowColor(nw) != _XfeTopShadowColor(ow))
    {
		insensitive_top_gc_flag = True;

		if (!_XfeIsSensitive(nw))
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
    }

    /* bottom_shadow_color */
    if (_XfeBottomShadowColor(nw) != _XfeBottomShadowColor(ow))
    {
		insensitive_bottom_gc_flag = True;

		if (!_XfeIsSensitive(nw))
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}

    }

    /* label_alignment */
    if (np->label_alignment != op->label_alignment)
    {
		/* Make sure the new label alignment is ok */
		XfeRepTypeCheck(nw,XmRAlignment,&np->label_alignment,
						XmALIGNMENT_CENTER);

		_XfeConfigFlags(nw) |= XfeConfigLE;   
    }

    /* label_direction */
    if (np->label_direction != op->label_direction)
    {
		/* Make sure the new label direction is ok */
		XfeRepTypeCheck(nw,XmRStringDirection,&np->label_direction,
						XmSTRING_DIRECTION_L_TO_R);

		_XfeConfigFlags(nw) |= XfeConfigExpose;   
    }

    /* selected */
    if (np->selected != op->selected)
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* selection_color */
    if (np->selection_color != op->selection_color)
    {
		selection_gc_flag = True;   

		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* Update the label GC */
    if (label_gc_flag)
    {
		/* Release the old label GC */
		XtReleaseGC(nw,np->label_GC);
	
		/* Allocate the new label GC */
		np->label_GC = XfeAllocateStringGc(nw,np->font_list,
										   _XfeForeground(nw),None,True);
    }

    /* Update the selection GC */
    if (selection_gc_flag)
    {
		/* Release the old selection GC */
		XtReleaseGC(nw,np->selection_GC);
	
		/* Allocate the new selection GC */
		np->selection_GC = XfeAllocateColorGc(nw,np->selection_color,
											  None,True);
    }

    /* Update the insens top GC */
    if (insensitive_top_gc_flag)
    {
		/* Release the old label GC */
		XtReleaseGC(nw,np->insensitive_top_GC);
	
		/* Allocate the new label GC */
		np->insensitive_top_GC = 
			XfeAllocateStringGc(nw,np->font_list,
								_XfeTopShadowColor(nw),
								None,True);
    }


    /* Update the insens bottom GC */
    if (insensitive_bottom_gc_flag)
    {
		/* Release the old label GC */
		XtReleaseGC(nw,np->insensitive_bottom_GC);
	
		/* Allocate the new label GC */
		np->insensitive_bottom_GC = 
			XfeAllocateStringGc(nw,np->font_list,
								_XfeBottomShadowColor(nw),
								None,True);
    }



    return _XfePrimitiveChainSetValues(ow,rw,nw,xfeLabelWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);
    Cardinal		i;
    
    for (i = 0; i < *nargs; i++)
    {
		/* label_string */
		if (strcmp(args[i].name,XmNlabelString) == 0)
		{
			*((XtArgVal *) args[i].value) = 
				(XtArgVal) XmStringCopy(lp->label_string);
		}
		/* font_list */
		else if (strcmp(args[i].name,XmNfontList) == 0)
		{
			*((XtArgVal *) args[i].value) = 
				(XtArgVal) XmFontListCopy(lp->font_list);
		}      
    }
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
    XfeLabelPart * lp = _XfeLabelPart(w);

    *width  = _XfeOffsetLeft(w) + _XfeOffsetRight(w)  + lp->label_rect.width;
    *height = _XfeOffsetTop(w)  + _XfeOffsetBottom(w) + lp->label_rect.height;
}
/*----------------------------------------------------------------------*/
static void
PrepareComponents(Widget w,int flags)
{
    if (flags & _XFE_PREPARE_LABEL_STRING)
    {
		XfeLabelPart *	lp = _XfeLabelPart(w);
	
		/* Obtain the labels's dimensions */
		XmStringExtent(lp->font_list,
					   lp->label_string,
					   &lp->label_rect.width,
					   &lp->label_rect.height);
    }
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
/*----------------------------------------------------------------------*/
{
	/* Invoke layout_string method */
	_XfeLabelLayoutString(w);
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
	/* Invoke draw_selection method */
	_XfeLabelDrawSelection(w,event,region,clip_rect);

	/* Invoke draw_string method */
	_XfeLabelDrawString(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/
static GC
GetLabelGC(Widget w)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

	return lp->label_GC;
}
/*----------------------------------------------------------------------*/
static GC
GetSelectionGC(Widget w)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

	return lp->selection_GC;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel methods														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutString(Widget w)
/*----------------------------------------------------------------------*/
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

	/* Layout the label according to the alignment */
	switch(lp->label_alignment)
	{
	case XmALIGNMENT_BEGINNING:

		lp->label_rect.x = _XfeRectX(w);

		break;
		
	case XmALIGNMENT_CENTER:

		lp->label_rect.x = (_XfeWidth(w) - lp->label_rect.width) / 2;

		break;
		
	case XmALIGNMENT_END:

		lp->label_rect.x = 
			XfeWidth(w) - 
			lp->label_rect.width - 
			_XfeOffsetRight(w);
		
		break;
	}

	/* the label is always centered vertically */
	lp->label_rect.y = (_XfeHeight(w) - lp->label_rect.height) / 2;
}
/*----------------------------------------------------------------------*/
static void
DrawString(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);
	XRectangle		rect;
	XmString		string = NULL;
	Boolean			need_to_free_string = False;

	/* Make sure the label has something before drawing */
	if (!lp->label_rect.width || !lp->label_rect.height || !lp->label_string)
	{
		return;
	}

	/* The height and y coordinate are the same regardless of truncation */
	rect.height		= lp->label_rect.height;
	rect.y			= lp->label_rect.y;

	/* Determine whether we need to truncate the string */
	if (lp->truncate_label && (lp->label_rect.width > _XfeRectWidth(w)))
	{
		string = CreateTruncatedString(w);

		rect.width	= _XfeRectWidth(w);
		rect.x		= _XfeRectX(w);

		need_to_free_string = True;		
	}
	else
	{
		string = lp->label_string;

		rect.width	= lp->label_rect.width;
		rect.x		= lp->label_rect.x;

		need_to_free_string = False;
	}

	assert( string != NULL );

    /* Determine which gc to use to render the label */
    if (_XfeIsSensitive(w))
    {
		/* Draw the string once */
		XmStringDraw(XtDisplay(w),
					 _XfePrimitiveDrawable(w),
					 lp->font_list,
					 string,
					 _XfeLabelGetLabelGC(w),
					 rect.x + lp->misc_offset,
					 rect.y + lp->misc_offset,
					 rect.width,
					 lp->label_alignment,
					 lp->label_direction,
					 &_XfeWidgetRect(w));
    }
    else
    {
		/* Draw the string twice to achieve that cool insensitve look */
		XmStringDraw(XtDisplay(w),
					 _XfePrimitiveDrawable(w),
					 lp->font_list,
					 string,
					 lp->insensitive_top_GC,
					 rect.x + INSENSITIVE_OFFSET + lp->misc_offset,
					 rect.y + INSENSITIVE_OFFSET + lp->misc_offset,
					 rect.width,
					 lp->label_alignment,
					 lp->label_direction,
					 &_XfeWidgetRect(w));

		XmStringDraw(XtDisplay(w),
					 _XfePrimitiveDrawable(w),
					 lp->font_list,
					 string,
					 lp->insensitive_bottom_GC,
					 rect.x + lp->misc_offset,
					 rect.y + lp->misc_offset,
					 rect.width,
					 lp->label_alignment,
					 lp->label_direction,
					 &_XfeWidgetRect(w));
    }

	/* Free the string if needed */
	if (need_to_free_string)
	{
		XmStringFree(string);
	}
}
/*----------------------------------------------------------------------*/
static void
DrawSelection(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);
	Dimension		dx;
	Dimension		dy;

	/* Make sure the widget is selected */
	if (!lp->selected)
	{
		return;
	}

	/* Make sure the label has something before drawing */
	if (!lp->label_rect.width || !lp->label_rect.height || !lp->label_string)
	{
		return;
	}

	/* Compute the extra offset for the selection */
	dx = XfeMin(_XfeMarginLeft(w),_XfeMarginRight(w));
	dx = XfeMin(dx,1);

	dy = XfeMin(_XfeMarginTop(w),_XfeMarginBottom(w));
	dy = XfeMin(dy,1);

	XFillRectangle(XtDisplay(w),
				   _XfePrimitiveDrawable(w),
				   _XfeLabelGetSelectionGC(w),
				   lp->label_rect.x - dx,
				   lp->label_rect.y - dy,
				   lp->label_rect.width + 2 * dx,
				   lp->label_rect.height + 2 * dy);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel Method invocation functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeLabelLayoutString(Widget w)
{
	XfeLabelWidgetClass	lc = (XfeLabelWidgetClass) XtClass(w);

	if (lc->xfe_label_class.layout_string)
	{
		(*lc->xfe_label_class.layout_string)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeLabelDrawString(Widget			w,
				   XEvent *			event,
				   Region			region,
				   XRectangle *		clip_rect)
{
	XfeLabelWidgetClass	lc = (XfeLabelWidgetClass) XtClass(w);

	if (lc->xfe_label_class.draw_string)
	{
		(*lc->xfe_label_class.draw_string)(w,event,region,clip_rect);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeLabelDrawSelection(Widget			w,
					   XEvent *			event,
					   Region			region,
					   XRectangle *		clip_rect)
{
	XfeLabelWidgetClass	lc = (XfeLabelWidgetClass) XtClass(w);

	if (lc->xfe_label_class.draw_selection)
	{
		(*lc->xfe_label_class.draw_selection)(w,event,region,clip_rect);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ GC
_XfeLabelGetLabelGC(Widget 	w)
{
	XfeLabelWidgetClass	lc = (XfeLabelWidgetClass) XtClass(w);

	assert( lc->xfe_label_class.get_label_gc != NULL );

	return (*lc->xfe_label_class.get_label_gc)(w);
}
/*----------------------------------------------------------------------*/
/* extern */ GC
_XfeLabelGetSelectionGC(Widget 	w)
{
	XfeLabelWidgetClass	lc = (XfeLabelWidgetClass) XtClass(w);

	assert( lc->xfe_label_class.get_selection_gc != NULL );

	return (*lc->xfe_label_class.get_selection_gc)(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeLabel functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmString
CreateTruncatedString(Widget w)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);
	XmString		result = NULL;

	/* Make sure the truncate proc is good */
	if (lp->truncate_proc)
	{
		String		psz_tmp;

		/* Access the label as a psz String */
		XmStringGetLtoR(lp->label_string,XmFONTLIST_DEFAULT_TAG,&psz_tmp);

		if (psz_tmp)
		{
			Dimension max_width = _XfeRectWidth(w) - lp->misc_offset;

			if (!_XfeIsSensitive(w))
			{
				max_width -= 1;
			}
			
			/* Create a truncated string */
			result = (*lp->truncate_proc)(psz_tmp,
										  XmFONTLIST_DEFAULT_TAG,
										  lp->font_list,
										  max_width);

			/* Free the psz String */
			XtFree(psz_tmp);
		}
	}
	else
	{
		_XfeWarning(w,MESSAGE2);
	}

	return result;
}
/*----------------------------------------------------------------------*/
static void
InvokeSelectionChangedCallback(Widget w,XEvent * event)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

    /* Invoke the callbacks only if needed */
	if (lp->selection_changed_callback)
    {
		XfeLabelSelectionChangedCallbackStruct cbs;
	
		cbs.event		= event;
		cbs.reason		= XmCR_SELECTION_CHANGED;
		cbs.selected	= lp->selected;

		/* Flush the display */
		XFlush(XtDisplay(w));
		
		/* Invoke the Callback List */
		XtCallCallbackList(w,lp->selection_changed_callback,&cbs);
    }
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */  void
_XfeLabelBtn1Down(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	/* Look for modifiers that matche the selection modifiers */
	if (_XfeLabelAcceptSelectionEvent(w,event,True))
	{
		_XfeLabelSetSelected(w,event,!lp->selected,True);

        return;
    }

	/* Look for modifiers that matche the edit modifiers */
	if (_XfeLabelAcceptEditEvent(w,event,True))
	{
      _XfeLabelEdit(w,event,params,nparams);
    }
}
/*----------------------------------------------------------------------*/
/* extern */  void
_XfeLabelSelect(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

	_XfeLabelSetSelected(w,event,!lp->selected,True);
}
/*----------------------------------------------------------------------*/
/* extern */  void
_XfeLabelEdit(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

	/* Make sure parent is XfeToolBar */
	if (!XfeIsToolBar(_XfeParent(w)))
	{
		_XfeWarning(w,MESSAGE3);

		return;
	}

	/* Ask the toolbar to edit us */
	XfeToolBarEditItem(_XfeParent(w),
					   w,
					   lp->label_rect.x,
					   lp->label_rect.y,
					   lp->label_rect.width,
					   lp->label_rect.height);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel private functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */  Boolean
_XfeLabelAcceptSelectionEvent(Widget w,XEvent * event,Boolean inside_label)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);
	Boolean			result = False;

	/* Look for modifiers that matches the selection modifiers */
	if ((XfeEventGetModifiers(event) & lp->selection_modifiers) || 
		(lp->selection_modifiers == AnyModifier))
	{
		/* Make sure the event's x,y occured inside the label if needed */
		if (inside_label)
		{
			int	x;
			int	y;

			/* Determine if the pointer is in the label */
			result = (XfeEventGetXY(event,&x,&y) && 
					  XfePointInRect(&lp->label_rect,x,y));
		}
		else
		{
			result = True;
		}
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */  Boolean
_XfeLabelAcceptEditEvent(Widget w,XEvent * event,Boolean inside_label)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);
	Boolean			result = False;

	/* Look for modifiers that matches the edit modifiers */
	if ((XfeEventGetModifiers(event) & lp->edit_modifiers) || 
		(lp->edit_modifiers == AnyModifier))
	{
		/* Make sure the event's x,y occured inside the label if needed */
		if (inside_label)
		{
			int	x;
			int	y;

			/* Determine if the pointer is in the label */
			result = (XfeEventGetXY(event,&x,&y) && 
					  XfePointInRect(&lp->label_rect,x,y));
		}
		else
		{
			result = True;
		}
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */  void
_XfeLabelSetSelected(Widget		w,
					 XEvent *	event,
					 Boolean	selected,
					 Boolean	invoke_callbacks)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

	assert( XfeIsLabel(w) );

	/* Make sure the selection has changed */
	if (lp->selected == selected)
	{
		return;
	}

	/* Assign the new selection */
	lp->selected = selected;

	/* Make sure the widget is alive and kicking before drawing */
	if (_XfeIsRealized(w) && _XfeIsManaged(w))
	{
		/* Erase the old stuff */
		_XfePrimitiveClearBackground(w);
		
		/* Redraw the buffer */
		_XfePrimitiveDrawEverything(w,NULL,NULL);
		
		/* Draw the buffer onto the window */
		XfeExpose(w,NULL,NULL);
	}

	/* Invoke the selection callbacks if needed */
	if (invoke_callbacks)
	{
		InvokeSelectionChangedCallback(w,event);
	}
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel Public Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateLabel(Widget pw,char * name,ArgList av,Cardinal ac)
{
    return XtCreateWidget(name,xfeLabelWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeLabelSetString(Widget w,XmString xm_label)
{
    XfeLabelPart *	lp = _XfeLabelPart(w);

    assert( _XfeIsAlive(w) );

	/* Make sure the label is different */
	if (XmStringByteCompare(xm_label,lp->label_string))
	{
		return;
	}

    /* Free the old string */
    XmStringFree(lp->label_string);

    /* Make a copy of the given string or set it to "" if null */
	if (xm_label)
	{
		lp->label_string = XfeXmStringCopy(w,xm_label,XtName(w));
	}
	else
	{
		lp->label_string = XmStringCreate("",XmFONTLIST_DEFAULT_TAG);
	}


	/* Prepare the label */
	_XfePrimitivePrepareComponents(w,_XFE_PREPARE_LABEL_STRING);

    /* Relayout the widget */
    _XfePrimitiveLayoutComponents(w);

	/* Make sure the widget is realized before drawing anything */
	if (!XtIsRealized(w))
	{
		return;
	}

    /* Erase the old stuff */
    _XfePrimitiveClearBackground(w);

    /* Redraw the buffer */
    _XfePrimitiveDrawEverything(w,NULL,NULL);

    /* Draw the buffer onto the window */
    XfeExpose(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeLabelSetStringPSZ(Widget w,String psz_label)
{
    XfeLabelPart *		lp = _XfeLabelPart(w);
	XmString			xm_new_label;

    assert( _XfeIsAlive(w) );

	/* Set the new string */
	if (psz_label)
	{
		xm_new_label = XmStringCreate(psz_label,XmFONTLIST_DEFAULT_TAG);
	}
	else
	{
		xm_new_label = XmStringCreate("",XmFONTLIST_DEFAULT_TAG);
	}

	/* Make sure the label is different */
	if (XmStringByteCompare(xm_new_label,lp->label_string))
	{
		XmStringFree(xm_new_label);

		return;
	}

    /* Free the old string */
    XmStringFree(lp->label_string);

	lp->label_string = xm_new_label;

	/* Prepare the label */
	_XfePrimitivePrepareComponents(w,_XFE_PREPARE_LABEL_STRING);

    /* Relayout the widget */
    _XfePrimitiveLayoutComponents(w);

	/* Make sure the widget is realized before drawing anything */
	if (!XtIsRealized(w))
	{
		return;
	}

    /* Erase the old stuff */
    _XfePrimitiveClearBackground(w);

    /* Redraw the buffer */
    _XfePrimitiveDrawEverything(w,NULL,NULL);

    /* Draw the buffer onto the window */
    XfeExpose(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
/* extern */  void
XfeLabelSetSelected(Widget w,Boolean selected)
{
	assert( XfeIsLabel(w) );

	_XfeLabelSetSelected(w,NULL,selected,False);
}
/*----------------------------------------------------------------------*/
