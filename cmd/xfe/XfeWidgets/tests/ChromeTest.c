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
/* Name:		ChromeTest.c											*/
/* Description:	Test for XfeChrome widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

static Widget	create_chrome		(Widget,String);
static Widget	create_url_bar		(Widget,String);
static Widget	create_tool_box		(Widget,String);
static void		menu_item_cb		(Widget,XtPointer,XtPointer);

static void		chrome_toggle		(Widget,XtPointer,XtPointer);
static void		chrome_flash		(Widget,XtPointer,XtPointer);
static void		dash_board_toggle	(Widget,XtPointer,XtPointer);
static void		dash_board_flash	(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
static XfeMenuItemRec toggle_dash_board_items[] =
{
	{ "ToolBar",		XfeMENU_TOGGLE, dash_board_toggle		},
	{ "StatusBar",		XfeMENU_TOGGLE, dash_board_toggle		},
	{ "ProgressBar",	XfeMENU_TOGGLE, dash_board_toggle		},
	{ "TaskBar",		XfeMENU_TOGGLE, dash_board_toggle		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec flash_dash_board_items[] =
{
	{ "ToolBar",		XfeMENU_PUSH, dash_board_flash		},
	{ "StatusBar",		XfeMENU_PUSH, dash_board_flash		},
	{ "ProgressBar",	XfeMENU_PUSH, dash_board_flash		},
	{ "TaskBar",		XfeMENU_PUSH, dash_board_flash		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec file_items[] =
{
	{ "Open",		XfeMENU_PUSH },
	{ "Save",		XfeMENU_PUSH },
	{ "Sep",		XfeMENU_SEP	},
	{ "Exit",		XfeMENU_PUSH,	XfeExitCallback	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec toggle_items[] =
{
	{ "MenuBar",	XfeMENU_TOGGLE, chrome_toggle		},
	{ "ToolBox",	XfeMENU_TOGGLE, chrome_toggle		},
	{ "DashBoard",	XfeMENU_TOGGLE, chrome_toggle		},
	{ "View",		XfeMENU_TOGGLE, chrome_toggle		},
	{ "TopView",	XfeMENU_TOGGLE, chrome_toggle		},
	{ "BottomView",	XfeMENU_TOGGLE, chrome_toggle		},
	{ "Sep",		XfeMENU_SEP	},
	{ "DashBoard",	XfeMENU_PANE, NULL, 	toggle_dash_board_items	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec flash_items[] =
{
	{ "MenuBar",	XfeMENU_PUSH, chrome_flash		},
	{ "ToolBox",	XfeMENU_PUSH, chrome_flash		},
	{ "DashBoard",	XfeMENU_PUSH, chrome_flash		},
	{ "View",		XfeMENU_PUSH, chrome_flash		},
	{ "TopView",	XfeMENU_PUSH, chrome_flash		},
	{ "BottomView",	XfeMENU_PUSH, chrome_flash		},
	{ "Sep",		XfeMENU_SEP	},
	{ "DashBoard",	XfeMENU_PANE, NULL, 	flash_dash_board_items	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec options_items[] =
{
	{ "One",	XfeMENU_PUSH		},
	{ "Two",	XfeMENU_PUSH		},
	{ "Three",	XfeMENU_PUSH		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec help_items[] =
{
	{ "Contents",	XfeMENU_PUSH		},
	{ "Search",		XfeMENU_PUSH		},
	{ "Sep",		XfeMENU_SEP	},
	{ "About",		XfeMENU_PUSH		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuPaneRec pane_items[] =
{
	{ "File",		file_items		},
	{ "Toggle",		toggle_items	},
	{ "Flash",		flash_items	},
	{ "Options",	options_items	},
	{ "Help",		help_items		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static Widget		_chrome = NULL;

static Widget		_menu_bar = NULL;
static Widget		_tool_box = NULL;
static Widget		_dash_board = NULL;
static Widget		_bottom_view = NULL;
static Widget		_center_view = NULL;
static Widget		_left_view = NULL;
static Widget		_right_view = NULL;
static Widget		_top_view = NULL;

static Widget		_personal_tool_bar = NULL;
static Widget		_tool_bar = NULL;
static Widget		_url_bar = NULL;

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;

	XfeAppCreateSimple("ChromeTest",&argc,argv,"MainFrame",&frame,&form);

	_menu_bar = XfeMenuBarCreate(form,"MainMenuBar",pane_items,NULL,NULL,0);

	_chrome = create_chrome(form,"Chrome");

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_url_bar(Widget parent,String name)
{
	Widget url_bar;
	Widget url_form;
	Widget url_label;
	Widget url_text;
	Widget bookmark_button;

	url_bar = XtVaCreateManagedWidget(name,
									  xmFrameWidgetClass,
									  parent,
									  NULL);

	url_form = XtVaCreateManagedWidget("UrlForm",
									   xmFormWidgetClass,
									   url_bar,
									   NULL);
	
	bookmark_button = XtVaCreateManagedWidget("BookMarkButton",
											  xfeButtonWidgetClass,
											  url_form,
											  XmNleftAttachment, XmATTACH_FORM,
											  NULL);


	url_label = XtVaCreateManagedWidget("UrlLabel",
										xmLabelWidgetClass,
										url_form,
										XmNleftAttachment,	XmATTACH_WIDGET,
										XmNleftWidget,		bookmark_button,
										NULL);

#if 1
	url_text = XtVaCreateManagedWidget("UrlText",
									   xmTextFieldWidgetClass,
									   url_form,
									   XmNleftAttachment,	XmATTACH_WIDGET,
									   XmNleftWidget,		url_label,
									   XmNrightAttachment,	XmATTACH_FORM,
									   NULL);
#else
	url_text = XtVaCreateManagedWidget("UrlCombo",
									   xfeFancyBoxWidgetClass,
									   url_form,
									   XmNleftAttachment,	XmATTACH_WIDGET,
									   XmNleftWidget,		url_label,
									   XmNrightAttachment,	XmATTACH_FORM,
									   NULL);
#endif
	
	return url_bar;
}
/*----------------------------------------------------------------------*/
static Widget
create_tool_box(Widget parent,String name)
{
	Widget tool_box;

	tool_box = XfeCreateLoadedToolBox(parent,"ToolBox",NULL,0);

	XtVaSetValues(tool_box,
				  XmNusePreferredWidth,		False,
				  XmNusePreferredHeight,	True,
				  NULL);

	return tool_box;
}
/*----------------------------------------------------------------------*/
static Widget
create_chrome(Widget parent,String name)
{
	Widget chrome;
	Widget menu_bar;
	Widget tool_bar_one;
	Widget url_bar;
	Widget tool_bar_two;

	chrome = XtVaCreateManagedWidget(name,
									  xfeChromeWidgetClass,
									  parent,
									  NULL);

	menu_bar = XfeMenuBarCreate(chrome,"MenuBar",pane_items,NULL,NULL,0);

	XtSetSensitive(menu_bar,False);

	_dash_board = XfeCreateLoadedDashBoard(
		chrome,					/* pw				*/
		"DashBoard",			/* name				*/
		"S",					/* tool_prefix		*/
		XfeActivateCallback,	/* tool_cb			*/
		3,						/* tool_count		*/
		"T",					/* tool_prefix		*/
		False,					/* tool_large		*/
		XfeActivateCallback,	/* tool_cb			*/
		4,						/* tool_count		*/
		NULL,					/* tool_bar_out		*/
		NULL,					/* progress_bar_out	*/
		NULL,					/* status_bar_out	*/
		NULL,					/* task_bar_out		*/
		NULL,					/* tool_items_out	*/
		NULL);					/* task_items_out	*/

	_tool_box = create_tool_box(chrome,"ToolBox");

	tool_bar_one = XfeCreateLoadedToolBar(_tool_box,
										  "ToolBoxOne",
										  "One",
										  10,
										  2,
										  XfeArmCallback,
										  XfeDisarmCallback,
										  XfeActivateCallback,
										  NULL);
	
	url_bar = create_url_bar(_tool_box,"UrlBar");
	
	
	tool_bar_two = XfeCreateLoadedToolBar(_tool_box,
										  "ToolBoxTwo",
										  "Two",
										  10,
										  2,
										  XfeArmCallback,
										  XfeDisarmCallback,
										  XfeActivateCallback,
										  NULL);

	_center_view = XfeCreateFormAndButton(chrome,
										  "CenterView",
										  "Center",
										  20,
										  False,
										  NULL,0);

	_top_view = XfeCreateFormAndButton(chrome,
									   "TopView",
									   "Top",
									   10,
									   False,
									   NULL,0);
	
	_bottom_view = XfeCreateFormAndButton(chrome,
										  "BottomView",
										  "Bottom",
										  10,
										  False,
										  NULL,0);

	_left_view = XfeCreateFormAndButton(chrome,
										"LeftView",
										"Left",
										10,
										False,
										NULL,0);
	
	_right_view = XfeCreateFormAndButton(chrome,
										 "RightView",
										  "Right",
										 10,
										 False,
										 NULL,0);
	
	return chrome;
}
/*----------------------------------------------------------------------*/
static void
menu_item_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	int code = (int) client_data;

	printf("code = %d\n",code);
}
/*----------------------------------------------------------------------*/
static void
chrome_toggle(Widget w,XtPointer client_data,XtPointer call_data)
{
	String name = XtName(w);
	Widget tw = XfeDescendantFindByName(_chrome,name,XfeFIND_ANY,False);

	if (XfeIsAlive(tw))
	{
		XfeWidgetToggleManaged(tw);
	}
}
/*----------------------------------------------------------------------*/
static void
dash_board_toggle(Widget w,XtPointer client_data,XtPointer call_data)
{
	String name = XtName(w);
	Widget tw = XfeDescendantFindByName(_dash_board,name,XfeFIND_ANY,False);

	if (XfeIsAlive(tw))
	{
		XfeWidgetToggleManaged(tw);
	}
}
/*----------------------------------------------------------------------*/
static void
chrome_flash(Widget w,XtPointer client_data,XtPointer call_data)
{
	String name = XtName(w);
	Widget tw = XfeDescendantFindByName(_chrome,name,XfeFIND_ANY,False);

	if (XfeIsAlive(tw))
	{
		XfeWidgetFlash(tw,400,3);
	}
}
/*----------------------------------------------------------------------*/
static void
dash_board_flash(Widget w,XtPointer client_data,XtPointer call_data)
{
	String name = XtName(w);
	Widget tw = XfeDescendantFindByName(_dash_board,name,XfeFIND_ANY,False);

	if (XfeIsAlive(tw))
	{
		XfeWidgetFlash(tw,400,3);
	}
}
/*----------------------------------------------------------------------*/
