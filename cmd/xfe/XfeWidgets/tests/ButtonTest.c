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
/* Name:		ButtonTest.c											*/
/* Description:	Test for XfeButton widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

static Widget	create_button			(Widget,String);
static void		install_large_pixmaps	(Widget);
static void		install_small_pixmaps	(Widget);
static void		change_pixmap			(Widget);
static void		change_label			(Widget);
static void		button_callback			(Widget,XtPointer,XtPointer);
static void		menu_item_cb			(Widget,XtPointer,XtPointer);

static Widget _button_target = NULL;

static String labels[] = 
{
	"Label One",
	"A",
	"This is a long label",
	"",
	"The previous one was blank",
	"Label Four",
	"First Line\nSecond Line\nThird Line",
	"-=-=-=-= What ? #$!&*",
};

#define CMP(s1,s2) (strcmp((s1),(s2)) == 0)

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
static XfeMenuItemRec button_type_items[] =
{
	{ "TypePush",		XfeMENU_RADIO,		menu_item_cb		},
	{ "TypeToggle",		XfeMENU_RADIO,		menu_item_cb		},
	{ "TypeNone",		XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec button_layout_items[] =
{
	{ "LayoutLabelOnly",		XfeMENU_RADIO,		menu_item_cb		},
	{ "LayoutLabelOnBottom",	XfeMENU_RADIO,		menu_item_cb		},
	{ "LayoutLabelOnLeft",		XfeMENU_RADIO,		menu_item_cb		},
	{ "LayoutLabelOnRight",		XfeMENU_RADIO,		menu_item_cb		},
	{ "LayoutLabelOnTop",		XfeMENU_RADIO,		menu_item_cb		},
	{ "LayoutPixmapOnly",		XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec button_trigger_items[] =
{
	{ "TriggerAnywhere",	XfeMENU_RADIO,		menu_item_cb		},
	{ "TriggerEither",		XfeMENU_RADIO,		menu_item_cb		},
	{ "TriggerNeither",		XfeMENU_RADIO,		menu_item_cb		},
	{ "TriggerLabel",		XfeMENU_RADIO,		menu_item_cb		},
	{ "TriggerPixmap",		XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec modifiers_items[] =
{
	{ "None",				XfeMENU_RADIO,		menu_item_cb		},
	{ "Any",				XfeMENU_RADIO,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "Shift",				XfeMENU_RADIO,		menu_item_cb		},
	{ "Control",			XfeMENU_RADIO,		menu_item_cb		},
	{ "Alt",				XfeMENU_RADIO,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "Mod1",				XfeMENU_RADIO,		menu_item_cb		},
	{ "Mod2",				XfeMENU_RADIO,		menu_item_cb		},
	{ "Mod3",				XfeMENU_RADIO,		menu_item_cb		},
	{ "Mod4",				XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec button_boolean_items[] =
{
	{ "TraversalOn",		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "RaiseOnEnter",		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "FillOnEnter",		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "FillOnArm"	,		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "EmulateMotif",		XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "Armed",				XfeMENU_TOGGLE,		menu_item_cb		},
	{ "Raised",				XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "Sensitive",			XfeMENU_TOGGLE,		menu_item_cb		},
	{ "PretendSensitive",	XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "Selected",			XfeMENU_TOGGLE,		menu_item_cb		},
	{ "------------",		XfeMENU_SEP	},
	{ "UsePreferredWidth",	XfeMENU_TOGGLE,		menu_item_cb		},
	{ "UsePreferredHeight",	XfeMENU_TOGGLE,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec misc_items[] =
{
	{ "ChangePixmap",	XfeMENU_PUSH,		menu_item_cb			},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec label_items[] =
{
	{ "ChangeLabel",	XfeMENU_PUSH,		menu_item_cb			},
	{ "Modifiers",		XfeMENU_PANE,		NULL, modifiers_items	},
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
static XfeMenuItemRec shadow_type_items[] =
{
	{ "ShadowIn",			XfeMENU_RADIO,		menu_item_cb		},
	{ "ShadowOut",			XfeMENU_RADIO,		menu_item_cb		},
	{ "ShadowEtchedIn",		XfeMENU_RADIO,		menu_item_cb		},
	{ "ShadowEtchedOut",	XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec alignment_items[] =
{
	{ "AlignmentBeginning",	XfeMENU_RADIO,		menu_item_cb		},
	{ "AlignmentCenter",	XfeMENU_RADIO,		menu_item_cb		},
	{ "AlignmentEnd",		XfeMENU_RADIO,		menu_item_cb		},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec enumeration_items[] =
{
	{ "ButtonType",			XfeMENU_PANE,		NULL, button_type_items		},
	{ "ButtonLayout",		XfeMENU_PANE,		NULL, button_layout_items	},
	{ "ButtonTrigger",		XfeMENU_PANE,		NULL, button_trigger_items	},
	{ "------------",		XfeMENU_SEP	},
	{ "ShadowType",			XfeMENU_PANE,		NULL, shadow_type_items		},
	{ "------------",		XfeMENU_SEP	},
	{ "LabelAlignment",		XfeMENU_PANE,		NULL, alignment_items		},
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
	{ "Margins",				XfeMENU_PANE,		NULL, margin_items		},
	{ "RaiseBorderThickness",	XfeMENU_PANE,		NULL, dimension_items	},
	{ "Spacing",				XfeMENU_PANE,		NULL, dimension_items	},
	{ "------------",			XfeMENU_SEP	},
	{ "ArmOffset",				XfeMENU_PANE,		NULL, dimension_items	},
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec xm_primitive_items[] =
{
	{ "Open",			XfeMENU_PUSH },
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuItemRec xfe_primitive_items[] =
{
	{ "Open",			XfeMENU_PUSH },
	{ NULL }
};
/*----------------------------------------------------------------------*/
static XfeMenuPaneRec pane_items[] =
{
	{ "File",				file_items				},
	{ "XfePrimitive",		xfe_primitive_items		},
	{ "Enumeration",		enumeration_items		},
	{ "Dimensions",			dimensions_items		},
	{ "Boolean",			button_boolean_items	},
	{ "Misc",				misc_items				},
	{ "Label",				label_items				},
	{ NULL }
};
/*----------------------------------------------------------------------*/

