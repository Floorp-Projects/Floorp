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
#include <Xm/DrawnB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <XmL/Folder.h>

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, folder, tab, folderForm;
	XmString str;
	char pageName[20], tabName[20];
	int i;

	shell =  XtAppInitialize(&app, "Folder2", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNmarginWidth, 8,
		XmNmarginHeight, 8,
		XmNshadowThickness, 0,
		NULL);

	folder = XtVaCreateManagedWidget("folder",
		xmlFolderWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNtabPlacement, XmFOLDER_RIGHT,
		XmNmarginWidth, 10,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);

	for (i = 0; i < 3; i++)
	{
		sprintf(pageName, "Page %d", i);

		/* Add a tab (DrawnButton) to the Folder */
		sprintf(tabName, "Tab %d", i);
		str = XmStringCreateSimple(tabName);
		tab = XtVaCreateManagedWidget("tab",
			xmDrawnButtonWidgetClass, folder,
			XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
			XtVaTypedArg, XmNforeground, XmRString, "black", 6,
			XmNlabelString, str,
			XmNtabManagedName, pageName,
			NULL);
		XmStringFree(str);

		/* Add a Form to the Folder which will appear in the page */
		/* area. This Form will be managed by the tab created above */
		/* because it has the same tabManagedName as the tab widget */
		folderForm = XtVaCreateManagedWidget("folderForm",
			xmFormWidgetClass, folder,
			XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
			XmNtabManagedName, pageName,
			NULL);

		/* Add a Label as a child of the Form */
		XtVaCreateManagedWidget(pageName,
			xmLabelWidgetClass, folderForm,
			XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
			XtVaTypedArg, XmNforeground, XmRString, "black", 6,
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
