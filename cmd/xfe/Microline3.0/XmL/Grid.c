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

#include "GridP.h"

#include <Xm/ScrollBar.h>
#include <Xm/Text.h>
#include <Xm/DrawnB.h>
#include <Xm/CutPaste.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef SUNOS4
int fprintf(FILE *, char *, ...);
int fseek(FILE *, long, int);
int fread(char *, int, int, FILE *);
#endif

/* Create and Destroy */
static void ClassInitialize(void);
static void ClassPartInitialize(WidgetClass wc);
static void Initialize(Widget req, Widget newW, ArgList args, Cardinal *nargs);
static void Destroy(Widget w);

/* Geometry, Drawing, Entry and Picking */
static void Realize(Widget w, XtValueMask *valueMask,
	XSetWindowAttributes *attr);
static void Redisplay(Widget w, XExposeEvent *event, Region region);
static void DrawResizeLine(XmLGridWidget g, int xy, int erase);
static void DrawXORRect(XmLGridWidget g, int xy, int size,
	int isVert, int erase);
static void DefineCursor(XmLGridWidget g, char defineCursor);
static void DrawArea(XmLGridWidget g, int type, int row, int col);
static void ExtendSelectRange(XmLGridWidget g, int *type,
	int *fr, int *lr, int *fc, int *lc);
static void ExtendSelect(XmLGridWidget g, XEvent *event, Boolean set,
	int row, int col);
static void SelectTypeArea(XmLGridWidget g, int type, XEvent *event,
	int row, int col, Boolean select, Boolean notify);
static int GetSelectedArea(XmLGridWidget g, int type, int *rowPos,
	int *colPos, int count);
static void ChangeManaged(Widget w);
static void Resize(Widget w);
static void PlaceScrollbars(XmLGridWidget w);
static void VertLayout(XmLGridWidget g, int resizeIfNeeded);
static void HorizLayout(XmLGridWidget g, int resizeIfNeeded);
static void ApplyVisibleRows(XmLGridWidget g);
static void ApplyVisibleCols(XmLGridWidget g);
static void VisPosChanged(XmLGridWidget g, int isVert);
static void RecalcVisPos(XmLGridWidget g, int isVert);
static int PosToVisPos(XmLGridWidget g, int pos, int isVert);
static int VisPosToPos(XmLGridWidget g, int visPos, int isVert);
static unsigned char ColPosToType(XmLGridWidget g, int pos);
static int ColPosToTypePos(XmLGridWidget g, unsigned char type, int pos);
static unsigned char RowPosToType(XmLGridWidget g, int pos);
static int RowPosToTypePos(XmLGridWidget g, unsigned char type, int pos);
static int ColTypePosToPos(XmLGridWidget g, unsigned char type,
	int pos, int allowNeg);
static int RowTypePosToPos(XmLGridWidget g, unsigned char type,
	int pos, int allowNeg);
static int ScrollRowBottomPos(XmLGridWidget g, int row);
static int ScrollColRightPos(XmLGridWidget g, int col);
static int PosIsResize(XmLGridWidget g, int x, int y,
	int *row, int *col, int *isVert);
static int XYToRowCol(XmLGridWidget g, int x, int y, int *row, int *col);
static int RowColToXY(XmLGridWidget g, int row, int col,
	Boolean clipped, XRectangle *rect);
static int RowColFirstSpan(XmLGridWidget g, int row, int col,
	int *spanRow, int *spanCol, XRectangle *rect, Boolean lookLeft,
	Boolean lookUp);
static void RowColSpanRect(XmLGridWidget g, int row, int col, XRectangle *rect);
static XmLGridCell GetCell(XmLGridWidget g, int row, int col);
static int GetColWidth(XmLGridWidget g, int col);
static int GetRowHeight(XmLGridWidget g, int row);
static int ColIsVisible(XmLGridWidget g, int col);
static int RowIsVisible(XmLGridWidget g, int row);
static XtGeometryResult GeometryManager(Widget w, XtWidgetGeometry *request,
	XtWidgetGeometry *);
static void ScrollCB(Widget w, XtPointer, XtPointer);
static int FindNextFocus(XmLGridWidget g, int row, int col,
	int rowDir, int colDir, int *nextRow, int *nextCol);
static int SetFocus(XmLGridWidget g, int row, int col, int rowDir, int colDir);
static void ChangeFocus(XmLGridWidget g, int row, int col);
static void MakeColVisible(XmLGridWidget g, int col);
static void MakeRowVisible(XmLGridWidget g, int row);
static void TextAction(XmLGridWidget g, int action);

/* Getting and Setting Values */
static void GetSubValues(Widget w, ArgList args, Cardinal *nargs);
static void SetSubValues(Widget w, ArgList args, Cardinal *nargs);
static Boolean SetValues(Widget curW, Widget, Widget newW, 
	ArgList args, Cardinal *nargs);
static void CopyFontList(XmLGridWidget g);
static Boolean CvtStringToSizePolicy(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static Boolean CvtStringToSelectionPolicy(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static Boolean CvtStringToRowColType(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static Boolean CvtStringToCellAlignment(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static Boolean CvtStringToCellType(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static Boolean CvtStringToCellBorderType(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static void SetSimpleHeadings(XmLGridWidget g, char *data);
static void SetSimpleWidths(XmLGridWidget g, char *data);

/* Getting and Setting Row Values */
static void GetRowValueMask(XmLGridWidget g, char *s, long *mask);
static void _GetRowValueMask(XmLGridWidget g, char *s, long *mask);
static void GetRowValue(XmLGridWidget g, XmLGridRow row,
	XtArgVal value, long mask);
static void _GetRowValue(XmLGridWidget g, XmLGridRow row,
	XtArgVal value, long mask);
static int SetRowValues(XmLGridWidget g, XmLGridRow row, long mask);
static int _SetRowValues(XmLGridWidget g, XmLGridRow row, long mask);

/* Getting and Setting Column Values */
static void GetColumnValueMask(XmLGridWidget g, char *s, long *mask);
static void _GetColumnValueMask(XmLGridWidget g, char *s, long *mask);
static void GetColumnValue(XmLGridWidget g, XmLGridColumn col,
	XtArgVal value, long mask);
static void _GetColumnValue(XmLGridWidget g, XmLGridColumn col,
	XtArgVal value, long mask);
static int SetColumnValues(XmLGridWidget g, XmLGridColumn col, long mask);
static int _SetColumnValues(XmLGridWidget g, XmLGridColumn col, long mask);

/* Getting and Setting Cell Values */
static void CellValueGetMask(char *s, long *mask);
static void GetCellValue(XmLGridCell cell, XtArgVal value, long mask);
static XmLGridCellRefValues *CellRefValuesCreate(XmLGridWidget g,
	XmLGridCellRefValues *copy);
static void SetCellValuesPreprocess(XmLGridWidget g, long mask);
static int SetCellHasRefValues(long mask);
static int SetCellValuesResize(XmLGridWidget g, XmLGridRow row,
	XmLGridColumn col, XmLGridCell cell, long mask);
static int _SetCellValuesResize(XmLGridWidget g, XmLGridRow row,
	XmLGridColumn col, XmLGridCell cell, long mask);
static void SetCellValues(XmLGridWidget g, XmLGridCell cell, long mask);
static void SetCellRefValues(XmLGridWidget g, XmLGridCellRefValues *values,
	long mask);
static int SetCellRefValuesCompare(void *, void **, void **);
static void SetCellRefValuesPreprocess(XmLGridWidget g, int row, int col,
	XmLGridCell cell, long mask);

/* Read, Write, Copy, Paste */
static int Read(XmLGridWidget g, int format, char delimiter,
	int row, int col, char *data);
static void Write(XmLGridWidget g, FILE *file, int format, char delimiter,
	Boolean skipHidden, int row, int col, int nrow, int ncol);
static char *CopyDataCreate(XmLGridWidget g, int selected, int row, int col,
	int nrow, int ncol);
static Boolean Copy(XmLGridWidget g, Time time, int selected,
	int row, int col, int nrow, int ncol);
static Boolean Paste(XmLGridWidget g, int row, int col);

/* Utility */
static void GetCoreBackground(Widget w, int, XrmValue *value);
static void GetManagerForeground(Widget w, int, XrmValue *value);
static void ClipRectToReg(XmLGridWidget g, XRectangle *rect, GridReg *reg);
static char *FileToString(FILE *file);
static char *CvtXmStringToStr(XmString str);
static XmLGridWidget WidgetToGrid(Widget w, char *funcname);

/* Actions, Callbacks and Handlers */
static void ButtonMotion(Widget w, XEvent *event, String *, Cardinal *);
static void DragTimer(XtPointer, XtIntervalId *);
static void CursorMotion(Widget w, XEvent *event, String *, Cardinal *);
static void Edit(Widget w, XEvent *event, String *, Cardinal *);
static void EditCancel(Widget w, XEvent *event, String *, Cardinal *);
static void EditComplete(Widget w, XEvent *event, String *, Cardinal *);
static void DragStart(Widget w, XEvent *event, String *, Cardinal *);
static Boolean DragConvert(Widget w, Atom *selection, Atom *target,
	Atom *type, XtPointer *value, unsigned long *length, int *format);
static void DragFinish(Widget w, XtPointer clientData, XtPointer callData);
static void DropRegister(XmLGridWidget g, Boolean set);
static void DropStart(Widget w, XtPointer clientData, XtPointer callData);
static void DropTransfer(Widget w, XtPointer clientData, Atom *selType,
	Atom *type, XtPointer value, unsigned long *length, int *format);
static void Select(Widget w, XEvent *event, String *, Cardinal *);
static void TextActivate(Widget w, XtPointer clientData, XtPointer callData);
static void TextFocus(Widget w, XtPointer clientData, XtPointer callData);
static void TextMapped(Widget w, XtPointer closure, XEvent *event,
	Boolean *ctd);
static void TextModifyVerify(Widget w, XtPointer clientData,
	XtPointer callData);
static void Traverse(Widget w, XEvent *event, String *, Cardinal *);

/* XFE Additions */
static void HideColumn(Widget w, XEvent *event, String *, Cardinal *);
static void UnhideColumn(Widget w, XEvent *event, String *, Cardinal *);
static void MenuArm(Widget w, XEvent *event, String *, Cardinal *);
static void MenuDisarm(Widget w, XEvent *event, String *, Cardinal *);

/* XmLGridRow */

static XmLGridRow XmLGridRowNew(Widget grid);
static void XmLGridRowFree(Widget grid, XmLGridRow row);
static XmLGridRow _GridRowNew(Widget grid);
static void _GridRowFree(XmLGridRow row);
static XmLArray XmLGridRowCells(XmLGridRow row);
static int XmLGridRowGetPos(XmLGridRow row);
static int XmLGridRowGetVisPos(XmLGridRow row);
static Boolean XmLGridRowIsHidden(XmLGridRow row);
static Boolean XmLGridRowIsSelected(XmLGridRow row);
static void XmLGridRowSetSelected(XmLGridRow row, Boolean selected);
static void XmLGridRowSetVisPos(XmLGridRow row, int visPos);
static int XmLGridRowHeightInPixels(XmLGridRow row);
static void XmLGridRowHeightChanged(XmLGridRow row);

/* XmLGridColumn */

static XmLGridColumn XmLGridColumnNew(Widget grid);
static void XmLGridColumnFree(Widget grid, XmLGridColumn column);
static XmLGridColumn _GridColumnNew(Widget grid);
static void _GridColumnFree(XmLGridColumn column);
static int XmLGridColumnGetPos(XmLGridColumn column);
static int XmLGridColumnGetVisPos(XmLGridColumn column);
static Boolean XmLGridColumnIsHidden(XmLGridColumn column);
static Boolean XmLGridColumnIsSelected(XmLGridColumn column);
static void XmLGridColumnSetSelected(XmLGridColumn column, Boolean selected);
static void XmLGridColumnSetVisPos(XmLGridColumn column, int visPos);
static int XmLGridColumnWidthInPixels(XmLGridColumn column);
static void XmLGridColumnWidthChanged(XmLGridColumn column);

/* XmLGridCell */

static XmLGridCell XmLGridCellNew();
static void XmLGridCellFree(Widget grid, XmLGridCell cell);
static int XmLGridCellAction(XmLGridCell cell, Widget w,
	XmLGridCallbackStruct *cbs);
static int _GridCellAction(XmLGridCell cell, Widget w,
	XmLGridCallbackStruct *cbs);
static XmLGridCellRefValues *XmLGridCellGetRefValues(XmLGridCell cell);
static void XmLGridCellSetRefValues(XmLGridCell cell,
	XmLGridCellRefValues *values);
static void XmLGridCellDerefValues(XmLGridCellRefValues *values);
static Boolean XmLGridCellInRowSpan(XmLGridCell cell);
static Boolean XmLGridCellInColumnSpan(XmLGridCell cell);
static Boolean XmLGridCellIsSelected(XmLGridCell cell);
static Boolean XmLGridCellIsValueSet(XmLGridCell cell);
static void XmLGridCellSetValueSet(XmLGridCell cell, Boolean set);
static void XmLGridCellSetInRowSpan(XmLGridCell cell, Boolean set);
static void XmLGridCellSetInColumnSpan(XmLGridCell cell, Boolean set);
static void XmLGridCellSetSelected(XmLGridCell cell, Boolean selected);
static void XmLGridCellAllocIcon(XmLGridCell cell);
static void XmLGridCellAllocPixmap(XmLGridCell cell);
static void XmLGridCellSetString(XmLGridCell cell, XmString string,
	Boolean copy);
static XmString XmLGridCellGetString(XmLGridCell cell);
static void XmLGridCellSetToggle(XmLGridCell cell, Boolean state);
static Boolean XmLGridCellGetToggle(XmLGridCell cell);
static void XmLGridCellSetPixmap(XmLGridCell cell, Pixmap pixmap,
	Dimension width, Dimension height);
static void XmLGridCellSetPixmask(XmLGridCell cell, Pixmap pixmask);
static XmLGridCellPixmap *XmLGridCellGetPixmap(XmLGridCell cell);
/* static void XmLGridCellClearTextString(XmLGridCell cell, Widget w); */
static int _XmLGridCellConfigureText(XmLGridCell cell, Widget w,
	XRectangle *rect);
static int _XmLGridCellBeginTextEdit(XmLGridCell cell, Widget w,
	XRectangle *clipRect);
static void _XmLGridCellCompleteTextEdit(XmLGridCell cell, Widget w);
static void _XmLGridCellInsertText(XmLGridCell cell, Widget w);
static int _XmLGridCellGetHeight(XmLGridCell cell, XmLGridRow row);
static int _XmLGridCellGetWidth(XmLGridCell cell, XmLGridColumn col);
static void _XmLGridCellFreeValue(XmLGridCell cell);

static XtActionsRec actions[] =
	{
	{ "XmLGridEditComplete", EditComplete },
	{ "XmLGridButtonMotion", ButtonMotion },
	{ "XmLGridCursorMotion", CursorMotion },
	{ "XmLGridEditCancel",   EditCancel   },
	{ "XmLGridEdit",         Edit         },
	{ "XmLGridSelect",       Select       },
	{ "XmLGridDragStart",    DragStart    },
	{ "XmLGridTraverse",     Traverse     },
	{ "XmLGridHideColumn",   HideColumn   },
	{ "XmLGridUnhideColumn", UnhideColumn },
	/* XFE Additions */
	{ "MenuArm", MenuArm },
	{ "MenuDisarm", MenuDisarm },
	};

#define TEXT_HIDE           1
#define TEXT_SHOW           2
#define TEXT_EDIT           3
#define TEXT_EDIT_CANCEL    4
#define TEXT_EDIT_COMPLETE  5
#define TEXT_EDIT_INSERT    6

/* future defines */
#define XmTOGGLE_CELL  240

/* backwards compatibility defines */
#define XmTEXT_CELL    250
#define XmLABEL_CELL   251

/* Cursors */

#define horizp_width 19
#define horizp_height 13
static unsigned char horizp_bits[] = {
 0x00, 0x00, 0x00, 0xff, 0x07, 0x00, 0xff, 0x07, 0x00, 0x00, 0x06, 0x00,
 0x00, 0x06, 0x00, 0x20, 0x46, 0x00, 0x30, 0xc6, 0x00, 0x38, 0xc6, 0x01,
 0xfc, 0xff, 0x03, 0x38, 0xc6, 0x01, 0x30, 0xc6, 0x00, 0x20, 0x46, 0x00,
 0x00, 0x06, 0x00 };

#define horizm_width 19
#define horizm_height 13
static unsigned char horizm_bits[] = {
 0xff, 0x0f, 0x00, 0xff, 0x0f, 0x00, 0xff, 0x0f, 0x00, 0xff, 0x0f, 0x00,
 0x60, 0x6f, 0x00, 0x70, 0xef, 0x00, 0x78, 0xef, 0x01, 0xfc, 0xff, 0x03,
 0xfe, 0xff, 0x07, 0xfc, 0xff, 0x03, 0x78, 0xef, 0x01, 0x70, 0xef, 0x00,
 0x60, 0x6f, 0x00 };

#define vertp_width 13
#define vertp_height 19
static unsigned char vertp_bits[] = {
   0x06, 0x00, 0x06, 0x00, 0x06, 0x01, 0x86, 0x03, 0xc6, 0x07, 0xe6, 0x0f,
   0x06, 0x01, 0x06, 0x01, 0x06, 0x01, 0xfe, 0x1f, 0xfe, 0x1f, 0x00, 0x01,
   0x00, 0x01, 0x00, 0x01, 0xe0, 0x0f, 0xc0, 0x07, 0x80, 0x03, 0x00, 0x01,
   0x00, 0x00};

#define vertm_width 13
#define vertm_height 19
static unsigned char vertm_bits[] = {
   0x0f, 0x00, 0x0f, 0x01, 0x8f, 0x03, 0xcf, 0x07, 0xef, 0x0f, 0xff, 0x1f,
   0xff, 0x1f, 0x8f, 0x03, 0xff, 0x1f, 0xff, 0x1f, 0xff, 0x1f, 0xff, 0x1f,
   0x80, 0x03, 0xf0, 0x1f, 0xf0, 0x1f, 0xe0, 0x0f, 0xc0, 0x07, 0x80, 0x03,
   0x00, 0x01};

/* Grid Translations */

static char translations[] =
"<Btn1Motion>:           XmLGridButtonMotion()\n\
<MotionNotify>:          XmLGridCursorMotion()\n\
~Ctrl ~Shift <Btn1Down>: XmLGridSelect(BEGIN)\n\
~Ctrl Shift <Btn1Down>:  XmLGridSelect(EXTEND)\n\
Ctrl ~Shift <Btn1Down>:  XmLGridSelect(TOGGLE)\n\
<Btn1Up>:                XmLGridSelect(END)\n\
<Btn2Down>:              XmLGridDragStart()\n\
<EnterWindow>:           ManagerEnter()\n\
<LeaveWindow>:           ManagerLeave()\n\
<FocusOut>:              ManagerFocusOut()\n\
<FocusIn>:               ManagerFocusIn()";

/* Text Translations */

static char traverseTranslations[] =
"~Ctrl ~Shift <Key>osfUp:         XmLGridTraverse(UP)\n\
~Ctrl Shift <Key>osfUp:           XmLGridTraverse(EXTEND_UP)\n\
Ctrl ~Shift <Key>osfUp:           XmLGridTraverse(PAGE_UP)\n\
~Ctrl ~Shift <Key>osfDown:        XmLGridTraverse(DOWN)\n\
~Ctrl Shift <Key>osfDown:         XmLGridTraverse(EXTEND_DOWN)\n\
Ctrl ~Shift <Key>osfDown:         XmLGridTraverse(PAGE_DOWN)\n\
~Ctrl ~Shift <Key>osfLeft:        XmLGridTraverse(LEFT)\n\
~Ctrl Shift <Key>osfLeft:         XmLGridTraverse(EXTEND_LEFT)\n\
Ctrl ~Shift <Key>osfLeft:         XmLGridTraverse(PAGE_LEFT)\n\
~Ctrl ~Shift <Key>osfRight:       XmLGridTraverse(RIGHT)\n\
~Ctrl Shift <Key>osfRight:        XmLGridTraverse(EXTEND_RIGHT)\n\
Ctrl ~Shift <Key>osfRight:        XmLGridTraverse(PAGE_RIGHT)\n\
~Ctrl ~Shift <Key>osfPageUp:      XmLGridTraverse(PAGE_UP)\n\
~Ctrl Shift <Key>osfPageUp:       XmLGridTraverse(EXTEND_PAGE_UP)\n\
Ctrl ~Shift <Key>osfPageUp:       XmLGridTraverse(PAGE_LEFT)\n\
Ctrl Shift <Key>osfPageUp:        XmLGridTraverse(EXTEND_PAGE_LEFT)\n\
~Ctrl Shift <Key>osfPageDown:     XmLGridTraverse(EXTEND_PAGE_DOWN)\n\
Ctrl ~Shift <Key>osfPageDown:     XmLGridTraverse(PAGE_RIGHT)\n\
~Ctrl ~Shift <Key>osfPageDown:    XmLGridTraverse(PAGE_DOWN)\n\
Ctrl Shift <Key>osfPageDown:      XmLGridTraverse(EXTEND_PAGE_RIGHT)\n\
~Ctrl ~Shift <Key>Tab:            XmLGridTraverse(RIGHT)\n\
~Ctrl Shift <Key>Tab:             XmLGridTraverse(LEFT)\n\
~Ctrl ~Shift <Key>Home:           XmLGridTraverse(TO_TOP)\n\
~Ctrl ~Shift <Key>osfBeginLine:   XmLGridTraverse(TO_TOP)\n\
Ctrl ~Shift <Key>Home:            XmLGridTraverse(TO_TOP_LEFT)\n\
~Ctrl ~Shift <Key>End:            XmLGridTraverse(TO_BOTTOM)\n\
~Ctrl ~Shift <Key>osfEndLine:     XmLGridTraverse(TO_BOTTOM)\n\
Ctrl ~Shift <Key>End:             XmLGridTraverse(TO_BOTTOM_RIGHT)\n\
<Key>osfInsert:                   XmLGridEdit()\n\
<Key>F2:                          XmLGridEdit()\n\
~Ctrl ~Shift <KeyDown>space:      XmLGridSelect(BEGIN)\n\
~Ctrl Shift <KeyDown>space:       XmLGridSelect(EXTEND)\n\
Ctrl ~Shift <KeyDown>space:       XmLGridSelect(TOGGLE)\n\
<KeyUp>space:                     XmLGridSelect(END)";

/* You can't put multiple actions for any translation
   where one translation changes the translation table
   XmLGridComplete() and XmLGridCancel() do this and these
   actions can't be combined with others */
static char editTranslations[] =
"~Ctrl ~Shift <Key>osfDown:     XmLGridEditComplete(DOWN)\n\
~Ctrl Shift <Key>Tab:	        XmLGridEditComplete(LEFT)\n\
~Ctrl ~Shift <Key>Tab:	        XmLGridEditComplete(RIGHT)\n\
~Ctrl ~Shift <Key>osfUp:        XmLGridEditComplete(UP)\n\
<Key>osfCancel:                 XmLGridEditCancel()";

static XtResource resources[] =
	{
		/* Grid Resources */
		{
		XmNactivateCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.activateCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNaddCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.addCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNallowColumnHide, XmCAllowColumnHide,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.allowColHide),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNallowColumnResize, XmCAllowColumnResize,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.allowColResize),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNallowDragSelected, XmCAllowDragSelected,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.allowDrag),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNallowDrop, XmCAllowDrop,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.allowDrop),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNallowRowHide, XmCAllowRowHide,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.allowRowHide),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNallowRowResize, XmCAllowRowResize,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.allowRowResize),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNautoSelect, XmCAutoSelect,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.autoSelect),
		XmRImmediate, (XtPointer)True,
		},
		{
		XmNblankBackground, XmCBlankBackground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.blankBg),
		XmRCallProc, (XtPointer)GetCoreBackground,
		},
		{
		XmNbottomFixedCount, XmCBottomFixedCount,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.bottomFixedCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNbottomFixedMargin, XmCBottomFixedMargin,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.bottomFixedMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellDefaults, XmCCellDefaults,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.cellDefaults),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNcellDrawCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.cellDrawCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellDropCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.cellDropCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellFocusCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.cellFocusCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellPasteCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.cellPasteCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcolumns, XmCColumns,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.colCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNdeleteCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.deleteCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNdeselectCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.deselectCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNdebugLevel, XmCDebugLevel,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.debugLevel),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNeditCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.editCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNeditTranslations, XmCTranslations,
		XmRTranslationTable, sizeof(XtTranslations),
		XtOffset(XmLGridWidget, grid.editTrans),
		XmRString, (XtPointer)editTranslations,
		},
		{
		XmNfontList, XmCFontList,
		XmRFontList, sizeof(XmFontList),
		XtOffset(XmLGridWidget, grid.fontList),
		XmRImmediate, (XtPointer)0
		},
		{
		XmNfooterColumns, XmCFooterColumns,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.footerColCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNfooterRows, XmCFooterRows,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.footerRowCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNglobalPixmapHeight, XmCGlobalPixmapHeight,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.globalPixmapHeight),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNglobalPixmapWidth, XmCGlobalPixmapWidth,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.globalPixmapWidth),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNheadingColumns, XmCHeadingColumns,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.headingColCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNheadingRows, XmCHeadingRows,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.headingRowCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNhiddenColumns, XmCHiddenColumns,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.hiddenColCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNhiddenRows, XmCHiddenRows,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.hiddenRowCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNhighlightRowMode, XmCHighlightRowMode,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.highlightRowMode),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNhighlightThickness, XmCHighlightThickness,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.highlightThickness),
		XmRImmediate, (XtPointer)2,
		},
		{
		XmNhorizontalScrollBar, XmCHorizontalScrollBar,
		XmRWidget, sizeof(Widget),
		XtOffset(XmLGridWidget, grid.hsb),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNhorizontalSizePolicy, XmCHorizontalSizePolicy,
		XmRGridSizePolicy, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.hsPolicy),
		XmRImmediate, (XtPointer)XmCONSTANT,
		},
		{
		XmNhsbDisplayPolicy, XmCHsbDisplayPolicy,
		XmRScrollBarDisplayPolicy, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.hsbDisplayPolicy),
		XmRImmediate, (XtPointer)XmAS_NEEDED,
		},
		{
		XmNimmediateDraw, XmCImmediateDraw,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.immediateDraw),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNlayoutFrozen, XmCLayoutFrozen,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.layoutFrozen),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNleftFixedCount, XmCLeftFixedCount,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.leftFixedCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNleftFixedMargin, XmCLeftFixedMargin,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.leftFixedMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNrightFixedCount, XmCRightFixedCount,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.rightFixedCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNrightFixedMargin, XmCRightFixedMargin,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.rightFixedMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNresizeCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.resizeCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNrows, XmCRows,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.rowCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNscrollBarMargin, XmCScrollBarMargin,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.scrollBarMargin),
		XmRImmediate, (XtPointer)2,
		},
		{
		XmNscrollCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.scrollCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNscrollColumn, XmCScrollColumn,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cScrollCol),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNscrollRow, XmCScrollRow,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cScrollRow),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNselectCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLGridWidget, grid.selectCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNselectionPolicy, XmCGridSelectionPolicy,
		XmRGridSelectionPolicy, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.selectionPolicy),
		XmRImmediate, (XtPointer)XmSELECT_BROWSE_ROW,
		},
		{
		XmNselectBackground, XmCSelectBackground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.selectBg),
		XmRCallProc, (XtPointer)GetManagerForeground,
		},
		{
		XmNselectForeground, XmCSelectForeground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.selectFg),
		XmRCallProc, (XtPointer)GetCoreBackground,
		},
		{
		XmNshadowRegions, XmCShadowRegions,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.shadowRegions),
		XmRImmediate, (XtPointer)511,
		},
		{
		XmNshadowType, XmCShadowType,
		XmRShadowType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.shadowType),
		XmRImmediate, (XtPointer)XmSHADOW_IN,
		},
		{
		XmNsimpleHeadings, XmCSimpleHeadings,
		XmRString, sizeof(char *),
		XtOffset(XmLGridWidget, grid.simpleHeadings),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNsimpleWidths, XmCSimpleWidths,
		XmRString, sizeof(char *),
		XtOffset(XmLGridWidget, grid.simpleWidths),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtextWidget, XmCTextWidget,
		XmRWidget, sizeof(Widget),
		XtOffset(XmLGridWidget, grid.text),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtoggleBottomColor, XmCToggleBottomColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.toggleBotColor),
		XmRCallProc, (XtPointer)GetManagerForeground,
		},
		{
		XmNtoggleTopColor, XmCToggleTopColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.toggleTopColor),
		XmRCallProc, (XtPointer)GetManagerForeground,
		},
		{
		XmNtoggleSize, XmCToggleSize,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.toggleSize),
		XmRImmediate, (XtPointer)16,
		},
		{
		XmNtraverseTranslations, XmCTranslations,
		XmRTranslationTable, sizeof(XtTranslations),
		XtOffset(XmLGridWidget, grid.traverseTrans),
		XmRString, (XtPointer)traverseTranslations,
		},
		{
		XmNtopFixedCount, XmCTopFixedCount,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.topFixedCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtopFixedMargin, XmCTopFixedMargin,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.topFixedMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNuseAverageFontWidth, XmCUseAverageFontWidth,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.useAvgWidth),
		XmRImmediate, (XtPointer)True,
		},
		{
		XmNverticalScrollBar, XmCVerticalScrollBar,
		XmRWidget, sizeof(Widget),
		XtOffset(XmLGridWidget, grid.vsb),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNverticalSizePolicy, XmCVerticalSizePolicy,
		XmRGridSizePolicy, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.vsPolicy),
		XmRImmediate, (XtPointer)XmCONSTANT,
		},
		{
		XmNvisibleColumns, XmCVisibleColumns,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.visibleCols),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNvisibleRows, XmCVisibleRows,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.visibleRows),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNvsbDisplayPolicy, XmCVsbDisplayPolicy,
		XmRScrollBarDisplayPolicy, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.vsbDisplayPolicy),
		XmRImmediate, (XtPointer)XmAS_NEEDED,
		},
		/* Row Resources */
		{
		XmNrow, XmCGridRow,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellRow),
		XmRImmediate, (XtPointer)-1,
		},
		{
		XmNrowUserData, XmCUserData,
		XmRPointer, sizeof(XtPointer),
		XtOffset(XmLGridWidget, grid.rowUserData),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNrowHeight, XmCRowHeight,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.rowHeight),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNrowRangeEnd, XmCRowRangeEnd,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellRowRangeEnd),
		XmRImmediate, (XtPointer)-1,
		},
		{
		XmNrowRangeStart, XmCRowRangeStart,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellRowRangeStart),
		XmRImmediate, (XtPointer)-1,
		},
		{
		XmNrowSizePolicy, XmCRowSizePolicy,
		XmRGridSizePolicy, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.rowSizePolicy),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNrowStep, XmCRowStep,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.rowStep),
		XmRImmediate, (XtPointer)1,
		},
		{
		XmNrowType, XmCRowType,
		XmRRowType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.rowType),
		XmRImmediate, (XtPointer)XmINVALID_TYPE,
		},
		/* Column Resources */
		{
		XmNcolumn, XmCGridColumn,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellCol),
		XmRImmediate, (XtPointer)-1,
		},
		{
		XmNcolumnUserData, XmCUserData,
		XmRPointer, sizeof(XtPointer),
		XtOffset(XmLGridWidget, grid.colUserData),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcolumnResizable, XmCColumnResizable,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.colResizable),
		XmRImmediate, (XtPointer)TRUE,
		},
		{
		XmNcolumnWidth, XmCColumnWidth,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.colWidth),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcolumnRangeEnd, XmCColumnRangeEnd,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellColRangeEnd),
		XmRImmediate, (XtPointer)-1,
		},
		{
		XmNcolumnRangeStart, XmCColumnRangeStart,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellColRangeStart),
		XmRImmediate, (XtPointer)-1,
		},
		{
		XmNcolumnSizePolicy, XmCColumnSizePolicy,
		XmRGridSizePolicy, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.colSizePolicy),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcolumnStep, XmCColumnStep,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.colStep),
		XmRImmediate, (XtPointer)1,
		},
		{
		XmNcolumnType, XmCColumnType,
		XmRColumnType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.colType),
		XmRImmediate, (XtPointer)XmINVALID_TYPE,
		},
		/* Cell Resources */
		{
		XmNcellAlignment, XmCCellAlignment,
		XmRCellAlignment, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.cellValues.alignment),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellBackground, XmCCellBackground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.cellValues.background),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellBottomBorderColor, XmCCellBottomBorderColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.cellValues.bottomBorderColor),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellBottomBorderType, XmCCellBottomBorderType,
		XmRCellBorderType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.cellValues.bottomBorderType),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellColumnSpan, XmCCellColumnSpan,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellValues.columnSpan),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellEditable, XmCCellEditable,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.cellValues.editable),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNcellFontList, XmCCellFontList,
		XmRFontList, sizeof(XmFontList),
		XtOffset(XmLGridWidget, grid.cellValues.fontList),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellForeground, XmCCellForeground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.cellValues.foreground),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellLeftBorderColor, XmCCellLeftBorderColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.cellValues.leftBorderColor),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellLeftBorderType, XmCCellLeftBorderType,
		XmRCellBorderType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.cellValues.leftBorderType),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellMarginBottom, XmCCellMarginBottom,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.cellValues.bottomMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellMarginLeft, XmCCellMarginLeft,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.cellValues.leftMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellMarginRight, XmCCellMarginRight,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.cellValues.rightMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellMarginTop, XmCCellMarginTop,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, grid.cellValues.topMargin),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellPixmap, XmCCellPixmap,
		XmRManForegroundPixmap, sizeof(Pixmap),
		XtOffset(XmLGridWidget, grid.cellPixmap),
		XmRImmediate, (XtPointer)XmUNSPECIFIED_PIXMAP,
		},
		{
		XmNcellPixmapMask, XmCCellPixmapMask,
		XtRBitmap, sizeof(Pixmap),
		XtOffset(XmLGridWidget, grid.cellPixmapMask),
		XmRImmediate, (XtPointer)XmUNSPECIFIED_PIXMAP,
		},
		{
		XmNcellRightBorderColor, XmCCellRightBorderColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.cellValues.rightBorderColor),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellRightBorderType, XmCCellRightBorderType,
		XmRCellBorderType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.cellValues.rightBorderType),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellRowSpan, XmCCellRowSpan,
		XmRInt, sizeof(int),
		XtOffset(XmLGridWidget, grid.cellValues.rowSpan),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellString, XmCXmString,
		XmRXmString, sizeof(XmString),
		XtOffset(XmLGridWidget, grid.cellString),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellToggleSet, XmCCellToggleSet,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.cellToggleSet),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNcellTopBorderColor, XmCCellTopBorderColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLGridWidget, grid.cellValues.topBorderColor),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellTopBorderType, XmCCellTopBorderType,
		XmRCellBorderType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.cellValues.topBorderType),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellType, XmCCellType,
		XmRCellType, sizeof(unsigned char),
		XtOffset(XmLGridWidget, grid.cellValues.type),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNcellUserData, XmCUserData,
		XmRPointer, sizeof(XtPointer),
		XtOffset(XmLGridWidget, grid.cellValues.userData),
		XmRImmediate, (XtPointer)0,
		},
		/* Overridden inherited resources */
		{
		XmNshadowThickness, XmCShadowThickness,
		XmRHorizontalDimension, sizeof(Dimension),
		XtOffset(XmLGridWidget, manager.shadow_thickness),
		XmRImmediate, (XtPointer)2,
		},

		{
		XmNshowHideButton, XmCShowHideButton,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.showHideButton),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNuseTextWidget, XmCUseTextWidget,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLGridWidget, grid.useTextWidget),
		XmRImmediate, (XtPointer)True,
		},

    };

XmLGridClassRec xmlGridClassRec =
	{
		{ /* core_class */
		(WidgetClass)&xmManagerClassRec,          /* superclass         */
		"XmLGrid",                                /* class_name         */
		sizeof(XmLGridRec),                       /* widget_size        */
		ClassInitialize,                          /* class_init         */
		ClassPartInitialize,                      /* class_part_init    */
		FALSE,                                    /* class_inited       */
		(XtInitProc)Initialize,                   /* initialize         */
		0,                                        /* initialize_hook    */
		(XtRealizeProc)Realize,                   /* realize            */
		(XtActionList)actions,                    /* actions            */
		(Cardinal)XtNumber(actions),              /* num_actions        */
		resources,                                /* resources          */
		XtNumber(resources),                      /* num_resources      */
		NULLQUARK,                                /* xrm_class          */
		TRUE,                                     /* compress_motion    */
		XtExposeCompressMaximal,                  /* compress_exposure  */
		TRUE,                                     /* compress_enterleav */
		TRUE,                                     /* visible_interest   */
		(XtWidgetProc)Destroy,                    /* destroy            */
		(XtWidgetProc)Resize,                     /* resize             */
		(XtExposeProc)Redisplay,                  /* expose             */
		(XtSetValuesFunc)SetValues,               /* set_values         */
		0,                                        /* set_values_hook    */
		XtInheritSetValuesAlmost,                 /* set_values_almost  */
		(XtArgsProc)GetSubValues,                 /* get_values_hook    */
		0,                                        /* accept_focus       */
		XtVersion,                                /* version            */
		0,                                        /* callback_private   */
		translations,                             /* tm_table           */
		0,                                        /* query_geometry     */
		0,                                        /* display_accelerato */
		0,                                        /* extension          */
		},
		{ /* composite_class */
		(XtGeometryHandler)GeometryManager,       /* geometry_manager   */
		(XtWidgetProc)ChangeManaged,              /* change_managed     */
		XtInheritInsertChild,                     /* insert_child       */
		XtInheritDeleteChild,                     /* delete_child       */
		0,                                        /* extension          */
		},
		{ /* constraint_class */
		0,                                        /* subresources       */
		0,                                        /* subresource_count  */
		sizeof(XmLGridConstraintRec),             /* constraint_size    */
		0,                                        /* initialize         */
		0,                                        /* destroy            */
		0,                                        /* set_values         */
		0,                                        /* extension          */
		},
		{ /* manager_class */
		XtInheritTranslations,                    /* translations       */
		0,                                        /* syn resources      */
		0,                                        /* num syn_resources  */
		0,                                        /* get_cont_resources */
		0,                                        /* num_get_cont_res   */
		XmInheritParentProcess,                   /* parent_process     */
		0,                                        /* extension          */
		},
		{ /* grid_class */
		0,                                        /* initial rows       */
		0,                                        /* initial columns    */
		XmInheritGridPreLayout,                   /* pre layout         */
		sizeof(struct _XmLGridRowRec),            /* row rec size       */
		_GridRowNew,                              /* row new            */
		_GridRowFree,                             /* row free           */
		_GetRowValueMask,                         /* get row value mask */
		_GetRowValue,                             /* get row value      */
		_SetRowValues,                            /* set row values     */
		sizeof(struct _XmLGridColumnRec),         /* column rec size    */
		_GridColumnNew,                           /* column new         */
		_GridColumnFree,                          /* column free        */
		_GetColumnValueMask,                      /* get col value mask */
		_GetColumnValue,                          /* get column value   */
		_SetColumnValues,                         /* set column values  */
		_SetCellValuesResize,                     /* set cell vl resize */
		_GridCellAction,                          /* cell action        */
		}
	};

WidgetClass xmlGridWidgetClass = (WidgetClass)&xmlGridClassRec;

/*
   Create and Destroy
*/

static void 
ClassInitialize(void)
	{
	int off1, off2;

	XmLInitialize();

	XtSetTypeConverter(XmRString, XmRGridSizePolicy,
		CvtStringToSizePolicy, 0, 0, XtCacheNone, 0);
	XtSetTypeConverter(XmRString, XmRColumnType,
		CvtStringToRowColType, 0, 0, XtCacheNone, 0);
	XtSetTypeConverter(XmRString, XmRRowType,
		CvtStringToRowColType, 0, 0, XtCacheNone, 0);
	XtSetTypeConverter(XmRString, XmRGridSelectionPolicy,
		CvtStringToSelectionPolicy, 0, 0, XtCacheNone, 0);
	XtSetTypeConverter(XmRString, XmRCellAlignment,
		CvtStringToCellAlignment, 0, 0, XtCacheNone, 0);
	XtSetTypeConverter(XmRString, XmRCellType,
		CvtStringToCellType, 0, 0, XtCacheNone, 0);
	XtSetTypeConverter(XmRString, XmRCellBorderType,
		CvtStringToCellBorderType, 0, 0, XtCacheNone, 0);
	/* long must be > 2 bytes for cell mask to work */
	if (sizeof(long) < 3)
		fprintf(stderr, "xmlGridClass: fatal error: long < 3 bytes\n");
	/* compiler sanity check - make sure array pos lines up */
	off1 = XtOffset(XmLArrayItem *, pos);
	off2 = XtOffset(XmLGridColumn, grid.pos);
	if (off1 != off2)
		fprintf(stderr, "xmlGridClass: fatal error: column pos offset bad\n");
	off2 = XtOffset(XmLGridRow, grid.pos);
	if (off1 != off2)
		fprintf(stderr, "xmlGridClass: fatal error: row pos offset bad\n");
	}

static void 
ClassPartInitialize(WidgetClass wc)
	{
	XmLGridWidgetClass c, sc;

	c = (XmLGridWidgetClass)wc;
	sc = (XmLGridWidgetClass)c->core_class.superclass;

#define INHERIT_PROC(proc, inherit) \
	if (c->grid_class.proc == inherit) \
		c->grid_class.proc = sc->grid_class.proc;

	INHERIT_PROC(rowNewProc, XmInheritGridRowNew)
	INHERIT_PROC(rowFreeProc, XmInheritGridRowFree)
	INHERIT_PROC(getRowValueMaskProc, XmInheritGridGetRowValueMask)
	INHERIT_PROC(getRowValueProc, XmInheritGridGetRowValue)
	INHERIT_PROC(setRowValuesProc, XmInheritGridSetRowValues)

	INHERIT_PROC(columnNewProc, XmInheritGridColumnNew)
	INHERIT_PROC(columnFreeProc, XmInheritGridColumnFree)
	INHERIT_PROC(getColumnValueMaskProc, XmInheritGridGetColumnValueMask)
	INHERIT_PROC(getColumnValueProc, XmInheritGridGetColumnValue)
	INHERIT_PROC(setColumnValuesProc, XmInheritGridSetColumnValues)

	INHERIT_PROC(setCellValuesResizeProc, XmInheritGridSetCellValuesResize)
	INHERIT_PROC(cellActionProc, XmInheritGridCellAction)

#undef INHERIT_PROC
	}

static void 
Initialize(Widget reqW,
	   Widget newW,
	   ArgList args,
	   Cardinal *narg)
	{
	XmLGridWidget g, request;
	Display *dpy;
	Pixmap pix, pixMask;
	Pixel white, black;
	XColor fg, bg;
	GridReg *reg;
	int i, valid, hc, c, fc, hr, r, fr;
	Boolean layoutFrozen;
#ifdef POINTER_FOCUS_CHECK
	unsigned char kfp;
	Widget shell;
#endif

	g = (XmLGridWidget)newW;
	dpy = XtDisplay((Widget)g);
	request = (XmLGridWidget)reqW;

#ifdef POINTER_FOCUS_CHECK
	shell = XmLShellOfWidget(newW);
	if (shell && XmIsVendorShell(shell)) 
		{
		XtVaGetValues(shell,
			XmNkeyboardFocusPolicy, &kfp,
			NULL);
		if (kfp == XmPOINTER)
			XmLWarning(newW, "keyboardFocusPolicy of XmPOINTER not supported");
		}
#endif

	black = BlackPixelOfScreen(XtScreen((Widget)g));
	white = WhitePixelOfScreen(XtScreen((Widget)g));

	g->grid.rowArray = XmLArrayNew(1, 1);
	g->grid.colArray = XmLArrayNew(1, 1);

	if (g->core.width <= 0) 
		g->core.width = 100;
	if (g->core.height <= 0) 
		g->core.height = 100;

	CopyFontList(g);

	if (g->grid.useTextWidget) {
	  g->grid.text = XtVaCreateManagedWidget("text", xmTextWidgetClass, (Widget)g,
						 XmNmarginHeight, 0,
						 XmNmarginWidth, 3,
						 XmNshadowThickness, 0,
						 XmNhighlightThickness, 0,
						 XmNx, 0,
						 XmNy, 0,
						 XmNwidth, 40,
						 XmNheight, 40,
						 XmNbackground, g->core.background_pixel,
						 XmNforeground, g->manager.foreground,
						 NULL);
	  XtOverrideTranslations(g->grid.text, g->grid.traverseTrans);
	  XtAddEventHandler(g->grid.text, StructureNotifyMask,
			    True, (XtEventHandler)TextMapped, (XtPointer)0);
	  XtAddCallback(g->grid.text, XmNactivateCallback, TextActivate, 0);
	  XtAddCallback(g->grid.text, XmNfocusCallback, TextFocus, 0);
	  XtAddCallback(g->grid.text, XmNlosingFocusCallback, TextFocus, 0);
	  XtAddCallback(g->grid.text, XmNmodifyVerifyCallback, TextModifyVerify, 0);
	}

	g->grid.hsb = XtVaCreateWidget(
		"hsb", xmScrollBarWidgetClass, (Widget)g,
		XmNincrement, 1,
		XmNorientation, XmHORIZONTAL,
		XmNtraversalOn, False,
		XmNbackground, g->core.background_pixel,
/* Don't force foreground on IRIX - it screws up the thumb color in sgiMode */
#ifndef IRIX
		XmNforeground, g->manager.foreground,
#endif
		XmNtopShadowColor, g->manager.top_shadow_color,
		XmNbottomShadowColor, g->manager.bottom_shadow_color,
		NULL);
	XtAddCallback(g->grid.hsb, XmNdragCallback, ScrollCB, 0);
	XtAddCallback(g->grid.hsb, XmNvalueChangedCallback, ScrollCB, 0);
	g->grid.vsb = XtVaCreateWidget(
		"vsb", xmScrollBarWidgetClass, (Widget)g,
		XmNorientation, XmVERTICAL,
		XmNincrement, 1,
		XmNtraversalOn, False,
		XmNbackground, g->core.background_pixel,
/* Don't force foreground on IRIX - it screws up the thumb color in sgiMode */
#ifndef IRIX
		XmNforeground, g->manager.foreground,
#endif
		XmNtopShadowColor, g->manager.top_shadow_color,
		XmNbottomShadowColor, g->manager.bottom_shadow_color,
		NULL);
	XtAddCallback(g->grid.vsb, XmNdragCallback, ScrollCB, 0);
	XtAddCallback(g->grid.vsb, XmNvalueChangedCallback, ScrollCB, 0);

	if (g->grid.showHideButton)
	    {
		g->grid.hideButton = XtVaCreateWidget(
		    "hide", xmDrawnButtonWidgetClass,
   		    (Widget)g, XmNhighlightThickness, 0,
		    XmNshadowThickness, 1,
   		    XmNtraversalOn, False,
  		    XmNbackground, g->core.background_pixel,
		    XmNforeground, g->manager.foreground,
		    XmNtopShadowColor, g->manager.top_shadow_color,
		    XmNbottomShadowColor, g->manager.bottom_shadow_color,
		    NULL);

		XmLDrawnButtonSetType(g->grid.hideButton,
				      XmDRAWNB_DOUBLEARROW,
				      XmDRAWNB_RIGHT);
	    }
	else 
	    {
		g->grid.hideButton = 0;
	    }

	g->grid.realColCount = -1;

	g->grid.inResize = False;

	/* Cursors */
	fg.red = ~0;
	fg.green = ~0;
	fg.blue = ~0;
	fg.pixel = white;
	fg.flags = DoRed | DoGreen | DoBlue;
	bg.red = 0;
	bg.green = 0;
	bg.blue = 0;
	bg.pixel = black;
	bg.flags = DoRed | DoGreen | DoBlue;
    pix = XCreatePixmapFromBitmapData(dpy, DefaultRootWindow(dpy),
		(char *)horizp_bits, horizp_width, horizp_height, 0, 1, 1);
    pixMask = XCreatePixmapFromBitmapData(dpy, DefaultRootWindow(dpy),
		(char *)horizm_bits, horizm_width, horizm_height, 1, 0, 1);
	g->grid.hResizeCursor = XCreatePixmapCursor(dpy, pix, pixMask,
		&fg, &bg, 9, 9);
	XFreePixmap(dpy, pix);
	XFreePixmap(dpy, pixMask);
    pix = XCreatePixmapFromBitmapData(dpy, DefaultRootWindow(dpy),
		(char *)vertp_bits, vertp_width, vertp_height, 0, 1, 1);
    pixMask = XCreatePixmapFromBitmapData(dpy, DefaultRootWindow(dpy),
		(char *)vertm_bits, vertm_width, vertm_height, 1, 0, 1);
	g->grid.vResizeCursor = XCreatePixmapCursor(dpy, pix, pixMask,
		&fg, &bg, 9, 9);
	XFreePixmap(dpy, pix);
	XFreePixmap(dpy, pixMask);

	g->grid.cursorDefined = CursorNormal;
	g->grid.focusIn = 0;
	g->grid.focusRow = -1;
	g->grid.focusCol = -1;
	g->grid.mayHaveRowSpans = 0;
	g->grid.scrollCol = 0;
	g->grid.scrollRow = 0;
	g->grid.textHidden = 1;
	g->grid.inMode = InNormal;
	g->grid.inEdit = 0;
	g->grid.singleColScrollMode = 0;
	g->grid.layoutStack = 0;
	g->grid.needsHorizLayout = 0;
	g->grid.needsVertLayout = 0;
	g->grid.recalcHorizVisPos = 0;
	g->grid.recalcVertVisPos = 0;
	g->grid.vertVisChangedHint = 0;
	g->grid.defCellValues = CellRefValuesCreate(g, 0);
	g->grid.defCellValues->refCount = 1;
	g->grid.ignoreModifyVerify = 0;
	g->grid.extendRow = -1;
	g->grid.extendCol = -1;
	g->grid.extendToRow = -1;
	g->grid.extendToCol = -1;
	g->grid.extendSelect = True;
	g->grid.lastSelectRow = -1;
	g->grid.lastSelectCol = -1;
	g->grid.lastSelectTime = 0;
	g->grid.dragTimerSet = 0;
	g->grid.gc = 0;
	reg = g->grid.reg;
	for (i = 0; i < 9; i++)
			{
			reg[i].x = 0;
			reg[i].y = 0;
			reg[i].width = 0;
			reg[i].height = 0;
			reg[i].row = 0;
			reg[i].col = 0;
			reg[i].nrow = 0;
			reg[i].ncol = 0;
			}

	layoutFrozen = g->grid.layoutFrozen;
	g->grid.layoutFrozen = True;

	if (g->grid.hiddenColCount || g->grid.hiddenRowCount)
		{
		XmLWarning(newW, "Initialize() - can't set hidden rows or columns");
		g->grid.hiddenColCount = 0;
		g->grid.hiddenRowCount = 0;
		}
	hc = g->grid.headingColCount;
	c = XmLGridClassPartOfWidget(g).initialCols;
	if (c < g->grid.colCount)
		c = g->grid.colCount;
	fc = g->grid.footerColCount;
	hr = g->grid.headingRowCount;
	r = XmLGridClassPartOfWidget(g).initialRows;
	if (r < g->grid.rowCount)
		r = g->grid.rowCount ;
	fr = g->grid.footerRowCount;
	g->grid.headingColCount = 0;
	g->grid.colCount = 0;
	g->grid.footerColCount = 0;
	g->grid.headingRowCount = 0;
	g->grid.rowCount = 0;
	g->grid.footerRowCount = 0;
	XmLGridAddColumns(newW, XmHEADING, -1, hc);
	XmLGridAddColumns(newW, XmCONTENT, -1, c);
	XmLGridAddColumns(newW, XmFOOTER, -1, fc);
	XmLGridAddRows(newW, XmHEADING, -1, hr);
	XmLGridAddRows(newW, XmCONTENT, -1, r);
	XmLGridAddRows(newW, XmFOOTER, -1, fr);
	if (g->grid.simpleHeadings)
		{
		g->grid.simpleHeadings = (char *)strdup(g->grid.simpleHeadings);
		SetSimpleHeadings(g, g->grid.simpleHeadings);
		}
	if (g->grid.simpleWidths)
		{
		g->grid.simpleWidths = (char *)strdup(g->grid.simpleWidths);
		SetSimpleWidths(g, g->grid.simpleWidths);
		}
	if (g->grid.visibleRows)
		ApplyVisibleRows(g);
	if (g->grid.visibleCols)
		ApplyVisibleCols(g);

	g->grid.layoutFrozen = layoutFrozen;
	VertLayout(g, 1);
	HorizLayout(g, 1);
	PlaceScrollbars(g);

	valid = 1;
	for (i = 0; i < *narg; i++)
		{
		if (!args[i].name)
			continue;
		if (!strcmp(args[i].name, XmNrows) ||
			!strcmp(args[i].name, XmNcolumns))
			continue;
		if (!strncmp(args[i].name, "row", 3) ||
			!strncmp(args[i].name, "column", 6) ||
			!strncmp(args[i].name, "cell", 4))
			valid = 0;
		}
	if (!valid)
		XmLWarning(newW,
			"Initialize() - can't set row,column or cell values in init");

	DropRegister(g, g->grid.allowDrop);
	}

static void
Destroy(Widget w)
	{
	XmLGridWidget g;
	Display *dpy;
	int i, count;

	g = (XmLGridWidget)w;
	dpy = XtDisplay(w);
	if (g->grid.dragTimerSet)
		XtRemoveTimeOut(g->grid.dragTimerId);
	DefineCursor(g, CursorNormal);
	XFreeCursor(dpy, g->grid.hResizeCursor);
	XFreeCursor(dpy, g->grid.vResizeCursor);
	if (g->grid.gc)
		{
		XFreeGC(dpy, g->grid.gc);
		XFreeFont(dpy, g->grid.fallbackFont);
		}
	XmFontListFree(g->grid.fontList);
	XmLGridCellDerefValues(g->grid.defCellValues);
	ExtendSelect(g, (XEvent *)0, False, -1, -1);
	count = XmLArrayGetCount(g->grid.rowArray);
	for (i = 0; i < count; i++)
		XmLGridRowFree(w, (XmLGridRow)XmLArrayGet(g->grid.rowArray, i));
	XmLArrayFree(g->grid.rowArray);
	count = XmLArrayGetCount(g->grid.colArray);
	for (i = 0; i < count; i++)
		XmLGridColumnFree(w, (XmLGridColumn)XmLArrayGet(g->grid.colArray, i));
	XmLArrayFree(g->grid.colArray);
	if (g->grid.simpleHeadings)
		free((char *)g->grid.simpleHeadings);
	if (g->grid.simpleWidths)
		free((char *)g->grid.simpleWidths);
	}

/*
   Geometry, Drawing, Entry and Picking
*/

static void 
Realize(Widget w,
	XtValueMask *valueMask,
	XSetWindowAttributes *attr)
	{
	XmLGridWidget g;
	Display *dpy;
	WidgetClass superClass;
	XtRealizeProc realize;
	XGCValues values;
	XtGCMask mask;
	static char dashes[2] = { 1, 1 };

	g = (XmLGridWidget)w;
	dpy = XtDisplay(g);
	superClass = xmlGridWidgetClass->core_class.superclass;
	realize = superClass->core_class.realize;
	(*realize)(w, valueMask, attr);

	if (!g->grid.gc)
		{
		g->grid.fallbackFont = XLoadQueryFont(dpy, "fixed");
		values.foreground = g->manager.foreground;
		values.font = g->grid.fallbackFont->fid;
		mask = GCForeground | GCFont;
		g->grid.gc = XCreateGC(dpy, XtWindow(g), mask, &values);
		XSetDashes(dpy, g->grid.gc, 0, dashes, 2);
		if (g->grid.selectionPolicy == XmSELECT_BROWSE_ROW &&
			g->grid.autoSelect == True &&
			g->grid.rowCount)
			XmLGridSelectRow(w, 0, False);
		}
	}

static void
Redisplay(Widget w,
	  XExposeEvent *event,
	  Region region)
	{
	XmLGridWidget g;
	Display *dpy;
	Window win;
	XmLGridCell cell;
	XmLGridRow row;
	XmLGridColumn col;
	XmLGridCellRefValues *cellValues;
	XmLGridDrawStruct ds;
	XmLGridCallbackStruct cbs;
	GridReg *reg;
	XRectangle eRect, rRect, clipRect, rect[6];
	int i, n, st, c, r, sc, sr, width, height, rowHeight;
	int lastVisPos, visPos, hasDrawCB;
	Boolean spanUp, spanLeft;

	g = (XmLGridWidget)w;
	if (!XtIsRealized((Widget)g))
		return;
	if (!g->core.visible)
		return;
	if (g->grid.layoutFrozen == True)
		XmLWarning(w, "Redisplay() - layout frozen is True during redraw");
	dpy = XtDisplay(g);
	win = XtWindow(g);
	st = g->manager.shadow_thickness;
	reg = g->grid.reg;
	if (event)
		{
		eRect.x = event->x;
		eRect.y = event->y;
		eRect.width = event->width;
		eRect.height = event->height;
		if (g->grid.debugLevel > 1)
			fprintf(stderr, "XmLGrid: Redisplay x %d y %d w %d h %d\n",
				event->x, event->y, event->width, event->height);
		}
	else
		{
		eRect.x = 0;
		eRect.y = 0;
		eRect.width = g->core.width;
		eRect.height = g->core.height;
		}
	if (!eRect.width || !eRect.height)
		return;
	/* Hide any XORed graphics */
	DrawResizeLine(g, 0, 1);
	hasDrawCB = 0;
	if (XtHasCallbacks(w, XmNcellDrawCallback) == XtCallbackHasSome)
			hasDrawCB = 1;
	for (i = 0; i < 9; i++)
		{
		if (g->grid.debugLevel > 1)
			fprintf(stderr, "XmLGrid: Redisplay region %d phase 0\n", i);
		if (!reg[i].width || !reg[i].height)
			continue;
		rRect.x = reg[i].x;
		rRect.y = reg[i].y;
		rRect.width = reg[i].width;
		rRect.height = reg[i].height;
		if (XmLRectIntersect(&eRect, &rRect) == XmLRectOutside)
			continue;
		if (g->grid.debugLevel > 1)
			fprintf(stderr, "XmLGrid: Redisplay region %d phase 1\n", i);
		rRect.x += st;
		rRect.width -= st * 2;
		rRect.y += st;
		rRect.height -= st * 2;
		if (XmLRectIntersect(&eRect, &rRect) != XmLRectInside &&
			g->manager.shadow_thickness)
			{
			if (g->grid.shadowRegions & (1 << i))
#ifdef MOTIF11
				_XmDrawShadow(dpy, win,
					g->manager.bottom_shadow_GC,
					g->manager.top_shadow_GC,
					g->manager.shadow_thickness,
					reg[i].x, reg[i].y,
					reg[i].width, reg[i].height);
#else
				_XmDrawShadows(dpy, win,
					g->manager.top_shadow_GC,
					g->manager.bottom_shadow_GC,
					reg[i].x, reg[i].y,
					reg[i].width, reg[i].height,
					g->manager.shadow_thickness,
					g->grid.shadowType);
#endif
			else
#ifdef MOTIF11
				_XmEraseShadow(dpy, win,
					g->manager.shadow_thickness,
					reg[i].x, reg[i].y,
					reg[i].width, reg[i].height);
#else
				_XmClearBorder(dpy, win,
					reg[i].x, reg[i].y,
					reg[i].width, reg[i].height,
					g->manager.shadow_thickness);
#endif
			}
		rRect.x += st;
		height = 0;
		if (g->grid.debugLevel > 1)
			fprintf(stderr, "XmLGrid: Redisplay region %d phase 2\n", i);
		for (r = reg[i].row; r < reg[i].row + reg[i].nrow; r++)
			{
			rowHeight = GetRowHeight(g, r);
			if (!rowHeight && !g->grid.mayHaveRowSpans)
				continue;
			width = 0;
			for (c = reg[i].col; c < reg[i].col + reg[i].ncol; c++)
				{
				rRect.x = reg[i].x + st + width;
				rRect.y = reg[i].y + st + height;
				if (g->grid.singleColScrollMode)
					rRect.x -= g->grid.singleColScrollPos;
				rRect.width = GetColWidth(g, c);
				rRect.height = rowHeight;
				width += rRect.width;
				cell = GetCell(g, r, c);
				if (!cell)
					continue;
				cellValues = XmLGridCellGetRefValues(cell);

				spanUp = False;
				spanLeft = False;
				if (XmLGridCellInRowSpan(cell))
					{
					if (r == reg[i].row)
						{
						spanUp = True;
						if (c == reg[i].col)
							spanLeft = True;
						}
					else
						continue;
					}
				if (XmLGridCellInColumnSpan(cell))
					{
					if (c == reg[i].col)
						spanLeft = True;
					else
						continue;
					}
				sr = r;
				sc = c;
				if (spanUp == True || spanLeft == True ||
					cellValues->rowSpan || cellValues->columnSpan)
					{
					if (RowColFirstSpan(g, r, c, &sr, &sc, &rRect,
						spanLeft, spanUp) == -1)
						continue;
					RowColSpanRect(g, sr, sc, &rRect);
					}
				if (!rRect.width || !rRect.height)
					continue;
				clipRect = rRect;
				ClipRectToReg(g, &clipRect, &reg[i]);
				if (!clipRect.width || !clipRect.height)
					continue;
				if (event && XRectInRegion(region, clipRect.x, clipRect.y,
						clipRect.width, clipRect.height) == RectangleOut)
					continue;
				cell = GetCell(g, sr, sc);
				if (!cell)
					continue;
				cellValues = XmLGridCellGetRefValues(cell);
				cbs.reason = XmCR_CELL_DRAW;
				cbs.event = (XEvent *)event;
				cbs.rowType = RowPosToType(g, sr);
				cbs.row = RowPosToTypePos(g, cbs.rowType, sr);
				cbs.columnType = ColPosToType(g, sc);
				cbs.column = ColPosToTypePos(g, cbs.columnType, sc);
				cbs.clipRect = &clipRect;
				cbs.drawInfo = &ds;
				ds.gc = g->grid.gc;
				ds.cellRect = &rRect;
				ds.topMargin = cellValues->topMargin;
				ds.bottomMargin = cellValues->bottomMargin;
				ds.leftMargin = cellValues->leftMargin;
				ds.rightMargin = cellValues->rightMargin;
				ds.background = cellValues->background;
				ds.foreground = cellValues->foreground;
				ds.fontList = cellValues->fontList;
				ds.alignment = cellValues->alignment;
				ds.selectBackground = g->grid.selectBg;
				ds.selectForeground = g->grid.selectFg;
				row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, sr);
				col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, sc);
				ds.drawFocusType = XmDRAW_FOCUS_NONE;
				if (g->grid.focusRow == sr &&
					g->grid.highlightRowMode == True &&
					g->grid.focusIn)
					{
					RecalcVisPos(g, 0);
					visPos = XmLGridColumnGetVisPos(col);
					lastVisPos = XmLArrayGetCount(g->grid.colArray) -
						g->grid.hiddenColCount - 1;
					if (visPos == 0 && visPos == lastVisPos)
						ds.drawFocusType = XmDRAW_FOCUS_CELL;
					else if (visPos == 0)
						ds.drawFocusType = XmDRAW_FOCUS_LEFT;
					else if (visPos == lastVisPos)
						ds.drawFocusType = XmDRAW_FOCUS_RIGHT;
					else
						ds.drawFocusType = XmDRAW_FOCUS_MID;
					}
				if (g->grid.focusRow == sr &&
					g->grid.focusCol == sc &&
					g->grid.highlightRowMode == False &&
					g->grid.focusIn)
					ds.drawFocusType = XmDRAW_FOCUS_CELL;
				if (XmLGridRowIsSelected(row) == True ||
					XmLGridColumnIsSelected(col) == True ||
					XmLGridCellIsSelected(cell) == True)
					ds.drawSelected = True;
				else
					ds.drawSelected = False;
				if (g->grid.selectionPolicy == XmSELECT_CELL &&
					g->grid.focusIn && g->grid.focusRow == sr &&
					g->grid.focusCol == sc)
					ds.drawSelected = False;
				ds.stringDirection = g->manager.string_direction;
				XmLGridCellAction(cell, (Widget)g, &cbs);
				if (hasDrawCB)
					XtCallCallbackList(w, g->grid.cellDrawCallback,
						(XtPointer)&cbs);
				}
			height += rowHeight;
			}
		}
	if (g->grid.debugLevel > 1)
		fprintf(stderr, "XmLGrid: Redisplay non-cell areas\n");
	n = 0;
	if (reg[0].width && g->grid.leftFixedMargin)
		{
		rect[n].x = reg[0].width;
		rect[n].y = 0;
		rect[n].width = g->grid.leftFixedMargin;
		rect[n].height = g->core.height;
		n++;
		}
	if (reg[2].width && g->grid.rightFixedMargin)
		{
		width = 0;
		if (reg[0].ncol)
			width += reg[0].width + g->grid.leftFixedMargin;
		if (reg[1].ncol)
			width += reg[1].width;
		rect[n].x = width;
		rect[n].y = 0;
		rect[n].width = g->grid.rightFixedMargin;
		rect[n].height = g->core.height;
		n++;
		}
	if (reg[0].height && g->grid.topFixedMargin)
		{
		rect[n].x = 0;
		rect[n].y = reg[0].height;
		rect[n].width = g->core.width;
		rect[n].height = g->grid.topFixedMargin;
		n++;
		}
	if (reg[6].height && g->grid.bottomFixedMargin)
		{
		rect[n].x = 0;
		height = 0;
		if (reg[0].nrow)
			height += reg[0].height + g->grid.topFixedMargin;
		if (reg[3].nrow)
			height += reg[3].height;
		rect[n].y = height;
		rect[n].width = g->core.width;
		rect[n].height = g->grid.bottomFixedMargin;
		n++;
		}
	width = reg[1].width;
	if (reg[0].ncol)
		width += reg[0].width + g->grid.leftFixedMargin;
	if (reg[2].ncol)
		width += g->grid.rightFixedMargin + reg[2].width;
	if (width < (int)g->core.width)
		{
		rect[n].x = width;
		rect[n].y = 0;
		rect[n].width = g->core.width - width;
		rect[n].height = g->core.height;
		n++;
		}
	height = reg[3].height;
	if (reg[0].nrow)
		height += reg[0].height + g->grid.topFixedMargin;
	if (reg[6].nrow)
		height += g->grid.bottomFixedMargin + reg[6].height;
	if (height < (int)g->core.height)
		{
		rect[n].x = 0;
		rect[n].y = height;
		rect[n].width = g->core.width;
		rect[n].height = g->core.height - height;
		n++;
		}
	for (i = 0; i < n; i++)
		{
		if (XmLRectIntersect(&eRect, &rect[i]) == XmLRectOutside)
			continue;
		XClearArea(dpy, win, rect[i].x, rect[i].y, rect[i].width,
			rect[i].height, False);
		}
	n = 0;
	if (reg[1].width)
		{
		width = 0;
		for (c = reg[1].col; c < reg[1].col + reg[1].ncol; c++)
			width += GetColWidth(g, c);
		for (i = 1; i < 9; i += 3)
			if (reg[i].height && width < reg[i].width - st * 2)
				{
				rect[n].x = reg[i].x + st + width;
				rect[n].y = reg[i].y + st;
				rect[n].width = reg[i].x + reg[i].width -
					rect[n].x - st;
				rect[n].height = reg[i].height - st * 2;
				n++;
				}
		}
	if (reg[3].height)
		{
		height = 0;
		for (r = reg[3].row; r < reg[3].row + reg[3].nrow; r++)
			height += GetRowHeight(g, r);
		for (i = 3; i < 6; i++)
			if (reg[i].width && height < reg[i].height - st * 2)
				{
				rect[n].x = reg[i].x + st;
				rect[n].y = reg[i].y + st + height;
				rect[n].width = reg[i].width - st * 2;
				rect[n].height = reg[i].y + reg[i].height -
					rect[n].y - st;
				n++;
				}
		}
	XSetForeground(dpy, g->grid.gc, g->grid.blankBg);
	for (i = 0; i < n; i++)
		{
		if (XmLRectIntersect(&eRect, &rect[i]) == XmLRectOutside)
			continue;
		XFillRectangle(dpy, win, g->grid.gc, rect[i].x, rect[i].y,
			rect[i].width, rect[i].height);
		}
	/* Show any XORed graphics */
	DrawResizeLine(g, 0, 1);
	}

static void
DrawResizeLine(XmLGridWidget g,
	       int xy,
	       int erase)
	{
	if (g->grid.inMode != InResize)
		return;
	DrawXORRect(g, xy, 2, g->grid.resizeIsVert, erase);
	}

static void
DrawXORRect(XmLGridWidget g,
	    int xy,
	    int size,
	    int isVert,
	    int erase)
	{
	Display *dpy;
	Window win;
	GC gc;
	Pixel black, white;
	int oldXY, maxX, maxY;

	if (!XtIsRealized((Widget)g))
		return;
	/* erase is (0 == none) (1 == hide/show) (2 == permenent erase) */
	dpy = XtDisplay(g);
	win = XtWindow(g);
	gc = g->grid.gc;
	XSetFunction(dpy, gc, GXinvert);
	black = BlackPixelOfScreen(XtScreen((Widget)g));
	white = WhitePixelOfScreen(XtScreen((Widget)g));
	XSetForeground(dpy, gc, black ^ white);
	maxX = g->core.width;
	if (XtIsManaged(g->grid.vsb))
		maxX -= g->grid.vsb->core.width + g->grid.scrollBarMargin;
	maxY = g->core.height;
	if (XtIsManaged(g->grid.hsb))
		maxY -= g->grid.hsb->core.height + g->grid.scrollBarMargin;
	oldXY = g->grid.resizeLineXY;
	if (isVert)
		{
		if (oldXY != -1)
			XFillRectangle(dpy, win, gc, 0, oldXY, maxX, size);
		}
	else
		{
		if (oldXY != -1)
			XFillRectangle(dpy, win, gc, oldXY, 0, size, maxY);
		}
	if (!erase)
		{
		if (isVert)
			{
			if (xy > maxY)
				xy = maxY - 2;
			if (xy < 0)
				xy = 0;
			XFillRectangle(dpy, win, gc, 0, xy, maxX, size);
			}
		else
			{
			if (xy > maxX)
				xy = maxX - 2;
			if (xy < 0)
				xy = 0;
			XFillRectangle(dpy, win, gc, xy, 0, size, maxY);
			}
		g->grid.resizeLineXY = xy;
		}
	else if (erase == 2)
		g->grid.resizeLineXY = -1;
	XSetFunction(dpy, gc, GXcopy);
	}

static void 
DefineCursor(XmLGridWidget g,
	     char defineCursor)
	{
	Display *dpy;
	Window win;

	if (!XtIsRealized((Widget)g))
		return;
	dpy = XtDisplay(g);
	win = XtWindow(g);
	if (defineCursor != g->grid.cursorDefined)
		XUndefineCursor(dpy, win);
	if (defineCursor == CursorVResize)
		XDefineCursor(dpy, win, g->grid.vResizeCursor);
	else if (defineCursor == CursorHResize)
		XDefineCursor(dpy, win, g->grid.hResizeCursor);
	g->grid.cursorDefined = defineCursor;
	}

static void
DrawArea(XmLGridWidget g,
	 int type,
	 int row,
	 int col)
	{
	GridReg *reg;
	Display *dpy;
	Window win;
	XExposeEvent event;
	XRectangle rect[3];
	Region region;
	int i, j, n;
	Dimension width, height, st;

	if (g->grid.layoutFrozen == True)
		return;
	if (!XtIsRealized((Widget)g))
		return;
	if (!g->core.visible)
		return;
	dpy = XtDisplay(g);
	win = XtWindow(g);
	reg = g->grid.reg;
	st = g->manager.shadow_thickness;
	if (g->grid.debugLevel > 1)
		fprintf(stderr, "XmLGrid: DrawArea %d %d %d\n", type, row, col);

	n = 0;
	switch (type)
	{
	case DrawAll:
		{
		rect[n].x = 0;
		rect[n].y = 0;
		rect[n].width = g->core.width;
		rect[n].height = g->core.height;
		n++;
		break;
		}
	case DrawHScroll:
		{
		for (i = 1; i < 9; i += 3)
			{
			if (!reg[i].width || !reg[i].height)
				continue;
			rect[n].x = reg[i].x + st;
			rect[n].y = reg[i].y + st;
			rect[n].width = reg[i].width - st * 2;
			rect[n].height = reg[i].height - st * 2;
			n++;
			}
		break;
		}
	case DrawVScroll:
		{
		for (i = 3; i < 6; i++)
			{
			if (!reg[i].width || !reg[i].height)
				continue;
			rect[n].x = reg[i].x + st;
			rect[n].y = reg[i].y + st;
			rect[n].width = reg[i].width - st * 2;
			rect[n].height = reg[i].height - st * 2;
			n++;
			}
		break;
		}
	case DrawRow:
		{
		for (i = 0; i < 9; i++)
			{
			if (!reg[i].width || !reg[i].height)
				continue;
			if (!(row >= reg[i].row &&
				row < reg[i].row + reg[i].nrow))
				continue;
			height = 0;
			for (j = reg[i].row; j < row; j++)
				height += GetRowHeight(g, j);
			rect[n].x = reg[i].x + st;
			rect[n].y = reg[i].y + st + height;
			rect[n].width = reg[i].width - st * 2;
			rect[n].height = GetRowHeight(g, row);
			ClipRectToReg(g, &rect[n], &reg[i]);
			n++;
			}
		break;
		}
	case DrawCol:
		{
		for (i = 0; i < 9; i++)
			{
			if (!reg[i].width || !reg[i].height)
				continue;
			if (!(col >= reg[i].col &&
				col < reg[i].col + reg[i].ncol))
				continue;
			width = 0;
			for (j = reg[i].col; j < col; j++)
				width += GetColWidth(g, j);
			rect[n].x = reg[i].x + st + width;
			rect[n].y = reg[i].y + st;
			rect[n].width = GetColWidth(g, col);
			rect[n].height = reg[i].height - st * 2;
			ClipRectToReg(g, &rect[n], &reg[i]);
			n++;
			}
		break;
		}
	case DrawCell:
		{
		if (!RowColToXY(g, row, col, True, &rect[n]))
			n++;
		break;
		}
	}
	for (i = 0; i < n; i++)
		{
		if (!rect[i].width || !rect[i].height)
			continue;
		event.type = Expose;
		event.window = win;
		event.display = dpy;
		event.x = rect[i].x;
		event.y = rect[i].y;
		event.width = rect[i].width;
		event.height = rect[i].height;
		event.send_event = True;
		event.count = 0;
		if (g->grid.immediateDraw)
			{
			region = XCreateRegion();
			XUnionRectWithRegion(&rect[i], region, region);
			Redisplay((Widget)g, &event, region);
			XDestroyRegion(region);
			}
		else
			XSendEvent(dpy, win, False, ExposureMask, (XEvent *)&event);
		if (g->grid.debugLevel > 1)
			fprintf(stderr, "XmLGrid: DrawArea expose x %d y %d w %d h %d\n",
				event.x, event.y, event.width, event.height);
		}
	}

static void
ExtendSelectRange(XmLGridWidget g,
		  int *type,
		  int *fr,
		  int *lr,
		  int *fc,
		  int *lc)
	{
	int r, c;

	if ((g->grid.selectionPolicy == XmSELECT_MULTIPLE_ROW) ||
		(ColPosToType(g, g->grid.extendCol) != XmCONTENT))
		*type = SelectRow;
	else if (RowPosToType(g, g->grid.extendRow) != XmCONTENT)
		*type = SelectCol;
	else
		*type = SelectCell;

	r = g->grid.extendToRow;
	if (r < g->grid.headingRowCount)
		r = g->grid.headingRowCount;
	if (r >= g->grid.headingRowCount + g->grid.rowCount)
		r = g->grid.headingRowCount + g->grid.rowCount - 1;
	if (*type == SelectCol)
		{
		*fr = 0;
		*lr = 1;
		}
	else if (g->grid.extendRow < r)
		{
		*fr = g->grid.extendRow;
		*lr = r;
		}
	else
		{
		*fr = r;
		*lr = g->grid.extendRow;
		}
	c = g->grid.extendToCol;
	if (c < g->grid.headingColCount)
		c = g->grid.headingColCount;
	if (c >= g->grid.headingColCount + g->grid.colCount)
		c = g->grid.headingColCount + g->grid.colCount - 1;
	if (*type == SelectRow)
		{
		*fc = 0;
		*lc = 1;
		}
	else if (g->grid.extendCol < c)
		{
		*fc = g->grid.extendCol;
		*lc = c;
		}
	else
		{
		*fc = c;
		*lc = g->grid.extendCol;
		}
	}

static void 
ExtendSelect(XmLGridWidget g,
	     XEvent *event,
	     Boolean set,
	     int row,
	     int col)
	{
	int type;
	int r, fr, lr;
	int c, fc, lc;

	if (row == -1 || col == -1)
		{
		g->grid.extendRow = -1;
		g->grid.extendCol = -1;
		g->grid.extendToRow = -1;
		g->grid.extendToCol = -1;
		g->grid.extendSelect = True;
		return;
		}
	if (RowPosToType(g, row) == XmFOOTER || ColPosToType(g, col) == XmFOOTER)
		return;
	if ((g->grid.extendToRow == row && g->grid.extendToCol == col) ||
		(g->grid.extendRow == -1 && row == g->grid.focusRow &&
		g->grid.extendCol == -1 && col == g->grid.focusCol))
		return;
	if (g->grid.extendRow != -1 && g->grid.extendCol != -1)
		{
		/* clear previous extend */
		ExtendSelectRange(g, &type, &fr, &lr, &fc, &lc);
		for (r = fr; r <= lr; r += 1)
			for (c = fc; c <= lc; c += 1)
				SelectTypeArea(g, type, event,
					RowPosToTypePos(g, XmCONTENT, r),
					ColPosToTypePos(g, XmCONTENT, c), False, True);
		}
	else
		{
		g->grid.extendRow = g->grid.focusRow;
		g->grid.extendCol = g->grid.focusCol;
		}
	if (set == True)
		{
		g->grid.extendRow = row;
		g->grid.extendCol = col;
		}
	if (g->grid.extendRow < 0 || g->grid.extendCol < 0)
		return;
	g->grid.extendToRow = row;
	g->grid.extendToCol = col;

	/* set new extend */
	ExtendSelectRange(g, &type, &fr, &lr, &fc, &lc);
	for (r = fr; r <= lr; r += 1)
		for (c = fc; c <= lc; c += 1)
			SelectTypeArea(g, type, event,
				RowPosToTypePos(g, XmCONTENT, r),
				ColPosToTypePos(g, XmCONTENT, c),
				g->grid.extendSelect, True);
	}

static void 
SelectTypeArea(XmLGridWidget g,
	       int type,
	       XEvent *event,
	       int row,
	       int col,
	       Boolean select,
	       Boolean notify)
	{
	Widget w;
	XmLGridRow rowp;
	XmLGridColumn colp;
	XmLGridCell cellp;
	int r, fr, lr, hrc;
	int c, fc, lc, hcc;
	int badPos, hasCB;
	XmLGridCallbackStruct cbs;

	w = (Widget)g;
	hrc = g->grid.headingRowCount;
	hcc = g->grid.headingColCount;
	cbs.event = event;
	cbs.rowType = XmCONTENT;
	cbs.columnType = XmCONTENT;
	hasCB = 0;
	if (select == True)
		{
		if (type == SelectRow)
			cbs.reason = XmCR_SELECT_ROW;
		else if (type == SelectCol)
			cbs.reason = XmCR_SELECT_COLUMN;
		else if (type == SelectCell)
			cbs.reason = XmCR_SELECT_CELL;
		if (XtHasCallbacks(w, XmNselectCallback) == XtCallbackHasSome)
			hasCB = 1;
		}
	else
		{
		if (type == SelectRow)
			cbs.reason = XmCR_DESELECT_ROW;
		else if (type == SelectCol)
			cbs.reason = XmCR_DESELECT_COLUMN;
		else if (type == SelectCell)
			cbs.reason = XmCR_DESELECT_CELL;
		if (XtHasCallbacks(w, XmNdeselectCallback) == XtCallbackHasSome)
			hasCB = 1;
		}
	if (row != -1)
		{
		fr = hrc + row;
		lr = fr + 1;
		}
	else
		{
		fr = hrc;
		lr = XmLArrayGetCount(g->grid.rowArray) - g->grid.footerRowCount;
		}
	if (col != -1)
		{
		fc = hcc + col;
		lc = fc + 1;
		}
	else
		{
		fc = hcc;
		lc = XmLArrayGetCount(g->grid.colArray) - g->grid.footerColCount;
		}
	badPos = 0;
	for (r = fr; r < lr; r++)
		for (c = fc; c < lc; c++)
			{
			if (type == SelectRow)
				{
				rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
				if (!rowp)
					{
					badPos = 1;
					continue;
					}
				if (XmLGridRowIsSelected(rowp) == select)
					continue;
				if (select == True &&
					(g->grid.selectionPolicy == XmSELECT_BROWSE_ROW ||
					g->grid.selectionPolicy == XmSELECT_SINGLE_ROW))
					SelectTypeArea(g, SelectRow, event, -1, 0, False, notify);
				XmLGridRowSetSelected(rowp, select);
				if (RowIsVisible(g, r))
					DrawArea(g, DrawRow, r, 0);
				if (notify && hasCB)
					{
					cbs.row = r - hrc;
					if (select == True)
						XtCallCallbackList(w, g->grid.selectCallback,
							(XtPointer)&cbs);
					else
						XtCallCallbackList(w, g->grid.deselectCallback,
							(XtPointer)&cbs);
					}
				}
			else if (type == SelectCol)
				{
				colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
				if (!colp)
					{
					badPos = 1;
					continue;
					}
				if (XmLGridColumnIsSelected(colp) == select)
					continue;
				XmLGridColumnSetSelected(colp, select);
				if (ColIsVisible(g, c))
					DrawArea(g, DrawCol, 0, c);
				if (notify && hasCB)
					{
					cbs.column = c - hcc;
					if (select == True)
						XtCallCallbackList(w, g->grid.selectCallback,
							(XtPointer)&cbs);
					else
						XtCallCallbackList(w, g->grid.deselectCallback,
							(XtPointer)&cbs);
					}
				}
			else if (type == SelectCell)
				{
				cellp = GetCell(g, r, c);
				if (!cellp)
					{
					badPos = 1;
					continue;
					}
				if (XmLGridCellIsSelected(cellp) == select)
					continue;
				XmLGridCellSetSelected(cellp, select);
				DrawArea(g, DrawCell, r, c);
				if (notify && hasCB)
					{
					cbs.column = c - hcc;
					cbs.row = r - hrc;
					if (select == True)
						XtCallCallbackList(w, g->grid.selectCallback,
							(XtPointer)&cbs);
					else
						XtCallCallbackList(w, g->grid.deselectCallback,
							(XtPointer)&cbs);
					}
				}
			}
	if (badPos)
		XmLWarning((Widget)g, "SelectTypeArea() - bad position");
	}

static int 
GetSelectedArea(XmLGridWidget g,
		int type,
		int *rowPos,
		int *colPos,
		int count)
	{
	XmLGridRow row;
	XmLGridColumn col;
	XmLGridCell cell;
	int r, fr, lr;
	int c, fc, lc;
	int n;

	if (type == SelectCol)
		{
		fr = 0;
		lr = 1;
		}
	else
		{
		fr = g->grid.headingRowCount;
		lr = XmLArrayGetCount(g->grid.rowArray) - g->grid.footerRowCount;
		}
	if (type == SelectRow)
		{
		fc = 0;
		lc = 1;
		}
	else
		{
		fc = g->grid.headingColCount;
		lc = XmLArrayGetCount(g->grid.colArray) - g->grid.footerColCount;
		}
	n = 0;
	for (r = fr; r < lr; r++)
		for (c = fc; c < lc; c++)
			{
			if (type == SelectRow)
				{
				row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
				if (row && XmLGridRowIsSelected(row) == True)
					{
					if (rowPos && n < count)
						rowPos[n] = r - fr;
					n++;
					}
				}
			else if (type == SelectCol)
				{
				col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
				if (col && XmLGridColumnIsSelected(col) == True)
					{
					if (colPos && n < count)
						colPos[n] = c - fc;
					n++;
					}
				}
			else if (type == SelectCell)
				{
				cell = GetCell(g, r, c);
				if (cell && XmLGridCellIsSelected(cell) == True)
					{
					if (rowPos && colPos && n < count)
						{
						rowPos[n] = r - fr;
						colPos[n] = c - fc;
						}
					n++;
					}
				}
			}
	return n;
	}

static void
ChangeManaged(Widget w)
	{
	_XmNavigChangeManaged(w);
	}

static void
Resize(Widget w)
	{
	XmLGridWidget g;
	XmLGridCallbackStruct cbs;

	g = (XmLGridWidget)w;

	if (!g->grid.inResize)
	  {
	    cbs.reason = XmCR_RESIZE_GRID;

	    g->grid.inResize = True;
	    XtCallCallbackList((Widget)g, g->grid.resizeCallback,
			       (XtPointer)&cbs);
	    g->grid.inResize = False;
	  }

	VertLayout(g, 0);
	HorizLayout(g, 0);
	PlaceScrollbars(g);
	DrawArea(g, DrawAll, 0, 0);
	}

static void
PlaceScrollbars(XmLGridWidget g)
	{
	int x, y;
	int width, height;
	Widget vsb, hsb;
	Dimension headingRowHeight = 0;

	vsb = g->grid.vsb;
	hsb = g->grid.hsb;
	width = g->core.width;
	if (XtIsManaged(vsb))
		width -= vsb->core.width;
	if (width <= 0)
		width = 1;
	y = g->core.height - hsb->core.height;
	XtConfigureWidget(hsb, 0, y, width, hsb->core.height, 0);

	height = g->core.height;
	if (XtIsManaged(hsb))
		height -= hsb->core.height;

	y = 0;
	
	if (g->grid.headingRowCount > 0)
	  {
	    XmLGridRow rowp;

	    rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, 0);
	    
	    headingRowHeight = XmLGridRowHeightInPixels(rowp) + g->manager.shadow_thickness;
	  }

        if (g->grid.showHideButton && g->grid.hideButton)
	    {
	      if (headingRowHeight == 0)
		headingRowHeight = 20;

	      /* if there is at least heading row, we make the
		   height of the button the height of the first
		   heading row.

		   This is pretty braindead... */

		XtConfigureWidget(g->grid.hideButton, 
				  g->core.width - vsb->core.width,
				  y + g->manager.shadow_thickness,
				  vsb->core.width,
				  headingRowHeight, 0);

		/* once we've positioned it, make sure it's managed.
		   Doing it in this order (position, then manage) reduces
		   screen flickering -- the button doesn't flash on in the
		   upper left corner for an instant. */
		if (!XtIsManaged(g->grid.hideButton)) 
		    XtManageChild(g->grid.hideButton);
	    }

	if (height <= 0)
		width = 1;
	x = g->core.width - vsb->core.width;
	XtConfigureWidget(vsb, 
			  x, y + headingRowHeight + g->manager.shadow_thickness * 2, 
			  vsb->core.width, height - headingRowHeight - g->manager.shadow_thickness * 2, 
			  0);
	}

void 
_XmLGridLayout(XmLGridWidget g)
	{
	VertLayout(g, 1);
	HorizLayout(g, 1);
	}

static void
VertLayout(XmLGridWidget g,
	   int resizeIfNeeded)
	{
	GridReg *reg;
	int i, j, st2, height, rowCount;
	int topNRow, topHeight;
	int midRow, midY, midNRow, midHeight;
	int botRow, botY, botNRow, botHeight;
	int scrollChanged, needsSB, needsResize, cRow;
	int maxRow, maxPos, maxHeight, newHeight, sliderSize;
	int horizSizeChanged;
	XtWidgetGeometry req;
	XmLGridCallbackStruct cbs;
	XmLGridPreLayoutProc preLayoutProc;

	if (g->grid.layoutFrozen == True)
		{
		g->grid.needsVertLayout = 1;
		return;
		}

	maxRow = maxPos = maxHeight = newHeight = horizSizeChanged = 0;
	preLayoutProc = XmLGridClassPartOfWidget(g).preLayoutProc;
	if (g->grid.layoutStack < 2 && preLayoutProc != XmInheritGridPreLayout)
		horizSizeChanged = preLayoutProc(g, 1);

	scrollChanged = 0;
	needsResize = 0;
	needsSB = 0;
	rowCount = XmLArrayGetCount(g->grid.rowArray);
	reg = g->grid.reg;
	st2 = g->manager.shadow_thickness * 2;
	TextAction(g, TEXT_HIDE);

	topHeight = 0;
	topNRow = g->grid.topFixedCount;
	if (topNRow > g->grid.rowCount + g->grid.headingRowCount)
		topNRow = g->grid.rowCount + g->grid.headingRowCount;
	if (topNRow)
		{
		topHeight += st2;
		for (i = 0; i < topNRow; i++)
			topHeight += GetRowHeight(g, i);
		}
	botHeight = 0;
	botNRow = g->grid.bottomFixedCount;
	if (topNRow + botNRow > rowCount)
		botNRow = rowCount - topNRow;
	botRow = rowCount - botNRow;
	if (botNRow)
		{
		botHeight += st2;
		for (i = botRow; i < rowCount; i++)
			botHeight += GetRowHeight(g, i);
		}
	height = 0;
	if (topNRow)
		height += topHeight + g->grid.topFixedMargin;
	midY = height;
	if (botNRow)
		height += botHeight + g->grid.bottomFixedMargin;
	if (XtIsManaged(g->grid.hsb))
		{
		height += g->grid.hsb->core.height; 
		height += g->grid.scrollBarMargin;
		}
	maxHeight = g->core.height - height;
	if (g->grid.vsPolicy != XmCONSTANT)
		{
		if (rowCount == topNRow + botNRow)
			midHeight = 0;
		else
			midHeight = st2;
		for (i = topNRow; i < rowCount - botNRow; i++)
			midHeight += GetRowHeight(g, i);
		midRow = topNRow;
		midNRow = rowCount - topNRow - botNRow;
		needsResize = 1;
		newHeight = midHeight + height;
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: VertLayout VARIABLE height\n");
		}
	else
		{
		if (maxHeight < st2)
			maxHeight = 0;
		height = st2;
		j = rowCount - botNRow - 1;
		for (i = j; i >= topNRow; i--)
			{
			height += GetRowHeight(g, i);
			if (height > maxHeight)
				break;
			}
		i++;
		if (i > j)
			i = j;
		maxRow = i;
		if (maxRow < topNRow)
			maxRow = topNRow;
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: VertLayout max scroll row %d\n", i);
		if (maxRow == topNRow)
			{
			if (g->grid.scrollRow != topNRow)
				{
				scrollChanged = 1;
				g->grid.scrollRow = topNRow;
				}
			midRow = topNRow;
			midHeight = maxHeight;
			midNRow = rowCount - topNRow - botNRow;
			if (g->grid.debugLevel)
				fprintf(stderr, "XmLGrid: VertLayout everything fits\n");
			}
		else
			{
			if (g->grid.scrollRow < topNRow)
				{
				scrollChanged = 1;
				g->grid.scrollRow = topNRow;
				if (g->grid.debugLevel)
					fprintf(stderr, "XmLGrid: VertLayout scrolled < topRow\n");
				}
			if (g->grid.scrollRow > maxRow)
				{
				if (g->grid.debugLevel)
					fprintf(stderr, "XmLGrid: VertLayout scrolled > maxRow\n");
				scrollChanged = 1;
				g->grid.scrollRow = maxRow;
				}
			height = st2;
			midNRow = 0;
			for (i = g->grid.scrollRow; i < rowCount - botNRow; i++)
				{
				midNRow++;
				height += GetRowHeight(g, i);
				if (height >= maxHeight)
					break;
				}
			needsSB = 1;
			midRow = g->grid.scrollRow;
			midHeight = maxHeight;
			}
		}
	botY = midY + midHeight;
	if (botNRow)
		botY += g->grid.bottomFixedMargin;
	for (i = 0; i < 3; i++)
		{
		reg[i].y = 0;
		reg[i].height = topHeight;
		reg[i].row = 0;
		reg[i].nrow = topNRow;
		}
	for (i = 3; i < 6; i++)
		{
		reg[i].y = midY;
		reg[i].height = midHeight;
		reg[i].row = midRow;
		reg[i].nrow = midNRow;
		}
	for (i = 6; i < 9; i++)
		{
		reg[i].y = botY;
		reg[i].height = botHeight;
		reg[i].row = botRow;
		reg[i].nrow = botNRow;
		}
	if (g->grid.debugLevel)
			{
			fprintf(stderr, "XmLGrid: VertLayout T y %d h %d r %d nr %d\n",
				reg[0].y, reg[0].height, reg[0].row, reg[0].nrow);
			fprintf(stderr, "XmLGrid: VertLayout M y %d h %d r %d nr %d\n",
				reg[3].y, reg[3].height, reg[3].row, reg[3].nrow);
			fprintf(stderr, "XmLGrid: VertLayout B y %d h %d r %d nr %d\n",
				reg[6].y, reg[6].height, reg[6].row, reg[6].nrow);
			}
	if (needsSB)
		{
		if (!XtIsManaged(g->grid.vsb))
			{
			XtManageChild(g->grid.vsb);
			horizSizeChanged = 1;
			}
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: VertLayout set sb values\n");
		maxPos = PosToVisPos(g, maxRow, 1);
		sliderSize = PosToVisPos(g, rowCount - botNRow - 1, 1) - maxPos + 1;
		XtVaSetValues(g->grid.vsb,
			XmNminimum, PosToVisPos(g, topNRow, 1),
			XmNmaximum, maxPos + sliderSize,
			XmNsliderSize, sliderSize,
			XmNpageIncrement, sliderSize,
			XmNvalue, PosToVisPos(g, g->grid.scrollRow, 1),
			NULL);
		}
	else if (g->grid.vsPolicy == XmCONSTANT &&
		g->grid.vsbDisplayPolicy == XmSTATIC)
		{
		if (!XtIsManaged(g->grid.vsb))
			{
			XtManageChild(g->grid.vsb);
			horizSizeChanged = 1;
			}
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: VertLayout vsb not needed but static\n");
		XtVaSetValues(g->grid.vsb,
			XmNminimum, 0,
			XmNmaximum, 1,
			XmNsliderSize, 1,
			XmNpageIncrement, 1,
			XmNvalue, 0,
			NULL);
		}
	else
		{
		if (XtIsManaged(g->grid.vsb))
			{
			XtUnmanageChild(g->grid.vsb);
			horizSizeChanged = 1;
			}
		}
	if (needsResize && resizeIfNeeded)
		{
		if (newHeight < 1)
			newHeight = 1;
		req.request_mode = CWHeight;
		req.height = newHeight;
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: VertLayout Req h %d\n", (int)newHeight);
		XtMakeGeometryRequest((Widget)g, &req, NULL);
		PlaceScrollbars(g);
		}
	if (scrollChanged)
		DrawArea(g, DrawVScroll, 0, 0);
	TextAction(g, TEXT_SHOW);
	cRow = g->grid.scrollRow - g->grid.headingRowCount;
	if (cRow != g->grid.cScrollRow)
		{
		g->grid.cScrollRow = cRow;
		cbs.reason = XmCR_SCROLL_ROW;
		cbs.rowType = XmCONTENT;
		cbs.row = cRow;
		XtCallCallbackList((Widget)g, g->grid.scrollCallback, (XtPointer)&cbs);
		}
	if (horizSizeChanged)
		{
		if (g->grid.layoutStack > 2)
			XmLWarning((Widget)g, "VertLayout() - recursion error");
		else
			{
			g->grid.layoutStack++;
			HorizLayout(g, resizeIfNeeded);
			g->grid.layoutStack--;
			}
		PlaceScrollbars(g);
		}
	}

static void
HorizLayout(XmLGridWidget g,
	    int resizeIfNeeded)
	{
	GridReg *reg;
	int i, j, st2, width, colCount;
	int leftNCol, leftWidth;
	int midCol, midX, midNCol, midWidth;
	int rightCol, rightX, rightNCol, rightWidth;
	int scrollChanged, needsSB, needsResize, cCol;
	int maxCol, maxPos, maxWidth, newWidth, sliderSize;
	int vertSizeChanged;
	XtWidgetGeometry req;
	XmLGridCallbackStruct cbs;
	XmLGridPreLayoutProc preLayoutProc;

	if (g->grid.layoutFrozen == True)
		{
		g->grid.needsHorizLayout = 1;
		return;
		}

	maxCol = maxPos = newWidth = vertSizeChanged = 0;
	preLayoutProc = XmLGridClassPartOfWidget(g).preLayoutProc;
	if (g->grid.layoutStack < 2 && preLayoutProc != XmInheritGridPreLayout)
		vertSizeChanged = preLayoutProc(g, 0);

	scrollChanged = 0;
	needsResize = 0;
	needsSB = 0;
#if 0
	/* we can't use this one if we're going to allow for hidden columns,
	   as we're changing what g->grid.colCount is. 

	   the calculation we're replacing it with works the same, and results
	   in less code being munged.
	   */
	colCount = XmLArrayGetCount(g->grid.colArray);
#else
	colCount = g->grid.colCount + g->grid.headingColCount + g->grid.footerColCount;
#endif
	reg = g->grid.reg;
	st2 = g->manager.shadow_thickness * 2;
	TextAction(g, TEXT_HIDE);

	leftWidth = 0;
	leftNCol = g->grid.leftFixedCount;
	if (leftNCol > g->grid.colCount + g->grid.headingColCount)
		leftNCol = g->grid.colCount + g->grid.headingColCount;
	if (leftNCol)
		{
		leftWidth += st2;
		for (i = 0; i < leftNCol; i++)
			leftWidth += GetColWidth(g, i);
		}
	rightWidth = 0;
	rightNCol = g->grid.rightFixedCount;
	if (rightNCol + leftNCol > colCount)
		rightNCol = colCount - leftNCol;
	rightCol = colCount - rightNCol;
	if (rightNCol)
		{
		rightWidth += st2;
		for (i = rightCol; i < colCount; i++)
			rightWidth += GetColWidth(g, i);
		}
	width = 0;
	if (leftNCol)
		width += leftWidth + g->grid.leftFixedMargin;
	midX = width;
	if (rightNCol)
		width += rightWidth + g->grid.rightFixedMargin;
	if (XtIsManaged(g->grid.vsb))
		{
		width += g->grid.vsb->core.width; 
		width += g->grid.scrollBarMargin;
		}
	maxWidth = g->core.width - width;
	if (g->grid.hsPolicy != XmCONSTANT)
		{
		if (colCount == leftNCol + rightNCol)
			midWidth = 0;
		else
			midWidth = st2;
		for (i = leftNCol; i < colCount - rightNCol; i++)
			midWidth += GetColWidth(g, i);
		midCol = leftNCol;
		midNCol = colCount - leftNCol - rightNCol;
		needsResize = 1;
		newWidth = midWidth + width;
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: HorizLayout VARIABLE width\n");
		}
	else
		{
		if (maxWidth < st2)
			maxWidth = 0;
		width = st2;
		j = colCount - rightNCol - 1;
		for (i = j; i >= leftNCol; i--)
			{
			width += GetColWidth(g, i);
			if (width > maxWidth)
				break;
			}
		i++;
		if (i > j)
			i = j;
		maxCol = i;
		if (maxCol < leftNCol)
			maxCol = leftNCol;
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: HorizLayout max scroll col %d\n", i);
		if (maxCol == leftNCol)
			{
			if (g->grid.scrollCol != leftNCol)
				{
				scrollChanged = 1;
				g->grid.scrollCol = leftNCol;
				}
			midCol = leftNCol;
			midWidth = maxWidth;
			midNCol = colCount - leftNCol - rightNCol;
			if (g->grid.debugLevel)
				fprintf(stderr, "XmLGrid: HorizLayout everything fits\n");
			}
		else
			{
			if (g->grid.scrollCol < leftNCol)
				{
				scrollChanged = 1;
				g->grid.scrollCol = leftNCol;
				if (g->grid.debugLevel)
					fprintf(stderr, "XmLGrid: HorizLayout scroll < leftCol\n");
				}
			if (g->grid.scrollCol > maxCol)
				{
				if (g->grid.debugLevel)
					fprintf(stderr, "XmLGrid: HorizLayout scroll > maxCol\n");
				scrollChanged = 1;
				g->grid.scrollCol = maxCol;
				}
			width = st2;
			midNCol = 0;
			for (i = g->grid.scrollCol; i < colCount - rightNCol; i++)
				{
				midNCol++;
				width += GetColWidth(g, i);
				if (width >= maxWidth)
					break;
				}
			needsSB = 1;
			midCol = g->grid.scrollCol;
			midWidth = maxWidth;
			}
		}
	rightX = midX + midWidth;
	if (rightNCol)
		rightX += g->grid.rightFixedMargin;
	for (i = 0; i < 9; i += 3)
		{
		reg[i].x = 0;
		reg[i].width = leftWidth;
		reg[i].col = 0;
		reg[i].ncol = leftNCol;
		}
	for (i = 1; i < 9; i += 3)
		{
		reg[i].x = midX;
		reg[i].width = midWidth;
		reg[i].col = midCol;
		reg[i].ncol = midNCol;
		}
	for (i = 2; i < 9; i += 3)
		{
		reg[i].x = rightX;
		reg[i].width = rightWidth;
		reg[i].col = rightCol;
		reg[i].ncol = rightNCol;
		}
	if (g->grid.debugLevel)
		{
		fprintf(stderr, "XmLGrid: HorizLayout TOP x %d w %d c %d nc %d\n",
			reg[0].x, reg[0].width, reg[0].col, reg[0].ncol);
		fprintf(stderr, "XmLGrid: HorizLayout MID x %d w %d c %d nc %d\n",
			reg[1].x, reg[1].width, reg[1].col, reg[1].ncol);
		fprintf(stderr, "XmLGrid: HorizLayout BOT x %d w %d c %d nc %d\n",
			reg[2].x, reg[2].width, reg[2].col, reg[2].ncol);
		}
	if (g->grid.hsPolicy == XmCONSTANT && colCount == 1 &&
		g->grid.colCount == 1 && width > maxWidth)
		{
		/* Single Column Pixel Scroll Mode */
		if (g->grid.singleColScrollMode)
			{
			i = g->grid.singleColScrollPos;
			if (i < 0)
				i = 0;
			else if (i > width - maxWidth)
				i = width - maxWidth;
			}
		else
			i = 0;
		XtVaSetValues(g->grid.hsb,
			XmNminimum, 0,
			XmNmaximum, width - maxWidth + 1,
			XmNsliderSize, 1,
			XmNpageIncrement, ((width - maxWidth) / 4) + 1,
			XmNvalue, i,
			NULL);
		if (!XtIsManaged(g->grid.hsb))
			{
			XtManageChild(g->grid.hsb);
			vertSizeChanged = 1;
			}
		g->grid.singleColScrollMode = 1;
		g->grid.singleColScrollPos = i;
		}
	else
		g->grid.singleColScrollMode = 0;
	if (g->grid.singleColScrollMode)
		;
	else if (needsSB)
		{
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: HorizLayout set sb values\n");
		if (!XtIsManaged(g->grid.hsb))
			{
			XtManageChild(g->grid.hsb);
			vertSizeChanged = 1;
			}
		maxPos = PosToVisPos(g, maxCol, 0);
		sliderSize = PosToVisPos(g, colCount - rightNCol - 1, 0) - maxPos + 1;
		XtVaSetValues(g->grid.hsb,
			XmNminimum, PosToVisPos(g, leftNCol, 0),
			XmNmaximum, maxPos + sliderSize,
			XmNsliderSize, sliderSize,
			XmNpageIncrement, sliderSize,
			XmNvalue, PosToVisPos(g, g->grid.scrollCol, 0),
			NULL);
		}
	else if (g->grid.hsPolicy == XmCONSTANT &&
		g->grid.hsbDisplayPolicy == XmSTATIC)
		{
		if (!XtIsManaged(g->grid.hsb))
			{
			XtManageChild(g->grid.hsb);
			vertSizeChanged = 1;
			}
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: HorizLayout hsb not needed - static\n");
		XtVaSetValues(g->grid.hsb,
			XmNminimum, 0,
			XmNmaximum, 1,
			XmNsliderSize, 1,
			XmNpageIncrement, 1,
			XmNvalue, 0,
			NULL);
		}
	else
		{
		if (XtIsManaged(g->grid.hsb))
			{
			XtUnmanageChild(g->grid.hsb);
			vertSizeChanged = 1;
			}
		}
	if (needsResize && resizeIfNeeded)
		{
		if (newWidth < 1)
			newWidth = 1;
		req.request_mode = CWWidth;
		req.width = newWidth;
		if (g->grid.debugLevel)
			fprintf(stderr, "XmLGrid: HorizLayout Req w %d\n", (int)newWidth);
		XtMakeGeometryRequest((Widget)g, &req, NULL);
		PlaceScrollbars(g);
		}
	if (scrollChanged)
		DrawArea(g, DrawHScroll, 0, 0);
	TextAction(g, TEXT_SHOW);
	cCol = g->grid.scrollCol - g->grid.headingColCount;
	if (cCol != g->grid.cScrollCol)
		{
		g->grid.cScrollCol = cCol;
		cbs.reason = XmCR_SCROLL_COLUMN;
		cbs.columnType = XmCONTENT;
		cbs.column = cCol;
		XtCallCallbackList((Widget)g, g->grid.scrollCallback, (XtPointer)&cbs);
		}
	if (vertSizeChanged)
		{
		if (g->grid.layoutStack > 2)
			XmLWarning((Widget)g, "HorizLayout() - recursion error");
		else
			{
			g->grid.layoutStack++;
			VertLayout(g, resizeIfNeeded);
			g->grid.layoutStack--;
			}
		PlaceScrollbars(g);
		}
	}

static void
ApplyVisibleRows(XmLGridWidget g)
	{
	XtWidgetGeometry req;
	XmLGridCellRefValues *cellValues;

	if (g->grid.vsPolicy != XmCONSTANT)
		{
		XmLWarning((Widget)g,
			"verticalSizePolicy must be XmCONSTANT to set visibleRows");
		return;
		}
	cellValues = g->grid.defCellValues;
	req.request_mode = CWHeight;
	req.height = g->manager.shadow_thickness * 2 + g->grid.visibleRows *
		(4 + cellValues->fontHeight + cellValues->topMargin +
		cellValues->bottomMargin);
	if (g->grid.hsPolicy == XmCONSTANT &&
		g->grid.hsbDisplayPolicy == XmSTATIC)
		req.height += g->grid.scrollBarMargin + XtHeight(g->grid.hsb);
	XtMakeGeometryRequest((Widget)g, &req, NULL);
	}

static void
ApplyVisibleCols(XmLGridWidget g)
	{
	XtWidgetGeometry req;
	XmLGridCellRefValues *cellValues;

	if (g->grid.vsPolicy != XmCONSTANT)
		{
		XmLWarning((Widget)g,
			"horizontalSizePolicy must be XmCONSTANT to set visibleColumns");
		return;
		}
	cellValues = g->grid.defCellValues;
	req.request_mode = CWWidth;
	req.width = g->manager.shadow_thickness * 2 + g->grid.visibleCols *
		(4 + 8 * cellValues->fontWidth + cellValues->leftMargin +
		cellValues->rightMargin);
	if (g->grid.vsPolicy == XmCONSTANT &&
		g->grid.vsbDisplayPolicy == XmSTATIC)
		req.width += g->grid.scrollBarMargin + XtWidth(g->grid.vsb);
	XtMakeGeometryRequest((Widget)g, &req, NULL);
	}

static void
VisPosChanged(XmLGridWidget g,
	      int isVert)
	{
	if (isVert)
		{
		g->grid.recalcVertVisPos = 1;
		g->grid.vertVisChangedHint = 1;
		}
	else
		g->grid.recalcHorizVisPos = 1;
	}

static void 
RecalcVisPos(XmLGridWidget g,
	     int isVert)
	{
	XmLGridRow row;
	XmLGridColumn col;
	int i, count, visPos;

	if (g->grid.layoutFrozen == True)
			XmLWarning((Widget)g, "RecalcVisPos() - layout is frozen");
	visPos = 0;
	if (isVert)
		{
		if (!g->grid.recalcVertVisPos)
			return;
		count = XmLArrayGetCount(g->grid.rowArray);
		for (i = 0; i < count; i++)
			{
			row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, i);
			XmLGridRowSetVisPos(row, visPos);
			if (!XmLGridRowIsHidden(row))
				visPos++;
			}
		g->grid.recalcVertVisPos = 0;
		}
	else
		{
		if (!g->grid.recalcHorizVisPos)
			return;
		count = XmLArrayGetCount(g->grid.colArray);
		for (i = 0; i < count; i++)
			{
			col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, i);
			XmLGridColumnSetVisPos(col, visPos);
			if (!XmLGridColumnIsHidden(col))
				visPos++;
			}
		g->grid.recalcHorizVisPos = 0;
		}
	}

static int 
PosToVisPos(XmLGridWidget g,
	    int pos,
	    int isVert)
	{
	int visPos;
	XmLGridRow row;
	XmLGridColumn col;

	if (isVert)
		{
		if (!g->grid.hiddenRowCount)
			visPos = pos;
		else
			{
			RecalcVisPos(g, isVert);
			row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, pos);
			if (row)
				visPos = XmLGridRowGetVisPos(row);
			else
				visPos = -1;
			}
		}
	else
		{
		if (!g->grid.hiddenColCount)
			visPos = pos;
		else
			{
			RecalcVisPos(g, isVert);
			col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, pos);
			if (col)
				visPos = XmLGridColumnGetVisPos(col);
			else
				visPos = -1;
			}
		}
	if (visPos == -1)
		XmLWarning((Widget)g, "PosToVisPos() - invalid pos");
	return visPos;
	}

static int
VisPosToPos(XmLGridWidget g,
	    int visPos,
	    int isVert)
	{
	XmLGridRow row;
	XmLGridColumn col;
	int i1, i2, vp1, vp2, ic, mid, val, count;

	if (isVert)
		{
		if (!g->grid.hiddenRowCount)
			return visPos;
		count = XmLArrayGetCount(g->grid.rowArray);
		if (!count)
			{
			XmLWarning((Widget)g, "VisPosToPos() - called when no rows exist");
			return -1;
			}
		RecalcVisPos(g, isVert);
		row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, 0);
		vp1 = XmLGridRowGetVisPos(row);
		row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, count - 1);
		vp2 = XmLGridRowGetVisPos(row);
		if (visPos < vp1 || visPos > vp2)
			{
			XmLWarning((Widget)g, "VisPosToPos() - invalid Vert visPos");
			return -1;
			}
		}
	else
		{
		if (!g->grid.hiddenColCount)
			return visPos;
		count = XmLArrayGetCount(g->grid.colArray);
		if (!count)
			{
			XmLWarning((Widget)g, "VisPosToPos() - called when no cols exist");
			return -1;
			}
		RecalcVisPos(g, isVert);
		col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, 0);
		vp1 = XmLGridColumnGetVisPos(col);
		col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, count - 1);
		vp2 = XmLGridColumnGetVisPos(col);
		if (visPos < vp1 || visPos > vp2)
			{
			XmLWarning((Widget)g, "VisPosToPos() - invalid Horiz visPos");
			return -1;
			}
		}
	i1 = 0;
	i2 = count;
	ic = 0;
	while (1)
		{
		mid = i1 + (i2 - i1) / 2;
		if (isVert)
			{
			row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, mid);
			val = XmLGridRowGetVisPos(row);
			}
		else
			{
			col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, mid);
			val = XmLGridColumnGetVisPos(col);
			}
		if (visPos > val)
			i1 = mid;
		else if (visPos < val)
			i2 = mid;
		else
			break;
		if (++ic > 1000)
				return -1; /* inf loop */
		}
	return mid;
	}

static unsigned char
ColPosToType(XmLGridWidget g,
	     int pos)
	{
	unsigned char type;

	if (pos < g->grid.headingColCount)
		type = XmHEADING;
	else if (pos < g->grid.headingColCount + g->grid.colCount)
		type = XmCONTENT;
	else
		type = XmFOOTER;
	return type;
	}

static int 
ColPosToTypePos(XmLGridWidget g,
		unsigned char type,
		int pos)
{
	int c;

	if (type == XmHEADING)
		c = pos;
	else if (type == XmCONTENT)
		c = pos - g->grid.headingColCount;
	else
		c = pos - g->grid.headingColCount - g->grid.colCount;
	return c;
	}

static unsigned char 
RowPosToType(XmLGridWidget g,
	     int pos)
	{
	unsigned char type;

	if (pos < g->grid.headingRowCount)
		type = XmHEADING;
	else if (pos < g->grid.headingRowCount + g->grid.rowCount)
		type = XmCONTENT;
	else
		type = XmFOOTER;
	return type;
	}

static int
RowPosToTypePos(XmLGridWidget g,
		unsigned char type,
		int pos)
	{
	int r;

	if (type == XmHEADING)
		r = pos;
	else if (type == XmCONTENT)
		r = pos - g->grid.headingRowCount;
	else
		r = pos - g->grid.headingRowCount - g->grid.rowCount;
	return r;
	}

static int 
ColTypePosToPos(XmLGridWidget g,
		unsigned char type,
		int pos,
		int allowNeg)
	{
	if (pos < 0)
		{
		if (!allowNeg)
			return -1;
		if (type == XmHEADING)
			pos = g->grid.headingColCount;
		else if (type == XmFOOTER || type == XmALL_TYPES)
			pos = g->grid.headingColCount + g->grid.colCount +
				g->grid.footerColCount;
		else /* XmCONTENT */
			pos = g->grid.headingColCount + g->grid.colCount;
		}
	else
		{
		if (type == XmALL_TYPES)
			;
		else if (type == XmHEADING)
			{
			if (pos >= g->grid.headingColCount)
				return -1;
			}
		else if (type == XmFOOTER)
			{
			if (pos >= g->grid.footerColCount)
				return -1;
			pos += g->grid.headingColCount + g->grid.colCount;
			}
		else /* XmCONTENT */
			{
			if (pos >= g->grid.colCount)
				return -1;
			pos += g->grid.headingColCount;
			}
		}
	return pos;
	}

static int
RowTypePosToPos(XmLGridWidget g,
		unsigned char type,
		int pos,
		int allowNeg)
	{
	if (pos < 0)
		{
		if (!allowNeg)
			return -1;
		if (type == XmHEADING)
			pos = g->grid.headingRowCount;
		else if (type == XmFOOTER || type == XmALL_TYPES)
			pos = g->grid.headingRowCount + g->grid.rowCount +
					g->grid.footerRowCount;
		else /* XmCONTENT */
			pos = g->grid.headingRowCount + g->grid.rowCount;
		}
	else
		{
		if (type == XmALL_TYPES)
			;
		else if (type == XmHEADING)
			{
			if (pos >= g->grid.headingRowCount)
				return -1;
			}
		else if (type == XmFOOTER)
			{
			if (pos >= g->grid.footerRowCount)
				return -1;
			pos += g->grid.headingRowCount + g->grid.rowCount;
			}
		else /* XmCONTENT */
			{
			if (pos >= g->grid.rowCount)
				return -1;
			pos += g->grid.headingRowCount;
			}
		}
	return pos;
	}

static int
ScrollRowBottomPos(XmLGridWidget g,
		   int row)
	{
	int r, h, height;

	if (g->grid.vsPolicy == XmVARIABLE)
		return -1;
	height = g->grid.reg[4].height;
	h = 0;
	r = row;
	while (r >= 0)
		{
		h += GetRowHeight(g, r);
		if (h > height)
			break;
		r--;
		}
	if (r != row)
		r++;
	return r;
	}

static int
ScrollColRightPos(XmLGridWidget g,
		  int col)
	{
	int c, w, width;

	if (g->grid.hsPolicy == XmVARIABLE)
		return -1;
	width = g->grid.reg[4].width;
	w = 0;
	c = col;
	while (c >= 0)
		{
		w += GetColWidth(g, c);
		if (w >= width)
			break;
		c--;
		}
	if (c != col)
		c++;
	return c;
	}

static int
PosIsResize(XmLGridWidget g,
			int x,
			int y,
			int *row,
			int *col,
			int *isVert)
{
	XRectangle rect;
	int i, nx, ny, margin;
	
	/* check for bottom resize */
	if (g->grid.allowRowResize == True)
		for (i = 0; i < 2; i++)
			{
				nx = x;
				ny = y;
				if (i)
					ny = y - 4;
				if (XYToRowCol(g, nx, ny, row, col) == -1)
					continue;
				if (RowColToXY(g, *row, *col, False, &rect) == -1)
					continue;
				if (ColPosToType(g, *col) != XmHEADING)
					continue;
				margin = ny - (rect.y + rect.height);
				if (margin > -5 && margin < 5)
					{
						*isVert = 1;
						return 1;
					}
			}
	/* check for right resize */
	if (g->grid.allowColResize == True)
		for (i = 0; i < 2; i++)
			{
				XmLGridColumn colp;
				int c;

				nx = x;
				ny = y;
				if (i)
					nx = x - 4;
				if (XYToRowCol(g, nx, ny, row, col) == -1)
					continue;
				if (RowColToXY(g, *row, *col, False, &rect) == -1)
					continue;
				if (RowPosToType(g, *row) != XmHEADING)
					continue;
				
				/* if it's the last column, don't allow resize. */
				if (*col == XmLArrayGetCount(g->grid.colArray) - 1)
					continue;
				
				colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, *col);
				
				if (!colp->grid.resizable)
					continue;
				
				for (c = *col + 1 + i; c < XmLArrayGetCount(g->grid.colArray); c ++)
					{
						colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
						
						if (colp->grid.resizable)
							break;

						return 0;
					}

				margin = nx - (rect.x + rect.width);
				if (margin > -5 && margin < 5)
					{
						*isVert = 0;
						return 1;
					}
			}
	return 0;
}

int 
XmLGridPosIsResize(Widget g,
		   int x,
		   int y)
{
  int row, col;
  int isVert;
  return PosIsResize((XmLGridWidget) g, x, y, &row, &col, &isVert);
}

static int
XYToRowCol(XmLGridWidget g,
	   int x,
	   int y,
	   int *row,
	   int *col)
	{
	XRectangle xyRect, rect;
	GridReg *reg;
	int i, r, c;
	int width, height;
	int st;

	reg = g->grid.reg;
	st = g->manager.shadow_thickness;
	xyRect.x = x;
	xyRect.y = y;
	xyRect.width = 1;
	xyRect.height = 1;
	for (i = 0; i < 9; i++)
		{
		if (!reg[i].width || !reg[i].height)
			continue;
		rect.x = reg[i].x + st;
		rect.y = reg[i].y + st;
		rect.width = reg[i].width - st * 2;
		rect.height = reg[i].height - st * 2;
		if (XmLRectIntersect(&xyRect, &rect) == XmLRectOutside)
			continue;
		height = 0;
		for (r = reg[i].row; r < reg[i].row + reg[i].nrow; r++)
			{
			width = 0;
			for (c = reg[i].col; c < reg[i].col + reg[i].ncol; c++)
				{
				rect.x = reg[i].x + st + width;
				rect.y = reg[i].y + st + height;
				rect.width = GetColWidth(g, c);
				rect.height = GetRowHeight(g, r);
				if (g->grid.singleColScrollMode)
					rect.x -= g->grid.singleColScrollPos;
				ClipRectToReg(g, &rect, &reg[i]);
				if (XmLRectIntersect(&xyRect, &rect) != XmLRectOutside)
					{
					if (!RowColFirstSpan(g, r, c, row, col, 0, True, True))
						return 0; /* SUCCESS */
					else
						return -1;
					}
				width += GetColWidth(g, c);
				}
			height += GetRowHeight(g, r);
			}
		}
	return -1;
	}

static int
RowColToXY(XmLGridWidget g,
	   int row,
	   int col,
	   Boolean clipped,
	   XRectangle *rect)
	{
	XmLGridCell cell;
	XmLGridCellRefValues *cellValues;
	GridReg *reg;
	Dimension st;
	int i, r, c, off;

	reg = g->grid.reg;
	st = g->manager.shadow_thickness;
	cell = GetCell(g, row, col);
	if (!cell)
		return -1;
	cellValues = XmLGridCellGetRefValues(cell);

	if (!cellValues) return -1;

	for (i = 0; i < 9; i++)
		{
		if (!reg[i].width || !reg[i].height)
			continue;
		if (!(col >= reg[i].col &&
			col < reg[i].col + reg[i].ncol &&
			row >= reg[i].row &&
			row < reg[i].row + reg[i].nrow))
			continue;
		off = 0;
		for (r = reg[i].row; r < row; r++)
			off += GetRowHeight(g, r);
		rect->y = reg[i].y + st + off;
		rect->height = GetRowHeight(g, row);
		if (cellValues->rowSpan)
			for (r = row + 1; r <= row + cellValues->rowSpan; r++)
				rect->height += GetRowHeight(g, r);
		off = 0;
		for (c = reg[i].col; c < col; c++)
			off += GetColWidth(g, c);
		rect->x = reg[i].x + st + off;
		rect->width = GetColWidth(g, col);
		if (cellValues->columnSpan)
			for (c = col + 1; c <= col + cellValues->columnSpan; c++)
				rect->width += GetColWidth(g, c);
		if (g->grid.singleColScrollMode)
			rect->x -= g->grid.singleColScrollPos;
		if (clipped == True)
			ClipRectToReg(g, rect, &reg[i]);
		return 0;
		}
	return -1;
	}

static int
RowColFirstSpan(XmLGridWidget g,
		int row,
		int col,
		int *firstRow,
		int *firstCol, 
		XRectangle *rect,
		Boolean lookLeft,
		Boolean lookUp)
	{
	XmLGridCell cell;
	int done;

	done = 0;
	while (!done)
		{
		cell = GetCell(g, row, col);
		if (!cell)
			return -1;
		if (XmLGridCellInRowSpan(cell) == True)
			{
			if (lookUp == True)
				{
				row--;
				if (rect)
					rect->y -= GetRowHeight(g, row);
				}
			else
				row = -1;
			if (row < 0)
				done = 1;
			}
		else if (XmLGridCellInColumnSpan(cell) == True)
			{
			if (lookLeft == True)
				{
				col--;
				if (rect)
					rect->x -= GetColWidth(g, col);
				}
			else
				col = -1;
			if (col < 0)
				done = 1;
			}
		else
			done = 1;
		}
	if (row < 0 || col < 0)
		return -1;
	*firstRow = row;
	*firstCol = col;
	return 0;
	}

static void
RowColSpanRect(XmLGridWidget g,
	       int row,
	       int col,
	       XRectangle *rect)
	{
	XmLGridCell cell;
	XmLGridCellRefValues *cellValues;
	int r, c;

	cell = GetCell(g, row, col);
	cellValues = XmLGridCellGetRefValues(cell);
	rect->width = 0;
	rect->height = 0;
	for (r = row; r <= row + cellValues->rowSpan; r++)
		rect->height += GetRowHeight(g, r);
	for (c = col; c <= col + cellValues->columnSpan; c++)
		rect->width += GetColWidth(g, c);
	}

static XmLGridCell
GetCell(XmLGridWidget g,
	int row,
	int col)
	{
	XmLGridRow rowp;

	rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
	if (!rowp)
		return 0;
	return (XmLGridCell)XmLArrayGet(XmLGridRowCells(rowp), col);
	}

static int
GetColWidth(XmLGridWidget g,
	    int col)
	{
	XmLGridColumn colp;

	colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, col);
	if (!colp)
		return 0;
	return XmLGridColumnWidthInPixels(colp);
	}

static int
GetRowHeight(XmLGridWidget g,
	     int row)
	{
	XmLGridRow rowp;

	rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
	if (!rowp)
		return 0;
	return XmLGridRowHeightInPixels(rowp);
	}

static int
ColIsVisible(XmLGridWidget g,
	     int col)
	{
	XmLGridColumn c;
	int i, c1, c2;

	c = (XmLGridColumn)XmLArrayGet(g->grid.colArray, col);
	if (!c)
		{
		XmLWarning((Widget)g, "ColumnIsVisible() - invalid column");
		return -1;
		}
	if (XmLGridColumnIsHidden(c) == True)
		return 0;
	if (g->grid.hsPolicy != XmCONSTANT)
		return 1;
	for (i = 0; i < 3; i ++)
		{
		c1 = g->grid.reg[i].col;
		c2 = c1 + g->grid.reg[i].ncol;
		if (col >= c1 && col < c2)
			return 1;
		}
	return 0;
	}

static int
RowIsVisible(XmLGridWidget g,
	     int row)
	{
	XmLGridRow r;
	int i, r1, r2;

	r = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
	if (!r)
		{
		XmLWarning((Widget)g, "RowIsVisible() - invalid row");
		return -1;
		}
	if (XmLGridRowIsHidden(r) == True)
		return 0;
	if (g->grid.vsPolicy != XmCONSTANT)
		return 1;
	for (i = 0; i < 9; i += 3)
		{
		r1 = g->grid.reg[i].row;
		r2 = r1 + g->grid.reg[i].nrow;
		if (row >= r1 && row < r2)
			return 1;
		}
	return 0;
	}

static XtGeometryResult
GeometryManager(Widget w,
		XtWidgetGeometry *request,
		XtWidgetGeometry *allow)
	{
	if (request->request_mode & CWWidth)
		w->core.width = request->width;
	if (request->request_mode & CWHeight)
		w->core.height = request->height;
	if (request->request_mode & CWX)
		w->core.x = request->x;
	if (request->request_mode & CWY)
		w->core.y = request->y;
	if (request->request_mode & CWBorderWidth)
		w->core.border_width = request->border_width;
	return XtGeometryYes;
	}

static void
ScrollCB(Widget w,
	 XtPointer clientData,
	 XtPointer callData)
{
	XmLGridWidget g;
	XmScrollBarCallbackStruct *cbs;
	unsigned char orientation;
	int visPos;

	g = (XmLGridWidget)(XtParent(w));
	cbs = (XmScrollBarCallbackStruct *)callData;
	visPos = cbs->value;
	XtVaGetValues(w, XmNorientation, &orientation, NULL);
	if (orientation == XmHORIZONTAL && g->grid.singleColScrollMode)
		{
		g->grid.singleColScrollPos = visPos;
		DrawArea(g, DrawHScroll, 0, 0);
		}
	else if (orientation == XmVERTICAL)
		{
		if (visPos == PosToVisPos(g, g->grid.scrollRow, 1))
			return;
		g->grid.scrollRow = VisPosToPos(g, visPos, 1);
		VertLayout(g, 0);
		DrawArea(g, DrawVScroll, 0, 0);
		}
	else
		{
		if (visPos == PosToVisPos(g, g->grid.scrollCol, 0))
			return;
		g->grid.scrollCol = VisPosToPos(g, visPos, 0);
		HorizLayout(g, 0);
		DrawArea(g, DrawHScroll, 0, 0);
		}
	}

static int
FindNextFocus(XmLGridWidget g,
	      int row,
	      int col,
	      int rowDir,
	      int colDir,
	      int *nextRow,
	      int *nextCol)
	{
	int traverse;
	int hrc, hcc, rc, cc;
	XmLGridColumn colp;
	XmLGridRow rowp;
	XmLGridCell cell;

	hcc = g->grid.headingColCount;
	cc = g->grid.colCount;
	hrc = g->grid.headingRowCount;
	rc = g->grid.rowCount;
	if (!rc || !cc)
		return -1;
	if (col < hcc)
		col = hcc;
	else if (col >= hcc + cc)
		col = hcc + cc - 1;
	if (row < hrc)
		row = hrc;
	else if (row >= hrc + rc)
		row = hrc + rc - 1;
	traverse = 0;
	while (1)
		{
		if (row < hrc || row >= hrc + rc)
			break;
		if (col < hcc || col >= hcc + cc)
			break;
		rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
		colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, col);
		cell = GetCell(g, row, col);
		if (cell &&
			RowPosToType(g, row) == XmCONTENT &&
			ColPosToType(g, col) == XmCONTENT &&
			XmLGridCellInRowSpan(cell) == False &&
			XmLGridCellInColumnSpan(cell) == False &&
			XmLGridRowIsHidden(rowp) == False &&
			XmLGridColumnIsHidden(colp) == False)
			{
			traverse = 1;
			break;
			}
		if (!rowDir && !colDir)
			break;
		row += rowDir;
		col += colDir;
		}
	if (!traverse)
		return -1;
	*nextRow = row;
	*nextCol = col;
	return 0;
	}

static int
SetFocus(XmLGridWidget g,
	 int row,
	 int col,
	 int rowDir,
	 int colDir)
	{
	if (FindNextFocus(g, row, col, rowDir, colDir, &row, &col) == -1)
		return -1;
	ChangeFocus(g, row, col);
	return 0;
	}

static void
ChangeFocus(XmLGridWidget g,
	    int row,
	    int col)
	{
	XmLGridCell cell;
	XmLGridCallbackStruct cbs;
	int focusRow, focusCol;

	focusRow = g->grid.focusRow;
	focusCol = g->grid.focusCol;
	g->grid.focusRow = row;
	g->grid.focusCol = col;
	if (focusRow != -1 && focusCol != -1)
		{
		cell = GetCell(g, focusRow, focusCol);
		if (cell)
			{
			if (g->grid.highlightRowMode == True)
				DrawArea(g, DrawRow, focusRow, 0);
			else
				DrawArea(g, DrawCell, focusRow, focusCol);
			cbs.reason = XmCR_CELL_FOCUS_OUT;
			cbs.columnType = ColPosToType(g, focusCol);
			cbs.column = ColPosToTypePos(g, cbs.columnType, focusCol);
			cbs.rowType = RowPosToType(g, focusRow);
			cbs.row = RowPosToTypePos(g, cbs.rowType, focusRow);
			XmLGridCellAction(cell, (Widget)g, &cbs);
			XtCallCallbackList((Widget)g, g->grid.cellFocusCallback,
				(XtPointer)&cbs);
			}
		}
	if (row != -1 && col != -1)
		{
		cell = GetCell(g, row, col);
		if (cell)
			{
			if (g->grid.highlightRowMode == True)
				DrawArea(g, DrawRow, row, 0);
			else
				DrawArea(g, DrawCell, row, col);
			cbs.reason = XmCR_CELL_FOCUS_IN;
			cbs.columnType = ColPosToType(g, col);
			cbs.column = ColPosToTypePos(g, cbs.columnType, col);
			cbs.rowType = RowPosToType(g, row);
			cbs.row = RowPosToTypePos(g, cbs.rowType, row);
			XmLGridCellAction(cell, (Widget)g, &cbs);
			XtCallCallbackList((Widget)g, g->grid.cellFocusCallback,
				(XtPointer)&cbs);
			}
		else
			{
			if (!XmLArrayGet(g->grid.rowArray, row))
				g->grid.focusRow = -1;
			if (!XmLArrayGet(g->grid.colArray, col))
				g->grid.focusCol = -1;
			}
		}
	}
	
static void
MakeColVisible(XmLGridWidget g,
	       int col)
	{
	int st, width, scrollWidth, scrollCol, prevScrollCol;

	if (g->grid.hsPolicy != XmCONSTANT)
		return;
	if (col < g->grid.leftFixedCount ||
		col >= XmLArrayGetCount(g->grid.colArray) - g->grid.rightFixedCount)
		return;
	st = g->manager.shadow_thickness;
	scrollCol = col;
	if (col > g->grid.scrollCol)
		{
		scrollWidth = g->grid.reg[4].width - st * 2;
		width = 0;
		while (1)
			{
			width += GetColWidth(g, scrollCol);
			if (width > scrollWidth)
				break;
			if (scrollCol < g->grid.leftFixedCount)
				break;
			scrollCol--;
			}
		scrollCol++;
		if (scrollCol > col)
			scrollCol = col;
		/* only scroll if needed */
		if (scrollCol < g->grid.scrollCol)
			scrollCol = g->grid.scrollCol;
		}
	if (scrollCol == g->grid.scrollCol)
		return;
	prevScrollCol = g->grid.scrollCol;
	g->grid.scrollCol = scrollCol;
	HorizLayout(g, 0);
	if (g->grid.scrollCol != prevScrollCol)
		DrawArea(g, DrawHScroll, 0, 0);
	}

static void
MakeRowVisible(XmLGridWidget g,
	       int row)
	{
	int st, height, scrollHeight, scrollRow, prevScrollRow;

	if (g->grid.vsPolicy != XmCONSTANT)
		return;
	if (row < g->grid.topFixedCount ||
		row >= XmLArrayGetCount(g->grid.rowArray) - g->grid.bottomFixedCount)
		return;
	st = g->manager.shadow_thickness;
	scrollRow = row;
	if (row > g->grid.scrollRow)
		{
		scrollHeight = g->grid.reg[4].height - st * 2;
		height = 0;
		while (1)
			{
			height += GetRowHeight(g, scrollRow);
			if (height > scrollHeight)
				break;
			if (scrollRow < g->grid.topFixedCount)
				break;
			scrollRow--;
			}
		scrollRow++;
		if (scrollRow > row)
			scrollRow = row;
		/* only scroll if needed */
		if (scrollRow < g->grid.scrollRow)
			scrollRow = g->grid.scrollRow;
		}
	if (scrollRow == g->grid.scrollRow)
		return;
	prevScrollRow = g->grid.scrollRow;
	g->grid.scrollRow = scrollRow;
	VertLayout(g, 0);
	if (g->grid.scrollRow != prevScrollRow)
		DrawArea(g, DrawVScroll, 0, 0);
	}

static void
TextAction(XmLGridWidget g,
	   int action)
	{
	int row, col, reason, ret, isVisible;
	XRectangle rect;
	XtTranslations trans;
	WidgetClass wc;
	XmLGridCell cellp;
	XmLGridCallbackStruct cbs;

	if (!g->grid.text || !XtIsRealized(g->grid.text))
		return;
	row = g->grid.focusRow;
	col = g->grid.focusCol;
	if (row == -1 || col == -1)
		return;
	isVisible = 0;
	if (RowColToXY(g, row, col, True, &rect) != -1)
		isVisible = 1;
	cellp = GetCell(g, row, col);
	if (!cellp)
		return;
	cbs.rowType = XmCONTENT;
	cbs.row = RowPosToTypePos(g, XmCONTENT, row);
	cbs.columnType = XmCONTENT;
	cbs.column = ColPosToTypePos(g, XmCONTENT, col);
	cbs.clipRect = &rect;
	switch (action)
		{
		case TEXT_EDIT:
		case TEXT_EDIT_INSERT:
			{
			if (g->grid.inEdit || !isVisible)	
				return;
			g->grid.inEdit = 1;
			if (action == TEXT_EDIT)
				cbs.reason = XmCR_EDIT_BEGIN;
			else
				cbs.reason = XmCR_EDIT_INSERT;
			ret = XmLGridCellAction(cellp, (Widget)g, &cbs);
			if (ret)
				{
				reason = cbs.reason;
				cbs.reason = XmCR_CONF_TEXT;
				XmLGridCellAction(cellp, (Widget)g, &cbs);
				cbs.reason = reason;
				wc = g->grid.text->core.widget_class;
				trans = (XtTranslations)wc->core_class.tm_table;
				XtVaSetValues(g->grid.text, XmNtranslations, trans, NULL);
				XtOverrideTranslations(g->grid.text, g->grid.editTrans);
				XtMapWidget(g->grid.text);
				g->grid.textHidden = 0;
				XtCallCallbackList((Widget)g, g->grid.editCallback,
					(XtPointer)&cbs);
				}
			else
				g->grid.inEdit = 0;
			break;
			}
		case TEXT_EDIT_CANCEL:
		case TEXT_EDIT_COMPLETE:
			{
			if (!g->grid.inEdit)
				return;
			if (action == TEXT_EDIT_COMPLETE)
				cbs.reason = XmCR_EDIT_COMPLETE;
			else
				cbs.reason = XmCR_EDIT_CANCEL;
			XmLGridCellAction(cellp, (Widget)g, &cbs);
			wc = g->grid.text->core.widget_class;
			trans = (XtTranslations)wc->core_class.tm_table;
			XtVaSetValues(g->grid.text, XmNtranslations, trans, NULL);
			XtOverrideTranslations(g->grid.text, g->grid.traverseTrans);
			XtCallCallbackList((Widget)g, g->grid.editCallback,
				(XtPointer)&cbs);
			XmTextSetString(g->grid.text, "");
			XmTextSetInsertionPosition(g->grid.text, 0);
			g->grid.inEdit = 0;
			XtUnmapWidget(g->grid.text);
			g->grid.textHidden = 1;
			XtConfigureWidget(g->grid.text, 0, 0, 30, 10, 0);
			break;
			}
		case TEXT_HIDE:
			{
			if (g->grid.textHidden || !isVisible)
				return;
			XtUnmapWidget(g->grid.text);
			g->grid.textHidden = 1;
			break;
			}
		case TEXT_SHOW:
			{
			if (!g->grid.textHidden || !g->grid.inEdit || !isVisible)
				return;
			if (rect.width == 0 || rect.height == 0)
				TextAction(g, TEXT_EDIT_CANCEL);
			else
				{
				cbs.reason = XmCR_CONF_TEXT;
				XmLGridCellAction(cellp, (Widget)g, &cbs);
				XtMapWidget(g->grid.text);
				g->grid.textHidden = 0;
				}
			break;
			}
		}
	}

/*
   Getting and Setting Values
*/

static void
GetSubValues(Widget w,
	     ArgList args,
	     Cardinal *nargs)
	{
	XmLGridWidget g;
	int i, c;
	long mask;
	XmLGridRow row;
	XmLGridColumn col;
	XmLGridCell cell;

	g = (XmLGridWidget)w;
	row = 0;
	col = 0;
	for (i = 0; i < *nargs; i++)
		{
		if (!args[i].name)
			continue;
		if (!strncmp(args[i].name, "row", 3))
			{
			if (!strcmp(args[i].name, XmNrowPtr))
				{
				row = (XmLGridRow)args[i].value;
				continue;
				}
			mask = 0;
			GetRowValueMask(g, args[i].name, &mask);
			if (!mask)
				continue;
			if (!row)
				{
				XmLWarning(w, "GetValues() - invalid row");
				continue;
				}
			GetRowValue(g, row, args[i].value, mask);
			}
		else if (!strncmp(args[i].name, "column", 6))
			{
			if (!strcmp(args[i].name, XmNcolumnPtr))
				{
				col = (XmLGridColumn)args[i].value;
				continue;
				}
			mask = 0;
			GetColumnValueMask(g, args[i].name, &mask);
			if (!mask)
				continue;
			if (!col)
				{
				XmLWarning(w, "GetValues() - invalid column");
				continue;
				}
			GetColumnValue(g, col, args[i].value, mask);
			}
		else if (!strncmp(args[i].name, "cell", 4))
			{
			mask = 0;
			CellValueGetMask(args[i].name, &mask);
			if (!mask)
				continue;
			if (!row || !col)
				{
				XmLWarning(w, "GetValues() - invalid cell");
				continue;
				}
			c = XmLGridColumnGetPos(col);
			cell = (XmLGridCell)XmLArrayGet(XmLGridRowCells(row), c);
			GetCellValue(cell, args[i].value, mask);
			}
		}
	}

static void
SetSubValues(Widget w,
	     ArgList args,
	     Cardinal *nargs)
	{
	XmLGridWidget g;
	XmLGridRow row;
	XmLGridColumn col;
	XmLGridCell cell;
	XmLGridCellRefValues *values, *newValues, *prevValues;
	XmLArray cellArray;
	int r, r1, r2, rowsValid;
	int c, c1, c2, cd1, cd2, colsValid;
	unsigned char rowType, colType;
	long rowMask, colMask, cellMask;
	int i, nRefValues, allCols;
	int needsHorizResize, needsVertResize;
	char buf[256];

	newValues = (XmLGridCellRefValues *)NULL;
	g = (XmLGridWidget)w;
	needsHorizResize = 0;
	needsVertResize = 0;
	rowMask = 0;
	colMask = 0;
	cellMask = 0;
	for (i = 0; i < *nargs; i++)
		{
		if (!args[i].name)
			continue;
		if (!strncmp(args[i].name, "row", 3))
			{
			if (!strcmp(args[i].name, XmNrowPtr))
				XmLWarning(w, "SetValues() - can't use XmNrowPtr");
			GetRowValueMask(g, args[i].name, &rowMask);
			}
		else if (!strncmp(args[i].name, "column", 6))
			{
			if (!strcmp(args[i].name, XmNcolumnPtr))
				XmLWarning(w, "SetValues() - can't use XmNcolumnPtr");
			GetColumnValueMask(g, args[i].name, &colMask);
			}
		else if (!strncmp(args[i].name, "cell", 4))
			CellValueGetMask(args[i].name, &cellMask);
		else if (rowMask || colMask || cellMask)
			{
			sprintf(buf, "SetValues() - %s is not a row/column/cell resource",
				args[i].name);
			XmLWarning(w, buf);
			}
		}
	if (rowMask || colMask || cellMask)
		{
		if (g->grid.rowType == XmINVALID_TYPE)
			rowType = XmCONTENT;
		else
			rowType = g->grid.rowType;
		rowsValid = 1;
		if (g->grid.cellRowRangeStart != -1 && g->grid.cellRowRangeEnd != -1)
			{
			r1 = RowTypePosToPos(g, rowType, g->grid.cellRowRangeStart, 0);
			r2 = RowTypePosToPos(g, rowType, g->grid.cellRowRangeEnd, 0);
			if (r1 < 0 || r2 < 0 || r1 > r2)
				rowsValid = 0;
			r2++;
			}
		else if (g->grid.cellRow != -1)
			{
			r1 = RowTypePosToPos(g, rowType, g->grid.cellRow, 0);
			if (r1 == -1)
				rowsValid = 0;
			r2 = r1 + 1;
			}
		else
			{
			r1 = RowTypePosToPos(g, rowType, 0, 0);
			if (r1 == -1)
				r1 = 0;
			r2 = RowTypePosToPos(g, rowType, -1, 1);
			}
		if (!rowsValid)
			{
			XmLWarning(w, "SetValues() - invalid row(s) specified");
			r1 = 0;
			r2 = 0;
			}
		if (g->grid.colType == XmINVALID_TYPE)
			colType = XmCONTENT;
		else
			colType = g->grid.colType;
		colsValid = 1;
		if (g->grid.cellColRangeStart != -1 && g->grid.cellColRangeEnd != -1)
			{
			c1 = ColTypePosToPos(g, colType, g->grid.cellColRangeStart, 0);
			c2 = ColTypePosToPos(g, colType, g->grid.cellColRangeEnd, 0);
			if (c1 < 0 || c2 < 0 || c1 > c2)
				colsValid = 0;
			c2++;
			}
		else if (g->grid.cellCol != -1)
			{
			c1 = ColTypePosToPos(g, colType, g->grid.cellCol, 0);
			if (c1 == -1)
				colsValid = 0;
			c2 = c1 + 1;
			}
		else
			{
			c1 = ColTypePosToPos(g, colType, 0, 0);
			if (c1 == -1)
				c1 = 0;
			c2 = ColTypePosToPos(g, colType, -1, 1);
			}
		if (!colsValid)
			{
			XmLWarning(w, "SetValues() - invalid column(s) specified");
			c1 = 0;
			c2 = 0;
			}
		if (g->grid.debugLevel)
			{
			fprintf(stderr, "XmLGrid: SetValues for rows %d to %d\n", r1, r2);
			fprintf(stderr, "XmLGrid: & columns %d to %d\n", c1, c2);
			}
		if (rowMask)
			for (r = r1; r < r2; r += g->grid.rowStep)
				{
				row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
				if (!row)
					continue;
				if (SetRowValues(g, row, rowMask))
					needsVertResize = 1;
				DrawArea(g, DrawRow, r, 0);
				}
		if (colMask)
			for (c = c1; c < c2; c += g->grid.colStep)
				{
				col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
				if (!col)
					continue;
				if (SetColumnValues(g, col, colMask))
					needsHorizResize = 1;
				DrawArea(g, DrawCol, 0, c);
				}
		if (cellMask)
			SetCellValuesPreprocess(g, cellMask);
		if (cellMask && g->grid.cellDefaults == True)
			{
			allCols = 0;
			if (g->grid.colType == XmINVALID_TYPE &&
				g->grid.cellCol == -1 &&
				g->grid.cellColRangeStart == -1 &&
				g->grid.cellColRangeEnd == -1 &&
				g->grid.colStep == 1)
				allCols = 1;

			if (allCols)
				{
				/* set cell default values */
				newValues = CellRefValuesCreate(g, g->grid.defCellValues);
				newValues->refCount = 1;
				SetCellRefValues(g, newValues, cellMask);
				if (newValues->rowSpan || newValues->columnSpan)
					{
					newValues->rowSpan = 0;
					newValues->columnSpan = 0;
					XmLWarning((Widget)g,
						"SetValues() - can't set default cell spans");
					}
				XmLGridCellDerefValues(g->grid.defCellValues);
				g->grid.defCellValues = newValues;
				cd1 = 0;
				cd2 = XmLArrayGetCount(g->grid.colArray);
				}
			else
				{
				cd1 = c1;
				cd2 = c2;
				}
			/* set column cell default values */
			for (c = cd1; c < cd2; c += g->grid.colStep)
				{
				col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
				if (!col)
					continue;
				if (allCols && !col->grid.defCellValues)
					continue;
				if (col->grid.defCellValues)
					newValues = CellRefValuesCreate(g, col->grid.defCellValues);
				else
					newValues = CellRefValuesCreate(g, g->grid.defCellValues);
				newValues->refCount = 1;
				SetCellRefValues(g, newValues, cellMask);
				if (newValues->rowSpan || newValues->columnSpan)
					{
					newValues->rowSpan = 0;
					newValues->columnSpan = 0;
					XmLWarning((Widget)g,
						"SetValues() - can't set default cell spans");
					}
				if (col->grid.defCellValues)
					XmLGridCellDerefValues(col->grid.defCellValues);
				col->grid.defCellValues = newValues;
				}
			}
		else if (cellMask)
			{
			/* set cell values */
			if (SetCellHasRefValues(cellMask))
				{
				cellArray = XmLArrayNew(0, 0);
				XmLArrayAdd(cellArray, -1, (r2 - r1) * (c2 - c1));
				nRefValues = 0;
				for (r = r1; r < r2; r += g->grid.rowStep)
					for (c = c1; c < c2; c += g->grid.colStep)
						{
						cell = GetCell(g, r, c);
						if (!cell)
							continue;
						SetCellRefValuesPreprocess(g, r, c, cell, cellMask);
						XmLArraySet(cellArray, nRefValues, cell);
						nRefValues++;
						}
				XmLArraySort(cellArray, SetCellRefValuesCompare,
					(void *)&cellMask, 0, nRefValues);
				prevValues = 0;
				for (i = 0; i < nRefValues; i++)
					{
					cell = (XmLGridCell)XmLArrayGet(cellArray, i);
					values = XmLGridCellGetRefValues(cell);
					if (values != prevValues)
						{
						newValues = CellRefValuesCreate(g, values);
						SetCellRefValues(g, newValues, cellMask);
						}
					XmLGridCellSetRefValues(cell, newValues);
					prevValues = values;
					}
				XmLArrayFree(cellArray);
				}
			for (r = r1; r < r2; r += g->grid.rowStep)
				for (c = c1; c < c2; c += g->grid.colStep)
					{
					row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
					col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
					cell = GetCell(g, r, c);
					if (!row || !col || !cell)
						continue;
					if (SetCellValuesResize(g, row, col, cell, cellMask))
						{
						needsHorizResize = 1;
						needsVertResize = 1;
						}
					SetCellValues(g, cell, cellMask);
					if (!needsHorizResize && !needsVertResize)
						DrawArea(g, DrawCell, r, c);
					}
			}
		}
	if (needsHorizResize)
		HorizLayout(g, 1);
	if (needsVertResize)
		VertLayout(g, 1);
	if (needsHorizResize || needsVertResize)
		DrawArea(g, DrawAll, 0, 0);
	g->grid.rowType = XmINVALID_TYPE;
	g->grid.colType = XmINVALID_TYPE;
	g->grid.rowStep = 1;
	g->grid.colStep = 1;
	g->grid.cellRow = -1;
	g->grid.cellRowRangeStart = -1;
	g->grid.cellRowRangeEnd = -1;
	g->grid.cellCol = -1;
	g->grid.cellColRangeStart = -1;
	g->grid.cellColRangeEnd = -1;
	g->grid.cellDefaults = False;
	}

static Boolean
SetValues(Widget curW,
	  Widget reqW,
	  Widget newW,
	  ArgList args,
	  Cardinal *nargs)
	{
	XmLGridWidget g, cur;
	int hc, c, fc, hr, r, fr;
	int tfc, bfc, lfc, rfc;
	int needsVertLayout, needsHorizLayout, needsRedraw;
	XmLGridCellRefValues *cellValues;
 
	g = (XmLGridWidget)newW;
	cur = (XmLGridWidget)curW;
	needsRedraw = 0;
	needsVertLayout = 0;
	needsHorizLayout = 0;

#define NE(value) (g->value != cur->value)
	if (NE(grid.cScrollCol))
		{
		g->grid.scrollCol = g->grid.cScrollCol + g->grid.headingColCount;
		needsHorizLayout = 1;
		needsRedraw = 1;
		}
	if (NE(grid.cScrollRow))
		{
		g->grid.scrollRow = g->grid.cScrollRow + g->grid.headingRowCount;
		needsVertLayout = 1;
		needsRedraw = 1;
		}
	if (NE(grid.bottomFixedCount) ||
		NE(grid.bottomFixedMargin) ||
		NE(grid.topFixedCount) ||
		NE(grid.topFixedMargin) ||
		NE(grid.vsbDisplayPolicy))
		{
		needsVertLayout = 1;
		needsRedraw = 1;
		}
	if (NE(grid.leftFixedCount) ||
		NE(grid.leftFixedMargin) ||
		NE(grid.rightFixedCount) ||
		NE(grid.rightFixedMargin) ||
		NE(grid.hsbDisplayPolicy))
		{
		needsHorizLayout = 1;
		needsRedraw = 1;
		}
	if (NE(grid.layoutFrozen))
		{
		if (g->grid.layoutFrozen == False)
			{
			if (g->grid.needsVertLayout == True)
				{
				needsVertLayout = 1;
				g->grid.needsVertLayout = False;
				}
			if (g->grid.needsHorizLayout == True)
				{
				needsHorizLayout = 1;
				g->grid.needsHorizLayout = False;
				}
			needsRedraw = 1;
			}
		}
	if (NE(grid.scrollBarMargin) ||
		NE(manager.shadow_thickness) ||
		NE(core.border_width) ||
		NE(grid.vsPolicy) ||
		NE(grid.hsPolicy))
		{
		needsVertLayout = 1;
		needsHorizLayout = 1;
		needsRedraw = 1;
		}
	if (NE(grid.blankBg) ||
		NE(grid.highlightThickness) ||
		NE(grid.selectBg) ||
		NE(grid.selectFg) ||
		NE(grid.toggleTopColor) ||
		NE(grid.toggleBotColor) ||
		NE(grid.shadowRegions) ||
		NE(grid.shadowType))
		needsRedraw = 1;
	if (NE(grid.fontList))
		{
		XmFontListFree(cur->grid.fontList);
		CopyFontList(g);
		cellValues = CellRefValuesCreate(g, g->grid.defCellValues);
		cellValues->refCount = 1;
		XmFontListFree(cellValues->fontList);
		cellValues->fontList = XmFontListCopy(g->grid.fontList);
		XmLFontListGetDimensions(cellValues->fontList,
			&cellValues->fontWidth, &cellValues->fontHeight,
			g->grid.useAvgWidth);
		XmLGridCellDerefValues(g->grid.defCellValues);
		g->grid.defCellValues = cellValues;
		}
	if (NE(grid.allowDrop))
		DropRegister(g, g->grid.allowDrop); 
	if (g->grid.rowStep < 1)
		{
		XmLWarning(newW, "SetValues() - rowStep can't be < 1");
		g->grid.rowStep = 1;
		}
	if (g->grid.colStep < 1)
		{
		XmLWarning(newW, "SetValues() - colStep can't be < 1");
		g->grid.colStep = 1;
		}
	if (NE(grid.hsb) ||
		NE(grid.vsb) ||
		NE(grid.text))
		{
		XmLWarning(newW, "SetValues() - child Widgets are read-only");
		g->grid.hsb = cur->grid.hsb;
		g->grid.vsb = cur->grid.vsb;
		g->grid.text = cur->grid.text;
		}
	if (NE(grid.useTextWidget))
		{
		XmLWarning(newW, "SetValues() - can't set use of text widget");
		g->grid.useTextWidget = cur->grid.useTextWidget;
		}
	if (NE(grid.hiddenColCount) ||
		NE(grid.hiddenRowCount))
		{
		XmLWarning(newW, "SetValues() - can't set hidden rows or columns");
		g->grid.hiddenColCount = cur->grid.hiddenColCount;
		g->grid.hiddenRowCount = cur->grid.hiddenRowCount;
		}

	/* store fixed counts, add/removing rows/columns can change these */
	tfc = -1;
	bfc = -1;
	lfc = -1;
	rfc = -1;
	if (NE(grid.topFixedCount))
		tfc = g->grid.topFixedCount;
	if (NE(grid.bottomFixedCount))
		bfc = g->grid.bottomFixedCount;
	if (NE(grid.leftFixedCount))
		lfc = g->grid.leftFixedCount;
	if (NE(grid.rightFixedCount))
		rfc = g->grid.rightFixedCount;
	g->grid.topFixedCount = cur->grid.topFixedCount;
	g->grid.bottomFixedCount = cur->grid.bottomFixedCount;
	g->grid.leftFixedCount = cur->grid.leftFixedCount;
	g->grid.rightFixedCount = cur->grid.rightFixedCount;

	hc = g->grid.headingColCount - cur->grid.headingColCount;
	c = g->grid.colCount - cur->grid.colCount;
	fc = g->grid.footerColCount - cur->grid.footerColCount;
	hr = g->grid.headingRowCount - cur->grid.headingRowCount;
	r = g->grid.rowCount - cur->grid.rowCount;
	fr = g->grid.footerRowCount - cur->grid.footerRowCount;
	g->grid.headingColCount = cur->grid.headingColCount;
	g->grid.colCount = cur->grid.colCount;
	g->grid.footerColCount = cur->grid.footerColCount;
	g->grid.headingRowCount = cur->grid.headingRowCount;
	g->grid.rowCount = cur->grid.rowCount;
	g->grid.footerRowCount = cur->grid.footerRowCount;
	if (hc > 0)
		XmLGridAddColumns((Widget)g, XmHEADING, -1, hc);
	if (hc < 0)
		XmLGridDeleteColumns((Widget)g, XmHEADING,
			g->grid.headingColCount + hc, -(hc));
	if (c > 0)
		XmLGridAddColumns((Widget)g, XmCONTENT, -1, c);
	if (c < 0)
		XmLGridDeleteColumns((Widget)g, XmCONTENT,
			g->grid.colCount + c, -(c));
	if (fc > 0)
		XmLGridAddColumns((Widget)g, XmFOOTER, -1, fc);
	if (fc < 0)
		XmLGridDeleteColumns((Widget)g, XmFOOTER,
			g->grid.footerColCount + fc, -(fc));
	if (hr > 0)
		XmLGridAddRows((Widget)g, XmHEADING, -1, hr);
	if (hr < 0)
		XmLGridDeleteRows((Widget)g, XmHEADING,
			g->grid.headingRowCount + hr, -(hr));
	if (r > 0)
		XmLGridAddRows((Widget)g, XmCONTENT, -1, r);
	if (r < 0)
		XmLGridDeleteRows((Widget)g, XmCONTENT,
			g->grid.rowCount + r, -(r));
	if (fr > 0)
		XmLGridAddRows((Widget)g, XmFOOTER, -1, fr);
	if (fr < 0)
		XmLGridDeleteRows((Widget)g, XmFOOTER,
			g->grid.footerRowCount + fr, -(fr));

	/* restore fixed counts if user specified */
	if (tfc != -1)
		g->grid.topFixedCount = tfc;
	if (bfc != -1)
		g->grid.bottomFixedCount = bfc;
	if (lfc != -1)
		g->grid.leftFixedCount = lfc;
	if (rfc != -1)
		g->grid.rightFixedCount = rfc;

	if (g->grid.topFixedCount < g->grid.headingRowCount)
		{
		XmLWarning(newW,
			"SetValues() - can't set topFixedCount < headingRowCount");
		g->grid.topFixedCount = cur->grid.topFixedCount;
		}
	if (g->grid.bottomFixedCount < g->grid.footerRowCount)
		{
		XmLWarning(newW,
			"SetValues() - can't set bottomFixedCount < footerRowCount");
		g->grid.bottomFixedCount = cur->grid.bottomFixedCount;
		}
	if (g->grid.leftFixedCount < g->grid.headingColCount)
		{
		XmLWarning(newW,
			"SetValues() - can't set leftFixedCount < headingColumnCount");
		g->grid.leftFixedCount = cur->grid.leftFixedCount;
		}
	if (g->grid.rightFixedCount < g->grid.footerColCount)
		{
		XmLWarning(newW,
			"SetValues() - can't set rightFixedCount < footerColumnCount");
		g->grid.rightFixedCount = cur->grid.rightFixedCount;
		}

	if (NE(grid.simpleHeadings))
		{
		if (cur->grid.simpleHeadings)
			free((char *)cur->grid.simpleHeadings);
		if (g->grid.simpleHeadings)
			{
			g->grid.simpleHeadings = (char *)strdup(g->grid.simpleHeadings);
			SetSimpleHeadings(g, g->grid.simpleHeadings);
			}
		}
	if (NE(grid.simpleWidths))
		{
		if (cur->grid.simpleWidths)
			free((char *)cur->grid.simpleWidths);
		if (g->grid.simpleWidths)
			{
			g->grid.simpleWidths = (char *)strdup(g->grid.simpleWidths);
			SetSimpleWidths(g, g->grid.simpleWidths);
			}
		}
	if (NE(grid.visibleRows))
		{
		ApplyVisibleRows(g);
		needsVertLayout = 1;
		needsRedraw = 1;
		}
	if (NE(grid.visibleCols))
		{
		ApplyVisibleCols(g);
		needsHorizLayout = 1;
		needsRedraw = 1;
		}

	/* for the hidden columns stuff */
	if (NE(grid.showHideButton))
	    {
		if (g->grid.showHideButton)
		    {
			g->grid.hideButton = XtVaCreateWidget(
			    "hide", xmDrawnButtonWidgetClass,
			(Widget)g, XmNhighlightThickness, 0,
		        XmNshadowThickness, 1,
			XmNtraversalOn, False,
  		        XmNbackground, g->core.background_pixel,
		        XmNforeground, g->manager.foreground,
		        XmNtopShadowColor, g->manager.top_shadow_color,
		        XmNbottomShadowColor, g->manager.bottom_shadow_color,
			NULL);

			XmLDrawnButtonSetType(g->grid.hideButton,
					      XmDRAWNB_DOUBLEARROW,
					      XmDRAWNB_RIGHT);
		    }
		else 
		    {
			XtDestroyWidget(g->grid.hideButton);
		    }

		needsVertLayout = 1;
		needsHorizLayout = 1;
		needsRedraw = 1;
	    }
#undef NE

	SetSubValues(newW, args, nargs);

	if (needsVertLayout)
		VertLayout(g, 1);
	if (needsHorizLayout)
		HorizLayout(g, 1);
	if (needsRedraw)
		DrawArea(g, DrawAll, 0, 0);
	return (False);
	}

static void
CopyFontList(XmLGridWidget g)
	{
	if (!g->grid.fontList)
		g->grid.fontList = XmLFontListCopyDefault((Widget)g);
	else
		g->grid.fontList = XmFontListCopy(g->grid.fontList);
	if (!g->grid.fontList)
		XmLWarning((Widget)g, "- fatal error - font list NULL");
	}

static Boolean
CvtStringToSizePolicy(Display *dpy,
		      XrmValuePtr args,
		      Cardinal *narg,
		      XrmValuePtr fromVal,
		      XrmValuePtr toVal,
		      XtPointer *data)
	{
	static XmLStringToUCharMap map[] =
		{
		{ "CONSTANT", XmCONSTANT },
		{ "VARIABLE", XmVARIABLE },
		{ 0, 0 },
		};

	return XmLCvtStringToUChar(dpy, "XmRGridSizePolicy", map, fromVal, toVal);
	}

static Boolean
CvtStringToRowColType(Display *dpy,
		      XrmValuePtr args,
		      Cardinal *narg,
		      XrmValuePtr fromVal,
		      XrmValuePtr toVal,
		      XtPointer *data)
	{
	static XmLStringToUCharMap map[] =
		{
		{ "HEADING",    XmHEADING },
		{ "CONTENT",    XmCONTENT },
		{ "FOOTER",     XmFOOTER },
		{ "ALL_TYPES",  XmALL_TYPES },
		{ 0, 0 },
		};

	return XmLCvtStringToUChar(dpy, "XmRRowColType", map, fromVal, toVal);
	}

static Boolean
CvtStringToSelectionPolicy(Display *dpy,
			   XrmValuePtr args,
			   Cardinal *narg,
			   XrmValuePtr fromVal,
			   XrmValuePtr toVal,
			   XtPointer *data)
	{
	static XmLStringToUCharMap map[] =
		{
		{ "SELECT_NONE", XmSELECT_NONE },
		{ "SELECT_SINGLE_ROW", XmSELECT_SINGLE_ROW },
		{ "SELECT_BROWSE_ROW", XmSELECT_BROWSE_ROW },
		{ "SELECT_MULTIPLE_ROW", XmSELECT_MULTIPLE_ROW },
		{ "SELECT_CELL", XmSELECT_CELL },
		{ 0, 0 },
		};

	return XmLCvtStringToUChar(dpy, "XmRGridSelectionPolicy", map,
		fromVal, toVal);
	}

static Boolean
CvtStringToCellAlignment(Display *dpy,
			 XrmValuePtr args,
			 Cardinal *narg,
			 XrmValuePtr fromVal,
			 XrmValuePtr toVal,
			 XtPointer *data)
	{
	static XmLStringToUCharMap map[] =
		{
		{ "ALIGNMENT_LEFT", XmALIGNMENT_LEFT },
		{ "ALIGNMENT_CENTER", XmALIGNMENT_CENTER },
		{ "ALIGNMENT_RIGHT", XmALIGNMENT_RIGHT },
		{ "ALIGNMENT_TOP_LEFT", XmALIGNMENT_TOP_LEFT },
		{ "ALIGNMENT_TOP", XmALIGNMENT_TOP },
		{ "ALIGNMENT_TOP_RIGHT", XmALIGNMENT_TOP_RIGHT },
		{ "ALIGNMENT_BOTTOM_LEFT", XmALIGNMENT_BOTTOM_LEFT },
		{ "ALIGNMENT_BOTTOM", XmALIGNMENT_BOTTOM },
		{ "ALIGNMENT_BOTTOM_RIGHT", XmALIGNMENT_BOTTOM_RIGHT },
		{ 0, 0 },
		};

	return XmLCvtStringToUChar(dpy, "XmRGridCellAlignment", map,
		fromVal, toVal);
	}

static Boolean
CvtStringToCellBorderType(Display *dpy,
			  XrmValuePtr args,
			  Cardinal *narg,
			  XrmValuePtr fromVal,
			  XrmValuePtr toVal,
			  XtPointer *data)
	{
	static XmLStringToUCharMap map[] =
		{
		{ "BORDER_NONE", XmBORDER_NONE },
		{ "BORDER_LINE", XmBORDER_LINE },
		{ "BORDER_DASH", XmBORDER_DASH },
		{ 0, 0 },
		};

	return XmLCvtStringToUChar(dpy, "XmRGridCellBorderType", map,
		fromVal, toVal);
	}

static Boolean
CvtStringToCellType(Display *dpy,
		    XrmValuePtr args,
		    Cardinal *narg,
		    XrmValuePtr fromVal,
		    XrmValuePtr toVal,
		    XtPointer *data)
	{
	static XmLStringToUCharMap map[] =
		{
		{ "ICON_CELL",   XmICON_CELL },
		{ "STRING_CELL", XmSTRING_CELL },
		{ "PIXMAP_CELL", XmPIXMAP_CELL },
		{ "TOGGLE_CELL", XmTOGGLE_CELL },
		{ 0, 0 },
		};

	return XmLCvtStringToUChar(dpy, "XmRGridCellType", map, fromVal, toVal);
	}

static void
SetSimpleHeadings(XmLGridWidget g,
		  char *data)
	{
	char *c;
	int n, count;

	if (!data || !*data)
		return;
	c = data;
	n = 1;
	while (*c)
		{
		if (*c == '|')
			n++;
		c++;
		}
	count = XmLArrayGetCount(g->grid.colArray);
	if (n > count)
		{
		XmLWarning((Widget)g,
			"SetSimpleHeadings() - headings given for non-existing columns");
		return;
		}
	if (g->grid.headingRowCount < 1)
		{
		XmLWarning((Widget)g, "SetSimpleHeadings() - no heading row exists");
		return;
		}
	XmLGridSetStrings((Widget)g, data);
	}

static void
SetSimpleWidths(XmLGridWidget g,
		char *data)
	{
	XmLGridColumn colp;
	int i, n, colCount, valid;
	Dimension prevWidth;
	unsigned char prevSizePolicy;
	long mask;
	struct WidthRec
		{
		Dimension width;
		unsigned char sizePolicy;
		} *widths;

	if (!data)
		return;
	i = ((int)strlen(data) / 2) + 1;
	widths = (struct WidthRec *)malloc(i * sizeof(struct WidthRec));
	valid = 1;
	n = 0;
	while (*data)
		{
		if (*data >= '0' && *data <= '9')
			{	
			widths[n].width = atoi(data);
			while (*data >= '0' && *data <= '9')
				data++;
			}
		else
			{
			valid = 0;
			break;
			}
		if (*data == 'c' || *data == 'C')
			{
			widths[n].sizePolicy = XmVARIABLE;
			data++;
			}
		else if (*data == 'p' || *data == 'P')
			{
			widths[n].sizePolicy = XmCONSTANT;
			data++;
			}
		else
			{
			valid = 0;
			break;
			}
		while (*data == ' ')
			data++;
		n++;
		}
	if (!valid)
		{
		free((char *)widths);
		XmLWarning((Widget)g, "SetSimpleWidths() - invalid widths string");
		return;
		}
	colCount = XmLArrayGetCount(g->grid.colArray);
	if (n > colCount)
		{
		free((char *)widths);
		XmLWarning((Widget)g,
			"SetSimpleWidths() - widths given for non-existing columns");
		return;
		}
	prevWidth = g->grid.colWidth;
	prevSizePolicy = g->grid.colSizePolicy;
	for (i = 0; i < n; i++)
		{
		colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, i);
		if (!colp)
			continue;
		GetColumnValueMask(g, XmNcolumnWidth, &mask);
		GetColumnValueMask(g, XmNcolumnSizePolicy, &mask);
		g->grid.colWidth = widths[i].width;
		g->grid.colSizePolicy = widths[i].sizePolicy;
		SetColumnValues(g, colp, mask);
		}
	free((char *)widths);
	g->grid.colWidth = prevWidth;
	g->grid.colSizePolicy = prevSizePolicy;
	HorizLayout(g, 1);
	DrawArea(g, DrawAll, 0, 0);
	}

/*
   Getting and Setting Row Values
*/

static void
GetRowValueMask(XmLGridWidget g,
		char *s,
		long *mask)
	{
	XmLGridClassPartOfWidget(g).getRowValueMaskProc(g, s, mask);
	}

/* Only to be called through Grid class */
static void 
_GetRowValueMask(XmLGridWidget g,
		 char *s,
		 long *mask)
	{
	static XrmQuark qHeight, qSizePolicy, qUserData;
	static int quarksValid = 0;
	XrmQuark q;

	if (!quarksValid)
		{
		qHeight = XrmStringToQuark(XmNrowHeight);
		qSizePolicy = XrmStringToQuark(XmNrowSizePolicy);
		qUserData = XrmStringToQuark(XmNrowUserData);
		quarksValid = 1;
		}
	q = XrmStringToQuark(s);
	if (q == qHeight)
			*mask |= XmLGridRowHeight;
	else if (q == qSizePolicy)
			*mask |= XmLGridRowSizePolicy;
	else if (q == qUserData)
			*mask |= XmLGridRowUserData;
	}

static void
GetRowValue(XmLGridWidget g,
	    XmLGridRow row,
	    XtArgVal value,
	    long mask)
	{
	XmLGridClassPartOfWidget(g).getRowValueProc(g, row, value, mask);
	}

/* Only to be called through Grid class */
static void 
_GetRowValue(XmLGridWidget g,
	     XmLGridRow row,
	     XtArgVal value,
	     long mask)
	{
	switch (mask)
		{
		case XmLGridRowHeight:
			*((Dimension *)value) = row->grid.height; 
			break;
		case XmLGridRowSizePolicy:
			*((unsigned char *)value) = row->grid.sizePolicy; 
			break;
		case XmLGridRowUserData:
			*((XtPointer *)value) = (XtPointer)row->grid.userData;
			break;
		}
	}

static int
SetRowValues(XmLGridWidget g,
	     XmLGridRow row,
	     long mask)
	{
	return XmLGridClassPartOfWidget(g).setRowValuesProc(g, row, mask);
	}

/* Only to be called through Grid class */
static int 
_SetRowValues(XmLGridWidget g, XmLGridRow row, long mask)
	{
	int needsResize = 0, visible = 0;
	Boolean newIsHidden;

	if (mask & XmLGridRowHeight || mask & XmLGridRowSizePolicy)
		{
		visible = RowIsVisible(g, XmLGridRowGetPos(row));
		XmLGridRowHeightChanged(row);
		}
	if (mask & XmLGridRowHeight)
		{
		if (g->grid.rowHeight > 0)
			newIsHidden = False;
		else
			newIsHidden = True;
		if (XmLGridRowIsHidden(row) != newIsHidden)
			{
			if (newIsHidden == True)
				g->grid.hiddenRowCount++;
			else
				g->grid.hiddenRowCount--;
			VisPosChanged(g, 1);
			needsResize = 1;
			}
		if (visible && !g->grid.inResize)
			needsResize = 1;
		row->grid.height = g->grid.rowHeight;
		}
	if (mask & XmLGridRowSizePolicy)
		{
		row->grid.sizePolicy = g->grid.rowSizePolicy;
		if (visible && !g->grid.inResize)
			needsResize = 1;
		}
	if (mask & XmLGridRowUserData)
		row->grid.userData = g->grid.rowUserData;
	return needsResize;
	}

/*
   Getting and Setting Column Values
*/

static void
GetColumnValueMask(XmLGridWidget g,
		   char *s,
		   long *mask)
	{
	XmLGridClassPartOfWidget(g).getColumnValueMaskProc(g, s, mask);
	}

/* Only to be called through Grid class */
static void 
_GetColumnValueMask(XmLGridWidget g,
		    char *s,
		    long *mask)
	{
	static XrmQuark qWidth, qSizePolicy, qUserData, qResizable;
	static int quarksValid = 0;
	XrmQuark q;

	if (!quarksValid)
		{
		qWidth = XrmStringToQuark(XmNcolumnWidth);
		qSizePolicy = XrmStringToQuark(XmNcolumnSizePolicy);
		qUserData = XrmStringToQuark(XmNcolumnUserData);
		qResizable = XrmStringToQuark(XmNcolumnResizable);
		quarksValid = 1;
		}
	q = XrmStringToQuark(s);
	if (q == qWidth)
			*mask |= XmLGridColumnWidth;
	else if (q == qSizePolicy)
			*mask |= XmLGridColumnSizePolicy;
	else if (q == qUserData)
			*mask |= XmLGridColumnUserData;
	else if (q == qResizable)
			*mask |= XmLGridColumnResizable;
	}

static void
GetColumnValue(XmLGridWidget g,
	       XmLGridColumn col,
	       XtArgVal value,
	       long mask)
	{
	XmLGridClassPartOfWidget(g).getColumnValueProc(g, col, value, mask);
	}

/* Only to be called through Grid class */
static void 
_GetColumnValue(XmLGridWidget g,
		XmLGridColumn col,
		XtArgVal value,
		long mask)
	{
	switch (mask)
		{
		case XmLGridColumnWidth:
			*((Dimension *)value) = col->grid.width; 
			break;
		case XmLGridColumnSizePolicy:
			*((unsigned char *)value) = col->grid.sizePolicy; 
			break;
		case XmLGridColumnUserData:
			*((XtPointer *)value) = col->grid.userData;
			break;
		case XmLGridColumnResizable:
			*((Boolean *)value) = col->grid.resizable;
			break;

		}
	}

static int
SetColumnValues(XmLGridWidget g,
		XmLGridColumn col,
		long mask)
	{
	return XmLGridClassPartOfWidget(g).setColumnValuesProc(g, col, mask);
	}

/* Only to be called through Grid class */
static int 
_SetColumnValues(XmLGridWidget g, XmLGridColumn col, long mask)
	{
	int needsResize = 0, visible = 0;
	Boolean newIsHidden;

	if (mask & XmLGridColumnWidth || mask & XmLGridColumnSizePolicy)
		{
		visible = ColIsVisible(g, XmLGridColumnGetPos(col));
		XmLGridColumnWidthChanged(col);
		}
	if (mask & XmLGridColumnWidth)
		{
		if (g->grid.colWidth > 0)
			newIsHidden = False;
		else
			newIsHidden = True;
		if (XmLGridColumnIsHidden(col) != newIsHidden)
			{
			if (newIsHidden == True)
				g->grid.hiddenColCount++;
			else
				g->grid.hiddenRowCount--;
			VisPosChanged(g, 0);
			needsResize = 1;
			}
		if (visible && !g->grid.inResize)
			needsResize = 1;
		col->grid.width = g->grid.colWidth;
		}
	if (mask & XmLGridColumnSizePolicy)
		{
		col->grid.sizePolicy = g->grid.colSizePolicy;
		if (visible && !g->grid.inResize)
			needsResize = 1;
		}
	if (mask & XmLGridColumnUserData)
		col->grid.userData = g->grid.colUserData;
	if (mask & XmLGridColumnResizable)
		col->grid.resizable = g->grid.colResizable;
	return needsResize;
	}

/*
   Getting and Setting Cell Values
*/

static void
CellValueGetMask(char *s,
		 long *mask)
	{
	static XrmQuark qAlignment, qBackground, qBottomBorderColor;
	static XrmQuark qBottomBorderType, qColumnSpan;
	static XrmQuark qEditable, qFontList, qForeground;
	static XrmQuark qLeftBorderColor, qLeftBorderType;
	static XrmQuark qMarginBottom, qMarginLeft, qMarginRight;
	static XrmQuark qMarginTop, qPixmap, qPixmapMask;
	static XrmQuark qRightBorderColor, qRightBorderType;
	static XrmQuark qRowSpan, qString, qToggleSet;
	static XrmQuark qTopBorderColor, qTopBorderType, qType;
	static XrmQuark qUserData;
	static int quarksValid = 0;
	XrmQuark q;

	if (!quarksValid)
		{
		qAlignment = XrmStringToQuark(XmNcellAlignment);
		qBackground = XrmStringToQuark(XmNcellBackground);
		qBottomBorderColor = XrmStringToQuark(XmNcellBottomBorderColor);
		qBottomBorderType = XrmStringToQuark(XmNcellBottomBorderType);
		qColumnSpan = XrmStringToQuark(XmNcellColumnSpan);
		qEditable = XrmStringToQuark(XmNcellEditable);
		qFontList = XrmStringToQuark(XmNcellFontList);
		qForeground = XrmStringToQuark(XmNcellForeground);
		qLeftBorderColor = XrmStringToQuark(XmNcellLeftBorderColor);
		qLeftBorderType = XrmStringToQuark(XmNcellLeftBorderType);
		qMarginBottom = XrmStringToQuark(XmNcellMarginBottom);
		qMarginLeft = XrmStringToQuark(XmNcellMarginLeft);
		qMarginRight = XrmStringToQuark(XmNcellMarginRight);
		qMarginTop= XrmStringToQuark(XmNcellMarginTop);
		qPixmap = XrmStringToQuark(XmNcellPixmap);
		qPixmapMask = XrmStringToQuark(XmNcellPixmapMask);
		qRightBorderColor = XrmStringToQuark(XmNcellRightBorderColor);
		qRightBorderType = XrmStringToQuark(XmNcellRightBorderType);
		qRowSpan = XrmStringToQuark(XmNcellRowSpan);
		qString = XrmStringToQuark(XmNcellString);
		qToggleSet = XrmStringToQuark(XmNcellToggleSet);
		qTopBorderColor = XrmStringToQuark(XmNcellTopBorderColor);
		qTopBorderType = XrmStringToQuark(XmNcellTopBorderType);
		qType = XrmStringToQuark(XmNcellType);
		qUserData = XrmStringToQuark(XmNcellUserData);
		quarksValid = 1;
		}
	q = XrmStringToQuark(s);
	if (q == qAlignment)
			*mask |= XmLGridCellAlignment;
	else if (q == qBackground)
			*mask |= XmLGridCellBackground;
	else if (q == qBottomBorderColor)
			*mask |= XmLGridCellBottomBorderColor;
	else if (q == qBottomBorderType)
			*mask |= XmLGridCellBottomBorderType;
	else if (q == qColumnSpan)
			*mask |= XmLGridCellColumnSpan;
	else if (q == qEditable)
			*mask |= XmLGridCellEditable;
	else if (q == qFontList)
			*mask |= XmLGridCellFontList;
	else if (q == qForeground)
			*mask |= XmLGridCellForeground;
	else if (q == qLeftBorderColor)
			*mask |= XmLGridCellLeftBorderColor;
	else if (q == qLeftBorderType)
			*mask |= XmLGridCellLeftBorderType;
	else if (q == qMarginBottom)
			*mask |= XmLGridCellMarginBottom;
	else if (q == qMarginLeft)
			*mask |= XmLGridCellMarginLeft;
	else if (q == qMarginRight)
			*mask |= XmLGridCellMarginRight;
	else if (q == qMarginTop)
			*mask |= XmLGridCellMarginTop;
	else if (q == qPixmap)
			*mask |= XmLGridCellPixmapF;
	else if (q == qPixmapMask)
			*mask |= XmLGridCellPixmapMask;
	else if (q == qRightBorderColor)
			*mask |= XmLGridCellRightBorderColor;
	else if (q == qRightBorderType)
			*mask |= XmLGridCellRightBorderType;
	else if (q == qRowSpan)
			*mask |= XmLGridCellRowSpan;
	else if (q == qString)
			*mask |= XmLGridCellString;
	else if (q == qToggleSet)
			*mask |= XmLGridCellToggleSet;
	else if (q == qTopBorderColor)
			*mask |= XmLGridCellTopBorderColor;
	else if (q == qTopBorderType)
			*mask |= XmLGridCellTopBorderType;
	else if (q == qType)
			*mask |= XmLGridCellType;
	else if (q == qUserData)
			*mask |= XmLGridCellUserData;
	}

static void
GetCellValue(XmLGridCell cell,
	     XtArgVal value,
	     long mask)
	{
	XmLGridCellRefValues *values;
	XmLGridCellPixmap *pix;
	XmString str;

	values = XmLGridCellGetRefValues(cell);
	switch (mask)
		{
		case XmLGridCellAlignment:
			*((unsigned char *)value) = values->alignment; 
			break;
		case XmLGridCellBackground:
			*((Pixel *)value) = values->background; 
			break;
		case XmLGridCellBottomBorderColor:
			*((Pixel *)value) = values->bottomBorderColor;
			break;
		case XmLGridCellBottomBorderType:
			*((unsigned char *)value) = values->bottomBorderType;
			break;
		case XmLGridCellColumnSpan:
			*((int *)value) = values->columnSpan;
			break;
		case XmLGridCellEditable:
			*((Boolean *)value) = values->editable;
			break;
		case XmLGridCellFontList:
			*((XmFontList *)value) = values->fontList; 
			break;
		case XmLGridCellForeground:
			*((Pixel *)value) = values->foreground; 
			break;
		case XmLGridCellLeftBorderColor:
			*((Pixel *)value) = values->leftBorderColor;
			break;
		case XmLGridCellLeftBorderType:
			*((unsigned char *)value) = values->leftBorderType;
			break;
		case XmLGridCellMarginBottom:
			*((Dimension *)value) = values->bottomMargin;
			break;
		case XmLGridCellMarginLeft:
			*((Dimension *)value) = values->leftMargin;
			break;
		case XmLGridCellMarginRight:
			*((Dimension *)value) = values->rightMargin;
			break;
		case XmLGridCellMarginTop:
			*((Dimension *)value) = values->topMargin;
			break;
		case XmLGridCellPixmapF:
			pix = XmLGridCellGetPixmap(cell);
			if (pix)
				*((Pixmap *)value) = pix->pixmap;
			else
				*((Pixmap *)value) = (Pixmap)XmUNSPECIFIED_PIXMAP;
			break;
		case XmLGridCellPixmapMask:
			pix = XmLGridCellGetPixmap(cell);
			if (pix)
				*((Pixmap *)value) = pix->pixmask;
			else
				*((Pixmap *)value) = (Pixmap)XmUNSPECIFIED_PIXMAP;
			break;
		case XmLGridCellRightBorderColor:
			*((Pixel *)value) = values->rightBorderColor;
			break;
		case XmLGridCellRightBorderType:
			*((unsigned char *)value) = values->rightBorderType;
			break;
		case XmLGridCellRowSpan:
			*((int *)value) = values->rowSpan;
			break;
		case XmLGridCellString:
			str = XmLGridCellGetString(cell);
			if (str)
				*((XmString *)value) = XmStringCopy(str);
			else
				*((XmString *)value) = 0;
			break;
		case XmLGridCellToggleSet:
			*((Boolean *)value) = XmLGridCellGetToggle(cell);
			break;
		case XmLGridCellTopBorderColor:
			*((Pixel *)value) = values->topBorderColor;
			break;
		case XmLGridCellTopBorderType:
			*((unsigned char *)value) = values->topBorderType;
			break;
		case XmLGridCellType:
			*((unsigned char *)value) = values->type;
			break;
		case XmLGridCellUserData:
			*((XtPointer *)value) = (XtPointer)values->userData;
			break;
		}
	}

static XmLGridCellRefValues *
CellRefValuesCreate(XmLGridWidget g,
		    XmLGridCellRefValues *copy)
	{
	short width, height;
	XmLGridCellRefValues *values;

	values = (XmLGridCellRefValues *)malloc(sizeof(XmLGridCellRefValues));
	if (!copy)
		{
		/* default values */
		values->bottomBorderType = XmBORDER_LINE;
		values->leftBorderType = XmBORDER_LINE;
		values->rightBorderType = XmBORDER_LINE;
		values->topBorderType = XmBORDER_LINE;
		XmLFontListGetDimensions(g->grid.fontList, &width, &height,
			g->grid.useAvgWidth);
		values->alignment = XmALIGNMENT_CENTER;
		values->background = g->core.background_pixel;
		values->bottomBorderColor = g->manager.bottom_shadow_color;
		values->bottomMargin = 0;
		values->columnSpan = 0;
		values->editable = False;
		values->fontHeight = height;
		values->fontList = XmFontListCopy(g->grid.fontList);
		values->fontWidth = width;
		values->foreground = g->manager.foreground;
		values->leftBorderColor = g->manager.top_shadow_color;
		values->leftMargin = 0;
		values->refCount = 0;
		values->rightBorderColor = g->manager.bottom_shadow_color;
		values->rightMargin = 0;
		values->rowSpan = 0;
		values->topBorderColor = g->manager.top_shadow_color;
		values->topMargin = 0;
		values->type = XmSTRING_CELL;
		values->userData = 0;
		}
	else
		{
		/* copy values */
		*values = *copy;
		values->fontList = XmFontListCopy(copy->fontList);
		values->refCount = 0;
		}
	return values;
	}

static void
SetCellValuesPreprocess(XmLGridWidget g,
			long mask)
	{
	XmLGridCellRefValues *newValues;
	int x, y;
	short width, height;
	Display *dpy;
	Window pixRoot;
	unsigned int pixWidth, pixHeight;
	unsigned int pixBW, pixDepth;

	/* calculate font width and height if set */
	newValues = &g->grid.cellValues;
	if (mask & XmLGridCellFontList)
		{
		XmLFontListGetDimensions(newValues->fontList, &width, &height,
			g->grid.useAvgWidth);
		newValues->fontWidth = width;
		newValues->fontHeight = height;
		}
	if (mask & XmLGridCellPixmapF)
		{
		if (g->grid.cellPixmap != XmUNSPECIFIED_PIXMAP &&
			g->grid.globalPixmapWidth &&
			g->grid.globalPixmapHeight)
			{
			g->grid.cellPixmapWidth = g->grid.globalPixmapWidth;
			g->grid.cellPixmapHeight = g->grid.globalPixmapHeight;
			}
		else if (g->grid.cellPixmap != XmUNSPECIFIED_PIXMAP)
			{
			dpy = XtDisplay(g);
			XGetGeometry(dpy, g->grid.cellPixmap, &pixRoot,
				&x, &y, &pixWidth, &pixHeight, &pixBW, &pixDepth);
			g->grid.cellPixmapWidth = (Dimension)pixWidth;
			g->grid.cellPixmapHeight = (Dimension)pixHeight;
			}
		else
			{
			g->grid.cellPixmapWidth = 0;
			g->grid.cellPixmapHeight = 0;
			}
		}
	}

static int
SetCellHasRefValues(long mask)
	{
	long unrefMask;

	/* return 1 if mask contains any reference counted values */
	unrefMask = XmLGridCellPixmapF | XmLGridCellPixmapMask |
		XmLGridCellString | XmLGridCellToggleSet;
	mask = mask | unrefMask;
	mask = mask ^ unrefMask;
	if (!mask)
		return 0;
	return 1;
	}

static int
SetCellValuesResize(XmLGridWidget g,
		    XmLGridRow row,
		    XmLGridColumn col,
		    XmLGridCell cell,
		    long mask)
	{
	return XmLGridClassPartOfWidget(g).setCellValuesResizeProc(g, row, col,
		cell, mask);
	}

static int 
_SetCellValuesResize(XmLGridWidget g,
		     XmLGridRow row,
		     XmLGridColumn col,
		     XmLGridCell cell,
		     long mask)
	{
	XmLGridCellPixmap *cellPix;
	int pixResize, needsResize, rowVisible, colVisible;

	needsResize = 0;
	pixResize = 0;
	if (mask & XmLGridCellPixmapF)
		{
		pixResize = 1;
		if (!(mask & XmLGridCellType))
			{
			/* no resize needed if we replace with an equal size pixmap */
			cellPix = XmLGridCellGetPixmap(cell);
			if (cellPix && cellPix->pixmap != XmUNSPECIFIED_PIXMAP &&
				g->grid.cellPixmap != XmUNSPECIFIED_PIXMAP)
				{
				if (cellPix->width == g->grid.cellPixmapWidth &&
					cellPix->height == g->grid.cellPixmapHeight)
					pixResize = 0;
				}
			}
		}
	if (mask & XmLGridCellType || mask & XmLGridCellFontList || pixResize ||
		mask & XmLGridCellRowSpan || mask & XmLGridCellColumnSpan ||
		mask & XmLGridCellMarginLeft || mask & XmLGridCellMarginRight ||
		mask & XmLGridCellMarginTop || mask & XmLGridCellMarginBottom)
		{
		XmLGridRowHeightChanged(row);
		XmLGridColumnWidthChanged(col);
		rowVisible = RowIsVisible(g, XmLGridRowGetPos(row));
		colVisible = ColIsVisible(g, XmLGridColumnGetPos(col));
		if (rowVisible | colVisible)
			needsResize = 1;
		}
	return needsResize;
	}

static void 
SetCellValues(XmLGridWidget g,
	      XmLGridCell cell,
	      long mask)
	{
	/* set non-reference counted cell values */
	if (mask & XmLGridCellPixmapF)
		XmLGridCellSetPixmap(cell, g->grid.cellPixmap,
			g->grid.cellPixmapWidth, g->grid.cellPixmapHeight);
	if (mask & XmLGridCellPixmapMask)
		XmLGridCellSetPixmask(cell, g->grid.cellPixmapMask);
	if (mask & XmLGridCellString)
		XmLGridCellSetString(cell, g->grid.cellString, True);
	if (mask & XmLGridCellToggleSet)
		XmLGridCellSetToggle(cell, g->grid.cellToggleSet);
	}

static void
SetCellRefValues(XmLGridWidget g,
		 XmLGridCellRefValues *values,
		 long mask)
	{
	XmLGridCellRefValues *newValues;

	/* set reference counted cell values */
	newValues = &g->grid.cellValues;
	if (mask & XmLGridCellAlignment)
		values->alignment = newValues->alignment;
	if (mask & XmLGridCellBackground)
		values->background = newValues->background;
	if (mask & XmLGridCellBottomBorderColor)
		values->bottomBorderColor = newValues->bottomBorderColor;
	if (mask & XmLGridCellBottomBorderType)
		values->bottomBorderType = newValues->bottomBorderType;
	if (mask & XmLGridCellColumnSpan)
		values->columnSpan = newValues->columnSpan;
	if (mask & XmLGridCellEditable)
		values->editable = newValues->editable;
	if (mask & XmLGridCellFontList)
		{
		XmFontListFree(values->fontList);
		values->fontList = XmFontListCopy(newValues->fontList);
		values->fontWidth = newValues->fontWidth;
		values->fontHeight = newValues->fontHeight;
		}
	if (mask & XmLGridCellForeground)
		values->foreground = newValues->foreground;
	if (mask & XmLGridCellLeftBorderColor)
		values->leftBorderColor = newValues->leftBorderColor;
	if (mask & XmLGridCellLeftBorderType)
		values->leftBorderType = newValues->leftBorderType;
	if (mask & XmLGridCellRightBorderColor)
		values->rightBorderColor = newValues->rightBorderColor;
	if (mask & XmLGridCellRightBorderType)
		values->rightBorderType = newValues->rightBorderType;
	if (mask & XmLGridCellMarginBottom)
		values->bottomMargin = newValues->bottomMargin;
	if (mask & XmLGridCellMarginLeft)
		values->leftMargin = newValues->leftMargin;
	if (mask & XmLGridCellMarginRight)
		values->rightMargin = newValues->rightMargin;
	if (mask & XmLGridCellMarginTop)
		values->topMargin = newValues->topMargin;
	if (mask & XmLGridCellRowSpan)
		values->rowSpan = newValues->rowSpan;
	if (mask & XmLGridCellTopBorderColor)
		values->topBorderColor = newValues->topBorderColor;
	if (mask & XmLGridCellTopBorderType)
		values->topBorderType = newValues->topBorderType;
	if (mask & XmLGridCellType)
		{
		values->type = newValues->type;
		/* backwards compatibility cell types */
		if (values->type == XmLABEL_CELL)
			{
			values->type = XmSTRING_CELL;
			values->editable = False;
			}
		else if (values->type == XmTEXT_CELL)
			{
			values->type = XmSTRING_CELL;
			values->editable = True;
			}
		}
	if (mask & XmLGridCellUserData)
		values->userData = newValues->userData;
	}

static int
SetCellRefValuesCompare(void *userData,
			void **item1,
			void **item2)
	{
	XmLGridCell cell1, cell2;
	XmLGridCellRefValues *values1, *values2;
	long mask;

	mask = *((long *)userData);
	cell1 = (XmLGridCell)*item1;
	cell2 = (XmLGridCell)*item2;
	values1 = XmLGridCellGetRefValues(cell1);
	values2 = XmLGridCellGetRefValues(cell2);
	if (values1 == values2)
		return 0;

#define RVCOMPARE(res, var) \
	if (!(mask & res)) \
		{ \
		if (values1->var < values2->var) \
			return -1; \
		if (values1->var > values2->var) \
			return 1; \
		}
	RVCOMPARE(XmLGridCellAlignment, alignment)
	RVCOMPARE(XmLGridCellBackground, background)
	RVCOMPARE(XmLGridCellBottomBorderColor, bottomBorderColor)
	RVCOMPARE(XmLGridCellBottomBorderType, bottomBorderType)
	RVCOMPARE(XmLGridCellColumnSpan, columnSpan)
	RVCOMPARE(XmLGridCellEditable, editable)
	RVCOMPARE(XmLGridCellFontList, fontList)
	RVCOMPARE(XmLGridCellForeground, foreground)
	RVCOMPARE(XmLGridCellLeftBorderColor, leftBorderColor)
	RVCOMPARE(XmLGridCellLeftBorderType, leftBorderType)
	RVCOMPARE(XmLGridCellMarginBottom, bottomMargin)
	RVCOMPARE(XmLGridCellMarginLeft, leftMargin)
	RVCOMPARE(XmLGridCellMarginRight, rightMargin)
	RVCOMPARE(XmLGridCellMarginTop, topMargin)
	RVCOMPARE(XmLGridCellRightBorderColor, rightBorderColor)
	RVCOMPARE(XmLGridCellRightBorderType, rightBorderType)
	RVCOMPARE(XmLGridCellRowSpan, rowSpan)
	RVCOMPARE(XmLGridCellTopBorderColor, topBorderColor)
	RVCOMPARE(XmLGridCellTopBorderType, topBorderType)
	RVCOMPARE(XmLGridCellType, type)
	RVCOMPARE(XmLGridCellUserData, userData)
#undef RVCOMPARE

	/* If the two cell values are equal, we merge them
	   into one record here.  This speeds up the sort
  	   and will allow the merge to compare just the values
	   pointers to test equality. Note that this will not
	   merge every possible item that could be merged, we
	   don't want to do that because of the performance impact */
	if (values1 < values2)
		XmLGridCellSetRefValues(cell1, values2);
	else
		XmLGridCellSetRefValues(cell2, values1);
	return 0;
	}

static void
SetCellRefValuesPreprocess(XmLGridWidget g,
			   int row,
			   int col,
			   XmLGridCell cell,
			   long mask)
	{
	int r, c, rowSpan, colSpan;
	XmLGridCell spanCell;
	XmLGridCellRefValues *oldValues, *newValues;
	unsigned char oldType, newType;
	XmLGridCallbackStruct cbs;

	if (mask & XmLGridCellType)
		{
		oldType = XmLGridCellGetRefValues(cell)->type;
		newType = g->grid.cellValues.type;
		if (oldType != newType)
			{
			cbs.reason = XmCR_FREE_VALUE;
			XmLGridCellAction(cell, (Widget)g, &cbs);
			}
		}
	if (mask & XmLGridCellRowSpan || mask & XmLGridCellColumnSpan)
		{
		/* expose old cell area in case the span area shrinks */
		DrawArea(g, DrawCell, row, col);
		oldValues = XmLGridCellGetRefValues(cell);
		newValues = &g->grid.cellValues;
		if (mask & XmLGridCellRowSpan)
			{
			g->grid.mayHaveRowSpans = 1;
			if (newValues->rowSpan < 0)
				{
				XmLWarning((Widget)g,
					"SetValues() - row span can't be < 0");
				newValues->rowSpan = 0;
				}
			rowSpan = newValues->rowSpan;
			}
		else
			rowSpan = oldValues->rowSpan;
		if (mask & XmLGridCellColumnSpan)
			{
			if (newValues->columnSpan < 0)
				{
				XmLWarning((Widget)g,
					"SetValues() - column span can't be < 0");
				newValues->columnSpan = 0;
				}
			colSpan = newValues->columnSpan;
			}
		else
			colSpan = oldValues->columnSpan;
		/* clear old span */
		for (c = col; c <= col + oldValues->columnSpan; c++)
			for (r = row; r <= row + oldValues->rowSpan; r++)
				{
				/* skip the cell itself */
				if (c == col && r == row)
					continue;
				spanCell = GetCell(g, r, c);
				if (!spanCell)
					continue;
				XmLGridCellSetInRowSpan(spanCell, False);
				XmLGridCellSetInColumnSpan(spanCell, False);
				}
		/* set new span */
		for (c = col; c <= col + colSpan; c++)
			for (r = row; r <= row + rowSpan; r++)
				{
				/* skip the cell itself */
				if (c == col && r == row)
					continue;
				spanCell = GetCell(g, r, c);
				if (!spanCell)
					continue;
				if (r == row)
					XmLGridCellSetInColumnSpan(spanCell, True);
				else
					XmLGridCellSetInRowSpan(spanCell, True);
				}
		}
	}

/*
   Read, Write, Copy, Paste
*/

static int
Read(XmLGridWidget g,
     int format,
     char delimiter,
     int row,
     int col,
     char *data)
	{
	char *c1, *c2, buf[256], *bufp;
	int r, c, i, j, len, n, needsResize, allowSet, done;
	XmString str;
	XmLGridCell cell;
	XmLGridRow rowp;
	XmLGridColumn colp;
	XmLGridCellRefValues *cellValues;
	XmLGridCallbackStruct cbs;

	if (format == XmFORMAT_PAD)
		{
		XmLWarning((Widget)g, "Read() - FORMAT_PAD not supported");
		return 0;
		}
	if (format == XmFORMAT_XL ||
		format == XmFORMAT_DROP ||
		format == XmFORMAT_PASTE)
		delimiter = '\t';
	c1 = data;
	c2 = data;
	r = row;
	c = col;
	needsResize = 0;
	done = 0;
	n = 0;
	while (!done)
		{
		if (!(*c2) || *c2 == delimiter || *c2 == '\n')
			{
			len = c2 - c1;
			if (len < 256)
				bufp = buf;
			else
				bufp = (char *)malloc(len + 1);
			if (format == XmFORMAT_XL)
				{
				/* strip leading and trailing double-quotes */
				if (len && c1[0] == '"')
					{
					c1++;
					len--;
					}
				if (len && c1[len - 1] == '"')
					len--;
				}
			j = 0;
			for (i = 0; i < len; i++)
				{
				if (c1[0] == '\\' && c1[1] == 'n')
					{
					bufp[j++] = '\n';
					c1 += 2;
					i++;
					}
				else
					bufp[j++] = *c1++;
				}
			bufp[j] = 0;
			j = 0;
			str = XmStringCreateLtoR(bufp, XmSTRING_DEFAULT_CHARSET);
			if (bufp != buf)
				free((char *)bufp);
			rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
			colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
			cell = GetCell(g, r, c);
			allowSet = 1;
			if (cell && (format == XmFORMAT_PASTE || format == XmFORMAT_DROP))
				{
				cellValues = XmLGridCellGetRefValues(cell);
				if (cellValues->type != XmSTRING_CELL ||
					cellValues->editable != True ||
					RowPosToType(g, r) != XmCONTENT ||
					ColPosToType(g, c) != XmCONTENT)
					allowSet = 0;
				}
			if (cell && allowSet)
				{
				XmLGridCellSetString(cell, str, False);
				if (SetCellValuesResize(g, rowp, colp, cell, XmLGridCellString))
					needsResize = 1;
				if (!needsResize)
					DrawArea(g, DrawCell, r, c);
				cbs.columnType = ColPosToType(g, c);
				cbs.column = ColPosToTypePos(g, cbs.columnType, c);
				cbs.rowType = RowPosToType(g, r);
				cbs.row = RowPosToTypePos(g, cbs.rowType, r);
				if (format == XmFORMAT_PASTE)
					{
					cbs.reason = XmCR_CELL_PASTE;
					XtCallCallbackList((Widget)g, g->grid.cellPasteCallback,
						(XtPointer)&cbs);
					}
				else if (format == XmFORMAT_DROP)
					{
					cbs.reason = XmCR_CELL_DROP;
					XtCallCallbackList((Widget)g, g->grid.cellDropCallback,
						(XtPointer)&cbs);
					}
				n++;
				}
			else
				XmStringFree(str);
			}
		if (!(*c2))
			done = 1;
		else if (*c2 == delimiter)
			{
			c++;
			c1 = c2 + 1;
			}
		else if (*c2 == '\n')
			{
			r++;
			c = col;
			c1 = c2 + 1;
			}
		c2++;
		}
	if (needsResize)
		{
		VertLayout(g, 1);
		HorizLayout(g, 1);
		DrawArea(g, DrawAll, 0, 0);
		}
	return n;
	}

static void
Write(XmLGridWidget g,
      FILE *file,
      int format,
      char delimiter,
      Boolean skipHidden,
      int row,
      int col,
      int nrow,
      int ncol)
	{
	int r, c, i, first, last;
	char *cs = NULL;
	Boolean set;
	XmString str;
	XmLGridColumn colp;
	XmLGridRow rowp;
	XmLGridCell cell;

	first = 1;
	for (r = row; r < row + nrow; r++)
		{
		rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
		if (!rowp)
			continue;
		if (skipHidden == True && XmLGridRowIsHidden(rowp) == True)
			continue;
		if (first)
			first = 0;
		else
			fprintf(file, "\n");
		for (c = col; c < col + ncol; c++)
			{
			colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
			if (!colp)
				continue;
			if (skipHidden == True && XmLGridColumnIsHidden(colp) == True)
				continue;
			cell = GetCell(g, r, c);
			if (!cell)
				continue;
			str = XmLGridCellGetString(cell);
			set = False;
			if (str)
				{
				cs = CvtXmStringToStr(str);
				if (cs)
					set = True;
				}
			if (set == False)
				cs = "";
			fprintf(file, "%s", cs);

			last = 0;
			if (c == col + ncol - 1)
				last = 1;
			if (!last && format == XmFORMAT_DELIMITED)
				fprintf(file, "%c", delimiter);
			else if (!last && format == XmFORMAT_XL)
				fprintf(file, "\t");
			else if (format == XmFORMAT_PAD)
				{
				if (colp->grid.sizePolicy == XmVARIABLE)
					for (i = 0; i < (int)(colp->grid.width - strlen(cs)); i++)
						fprintf(file, " ");
				}

			if (set == True)
				free(cs);
			}
		}
	}

static char *
CopyDataCreate(XmLGridWidget g, int selected, int row, int col, int nrow, int ncol)
	{
	XmLGridColumn colp;
	XmLGridRow rowp;
	XmLGridCell cell;
	char *buf, *cs = NULL;
	XmString str;
	Boolean set;
	int r, c, wroteStr, bufsize, buflen, len;

	if (selected)
		{
		row = 0;
		nrow = XmLArrayGetCount(g->grid.rowArray);
		col = 0;
		ncol = XmLArrayGetCount(g->grid.colArray);
		}
	bufsize = 1024;
	buflen = 0;
	buf = (char *)malloc(bufsize);

	for (r = row; r < row + nrow; r++)
		{
		wroteStr = 0;
		rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
		if (!rowp)
			continue;
		for (c = col; c < col + ncol; c++)
			{
			colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
			if (!colp)
				continue;
			cell = GetCell(g, r, c);
			if (!cell)
				continue;
			if (selected &&
				XmLGridRowIsSelected(rowp) == False &&
				XmLGridColumnIsSelected(colp) == False &&
				XmLGridCellIsSelected(cell) == False)
				continue;
			str = XmLGridCellGetString(cell);
			set = False;
			if (str)
				{
				cs = CvtXmStringToStr(str);
				if (cs)
					set = True;
				}
			if (set == False)
				cs = "";
			if (wroteStr)
				buf[buflen++] = '\t';

			len = strlen(cs);
			/* allocate if string plus tab plus new-line plus 0 if too large */
			while (len + buflen + 3 > bufsize)
				bufsize *= 2;
			buf = (char *)realloc(buf, bufsize);
			strcpy(&buf[buflen], cs);
			buflen += len;
			if (set == True)
				free(cs);
			wroteStr = 1;
			}
		if (wroteStr)
			buf[buflen++] = '\n';
		}
	if (!buflen)
		{
		free((char *)buf);
		return 0;
		}
	buf[buflen - 1] = 0;
	return buf;
	}

static Boolean
Copy(XmLGridWidget g,
     Time time,
     int selected,
     int row,
     int col,
     int nrow,
     int ncol)
	{
	int i;
	long itemID;
	Display *dpy;
	Window win;
	XmString clipStr;
	char *buf;
#ifdef MOTIF11
	int dataID;
#else
	long ldataID;
#endif

	if (!XtIsRealized((Widget)g))
		{
		XmLWarning((Widget)g, "Copy() - widget not realized");
		return False;
		}
	dpy = XtDisplay((Widget)g);
	win = XtWindow((Widget)g);
	buf = CopyDataCreate(g, selected, row, col, nrow, ncol);
	if (!buf)
		return False;
	clipStr = XmStringCreateSimple("Grid Copy");
	for (i = 0; i < 10000; i++)
		if (XmClipboardStartCopy(dpy, win, clipStr, time, NULL,
			NULL, &itemID) == ClipboardSuccess)
			break;
	XmStringFree(clipStr);
	if (i == 10000)
		{
		XmLWarning((Widget)g, "Copy() - start clipboard copy failed");
		return False;
		}
	for (i = 0; i < 10000; i++)
#ifdef MOTIF11
		if (XmClipboardCopy(dpy, win, itemID, "STRING", buf,
			(long)strlen(buf) + 1, 0, &dataID) == ClipboardSuccess)
#else
		if (XmClipboardCopy(dpy, win, itemID, "STRING", buf,
			(long)strlen(buf) + 1, 0, &ldataID) == ClipboardSuccess)
#endif
			break;
	free((char *)buf);
	if (i == 10000)
		{
		XmLWarning((Widget)g, "Copy() - clipboard copy transfer failed");
		return False;
		}
	for (i = 0; i < 10000; i++)
		if (XmClipboardEndCopy(dpy, win, itemID) == ClipboardSuccess)
			break;
	if (i == 10000)
		{
		XmLWarning((Widget)g, "Copy() - end clipboard copy failed");
		return False;
		}
	return True;
	}

static Boolean
Paste(XmLGridWidget g,
      int row,
      int col)
	{
	Display *dpy;
	Window win;
	int i, res, done;
	unsigned long len, reclen;
	char *buf;

	if (!XtIsRealized((Widget)g))
		{
		XmLWarning((Widget)g, "Paste() - widget not realized");
		return False;
		}
	dpy = XtDisplay((Widget)g);
	win = XtWindow((Widget)g);
	for (i = 0; i < 10000; i++)
		if (XmClipboardInquireLength(dpy, win, "STRING", &len) ==
			ClipboardSuccess)
			break;
	if (i == 10000)
		{
		XmLWarning((Widget)g, "Paste() - can't retrieve clipboard length");
		return False;
		}
	if (!len)
		return False;
	buf = (char *)malloc((int)len);
	done = 0;
	while (!done)
		{
		res = XmClipboardRetrieve(dpy, win, "STRING", buf, len,
			&reclen, NULL);
		switch (res)
			{
			case ClipboardSuccess:
				done = 2;
				break;
			case ClipboardTruncate:
			case ClipboardNoData:
				done = 1;
				break;
			case ClipboardLocked:
				break;
			}
		}
	if (done != 2 || reclen != len)
		{
		free((char *)buf);
		XmLWarning((Widget)g, "Paste() - retrieve from clipboard failed");
		return False;
		}
	Read(g, XmFORMAT_PASTE, 0, row, col, buf);
	free((char *)buf);
	return True;
	}

/*
   Utility
*/

static void
GetCoreBackground(Widget w,
		  int offset,
		  XrmValue *value)
	{
	value->addr = (caddr_t)&w->core.background_pixel;
	}

static void
GetManagerForeground(Widget w,
		     int offset,
		     XrmValue *value)
	{
	XmLGridWidget g;

	g = (XmLGridWidget)w;
	value->addr = (caddr_t)&g->manager.foreground;
	}

static void
ClipRectToReg(XmLGridWidget g,
	      XRectangle *rect,
	      GridReg *reg)
	{
	int i, st;
	XRectangle regRect;

	st = g->manager.shadow_thickness;
	if (!reg->width || !reg->height)
		i = XmLRectOutside;
	else 
		{
		regRect.x = reg->x + st;
		regRect.y = reg->y + st;
		regRect.width = reg->width - st * 2;
		regRect.height = reg->height - st * 2;
		i = XmLRectIntersect(rect, &regRect);
		}
	if (i == XmLRectInside)
		return;
	if (i == XmLRectOutside)
		{
		rect->width = 0;
		rect->height = 0;
		return;
		}
	if (rect->y + (int)rect->height - 1 >= reg->y + (int)reg->height - st)
		rect->height = reg->y + reg->height - rect->y - st;
	if (rect->x + (int)rect->width - 1 >= reg->x + (int)reg->width - st)
		rect->width = reg->x + reg->width - rect->x - st;
	if (rect->y < reg->y + st)
		{
		rect->height -= (reg->y + st) - rect->y;
		rect->y = reg->y + st;
		}
	if (rect->x < reg->x + st)
		{
		rect->width -= (reg->x + st) - rect->x;
		rect->x = reg->x + st;
		}
	}

static char *
FileToString(FILE *file)
	{
	long len, n;
	char *s;

	if (!file)
		return 0;
	fseek(file, 0L, 2);
	len = ftell(file);
	s = (char *)malloc((int)len + 1);
	if (!s)
		return 0;
	s[len] = 0;
	fseek(file, 0L, 0);
	n = fread(s, 1, (int)len, file);
	if (n != len)
		{
		free((char *)s);
		return 0;
		}
	return s;
	}

static char *
CvtXmStringToStr(XmString str)
	{
	XmStringContext context;
	XmStringCharSet charset;
	XmStringDirection dir;
	Boolean sep;
	char *text, *c;
	int len, size;

	if (!XmStringInitContext(&context, str))
		return 0;
	size = 0;
	c = 0;
	while (XmStringGetNextSegment(context, &text, &charset, &dir, &sep))
		{
		len = strlen(text);
		size += len + 3;
		if (!c)
			{
			c = (char *)malloc(size);
			*c = 0;
			}
		else
			c = (char *)realloc(c, size);
		strcat(c, text);
		if (sep == True)
			{
			len = strlen(c);
			c[len] = '\\';
			c[len + 1] = 'n';
			c[len + 2] = 0;
			}
		XtFree(text);
		XtFree(charset);
		}
	XmStringFreeContext(context);
	return c;
	}

static XmLGridWidget 
WidgetToGrid(Widget w,
	     char *funcname)
	{
	char buf[256];

	if (!XmLIsGrid(w))
		{
		sprintf(buf, "%s - widget not an XmLGrid", funcname);
		XmLWarning(w, buf);
		return 0;
		}
	return (XmLGridWidget)w;
	}

/*
   Actions, Callbacks and Handlers
*/

static void
ButtonMotion(Widget w,
	     XEvent *event,
	     String *params,
	     Cardinal *nparam)
	{
	XmLGridWidget g;
	XMotionEvent *me;
	char dragTimerSet;
	int row, col, x, y;

	if (event->type != MotionNotify)
		return;
	g = (XmLGridWidget)w;
	me = (XMotionEvent *)event;
	if (g->grid.inMode == InResize)
		{
		if (g->grid.resizeIsVert)
			DrawResizeLine(g, me->y, 0);
		else
			DrawResizeLine(g, me->x, 0);
		}

	/* drag scrolling */
	dragTimerSet = 0;
	if (g->grid.inMode == InSelect)
		{
		if (g->grid.vsPolicy == XmCONSTANT)
			{
			y = g->grid.reg[4].y;
			if (g->grid.selectionPolicy == XmSELECT_CELL &&
				g->grid.extendRow != -1 &&
				g->grid.extendCol != -1 &&
				RowPosToType(g, g->grid.extendRow) == XmHEADING)
				;
			else if (me->y < y)
				dragTimerSet |= DragUp;
			y += g->grid.reg[4].height;
			if (me->y > y)
				dragTimerSet |= DragDown;
			}
		if (g->grid.hsPolicy == XmCONSTANT)
			{
			x = g->grid.reg[4].x;
			if (g->grid.selectionPolicy == XmSELECT_CELL &&
				g->grid.extendCol != -1 &&
				g->grid.extendRow != -1 &&
				ColPosToType(g, g->grid.extendCol) == XmHEADING)
				;
			else if (me->x < x)
				dragTimerSet |= DragLeft;
			x += g->grid.reg[4].width;
			if (me->x > x)
				dragTimerSet |= DragRight;
			}
		}
	if (!g->grid.dragTimerSet && dragTimerSet)
		g->grid.dragTimerId = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
			80, DragTimer, (caddr_t)g);
	else if (g->grid.dragTimerSet && !dragTimerSet)
		XtRemoveTimeOut(g->grid.dragTimerId);
	g->grid.dragTimerSet = dragTimerSet;

	/* Extend Selection */
	if (g->grid.inMode == InSelect && XYToRowCol(g, me->x, me->y,
		&row, &col) != -1)
		{
		if (g->grid.selectionPolicy == XmSELECT_MULTIPLE_ROW &&
			RowPosToType(g, row) == XmCONTENT)
			ExtendSelect(g, event, False, row, col);
		else if (g->grid.selectionPolicy == XmSELECT_CELL)
			ExtendSelect(g, event, False, row, col);
		}

	if (g->grid.inMode == InSelect &&
		g->grid.selectionPolicy == XmSELECT_BROWSE_ROW &&
		XYToRowCol(g, me->x, me->y, &row, &col) != -1)
		{
		if (RowPosToType(g, row) == XmCONTENT)
			{
			if (!SetFocus(g, row, col, 0, 1))
				SelectTypeArea(g, SelectRow, event,
					RowPosToTypePos(g, XmCONTENT,
					g->grid.focusRow), 0, True, True);
			}
		}
	}

static void
DragTimer(XtPointer clientData,
	  XtIntervalId *intervalId)
	{
	Widget w;
	XmLGridWidget g;
	XRectangle rect;
	XmLGridRow rowp;
	XmLGridColumn colp;
	int r, c, min, max, inc, pi, ss, value, newValue;
	int extRow, extCol;

	g = (XmLGridWidget)clientData;
	w = (Widget)g;
	extRow = -1;
	extCol = -1;
	if (g->grid.vsPolicy == XmCONSTANT && ((g->grid.dragTimerSet & DragUp) ||
		(g->grid.dragTimerSet & DragDown)))
		{
		XtVaGetValues(g->grid.vsb,
			XmNminimum, &min,
			XmNmaximum, &max,
			XmNvalue, &value,
			XmNsliderSize, &ss,
			XmNincrement, &inc,
			XmNpageIncrement, &pi,
			NULL);
		newValue = value;
		if (g->grid.dragTimerSet & DragUp)
			newValue--;
		else
			newValue++;
		if (newValue != value && newValue >= min && newValue <= (max - ss))
			{
			XmScrollBarSetValues(g->grid.vsb, newValue, ss, inc, pi, True);
			r = g->grid.reg[4].row;
			if (g->grid.dragTimerSet & DragDown)
				r += g->grid.reg[4].nrow - 1;
			/* simple check to make sure row selected is totally visible */
			if (g->grid.reg[4].nrow)
				{
				rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, r);
				if (rowp && !RowColToXY(g, r, 0, True, &rect))
					{
					if (GetRowHeight(g, r) != rect.height)
						r--;
					}
				}
			if (g->grid.selectionPolicy == XmSELECT_BROWSE_ROW)
				{
				if (!SetFocus(g, r, g->grid.focusCol, -1, 1))
					SelectTypeArea(g, SelectRow, (XEvent *)0,
						RowPosToTypePos(g, XmCONTENT, g->grid.focusRow),
						0, True, True);
				}
			else if (g->grid.selectionPolicy == XmSELECT_MULTIPLE_ROW)
				ExtendSelect(g, (XEvent *)0, False, r, g->grid.focusCol);
			else if (g->grid.selectionPolicy == XmSELECT_CELL)
				{
				extRow = r;
				extCol = g->grid.extendToCol;
				}
			}
		}
	if (g->grid.hsPolicy == XmCONSTANT && ((g->grid.dragTimerSet & DragLeft) ||
		(g->grid.dragTimerSet & DragRight)))
		{
		XtVaGetValues(g->grid.hsb,
			XmNminimum, &min,
			XmNmaximum, &max,
			XmNvalue, &value,
			XmNsliderSize, &ss,
			XmNincrement, &inc,
			XmNpageIncrement, &pi,
			NULL);
		newValue = value;
		if (g->grid.dragTimerSet & DragLeft)
			newValue--;
		else
			newValue++;
		if (newValue != value && newValue >= min && newValue <= (max - ss))
			{
			XmScrollBarSetValues(g->grid.hsb, newValue, ss, inc, pi, True);
			c = g->grid.reg[4].col;
			if (g->grid.dragTimerSet & DragRight)
				c += g->grid.reg[4].ncol - 1;
			if (g->grid.selectionPolicy == XmSELECT_CELL)
				{
				/* simple check to make sure col selected is totally visible */
				if (g->grid.reg[4].ncol)
					{
					colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, c);
					if (colp && !RowColToXY(g, c, 0, True, &rect))
						{
						if (GetColWidth(g, c) != rect.width)
							c--;
						}
					}
				if (extRow == -1)
					extRow = g->grid.extendToRow;
				extCol = c;
				}
			}
		}
	if (extRow != -1 && extCol != -1)
		ExtendSelect(g, (XEvent *)0, False, extRow, extCol);
	g->grid.dragTimerId = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
		80, DragTimer, (caddr_t)g);
	}

static void
CursorMotion(Widget w,
	     XEvent *event,
	     String *params,
	     Cardinal *nparam)
	{
	XmLGridWidget g;
	XMotionEvent *me;
	int isVert, row, col;
	char defineCursor;

	if (event->type != MotionNotify)
		return;
	g = (XmLGridWidget)w;
	me = (XMotionEvent *)event;
	defineCursor = CursorNormal;
	if (PosIsResize(g, me->x, me->y, &row, &col, &isVert))
		{
		if (isVert)
			defineCursor = CursorVResize;
		else
			defineCursor = CursorHResize;
		}
	DefineCursor(g, defineCursor);
	}

static void
Edit(Widget w,
     XEvent *event,
     String *params,
     Cardinal *nparam)
	{
	XmLGridWidget g;

	g = (XmLGridWidget)XtParent(w);
	TextAction(g, TEXT_EDIT_INSERT);
	}

static void
EditCancel(Widget w,
	   XEvent *event,
	   String *params,
	   Cardinal *nparam)
	{
	XmLGridWidget g;

	g = (XmLGridWidget)XtParent(w);
	TextAction(g, TEXT_EDIT_CANCEL);
	}

static void
EditComplete(Widget w,
	     XEvent *event,
	     String *params,
	     Cardinal *nparam)
	{
	XmLGridWidget g;

	g = (XmLGridWidget)XtParent(w);
	TextAction(g, TEXT_EDIT_COMPLETE);
	if (*nparam == 1)
		Traverse(w, event, params, nparam);
	}

#ifndef MOTIF11
extern Widget _XmGetTextualDragIcon(Widget);
#endif

static void
DragStart(Widget w,
	  XEvent *event,
	  String *params,
	  Cardinal *nparam)
	{
#ifndef MOTIF11
	XmLGridWidget g;
	Widget dragIcon;
	Atom exportTargets[1];
	Arg args[10];
	char *data;
	XButtonEvent *be;
	int row, col;
	XmLGridColumn colp;
	XmLGridRow rowp;
	XmLGridCell cell;
	static XtCallbackRec dragFinish[2] =
		{ { DragFinish, NULL }, { NULL, NULL } };

	g = (XmLGridWidget)w;
	be = (XButtonEvent *)event;
	if (!g->grid.allowDrag || !be)
		return;
	if (XYToRowCol(g, be->x, be->y, &row, &col) == -1)
		return;
	rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
	colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, col);
	cell = GetCell(g, row, col);
	if (!rowp || !colp || !cell)
		return;
	if (XmLGridRowIsSelected(rowp) == False &&
		XmLGridColumnIsSelected(colp) == False &&
		XmLGridCellIsSelected(cell) == False)
		return;
	data = CopyDataCreate(g, 1, 0, 0, 0, 0);
	if (!data)
		return;
	dragIcon = _XmGetTextualDragIcon((Widget)w);
	exportTargets[0] = XA_STRING;
	dragFinish[0].closure = (XtPointer)data;
	XtSetArg(args[0], XmNconvertProc, DragConvert);
	XtSetArg(args[1], XmNexportTargets, exportTargets);
	XtSetArg(args[2], XmNnumExportTargets, 1);
	XtSetArg(args[3], XmNdragOperations, XmDROP_COPY);
	XtSetArg(args[4], XmNsourceCursorIcon, dragIcon);
	XtSetArg(args[4], XmNclientData, (XtPointer)data);
	XtSetArg(args[5], XmNdragDropFinishCallback, dragFinish);
	XmDragStart(w, event, args, 6);
#endif
	}

static Boolean
DragConvert(Widget w,
	    Atom *selection,
	    Atom *target,
	    Atom *type,
	    XtPointer *value,
	    unsigned long *length,
	    int *format)
	{
#ifdef MOTIF11
	return FALSE;
#else
	Atom targetsA, timestampA, multipleA, *exportTargets;
	int n;
	char *data, *dataCopy;

	if (!XmIsDragContext(w))
		return FALSE;
	targetsA = XInternAtom(XtDisplay(w), "TARGETS", FALSE);
	timestampA = XInternAtom(XtDisplay(w), "TIMESTAMP", FALSE);
	multipleA = XInternAtom(XtDisplay(w), "MULTIPLE", FALSE);
	if (*target == targetsA)
		{
		n = 4;
		exportTargets = (Atom *)XtMalloc(sizeof(Atom) * n);
		exportTargets[0] = XA_STRING;
		exportTargets[1] = targetsA;
		exportTargets[2] = multipleA;
		exportTargets[3] = timestampA;
		*type = XA_ATOM;
		*value = (XtPointer)exportTargets;
		*format = 32;
		*length = (n * sizeof(Atom)) >> 2;
		return TRUE;
		}
	else if (*target == XA_STRING)
		{
		XtVaGetValues(w, XmNclientData, &data, NULL);
  		*type = XA_STRING;
		dataCopy = XtMalloc(strlen(data));
		strncpy(dataCopy, data, strlen(data));
  		*value = (XtPointer)dataCopy;
  		*length = strlen(data);
  		*format = 8;
  		return TRUE;
		}
	return FALSE;
#endif
	}

static void
DragFinish(Widget w,
	   XtPointer clientData,
	   XtPointer callData)
	{
	free ((char *)clientData);
	}

static void
DropRegister(XmLGridWidget g, Boolean set)
	{
#ifndef MOTIF11
	Atom importTargets[1];
	Arg args[4];

	if (set == True)
		{
		importTargets[0] = XA_STRING;
		XtSetArg(args[0], XmNdropSiteOperations, XmDROP_COPY);
		XtSetArg(args[1], XmNimportTargets, importTargets);
		XtSetArg(args[2], XmNnumImportTargets, 1);
		XtSetArg(args[3], XmNdropProc, DropStart);
		XmDropSiteRegister((Widget)g, args, 4);
		}
	else
		XmDropSiteUnregister((Widget)g);
#endif
	}

static void
DropStart(Widget w,
	  XtPointer clientData,
	  XtPointer callData)
	{
#ifndef MOTIF11
	XmLGridWidget g;
	XmDropProcCallbackStruct *cbs;
	XmDropTransferEntryRec te[2];
	Atom *exportTargets;
	Arg args[10];
	int row, col, i, n, valid;

	g = (XmLGridWidget)w;
	cbs = (XmDropProcCallbackStruct *)callData;
	if (g->grid.allowDrop == False || cbs->dropAction == XmDROP_HELP)
		{
		cbs->dropSiteStatus = XmINVALID_DROP_SITE;
		return;
		}
	valid = 0;
	if (XYToRowCol(g, cbs->x, cbs->y, &row, &col) != -1 &&
		cbs->dropAction == XmDROP && cbs->operation == XmDROP_COPY)
		{
		XtVaGetValues(cbs->dragContext,
			XmNexportTargets, &exportTargets,
			XmNnumExportTargets, &n,
			NULL);
		for (i = 0; i < n; i++)
			if (exportTargets[i] == XA_STRING)
				valid = 1;
		}
	if (!valid)
		{
		cbs->operation = (long)XmDROP_NOOP;
		cbs->dropSiteStatus = XmINVALID_DROP_SITE;
		XtSetArg(args[0], XmNtransferStatus, XmTRANSFER_FAILURE);
		XtSetArg(args[1], XmNnumDropTransfers, 0);
		XmDropTransferStart(cbs->dragContext, args, 2);
		return;
		}
	g->grid.dropLoc.row = row;
	g->grid.dropLoc.col = col;
	cbs->operation = (long)XmDROP_COPY;
	te[0].target = XA_STRING;
	te[0].client_data = (XtPointer)g;
	XtSetArg(args[0], XmNdropTransfers, te);
	XtSetArg(args[1], XmNnumDropTransfers, 1);
	XtSetArg(args[2], XmNtransferProc, DropTransfer);
	XmDropTransferStart(cbs->dragContext, args, 3);
#endif
	}

static void
DropTransfer(Widget w,
	     XtPointer clientData,
	     Atom *selType,
	     Atom *type,
	     XtPointer value,
	     unsigned long *length,
	     int *format)
	{
#ifndef MOTIF11
	XmLGridWidget g;
	char *buf;
	int len;

	if (!value)
		return;
	g = (XmLGridWidget)clientData;
	len = (int)*length;
	if (len < 0)
		return;
	buf = (char *)malloc(len + 1);
	strncpy(buf, (char *)value, len);
	XtFree((char *)value);
	buf[len] = 0;
	Read(g, XmFORMAT_DROP, 0, g->grid.dropLoc.row, g->grid.dropLoc.col, buf);
	free((char *)buf);
#endif
	}

static void
Select(Widget w,
       XEvent *event,
       String *params,
       Cardinal *nparam)
	{
	XmLGridWidget g;
	Display *dpy;
	Window win;
	static XrmQuark qACTIVATE, qBEGIN, qEXTEND, qEND;
	static XrmQuark qTOGGLE;
	static int quarksValid = 0;
	XrmQuark q;
	int isVert;
	int row, col, clickTime, resizeRow, resizeCol;
	XButtonEvent *be;
	XRectangle rect;
	XmLGridRow rowp;
	XmLGridCell cellp;
	XmLGridColumn colp;
	XmLGridCallbackStruct cbs;
	Boolean flag;

	if (*nparam != 1)
		return;

	if (XmLIsGrid(w))
		g = (XmLGridWidget)w;
	else
		g = (XmLGridWidget)XtParent(w);
	dpy = XtDisplay(g);
	win = XtWindow(g);
	if (!quarksValid)
		{
		qACTIVATE = XrmStringToQuark("ACTIVATE");
		qBEGIN = XrmStringToQuark("BEGIN");
		qEXTEND = XrmStringToQuark("EXTEND");
		qEND = XrmStringToQuark("END");
		qTOGGLE = XrmStringToQuark("TOGGLE");
		}
	q = XrmStringToQuark(params[0]);
	be = 0;
	if (event->type == KeyPress || event->type == KeyRelease)
		{
		row = g->grid.focusRow;
		col = g->grid.focusCol;
		}
	else /* Button */
		{
		be = (XButtonEvent *)event;
		if (XYToRowCol(g, be->x, be->y, &row, &col) == -1)
			{
			row = -1;
			col = -1;
			}
		}
	/* double click activate check */
	if (q == qBEGIN && be)
		{
		clickTime = XtGetMultiClickTime(dpy);
		if (row != -1 && col != -1 &&
			row == g->grid.lastSelectRow && col == g->grid.lastSelectCol &&
			(be->time - g->grid.lastSelectTime) < clickTime)
			q = qACTIVATE;
		g->grid.lastSelectRow = row;
		g->grid.lastSelectCol = col;
		g->grid.lastSelectTime = be->time;
		}
	else if (q == qBEGIN)
		g->grid.lastSelectTime = 0;

	if (q == qBEGIN && be && PosIsResize(g, be->x, be->y,
		&resizeRow, &resizeCol, &isVert))
		{
		g->grid.resizeIsVert = isVert;
		g->grid.inMode = InResize;
		g->grid.resizeLineXY = -1; 
		g->grid.resizeRow = resizeRow;
		g->grid.resizeCol = resizeCol;
		if (isVert)
			{
			DrawResizeLine(g, be->y, 0);
			DefineCursor(g, CursorVResize);
			}
		else
			{
			DrawResizeLine(g, be->x, 0);
			DefineCursor(g, CursorHResize);
			}
		}
	else if (q == qBEGIN || q == qEXTEND || q == qTOGGLE)
		{
		if (g->grid.inMode != InNormal)
			return;
		if (row == -1 || col == -1)
			return;
		if (RowPosToType(g, row) == XmCONTENT &&
			ColPosToType(g, col) == XmCONTENT)
			{
			TextAction(g, TEXT_EDIT_COMPLETE);
			if (q != qEXTEND)
				{
				SetFocus(g, row, col, 0, 1);
				ExtendSelect(g, event, False, -1, -1);
				}
			XmProcessTraversal(g->grid.text, XmTRAVERSE_CURRENT);
			}
		if (g->grid.selectionPolicy == XmSELECT_MULTIPLE_ROW &&
			RowPosToType(g, row) == XmCONTENT)
			{
			flag = True;
			rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
			if (q == qBEGIN && rowp && XmLGridRowIsSelected(rowp) == True)
				flag = False;
			if (q == qTOGGLE && rowp && XmLGridRowIsSelected(rowp) == True)
				flag = False;
			if (q == qBEGIN)
				SelectTypeArea(g, SelectRow, event, -1, 0, False, True);
			if (be && q == qEXTEND)
				ExtendSelect(g, event, False, row, col);
			else
				SelectTypeArea(g, SelectRow, event,
					RowPosToTypePos(g, XmCONTENT, row), 0, flag, True);
			}
		if (g->grid.selectionPolicy == XmSELECT_CELL)
			{
			if (q == qBEGIN)
				{
				SelectTypeArea(g, SelectCell, event, -1, -1, False, True);
				SelectTypeArea(g, SelectRow, event, -1, 0, False, True);
				SelectTypeArea(g, SelectCol, event, 0, -1, False, True);
				}
			else if (q == qTOGGLE)
				ExtendSelect(g, event, False, -1, -1);
			if (RowPosToType(g, row) == XmFOOTER ||
				ColPosToType(g, col) == XmFOOTER)
				ExtendSelect(g, event, False, -1, -1);
			if (be && q == qEXTEND)
				ExtendSelect(g, event, False, row, col);
			else if (RowPosToType(g, row) == XmCONTENT &&
				ColPosToType(g, col) == XmCONTENT)
				{
				flag = True;
				cellp = GetCell(g, row, col);
				if (q == qTOGGLE && cellp &&
					XmLGridCellIsSelected(cellp) == True)
					flag = False;
				SelectTypeArea(g, SelectCell, event,
					RowPosToTypePos(g, XmCONTENT, row),
					ColPosToTypePos(g, XmCONTENT, col), flag, True);
				}
			else if (RowPosToType(g, row) == XmHEADING &&
				ColPosToType(g, col) == XmCONTENT)
				{
				if (q == qTOGGLE)
					{
					colp = (XmLGridColumn)XmLArrayGet(g->grid.colArray, col);
					if (colp && XmLGridColumnIsSelected(colp) == True)
						g->grid.extendSelect = False;
					}
				ExtendSelect(g, event, True, row, col);
				}
			else if (ColPosToType(g, col) == XmHEADING &&
				RowPosToType(g, row) == XmCONTENT)
				{
				if (q == qTOGGLE)
					{
					rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
					if (rowp && XmLGridRowIsSelected(rowp) == True)
						g->grid.extendSelect = False;
					}
				ExtendSelect(g, event, True, row, col);
				}
			}
		if (g->grid.selectionPolicy == XmSELECT_SINGLE_ROW &&
			RowPosToType(g, row) == XmCONTENT)
			{
			flag = True;
			rowp = (XmLGridRow)XmLArrayGet(g->grid.rowArray, row);
			if (rowp && XmLGridRowIsSelected(rowp) == True)
				flag = False;
			SelectTypeArea(g, SelectRow, event,
				RowPosToTypePos(g, XmCONTENT, row), 0, flag, True);
			}
		if (g->grid.selectionPolicy == XmSELECT_BROWSE_ROW &&
			RowPosToType(g, row) == XmCONTENT)
			SelectTypeArea(g, SelectRow, event,
				RowPosToTypePos(g, XmCONTENT, row), 0, True, True);
		if (g->grid.selectionPolicy == XmSELECT_NONE ||
			(g->grid.selectionPolicy == XmSELECT_BROWSE_ROW &&
			RowPosToType(g, row) != XmCONTENT) ||
			(g->grid.selectionPolicy == XmSELECT_SINGLE_ROW &&
			RowPosToType(g, row) != XmCONTENT) ||
			(g->grid.selectionPolicy == XmSELECT_MULTIPLE_ROW &&
			RowPosToType(g, row) != XmCONTENT) )
			{
			cbs.reason = XmCR_SELECT_CELL;
			cbs.event = event;
			cbs.columnType = ColPosToType(g, col);
			cbs.column = ColPosToTypePos(g, cbs.columnType, col);
			cbs.rowType = RowPosToType(g, row);
			cbs.row = RowPosToTypePos(g, cbs.rowType, row);
			XtCallCallbackList((Widget)g, g->grid.selectCallback,
				(XtPointer)&cbs);
			}
		g->grid.inMode = InSelect;
		}
	else if (q == qEND && g->grid.inMode == InResize)
		{
		int r, c, width, height;
		r = g->grid.resizeRow;
		c = g->grid.resizeCol;
		g->grid.resizeRow = -1;
		g->grid.resizeCol = -1;
		if (!RowColToXY(g, r, c, False, &rect))
			{
			if (g->grid.resizeIsVert)
				{
				cbs.rowType = RowPosToType(g, r);
				cbs.row = RowPosToTypePos(g, cbs.rowType, r);
				height = 0;
				if (g->grid.resizeLineXY > rect.y)
					height = g->grid.resizeLineXY - rect.y -
						(rect.height - GetRowHeight(g, r));
				if (height < 6 && g->grid.allowRowHide == False)
					height = 6;
				XtVaSetValues((Widget)g,
					XmNrowType, cbs.rowType,
					XmNrow, cbs.row,
					XmNrowHeight, height,
					XmNrowSizePolicy, XmCONSTANT,
					NULL);
				cbs.reason = XmCR_RESIZE_ROW;
				}
			else
				{
				cbs.columnType = ColPosToType(g, c);
				cbs.column = ColPosToTypePos(g, cbs.columnType, c);
				width = 0;
				if (g->grid.resizeLineXY > rect.x)
					width = g->grid.resizeLineXY - rect.x -
						(rect.width - GetColWidth(g, c));
				if (width < 6 && g->grid.allowColHide == False)
					width = 6;
				XtVaSetValues((Widget)g,
					XmNcolumnType, cbs.columnType,
					XmNcolumn, cbs.column,
					XmNcolumnWidth, width,
					XmNcolumnSizePolicy, XmCONSTANT,
					NULL);
				cbs.reason = XmCR_RESIZE_COLUMN;
				}
			XtCallCallbackList((Widget)g, g->grid.resizeCallback,
				(XtPointer)&cbs);
			}
		DrawResizeLine(g, 0, 2);
		DefineCursor(g, CursorNormal);
		g->grid.inMode = InNormal;
		}
	else if (q == qEND)
		{
		g->grid.inMode = InNormal;
		if (g->grid.dragTimerSet)
			XtRemoveTimeOut(g->grid.dragTimerId);
		g->grid.dragTimerSet = 0;

		/* XFE Additions to handle button up events in menus - they generate activate events */
		  {
		    if (XmIsMenuShell(XmLShellOfWidget((Widget)g)))
		    {
		      cbs.reason = XmCR_ACTIVATE;
		      cbs.event = event;
		      cbs.columnType = ColPosToType(g, col);
		      cbs.column = ColPosToTypePos(g, cbs.columnType, col);
		      cbs.rowType = RowPosToType(g, row);
		      cbs.row = RowPosToTypePos(g, cbs.rowType, row);
		      XtCallCallbackList((Widget)g, g->grid.activateCallback,
					 (XtPointer)&cbs);		      
		    }
		  }
		}
	if (q == qACTIVATE)
		{
		cbs.reason = XmCR_ACTIVATE;
		cbs.event = event;
		cbs.columnType = ColPosToType(g, col);
		cbs.column = ColPosToTypePos(g, cbs.columnType, col);
		cbs.rowType = RowPosToType(g, row);
		cbs.row = RowPosToTypePos(g, cbs.rowType, row);
		XtCallCallbackList((Widget)g, g->grid.activateCallback,
			(XtPointer)&cbs);
		}
	}

static void
Traverse(Widget w,
	 XEvent *event,
	 String *params,
	 Cardinal *nparam)
	{
	XmLGridWidget g;
	static XrmQuark qDOWN, qEXTEND_DOWN, qEXTEND_LEFT;
	static XrmQuark qEXTEND_PAGE_DOWN, qEXTEND_PAGE_LEFT;
	static XrmQuark qEXTEND_PAGE_RIGHT, qEXTEND_PAGE_UP;
	static XrmQuark qEXTEND_RIGHT, qEXTEND_UP, qLEFT, qPAGE_DOWN;
	static XrmQuark qPAGE_LEFT, qPAGE_RIGHT, qPAGE_UP, qRIGHT;
	static XrmQuark qTO_BOTTOM, qTO_BOTTOM_RIGHT, qTO_LEFT;
	static XrmQuark qTO_RIGHT, qTO_TOP, qTO_TOP_LEFT, qUP;
	static int quarksValid = 0;
	int extend, focusRow, focusCol, rowDir, colDir;
	int rowLoc, colLoc, prevRowLoc, prevColLoc;
	int scrollRow, scrollCol, prevScrollRow, prevScrollCol;
	XrmQuark q;

	g = (XmLGridWidget)XtParent(w);
	if (*nparam != 1)
		return;
	if (!quarksValid)
		{
		qDOWN = XrmStringToQuark("DOWN");
		qEXTEND_DOWN = XrmStringToQuark("EXTEND_DOWN");
		qEXTEND_LEFT = XrmStringToQuark("EXTEND_LEFT");
		qEXTEND_PAGE_DOWN = XrmStringToQuark("EXTEND_PAGE_DOWN");
		qEXTEND_PAGE_LEFT = XrmStringToQuark("EXTEND_PAGE_LEFT");
		qEXTEND_PAGE_RIGHT = XrmStringToQuark("EXTEND_PAGE_RIGHT");
		qEXTEND_PAGE_UP = XrmStringToQuark("EXTEND_PAGE_UP");
		qEXTEND_RIGHT = XrmStringToQuark("EXTEND_RIGHT");
		qEXTEND_UP = XrmStringToQuark("EXTEND_UP");
		qLEFT = XrmStringToQuark("LEFT");
		qPAGE_DOWN = XrmStringToQuark("PAGE_DOWN");
		qPAGE_LEFT = XrmStringToQuark("PAGE_LEFT");
		qPAGE_RIGHT = XrmStringToQuark("PAGE_RIGHT");
		qPAGE_UP = XrmStringToQuark("PAGE_UP");
		qRIGHT = XrmStringToQuark("RIGHT");
		qTO_BOTTOM = XrmStringToQuark("TO_BOTTOM");
		qTO_BOTTOM_RIGHT = XrmStringToQuark("TO_BOTTOM_RIGHT");
		qTO_LEFT = XrmStringToQuark("TO_LEFT");
		qTO_RIGHT = XrmStringToQuark("TO_RIGHT");
		qTO_TOP = XrmStringToQuark("TO_TOP");
		qTO_TOP_LEFT = XrmStringToQuark("TO_TOP_LEFT");
		qUP = XrmStringToQuark("UP");
		quarksValid = 1;
		}
	q = XrmStringToQuark(params[0]);
	extend = 0;
	/* map the extends to their counterparts and set extend flag */
	if (q == qEXTEND_DOWN)
		{
		q = qDOWN;
		extend = 1;
		}
	else if (q == qEXTEND_LEFT)
		{
		q = qLEFT;
		extend = 1;
		}
	else if (q == qEXTEND_PAGE_DOWN)
		{
		q = qPAGE_DOWN;
		extend = 1;
		}
	else if (q == qEXTEND_PAGE_LEFT)
		{
		q = qPAGE_LEFT;
		extend = 1;
		}
	else if (q == qEXTEND_PAGE_RIGHT)
		{
		q = qPAGE_RIGHT;
		extend = 1;
		}
	else if (q == qEXTEND_PAGE_UP)
		{
		q = qPAGE_UP;
		extend = 1;
		}
	else if (q == qEXTEND_RIGHT)
		{
		q = qRIGHT;
		extend = 1;
		}
	else if (q == qEXTEND_UP)
		{
		q = qUP;
		extend = 1;
		}
	if (extend && g->grid.selectionPolicy != XmSELECT_MULTIPLE_ROW &&
		g->grid.selectionPolicy != XmSELECT_CELL)
		return;
	if (extend && g->grid.extendRow != -1 && g->grid.extendCol != -1)
		{
		focusRow = g->grid.extendToRow;
		focusCol = g->grid.extendToCol;
		}
	else
		{
		focusRow = g->grid.focusRow;
		focusCol = g->grid.focusCol;
		}
	rowDir = 0;
	colDir = 0;
	if (focusRow < g->grid.topFixedCount)
		prevRowLoc = 0;
	else if (focusRow >= XmLArrayGetCount(g->grid.rowArray) -
		g->grid.bottomFixedCount)
		prevRowLoc = 2;
	else
		prevRowLoc = 1;
	if (focusCol < g->grid.leftFixedCount)
		prevColLoc = 0;
	else if (focusCol >= XmLArrayGetCount(g->grid.colArray) -
		g->grid.rightFixedCount)
		prevColLoc = 2;
	else
		prevColLoc = 1;
	/* calculate new focus row, col and walk direction */
	if (q == qDOWN)
		{
		focusRow++;
		rowDir = 1;
		}
	else if (q == qLEFT)
		{
		focusCol--;
		colDir = -1;
		}
	else if (q == qPAGE_DOWN)
		{
		if (prevRowLoc == 1)
			focusRow = g->grid.reg[4].row + g->grid.reg[4].nrow - 1;
		if (focusRow == g->grid.focusRow)
			focusRow += 4;
		rowDir = 1;
		}
	else if (q == qPAGE_LEFT)
		{
		if (prevColLoc == 1)
			focusCol = g->grid.reg[4].col - 1;
		if (focusCol == g->grid.focusCol)
			focusCol -= 4;
		colDir = -1;
		}
	else if (q == qPAGE_RIGHT)
		{
		if (prevColLoc == 1)
			focusCol = g->grid.reg[4].col + g->grid.reg[4].ncol - 1;
		if (focusCol == g->grid.focusCol)
			focusCol += 4;
		colDir = 1;
		}
	else if (q == qPAGE_UP)
		{
		if (prevRowLoc == 1)
			focusRow = g->grid.reg[4].row - 1;
		if (focusRow == g->grid.focusRow)
			focusRow -= 4;
		rowDir = -1;
		}
	else if (q == qRIGHT)
		{
		focusCol++;
		colDir = 1;
		}
	else if (q == qTO_BOTTOM)
		{
		focusRow = XmLArrayGetCount(g->grid.rowArray) - 1;
		rowDir = -1;
		}
	else if (q == qTO_BOTTOM_RIGHT)
		{
		focusCol = XmLArrayGetCount(g->grid.colArray) - 1;
		focusRow = XmLArrayGetCount(g->grid.rowArray) - 1;
		rowDir = -1;
		colDir = -1;
		}
	else if (q == qTO_LEFT)
		{
		focusCol = 0;
		colDir = 1;
		}
	else if (q == qTO_RIGHT)
		{
		focusCol = XmLArrayGetCount(g->grid.colArray) - 1;
		colDir = -1;
		}
	else if (q == qTO_TOP)
		{
		focusRow = 0;
		rowDir = 1;
		}
	else if (q == qTO_TOP_LEFT)
		{
		focusCol = 0;
		focusRow = 0;
		rowDir = 1;
		colDir = 1;
		}
	else if (q == qUP)
		{
		focusRow -= 1;
		rowDir = -1;
		}
	if (!rowDir && !colDir)
		return;
	if (extend)
		{
		if (FindNextFocus(g, focusRow, focusCol, rowDir, colDir,
			&focusRow, &focusCol) == -1)
			return;
		ExtendSelect(g, event, False, focusRow, focusCol);
		}
	else
		{
		if (SetFocus(g, focusRow, focusCol, rowDir, colDir) == -1)
			return;
		ExtendSelect(g, event, False, -1, -1);
		focusRow = g->grid.focusRow;
		focusCol = g->grid.focusCol;
		if (g->grid.selectionPolicy == XmSELECT_CELL)
			{
			SelectTypeArea(g, SelectRow, event, -1, 0, False, True);
			SelectTypeArea(g, SelectCol, event, 0, -1, False, True);
			SelectTypeArea(g, SelectCell, event, -1, -1, False, True);
			SelectTypeArea(g, SelectCell, event,
				RowPosToTypePos(g, XmCONTENT, focusRow),
				ColPosToTypePos(g, XmCONTENT, focusCol),
				True, True);
			}
		else if (g->grid.selectionPolicy == XmSELECT_BROWSE_ROW)
			SelectTypeArea(g, SelectRow, event,
				RowPosToTypePos(g, XmCONTENT, focusRow), 0, True, True);
		}
	scrollRow = -1;
	scrollCol = -1;
	if (q == qPAGE_UP)
		scrollRow = ScrollRowBottomPos(g, focusRow);
	else if (q == qPAGE_DOWN)
		scrollRow = focusRow;
	else if (q == qPAGE_LEFT)
		scrollCol = ScrollColRightPos(g, focusCol);
	else if (q == qPAGE_RIGHT)
		scrollCol = focusCol;
	else
		{
		if (focusRow < g->grid.topFixedCount)
			rowLoc = 0;
		else if (focusRow >= XmLArrayGetCount(g->grid.rowArray) -
			g->grid.bottomFixedCount)
			rowLoc = 2;
		else
			rowLoc = 1;
		if (prevRowLoc != 0 && rowLoc == 0)
			scrollRow = g->grid.reg[0].nrow;
		else if (prevRowLoc != 2 && rowLoc == 2)
			scrollRow = g->grid.reg[6].row - 1;

		if (focusCol < g->grid.leftFixedCount)
			colLoc = 0;
		else if (focusCol >= XmLArrayGetCount(g->grid.colArray) -
			g->grid.rightFixedCount)
			colLoc = 2;
		else
			colLoc = 1;
		if (prevColLoc != 0 && colLoc == 0)
			scrollCol = g->grid.reg[0].ncol;
		else if (prevColLoc != 2 && colLoc == 2)
			scrollCol = g->grid.reg[2].col - 1;
		}
	if (g->grid.vsPolicy == XmVARIABLE)
		;
	else if (scrollRow != -1)
		{
		prevScrollRow = g->grid.scrollRow;
		g->grid.scrollRow = scrollRow;
		VertLayout(g, 0);
		if (g->grid.scrollRow != prevScrollRow)
			DrawArea(g, DrawVScroll, 0, 0);
		}
	else
		MakeRowVisible(g, focusRow);
	if (g->grid.hsPolicy == XmVARIABLE)
		;
	else if (scrollCol != -1)
		{
		prevScrollCol = g->grid.scrollCol;
		g->grid.scrollCol = scrollCol;
		HorizLayout(g, 0);
		if (g->grid.scrollCol != prevScrollCol)
			DrawArea(g, DrawHScroll, 0, 0);
		}
	else
		MakeColVisible(g, focusCol);
	}

static void
TextActivate(Widget w,
	     XtPointer clientData,
	     XtPointer callData)
	{
	XmLGridWidget g;
	XmAnyCallbackStruct *cbs;
	String params[1];
	Cardinal nparam;

	cbs = (XmAnyCallbackStruct *)callData;
	g = (XmLGridWidget)XtParent(w);
	if (g->grid.inEdit)
		{
		nparam = 0;
		EditComplete(w, cbs->event, params, &nparam);
		}
	else
		{
		params[0] = "ACTIVATE";
		nparam = 1;
		Select(XtParent(w), cbs->event, params, &nparam);
		}
	}

static void
TextFocus(Widget w,
	  XtPointer clientData,
	  XtPointer callData)
	{
	XmLGridWidget g;
	XmAnyCallbackStruct *cbs;
	int focusRow, focusCol;

	cbs = (XmAnyCallbackStruct *)callData;
	g = (XmLGridWidget)XtParent(w);
	if (cbs->reason == XmCR_FOCUS)
		g->grid.focusIn = 1;
	else
		g->grid.focusIn = 0;
	focusRow = g->grid.focusRow;
	focusCol = g->grid.focusCol;
	if (focusRow != -1 && focusCol != -1)
		{
		if (g->grid.highlightRowMode == True)
			DrawArea(g, DrawRow, focusRow, 0);
		else
			DrawArea(g, DrawCell, focusRow, focusCol);
		}
	}

static void
TextMapped(Widget w,
	   XtPointer clientData,
	   XEvent *event,
	   Boolean *con)
	{
	XmLGridWidget g;

	g = (XmLGridWidget)(XtParent(w));
	if (event->type != MapNotify)
		return;
	if (g->grid.textHidden)
		XtUnmapWidget(g->grid.text);
	}

static void
TextModifyVerify(Widget w,
		 XtPointer clientData,
		 XtPointer callData)
	{
	XmLGridWidget g;
	XmTextVerifyCallbackStruct *cbs;

	g = (XmLGridWidget)XtParent(w);
	cbs = (XmTextVerifyCallbackStruct *)callData;
	if (!cbs->event || g->grid.ignoreModifyVerify || g->grid.inEdit)
		return;
	TextAction(g, TEXT_EDIT);
	if (!g->grid.inEdit)
		cbs->doit = False;
	}

/*
   XmLGridRow
*/

static
XmLGridRow XmLGridRowNew(Widget grid)
	{
	return XmLGridClassPartOfWidget(grid).rowNewProc(grid);
	}

/* Only to be called through Grid class */
static XmLGridRow 
_GridRowNew(Widget grid)
	{
	XmLGridWidget g;
	XmLGridRow row;
	int size;

	g = (XmLGridWidget)grid;
	size = XmLGridClassPartOfWidget(grid).rowRecSize;
	row = (XmLGridRow)malloc(size);
	row->grid.grid = grid;
	row->grid.heightInPixels = 0;
	row->grid.heightInPixelsValid = 0;
	row->grid.height = 1;
	row->grid.selected = False;
	row->grid.sizePolicy = XmVARIABLE;
	row->grid.userData = 0;
	row->grid.cellArray = XmLArrayNew(0, 0);
	return row;
	}

static void
XmLGridRowFree(Widget grid,
	       XmLGridRow row)
	{
	XmLGridClassPartOfWidget(grid).rowFreeProc(row);
	}

/* Only to be called through Grid class */
static void
_GridRowFree(XmLGridRow row)
	{
	int i, count;

	count = XmLArrayGetCount(row->grid.cellArray);
	for (i = 0; i < count; i++)
		XmLGridCellFree(row->grid.grid,
			(XmLGridCell)XmLArrayGet(row->grid.cellArray, i));
	XmLArrayFree(row->grid.cellArray);
	free ((char *)row);
	}

static
XmLArray XmLGridRowCells(XmLGridRow row)
	{
	return row->grid.cellArray;
	}

static int
XmLGridRowGetPos(XmLGridRow row)
	{
	return row->grid.pos;
	}

static int
XmLGridRowGetVisPos(XmLGridRow row)
	{
	return row->grid.visPos;
	}

static Boolean
XmLGridRowIsHidden(XmLGridRow row)
	{
	if (!row->grid.height)
		return True;
	return False;
	}

static Boolean
XmLGridRowIsSelected(XmLGridRow row)
	{
	return row->grid.selected;
	}

static void
XmLGridRowSetSelected(XmLGridRow row,
		      Boolean selected)
	{
	row->grid.selected = selected;
	}

static void
XmLGridRowSetVisPos(XmLGridRow row,
		    int visPos)
	{
	row->grid.visPos = visPos;
	}

static int
XmLGridRowHeightInPixels(XmLGridRow row)
	{
	int i, count;
	Dimension height, maxHeight;
	XmLGridWidget g;
	XmLGridCell cell;
	XmLGridCallbackStruct cbs;
	XmLGridCellRefValues *cellValues;

	if (row->grid.sizePolicy == XmCONSTANT)
		return row->grid.height;
	if (!row->grid.height)
		return 0;
	if (!row->grid.heightInPixelsValid)
		{
		g = (XmLGridWidget)row->grid.grid;
		count = XmLArrayGetCount(row->grid.cellArray);
		if (!count)
			{
			cellValues = g->grid.defCellValues;
			maxHeight = 4 + row->grid.height * cellValues->fontHeight +
				cellValues->topMargin + cellValues->bottomMargin;
			}
		else
			maxHeight = 0;
		for (i = 0; i < count; i++)
			{
			cell = (XmLGridCell)XmLArrayGet(row->grid.cellArray, i);
			cbs.reason = XmCR_PREF_HEIGHT;
			cbs.rowType = RowPosToType(g, row->grid.pos);
			cbs.row = RowPosToTypePos(g, cbs.rowType, row->grid.pos);
			cbs.columnType = ColPosToType(g, i);
			cbs.column = ColPosToTypePos(g, cbs.columnType, i);
			cbs.object = (void *)row;
			height = XmLGridCellAction(cell, (Widget)g, &cbs);
			if (height > maxHeight)
				maxHeight = height;
			}
		row->grid.heightInPixels = maxHeight;
		row->grid.heightInPixelsValid = 1;
		}
	return row->grid.heightInPixels;
	}

static void
XmLGridRowHeightChanged(XmLGridRow row)
	{
	row->grid.heightInPixelsValid = 0;
	}

/*
   XmLGridColumn
*/

static XmLGridColumn
XmLGridColumnNew(Widget grid)
	{
	return XmLGridClassPartOfWidget(grid).columnNewProc(grid);
	}

/* Only to be called through Grid class */
static XmLGridColumn 
_GridColumnNew(Widget grid)
	{
	XmLGridWidget g;
	XmLGridColumn column;
	int size;

	g = (XmLGridWidget)grid;
	size = XmLGridClassPartOfWidget(grid).columnRecSize;
	column = (XmLGridColumn)malloc(size);
	column->grid.grid = grid;
	column->grid.widthInPixels = 0;
	column->grid.widthInPixelsValid = 0;
	column->grid.defCellValues = 0;
	column->grid.selected = False;
	column->grid.sizePolicy = XmVARIABLE;
	column->grid.width = 8;
	column->grid.userData = 0;
	column->grid.resizable = True;
	return column;
	}

static void
XmLGridColumnFree(Widget grid,
		  XmLGridColumn column)
	{
	XmLGridClassPartOfWidget(grid).columnFreeProc(column);
	}

/* Only to be called through Grid class */
static void
_GridColumnFree(XmLGridColumn column)
	{
	if (column->grid.defCellValues)
		XmLGridCellDerefValues(column->grid.defCellValues);
	free((char *)column);
	}

static int
XmLGridColumnGetPos(XmLGridColumn column)
	{
	return column->grid.pos;
	}

static int
XmLGridColumnGetVisPos(XmLGridColumn column)
	{
	return column->grid.visPos;
	}

static Boolean
XmLGridColumnIsHidden(XmLGridColumn column)
	{
	if (!column->grid.width)
		return True;
	return False;
	}

static Boolean
XmLGridColumnIsSelected(XmLGridColumn column)
	{
	return column->grid.selected;
	}

static void
XmLGridColumnSetSelected(XmLGridColumn column,
			 Boolean selected)
	{
	column->grid.selected = selected;
	}

static void
XmLGridColumnSetVisPos(XmLGridColumn column,
		       int visPos)
	{
	column->grid.visPos = visPos;
	}

static int
XmLGridColumnWidthInPixels(XmLGridColumn column)
	{
	int i, count;
	Dimension width, maxWidth;
	XmLGridCell cell;
	XmLGridWidget g;
	XmLGridCallbackStruct cbs;
	XmLGridCellRefValues *cellValues;

	if (column->grid.sizePolicy == XmCONSTANT)
		return column->grid.width;
	if (!column->grid.width)
		return 0;
	if (!column->grid.widthInPixelsValid)
		{
		g = (XmLGridWidget)column->grid.grid;
		count = XmLArrayGetCount(g->grid.rowArray);
		if (!count)
			{
			cellValues = g->grid.defCellValues;
			maxWidth = 4 + column->grid.width * cellValues->fontWidth +
				cellValues->leftMargin + cellValues->rightMargin;
			}
		else
			maxWidth = 0;
		for (i = 0; i < count; i++)
			{
			cell = GetCell(g, i, column->grid.pos);
			cbs.reason = XmCR_PREF_WIDTH;
			cbs.rowType = RowPosToType(g, i);
			cbs.row = RowPosToTypePos(g, cbs.rowType, i);
			cbs.columnType = ColPosToType(g, column->grid.pos);
			cbs.column = ColPosToTypePos(g, cbs.columnType, column->grid.pos);
			cbs.object = (void *)column;
			width = XmLGridCellAction(cell, (Widget)g, &cbs);
			if (width > maxWidth)
				maxWidth = width;
			}
		column->grid.widthInPixels = maxWidth;
		column->grid.widthInPixelsValid = 1;
		}
	return column->grid.widthInPixels;
	}

static void
XmLGridColumnWidthChanged(XmLGridColumn column)
	{
	column->grid.widthInPixelsValid = 0;
	}

/*
   XmLGridCell
*/

static XmLGridCell
XmLGridCellNew(void)
	{
	XmLGridCell c;

	c = (XmLGridCell)malloc(sizeof(struct _XmLGridCellRec));
	c->cell.refValues = 0;
	c->cell.flags = 0;
	return c;
	}

static void
XmLGridCellFree(Widget grid,
		XmLGridCell cell)
	{
	XmLGridCallbackStruct cbs;

	cbs.reason = XmCR_FREE_VALUE;
	XmLGridCellAction(cell, grid, &cbs);
	XmLGridCellDerefValues(cell->cell.refValues);
	free((char *)cell);
	}

static int
XmLGridCellAction(XmLGridCell cell,
		  Widget w,
		  XmLGridCallbackStruct *cbs)
	{
	return XmLGridClassPartOfWidget(w).cellActionProc(cell, w, cbs);
	}

/* Only to be called through Grid class */
static int
_GridCellAction(XmLGridCell cell,
		Widget w,
		XmLGridCallbackStruct *cbs)
	{
	int ret;

	ret = 0;
	switch (cbs->reason)
		{
		case XmCR_CELL_DRAW:
			_XmLGridCellDrawBackground(cell, w, cbs->clipRect, cbs->drawInfo);
			_XmLGridCellDrawValue(cell, w, cbs->clipRect, cbs->drawInfo);
			_XmLGridCellDrawBorders(cell, w, cbs->clipRect, cbs->drawInfo);
			break;
		case XmCR_CELL_FOCUS_IN:
			break;
		case XmCR_CELL_FOCUS_OUT:
			break;
		case XmCR_CONF_TEXT:
			_XmLGridCellConfigureText(cell, w, cbs->clipRect);
			break;
		case XmCR_EDIT_BEGIN:
			ret = _XmLGridCellBeginTextEdit(cell, w, cbs->clipRect);
			break;
		case XmCR_EDIT_CANCEL:
			break;
		case XmCR_EDIT_COMPLETE:
			_XmLGridCellCompleteTextEdit(cell, w);
			break;
		case XmCR_EDIT_INSERT:
			ret = _XmLGridCellBeginTextEdit(cell, w, cbs->clipRect);
			if (ret)
				_XmLGridCellInsertText(cell, w);
			break;
		case XmCR_FREE_VALUE:
			_XmLGridCellFreeValue(cell);
			break;
		case XmCR_PREF_HEIGHT:
			ret = _XmLGridCellGetHeight(cell, (XmLGridRow)cbs->object);
			break;
		case XmCR_PREF_WIDTH:
			ret = _XmLGridCellGetWidth(cell, (XmLGridColumn)cbs->object);
			break;
		}
	return ret;
	}

static XmLGridCellRefValues *
XmLGridCellGetRefValues(XmLGridCell cell)
	{
	return cell->cell.refValues;
	}

static void
XmLGridCellSetRefValues(XmLGridCell cell,
			XmLGridCellRefValues *values)
	{
	values->refCount++;
	XmLGridCellDerefValues(cell->cell.refValues);
	cell->cell.refValues = values;
	}

static void
XmLGridCellDerefValues(XmLGridCellRefValues *values)
	{
	if (!values)
		return;
	values->refCount--;
	if (!values->refCount)
		{
		XmFontListFree(values->fontList);
		free((char *)values);
		}
	}

static Boolean
XmLGridCellInRowSpan(XmLGridCell cell)
	{
	if (cell->cell.flags & XmLGridCellInRowSpanFlag)
		return True;
	return False;
	}

static Boolean
XmLGridCellInColumnSpan(XmLGridCell cell)
	{
	if (cell->cell.flags & XmLGridCellInColumnSpanFlag)
		return True;
	return False;
	}

static Boolean
XmLGridCellIsSelected(XmLGridCell cell)
	{
	if (cell->cell.flags & XmLGridCellSelectedFlag)
		return True;
	return False;
	}

static Boolean
XmLGridCellIsValueSet(XmLGridCell cell)
	{
	if (cell->cell.flags & XmLGridCellValueSetFlag)
		return True;
	return False;
	}

static void
XmLGridCellSetValueSet(XmLGridCell cell,
		       Boolean set)
	{
	cell->cell.flags |= XmLGridCellValueSetFlag;
	if (set != True)
		cell->cell.flags ^= XmLGridCellValueSetFlag;
	}

static void
XmLGridCellSetInRowSpan(XmLGridCell cell,
			Boolean set)
	{
	cell->cell.flags |= XmLGridCellInRowSpanFlag;
	if (set != True)
		cell->cell.flags ^= XmLGridCellInRowSpanFlag;
	}

static void
XmLGridCellSetInColumnSpan(XmLGridCell cell,
			   Boolean set)
	{
	cell->cell.flags |= XmLGridCellInColumnSpanFlag;
	if (set != True)
		cell->cell.flags ^= XmLGridCellInColumnSpanFlag;
	}

static void
XmLGridCellSetSelected(XmLGridCell cell,
		       Boolean selected)
	{
	cell->cell.flags |= XmLGridCellSelectedFlag;
	if (selected != True)
		cell->cell.flags ^= XmLGridCellSelectedFlag;
	}

static void
XmLGridCellAllocIcon(XmLGridCell cell)
	{
	XmLGridCellIcon *icon;

	icon = (XmLGridCellIcon *)malloc(sizeof(XmLGridCellIcon));
	icon->pix.pixmap = XmUNSPECIFIED_PIXMAP;
	icon->pix.pixmask = XmUNSPECIFIED_PIXMAP;
	icon->pix.width = 0;
	icon->pix.height = 0;
	icon->string = 0;
	cell->cell.value = (void *)icon;
	XmLGridCellSetValueSet(cell, True);
	}

static void
XmLGridCellAllocPixmap(XmLGridCell cell)
	{
	XmLGridCellPixmap *pix;

	pix = (XmLGridCellPixmap *)malloc(sizeof(XmLGridCellPixmap));
	pix->width = 0;
	pix->height = 0;
	pix->pixmap = XmUNSPECIFIED_PIXMAP;
	pix->pixmask = XmUNSPECIFIED_PIXMAP;
	cell->cell.value = (void *)pix;
	XmLGridCellSetValueSet(cell, True);
	}

static void
XmLGridCellSetString(XmLGridCell cell,
		     XmString string,
		     Boolean copy)
	{
	XmLGridCellIcon *icon;

	if (cell->cell.refValues->type == XmSTRING_CELL)
		{
		_XmLGridCellFreeValue(cell);
		if (string)
			{
			if (copy == True)
				cell->cell.value = (void *)XmStringCopy(string);
			else
				cell->cell.value = (void *)string;
			XmLGridCellSetValueSet(cell, True);
			}
		else
			XmLGridCellSetValueSet(cell, False);
		}
	else if (cell->cell.refValues->type == XmICON_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == False)
			XmLGridCellAllocIcon(cell);
		icon = (XmLGridCellIcon *)cell->cell.value;
		if (icon->string)
			XmStringFree(icon->string);
		if (string && copy == True)
			icon->string = XmStringCopy(string);
		else
			icon->string = string;
		}
	else if (string && copy == False)
		XmStringFree(string);
	}

static XmString
XmLGridCellGetString(XmLGridCell cell)
	{
	XmString str;

	str = 0;
	if (cell->cell.refValues->type == XmSTRING_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == True)
			str = (XmString)cell->cell.value;
		}
	else if (cell->cell.refValues->type == XmICON_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == True)
			str = ((XmLGridCellIcon *)cell->cell.value)->string;
		}
	return str;
	}

static void
XmLGridCellSetToggle(XmLGridCell cell,
		     Boolean state)
	{
	if (cell->cell.refValues->type == XmTOGGLE_CELL)
		{
		if (state)
			cell->cell.value = (void *)1;
		else
			cell->cell.value = (void *)0;
		XmLGridCellSetValueSet(cell, True);
		}
	}

static Boolean
XmLGridCellGetToggle(XmLGridCell cell)
	{
	Boolean toggled;

	toggled = False;
	if (cell->cell.refValues->type == XmTOGGLE_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == True &&
			cell->cell.value == (void *)1)
			toggled = True;
		}
	return toggled;
	}

static void
XmLGridCellSetPixmap(XmLGridCell cell,
		     Pixmap pixmap,
		     Dimension width,
		     Dimension height)
	{
	XmLGridCellPixmap *pix;

	pix = 0;
	if (cell->cell.refValues->type == XmPIXMAP_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == False)
			XmLGridCellAllocPixmap(cell);
		pix = (XmLGridCellPixmap *)cell->cell.value;
		}
	else if (cell->cell.refValues->type == XmICON_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == False)
			XmLGridCellAllocIcon(cell);
		pix = &((XmLGridCellIcon *)cell->cell.value)->pix;
		}
	if (!pix)
		return;
	pix->pixmap = pixmap;
	pix->height = height;
	pix->width = width;
	}

static void
XmLGridCellSetPixmask(XmLGridCell cell,
		      Pixmap pixmask)
	{
	XmLGridCellPixmap *pix;

	if (XmLGridCellIsValueSet(cell) == False)
		{
		fprintf(stderr, "XmLGridCellSetPixmask: pixmap must be set ");
		fprintf(stderr, "before pixmask\n");
		return; 
		}
	pix = 0;
	if (cell->cell.refValues->type == XmPIXMAP_CELL)
		pix = (XmLGridCellPixmap *)cell->cell.value;
	else if (cell->cell.refValues->type == XmICON_CELL)
		pix = &((XmLGridCellIcon *)cell->cell.value)->pix;
	if (!pix)
		return;
	pix->pixmask = pixmask;
	}

static XmLGridCellPixmap *
XmLGridCellGetPixmap(XmLGridCell cell)
	{
	XmLGridCellPixmap *pix;

	pix = 0;
	if (cell->cell.refValues->type == XmPIXMAP_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == True)
			pix = (XmLGridCellPixmap *)cell->cell.value;
		}
	else if (cell->cell.refValues->type == XmICON_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == True)
			pix = &((XmLGridCellIcon *)cell->cell.value)->pix;
		}
	return pix;
	}

/* Only to be called by XmLGridCellAction() */
void 
_XmLGridCellDrawBackground(XmLGridCell cell,
			   Widget w,
			   XRectangle *clipRect,
			   XmLGridDrawStruct *ds)
	{
	Display *dpy;
	Window win;

	dpy = XtDisplay(w);
	win = XtWindow(w);
	if (ds->drawSelected == True)
		XSetForeground(dpy, ds->gc, ds->selectBackground);
	else
		XSetForeground(dpy, ds->gc, ds->background);
	XFillRectangle(dpy, win, ds->gc, clipRect->x, clipRect->y,
		clipRect->width, clipRect->height);
	}

/* Only to be called by XmLGridCellAction() */
void 
_XmLGridCellDrawBorders(XmLGridCell cell,
			Widget w,
			XRectangle *clipRect,
			XmLGridDrawStruct *ds)
	{
	XmLGridWidget g;
	Display *dpy;
	Window win;
	GC gc;
	Pixel black, white;
	int x1, x2, y1, y2, loff, roff;
	int drawLeft, drawRight, drawBot, drawTop;
	XRectangle *cellRect;
	unsigned char borderType;

	g = (XmLGridWidget)w;
	dpy = XtDisplay(w);
	win = XtWindow(w);
	gc = ds->gc;
	cellRect = ds->cellRect;
	black = BlackPixelOfScreen(XtScreen(w));
	white = WhitePixelOfScreen(XtScreen(w));
	x1 = clipRect->x;
	x2 = clipRect->x + clipRect->width - 1;
	y1 = clipRect->y;
	y2 = clipRect->y + clipRect->height - 1;
	drawLeft = 1;
	drawRight = 1;
	drawBot = 1;
	drawTop = 1;
	if (cellRect->x != clipRect->x)	
		drawLeft = 0;
	if (cellRect->x + cellRect->width != clipRect->x + clipRect->width)
		drawRight = 0;
	if (cellRect->y != clipRect->y)
		drawTop = 0;
	if (cellRect->y + cellRect->height != clipRect->y + clipRect->height)
		drawBot = 0;
	borderType = cell->cell.refValues->rightBorderType;
	if (borderType != XmBORDER_NONE && drawRight)
		{
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineOnOffDash, CapButt, JoinMiter);
		XSetForeground(dpy, gc, cell->cell.refValues->rightBorderColor);
		XDrawLine(dpy, win, gc, x2, y1, x2, y2);
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
		}
	borderType = cell->cell.refValues->bottomBorderType;
	if (borderType != XmBORDER_NONE && drawBot)
		{
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineOnOffDash, CapButt, JoinMiter);
		XSetForeground(dpy, gc, cell->cell.refValues->bottomBorderColor);
		XDrawLine(dpy, win, gc, x1, y2, x2, y2);
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
		}	
	borderType = cell->cell.refValues->topBorderType;
	if (borderType != XmBORDER_NONE && drawTop)
		{
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineOnOffDash, CapButt, JoinMiter);
		XSetForeground(dpy, gc, cell->cell.refValues->topBorderColor);
		XDrawLine(dpy, win, gc, x1, y1, x2, y1);
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
		}
	borderType = cell->cell.refValues->leftBorderType;
	if (borderType != XmBORDER_NONE && drawLeft)
		{
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineOnOffDash, CapButt, JoinMiter);
		XSetForeground(dpy, gc, cell->cell.refValues->leftBorderColor);
		XDrawLine(dpy, win, gc, x1, y1, x1, y2);
		if (borderType == XmBORDER_DASH)
			XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
		}
	if (ds->drawFocusType == XmDRAW_FOCUS_NONE)
		return;
	if (ds->drawFocusType == XmDRAW_FOCUS_LEFT)
		drawRight = 0;
	else if (ds->drawFocusType == XmDRAW_FOCUS_RIGHT)
		drawLeft = 0;
	else if (ds->drawFocusType == XmDRAW_FOCUS_MID)
		{
		drawLeft = 0;
		drawRight = 0;
		}
	if (!drawLeft)
		loff = 0;
	else
		loff = 2;
	if (!drawRight)
		roff = 0;
	else
		roff = 2;
	if (g->grid.highlightThickness < 2)
		return;
	XSetForeground(dpy, gc, g->manager.highlight_color);
	if (drawTop)
		XDrawLine(dpy, win, gc, x1, y1, x2, y1);
	if (drawLeft)
		XDrawLine(dpy, win, gc, x1, y1 + 2, x1, y2);
	if (drawRight)
		XDrawLine(dpy, win, gc, x2, y1 + 2, x2, y2);
	if (drawBot)
		XDrawLine(dpy, win, gc, x1 + loff, y2, x2 - roff, y2);
	if (drawTop && clipRect->height > 1)
		XDrawLine(dpy, win, gc, x1, y1 + 1, x2, y1 + 1);
	if (drawBot && (int)clipRect->height > 1 &&
		(int)clipRect->width > roff &&
		(int)clipRect->width > loff)
		XDrawLine(dpy, win, gc, x1 + loff, y2 - 1, x2 - roff, y2 - 1);
	if (clipRect->width > 1 && clipRect->height > 2)
		{
		if (drawLeft)
			XDrawLine(dpy, win, gc, x1 + 1, y1 + 2, x1 + 1, y2);
		if (drawRight)
			XDrawLine(dpy, win, gc, x2 - 1, y1 + 2, x2 - 1, y2);
		}
	}

/* Only to be called by XmLGridCellAction() */
void
_XmLGridCellDrawValue(XmLGridCell cell,
		      Widget w,
		      XRectangle *clipRect,
		      XmLGridDrawStruct *ds)
	{
	XmLGridWidget g;
	Display *dpy;
	XmLGridCellPixmap *pix;
	XmLGridCellIcon *icon;
	XRectangle cellRect;
	XmLGridCellRefValues *cellValues;
	int type, horizMargin, vertMargin, xoff;

	g = (XmLGridWidget)w;
	cellRect = *ds->cellRect;
	cellValues = cell->cell.refValues;
	horizMargin = ds->leftMargin + ds->rightMargin; 
	vertMargin = ds->topMargin + ds->bottomMargin;
	if (horizMargin >= (int)cellRect.width ||
		vertMargin >= (int)cellRect.height)
		return;
	type = cellValues->type;
	cellRect.x += ds->leftMargin;
	cellRect.y += ds->topMargin;
	cellRect.width -= horizMargin;
	cellRect.height -= vertMargin;
	dpy = XtDisplay(w);
	if (type == XmSTRING_CELL && XmLGridCellIsValueSet(cell) == True)
		{
		if (ds->drawSelected == True)
			XSetForeground(dpy, ds->gc, ds->selectForeground);
		else
			XSetForeground(dpy, ds->gc, ds->foreground);
		XmLStringDraw(w, (XmString)cell->cell.value, ds->stringDirection,
			ds->fontList, ds->alignment, ds->gc,
			&cellRect, clipRect);
		}
	if (type == XmPIXMAP_CELL && XmLGridCellIsValueSet(cell) == True)
		{
		pix = (XmLGridCellPixmap *)cell->cell.value;
		XmLPixmapDraw(w, pix->pixmap, pix->pixmask,
			pix->width, pix->height, ds->alignment,
			ds->gc, &cellRect, clipRect);
		}
	if (type == XmICON_CELL && XmLGridCellIsValueSet(cell) == True)
		{
		icon = (XmLGridCellIcon *)cell->cell.value;
		if (icon->pix.pixmap != XmUNSPECIFIED_PIXMAP)
			{
			XmLPixmapDraw(w, icon->pix.pixmap, icon->pix.pixmask,
				icon->pix.width, icon->pix.height, ds->alignment,
				ds->gc, &cellRect, clipRect);
			xoff = icon->pix.width + XmLICON_SPACING;
			cellRect.x += xoff;
			if (xoff < (int)cellRect.width)
				cellRect.width -= xoff;
			else
				cellRect.width = 0;
			}
		if (cellRect.width && icon->string)
			{
			if (ds->drawSelected == True)
				XSetForeground(dpy, ds->gc, ds->selectForeground);
			else
				XSetForeground(dpy, ds->gc, ds->foreground);
			XmLStringDraw(w, icon->string, ds->stringDirection,
				ds->fontList, ds->alignment, ds->gc,
				&cellRect, clipRect);
			}
		}
	if (type == XmTOGGLE_CELL)
		XmLDrawToggle(w, XmLGridCellGetToggle(cell),
			g->grid.toggleSize, ds->alignment, ds->gc,
			ds->background, g->grid.toggleTopColor,
			g->grid.toggleBotColor, ds->foreground,
			&cellRect, clipRect);
	}

/* Only to be called by XmLGridCellAction() */
static int
_XmLGridCellConfigureText(XmLGridCell cell, Widget w, XRectangle *rect)
	{
	XmLGridWidget g;

	g = (XmLGridWidget)w;
	XtConfigureWidget(g->grid.text, rect->x, rect->y,
		rect->width, rect->height, 0);
	return(0);
	}

/* Only to be called by XmLGridCellAction() */
static int
_XmLGridCellBeginTextEdit(XmLGridCell cell,
			  Widget w,
			  XRectangle *clipRect)
	{
        XmLGridWidget g = (XmLGridWidget)w;
	Widget text;

	if (cell->cell.refValues->type != XmSTRING_CELL ||
		cell->cell.refValues->editable != True
	    || g->grid.useTextWidget != True)
		return 0;
	XtVaGetValues(w,
		XmNtextWidget, &text,
		NULL);
	XtVaSetValues(text,
		XmNfontList, cell->cell.refValues->fontList,
		NULL);
	return 1;
	}

/* Only to be called by XmLGridCellAction() */
static void
_XmLGridCellCompleteTextEdit(XmLGridCell cell,
			     Widget w)
	{
        XmLGridWidget g = (XmLGridWidget)w;
	Widget text;
	char *s;
	int i, len;

	if (cell->cell.refValues->type != XmSTRING_CELL ||
		cell->cell.refValues->editable != True 
	    || g->grid.useTextWidget != True)
		return;
	XtVaGetValues(w,
		XmNtextWidget, &text,
		NULL);
	s = XmTextGetString(text);
	len = strlen(s);
	for (i = 0; i < len - 1; i++)
		if (s[i] == '\\' && s[i + 1] == 'n')
			{
			s[i] = '\n';
			strcpy(&s[i + 1], &s[i + 2]);
			}
	if (XmLGridCellIsValueSet(cell) == True)
		XmStringFree((XmString)cell->cell.value);
	if (strlen(s))
		{
		cell->cell.value = (void *)XmStringCreateLtoR(s,
			XmSTRING_DEFAULT_CHARSET);
		XmLGridCellSetValueSet(cell, True);
		}
	else
		XmLGridCellSetValueSet(cell, False);
	XtFree(s);
	}

/* Only to be called by XmLGridCellAction() */
static void
_XmLGridCellInsertText(XmLGridCell cell,
		       Widget w)
	{
        XmLGridWidget g = (XmLGridWidget)w;
	char *s;
	Widget text;

	if (cell->cell.refValues->type != XmSTRING_CELL ||
		cell->cell.refValues->editable != True
	    || g->grid.useTextWidget != True)
		return;
	XtVaGetValues(w,
		XmNtextWidget, &text,
		NULL);
	s = 0;
	if (XmLGridCellIsValueSet(cell) == True)
		s = CvtXmStringToStr((XmString)cell->cell.value);
	if (s)
		{
		XmTextSetString(text, s);
		free(s);
		}
	else
		XmTextSetString(text, "");
	XmTextSetInsertionPosition(text, XmTextGetLastPosition(text));
	XmProcessTraversal(text, XmTRAVERSE_CURRENT);
	}

/* Only to be called by XmLGridCellAction() */
static int
_XmLGridCellGetHeight(XmLGridCell cell,
		      XmLGridRow row)
	{
	XmLGridCellPixmap *pix;
	XmLGridCellIcon *icon;
	XmLGridCellRefValues *cellValues;
	int height, pixHeight;

	cellValues = cell->cell.refValues;
	height = -1;
	if (cellValues->type == XmPIXMAP_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == True)
			{
			pix = (XmLGridCellPixmap *)cell->cell.value;
			if (pix->height)
				height = 4 + pix->height + cellValues->topMargin +
					cellValues->bottomMargin;
			}
		}
	else if (cellValues->type == XmICON_CELL)
		{
		pixHeight = 0;
		if (XmLGridCellIsValueSet(cell) == True)
			{
			icon = (XmLGridCellIcon *)cell->cell.value;
			if (icon->pix.pixmap != XmUNSPECIFIED_PIXMAP && icon->pix.height)
				pixHeight = 4 + icon->pix.height +
					cellValues->topMargin + cellValues->bottomMargin;
			}
		height = 4 + row->grid.height * cellValues->fontHeight +
			cellValues->topMargin + cellValues->bottomMargin;
		if (pixHeight > height)
			height = pixHeight;
		}
	else if (cellValues->type == XmTOGGLE_CELL)
		height = 4 + 16 + cellValues->topMargin +
			cellValues->bottomMargin;
	else if (cellValues->type == XmSTRING_CELL)
		{
		height = 4 + row->grid.height * cellValues->fontHeight +
			cellValues->topMargin + cellValues->bottomMargin;
		}
	if (height < 0 || cellValues->rowSpan)
		height = 4;
	return height;
	}

/* Only to be called by XmLGridCellAction() */
static int
_XmLGridCellGetWidth(XmLGridCell cell,
		     XmLGridColumn col)
	{
	XmLGridCellPixmap *pix;
	XmLGridCellIcon *icon;
	XmLGridCellRefValues *cellValues;
	int width;

	cellValues = cell->cell.refValues;
	width = -1;
	if (cellValues->type == XmPIXMAP_CELL)
		{
		if (XmLGridCellIsValueSet(cell) == True)
			{
			pix = (XmLGridCellPixmap *)cell->cell.value;
			if (pix->width)
				width = 4 + pix->width + cellValues->leftMargin +
					cellValues->rightMargin;
			}
		}
	else if (cellValues->type == XmICON_CELL)
		{
		width = 4 + col->grid.width * cellValues->fontWidth +
			cellValues->leftMargin + cellValues->rightMargin;
		if (XmLGridCellIsValueSet(cell) == True)
			{
			icon = (XmLGridCellIcon *)cell->cell.value;
			if (icon->pix.pixmap != XmUNSPECIFIED_PIXMAP && icon->pix.width)
				width += icon->pix.width + XmLICON_SPACING;
			}
		}
	else if (cellValues->type == XmTOGGLE_CELL)
		width = 4 + 16 + cellValues->leftMargin +
			cellValues->rightMargin;
	else if (cellValues->type == XmSTRING_CELL)
		width = 4 + col->grid.width * cellValues->fontWidth +
			cellValues->leftMargin + cellValues->rightMargin;
	if (width < 0 || cellValues->columnSpan)
		width = 4;
	return width;
	}

/* Only to be called by XmLGridCellAction() */
static void
_XmLGridCellFreeValue(XmLGridCell cell)
	{
	int type;
	XmLGridCellIcon *icon;

	if (XmLGridCellIsValueSet(cell) == False)
		return;
	type = cell->cell.refValues->type;
	if (type == XmSTRING_CELL)
		XmStringFree((XmString)cell->cell.value);
	else if (type == XmPIXMAP_CELL)
		free((char *)cell->cell.value);
	else if (type == XmICON_CELL)
		{
		icon = (XmLGridCellIcon *)cell->cell.value;
		if (icon->string)
			XmStringFree(icon->string);
		free((char *)icon);
		}
	XmLGridCellSetValueSet(cell, False);
	}

/*
   Public Functions
*/

Widget
XmLCreateGrid(Widget parent,
	      char *name,
	      ArgList arglist,
	      Cardinal argcount)
	{
	return XtCreateWidget(name, xmlGridWidgetClass, parent,
		arglist, argcount);
	}

void
XmLGridAddColumns(Widget w,
		  unsigned char type,
		  int position,
		  int count)
	{
	XmLGridWidget g;
	XmLGridColumn col;
	XmLGridRow row;
	XmLGridCell cell;
	XmLGridCallbackStruct cbs;
	int i, j, rowCount, hasAddCB, redraw;

	g = WidgetToGrid(w, "AddColumns()");
	if (!g)
		return;
	if (count <= 0)
		return;
	redraw = 0;
	position = ColTypePosToPos(g, type, position, 1);
	if (position < 0)
		position = ColTypePosToPos(g, type, -1, 1);
	/* adjust count */
	if (type == XmHEADING)
		{
		g->grid.headingColCount += count;
		g->grid.leftFixedCount += count;
		redraw = 1;
		}
	else if (type == XmFOOTER)
		{
		g->grid.footerColCount += count;
		g->grid.rightFixedCount += count;
		redraw = 1;
		}
	else /* XmCONTENT */
		g->grid.colCount += count;

	hasAddCB = 0;
	if (XtHasCallbacks(w, XmNaddCallback) == XtCallbackHasSome)
		hasAddCB = 1;
	/* add columns */
	XmLArrayAdd(g->grid.colArray, position, count);
	for (i = 0; i < count; i++)
		{
		col = 0;
		if (hasAddCB)
			{
			cbs.reason = XmCR_ADD_COLUMN;
			cbs.columnType = type;
			cbs.object = 0;
			XtCallCallbackList(w, g->grid.addCallback, (XtPointer)&cbs);
			col = (XmLGridColumn)cbs.object;
			}
		if (!col)
			col = XmLGridColumnNew(w);
		XmLArraySet(g->grid.colArray, position + i, col);
		}
	/* add cells */
	rowCount = XmLArrayGetCount(g->grid.rowArray);
	for (j = 0; j < rowCount; j++)
		{
		row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, j);
		XmLArrayAdd(XmLGridRowCells(row), position, count);
		for (i = position; i < position + count; i++)
			{
			cell = 0;
			if (hasAddCB)
				{
				cbs.reason = XmCR_ADD_CELL;
				cbs.rowType = RowPosToType(g, j);
				cbs.columnType = type;
				cbs.object = 0;
				XtCallCallbackList(w, g->grid.addCallback, (XtPointer)&cbs);
				cell = (XmLGridCell)cbs.object;
				}
			if (!cell)
				cell = XmLGridCellNew();
			col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, i);
			if (col->grid.defCellValues)
				XmLGridCellSetRefValues(cell, col->grid.defCellValues);
			else
				XmLGridCellSetRefValues(cell, g->grid.defCellValues);
			XmLArraySet(XmLGridRowCells(row), i, cell);
			}
		XmLGridRowHeightChanged(row);
		}
	for (i = position; i < position + count; i++)
		{
		col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, i);
		XmLGridColumnWidthChanged(col);
		}
	/* sanity check */
	if (XmLArrayGetCount(g->grid.colArray) != g->grid.headingColCount +
		g->grid.colCount + g->grid.footerColCount)
			XmLWarning(w, "AddColumns() - count sanity check failed");

	/* maintain scroll and focus position */
	if (g->grid.hsPolicy == XmCONSTANT)
		{
		if (type == XmCONTENT && g->grid.colCount == count)
			g->grid.scrollCol = 0;
		else if (position <= g->grid.scrollCol)
			g->grid.scrollCol += count;
		}
	if (position <= g->grid.focusCol)
		g->grid.focusCol += count;
	VisPosChanged(g, 0);
	HorizLayout(g, 1);
	VertLayout(g, 1);
	if (g->grid.focusCol == -1 && type == XmCONTENT)
		{
		if (g->grid.focusRow != -1)
			SetFocus(g, g->grid.focusRow, position, 0, 0);
		else
			g->grid.focusCol = position;
		}	
	for (i = position; i < position + count; i++)
		redraw |= ColIsVisible(g, i);
	if (redraw)
		DrawArea(g, DrawAll, 0, 0);
	}

void
XmLGridAddRows(Widget w,
	       unsigned char type,
	       int position,
	       int count)
	{
	XmLGridWidget g;
	XmLGridRow row;
	XmLGridColumn col;
	XmLGridCell cell;
	XmLGridCallbackStruct cbs;
	int i, j, hasAddCB, redraw, colCount;

	g = WidgetToGrid(w, "AddRows()");
	if (!g)
		return;
	if (count <= 0)
		return;
	redraw = 0;
	position = RowTypePosToPos(g, type, position, 1);
	if (position < 0)
		position = RowTypePosToPos(g, type, -1, 1);
	/* adjust count */
	if (type == XmHEADING)
		{
		g->grid.headingRowCount += count;
		g->grid.topFixedCount += count;
		redraw = 1;
		}
	else if (type == XmFOOTER)
		{
		g->grid.footerRowCount += count;
		g->grid.bottomFixedCount += count;
		redraw = 1;
		}
	else /* XmCONTENT */
		g->grid.rowCount += count;

	/* add rows and cells */
	XmLArrayAdd(g->grid.rowArray, position, count);
	colCount = XmLArrayGetCount(g->grid.colArray);
	hasAddCB = 0;
	if (XtHasCallbacks(w, XmNaddCallback) == XtCallbackHasSome)
		hasAddCB = 1;
	for (i = 0; i < count; i++)
		{
		row = 0;
		if (hasAddCB)
			{
			cbs.reason = XmCR_ADD_ROW;
			cbs.rowType = type;
			cbs.object = 0;
			XtCallCallbackList(w, g->grid.addCallback, (XtPointer)&cbs);
			row = (XmLGridRow)cbs.object;
			}
		if (!row)
			row = XmLGridRowNew(w);
		XmLArraySet(g->grid.rowArray, position + i, row);
		XmLArrayAdd(XmLGridRowCells(row), 0, colCount);
		for (j = 0; j < colCount; j++)
			{
			cell = 0;
			if (hasAddCB)
				{
				cbs.reason = XmCR_ADD_CELL;
				cbs.rowType = type;
				cbs.columnType = ColPosToType(g, j);
				cbs.object = 0;
				XtCallCallbackList(w, g->grid.addCallback, (XtPointer)&cbs);
				cell = (XmLGridCell)cbs.object;
				}
			if (!cell)
				cell = XmLGridCellNew();
			col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, j);
			if (col->grid.defCellValues)
				XmLGridCellSetRefValues(cell, col->grid.defCellValues);
			else
				XmLGridCellSetRefValues(cell, g->grid.defCellValues);
			XmLArraySet(XmLGridRowCells(row), j, cell);
			}
		XmLGridRowHeightChanged(row);
		}
	for (j = 0; j < colCount; j++)
		{
		col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, j);
		XmLGridColumnWidthChanged(col);
		}
	/* sanity check */
	if (XmLArrayGetCount(g->grid.rowArray) != g->grid.headingRowCount +
		g->grid.rowCount + g->grid.footerRowCount)
			XmLWarning(w, "AddRows() - count sanity check failed");

	/* maintain scroll and focus position */
	if (g->grid.vsPolicy == XmCONSTANT)
		{
		if (type == XmCONTENT && g->grid.rowCount == count)
			g->grid.scrollRow = 0;
		else if (position <= g->grid.scrollRow)
			g->grid.scrollRow += count;
		}
	if (position <= g->grid.focusRow)
		g->grid.focusRow += count;
	VisPosChanged(g, 1);
	HorizLayout(g, 1);
	VertLayout(g, 1);
	if (g->grid.focusRow == -1 && type == XmCONTENT)
		{
		if (g->grid.focusCol != -1)
			SetFocus(g, position, g->grid.focusCol, 0, 0);
		else
			g->grid.focusRow = position;
		}
	for (i = position; i < position + count; i++)
		redraw |= RowIsVisible(g, i);
	if (redraw)
		DrawArea(g, DrawAll, 0, 0);

#ifdef XmLEVAL
	if (g->grid.rowCount > 100)
		{
		fprintf(stderr, "XmL: evaluation version only supports <= 100 rows\n");
		exit(0);
		}
#endif
	}

void
XmLGridDeleteAllColumns(Widget w,
			unsigned char type)
	{
	XmLGridWidget g;
	int n;

	g = WidgetToGrid(w, "DeleteAllColumns()");
	if (!g)
		return;
	if (type == XmHEADING)
		n = g->grid.headingColCount;
	else if (type == XmCONTENT)
		n = g->grid.colCount;
	else if (type == XmFOOTER)
		n = g->grid.footerColCount;
	else
		{
		XmLWarning(w, "DeleteAllColumns() - invalid type");
		return;
		}
	if (!n)
		return;
	XmLGridDeleteColumns(w, type, 0, n);
	}

void
XmLGridDeleteAllRows(Widget w,
		     unsigned char type)
	{
	XmLGridWidget g;
	int n;

	g = WidgetToGrid(w, "DeleteAllRows()");
	if (!g)
		return;
	if (type == XmHEADING)
		n = g->grid.headingRowCount;
	else if (type == XmCONTENT)
		n = g->grid.rowCount;
	else if (type == XmFOOTER)
		n = g->grid.footerRowCount;
	else
		{
		XmLWarning(w, "DeleteAllRows() - invalid type");
		return;
		}
	if (!n)
		return;
	XmLGridDeleteRows(w, type, 0, n);
	}

void
XmLGridDeselectAllRows(Widget w,
		       Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "DeselectAllRows()");
	if (!g)
		return;
	SelectTypeArea(g, SelectRow, (XEvent *)0, -1, 0, False, notify);
	}

void
XmLGridDeselectAllColumns(Widget w,
			  Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "DeselectAllColumns()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCol, (XEvent *)0, 0, -1, False, notify);
	}

void
XmLGridDeselectAllCells(Widget w,
			Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "DeselectAllCells()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCell, (XEvent *)0, -1, -1, False, notify);
	}

void
XmLGridDeselectCell(Widget w,
		    int row,
		    int column,
		    Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "DeselectCell()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCell, (XEvent *)0, row, column, False, notify);
	}

void
XmLGridDeselectColumn(Widget w,
		      int column,
		      Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "DeselectColumn()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCol, (XEvent *)0, 0, column, False, notify);
	}

void
XmLGridDeselectRow(Widget w,
		   int row,
		   Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "DeselectRow()");
	if (!g)
		return;
	SelectTypeArea(g, SelectRow, (XEvent *)0, row, 0, False, notify);
	}

void
XmLGridDeleteColumns(Widget w,
		     unsigned char type,
		     int position,
		     int count)
	{
	XmLGridWidget g;
	XmLGridRow row;
	XmLGridColumn col;
	XmLGridCallbackStruct cbs;
	int changeFocus, i, j, rowCount, lastPos, redraw;

	g = WidgetToGrid(w, "DeleteColumns()");
	if (!g)
		return;
	if (count <= 0)
		return;
	lastPos = ColTypePosToPos(g, type, position + count - 1, 0);
	position = ColTypePosToPos(g, type, position, 0);
	if (position < 0 || lastPos < 0)
		{
		XmLWarning(w, "DeleteColumns() - invalid position");
		return;
		}
	changeFocus = 0;
	if (position <= g->grid.focusCol && lastPos >= g->grid.focusCol)
		{
		/* deleting focus col */
		TextAction(g, TEXT_EDIT_CANCEL);
		ChangeFocus(g, g->grid.focusRow, -1);
		changeFocus = 1;
		}
	redraw = 0;

	/* adjust count */
	if (type == XmHEADING)
		{
		g->grid.headingColCount -= count;
		g->grid.leftFixedCount -= count;
		redraw = 1;
		}
	else if (type == XmFOOTER)
		{
		g->grid.footerColCount -= count;
		g->grid.rightFixedCount -= count;
		redraw = 1;
		}
	else /* XmCONTENT */
		g->grid.colCount -= count;

	for (i = position; i < position + count; i++)
		{
		col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, i);
		if (XmLGridColumnIsHidden(col) == True)
			g->grid.hiddenColCount--;
		redraw |= ColIsVisible(g, i);
		}
	if (XtHasCallbacks(w, XmNdeleteCallback) == XtCallbackHasSome)
		for (i = position; i < position + count; i++)
			{
			rowCount = XmLArrayGetCount(g->grid.rowArray);
			for (j = 0; j < rowCount; j++)
				{
				row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, j);
				cbs.reason = XmCR_DELETE_CELL;
				cbs.rowType = RowPosToType(g, j);
				cbs.columnType = type;
				cbs.object = XmLArrayGet(XmLGridRowCells(row), i);
				XtCallCallbackList(w, g->grid.deleteCallback, (XtPointer)&cbs);
				}
			cbs.reason = XmCR_DELETE_COLUMN;
			cbs.columnType = type;
			cbs.object = XmLArrayGet(g->grid.colArray, i);
			XtCallCallbackList(w, g->grid.deleteCallback, (XtPointer)&cbs);
			}
	/* delete columns */
	for (i = position; i < position + count; i++)
		{
		col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, i);
		XmLGridColumnFree(w, col);
		}
	XmLArrayDel(g->grid.colArray, position, count);
	/* delete cells */
	rowCount = XmLArrayGetCount(g->grid.rowArray);
	for (i = 0; i < rowCount; i++)
		{
		row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, i);
		for (j = position; j < position + count; j++)
			XmLGridCellFree(w,
				(XmLGridCell)XmLArrayGet(XmLGridRowCells(row), j));
		XmLArrayDel(XmLGridRowCells(row), position, count);
		XmLGridRowHeightChanged(row);
		}
	/* sanity check */
	if (XmLArrayGetCount(g->grid.colArray) != g->grid.headingColCount +
		g->grid.colCount + g->grid.footerColCount)
			XmLWarning(w, "DeleteColumns() - count sanity check failed");

	/* maintain scroll and focus position */
	if (g->grid.hsPolicy == XmCONSTANT)
		{
		if (lastPos < g->grid.scrollCol)
			g->grid.scrollCol -= count;
		else if (position <= g->grid.scrollCol)
			g->grid.scrollCol = position;
		}
	if (lastPos < g->grid.focusCol)
		g->grid.focusCol -= count;
	VisPosChanged(g, 0);
	HorizLayout(g, 1);
	VertLayout(g, 1);
	if (changeFocus)
		{
		SetFocus(g, g->grid.focusRow, position, 0, 1);
		if (g->grid.focusCol == -1)
			SetFocus(g, g->grid.focusRow, position, 0, -1);
		}
	if (redraw)
		DrawArea(g, DrawAll, 0, 0);
	}

void
XmLGridDeleteRows(Widget w,
		  unsigned char type,
		  int position,
		  int count)
	{
	XmLGridWidget g;
	XmLGridRow row;
	XmLGridColumn col;
	XmLGridCallbackStruct cbs;
	int changeFocus, i, j, colCount, lastPos, redraw;

	g = WidgetToGrid(w, "DeleteRows()");
	if (!g)
		return;
	if (count <= 0)
		return;
	lastPos = RowTypePosToPos(g, type, position + count - 1, 0);
	position = RowTypePosToPos(g, type, position, 0);
	if (position < 0 || lastPos < 0)
		{
		XmLWarning(w, "DeleteRows() - invalid position");
		return;
		}
	changeFocus = 0;
	if (position <= g->grid.focusRow && lastPos >= g->grid.focusRow)
		{
		/* deleting focus row */
		TextAction(g, TEXT_EDIT_CANCEL);
		ChangeFocus(g, -1, g->grid.focusCol);
		changeFocus = 1;
		}
	redraw = 0;

	/* adjust count */
	if (type == XmHEADING)
		{
		g->grid.headingRowCount -= count;
		g->grid.topFixedCount -= count;
		redraw = 1;
		}
	else if (type == XmFOOTER)
		{
		g->grid.footerRowCount -= count;
		g->grid.bottomFixedCount -= count;
		redraw = 1;
		}
	else /* XmCONTENT */
		g->grid.rowCount -= count;

	for (i = position; i < position + count; i++)
		{
		row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, i);
		if (XmLGridRowIsHidden(row) == True)
			g->grid.hiddenRowCount--;
		redraw |= RowIsVisible(g, i);
		}
	if (XtHasCallbacks(w, XmNdeleteCallback) == XtCallbackHasSome)
		for (i = position; i < position + count; i++)
			{
			row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, i);
			colCount = XmLArrayGetCount(g->grid.colArray);
			for (j = 0; j < colCount; j++)
				{
				cbs.reason = XmCR_DELETE_CELL;
				cbs.rowType = type;
				cbs.columnType = ColPosToType(g, j);
				cbs.object = XmLArrayGet(XmLGridRowCells(row), j);
				XtCallCallbackList(w, g->grid.deleteCallback, (XtPointer)&cbs);
				}
			cbs.reason = XmCR_DELETE_ROW;
			cbs.rowType = type;
			cbs.object = (void *)row;
			XtCallCallbackList(w, g->grid.deleteCallback, (XtPointer)&cbs);
			}
	/* delete rows and cells */
	for (i = position; i < position + count; i++)
		{
		row = (XmLGridRow)XmLArrayGet(g->grid.rowArray, i);
		XmLGridRowFree(w, row);
		}
	XmLArrayDel(g->grid.rowArray, position, count);
	colCount = XmLArrayGetCount(g->grid.colArray);
	for (i = 0; i < colCount; i++)
		{
		col = (XmLGridColumn)XmLArrayGet(g->grid.colArray, i);
		XmLGridColumnWidthChanged(col);
		}
	/* sanity check */
	if (XmLArrayGetCount(g->grid.rowArray) != g->grid.headingRowCount +
		g->grid.rowCount + g->grid.footerRowCount)
			XmLWarning(w, "DeleteRows() - count sanity check failed");

	/* maintain scroll and focus position */
	if (g->grid.vsPolicy == XmCONSTANT)
		{
		if (lastPos < g->grid.scrollRow)
			g->grid.scrollRow -= count;
		else if (position <= g->grid.scrollRow)
			g->grid.scrollRow = position;
		}
	if (lastPos < g->grid.focusRow)
		g->grid.focusRow -= count;
	VisPosChanged(g, 1);
	HorizLayout(g, 1);
	VertLayout(g, 1);
	if (changeFocus)
		{
		SetFocus(g, position, g->grid.focusCol, 1, 0);
		if (g->grid.focusRow == -1)
			SetFocus(g, position, g->grid.focusCol, -1, 0);
		}
	if (redraw)
		DrawArea(g, DrawAll, 0, 0);
	}

void
XmLGridGetFocus(Widget w,
		int *row,
		int *column,
		Boolean *focusIn)
	{
	XmLGridWidget g;
	unsigned char rowType, colType;

	g = WidgetToGrid(w, "GetFocus()");
	if (!g)
		return;
	if (g->grid.focusIn)
		*focusIn = True;
	else
		*focusIn = False;
	if (g->grid.focusRow < 0 || g->grid.focusCol < 0)
		{
		*row = -1;
		*column = -1;
		return;
		}
	rowType = RowPosToType(g, g->grid.focusRow);
	colType = ColPosToType(g, g->grid.focusCol);
	if (rowType != XmCONTENT || colType != XmCONTENT)
		{
		*row = -1;
		*column = -1;
		XmLWarning(w, "GetFocus() - focus row/column invalid\n");
		return;
		}
	*row = RowPosToTypePos(g, XmCONTENT, g->grid.focusRow);
	*column = ColPosToTypePos(g, XmCONTENT, g->grid.focusCol);
	}

XmLGridRow
XmLGridGetRow(Widget w,
	      unsigned char rowType,
	      int row)

	{
	XmLGridWidget g;
	XmLGridRow r;
	int pos;

	g = WidgetToGrid(w, "GetRow()");
	if (!g)
		return 0;
	pos = RowTypePosToPos(g, rowType, row, 0);
	r = (XmLGridRow)XmLArrayGet(g->grid.rowArray, pos);
	if (!r)
		XmLWarning(w, "GetRow() - invalid row");
	return r;
	}

int
XmLGridEditBegin(Widget w,
		 Boolean insert,
		 int row,
		 int column)
	{
	XmLGridWidget g;
	int r, c;
	XRectangle rect;

	g = WidgetToGrid(w, "EditBegin()");
	if (!g)
		return -1;
	if (column < 0 || column >= g->grid.colCount) 
		{
		XmLWarning(w, "EditBegin() - invalid column");
		return -1;
		}
	if (row < 0 || row >= g->grid.rowCount) 
		{
		XmLWarning(w, "EditBegin() - invalid row");
		return -1;
		}
	r = RowTypePosToPos(g, XmCONTENT, row, 0);
	c = ColTypePosToPos(g, XmCONTENT, column, 0);
	if (RowColToXY(g, r, c, True, &rect) == -1)
		{
		XmLWarning(w, "EditBegin() - cell must be visible to begin edit");
		return -1;
		}
	if (SetFocus(g, r, c, 0, 0) == -1)
		{
		XmLWarning(w, "EditBegin() - can't set focus to cell");
		return -1;
		}
	if (insert == False)
		TextAction(g, TEXT_EDIT);
	else 
		TextAction(g, TEXT_EDIT_INSERT);
	return 0;
	}

void
XmLGridEditCancel(Widget w)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "EditCancel()");
	if (!g)
		return;
	TextAction(g, TEXT_EDIT_CANCEL);
	}

void
XmLGridEditComplete(Widget w)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "EditComplete()");
	if (!g)
		return;
	TextAction(g, TEXT_EDIT_COMPLETE);
	}

XmLGridColumn
XmLGridGetColumn(Widget w,
		 unsigned char columnType,
		 int column)
	{
	XmLGridWidget g;
	XmLGridColumn c;
	int pos;

	g = WidgetToGrid(w, "GetColumn()");
	if (!g)
		return 0;
	pos = ColTypePosToPos(g, columnType, column, 0);
	c = (XmLGridColumn)XmLArrayGet(g->grid.colArray, pos);
	if (!c)
		XmLWarning(w, "GetColumn() - invalid column");
	return c;
	}

int
XmLGridGetSelectedCellCount(Widget w)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "GetSelectedCellCount()");
	if (!g)
		return -1;
	return GetSelectedArea(g, SelectCell, 0, 0, 0);
	}

int
XmLGridGetSelectedCells(Widget w,
			int *rowPositions,
			int *columnPositions,
			int count)
	{
	XmLGridWidget g;
	int n;

	g = WidgetToGrid(w, "GetSelectedCells()");
	if (!g)
		return -1;
	n = GetSelectedArea(g, SelectCell, rowPositions, columnPositions, count);
	if (count != n)
		{
		XmLWarning(w, "GetSelectedCells() - count is incorrect");
		return -1;
		}
	return 0;
	}

int
XmLGridGetSelectedColumnCount(Widget w)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "GetSelectedColumnCount()");
	if (!g)
		return -1;
	return GetSelectedArea(g, SelectCol, 0, 0, 0);
	}

int
XmLGridGetSelectedColumns(Widget w,
			  int *positions,
			  int count)
	{
	XmLGridWidget g;
	int n;

	g = WidgetToGrid(w, "GetSelectedColumns()");
	if (!g)
		return -1;
	n = GetSelectedArea(g, SelectCol, 0, positions, count);
	if (count != n)
		{
		XmLWarning(w, "GetSelectedColumns() - count is incorrect");
		return -1;
		}
	return 0;
	}

int
XmLGridGetSelectedRow(Widget w)
	{
	XmLGridWidget g;
	int n, pos;

	g = WidgetToGrid(w, "GetSelectedRow()");
	if (!g)
		return -1;
	n = GetSelectedArea(g, SelectRow, &pos, 0, 1);
	if (n != 1)
		return -1;
	return pos;
	}

int
XmLGridGetSelectedRowCount(Widget w)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "GetSelectedRowCount()");
	if (!g)
		return -1;
	return GetSelectedArea(g, SelectRow, 0, 0, 0);
	}

int
XmLGridGetSelectedRows(Widget w,
		       int *positions,
		       int count)
	{
	XmLGridWidget g;
	int n;

	g = WidgetToGrid(w, "GetSelectedRows()");
	if (!g)
		return -1;
	n = GetSelectedArea(g, SelectRow, positions, 0, count);
	if (count != n)
		{
		XmLWarning(w, "GetSelectedRows() - count is incorrect");
		return -1;
		}
	return 0;
	}

Boolean
XmLGridColumnIsVisible(Widget w,
		       int col)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "ColumnIsVisible()");
	if (!g)
		return -1;
	if (!ColIsVisible(g, g->grid.headingColCount + col))
		return False;
	return True;
	}

void
XmLGridMoveColumns(Widget w,
		   int newPosition,
		   int position,
		   int count)
	{
	XmLGridWidget g;
	XmLGridRow row;
	int i, np, p, rowCount;

	g = WidgetToGrid(w, "MoveColumns()");
	if (!g)
		return;
	np = ColTypePosToPos(g, XmCONTENT, newPosition, 0);
	p = ColTypePosToPos(g, XmCONTENT, position, 0);
	if (XmLArrayMove(g->grid.colArray, np, p, count) == -1)
		{
		XmLWarning(w, "MoveColumns() - invalid position");
		return;
		}
	rowCount = XmLArrayGetCount(g->grid.rowArray);
	for (i = 0; i < rowCount; i++)
		{
		row = XmLArrayGet(g->grid.rowArray, i);
		XmLArrayMove(XmLGridRowCells(row), np, p, count);
		}
	VisPosChanged(g, 0);
	HorizLayout(g, 1);
	DrawArea(g, DrawAll, 0, 0);
	}

void
XmLGridMoveRows(Widget w,
		int newPosition,
		int position,
		int count)
	{
	XmLGridWidget g;
	int np, p;

	g = WidgetToGrid(w, "MoveRows()");
	if (!g)
		return;
	np = RowTypePosToPos(g, XmCONTENT, newPosition, 0);
	p = RowTypePosToPos(g, XmCONTENT, position, 0);
	if (XmLArrayMove(g->grid.rowArray, np, p, count) == -1)
		{
		XmLWarning(w, "MoveRows() - invalid position/count");
		return;
		}
	VisPosChanged(g, 1);
	VertLayout(g, 1);
	DrawArea(g, DrawAll, 0, 0);
	}

void
XmLGridReorderColumns(Widget w,
		      int *newPositions,
		      int position,
		      int count)
	{
	XmLGridWidget g;
	XmLGridRow row;
	int i, *np, p, rowCount, status;

	g = WidgetToGrid(w, "ReorderColumns()");
	if (!g)
		return;
	if (count <= 0)
		return;
	p = ColTypePosToPos(g, XmCONTENT, position, 0);
	np = (int *)malloc(sizeof(int) * count);
	for (i = 0; i < count; i++)
		np[i] = ColTypePosToPos(g, XmCONTENT, newPositions[i], 0);
	status = XmLArrayReorder(g->grid.colArray, np, p, count);
	if (status < 0)
		{
		free((char *)np);
		XmLWarning(w, "ReorderColumns() - invalid position/count");
		return;
		}
	rowCount = XmLArrayGetCount(g->grid.rowArray);
	for (i = 0; i < rowCount; i++)
		{
		row = XmLArrayGet(g->grid.rowArray, i);
		XmLArrayReorder(XmLGridRowCells(row), np, p, count);
		}
	free((char *)np);
	VisPosChanged(g, 0);
	HorizLayout(g, 1);
	DrawArea(g, DrawAll, 0, 0);
	}

void
XmLGridReorderRows(Widget w,
		   int *newPositions,
		   int position,
		   int count)
	{
	XmLGridWidget g;
	int i, *np, p, status;

	g = WidgetToGrid(w, "ReorderRows()");
	if (!g)
		return;
	if (count <= 0)
		return;
	p = RowTypePosToPos(g, XmCONTENT, position, 0);
	np = (int *)malloc(sizeof(int) * count);
	for (i = 0; i < count; i++)
		np[i] = RowTypePosToPos(g, XmCONTENT, newPositions[i], 0);
	status = XmLArrayReorder(g->grid.rowArray, np, p, count);
	free((char *)np);
	if (status == -1)
		{
		XmLWarning(w, "ReorderRows() - invalid position/count");
		return;
		}
	VisPosChanged(g, 1);
	VertLayout(g, 1);
	DrawArea(g, DrawAll, 0, 0);
	}

int
XmLGridRowColumnToXY(Widget w,
		     unsigned char rowType,
		     int row,
		     unsigned char columnType,
		     int column,
		     Boolean clipped,
		     XRectangle *rect)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "RowColumnToXY()");
	if (!g)
		return -1;
	r = RowTypePosToPos(g, rowType, row, 0);
	c = ColTypePosToPos(g, columnType, column, 0);
	if (r < 0 || r >= XmLArrayGetCount(g->grid.rowArray) ||
		c < 0 || c >= XmLArrayGetCount(g->grid.colArray))
		{
/*		XmLWarning(w, "RowColumnToXY() - invalid position");*/
		return -1;
		}
	return RowColToXY(g, r, c, clipped, rect);
	}

Boolean
XmLGridRowIsVisible(Widget w, int row)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "RowIsVisible()");
	if (!g)
		return -1;
	if (!RowIsVisible(g, g->grid.headingRowCount + row))
		return False;
	return True;
	}

void
XmLGridSelectAllRows(Widget w,
		     Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "SelectAllRows()");
	if (!g)
		return;
	SelectTypeArea(g, SelectRow, (XEvent *)0, -1, 0, True, notify);
	}

void
XmLGridSelectAllColumns(Widget w,
			Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "SelectAllColumns()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCol, (XEvent *)0, 0, -1, True, notify);
	}

void
XmLGridSelectAllCells(Widget w,
		      Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "SelectAllCells()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCell, (XEvent *)0, -1, -1, True, notify);
	}

void
XmLGridSelectCell(Widget w,
		  int row,
		  int column,
		  Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "SelectCell()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCell, (XEvent *)0, row, column, True, notify);
	}

void
XmLGridSelectColumn(Widget w,
		    int column,
		    Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "SelectColumn()");
	if (!g)
		return;
	SelectTypeArea(g, SelectCol, (XEvent *)0, 0, column, True, notify);
	}

void
XmLGridSelectRow(Widget w,
		 int row,
		 Boolean notify)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "SelectRow()");
	if (!g)
		return;
	SelectTypeArea(g, SelectRow, (XEvent *)0, row, 0, True, notify);
	}

int
XmLGridSetFocus(Widget w,
		int row,
		int column)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "SetFocus()");
	if (!g)
		return -1;
	if (row < 0 || row >= g->grid.rowCount)
		{
		XmLWarning(w, "SetFocus() - invalid row");
		return -1;
		}
	if (column < 0 || column >= g->grid.colCount)
		{
		XmLWarning(w, "SetFocus() - invalid column");
		return -1;
		}
	r = RowTypePosToPos(g, XmCONTENT, row, 0);
	c = ColTypePosToPos(g, XmCONTENT, column, 0);
	return SetFocus(g, r, c, 0, 0);
	}

int
XmLGridXYToRowColumn(Widget w,
		     int x,
		     int y,
		     unsigned char *rowType,
		     int *row,
		     unsigned char *columnType,
		     int *column)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "XYToRowColumn()");
	if (!g)
		return -1;
	if (XYToRowCol(g, x, y, &r, &c) == -1)
		return -1;
	*rowType = RowPosToType(g, r);
	*row = RowPosToTypePos(g, *rowType, r);
	*columnType = ColPosToType(g, c);
	*column = ColPosToTypePos(g, *columnType, c);
	return 0;
	}

void
XmLGridRedrawAll(Widget w)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "RedrawAll()");
	if (!g)
		return;
	DrawArea(g, DrawAll, 0, 0);
	}

void
XmLGridRedrawCell(Widget w,
		  unsigned char rowType,
		  int row,
		  unsigned char columnType,
		  int column)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "RedrawCell()");
	if (!g)
		return;
	r = RowTypePosToPos(g, rowType, row, 0);
	c = ColTypePosToPos(g, columnType, column, 0);
	DrawArea(g, DrawCell, r, c);
	}

void
XmLGridRedrawColumn(Widget w,
		    unsigned char type,
		    int column)
	{
	XmLGridWidget g;
	int c;

	g = WidgetToGrid(w, "RedrawColumn()");
	if (!g)
		return;
	c = ColTypePosToPos(g, type, column, 0);
	DrawArea(g, DrawCol, 0, c);
	}

void
XmLGridRedrawRow(Widget w,
		 unsigned char type,
		 int row)
	{
	XmLGridWidget g;
	int r;

	g = WidgetToGrid(w, "RedrawRow()");
	if (!g)
		return;
	r = RowTypePosToPos(g, type, row, 0);
	DrawArea(g, DrawRow, r, 0);
	}

int
XmLGridRead(Widget w,
	    FILE *file,
	    int format,
	    char delimiter)
	{
	XmLGridWidget g;
	char *data;
	int n;

	g = WidgetToGrid(w, "Read()");
	if (!g)
		return 0;
	if (!file)
		{
		XmLWarning(w, "Read() - file is NULL");
		return 0;
		}
	data = FileToString(file);
	if (!data)
		{
		XmLWarning(w, "Read() - error loading file");
		return 0;
		}
	n = Read(g, format, delimiter, 0, 0, data);
	free((char *)data);
	return n;
	}

int
XmLGridReadPos(Widget w,
	       FILE *file,
	       int format,
	       char delimiter,
	       unsigned char rowType,
	       int row,
	       unsigned char columnType,
	       int column)
	{
	XmLGridWidget g;
	char *data;
	int r, c, n;

	g = WidgetToGrid(w, "ReadPos()");
	if (!g)
		return 0;
	if (!file)
		{
		XmLWarning(w, "ReadPos() - file is NULL");
		return 0;
		}
	data = FileToString(file);
	if (!data)
		{
		XmLWarning(w, "ReadPos() - error loading file");
		return 0;
		}
	r = RowTypePosToPos(g, rowType, row, 0);
	c = ColTypePosToPos(g, columnType, column, 0);
	n = Read(g, format, delimiter, r, c, data);
	free((char *)data);
	return n;
	}

int
XmLGridSetStrings(Widget w,
		  char *data)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "SetStrings()");
	if (!g)
		return 0;
	return Read(g, XmFORMAT_DELIMITED, '|', 0, 0, data);
	}

int
XmLGridSetStringsPos(Widget w,
		     unsigned char rowType,
		     int row,
		     unsigned char columnType,
		     int column,
		     char *data)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "SetStringsPos()");
	if (!g)
		return 0;
	r = RowTypePosToPos(g, rowType, row, 0);
	c = ColTypePosToPos(g, columnType, column, 0);
	return Read(g, XmFORMAT_DELIMITED, '|', r, c, data);
	}

int
XmLGridWrite(Widget w,
	     FILE *file,
	     int format,
	     char delimiter,
	     Boolean skipHidden)
	{
	XmLGridWidget g;
	int nrow, ncol;

	g = WidgetToGrid(w, "Write()");
	if (!g)
		return 0;
	nrow = XmLArrayGetCount(g->grid.rowArray);
	ncol = XmLArrayGetCount(g->grid.colArray);
	Write(g, file, format, delimiter, skipHidden, 0, 0, nrow, ncol);
	return 0;
	}

int
XmLGridWritePos(Widget w,
		FILE *file,
		int format,
		char delimiter,
		Boolean skipHidden,
		unsigned char rowType,
		int row,
		unsigned char columnType,
		int column,
		int nrow,
		int ncolumn)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "WritePos()");
	if (!g)
		return 0;
	r = RowTypePosToPos(g, rowType, row, 0);
	c = ColTypePosToPos(g, columnType, column, 0);
	Write(g, file, format, delimiter, skipHidden, r, c, nrow, ncolumn);
	return 0;
	}

Boolean
XmLGridCopyPos(Widget w,
	       Time time,
	       unsigned char rowType,
	       int row,
	       unsigned char columnType,
	       int column,
	       int nrow,
	       int ncolumn)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "CopyPos()");
	if (!g)
		return False;
	r = RowTypePosToPos(g, rowType, row, 0);
	c = ColTypePosToPos(g, columnType, column, 0);
	return Copy(g, time, 0, r, c, nrow, ncolumn);
	}

Boolean
XmLGridCopySelected(Widget w,
		    Time time)
	{
	XmLGridWidget g;

	g = WidgetToGrid(w, "CopySelected()");
	if (!g)
		return False;
	return Copy(g, time, 1, 0, 0, 0, 0);
	}

Boolean
XmLGridPaste(Widget w)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "Paste()");
	if (!g)
		return False;
	r = g->grid.focusRow;
	c = g->grid.focusCol;
	if (r < 0 || c < 0)
		{
		XmLWarning(w, "Paste() - no cell has focus");
		return False;
		}
	return Paste(g, r, c);
	}

Boolean
XmLGridPastePos(Widget w,
		unsigned char rowType,
		int row,
		unsigned char columnType,
		int column)
	{
	XmLGridWidget g;
	int r, c;

	g = WidgetToGrid(w, "PastePos()");
	if (!g)
		return False;
	r = RowTypePosToPos(g, rowType, row, 0);
	c = ColTypePosToPos(g, columnType, column, 0);
	return Paste(g, r, c);
	}

/* XFE Additions below here */
void
XmLGridInstallHideButtonTranslations(Widget w)
{
  XmLGridWidget g = (XmLGridWidget)w;
  Widget hideButton = g->grid.hideButton;

  if (g->grid.showHideButton == False
      || hideButton == NULL)
    return;

  /*  XtOverrideTranslations(hideButton, g->grid.hideButtonTrans);*/
}

static void 
HideColumn(Widget w, 
	   XEvent *event, 
	   String *params, 
	   Cardinal *num_params)
{
    XmLGridWidget g = (XmLGridWidget)w;

    if (g->grid.colCount == 1 || g->grid.colCount == 0) {
	/* if there's only one column left, don't let them
	   hide it.  Also, if there are no columns at all,
	   they can't hide any. */

	return;
    }
    else {
    
	if (g->grid.realColCount == -1) {
	    /* nothing's been hidden yet, so we save off the
	       actual column count before mucking with it. */
	    g->grid.realColCount = g->grid.colCount;
	}

	g->grid.colCount--;
	
	HorizLayout(g, 1);
	DrawArea(g, DrawAll, 0, 0);
    }
}

void
XmLGridSetVisibleColumnCount(Widget w,
			     int num_visible)
{
  XmLGridWidget g = (XmLGridWidget)w;
  int real_count;

  if (g->grid.realColCount == -1)
    real_count = g->grid.colCount;
  else
    real_count = g->grid.realColCount;

  if (num_visible > real_count)
    return; /* should really be an error... */

  if (g->grid.realColCount == -1)
    g->grid.realColCount = g->grid.colCount;

  g->grid.colCount = num_visible;
}

static void 
UnhideColumn(Widget w, 
	     XEvent *event, 
	     String *params, 
	     Cardinal *num_params)
{
    XmLGridWidget g = (XmLGridWidget)w;

    if (g->grid.realColCount == -1) {
	/* there is nothing hidden, so we can't unhide it */
	return;
    }
    else {

	g->grid.colCount++;

	HorizLayout(g, 1);
	DrawArea(g, DrawAll, 0, 0);

	if (g->grid.colCount == g->grid.realColCount) {
	    /* there's nothing hidden anymore, so we reinitialize
	       the realColCount to -1, as some functions (like this
	       one) might want to use this value */
	    g->grid.realColCount = -1;
	}
    }
}

static void 
MenuArm(Widget w, 
	XEvent *event, 
	String *params, 
	Cardinal *num_params)
{
    XmLGridWidget g = (XmLGridWidget)w;

    SelectTypeArea(g, SelectRow, (XEvent *)0, g->grid.lastSelectRow, 0, False, False);

    g->grid.inMode = InSelect;
}

static void 
MenuDisarm(Widget w, 
	   XEvent *event, 
	   String *params, 
	   Cardinal *num_params)
{
    XmLGridWidget g = (XmLGridWidget)w;

    g->grid.inMode = InNormal;
}