static Widget _main_form;
static Widget _menu_bar;

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget frame;

	XfeAppCreateSimple("ButtonTest",&argc,argv,"MainFrame",&frame,&_main_form);

	_button_target = create_button(_main_form,"ButtonTarget");

	_menu_bar = XfeMenuBarCreate(_main_form,"MenuBar",
								 pane_items,
								 (XtPointer) _button_target,NULL,0);

	install_large_pixmaps(_button_target);

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_button(Widget parent,String name)
{
	Widget w;

    w = XtVaCreateManagedWidget(name,
								xfeButtonWidgetClass,
								parent,
								NULL);

 	XtAddCallback(w,XmNactivateCallback,button_callback,NULL);
 	XtAddCallback(w,XmNarmCallback,button_callback,NULL);
 	XtAddCallback(w,XmNdisarmCallback,button_callback,NULL);
 	XtAddCallback(w,XmNselectionChangedCallback,button_callback,NULL);
/*  	XtAddCallback(w,XmNenterCallback,button_callback,NULL); */
/*  	XtAddCallback(w,XmNleaveCallback,button_callback,NULL); */
	
	return w;
}
/*----------------------------------------------------------------------*/
static void
install_large_pixmaps(Widget w)
{
	if (!XfeIsAlive(w))
	{
		return;
	}

    XtVaSetValues(
		w,
		XmNpixmap,				XfeGetPixmap(w,"task_browser"),
		XmNpixmapMask,			XfeGetMask(w,"task_browser"),
		XmNarmedPixmap,			XfeGetPixmap(w,"task_browser_armed"),
		XmNarmedPixmapMask,		XfeGetMask(w,"task_browser_armed"),
		XmNraisedPixmap,		XfeGetPixmap(w,"task_browser_raised"),
		XmNraisedPixmapMask,	XfeGetMask(w,"task_browser_raised"),
		NULL);
}
/*----------------------------------------------------------------------*/
static void
install_small_pixmaps(Widget w)
{
	if (!XfeIsAlive(w))
	{
		return;
	}

    XtVaSetValues(
		w,
		XmNpixmap,				XfeGetPixmap(w,"task_small_browser"),
		XmNpixmapMask,			XfeGetMask(w,"task_small_browser"),
		XmNarmedPixmap,			XfeGetPixmap(w,"task_small_browser_armed"),
		XmNarmedPixmapMask,		XfeGetMask(w,"task_small_browser_armed"),
		XmNraisedPixmap,		XfeGetPixmap(w,"task_small_browser_raised"),
		XmNraisedPixmapMask,	XfeGetMask(w,"task_small_browser_raised"),
		NULL);
}
/*----------------------------------------------------------------------*/
static void
change_pixmap(Widget w)
{
	static Boolean toggle = True;

	if (!XfeIsAlive(w))
	{
		return;
	}

	if (toggle)
	{
		install_small_pixmaps(w);
	}
	else
	{
		install_large_pixmaps(w);
	}

	toggle = !toggle;
}
/*----------------------------------------------------------------------*/
static void
change_label(Widget w)
{
	static Cardinal i = 0;

	if (!XfeIsAlive(w))
	{
		return;
	}

	XfeSetXmStringPSZ(w,
					  XmNlabelString,
					  XmFONTLIST_DEFAULT_TAG,
					  labels[i % XtNumber(labels)]);

	i++;
}
/*----------------------------------------------------------------------*/
static void
menu_item_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	String		name = XtName(w);
	String		parent_name = XtName(XtParent(w));
	Arg			av[20];
	Cardinal	ac = 0;

	printf("menu_item_cb(%s,%s)\n",name,parent_name);

	if (!XfeIsAlive(_button_target))
	{
		return;
	}

	/* Modifiers */
	if (CMP(parent_name,"Modifiers"))
	{
		if (CMP(name,"None"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,None); ac++;
		}
		else if (CMP(name,"Any"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,AnyModifier); ac++;
		}
		else if (CMP(name,"Shift"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,ShiftMask); ac++;
		}
		else if (CMP(name,"Control"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,ControlMask); ac++;
		}
		else if (CMP(name,"Alt"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,Mod1Mask); ac++;
		}
		else if (CMP(name,"Mod1"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,Mod1Mask); ac++;
		}
		else if (CMP(name,"Mod2"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,Mod2Mask); ac++;
		}
		else if (CMP(name,"Mod3"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,Mod3Mask); ac++;
		}
		else if (CMP(name,"Mod4"))
		{
			XtSetArg(av[ac],XmNselectionModifiers,Mod4Mask); ac++;
		}
	}
	else if (strcmp(name,"TypePush") == 0)
	{
		XtSetArg(av[ac],XmNbuttonType,XmBUTTON_PUSH); ac++;
	}
	else if (strcmp(name,"TypeToggle") == 0)
	{
		XtSetArg(av[ac],XmNbuttonType,XmBUTTON_TOGGLE); ac++;
	}
	else if (strcmp(name,"TypeNone") == 0)
	{
		XtSetArg(av[ac],XmNbuttonType,XmBUTTON_NONE); ac++;
	}
	else if (strcmp(name,"RaiseOnEnter") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNraiseOnEnter);
	}
	else if (strcmp(name,"TraversalOn") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNtraversalOn);
	}
	else if (strcmp(name,"EmulateMotif") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNemulateMotif);
	}
	else if (strcmp(name,"Armed") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNarmed);
	}
	else if (strcmp(name,"Raised") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNraised);
	}
	else if (strcmp(name,"Selected") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNselected);
	}
	else if (strcmp(name,"PretendSensitive") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNpretendSensitive);
	}
	else if (strcmp(name,"Sensitive") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNsensitive);
	}
	else if (strcmp(name,"UsePreferredWidth") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNusePreferredWidth);
	}
	else if (strcmp(name,"UsePreferredHeight") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNusePreferredHeight);
	}
	else if (strcmp(name,"FillOnEnter") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNfillOnEnter);
	}
	else if (strcmp(name,"FillOnArm") == 0)
	{
		XfeResourceToggleBoolean(_button_target,XmNfillOnArm);
	}
	else if (strcmp(name,"LayoutLabelOnly") == 0)
	{
		XtSetArg(av[ac],XmNbuttonLayout,XmBUTTON_LABEL_ONLY); ac++;
	}
	else if (strcmp(name,"LayoutLabelOnBottom") == 0)
	{
		XtSetArg(av[ac],XmNbuttonLayout,XmBUTTON_LABEL_ON_BOTTOM); ac++;
	}
	else if (strcmp(name,"LayoutLabelOnLeft") == 0)
	{
		XtSetArg(av[ac],XmNbuttonLayout,XmBUTTON_LABEL_ON_LEFT); ac++;
	}
	else if (strcmp(name,"LayoutLabelOnRight") == 0)
	{
		XtSetArg(av[ac],XmNbuttonLayout,XmBUTTON_LABEL_ON_RIGHT); ac++;
	}
	else if (strcmp(name,"LayoutLabelOnTop") == 0)
	{
		XtSetArg(av[ac],XmNbuttonLayout,XmBUTTON_LABEL_ON_TOP); ac++;
	}
	else if (strcmp(name,"LayoutPixmapOnly") == 0)
	{
		XtSetArg(av[ac],XmNbuttonLayout,XmBUTTON_PIXMAP_ONLY); ac++;
	}
	else if (strcmp(name,"TriggerAnywhere") == 0)
	{
		XtSetArg(av[ac],XmNbuttonTrigger,XmBUTTON_TRIGGER_ANYWHERE); ac++;
	}
	else if (strcmp(name,"TriggerEither") == 0)
	{
		XtSetArg(av[ac],XmNbuttonTrigger,XmBUTTON_TRIGGER_EITHER); ac++;
	}
	else if (strcmp(name,"TriggerNeither") == 0)
	{
		XtSetArg(av[ac],XmNbuttonTrigger,XmBUTTON_TRIGGER_NEITHER); ac++;
	}
	else if (strcmp(name,"TriggerLabel") == 0)
	{
		XtSetArg(av[ac],XmNbuttonTrigger,XmBUTTON_TRIGGER_LABEL); ac++;
	}
	else if (strcmp(name,"TriggerPixmap") == 0)
	{
		XtSetArg(av[ac],XmNbuttonTrigger,XmBUTTON_TRIGGER_PIXMAP); ac++;
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
	else if (strcmp(parent_name,"HighlightThickness") == 0)
	{
		XtSetArg(av[ac],XmNhighlightThickness,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"RaiseBorderThickness") == 0)
	{
		XtSetArg(av[ac],XmNraiseBorderThickness,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"Spacing") == 0)
	{
		XtSetArg(av[ac],XmNspacing,atoi(name)); ac++;
	}
	else if (strcmp(parent_name,"ArmOffset") == 0)
	{
		XtSetArg(av[ac],XmNarmOffset,atoi(name)); ac++;
	}
	else if (strcmp(name,"ChangePixmap") == 0)
	{
		change_pixmap(_button_target);
	}
	else if (strcmp(name,"ChangeLabel") == 0)
	{
		change_label(_button_target);
	}

	if (ac)
	{
		XtSetValues(_button_target,av,ac);
	}
}
/*----------------------------------------------------------------------*/
static void
button_callback(Widget button,XtPointer client_data,XtPointer call_data)
{
	XfeButtonCallbackStruct *	data = (XfeButtonCallbackStruct *) call_data;
	String						rname;

	switch(data->reason)
	{
	case XmCR_ARM:					rname = "Arm";				break;
	case XmCR_DISARM:				rname = "Disarm";			break;
	case XmCR_ACTIVATE:				rname = "Activate";			break;
	case XmCR_ENTER:				rname = "Enter";			break;
	case XmCR_LEAVE:				rname = "Leave";			break;
	case XmCR_RAISE:				rname = "Raise";			break;
	case XmCR_LOWER:				rname = "Lower";			break;
	case XmCR_SELECTION_CHANGED:	rname = "SelectionChanged";	break;
	}

	printf("%s(%s)\n",rname,XtName(button));
}
/*----------------------------------------------------------------------*/
