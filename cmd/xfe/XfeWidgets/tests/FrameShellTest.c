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
/* Name:		FrameShellTest.c										*/
/* Description:	Test for XfeFrameShell widget.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include <XfeTest/XfeTest.h>

static Widget	create_control		(String);
static Widget	create_dummy		(String);

static void		first_map_cb		(Widget,XtPointer,XtPointer);
static void		map_cb				(Widget,XtPointer,XtPointer);
static void		unmap_cb			(Widget,XtPointer,XtPointer);

static void		track_cb			(Widget,XtPointer,XtPointer);
static void		operation_cb		(Widget,XtPointer,XtPointer);

static void		message_output		(String);
static String	message_create		(Widget,String);

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
static XfeMenuItemRec track_items[] =
{
	{ "Editres",		XfeMENU_TOGGLE,		track_cb		},
	{ "DeleteWindow",	XfeMENU_TOGGLE,		track_cb		},
	{ "SaveYourself",	XfeMENU_TOGGLE,		track_cb		},
	{ "Mapping",		XfeMENU_TOGGLE,		track_cb		},
	{ "Position",		XfeMENU_TOGGLE,		track_cb		},
	{ "Size",			XfeMENU_TOGGLE,		track_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec operations_items[] =
{
	{ "Popup",		XfeMENU_PUSH, operation_cb		},
	{ "Popdown",	XfeMENU_PUSH, operation_cb		},
	{ "Sep",		XfeMENU_SEP	},
	{ "Map",		XfeMENU_PUSH, operation_cb		},
	{ "Unmap",		XfeMENU_PUSH, operation_cb		},
	{ "Realize",		XfeMENU_PUSH, operation_cb		},
	{ "Sep",		XfeMENU_SEP	},
	{ "Raise",		XfeMENU_PUSH, operation_cb		},
	{ "Lower",		XfeMENU_PUSH, operation_cb		},
	{ "Sep",		XfeMENU_SEP	},
	{ "Destroy",	XfeMENU_PUSH, operation_cb		},
	{ "Create",		XfeMENU_PUSH, operation_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuPaneRec pane_items[] =
{
	{ "File",		file_items			},
	{ "Track",		track_items			},
	{ "Operations",	operations_items	},
	{ NULL }
};
/*----------------------------------------------------------------------*/

static Widget _control = NULL;
static Widget _dummy = NULL;
static Widget _tool_bar = NULL;
static Widget _log_list = NULL;

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	XfeAppCreate("FrameShellTest",&argc,argv);

	_control = create_control("Control");

	_dummy = create_dummy("Dummy");

	_tool_bar = XfeDescendantFindByName(_control,"ToolBar",XfeFIND_ANY,False);

	_log_list = XfeDescendantFindByName(_control,"List",XfeFIND_ANY,False);

/* 	XtPopup(_dummy,XtGrabNone); */
    
	XtPopup(_control,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_control(String name)
{
	Widget		frame = NULL;
	Widget		form = NULL;
	Widget		tool_bar;
	Widget		list;
	Arg			av[20];
	Cardinal	ac = 0;

	
	XtSetArg(av[ac],XmNtrackEditres,False); ac++;

	frame = XfeFrameCreate(name,av,ac);
	
	form = XfeDescendantFindByName(frame,"MainForm",XfeFIND_ANY,False);

	tool_bar = XfeToolBarCreate(form,"ToolBar",pane_items,NULL);

	list = XmCreateScrolledList(form,"List",NULL,0);

	XtManageChild(list);

	return frame;
}
/*----------------------------------------------------------------------*/
static Widget
create_dummy(String name)
{
	Widget frame = NULL;
	Widget form = NULL;

	frame = XfeFrameCreate(name,NULL,0);
	
	form = XfeDescendantFindByName(frame,"MainForm",XfeFIND_ANY,False);

	XtAddCallback(frame,XmNfirstMapCallback,first_map_cb,NULL);
	XtAddCallback(frame,XmNmapCallback,map_cb,NULL);
	XtAddCallback(frame,XmNunmapCallback,unmap_cb,NULL);

	return frame;
}
/*----------------------------------------------------------------------*/
static void
first_map_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	message_output(message_create(w,"FirstMap"));
}
/*----------------------------------------------------------------------*/
static void
map_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	message_output(message_create(w,"Map"));
}
/*----------------------------------------------------------------------*/
static void
unmap_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	message_output(message_create(w,"Unmap"));
}
/*----------------------------------------------------------------------*/
static void
track_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	String name = XtName(w);

	if (!XfeIsAlive(_dummy))
	{
		return;
	}

	if (strcmp(name,"Editres") == 0)
	{
		XfeResourceToggleBoolean(_dummy,XmNtrackEditres);
	}
	else if (strcmp(name,"DeleteWindow") == 0)
	{
		XfeResourceToggleBoolean(_dummy,XmNtrackDeleteWindow);
	}
	else if (strcmp(name,"Mapping") == 0)
	{
		XfeResourceToggleBoolean(_dummy,XmNtrackMapping);
	}
}
/*----------------------------------------------------------------------*/
static void
operation_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	String name = XtName(w);

	if (!XfeIsAlive(_dummy))
	{
		return;
	}

	if (strcmp(name,"Popup") == 0)
	{
		XtPopup(_dummy,XtGrabNone);
	}
	else if (strcmp(name,"Popdown") == 0)
	{
		XtPopdown(_dummy);
	}
	else if (strcmp(name,"Map") == 0)
	{
		XtMapWidget(_dummy);
	}
	else if (strcmp(name,"Unmap") == 0)
	{
		XtUnmapWidget(_dummy);
	}
	else if (strcmp(name,"Realize") == 0)
	{
		XtRealizeWidget(_dummy);
	}
}
/*----------------------------------------------------------------------*/
static void
message_output(String message)
{
	XmString item;

	assert( XfeIsAlive(_log_list) );

	if (!message)
	{
		message = "NULL";
	}

	item = XmStringCreate(message,XmFONTLIST_DEFAULT_TAG);

	XmListAddItem(_log_list,item,0);

	XmStringFree(item);
}
/*----------------------------------------------------------------------*/
static String
message_create(Widget w,String message)
{
	String name;

	static char buf[4096];
	static int count = 1;

	if (XfeIsAlive(w))
	{
		name = XtName(w);
	}
	else
	{
		name = "NULL";
	}

	sprintf(buf,"%3d. %s(%s)",count,message,name);

	count++;

	return buf;
}
/*----------------------------------------------------------------------*/
