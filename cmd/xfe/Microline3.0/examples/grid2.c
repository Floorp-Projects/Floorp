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
#include <Xm/PushB.h>
#include <XmL/Grid.h>

void showSelected();

static char *data = 
"Country|Media|Price\n\
Europe|CD-ROM|$29\n\
Yugoslovia|Floppy|$39\n\
North America|Tape|$29\n\
South America|CD-ROM|$49\n\
Japan|Tape|$49\n\
Russia|Floppy|$49\n\
Poland|CD-ROM|$39\n\
Norway|CD-ROM|$29\n\
England|Tape|$49\n\
Jordan|CD-ROM|$39";

Widget grid;

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, button;
	XmString str;

	shell =  XtAppInitialize(&app, "Grid2", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNmarginWidth, 5,
		XmNmarginHeight, 5,
		XmNverticalSpacing, 5,
		XmNshadowThickness, 0,
		NULL);

	str = XmStringCreateSimple("Print Selected");
	button = XtVaCreateManagedWidget("button",
		xmPushButtonWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNlabelString, str,
		NULL);
	XmStringFree(str);
	XtAddCallback(button, XmNactivateCallback, showSelected, NULL);

	/* Create a Grid in multiple row select mode with 1 heading row */
	/* and 3 columns */
	grid = XtVaCreateManagedWidget("grid",
		xmlGridWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XmNheadingRows, 1,
		XmNvisibleRows, 7,
		XmNcolumns, 3,
		XmNsimpleWidths, "20c 10c 10c",
		XmNhorizontalSizePolicy, XmVARIABLE,
		XmNvsbDisplayPolicy, XmSTATIC,
		XmNhighlightRowMode, True,
		XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
		XmNshadowThickness, 0,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, button,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);
	/* Set default cell values for new cells (which will be the */
	/* cells created when we add content rows) */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		NULL);
	/* Set default cell alignment for new cells in columns 0 and 1 */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumnRangeStart, 0,
		XmNcolumnRangeEnd, 1,
		XmNcellAlignment, XmALIGNMENT_LEFT,
		NULL);
	/* Set default cell alignment for new cells in column 2 */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumn, 2,
		XmNcellAlignment, XmALIGNMENT_RIGHT,
		NULL);
	/* Add 10 content rows */
	XtVaSetValues(grid,
		XmNrows, 10,
		NULL);
	XmLGridSetStrings(grid, data);

	XtRealizeWidget(shell);

	XtAppMainLoop(app);
}

void showSelected(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	int i, count, *pos;

	printf ("Selected Rows: ");
	count = XmLGridGetSelectedRowCount(grid);
	if (count)
	{
		pos = (int *)malloc(sizeof(int) * count);
		XmLGridGetSelectedRows(grid, pos, count);
		for (i = 0; i < count; i++)
			printf ("%d ", pos[i]);
		free((char *)pos);
	}
	else
		printf ("none");
	printf ("\n");
}
