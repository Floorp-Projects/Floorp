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
/* Name:		PaneTestTwo.c											*/
/* Description:	Test for XfePane widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

static void		menu_item_cb				(Widget,XtPointer,XtPointer);
static void		toggle_managed_cb			(Widget,XtPointer,XtPointer);

static Widget	create_simple_pane_frame	(String);
static Widget	create_form_and_button		(Widget,unsigned char,
											 unsigned char,Widget,String,
											 String,Dimension);

/*----------------------------------------------------------------------*/
static XfeMenuItemRec file_items[] =
{
	{ "Open",			XfeMENU_PUSH },
	{ "Save",			XfeMENU_PUSH },
	{ "------------",	XfeMENU_SEP	},
	{ "Exit",			XfeMENU_PUSH,	XfeExitCallback	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec orientation_items[] =
{
	{ "OrientationVertical",	XfeMENU_RADIO,		menu_item_cb		},
	{ "OrientationHorizontal",	XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec pane_sash_type_items[] =
{
	{ "pane_sash_double_line",		XfeMENU_RADIO,		menu_item_cb	},
	{ "pane_sash_filled_rectangle",	XfeMENU_RADIO,		menu_item_cb	},
	{ "pane_sash_live",				XfeMENU_RADIO,		menu_item_cb	},
	{ "pane_sash_rectangle",		XfeMENU_RADIO,		menu_item_cb	},
	{ "pane_sash_single_line",		XfeMENU_RADIO,		menu_item_cb	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec button_boolean_items[] =
{
	{ "TraversalOn",			XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",			XfeMENU_SEP	},
	{ "Sensitive",				XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",			XfeMENU_SEP	},
	{ "UsePreferredWidth",		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "UsePreferredHeight",		XfeMENU_TOGGLE,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec misc_items[] =
{
	{ "Orientation",	XfeMENU_PANE,	NULL,	orientation_items		},
	{ "PaneSashType",	XfeMENU_PANE,	NULL,	pane_sash_type_items	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec dimension_items[] =
{
	{ "0",				XfeMENU_RADIO,		menu_item_cb		},
	{ "1",				XfeMENU_RADIO,		menu_item_cb		},
	{ "2",				XfeMENU_RADIO,		menu_item_cb		},
	{ "3",				XfeMENU_RADIO,		menu_item_cb		},
	{ "4",				XfeMENU_RADIO,		menu_item_cb		},
	{ "5",				XfeMENU_RADIO,		menu_item_cb		},
	{ "6",				XfeMENU_RADIO,		menu_item_cb		},
	{ "7",				XfeMENU_RADIO,		menu_item_cb		},
	{ "8",				XfeMENU_RADIO,		menu_item_cb		},
	{ "9",				XfeMENU_RADIO,		menu_item_cb		},
	{ "10",				XfeMENU_RADIO,		menu_item_cb		},
	{ "20",				XfeMENU_RADIO,		menu_item_cb		},
	{ "30",				XfeMENU_RADIO,		menu_item_cb		},
	{ "40",				XfeMENU_RADIO,		menu_item_cb		},
	{ "50",				XfeMENU_RADIO,		menu_item_cb		},
	{ "100",			XfeMENU_RADIO,		menu_item_cb		},
	{ "200",			XfeMENU_RADIO,		menu_item_cb		},
	{ "300",			XfeMENU_RADIO,		menu_item_cb		},
	{ "400",			XfeMENU_RADIO,		menu_item_cb		},
	{ "500",			XfeMENU_RADIO,		menu_item_cb		},
	{ "1000",			XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec margin_items[] =
{
	{ "MarginLeft",		XfeMENU_PANE,		NULL, dimension_items		},
	{ "MarginRight",	XfeMENU_PANE,		NULL, dimension_items		},
	{ "MarginTop",		XfeMENU_PANE,		NULL, dimension_items		},
	{ "MarginBottom",	XfeMENU_PANE,		NULL, dimension_items		},
	{ "------------",	XfeMENU_SEP	},
	{ "MarginAll"	,	XfeMENU_PANE,		NULL, dimension_items		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec managing_items[] =
{
	{ "One"			,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "OneTop"		,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "OneBottom"	,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "OneLeft"		,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "OneRight"	,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "Two"			,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "TwoTop"		,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "TwoBottom"	,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "TwoLeft"		,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "TwoRight"	,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec dimensions_items[] =
{
	{ "Width",					XfeMENU_PANE,		NULL, dimension_items	},
	{ "Height",					XfeMENU_PANE,		NULL, dimension_items	},
	{ "------------",			XfeMENU_SEP	},
	{ "HighlightThickness",		XfeMENU_PANE,		NULL, dimension_items	},
	{ "ShadowThickness",		XfeMENU_PANE,		NULL, dimension_items	},
	{ "------------",			XfeMENU_SEP	},
	{ "SashOffset",				XfeMENU_PANE,		NULL, dimension_items	},
	{ "SashShadowThickness",	XfeMENU_PANE,		NULL, dimension_items	},
	{ "SashSpacing",			XfeMENU_PANE,		NULL, dimension_items	},
	{ "SashThickness",			XfeMENU_PANE,		NULL, dimension_items	},
	{ "------------",			XfeMENU_SEP	},
	{ "Margins",				XfeMENU_PANE,		NULL, margin_items		},
	{ "RaiseBorderThickness",	XfeMENU_PANE,		NULL, dimension_items	},
	{ "------------",			XfeMENU_SEP	},
	{ "ArmOffset",				XfeMENU_PANE,		NULL, dimension_items	},
	{ "------------",			XfeMENU_SEP	},
	{ "SashPosition",			XfeMENU_PANE,		NULL, dimension_items	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuPaneRec pane_items[] =
{
	{ "File",			file_items				},
	{ "Managing",		managing_items			},
	{ "Dimensions",		dimensions_items		},
	{ "BooleanItems",	button_boolean_items	},
	{ "MiscItems",		misc_items				},
	{ NULL }
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	simple_frame;

	XfeAppCreate("PaneTestTwo",&argc,argv);

	simple_frame = create_simple_pane_frame("Frame");

	XtPopup(simple_frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_simple_pane_frame(String frame_name)
{
	Widget	frame;
	Widget	form;
	Widget	menu;

	Widget	pane;

	Widget	one;
	Widget	two;

	frame = XfeFrameCreate(frame_name,NULL,0);

	form = XfeDescendantFindByName(frame,"MainForm",XfeFIND_ANY,False);

	pane = XtVaCreateManagedWidget("Pane",xfePaneWidgetClass,form,NULL);

	menu = XfeMenuBarCreate(form,"MenuBar",pane_items,(XtPointer) pane,NULL,0);

	one =  XtVaCreateManagedWidget("One",xmFormWidgetClass,pane,NULL);

  	two =  XtVaCreateManagedWidget("Two",xmFormWidgetClass,pane,NULL);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_ONE,
						   XmPANE_CHILD_ATTACH_TOP,
						   one,"OneTop","Top",2);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_ONE,
						   XmPANE_CHILD_ATTACH_BOTTOM,
						   one,"OneBottom","Bottom",2);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_ONE,
						   XmPANE_CHILD_ATTACH_LEFT,
						   one,"OneLeft","Left",2);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_ONE,
						   XmPANE_CHILD_ATTACH_RIGHT,
						   one,"OneRight","Right",2);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_TWO,
						   XmPANE_CHILD_ATTACH_TOP,
						   two,"TwoTop","Top",2);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_TWO,
						   XmPANE_CHILD_ATTACH_BOTTOM,
						   two,"TwoBottom","Bottom",2);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_TWO,
						   XmPANE_CHILD_ATTACH_LEFT,
						   two,"TwoLeft","Left",2);

	create_form_and_button(pane,
						   XmPANE_CHILD_ATTACHMENT_TWO,
						   XmPANE_CHILD_ATTACH_RIGHT,
						   two,"TwoRight","Right",2);
	return frame;
}
/*----------------------------------------------------------------------*/
static void
menu_item_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	Widget		pane = (Widget) client_data;
	String		name = XtName(w);
	String		parent_name = XtName(XtParent(w));
	Arg			av[20];
	Cardinal	ac = 0;

	printf("menu_item_cb(%s,%s) pane = %s\n",name,parent_name,XtName(pane));

	if (!XfeIsAlive(pane))
	{
		return;
	}

	if (strcmp(name,"TraversalOn") == 0)
	{
		XfeResourceToggleBoolean(pane,XmNtraversalOn);
	}
	else if (strcmp(name,"pane_sash_double_line") == 0)
	{
		XtSetArg(av[ac],XmNpaneSashType,XmPANE_SASH_DOUBLE_LINE); ac++;
	}
	else if (strcmp(name,"pane_sash_live") == 0)
	{
		XtSetArg(av[ac],XmNpaneSashType,XmPANE_SASH_LIVE); ac++;
	}
	else if (strcmp(name,"pane_sash_rectangle") == 0)
	{
		XtSetArg(av[ac],XmNpaneSashType,XmPANE_SASH_RECTANGLE); ac++;
	}
	else if (strcmp(name,"pane_sash_filled_rectangle") == 0)
	{
		XtSetArg(av[ac],XmNpaneSashType,XmPANE_SASH_FILLED_RECTANGLE); ac++;
	}
	else if (strcmp(name,"pane_sash_single_line") == 0)
	{
		XtSetArg(av[ac],XmNpaneSashType,XmPANE_SASH_SINGLE_LINE); ac++;
	}
	else if (strcmp(name,"Sensitive") == 0)
	{
		XfeResourceToggleBoolean(pane,XmNsensitive);
	}
	else if (strcmp(name,"UsePreferredWidth") == 0)
	{
		XfeResourceToggleBoolean(pane,XmNusePreferredWidth);
	}
	else if (strcmp(name,"UsePreferredHeight") == 0)
	{
		XfeResourceToggleBoolean(pane,XmNusePreferredHeight);
	}
	else if (strcmp(name,"ShadowIn") == 0)
	{
		XtSetArg(av[ac],XmNshadowType,XmSHADOW_IN); ac++;
	}
	else if (strcmp(name,"ShadowOut") == 0)
	{
		XtSetArg(av[ac],XmNshadowType,XmSHADOW_OUT); ac++;
	}
	else if (strcmp(name,"ShadowEtchedIn") == 0)
	{
		XtSetArg(av[ac],XmNshadowType,XmSHADOW_ETCHED_IN); ac++;
	}
	else if (strcmp(name,"ShadowEtchedOut") == 0)
	{
		XtSetArg(av[ac],XmNshadowType,XmSHADOW_ETCHED_OUT); ac++;
	}
	else if (strcmp(name,"AlignmentBeginning") == 0)
	{
		XtSetArg(av[ac],XmNlabelAlignment,XmALIGNMENT_BEGINNING); ac++;
	}
	else if (strcmp(name,"AlignmentCenter") == 0)
	{
		XtSetArg(av[ac],XmNlabelAlignment,XmALIGNMENT_CENTER); ac++;
	}
	else if (strcmp(name,"AlignmentEnd") == 0)
	{
		XtSetArg(av[ac],XmNlabelAlignment,XmALIGNMENT_END); ac++;
	}

	if (strcmp(parent_name,"MarginLeft") == 0)
	{
		XtSetArg(av[ac],XmNmarginLeft,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"MarginRight") == 0)
	{
		XtSetArg(av[ac],XmNmarginRight,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"MarginTop") == 0)
	{
		XtSetArg(av[ac],XmNmarginTop,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"MarginBottom") == 0)
	{
		XtSetArg(av[ac],XmNmarginBottom,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"MarginAll") == 0)
	{
		XtSetArg(av[ac],XmNmarginLeft,atoi(name)); ac++;
		XtSetArg(av[ac],XmNmarginRight,atoi(name)); ac++;
		XtSetArg(av[ac],XmNmarginTop,atoi(name)); ac++;
		XtSetArg(av[ac],XmNmarginBottom,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"Width") == 0)
	{
		XtSetArg(av[ac],XmNwidth,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"Height") == 0)
	{
		XtSetArg(av[ac],XmNheight,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"ShadowThickness") == 0)
	{
		XtSetArg(av[ac],XmNshadowThickness,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"SashOffset") == 0)
	{
		XtSetArg(av[ac],XmNsashOffset,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"SashSpacing") == 0)
	{
		XtSetArg(av[ac],XmNsashSpacing,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"SashThickness") == 0)
	{
		XtSetArg(av[ac],XmNsashThickness,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"SashShadowThickness") == 0)
	{
		XtSetArg(av[ac],XmNsashShadowThickness,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"HighlightThickness") == 0)
	{
		XtSetArg(av[ac],XmNhighlightThickness,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"RaiseBorderThickness") == 0)
	{
		XtSetArg(av[ac],XmNraiseBorderThickness,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"ArmOffset") == 0)
	{
		XtSetArg(av[ac],XmNarmOffset,atoi(name)); ac++;
	}
	else if (strcmp(name,"OrientationVertical") == 0)
	{
		XtSetArg(av[ac],XmNorientation,XmVERTICAL); ac++;
	}
	else if (strcmp(name,"OrientationHorizontal") == 0)
	{
		XtSetArg(av[ac],XmNorientation,XmHORIZONTAL); ac++;
	}
	else if (strcmp(parent_name,"Managing") == 0)
	{
		Widget child = XfeDescendantFindByName(pane,name,XfeFIND_ANY,False);

		if (XfeIsAlive(child))
		{
			XfeToggleManagedState(child);
		}
	}
	else if (strcmp(parent_name,"SashPosition") == 0)
	{
		XtSetArg(av[ac],XmNsashPosition,atoi(name)); ac++;
	}

	if (ac)
	{
		XtSetValues(pane,av,ac);
	}
}
/*----------------------------------------------------------------------*/
static void
toggle_managed_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	Widget		pane = (Widget) client_data;

	if (XfeIsAlive(pane))
	{
		XfeToggleManagedState(pane);

		if (XfeIsArrow(w))
		{
			unsigned char arrow_direction;

			XtVaGetValues(w,XmNarrowDirection,&arrow_direction,NULL);

			if (arrow_direction == XmARROW_DOWN)
			{
				arrow_direction = XmARROW_UP;
			}
			else if (arrow_direction == XmARROW_UP)
			{
				arrow_direction = XmARROW_DOWN;
			}
			else if (arrow_direction == XmARROW_RIGHT)
			{
				arrow_direction = XmARROW_LEFT;
			}
			else if (arrow_direction == XmARROW_LEFT)
			{
				arrow_direction = XmARROW_RIGHT;
			}

			XtVaSetValues(w,XmNarrowDirection,arrow_direction,NULL);
		}
	}
}
/*----------------------------------------------------------------------*/
static Widget
create_form_and_button(Widget			pw,
					   unsigned char	child_type,
					   unsigned char	child_attachment,
					   Widget			target,
					   String			form_name,
					   String			button_name,
					   Dimension		offset)
{
	Widget		form = NULL;
	Widget		button = NULL;
	Arg			av[20];
	Cardinal	ac = 0;

	XtSetArg(av[ac],XmNpaneChildType,		child_type);		ac++;
	XtSetArg(av[ac],XmNpaneChildAttachment,	child_attachment);	ac++;
 	XtSetArg(av[ac],XmNalwaysVisible,		True);				ac++;

#if 1

 	form = XfeCreateFormAndButton(pw,form_name,button_name,offset,True,av,ac);

	button = XfeDescendantFindByName(form,button_name,XfeFIND_ANY,False);

	assert( XfeIsAlive(button) );

	XtAddCallback(button,XmNactivateCallback,toggle_managed_cb,target);

#else

 	XtSetArg(av[ac],XmNwidth,				20);				ac++;
 	XtSetArg(av[ac],XmNheight,				20);				ac++;

	form = XtCreateManagedWidget(form_name,xmFormWidgetClass,pw,av,ac);

#endif

	return form;
}
/*----------------------------------------------------------------------*/
