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
#include <Xm/PushB.h>
#include <XmL/Folder.h>

void addTab();
void removeTab();
void activate();

#define sphere_width 16
#define sphere_height 16
static unsigned char sphere_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0x38, 0x3f,
	0xb8, 0x3f, 0xf8, 0x3f, 0xf8, 0x3f, 0xf8, 0x3f, 0xf0, 0x1f, 0xe0, 0x0f,
	0xc0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define monitor_width 16
#define monitor_height 16
static unsigned char monitor_bits[] = {
	0x00, 0x00, 0xf8, 0x3f, 0xf8, 0x3f, 0x18, 0x30, 0x58, 0x37, 0x18, 0x30,
	0x58, 0x37, 0x18, 0x30, 0xf8, 0x3f, 0xf8, 0x3f, 0x80, 0x03, 0x80, 0x03,
	0xf0, 0x1f, 0xf0, 0x1f, 0x00, 0x00, 0x00, 0x00};

Widget folder, label;

Pixmap monitorPixmap, spherePixmap;

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, folderForm, addButton, removeButton;
	Pixel black, grey;

	shell =  XtAppInitialize(&app, "Folder4", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	XtVaSetValues(shell,
		XmNallowShellResize, True,
		NULL);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNmarginWidth, 8,
		XmNmarginHeight, 8,
		XmNhorizontalSpacing, 4,
		XmNverticalSpacing, 4,
		XmNshadowThickness, 0,
		NULL);

	/* Create Pixmaps with grey background (from form) */
	black = BlackPixelOfScreen(XtScreen(shell));
	XtVaGetValues(form,
		XmNbackground, &grey,
		NULL);
	spherePixmap = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		sphere_bits, sphere_width, sphere_height,
		black, grey,
		DefaultDepthOfScreen(XtScreen(shell)));
	monitorPixmap = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		monitor_bits, monitor_width, monitor_height,
		black, grey,
		DefaultDepthOfScreen(XtScreen(shell)));

	removeButton = XtVaCreateManagedWidget("Remove Tab",
		xmPushButtonWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNmarginWidth, 10,
		NULL);
	XtAddCallback(removeButton, XmNactivateCallback, removeTab, NULL);

	addButton = XtVaCreateManagedWidget("Add Tab",
		xmPushButtonWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, removeButton,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNmarginWidth, 20,
		NULL);
	XtAddCallback(addButton, XmNactivateCallback, addTab, NULL);

	folder = XtVaCreateManagedWidget("folder",
		xmlFolderWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNtabsPerRow, 4,
		XmNcornerStyle, XmCORNER_NONE,
		XmNspacing, 1,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, removeButton,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNresizePolicy, XmRESIZE_DYNAMIC,
		NULL);
	XtAddCallback(folder, XmNactivateCallback, activate, NULL);

	/* Add a Form to the Folder, this will appear in the page area */
	folderForm = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, folder,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		NULL);

	/* Add a Label as a child of the Form */
	label = XtVaCreateManagedWidget("Page Area",
		xmLabelWidgetClass, folderForm,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNmarginWidth, 100,
		XmNmarginHeight, 80,
		XmNrecomputeSize, False,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}

void addTab(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	Widget tabButton, form;
	int count;
	char tabName[30];
	XmString str;
	Pixmap pixmap;

	XtVaGetValues(folder,
		XmNtabCount, &count,
		NULL);

	/* Every other tab will have a sphere pixmap */
	if (count % 2)
		pixmap = spherePixmap;
	else
		pixmap = monitorPixmap;

	/* Add a tab (DrawnButton) to the Folder */
	sprintf(tabName, "Tab %d", count);
	str = XmStringCreateSimple(tabName);
	tabButton = XtVaCreateManagedWidget("tab",
		xmDrawnButtonWidgetClass, folder,
		XmNlabelString, str,
		XmNmarginWidth, 8,
		XmNtabPixmap, pixmap,
		XmNtabInactivePixmap, pixmap,
		NULL);
	XmStringFree(str);
}

void removeTab(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	int count;
	WidgetList tabs;

	XtVaGetValues(folder,
		XmNtabCount, &count,
		XmNtabWidgetList, &tabs,
		NULL);
	if (!count)
		return;
	XtDestroyWidget(tabs[count - 1]);
}

void activate(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	XmLFolderCallbackStruct *cbs;
	XmString str;
	char buf[20];
	int pos;

	/* Change the Label in the page area to reflect */
	/* the selected position */
	cbs = (XmLFolderCallbackStruct *)callData;
	sprintf(buf, "Page %d", cbs->pos);
	str = XmStringCreateSimple(buf);
	XtVaSetValues(label,
		XmNlabelString, str,
		NULL);
}
