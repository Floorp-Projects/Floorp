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
#include <Xm/PushB.h>
#include <Xm/Form.h>
#include <XmL/Grid.h>

void cellSelect();
void showSelected();

#define HEADINGFONT "-*-helvetica-bold-o-*--*-100-*-*-*-*-iso8859-1"
#define CONTENTFONT "-*-helvetica-medium-r-*--*-100-*-*-*-*-iso8859-1"

#define check_width 9
#define check_height 9
static unsigned char check_bits[] = {
	0x00, 0x01, 0x80, 0x01, 0xc0, 0x00, 0x61, 0x00, 0x37, 0x00, 0x3e, 0x00,
	0x1c, 0x00, 0x08, 0x00, 0x00, 0x00};

#define nocheck_width 9
#define nocheck_height 9
static char nocheck_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static char *data[20] = 
{
	"3|Bob Thompson|bobt@teledyne.com",
	"12|Darl Simon|ds@atg.org",
	"14|Jacq Frontier|jf@terrax.com",
	"19|Patty Lee|patlee@isis.com",
	"22|Michal Barnes|mickeyb@softorb.com",
	"23|Dave Schultz|daves@timing.com",
	"23|Eric Stanley|ericst@aldor.com",
	"29|Tim Winters|timw@terra.com",
	"31|Agin Tomitu|agt@umn.edu",
	"33|Betty Tinner|bett@ost.edu",
	"37|Tom Smith|tsmith@netwld.com",
	"38|Rick Wild|raw@mlsoft.com",
	"41|Al Joyce|aj@ulm.edu",
	"41|Tim Burtan|timb@autoc.com",
	"41|George Marlin|gjm@eyeln.com",
	"41|Bill Boxer|billb@idesk.com",
	"41|Maria Montez|marm@ohio.edu",
	"41|Yin Fang|aj@utxs.edu",
	"41|Suzy Saps|ss@umg.edu",
	"41|Jerry Rodgers|jr@lyra.com",
};

