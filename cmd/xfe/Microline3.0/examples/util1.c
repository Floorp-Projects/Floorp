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

/*
 * The following source code is part of the Microline Widget Library.
 * The Microline widget library is made available to Mozilla developers
 * under the Netscape Public License (NPL) by Neuron Data.  To learn
 * more about Neuron Data, please visit the Neuron Data Home Page at
 * http://www.neurondata.com.
 */


#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/DrawnB.h>
#include <XmL/XmL.h>

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, button[4][5];
	int i, j;
	static int types[5] =
	{
		XmDRAWNB_ARROW,
		XmDRAWNB_ARROWLINE,
		XmDRAWNB_DOUBLEARROW,
		XmDRAWNB_SQUARE,
		XmDRAWNB_DOUBLEBAR
	};
	static int dirs[4] = 
	{
		XmDRAWNB_RIGHT,
		XmDRAWNB_LEFT,
		XmDRAWNB_UP,
		XmDRAWNB_DOWN
	};

	shell =  XtAppInitialize(&app, "Grid1", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XmNfractionBase, 5,
		XmNshadowThickness, 0,
		NULL);

	for (i = 0 ; i < 4; i++)
		for (j = 0; j < 5; j++)
		{
			button[i][j] = XtVaCreateManagedWidget("drawnB",
				xmDrawnButtonWidgetClass, form,
				XmNtopAttachment, XmATTACH_POSITION,
				XmNtopPosition, i,
				XmNbottomAttachment, XmATTACH_POSITION,
				XmNbottomPosition, i + 1,
				XmNleftAttachment, XmATTACH_POSITION,
				XmNleftPosition, j,
				XmNrightAttachment, XmATTACH_POSITION,
				XmNrightPosition, j + 1,
				XmNwidth, 30,
				XmNheight, 30,
				NULL);
			XmLDrawnButtonSetType(button[i][j], types[j], dirs[i]);
		}

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}

