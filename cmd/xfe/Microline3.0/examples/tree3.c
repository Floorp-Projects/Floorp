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

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, tree;
	XmLTreeRowDefinition *rows;
	Pixmap monitorPixmap, spherePixmap;
	Pixel black, white;
	int i, n, size;
	static struct
			{
		Boolean expands;
		int level;
		char *string;
	} data[] =
	{
		{ True,  0, "Root" },
		{ True,  1, "Parent A" },
		{ False, 2, "Node A1" },
		{ False, 2, "Node A2" },
		{ True,  2, "Parent B" },
		{ False, 3, "Node B1" },
		{ False, 3, "Node B2" },
		{ True,  1, "Parent C" },
		{ False, 2, "Node C1" },
		{ True,  1, "Parent D" },
		{ False, 2, "Node D1" },
	};

	shell =  XtAppInitialize(&app, "Tree3", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	black = BlackPixelOfScreen(XtScreen(shell));
	white = WhitePixelOfScreen(XtScreen(shell));
	spherePixmap = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		sphere_bits, sphere_width, sphere_height,
		black, white,
		DefaultDepthOfScreen(XtScreen(shell)));
	monitorPixmap = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		monitor_bits, monitor_width, monitor_height,
		black, white,
		DefaultDepthOfScreen(XtScreen(shell)));

	/* Create a Tree with 3 columns and 1 heading row in multiple */
	/* select mode.  We also set globalPixmapWidth and height here */
	/* which specifys that every Pixmap we set on the Tree will be */
	/* the size specified (16x16).  This will increase performance. */
	tree = XtVaCreateManagedWidget("tree",
		xmlTreeWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNconnectingLineColor, XmRString, "#808080", 8,
		XmNallowColumnResize, True,
		XmNheadingRows, 1,
		XmNvisibleRows, 14,
		XmNcolumns, 3,
		XmNvisibleColumns, 5,
		XmNsimpleWidths, "12c 8c 10c",
		XmNsimpleHeadings, "All Folders|SIZE|DATA2",
		XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
		XmNhighlightRowMode, True,
		XmNglobalPixmapWidth, 16,
		XmNglobalPixmapHeight, 16,
		NULL);

	/* Set default values for new cells (the cells in the content rows) */
	XtVaSetValues(tree,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		NULL);

	/* Create a TreeRowDefinition array from the data array */
	/* and add rows to the Tree */
	n = 11;
	size = sizeof(XmLTreeRowDefinition) * n;
	rows = (XmLTreeRowDefinition *)malloc(size);
	for (i = 0; i < n; i++)
	{
		rows[i].level = data[i].level;
		rows[i].expands = data[i].expands;
		rows[i].isExpanded = True;
		if (data[i].expands)
			rows[i].pixmap = spherePixmap;
		else
			rows[i].pixmap = monitorPixmap;
		rows[i].pixmask = XmUNSPECIFIED_PIXMAP;
		rows[i].string = XmStringCreateSimple(data[i].string);
	}
	XmLTreeAddRows(tree, rows, n, -1);

	/* Free the TreeRowDefintion array we created above and set strings */
	/* in column 1 and 2 */
	for (i = 0; i < n; i++)
	{
		XmStringFree(rows[i].string);
		XmLGridSetStringsPos(tree, XmCONTENT, i, XmCONTENT, 1, "1032|1123");
	}
	free((char *)rows);

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
