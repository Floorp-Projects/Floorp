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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		ToolTipTest.c											*/
/* Description:	Test for XfeToolTip widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/
 
#include <Xfe/XfeTest.h>
#include <Xfe/ToolTip.h>

static Widget	_button_gadgets[3];
static Widget	_label_gadgets[3];

static Widget	_button_widgets[3];
static Widget	_label_widgets[3];

static void tool_tip_cb(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;
	
	XfeAppCreateSimple("ToolTipTest",&argc,argv,"MainFrame",&frame,&form);

	_button_widgets[0] = XmCreatePushButton(form,"ButtonWidget1",NULL,0);
	_button_widgets[1] = XmCreatePushButton(form,"ButtonWidget2",NULL,0);
	_button_widgets[2] = XmCreatePushButton(form,"ButtonWidget3",NULL,0);

	_label_widgets[0] = XmCreateLabel(form,"LabelWidget1",NULL,0);
	_label_widgets[1] = XmCreateLabel(form,"LabelWidget2",NULL,0);
	_label_widgets[2] = XmCreateLabel(form,"LabelWidget3",NULL,0);

	_button_gadgets[0] = XmCreatePushButtonGadget(form,"ButtonGadget1",NULL,0);
	_button_gadgets[1] = XmCreatePushButtonGadget(form,"ButtonGadget2",NULL,0);
	_button_gadgets[2] = XmCreatePushButtonGadget(form,"ButtonGadget3",NULL,0);

	_label_gadgets[0] = XmCreateLabelGadget(form,"LabelGadget1",NULL,0);
	_label_gadgets[1] = XmCreateLabelGadget(form,"LabelGadget2",NULL,0);
	_label_gadgets[2] = XmCreateLabelGadget(form,"LabelGadget3",NULL,0);

	XtVaSetValues(_button_gadgets[0],XmNtopWidget,_label_widgets[2],NULL);
	XtVaSetValues(_button_gadgets[1],XmNtopWidget,_button_gadgets[0],NULL);
	XtVaSetValues(_button_gadgets[2],XmNtopWidget,_button_gadgets[1],NULL);

	XtVaSetValues(_label_gadgets[0],XmNtopWidget,_button_gadgets[2],NULL);
	XtVaSetValues(_label_gadgets[1],XmNtopWidget,_label_gadgets[0],NULL);
	XtVaSetValues(_label_gadgets[2],XmNtopWidget,_label_gadgets[1],NULL);

/*  	XtRealizeWidget(form); */

	XtManageChild(_button_widgets[0]);
	XtManageChild(_button_widgets[1]);
	XtManageChild(_button_widgets[2]);

	XtManageChild(_label_widgets[0]);
	XtManageChild(_label_widgets[1]);
	XtManageChild(_label_widgets[2]);

	XtManageChild(_button_gadgets[0]);
	XtManageChild(_button_gadgets[1]);
	XtManageChild(_button_gadgets[2]);

	XtManageChild(_label_gadgets[0]);
	XtManageChild(_label_gadgets[1]);
	XtManageChild(_label_gadgets[2]);

	XtPopup(frame,XtGrabNone);

	XfeToolTipAddTipString(_button_widgets[0]);
	XfeToolTipAddTipString(_button_widgets[1]);
/* 	XfeToolTipAddTipString(_button_widgets[2]); */

	XfeToolTipAddTipString(_label_widgets[0]);
	XfeToolTipAddTipString(_label_widgets[1]);
/* 	XfeToolTipAddTipString(_label_widgets[2]); */

	XfeToolTipAddTipString(_button_gadgets[0]);
	XfeToolTipAddTipString(_button_gadgets[1]);
/* 	XfeToolTipAddTipString(_button_gadgets[2]); */

	XfeToolTipAddTipString(_label_gadgets[0]);
	XfeToolTipAddTipString(_label_gadgets[1]);
/* 	XfeToolTipAddTipString(_label_gadgets[2]); */


	XfeToolTipSetTipStringCallback(_button_widgets[0],
								   tool_tip_cb,
								   NULL);
	
 	XfeToolTipGlobalSetTipStringEnabledState(True);
 	XfeToolTipGlobalSetDocStringEnabledState(True);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static void
tool_tip_cb(Widget		w,
			XtPointer	client_data,
			XtPointer	call_data)
{
	XfeToolTipLabelCallbackStruct * data = 
		(XfeToolTipLabelCallbackStruct *) call_data;

	static int count = 1;

	char buf[256];

	sprintf(buf,"%s_%d","Count",count++);

	data->label_return = XmStringCreateLocalized(buf);
	data->need_to_free_return = True;
}
/*----------------------------------------------------------------------*/
