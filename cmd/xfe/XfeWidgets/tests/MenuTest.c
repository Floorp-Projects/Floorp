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
/* Name:		MenuTest.c												*/
/* Description:	Test for XfeBmButton and XfeBmCascade widgets.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include <XfeTest/XfeTest.h>

#include <Xm/RowColumnP.h>


static Widget	create_pulldown				(Widget,String);
static Widget	create_sep					(Widget,String);
static Widget	create_bm_pulldown			(Widget,String);
static Widget	create_item					(Widget,String);
static Widget	create_cascade_alone_one	(Widget parent,String name);
static Widget	create_cascade_alone_two	(Widget parent,String name);
static void		activate_callback			(Widget,XtPointer,XtPointer);
static Widget	get_sub_menu_id				(Widget);

static void		post_callback				(Widget,XtPointer,XtPointer);
static void		event_loop					(void);

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;
    Widget	menu;
	Widget	pane1;
	Widget	pane2;
	Widget	pane3;
	
	XfeAppCreateSimple("MenuTest",&argc,argv,"MainFrame",&frame,&form);

	menu = XmCreateMenuBar(form,"Menu",NULL,0);

	pane1 = create_bm_pulldown(menu,"File");

#if 1
	pane2 = create_bm_pulldown(menu,"Options");
#else
	pane2 = create_pulldown(menu,"Options");
#endif

	create_item(pane1,"Open");
	create_item(pane1,"Close");
	create_item(pane1,"Save");
	create_sep(pane1,"Sep2");
	create_item(pane1,"SaveAs");
	create_item(pane1,"Exit");

	create_item(pane2,"Browser");
	create_item(pane2,"Mail");
	create_item(pane2,"News");
	create_item(pane2,"Editor");

	pane3 = create_bm_pulldown(pane2,"More");

	create_item(pane3,"Browser");
	create_item(pane3,"Mail");
	create_item(pane3,"News");
	create_item(pane3,"Editor");

	create_cascade_alone_one(form,"CascadeAlone1");

	create_cascade_alone_two(form,"CascadeAlone2");

	XtManageChild(menu);

	{
		Widget pb = XmCreatePushButton(form,"Caca",NULL,0);

		XtManageChild(pb);
	}

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_pulldown(Widget parent,String name)
{
	Widget cascade;
	Widget pane;

	pane = XmCreatePulldownMenu(parent,"_Pane",NULL,0);

	cascade = XtVaCreateManagedWidget(name,
									  xmCascadeButtonWidgetClass,
									  parent,
									  XmNsubMenuId,		pane,
									  NULL);


/* 	XtVaSetValues(pane,XmNtearOffModel,XmTEAR_OFF_ENABLED,NULL); */

	return pane;
}
/*----------------------------------------------------------------------*/
static Widget
create_bm_pulldown(Widget parent,String name)
{
	Widget cascade;
	Widget pane;

	static Pixmap			arm_pixmap = XmUNSPECIFIED_PIXMAP;
	static Pixmap			pixmap = XmUNSPECIFIED_PIXMAP;


	pane = XmCreatePulldownMenu(parent,"_Pane",NULL,0);

	cascade = 
		XtVaCreateManagedWidget(name,
								xfeBmCascadeWidgetClass,
								parent,
								XmNbuttonLayout,	XmBUTTON_LABEL_ON_RIGHT,
								XmNsubMenuId,		pane,
								NULL);

	XtAddCallback(cascade,XmNactivateCallback,activate_callback,NULL);

/* 	XtVaSetValues(pane,XmNtearOffModel,XmTEAR_OFF_ENABLED,NULL); */

	if (!XfePixmapGood(pixmap))
	{
		pixmap = XfeGetPixmap(cascade,"task_small_browser");
	}

	if (!XfePixmapGood(arm_pixmap))
	{
		Pixel arm_color;

		XtVaGetValues(cascade,XmNarmColor,&arm_color,NULL);
		
		arm_pixmap = XfeGetPixmap(cascade,"task_small_browser_armed");
	}

	if (XfePixmapGood(pixmap))
	{
		XtVaSetValues(cascade,XmNlabelPixmap,pixmap,NULL);
	}

	if (XfePixmapGood(arm_pixmap))
	{
		XtVaSetValues(cascade,XmNarmPixmap,arm_pixmap,NULL);
	}


	return pane;
}
/*----------------------------------------------------------------------*/
static Widget
create_item(Widget parent,String name)
{
	Widget			item;

	static Pixmap			arm_pixmap = XmUNSPECIFIED_PIXMAP;
	static Pixmap			pixmap = XmUNSPECIFIED_PIXMAP;

	item = XtVaCreateManagedWidget(name,
#if 1
								   xfeBmButtonWidgetClass,
#else
								   xmPushButtonWidgetClass,
#endif
								   parent,
								   NULL);

	if (!XfePixmapGood(pixmap))
	{
		pixmap = XfeGetPixmap(parent,"task_small_browser");
	}

	if (!XfePixmapGood(arm_pixmap))
	{
		Pixel arm_color;

		XtVaGetValues(item,XmNarmColor,&arm_color,NULL);

		arm_pixmap = XfeGetPixmap(parent,"task_small_browser");
	}

	if (XfePixmapGood(pixmap))
	{
		XtVaSetValues(item,XmNlabelPixmap,pixmap,NULL);
	}

	if (XfePixmapGood(arm_pixmap))
	{
		XtVaSetValues(item,XmNarmPixmap,arm_pixmap,NULL);
	}

	XtAddCallback(item,XmNactivateCallback,activate_callback,NULL);

	XtManageChild(item);

	return item;
}
/*----------------------------------------------------------------------*/
static Widget
create_sep(Widget parent,String name)
{
	Widget			item;

	item = XtVaCreateManagedWidget(name,
								   xmSeparatorWidgetClass,
								   parent,
								   NULL);

	XtManageChild(item);

	return item;
}
/*----------------------------------------------------------------------*/
static void
activate_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
	unsigned char accent_type;

	XtVaGetValues(w,XmNaccentType,&accent_type,NULL);

	switch(accent_type)
	{
	case XmACCENT_TOP:
		printf("Activate(%s) ACCENT_TOP\n",XtName(w));
		break;

	case XmACCENT_BOTTOM:
		printf("Activate(%s) ACCENT_BOTTOM\n",XtName(w));
		break;

	case XmACCENT_NONE:
		printf("Activate(%s) ACCENT_NONE\n",XtName(w));
		break;

	case XmACCENT_ALL:
		printf("Activate(%s) ACCENT_ALL\n",XtName(w));
		break;
	}
}
/*----------------------------------------------------------------------*/
static Widget
create_cascade_alone_one(Widget parent,String name)
{
	Widget cascade;
	Widget sub_menu_id;
	Widget pane;
	Widget pane2;
	Widget pane3;
	
	cascade = 
		XtVaCreateManagedWidget(name,
								xfeCascadeWidgetClass,
								parent,
								XmNbuttonLayout,	XmBUTTON_LABEL_ON_BOTTOM,
								XmNbuttonType,		XmBUTTON_PUSH,
								XmNmappingDelay,	0,
								NULL);
	
	sub_menu_id = get_sub_menu_id(cascade);
	
	assert( XfeIsAlive(sub_menu_id) );

	/* Turn accent drawing on */
	XfeBmAccentSetFileMode(XmACCENT_FILE_ANYWHERE);
	XfeBmAccentEnable();

	XtVaSetValues(sub_menu_id,XmNtearOffModel,XmTEAR_OFF_ENABLED,NULL);
	
#if 1
/*	XtVaSetValues(cascade,XmNpixmap,_bm_pixmap,NULL);*/
	XtVaSetValues(cascade,XmNbuttonLayout,XmBUTTON_LABEL_ON_BOTTOM,NULL);
#endif
	
	create_item(sub_menu_id,"Open");
	create_item(sub_menu_id,"Close");
	pane = create_bm_pulldown(sub_menu_id,"More");
	create_item(sub_menu_id,"Save");
	create_item(sub_menu_id,"SaveAs");
	create_item(sub_menu_id,"Exit");
		
	/*pane = create_pulldown(sub_menu_id,"More");*/
	
	pane2 = create_bm_pulldown(pane,"More 2");

	create_item(pane,"Browser");
	create_item(pane,"Mail");
	create_item(pane,"News");
	create_item(pane,"Editor");

	pane3 = create_bm_pulldown(pane2,"More 3");

	create_item(pane2,"Browser");
	create_item(pane2,"Mail");
	create_item(pane2,"News");
	create_item(pane2,"Editor");

	create_item(pane3,"Browser");
	create_item(pane3,"Mail");
	create_item(pane3,"News");
	create_item(pane3,"Editor");

	XtAddCallback(cascade,XmNactivateCallback,post_callback,NULL);

	return cascade;
}
/*----------------------------------------------------------------------*/
static Widget
create_cascade_alone_two(Widget parent,String name)
{
	Widget cascade;
	Widget sub_menu_id;
	Widget pane;
	
	cascade = 
		XtVaCreateManagedWidget(name,
								xfeCascadeWidgetClass,
								parent,
/*								XmNpixmap,			_bm_pixmap,*/
								XmNbuttonLayout,	XmBUTTON_LABEL_ON_BOTTOM,
								XmNbuttonType,		XmBUTTON_PUSH,
								NULL);

	sub_menu_id = get_sub_menu_id(cascade);
	
/* 	XtVaSetValues(sub_menu_id,XmNtearOffModel,XmTEAR_OFF_ENABLED,NULL); */
	
	create_item(sub_menu_id,"Open");
	create_item(sub_menu_id,"Close");
	create_item(sub_menu_id,"Save");
	create_item(sub_menu_id,"SaveAs");
	create_item(sub_menu_id,"Exit");
		
	pane = create_bm_pulldown(sub_menu_id,"More");
	/*pane = create_pulldown(sub_menu_id,"More");*/

#if 0	
	create_item(pane,"Browser");
	create_item(pane,"Mail");
	create_item(pane,"News");
	create_item(pane,"Editor");
#endif


	return cascade;
}
/*----------------------------------------------------------------------*/
static Widget
get_sub_menu_id(Widget w)
{
	Widget sub_menu_id;

	XtVaGetValues(w,XmNsubMenuId,&sub_menu_id,NULL);

	return sub_menu_id;
}
/*----------------------------------------------------------------------*/
static void
post_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
	XfeSleep(w,event_loop,2000);

	XfeBmAccentEnable();

	XfeCascadeArmAndPost(w,NULL);
}
/*----------------------------------------------------------------------*/
static void
event_loop(void)
{
	XtAppContext	app = XfeAppContext();
	XEvent			event;

	/* XtAppLock(app); */

    while(1)
	{
    	XtAppNextEvent(app,&event);
		
		XtDispatchEvent(&event);
    }

	/* XtAppUnlock(app); */
}
/*----------------------------------------------------------------------*/
