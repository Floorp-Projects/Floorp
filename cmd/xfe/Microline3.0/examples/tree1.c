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
#include <XmL/Tree.h>

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, tree;
	Pixmap pixmap, pixmask;
	XmString str;

	shell =  XtAppInitialize(&app, "Tree1", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	tree = XtVaCreateManagedWidget("tree",
		xmlTreeWidgetClass, shell,
		XmNvisibleRows, 10,
		XmNwidth, 250,
		NULL);

	XtVaSetValues(tree,
		XmNlayoutFrozen, True,
		NULL);

	pixmap = XmUNSPECIFIED_PIXMAP;
	pixmask = XmUNSPECIFIED_PIXMAP;

	str = XmStringCreateSimple("Root");
	XmLTreeAddRow(tree, 0, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Level 1 Parent");
	XmLTreeAddRow(tree, 1, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("1st Child of Level 1 Parent");
	XmLTreeAddRow(tree, 2, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("2nd Child of Level 1 Parent");
	XmLTreeAddRow(tree, 2, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Level 2 Parent");
	XmLTreeAddRow(tree, 2, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Child of Level 2 Parent");
	XmLTreeAddRow(tree, 3, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Level 1 Parent");
	XmLTreeAddRow(tree, 1, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Child of Level 1 Parent");
	XmLTreeAddRow(tree, 2, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);

	XtVaSetValues(tree,
		XmNlayoutFrozen, False,
		NULL);

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
