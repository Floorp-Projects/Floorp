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
#include <Mrm/MrmPublic.h>
#include <XmL/Folder.h>
#include <XmL/Grid.h>
#include <XmL/Progress.h>
#include <XmL/Tree.h>
#include <stdio.h>

main(argc, argv)
int	argc;
String argv[];
{
	Display *dpy;
	XtAppContext app;
	Widget toplevel, tree, shellForm;
	Pixmap pixmap;
	XmString str;
	MrmHierarchy hier;
	MrmCode clas;
	static char *files[] = {
		"uil1.uid"			};

	MrmInitialize ();
	XtToolkitInitialize();
	app = XtCreateApplicationContext();
	dpy = XtOpenDisplay(app, NULL, argv[0], "Uil1",
		NULL, 0, &argc, argv);
	if (dpy == NULL) {
		fprintf(stderr, "%s:  Can't open display\n", argv[0]);
		exit(1);
	}

	toplevel = XtVaAppCreateShell(argv[0], NULL,
		applicationShellWidgetClass, dpy,
		XmNwidth, 400,
		XmNheight, 300,
		NULL);

	if (MrmOpenHierarchy (1, files, NULL, &hier) != MrmSUCCESS)
		printf ("can't open hierarchy\n");

	MrmRegisterClass(0, NULL, "XmLCreateFolder",
		XmLCreateFolder, xmlFolderWidgetClass);
	MrmRegisterClass(0, NULL, "XmLCreateGrid",
		XmLCreateGrid, xmlGridWidgetClass);
	MrmRegisterClass(0, NULL, "XmLCreateProgress",
		XmLCreateProgress, xmlProgressWidgetClass);
	MrmRegisterClass(0, NULL, "XmLCreateTree",
		XmLCreateTree, xmlTreeWidgetClass);

	if (MrmFetchWidget(hier, "shellForm", toplevel, &shellForm,
		&clas) != MrmSUCCESS)
		printf("can't fetch shellForm\n");

	tree = XtNameToWidget(shellForm, "*tree");

	/* Add two rows to the Tree */
	pixmap = XmUNSPECIFIED_PIXMAP;
	str = XmStringCreateSimple("Root");
	XmLTreeAddRow(tree, 0, True, True, -1, pixmap, pixmap, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Child of Root");
	XmLTreeAddRow(tree, 1, False, False, -1, pixmap, pixmap, str);
	XmStringFree(str);

	XtManageChild(shellForm);
	XtRealizeWidget(toplevel);
	XtAppMainLoop(app);

	return (0);
}
