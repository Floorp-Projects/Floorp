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
#include <Xm/Label.h>
#include <XmL/Folder.h>

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, folder, form;
	XmString str;
	char buf[20];
	int i;

	shell =  XtAppInitialize(&app, "Folder1", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	folder = XtVaCreateManagedWidget("folder",
		xmlFolderWidgetClass, shell,
		NULL);

	for (i = 0; i < 3; i++)
	{
		/* Add a tab and Form managed by the tab to the Folder */
		sprintf(buf, "Tab %d", i);
		str = XmStringCreateSimple(buf);
		form = XmLFolderAddTabForm(folder, str);
		XmStringFree(str);

		/* Add a Label as a child of the Form */
		sprintf(buf, "Form %d", i);
		XtVaCreateManagedWidget(buf,
			xmLabelWidgetClass, form,
			XmNmarginWidth, 100,
			XmNmarginHeight, 80,
			XmNtopAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			NULL);
	}

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
