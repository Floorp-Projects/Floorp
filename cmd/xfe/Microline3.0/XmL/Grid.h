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


#ifndef XmLGridH
#define XmLGridH

#include <XmL/XmL.h>
#include <stdio.h>

#ifdef XmL_CPP
extern "C" {
#endif

extern WidgetClass xmlGridWidgetClass;
typedef struct _XmLGridClassRec *XmLGridWidgetClass;
typedef struct _XmLGridRec *XmLGridWidget;
typedef struct _XmLGridRowRec *XmLGridRow;
typedef struct _XmLGridColumnRec *XmLGridColumn;
typedef struct _XmLGridCellRec *XmLGridCell;

#define XmLIsGrid(w) XtIsSubclass((w), xmlGridWidgetClass)

Widget XmLCreateGrid(Widget parent, char *name, ArgList arglist,
	Cardinal argcount);
void XmLGridAddColumns(Widget w, unsigned char type, int position, int count);
void XmLGridAddRows(Widget w, unsigned char type, int position, int count);
Boolean XmLGridColumnIsVisible(Widget w, int column);
Boolean XmLGridCopyPos(Widget w, Time time, unsigned char rowType, int row,
	unsigned char columnType, int column, int nrow, int ncolumn);
Boolean XmLGridCopySelected(Widget w, Time time);
void XmLGridDeleteAllColumns(Widget w, unsigned char type);
void XmLGridDeleteAllRows(Widget w, unsigned char type);
void XmLGridDeleteColumns(Widget w, unsigned char type, int position,
	int count);
void XmLGridDeleteRows(Widget w, unsigned char type, int position, int count);
void XmLGridDeselectAllCells(Widget w, Boolean notify);
void XmLGridDeselectAllColumns(Widget w, Boolean notify);
void XmLGridDeselectAllRows(Widget w, Boolean notify);
void XmLGridDeselectCell(Widget w, int row, int column, Boolean notify);
void XmLGridDeselectColumn(Widget w, int column, Boolean notify);
void XmLGridDeselectRow(Widget w, int row, Boolean notify);
int XmLGridEditBegin(Widget w, Boolean insert, int row, int column);
void XmLGridEditCancel(Widget w);
void XmLGridEditComplete(Widget w);
XmLGridColumn XmLGridGetColumn(Widget w, unsigned char columnType, int column);
void XmLGridGetFocus(Widget w, int *row, int *column, Boolean *focusIn);
XmLGridRow XmLGridGetRow(Widget w, unsigned char rowType, int row);
int XmLGridGetSelectedCellCount(Widget w);
int XmLGridGetSelectedCells(Widget w, int *rowPositions,
	int *columnPositions, int count);
int XmLGridGetSelectedColumnCount(Widget w);
int XmLGridGetSelectedColumns(Widget w, int *positions, int count);
int XmLGridGetSelectedRow(Widget w);
int XmLGridGetSelectedRowCount(Widget w);
int XmLGridGetSelectedRows(Widget w, int *positions, int count);
void XmLGridMoveColumns(Widget w, int newPosition, int position, int count);
void XmLGridMoveRows(Widget w, int newPosition, int position, int count);
Boolean XmLGridPaste(Widget w);
Boolean XmLGridPastePos(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column);
int XmLGridRead(Widget w, FILE *file, int format, char delimiter);
int XmLGridReadPos(Widget w, FILE *file, int format, char delimiter,
	unsigned char rowType, int row, unsigned char columnType, int column);
void XmLGridRedrawAll(Widget w);
void XmLGridRedrawCell(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column);
void XmLGridRedrawColumn(Widget w, unsigned char type, int column);
void XmLGridRedrawRow(Widget w, unsigned char type, int row);
void XmLGridReorderColumns(Widget w, int *newPositions,
	int position, int count);
void XmLGridReorderRows(Widget w, int *newPositions,
	int position, int count);
int XmLGridRowColumnToXY(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column, Boolean clipped, XRectangle *rect);
Boolean XmLGridRowIsVisible(Widget w, int row);
void XmLGridSelectAllCells(Widget w, Boolean notify);
void XmLGridSelectAllColumns(Widget w, Boolean notify);
void XmLGridSelectAllRows(Widget w, Boolean notify);
void XmLGridSelectCell(Widget w, int row, int column, Boolean notify);
void XmLGridSelectColumn(Widget w, int column, Boolean notify);
void XmLGridSelectRow(Widget w, int row, Boolean notify);
int XmLGridSetFocus(Widget w, int row, int column);
int XmLGridSetStrings(Widget w, char *data);
int XmLGridSetStringsPos(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column, char *data);
int XmLGridWrite(Widget w, FILE *file, int format, char delimiter,
	Boolean skipHidden);
int XmLGridWritePos(Widget w, FILE *file, int format, char delimiter,
	Boolean skipHidden, unsigned char rowType, int row,
	unsigned char columnType, int column, int nrow, int ncolumn);
int XmLGridXYToRowColumn(Widget w, int x, int y, unsigned char *rowType,
	int *row, unsigned char *columnType, int *column);

int XmLGridPosIsResize(Widget g, int x, int y);

void XmLGridSetVisibleColumnCount(Widget w, int num_visible);
void XmLGridHideRightColumn(Widget w);
void XmLGridUnhideRightColumn(Widget w);

int XmLGridGetRowCount(Widget w);
int XmLGridGetColumnCount(Widget w);

/* extern */ void 
XmLGridXYToCellTracking(Widget			widget, 
						int				x, /* input only args. */
						int				y, /* input only args. */
						Boolean *		m_inGrid, /* input/output args. */
						int *			m_lastRow, /* input/output args. */
						int *			m_lastCol, /* input/output args. */
						unsigned char * m_lastRowtype,/* input/output args. */
						unsigned char * m_lastColtype,/* input/output args. */
						int *			outRow, /* output only args. */
						int *			outCol, /* output only args. */
						Boolean *		enter, /* output only args. */
						Boolean *		leave); /* output only args. */

#ifdef XmL_CPP
}
#endif
#endif
