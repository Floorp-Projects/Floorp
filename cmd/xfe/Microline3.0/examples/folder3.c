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
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <XmL/Folder.h>

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, folder, shellForm, label, button;
	XmString str;
	char buf[20];
	int i;
	static char tabName[6][20] =
	{
		"Standard",
		"PTEL Server",
		"NTEL Server",
		"Advanced",
		"Transfer Address",
		"Multimedia"
	};

	shell =  XtAppInitialize(&app, "Folder3", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNmarginWidth, 10,
		XmNmarginHeight, 10,
		XmNshadowThickness, 0,
		NULL);

	folder = XtVaCreateManagedWidget("folder",
		xmlFolderWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNtabsPerRow, 3,
		XmNmarginWidth, 10,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);

	for (i = 0; i < 6; i++)
	{
		/* Add a tab and Form managed by the tab to the Folder */
		str = XmStringCreateSimple(tabName[i]);
		shellForm = XmLFolderAddTabForm(folder, str);
		XmStringFree(str);

		XtVaSetValues(shellForm,
			XmNmarginWidth, 8,
			XmNmarginHeight, 8,
			NULL);

		/* Add a Label as a child of the Form */
		sprintf(buf, "Label For Page %d", i);
		label = XtVaCreateManagedWidget(buf,
			xmLabelWidgetClass, shellForm,
			XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
			XtVaTypedArg, XmNforeground, XmRString, "black", 6,
			XmNmarginWidth, 100,
			XmNmarginHeight, 80,
			XmNtopAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			NULL);

		/* Add a Button to pages 0 and 1 */
		if (i < 2)
		{
			button = XtVaCreateManagedWidget("Sample Button",
				xmPushButtonWidgetClass, shellForm,
				XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
				XtVaTypedArg, XmNforeground, XmRString, "black", 6,
				XmNbottomAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNmarginWidth, 5,
				NULL);
			XtVaSetValues(label,
				XmNbottomAttachment, XmATTACH_WIDGET,
				XmNbottomWidget, button,
				NULL);
		}
		else
			XtVaSetValues(label,
				XmNbottomAttachment, XmATTACH_FORM,
				NULL);
	}

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
