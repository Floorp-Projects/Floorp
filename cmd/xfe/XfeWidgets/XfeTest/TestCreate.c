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
/* Name:		<XfeTest/TestCreate.c>									*/
/* Description:	Xfe widget tests convenience creation funcs.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeTest.h>

static String tool_names[] = 
{
	"back",
	"forward",
	"home",
	"loadimages",
	"mixsecurity",
	"places",
	"print",
	"reload",
	"search",
	"secure",
	"stop",
	"unsecure"
};

#define num_tool_names XtNumber(tool_names)

/*----------------------------------------------------------------------*/
/* extern */ void
XfeLoadToolBar(Widget			tool_bar,
			   String			item_prefix,
			   Cardinal			tool_count,
			   Cardinal			sep_count,
			   XtCallbackProc	arm_cb,
			   XtCallbackProc	disarm_cb,
			   XtCallbackProc	activate_cb,
			   WidgetList *		tool_items_out)
{
    Cardinal		i;
	WidgetList		tool_items = NULL;
	int				sep_offset = (int) tool_count / ((int) sep_count + 1);
	unsigned char	orientation;

	assert( XfeIsAlive(tool_bar) );
	assert( item_prefix != NULL );
	assert( tool_count > 0 );

	if (sep_count > 0)
	{
		XtVaGetValues(tool_bar,XmNorientation,&orientation,NULL);
	}

	tool_items = XfeMalloc(Widget,tool_count);

	XtVaSetValues(tool_bar,XmNignoreConfigure,True,NULL);
    
    for (i = 0; i < tool_count; i++)
    {
		int		index = i % num_tool_names;
		char	name_icon[1024];
		char	name_raised[1024];
		char	name_armed[1024];

		sprintf(name_icon,"tb_%s",tool_names[index]);
		sprintf(name_armed,"tb_%s_armed",tool_names[index]);
		sprintf(name_raised,"tb_%s_raised",tool_names[index]);

		tool_items[i] = XtVaCreateManagedWidget(
			XfeNameIndex(item_prefix,i + 1),
			xfeButtonWidgetClass,
			tool_bar,
			XmNbackground,			XfeBackground(tool_bar),
			XmNpixmap,				XfeGetPixmap(tool_bar,name_icon),
			XmNpixmapMask,			XfeGetMask(tool_bar,name_icon),
			XmNarmedPixmap,			XfeGetPixmap(tool_bar,name_armed),
			XmNarmedPixmapMask,		XfeGetMask(tool_bar,name_armed),
			XmNraisedPixmap,		XfeGetPixmap(tool_bar,name_raised),
			XmNraisedPixmapMask,	XfeGetMask(tool_bar,name_raised),
			XmNallowDrag,			True,
			NULL);
		
		if (arm_cb)
		{
			XtAddCallback(tool_items[i],XmNarmCallback,arm_cb,NULL);
		}
		
		if (disarm_cb)
		{
			XtAddCallback(tool_items[i],XmNdisarmCallback,disarm_cb,NULL);
		}
		
		if (activate_cb)
		{
			XtAddCallback(tool_items[i],XmNactivateCallback,activate_cb,NULL);
		}

        if (sep_count > 0)
        {
          if ((i % sep_offset) == 0)
          {
			  Widget sep = XmCreateSeparator(tool_bar,
											 (orientation == XmVERTICAL) ? 
											 "vsep" : "hsep",
											 NULL,0);
			  
			  XtManageChild(sep);
		  }
		}
    }

	XtVaSetValues(tool_bar,XmNignoreConfigure,False,NULL);

	XfeManagerLayout(tool_bar);

	if (tool_items_out)
		*tool_items_out = tool_items;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateLoadedToolBar(Widget			pw,
					   String			name,
					   String			item_prefix,
					   Cardinal			tool_count,
					   Cardinal			sep_count,
					   XtCallbackProc	arm_cb,
					   XtCallbackProc	disarm_cb,
					   XtCallbackProc	activate_cb,
					   WidgetList *		tool_items_out)
{
    Widget		tool_bar;

	assert( XfeIsAlive(pw) );
	assert( name != NULL );
	assert( item_prefix != NULL );
    
    tool_bar = XtVaCreateManagedWidget(
		name,
		xfeToolBarWidgetClass,
		pw,
		XmNbackground,		XfeBackground(pw),
/* 		XmNbuttonLayout,	XmBUTTON_LABEL_ON_BOTTOM, */
		NULL);

	if (tool_count > 0)
	{
		XfeLoadToolBar(tool_bar,
					   item_prefix,
					   tool_count,
					   sep_count,
					   arm_cb,
					   disarm_cb,
					   activate_cb,
					   tool_items_out);
	}

    return tool_bar;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateLoadedDashBoard(Widget				pw,
						 String				name,
						 String				tool_prefix,
						 XtCallbackProc		tool_cb,
						 Cardinal			tool_count,
						 String				task_prefix,
						 Boolean			task_large,
						 XtCallbackProc		task_cb,
						 Cardinal			task_count,
						 Widget *			tool_bar_out,
						 Widget *			progress_bar_out,
						 Widget *			status_bar_out,
						 Widget *			task_bar_out,
						 WidgetList *		tool_items_out,
						 WidgetList *		task_items_out)
{
	Widget			dash_board = NULL;
	Widget			tool_bar = NULL;
	Widget			status_bar = NULL;
	Widget			progress_bar = NULL;
	Widget			task_bar = NULL;

	assert( XfeIsAlive(pw) );
	assert( name != NULL );

	dash_board = XtVaCreateManagedWidget(name,
										 xfeDashBoardWidgetClass,
										 pw,
										 NULL);

	tool_bar = XtVaCreateManagedWidget("ToolBar",
									   xfeToolBarWidgetClass,
									   dash_board,
									   NULL);

	XfeLoadToolBar(tool_bar,
				   tool_prefix,
				   tool_count,
				   tool_count / 10,
				   NULL,
				   NULL,
				   tool_cb,
				   tool_items_out);

	progress_bar = XtVaCreateManagedWidget("ProgressBar",
										   xfeProgressBarWidgetClass,
										   dash_board,
										   NULL);

	status_bar = XtVaCreateManagedWidget("StatusBar",
										 xfeLabelWidgetClass,
										 dash_board,
										 NULL);

	task_bar = XfeCreateLoadedTaskBar(dash_board,
									  "TaskBar",
									  task_large,
									  task_prefix,
									  task_cb,
									  task_count,
									  task_items_out);
	if (tool_bar_out)
		*tool_bar_out = tool_bar;

	if (progress_bar_out)
		*progress_bar_out = progress_bar;


	if (status_bar_out)
		*status_bar_out = status_bar;

	return dash_board;
}
/*----------------------------------------------------------------------*/

static String task_names[] =
{
	"browser",
	"mail",
	"news",
	"editor"
};

#define num_task_names XtNumber(task_names)

/*----------------------------------------------------------------------*/
/* extern */ void
XfeLoadTaskBar(Widget			task_bar,
			   Boolean			large,
			   String			task_prefix,
			   XtCallbackProc	task_cb,
			   Cardinal			task_count,
			   WidgetList *		task_items_out)
{
	WidgetList		task_items = NULL;
	Cardinal		i;

	assert( XfeIsAlive(task_bar) );
	assert( task_count > 0 );

	task_items = XfeMalloc(Widget,task_count);
	
	XtVaSetValues(task_bar,XmNignoreConfigure,True,NULL);

	for(i = 0; i < task_count; i++)
	{
		int		index = i % num_task_names;
		char	name_icon[1024];
		char	name_raised[1024];
		char	name_armed[1024];
		String	suffix = large ? "" : "small_";

		sprintf(name_icon,"task_%s%s",suffix,task_names[index]);
		sprintf(name_armed,"task_%s%s_armed",suffix,task_names[index]);
		sprintf(name_raised,"task_%s%s_raised",suffix,task_names[index]);
		
		task_items[i] = 
			XtVaCreateManagedWidget(XfeNameIndex(task_prefix,i + 1),
									xfeButtonWidgetClass,
									task_bar,
									XmNbackground,	XfeBackground(task_bar),
									NULL);
		
		XtVaSetValues(
			task_items[i],
			XmNpixmap,				XfeGetPixmap(task_bar,name_icon),
			XmNpixmapMask,			XfeGetMask(task_bar,name_icon),
			XmNarmedPixmap,			XfeGetPixmap(task_bar,name_armed),
			XmNarmedPixmapMask,		XfeGetMask(task_bar,name_armed),
			XmNraisedPixmap,		XfeGetPixmap(task_bar,name_raised),
			XmNraisedPixmapMask,	XfeGetMask(task_bar,name_raised),
			NULL);
		
		if (task_cb)
		{
			XtAddCallback(task_items[i],XmNactivateCallback,task_cb,NULL);
		}
	}

	XtVaSetValues(task_bar,XmNignoreConfigure,False,NULL);

	XfeManagerLayout(task_bar);

	if (task_items_out)
		*task_items_out = task_items;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateLoadedTaskBar(Widget			pw,
					   String			name,
					   Boolean			large,
					   String			task_prefix,
					   XtCallbackProc	task_cb,
					   Cardinal			task_count,
					   WidgetList *		task_items_out)
{
	Widget			task_bar = NULL;
	unsigned char	layout = 
		large ? XmBUTTON_LABEL_ON_BOTTOM : XmBUTTON_PIXMAP_ONLY;

	assert( XfeIsAlive(pw) );
	assert( name != NULL );

	task_bar = XtVaCreateManagedWidget(
		"TaskBar",
		xfeTaskBarWidgetClass,
		pw,
		XmNbackground,			XfeBackground(pw),
		XmNbuttonLayout,		layout,
		NULL);


	XtVaSetValues(task_bar,
				  XmNactionPixmap,	XfeGetPixmap(task_bar,"task_small_handle"),
				  NULL);

	if (task_count)
	{
		XfeLoadTaskBar(task_bar,
					   large,
					   task_prefix,
					   task_cb,
					   task_count,
					   task_items_out);
	}

	return task_bar;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateLoadedToolBox(Widget pw,String name,ArgList av,Cardinal ac)
{
	Widget tb;

	assert( XfeIsAlive(pw) );
	assert( name != NULL );

	tb = XtCreateManagedWidget(name,xfeToolBoxWidgetClass,pw,av,ac);

#if 0
	XtAddCallback(tb,XmNopenCallback,XfeToolBoxOpenCallback,NULL);
	XtAddCallback(tb,XmNcloseCallback,XfeToolBoxCloseCallback,NULL);
#endif

#if 0	
	XtAddCallback(tb,XmNsnapCallback,snap_cb,NULL);
	XtAddCallback(tb,XmNswapCallback,swap_cb,NULL);
	XtAddCallback(tb,XmNnewItemCallback,new_item_cb,NULL);
#endif

	XtVaSetValues(
		tb,
		XmNbottomPixmap,			XfeGetPixmap(tb,"dtb_bottom"),
		XmNhorizontalPixmap,		XfeGetPixmap(tb,"dtb_horizontal"),
		XmNleftPixmap,				XfeGetPixmap(tb,"dtb_left"),
		XmNrightPixmap,				XfeGetPixmap(tb,"dtb_right"),
		XmNtopPixmap,				XfeGetPixmap(tb,"dtb_top"),
		XmNverticalPixmap,			XfeGetPixmap(tb,"dtb_vertical"),
		XmNbottomRaisedPixmap,		XfeGetPixmap(tb,"dtb_bottom_raised"),
		XmNhorizontalRaisedPixmap,	XfeGetPixmap(tb,"dtb_horizontal_raised"),
		XmNleftRaisedPixmap,		XfeGetPixmap(tb,"dtb_left_raised"),
		XmNrightRaisedPixmap,		XfeGetPixmap(tb,"dtb_right_raised"),
		XmNtopRaisedPixmap,			XfeGetPixmap(tb,"dtb_top_raised"),
		XmNverticalRaisedPixmap,	XfeGetPixmap(tb,"dtb_vertical_raised"),
		XmNdragCursor,				XfeCursorGetDragHand(tb),

		NULL);

	return tb;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateLoadedTab(Widget pw,String name,ArgList av,Cardinal ac)
{
	Widget	tab;
	Pixel	background;
	Pixel	foreground;
	Pixel	raise_background;
	Pixel	raise_foreground;

	assert( XfeIsAlive(pw) );
	assert( name != NULL );

	tab = XtCreateManagedWidget(name,xfeTabWidgetClass,pw,av,ac);

	XtVaGetValues(tab,
				  XmNbackground,		&background,
				  XmNforeground,		&foreground,
				  XmNraiseBackground,	&raise_background,
				  XmNraiseForeground,	&raise_foreground,
				  NULL);

	XtVaSetValues(
		tab,
		XmNbottomPixmap,			XfeGetPixmap(tab,"dtb_bottom"),
		XmNhorizontalPixmap,		XfeGetPixmap(tab,"dtb_horizontal"),
		XmNleftPixmap,				XfeGetPixmap(tab,"dtb_left"),
		XmNrightPixmap,				XfeGetPixmap(tab,"dtb_right"),
		XmNtopPixmap,				XfeGetPixmap(tab,"dtb_top"),
		XmNverticalPixmap,			XfeGetPixmap(tab,"dtb_vertical"),
		XmNbottomRaisedPixmap,		XfeGetPixmap(tab,"dtb_bottom_raised"),
		XmNhorizontalRaisedPixmap,	XfeGetPixmap(tab,"dtb_horizontal_raised"),
		XmNleftRaisedPixmap,		XfeGetPixmap(tab,"dtb_left_raised"),
		XmNrightRaisedPixmap,		XfeGetPixmap(tab,"dtb_right_raised"),
		XmNtopRaisedPixmap,			XfeGetPixmap(tab,"dtb_top_raised"),
		XmNverticalRaisedPixmap,	XfeGetPixmap(tab,"dtb_vertical_raised"),

		NULL);

	return tab;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateFormAndButton(Widget			pw,
					   String			name,
					   String			button_name,
					   Dimension		offset,
					   Boolean			use_gadget,
					   ArgList			av,
					   Cardinal			ac)
{
    Widget		form;
    Widget		button;

	assert( XfeIsAlive(pw) );
	assert( name != NULL );
	assert( button_name != NULL );
    
	form = XtCreateManagedWidget(name,xmFormWidgetClass,pw,av,ac);
	
    button = XtVaCreateManagedWidget(button_name,
									 (use_gadget ? 
									  xmPushButtonGadgetClass :
									  xmPushButtonWidgetClass),
									 form,
									 XmNleftAttachment,			XmATTACH_FORM,
									 XmNrightAttachment,		XmATTACH_FORM,
									 XmNtopAttachment,			XmATTACH_FORM,
									 XmNbottomAttachment,		XmATTACH_FORM,
									 XmNleftOffset,				offset,
									 XmNrightOffset,			offset,
									 XmNtopOffset,				offset,
									 XmNbottomOffset,			offset,
									 XmNbackground,		XfeBackground(form),
									 NULL);

    return form;
}
/*----------------------------------------------------------------------*/
Widget
XfeCreateManagedForm(Widget parent,String name,ArgList args,Cardinal n)
{
	return XtCreateManagedWidget(name,xmFormWidgetClass,parent,args,n);
}
/*----------------------------------------------------------------------*/
Widget
XfeCreateManagedFrame(Widget parent,String name,ArgList args,Cardinal n)
{
	return XtCreateManagedWidget(name,xmFrameWidgetClass,parent,args,n);
}
/*----------------------------------------------------------------------*/
Widget
XfeCreateManagedRowColumn(Widget parent,String name,ArgList args,Cardinal n)
{
	return XtCreateManagedWidget(name,xmRowColumnWidgetClass,parent,args,n);
}
/*----------------------------------------------------------------------*/
