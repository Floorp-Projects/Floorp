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
	int i, n, size;
	XmString str;
	XmLTreeRowDefinition *rows;
	static struct
			{
		Boolean expands;
		int level;
		char *string;
	} data[] =
	{
		{ True,  0, "Root" },
		{ True,  1, "Level 1 Parent" },
		{ False, 2, "1st Child of Level 1 Parent" },
		{ False, 2, "2nd Child of Level 1 Parent" },
		{ True,  2, "Level 2 Parent" },
		{ False, 3, "Child of Level 2 Parent" },
		{ True,  1, "Level 1 Parent" },
		{ False, 2, "Child of Level 1 Parent" },
	};

	shell =  XtAppInitialize(&app, "Tree2", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	tree = XtVaCreateManagedWidget("tree",
		xmlTreeWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 6,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNconnectingLineColor, XmRString, "#808080", 8,
		XmNvisibleRows, 10,
		XmNwidth, 250,
		NULL);
	XtVaSetValues(tree,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		NULL);

	/* Create a TreeRowDefinition array from the data array */
	/* and add rows to the Tree */
	n = 8;
	size = sizeof(XmLTreeRowDefinition) * n;
	rows = (XmLTreeRowDefinition *)malloc(size);
	for (i = 0; i < n; i++)
	{
		rows[i].level = data[i].level;
		rows[i].expands = data[i].expands;
		rows[i].isExpanded = True;
		rows[i].pixmap = XmUNSPECIFIED_PIXMAP;
		rows[i].pixmask = XmUNSPECIFIED_PIXMAP;
		rows[i].string = XmStringCreateSimple(data[i].string);
	}
	XmLTreeAddRows(tree, rows, n, -1);

	/* Free the TreeRowDefintion array (and XmStrings) we created above */
	for (i = 0; i < n; i++)
		XmStringFree(rows[i].string);
	free((char *)rows);

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
