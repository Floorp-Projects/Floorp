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
/* Name:		<Xfe/FontChooser.c>										*/
/* Description:	XfeFontChooser widget source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/FontChooserP.h>
#include <Xfe/ManagerP.h>

#include <Xm/ToggleB.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeFontChooser."

#define FONT_CHOOSER_ITEM_NAME "FontChooserItemName"

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
/* Misc XfeFontChooser functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmFontList	FontListAccess		(Widget,Cardinal);
static XmString		StringAccess		(Widget,Cardinal);
static void			UpdateItems			(Widget);
static void			UpdateFonts			(Widget);
static void			UpdateLabels		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Item callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			ItemArmCB			(Widget,XtPointer,XtPointer);
static void			ItemValueChangedCB	(Widget,XtPointer,XtPointer);
static void			ItemDisarmCB		(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooser Resources												*/
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
		XtOffsetOf(XfeFontChooserRec,xfe_font_chooser . selection_changed_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },

    /* Font item resources */
    { 
		XmNnumFontItems,
		XmCNumFontItems,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeFontChooserRec , xfe_font_chooser . num_font_items),
		XmRImmediate, 
 		(XtPointer) 0
    },
    { 
		XmNfontItemLabels,
		XmCFontItemLabels,
		XmRXmStringTable,
		sizeof(XmString *),
		XtOffsetOf(XfeFontChooserRec , xfe_font_chooser . font_item_labels),
		XmRImmediate, 
 		(XtPointer) NULL
    },
    { 
		XmNfontItemFonts,
		XmCFontItemFonts,
		XmRFontList,
		sizeof(XmFontList *),
		XtOffsetOf(XfeFontChooserRec , xfe_font_chooser . font_item_fonts),
		XmRImmediate, 
 		(XtPointer) NULL
    },

	/* Force mapping_delay to 0 */
    { 
		XmNmappingDelay,
		XmCMappingDelay,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeFontChooserRec , xfe_cascade . mapping_delay),
		XmRImmediate, 
 		(XtPointer) 0
    },

	/* Force sub_menu_alignment to XmALIGNMENT_END */
    { 
		XmNsubMenuAlignment,
		XmCSubMenuAlignment,
		XmRAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfeFontChooserRec , xfe_cascade . sub_menu_alignment),
		XmRImmediate, 
		(XtPointer) XmALIGNMENT_END
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooser widget class record initialization					*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(fontchooser,FontChooser) =
{
    {
		/* Core Part */
		(WidgetClass) &xfeCascadeClassRec,		/* superclass         	*/
		"XfeFontChooser",						/* class_name         	*/
		sizeof(XfeFontChooserRec),				/* widget_size        	*/
		NULL,									/* class_initialize   	*/
		NULL,									/* class_part_initialize*/
		FALSE,                                  /* class_inited       	*/
		Initialize,                             /* initialize         	*/
		NULL,                                   /* initialize_hook    	*/
		XtInheritRealize,                       /* realize            	*/
		NULL,									/* actions            	*/
		0,										/* num_actions        	*/
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
		NULL,									/* syn resources      	*/
		0,										/* num syn_resources  	*/
		NULL,									/* extension          	*/
    },
	
    /* XfePrimitive Part */
    {
		XfeInheritBitGravity,					/* bit_gravity			*/
		XfeInheritPreferredGeometry,			/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		NULL,									/* prepare_components	*/
		XfeInheritLayoutComponents,				/* layout_components	*/
		XfeInheritDrawBackground,				/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		XfeInheritDrawComponents,				/* draw_components		*/
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

    /* XfeFontChooser Part */
    {
		NULL,									/* extension            */
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeFontChooserWidgetClass declaration.								*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(fontchooser,FontChooser);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeFontChooserPart *	fp = _XfeFontChooserPart(nw);
    XfeCascadePart *		cp = _XfeCascadePart(nw);

	XtVaSetValues(cp->sub_menu_id,XmNradioBehavior,True,NULL);

	UpdateItems(nw);
	UpdateLabels(nw);
	UpdateFonts(nw);

    /* Finish of initialization */
    _XfePrimitiveChainInitialize(rw,nw,xfeFontChooserWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeFontChooserPart *	cp = _XfeFontChooserPart(w);

    /* Remove all CallBacks */
    /* XtRemoveAllCallbacks(w,XmNselectionCallback); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeFontChooserPart *	np = _XfeFontChooserPart(nw);
    XfeFontChooserPart *	op = _XfeFontChooserPart(ow);

    /* num_font_items */
    if (np->num_font_items != op->num_font_items)
    {
		UpdateItems(nw);

		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    /* font_item_labels */
    if (np->font_item_labels != op->font_item_labels)
    {
		UpdateLabels(nw);

		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    /* font_item_fonts */
    if (np->font_item_fonts != op->font_item_fonts)
    {
		UpdateFonts(nw);

		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    return _XfePrimitiveChainSetValues(ow,rw,nw,xfeFontChooserWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeFontChooser functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmFontList
FontListAccess(Widget w,Cardinal i)
{
    XfeFontChooserPart *	cp = _XfeFontChooserPart(w);
    XfeLabelPart *			lp = _XfeLabelPart(w);

	assert( i < cp->num_font_items );

	if (cp->font_item_fonts && cp->font_item_fonts[i])
	{
		return cp->font_item_fonts[i];
	}

	return lp->font_list;
}
/*----------------------------------------------------------------------*/
static XmString
StringAccess(Widget w,Cardinal i)
{
    XfeFontChooserPart *	cp = _XfeFontChooserPart(w);

	assert( i < cp->num_font_items );

	if (cp->font_item_labels && cp->font_item_labels[i])
	{
		return cp->font_item_labels[i];
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
static void
UpdateItems(Widget w)
{
    XfeFontChooserPart *	cp = _XfeFontChooserPart(w);
	Cardinal				i;
	Widget					sub_menu_id = XfeCascadeGetSubMenu(w);
	Cardinal				num_available = _XfemNumChildren(sub_menu_id);

	/* No need to add or remove items */
	if (num_available == cp->num_font_items)
	{
		return;
	}

	/* Too many items available - kill the extra */
	if (num_available > cp->num_font_items)
	{
		for(i = cp->num_font_items; i < num_available; i++)
		{
			XtDestroyWidget(_XfeChildrenIndex(sub_menu_id,i));
		}
	}
	/* Not enough items available - add the needed extras */
	else
	{
		for(i = num_available; i < cp->num_font_items; i++)
		{
			Arg				av[10];
			Cardinal		ac = 0;
			Widget			item;

			XtSetArg(av[ac],XmNindicatorType,	XmONE_OF_MANY);		ac++;
			
			item = XtCreateManagedWidget(FONT_CHOOSER_ITEM_NAME,
										 xmToggleButtonWidgetClass,
										 sub_menu_id,av,ac);
			
			XtAddCallback(item,XmNarmCallback,ItemArmCB,w);
			XtAddCallback(item,XmNdisarmCallback,ItemDisarmCB,w);
			XtAddCallback(item,XmNvalueChangedCallback,ItemValueChangedCB,w);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
UpdateFonts(Widget w)
{
    XfeFontChooserPart *	cp = _XfeFontChooserPart(w);
	Cardinal				i;
	Widget					sub_menu_id = XfeCascadeGetSubMenu(w);

	for(i = 0; i < cp->num_font_items; i++)
	{
		Widget			item = _XfeChildrenIndex(sub_menu_id,i);
		XmFontList		font_list = FontListAccess(w,i);

		if (_XfeIsAlive(item) && font_list)
		{
			XtVaSetValues(item,XmNfontList,font_list,NULL);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
UpdateLabels(Widget w)
{
    XfeFontChooserPart *	cp = _XfeFontChooserPart(w);
	Cardinal				i;
	Widget					sub_menu_id = XfeCascadeGetSubMenu(w);
	
	for(i = 0; i < cp->num_font_items; i++)
	{
		Widget			item = _XfeChildrenIndex(sub_menu_id,i);
		XmString		xm_string = StringAccess(w,i);

		if (_XfeIsAlive(item) && xm_string)
		{
			XtVaSetValues(item,XmNlabelString,xm_string,NULL);
		}
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Item callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ItemArmCB(Widget item,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
	XfeFontChooserPart *	cp = _XfeFontChooserPart(w);

}
/*----------------------------------------------------------------------*/
static void
ItemValueChangedCB(Widget item,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
	XfeFontChooserPart *	cp = _XfeFontChooserPart(w);

}
/*----------------------------------------------------------------------*/
static void
ItemDisarmCB(Widget item,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
	XfeFontChooserPart *	cp = _XfeFontChooserPart(w);

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooser Public Methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateFontChooser(Widget pw,char * name,Arg * av,Cardinal ac)
{
    return XtCreateWidget(name,xfeFontChooserWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeFontChooserDestroyChildren(Widget w)
{
#if 0
    XfeFontChooserPart *	cp = _XfeFontChooserPart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsFontChooser(w) );
	assert( _XfeIsAlive(cp->sub_menu_id) );

	XfeChildrenDestroy(cp->sub_menu_id);
#endif
}
/*----------------------------------------------------------------------*/