Pixmap nocheckPix, checkPix;
Widget grid;

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, button;
	Pixel blackPixel, whitePixel;
	Pixmap pix;
	int i;

	shell =  XtAppInitialize(&app, "Grid4", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	/* Create the pixmaps used for checkmarks */
	blackPixel = BlackPixelOfScreen(XtScreen(shell));
	whitePixel = WhitePixelOfScreen(XtScreen(shell));
	checkPix = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		check_bits, check_width, check_height,
		blackPixel, whitePixel, 
		DefaultDepthOfScreen(XtScreen(shell)));
	nocheckPix = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		nocheck_bits, nocheck_width, nocheck_height,
		blackPixel, whitePixel, 
		DefaultDepthOfScreen(XtScreen(shell)));

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNshadowThickness, 0,
		NULL);

	button = XtVaCreateManagedWidget("Show Selected",
		xmPushButtonWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		NULL);
	XtAddCallback(button, XmNactivateCallback, showSelected, NULL);

	/* Create a Grid with 4 columns.  We set the fontList in this */
	/* function for the Grid to use when calculating the visible rows */
	grid = XtVaCreateManagedWidget("grid",
		xmlGridWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNfontList, XmRString, CONTENTFONT,
		strlen(CONTENTFONT) + 1,
		XmNcolumns, 4,
		XmNsimpleWidths, "3c 4c 17c 20c",
		XmNhorizontalSizePolicy, XmVARIABLE,
		XmNvsbDisplayPolicy, XmSTATIC,
		XmNvisibleRows, 13,
		XmNselectionPolicy, XmSELECT_NONE,
		XmNhighlightRowMode, True,
		XmNshadowType, XmSHADOW_ETCHED_IN,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, button,
		NULL);
	XtAddCallback(grid, XmNselectCallback, cellSelect, NULL);

	/* Freeze the Grid's layout since we will be making changes which */
	/* would cause the Grid to recompute its layout.  The Grid will */
	/* recompute its layout when layoutFrozen is set back to False */
	XtVaSetValues(grid,
		XmNlayoutFrozen, True,
		NULL);

	/* Set defaults for new cells and aligments for new cells in */
	/* columns 1-3.  Then, add a heading row */
	XtVaSetValues(grid, 
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellFontList, XmRString, HEADINGFONT,
		strlen(HEADINGFONT) + 1,
		XtVaTypedArg, XmNcellBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNcellForeground, XmRString, "white", 6,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		NULL);
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumn, 1,
		XmNcellAlignment, XmALIGNMENT_RIGHT,
		NULL);
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumnRangeStart, 2,
		XmNcolumnRangeEnd, 3,
		XmNcellAlignment, XmALIGNMENT_LEFT,
		NULL);
	XmLGridAddRows(grid, XmHEADING, -1, 1);

	/* Set the headings */
	XmLGridSetStrings(grid, "OD|Qty|Name|EMail Addr");

	/* Set defaults for new cells.  Also, set the default cell type */
	/* for cells in column 0 to pixmap cell. Then add the content rows */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellFontList, XmRString, CONTENTFONT,
		strlen(CONTENTFONT) + 1,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNcellForeground, XmRString, "black", 6,
		XmNcellBottomBorderType, XmBORDER_LINE,
		XtVaTypedArg, XmNcellBottomBorderColor, XmRString, "black", 6,
		NULL);
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumn, 0,
		XmNcellType, XmPIXMAP_CELL,
		NULL);
	XmLGridAddRows(grid, XmCONTENT, -1, 20);

	/* Set the content rows, Rows 2, 4, 5, 8 and 13 will have checkmarks */
	for (i = 0; i < 20; i++)
	{
		XmLGridSetStringsPos(grid, XmCONTENT, i, XmCONTENT, 1, data[i]);
		if (i == 2 || i == 4 || i == 5 || i == 8 || i == 13)
			pix = checkPix;
		else
			pix = nocheckPix;
		XtVaSetValues(grid,
			XmNcolumn, 0,
			XmNrow, i,
			XmNcellPixmap, pix,
			NULL);
	}

	XtVaSetValues(grid,
		XmNlayoutFrozen, False,
		NULL);

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}

void showSelected(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	XmLGridRow row;
	XmLGridColumn column;
	Pixmap pix;
	int i, n;

	/* Display the selected rows, these are the rows which have a */
	/* checkPix Pixmap in the first cell in the row */
	printf ("Selected Rows: ");
	XtVaGetValues(grid,
		XmNrows, &n,
		NULL);
	for (i = 0; i < n; i++)
	{
		row = XmLGridGetRow(grid, XmCONTENT, i);
		column = XmLGridGetColumn(grid, XmCONTENT, 0);
		XtVaGetValues(grid,
			XmNrowPtr, row,
			XmNcolumnPtr, column,
			XmNcellPixmap, &pix,
			NULL);
		if (pix == checkPix)
			printf ("%d ", i);
	}
	printf ("\n");
}

void cellSelect(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	XmLGridCallbackStruct *cbs;
	XmLGridRow row;
	XmLGridColumn column;
	Pixmap pix;

	cbs = (XmLGridCallbackStruct *)callData;
	if (cbs->reason != XmCR_SELECT_CELL)
		return;
	if (cbs->rowType != XmCONTENT)
		return;

	/* Toggle the Pixmap in the first cell */
	row = XmLGridGetRow(w, cbs->rowType, cbs->row);
	column = XmLGridGetColumn(w, XmCONTENT, 0);
	XtVaGetValues(w,
		XmNrowPtr, row,
		XmNcolumnPtr, column,
		XmNcellPixmap, &pix,
		NULL);
	if (pix == nocheckPix)
		pix = checkPix;
	else
		pix = nocheckPix;
	XtVaSetValues(w,
		XmNrow, cbs->row,
		XmNcolumn, 0,
		XmNcellPixmap, pix,
		NULL);
}
