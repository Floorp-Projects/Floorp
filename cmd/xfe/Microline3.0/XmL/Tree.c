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

#include "TreeP.h"

#include <stdio.h>

static void Initialize(Widget req, Widget newW, ArgList args, Cardinal *nargs);
static void Destroy(Widget w);
static Boolean SetValues(Widget curW, Widget, Widget newW, 
	ArgList args, Cardinal *nargs);
static int _PreLayout(XmLGridWidget g, int isVert);
static int _TreeCellAction(XmLGridCell cell, Widget w,
	XmLGridCallbackStruct *cbs);
static void DrawIconCell(XmLGridCell cell, Widget w,
	int row, XRectangle *clipRect, XmLGridDrawStruct *ds);
static void DrawConnectingLine(Display *dpy, Window win, GC gc,
	XRectangle *clipRect, int offFlag, int x1, int y1, int x2, int y2);
static void BtnPress(Widget w, XtPointer closure, XEvent *event,
	Boolean *ctd);
static void Activate(Widget w, XtPointer clientData, XtPointer callData);
static void SwitchRowState(XmLTreeWidget t, int row, XEvent *event);
static XmLGridRow _RowNew(Widget tree);
static void _GetRowValueMask(XmLGridWidget g, char *s, long *mask);
static void _GetRowValue(XmLGridWidget g, XmLGridRow r,
	XtArgVal value, long mask);
static int _SetRowValues(XmLGridWidget g, XmLGridRow r, long mask);
static int _SetCellValuesResize(XmLGridWidget g, XmLGridRow row,
	XmLGridColumn col, XmLGridCell cell, long mask);

static void GetManagerForeground(Widget w, int, XrmValue *value);
static void CreateDefaultPixmaps(XmLTreeWidget t);
static XmLTreeWidget WidgetToTree(Widget w, char *funcname);

static XtResource resources[] =
	{
		{
		XmNcollapseCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLTreeWidget, tree.collapseCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNconnectingLineColor, XmCConnectingLineColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLTreeWidget, tree.lineColor),
		XmRCallProc, (XtPointer)GetManagerForeground,
		},
		{
		XmNexpandCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLTreeWidget, tree.expandCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNlevelSpacing, XmCLevelSpacing,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLTreeWidget, tree.levelSpacing),
		XmRImmediate, (XtPointer)11,
		},
		{
		XmNplusMinusColor, XmCPlusMinusColor,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLTreeWidget, tree.pmColor),
		XmRCallProc, (XtPointer)GetManagerForeground,
		},
		/* Row Resources */
		{
		XmNrowExpands, XmCRowExpands,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLTreeWidget, tree.rowExpands),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNrowIsExpanded, XmCRowIsExpanded,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLTreeWidget, tree.rowIsExpanded),
		XmRImmediate, (XtPointer)True,
		},
		{
		XmNrowLevel, XmCRowLevel,
		XmRInt, sizeof(int),
		XtOffset(XmLTreeWidget, tree.rowLevel),
		XmRImmediate, (XtPointer)0,
		},

        /* XmNignorePixmaps.  Causes the tree to NOT render any pixmaps */
		{
		XmNignorePixmaps, XmCIgnorePixmaps,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLTreeWidget, tree.ignorePixmaps),
		XmRImmediate, (XtPointer) False,
		},
	};

XmLTreeClassRec xmlTreeClassRec =
	{
		{ /* core_class */
		(WidgetClass)&xmlGridClassRec,            /* superclass         */
		"XmLTree",                                /* class_name         */
		sizeof(XmLTreeRec),                       /* widget_size        */
		(XtProc)NULL,                             /* class_init         */
		0,                                        /* class_part_init    */
		FALSE,                                    /* class_inited       */
		(XtInitProc)Initialize,                   /* initialize         */
		0,                                        /* initialize_hook    */
		XtInheritRealize,                         /* realize            */
		NULL,                                     /* actions            */
		0,                                        /* num_actions        */
		resources,                                /* resources          */
		XtNumber(resources),                      /* num_resources      */
		NULLQUARK,                                /* xrm_class          */
		TRUE,                                     /* compress_motion    */
		XtExposeCompressMaximal,                  /* compress_exposure  */
		TRUE,                                     /* compress_enterleav */
		TRUE,                                     /* visible_interest   */
		(XtWidgetProc)Destroy,                    /* destroy            */
		XtInheritResize,                          /* resize             */
		XtInheritExpose,                          /* expose             */
		(XtSetValuesFunc)SetValues,               /* set_values         */
		0,                                        /* set_values_hook    */
		XtInheritSetValuesAlmost,                 /* set_values_almost  */
		0,                                        /* get_values_hook    */
		0,                                        /* accept_focus       */
		XtVersion,                                /* version            */
		0,                                        /* callback_private   */
		XtInheritTranslations,                    /* tm_table           */
		0,                                        /* query_geometry     */
		0,                                        /* display_accelerato */
		0,                                        /* extension          */
		},
		{ /* composite_class */
		XtInheritGeometryManager,                 /* geometry_manager   */
		XtInheritChangeManaged,                   /* change_managed     */
		XtInheritInsertChild,                     /* insert_child       */
		XtInheritDeleteChild,                     /* delete_child       */
		0,                                        /* extension          */
		},
		{ /* constraint_class */
		0,	                                      /* subresources       */
		0,                                        /* subresource_count  */
		sizeof(XmLTreeConstraintRec),             /* constraint_size    */
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
		0,                                        /* num_get_cont_resou */
		XmInheritParentProcess,                   /* parent_process     */
		0,                                        /* extension          */
		},
		{ /* grid_class */
		0,                                        /* initial rows       */
		1,                                        /* initial cols       */
		_PreLayout,                               /* post layout        */
		sizeof(struct _XmLTreeRowRec),            /* row rec size       */
		_RowNew,                                  /* row new            */
		XmInheritGridRowFree,                     /* row free           */
		_GetRowValueMask,                         /* get row value mask */
		_GetRowValue,                             /* get row value      */
		_SetRowValues,                            /* set row values     */
		sizeof(struct _XmLGridColumnRec),         /* column rec size    */
		XmInheritGridColumnNew,                   /* column new         */
		XmInheritGridColumnFree,                  /* column free        */
		XmInheritGridGetColumnValueMask,          /* get col value mask */
		XmInheritGridGetColumnValue,              /* get column value   */
		XmInheritGridSetColumnValues,             /* set column values  */
		_SetCellValuesResize,                     /* set cell vl resize */
		_TreeCellAction,                          /* cell action        */
		},
		{ /* tree_class */
		0,                                        /* unused             */
		}
	};

WidgetClass xmlTreeWidgetClass = (WidgetClass)&xmlTreeClassRec;

static void
Initialize(Widget reqW,
	   Widget newW,
	   ArgList args,
	   Cardinal *narg)
	{
	XmLTreeWidget t;
 
	t = (XmLTreeWidget)newW;
	if (t->core.width <= 0) 
		t->core.width = 100;
	if (t->core.height <= 0) 
		t->core.height = 100;
	t->tree.defaultPixmapsCreated = 0;
	t->tree.linesData = 0;
	t->tree.linesSize = 0;
	t->tree.recalcTreeWidth = 0;
	if (t->grid.rowCount)
		{
		XmLWarning(newW, "Initialize() - can't set XmNrows");
		XmLGridDeleteAllRows(newW, XmCONTENT);
		}
	XtAddCallback(newW, XmNactivateCallback, Activate, NULL);
	XtAddEventHandler(newW, ButtonPressMask,
		True, (XtEventHandler)BtnPress, (XtPointer)0);

	XtVaSetValues(newW,
		XmNcellDefaults, True,
		XmNcolumn, 0,
		XmNcellType, XmICON_CELL,
		NULL);
	}

static void
Destroy(Widget w)
	{
	XmLTreeWidget t;
	Display *dpy;
	XWindowAttributes attr;

	t = (XmLTreeWidget)w;
	dpy = XtDisplay(t);
	if (t->tree.linesData)
		free((char *)t->tree.linesData);
	if (t->tree.defaultPixmapsCreated)
		{
		XGetWindowAttributes(dpy, XtWindow(w), &attr);
		XFreePixmap(dpy, t->tree.filePixmask);
		XFreePixmap(dpy, t->tree.folderPixmask);
		XFreePixmap(dpy, t->tree.folderOpenPixmask);
		XFreePixmap(dpy, t->tree.filePixmask);
		XFreePixmap(dpy, t->tree.folderPixmask);
		XFreePixmap(dpy, t->tree.folderOpenPixmask);
		XFreeColors(dpy, attr.colormap, t->tree.pixColors, 4, 0L);
		}
	}

static Boolean
SetValues(Widget curW,
	  Widget reqW,
	  Widget newW,
	  ArgList args,
	  Cardinal *nargs)
{
	XmLTreeWidget t, cur;
	XmLGridColumn col;
	Boolean needsResize, needsRedraw;
 
	t = (XmLTreeWidget)newW;
	cur = (XmLTreeWidget)curW;
	needsResize = False;
	needsRedraw = False;

#define NE(value) (t->value != cur->value)
	if (NE(grid.rowCount))
		XmLWarning(newW, "SetValues() - can't set XmNrows");
	if (NE(tree.pmColor) || NE(tree.lineColor))
		needsRedraw = True;
	if (NE(tree.levelSpacing) ||
		t->tree.recalcTreeWidth)
		{
		col = XmLGridGetColumn(newW, XmCONTENT, 0);
		if (col)
			col->grid.widthInPixelsValid = 0;
		t->tree.recalcTreeWidth = 0;
		needsResize = True;
		needsRedraw = True;
		}
#undef NE
	if (needsResize)
		_XmLGridLayout((XmLGridWidget)t);
	if (needsRedraw)
		XmLGridRedrawAll((Widget)t);
	return False;
}

static int
_PreLayout(XmLGridWidget g,
           int isVert)
	{
	XmLTreeWidget t;
	XmLTreeRow row;
	Widget w;
	int i, j, size, maxLevel, hideLevel, lineWidth;
	char *thisLine, *prevLine;

	t = (XmLTreeWidget)g;
	w = (Widget)g;
	if (!t->grid.vertVisChangedHint)
		return 0; /* ?? */
	t->grid.vertVisChangedHint = 0;

	/* top down calculation of hidden states and maxLevel */
	hideLevel = -1;
	maxLevel = 0;
	t->grid.layoutFrozen = True;
	for (i = 0; i < t->grid.rowCount; i++)
		{
		row = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, i);
		if (row->tree.level > maxLevel)
			maxLevel = row->tree.level;

		if (hideLevel != -1 && row->tree.level > hideLevel)
			{
			if (row->grid.height)
				XtVaSetValues(w,
					XmNrow, i,
					XmNrowHeight, 0,
					NULL);
			}
		else
			{
			if (row->tree.expands == True && row->tree.isExpanded == False)
				hideLevel = row->tree.level;
			else
				hideLevel = -1;
			if (!row->grid.height)
				XtVaSetValues(w,
					XmNrow, i,
					XmNrowHeight, 1,
					NULL);
			}
		}
	t->grid.layoutFrozen = False;
	t->tree.linesMaxLevel = maxLevel;
	if (!t->grid.rowCount)
		return 0;

	/* bottom up calculation of connecting lines */
	lineWidth = maxLevel + 1;
	size = lineWidth * t->grid.rowCount;
	if (t->tree.linesSize < size)
		{
		if (t->tree.linesData)
			free((char *)t->tree.linesData);
		t->tree.linesSize = size;
		t->tree.linesData = (char *)malloc(size);
		}
	prevLine = 0;
	thisLine = &t->tree.linesData[size - lineWidth];
	for (i = t->grid.rowCount - 1; i >= 0; i--)
		{
		row = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, i);
		if (!row->grid.height)
			{
			thisLine -= lineWidth;
			continue;
			}
		for (j = 0; j < row->tree.level - 1; j++)
			{
			if (prevLine && (prevLine[j] == 'L' || prevLine[j] == 'I' ||
				prevLine[j] == 'E'))
				thisLine[j] = 'I';
			else
				thisLine[j] = ' ';
			}	
		if (row->tree.level)
			{
			if (prevLine && (prevLine[j] == 'L' || prevLine[j] == 'I' ||
				prevLine[j] == 'E'))
				thisLine[j++] = 'E';
			else
				thisLine[j++] = 'L';
			}
		thisLine[j++] = 'O';
		for (; j < lineWidth; j++)
			thisLine[j] = ' ';
		prevLine = thisLine;
		thisLine -= lineWidth;
		}
	if (prevLine)
		{
		for (i = 0; i < lineWidth; i++)
			{
			if (prevLine[i] == 'L')
				prevLine[i] = '-';
			else if (prevLine[i] == 'E')
				prevLine[i] = 'P';
			}
		}

	/* if we are in VertLayout(), the horizontal size may need */
	/* recomputing because of the row hides. */
	if (isVert)
		return 1;

	/* if we are in HorizLayout(), the vertical recomputation */
	/* will be happening regardless, since row changes (vertical) */
	/* are why we are here */
	return 0;
	}

static int
_TreeCellAction(XmLGridCell cell,
            Widget w,
            XmLGridCallbackStruct *cbs)
	{
	XmLTreeWidget t;
	XmLTreeRow row;
	XmLGridColumn col;
	XmLGridWidgetClass sc;
	XmLGridCellActionProc cellActionProc;
	XmLGridCellRefValues *cellValues;
	XmLGridCellIcon *icon;
	/*	XRectangle *rect, cRect;*/
	int ret, h, isTreeCell;


	Dimension default_icon_width = 16;
	Dimension default_icon_height = 16;

	t = (XmLTreeWidget)w;
	if (cbs->rowType == XmCONTENT &&
		cbs->columnType == XmCONTENT &&
		cbs->column == 0)
		isTreeCell = 1;
	else
		isTreeCell = 0;
	sc = (XmLGridWidgetClass)xmlTreeWidgetClass->core_class.superclass;
	cellActionProc = sc->grid_class.cellActionProc;
	ret = 0;

	/* Check for ignore pixmaps */
	if (t->tree.ignorePixmaps)
	{
		default_icon_width = 0;
		default_icon_height = 0;
	}

	switch (cbs->reason)
		{
		case XmCR_CELL_DRAW:
			if (isTreeCell)
				DrawIconCell(cell, w, cbs->row, cbs->clipRect, cbs->drawInfo);
			else
				ret = cellActionProc(cell, w, cbs);
			break;
		case XmCR_EDIT_BEGIN: /* Fall through */
		case XmCR_EDIT_INSERT:
            if (isTreeCell)
            {
                int iconOffset;
				cellValues = cell->cell.refValues;
				row = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, cbs->row);
				icon = (XmLGridCellIcon *)cell->cell.value;
                iconOffset = 4 + cellValues->leftMargin
                    + t->tree.levelSpacing * 2 * row->tree.level;
				if (!icon)
 					iconOffset += default_icon_width;
				else if (icon->pix.pixmap == XmUNSPECIFIED_PIXMAP)
 					iconOffset += default_icon_width;
				else
					iconOffset += icon->pix.width;
                cbs->clipRect->x += iconOffset;
                if (cbs->clipRect->width > iconOffset)
                    cbs->clipRect->width -= iconOffset;
                else
                    cbs->clipRect->width = 0;
            }
            ret = cellActionProc(cell, w, cbs);
			break;
		case XmCR_PREF_HEIGHT:
			ret = cellActionProc(cell, w, cbs);
			if (isTreeCell)
				{
				cellValues = cell->cell.refValues;
				if (cellValues->type != XmICON_CELL)
					return 0;
				icon = (XmLGridCellIcon *)cell->cell.value;

				h = 4 + default_icon_height + cellValues->topMargin + cellValues->bottomMargin;

				if (icon && icon->pix.pixmap == XmUNSPECIFIED_PIXMAP &&
					ret < h)
					ret = h;
				}
			break;
		case XmCR_PREF_WIDTH:
			if (isTreeCell)
				{
				cellValues = cell->cell.refValues;
				if (cellValues->type != XmICON_CELL)
					return 0;
				icon = (XmLGridCellIcon *)cell->cell.value;
				col = (XmLGridColumn)cbs->object;
				row = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, cbs->row);
				if (row->tree.stringWidthValid == False)
					{
					if (icon && icon->string)
						row->tree.stringWidth =
							XmStringWidth(cellValues->fontList, icon->string);
					else
						row->tree.stringWidth = 0;
					row->tree.stringWidthValid = True;
					}
				ret = 4 + cellValues->leftMargin + t->tree.levelSpacing * 2 *
					row->tree.level + t->grid.iconSpacing + row->tree.stringWidth +
					cellValues->rightMargin;
				if (!icon)
 					ret += default_icon_width;
				else if (icon->pix.pixmap == XmUNSPECIFIED_PIXMAP)
 					ret += default_icon_width;
				else
					ret += icon->pix.width;
				if (!row->grid.height)
					ret = 0;
				}
			else
				ret = cellActionProc(cell, w, cbs);
			break;
		default:
			ret = cellActionProc(cell, w, cbs);
			break;
		}
	return ret;
	}

static void
DrawIconCell(XmLGridCell cell,
             Widget w,
             int row,
             XRectangle *clipRect,
             XmLGridDrawStruct *ds)
	{
	XmLTreeWidget t;
	XmLTreeRow rowp;
	XmLGridCellRefValues *cellValues;
	XmLGridCellIcon *icon;
	XRectangle *cellRect, rect;
	Display *dpy;
	Window win;
	char *thisLine;
	int i, clipSet, pixWidth, pixHeight;
	int xoff, xoff2, midy, oddFlag, x1, y1, x2, y2;
	Pixmap pixmap, pixmask;

	t = (XmLTreeWidget)w;
	rowp = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, row);
	dpy = XtDisplay(w);
	win = XtWindow(w);
	cellValues = cell->cell.refValues;
	if (cellValues->type != XmICON_CELL)
		return;
	icon = (XmLGridCellIcon *)cell->cell.value;
	if (!icon)
		return;
	cellRect = ds->cellRect;
	if (!t->tree.linesData)
		{
		XmLWarning(w, "DrawIconCell() - no lines data calculated");
		return;
		}

	/* draw background */
	XSetForeground(dpy, ds->gc, cell->cell.refValues->background);
	XFillRectangle(dpy, win, ds->gc, clipRect->x, clipRect->y,
		clipRect->width, clipRect->height);

	if (t->grid.singleColScrollMode)
		oddFlag = t->grid.singleColScrollPos & 1;
	else
		oddFlag = 0;

	pixWidth = 0;
	xoff = t->tree.levelSpacing;
	xoff2 = xoff * 2;
	y1 = cellRect->y;
	y2 = cellRect->y + cellRect->height - 1;
	midy = cellRect->y + cellRect->height / 2;
	if (midy & 1)
		midy += 1;

	/* draw connecting lines and pixmap */
	XSetForeground(dpy, ds->gc, t->tree.lineColor);
	thisLine = &t->tree.linesData[row * (t->tree.linesMaxLevel + 1)];
	for (i = 0; i <= t->tree.linesMaxLevel; i++)
		{
		x1 = cellRect->x + (xoff2 * i);
		if (x1 >= clipRect->x + (int)clipRect->width)
			continue;
		switch (thisLine[i])
			{
			case 'O':
              if (!t->tree.ignorePixmaps)
              {
				rect.x = x1;
				rect.y = cellRect->y;
				rect.width = cellRect->width;
				rect.height = cellRect->height;
				if (icon->pix.pixmap != XmUNSPECIFIED_PIXMAP)
					{
					pixmap = icon->pix.pixmap;
					pixmask = icon->pix.pixmask;
					pixWidth = icon->pix.width;
					pixHeight = icon->pix.height;
					}
				else
					{
					if (!t->tree.defaultPixmapsCreated)
						CreateDefaultPixmaps(t);
					if (rowp->tree.expands && rowp->tree.isExpanded)
						{
						pixmap = t->tree.folderOpenPixmap;
						pixmask = t->tree.folderOpenPixmask;
						}
					else if (rowp->tree.expands)
						{
						pixmap = t->tree.folderPixmap;
						pixmask = t->tree.folderPixmask;
						}
					else
						{
						pixmap = t->tree.filePixmap;
						pixmask = t->tree.filePixmask;
						}
					pixWidth = 16;
					pixHeight = 16;
					}
				
				XmLPixmapDraw(w, pixmap, pixmask, pixWidth, pixHeight,
							  XmALIGNMENT_BOTTOM_LEFT, ds->gc, &rect, clipRect);
              } /* !t->tree.ignorePixmaps */
				break;
			case 'I':
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, y1, x1 + xoff, y2);
				break;
			case 'E':
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, y1, x1 + xoff, y2);
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, midy, x1 + xoff2, midy);
				break;
			case 'L':
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, y1, x1 + xoff, midy);
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, midy, x1 + xoff2, midy);
				break;
			case 'P':
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, midy, x1 + xoff, y2);
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, midy, x1 + xoff2, midy);
				break;
			case '-':
				DrawConnectingLine(dpy, win, ds->gc, clipRect, oddFlag,
					x1 + xoff, midy, x1 + xoff2, midy);
				break;
			}
		}

	clipSet = 0;

	/* draw expand/collapse graphic */
	rect.x = cellRect->x + (rowp->tree.level - 1) * xoff2 + xoff - 5;
	rect.y = midy - 5;
	rect.width = 11;
	rect.height = 11;
	i = XmLRectIntersect(&rect, clipRect);
	if (rowp->tree.expands && rowp->tree.level && i != XmLRectOutside)
		{
		if (i == XmLRectPartial)
			{
			clipSet = 1;
			XSetClipRectangles(dpy, ds->gc, 0, 0, clipRect, 1, Unsorted);
			}
		x1 = rect.x;
		x2 = rect.x + rect.width - 1;
		y1 = rect.y;
		y2 = rect.y + rect.height - 1;
		XSetForeground(dpy, ds->gc, cellValues->background);
		XFillRectangle(dpy, win, ds->gc, x1, y1, 11, 11);
		XSetForeground(dpy, ds->gc, t->tree.lineColor);
		XDrawLine(dpy, win, ds->gc, x1 + 2, y1 + 1, x2 - 2, y1 + 1);
		XDrawLine(dpy, win, ds->gc, x2 - 1, y1 + 2, x2 - 1, y2 - 2);
		XDrawLine(dpy, win, ds->gc, x1 + 2, y2 - 1, x2 - 2, y2 - 1);
		XDrawLine(dpy, win, ds->gc, x1 + 1, y1 + 2, x1 + 1, y2 - 2);
		XSetForeground(dpy, ds->gc, t->tree.pmColor);
		if (!rowp->tree.isExpanded)
			XDrawLine(dpy, win, ds->gc, x1 + 5, y1 + 3, x1 + 5, y1 + 7);
		XDrawLine(dpy, win, ds->gc, x1 + 3, y1 + 5, x1 + 7, y1 + 5);
		}

	/* draw select background and highlight */
	i = rowp->tree.level * xoff2 + pixWidth + t->grid.iconSpacing;
	rect.x = cellRect->x + i;
	rect.y = cellRect->y;
	rect.height = cellRect->height;
	rect.width = 0;
	if (t->grid.colCount == 1 && rowp->tree.stringWidthValid)
		rect.width = rowp->tree.stringWidth + 4;
	else if ((int)cellRect->width > i)
		rect.width = cellRect->width - i;
	i = XmLRectIntersect(&rect, clipRect);
	if (i != XmLRectOutside && ds->drawSelected)
		{
		if (i == XmLRectPartial && !clipSet)
			{
			clipSet = 1;
			XSetClipRectangles(dpy, ds->gc, 0, 0, clipRect, 1, Unsorted);
			}
		XSetForeground(dpy, ds->gc, ds->selectBackground);
		XFillRectangle(dpy, win, ds->gc, rect.x, rect.y,
			rect.width, rect.height);
		}
	if (ds->drawFocusType != XmDRAW_FOCUS_NONE &&
		t->grid.highlightThickness >= 2)
		{
		if (i == XmLRectPartial && !clipSet)
			{
			clipSet = 1;
			XSetClipRectangles(dpy, ds->gc, 0, 0, clipRect, 1, Unsorted);
			}
		XSetForeground(dpy, ds->gc, t->manager.highlight_color);
		x1 = rect.x;
		x2 = rect.x + rect.width - 1;
		y1 = rect.y;
		y2 = rect.y + rect.height - 1;
		XDrawLine(dpy, win, ds->gc, x1, y1, x2, y1);
		if (ds->drawFocusType == XmDRAW_FOCUS_CELL ||
			ds->drawFocusType == XmDRAW_FOCUS_RIGHT)
			XDrawLine(dpy, win, ds->gc, x2, y1, x2, y2);
		XDrawLine(dpy, win, ds->gc, x1, y2, x2, y2);
		XDrawLine(dpy, win, ds->gc, x1, y1, x1, y2);
		y1 += 1;
		y2 -= 1;
		XDrawLine(dpy, win, ds->gc, x1, y2, x2, y2);
		XDrawLine(dpy, win, ds->gc, x1, y1, x2, y1);
		x1 += 1;
		x2 -= 1;
		XDrawLine(dpy, win, ds->gc, x1, y1, x1, y2);
		if (ds->drawFocusType == XmDRAW_FOCUS_CELL ||
			ds->drawFocusType == XmDRAW_FOCUS_RIGHT)
			XDrawLine(dpy, win, ds->gc, x2, y1, x2, y2);
		}

	/* draw string */
	if (icon->string)
		{
		if (ds->drawSelected == True)
			XSetForeground(dpy, ds->gc, ds->selectForeground);
		else
			XSetForeground(dpy, ds->gc, cellValues->foreground);
		XmLStringDraw(w, icon->string, ds->stringDirection,
			cellValues->fontList, XmALIGNMENT_LEFT,
			ds->gc, &rect, clipRect);
		}

	if (clipSet)
		XSetClipMask(dpy, ds->gc, None);
	}

static void
DrawConnectingLine(Display *dpy,
                    Window win,
                    GC gc,
                    XRectangle *clipRect,
                    int oddFlag,
                    int x1,
                    int y1,
                    int x2,
                    int y2)
	{
	int i, x, y;
	XPoint points[100];

	i = 0;
	for (x = x1; x <= x2; x++)
		for (y = y1; y <= y2; y++)
			{
			if ((((x + oddFlag) & 1) == (y & 1)) ||
				x < clipRect->x ||
				x >= (clipRect->x + (int)clipRect->width) ||
				y < clipRect->y ||
				y >= (clipRect->y + (int)clipRect->height))
				continue;
			points[i].x = x;
			points[i].y = y;
			if (++i < 100)
				continue;
			XDrawPoints(dpy, win, gc, points, i, CoordModeOrigin);
			i = 0;
			}
	if (i)
		XDrawPoints(dpy, win, gc, points, i, CoordModeOrigin);
	}

static void
BtnPress(Widget w,
         XtPointer closure,
         XEvent *event,
         Boolean *ctd)
	{
	XmLTreeWidget t;
	XmLTreeRow rowp;
	unsigned char rowType, colType;
	int row, col, x1, y1, x2, y2, xoff;
	XRectangle rect;
	XButtonEvent *be;
	static int lastRow = -1;
	static Time lastSelectTime = 0;

	t = (XmLTreeWidget)w;
	if (event->type != ButtonPress)
		return;
	be = (XButtonEvent *)event;
	if (be->button != Button1 || be->state & ControlMask ||
		be->state & ShiftMask)
		return;
	if (XmLGridXYToRowColumn(w, be->x, be->y, &rowType, &row,
		&colType, &col) == -1)
		return;
	rowp = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, row);
	if (rowType != XmCONTENT || colType != XmCONTENT || col != 0)
		return;
	if (XmLGridRowColumnToXY(w, rowType, row, colType, col,
		False, &rect) == -1)
		return;
	if ((be->time - lastSelectTime) < XtGetMultiClickTime(XtDisplay(w)) &&
		lastRow == row)
		{
		/* activate callback will be handling expand/collapse */
		lastSelectTime = be->time;
		return;
		}
	lastSelectTime = be->time;
	lastRow = row;
	xoff = t->tree.levelSpacing;
	x1 = rect.x + (rowp->tree.level - 1) * xoff * 2 + xoff - 6;
	x2 = x1 + 13;
	y1 = rect.y + rect.height / 2 - 6;
	y2 = y1 + 13;
	if (be->x > x2 || be->x < x1 || be->y > y2 || be->y < y1)
		return;
	SwitchRowState(t, row, event);
	}

static void
Activate(Widget w,
         XtPointer clientData,
         XtPointer callData)
	{
	XmLTreeWidget t;
	XmLGridCallbackStruct *cbs;

	t = (XmLTreeWidget)w;
	cbs = (XmLGridCallbackStruct *)callData;
	if (cbs->rowType != XmCONTENT)
	if (t->grid.selectionPolicy == XmSELECT_CELL &&
		(cbs->columnType != XmCONTENT || cbs->column != 0))
		return;
	SwitchRowState(t, cbs->row, cbs->event);
	}

static void
SwitchRowState(XmLTreeWidget t,
               int row,
               XEvent *event)
	{
	Widget w;
	XmLTreeRow rowp;
	XmLGridCallbackStruct cbs;

	w = (Widget)t;
	rowp = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, row);
	if (rowp->tree.expands == False) 
		return;
	cbs.event = event;
	cbs.columnType = XmCONTENT;
	cbs.column = 0;
	cbs.rowType = XmCONTENT;
	cbs.row = row;
	if (rowp->tree.isExpanded == True)
		{
		XtVaSetValues(w,
			XmNrow, row,
			XmNrowIsExpanded, False,
			NULL);
		cbs.reason = XmCR_COLLAPSE_ROW;
		XtCallCallbackList(w, t->tree.collapseCallback, (XtPointer)&cbs);
		}
	else
		{
		XtVaSetValues(w,
			XmNrow, row,
			XmNrowIsExpanded, True,
			NULL);
		cbs.reason = XmCR_EXPAND_ROW;
		XtCallCallbackList(w, t->tree.expandCallback, (XtPointer)&cbs);
		}
	}

/* Only to be called through Grid class */
static XmLGridRow
_RowNew(Widget tree)
	{
	XmLGridWidgetClass sc;
	XmLTreeRow row;

	sc = (XmLGridWidgetClass)xmlTreeWidgetClass->core_class.superclass;
	row = (XmLTreeRow)sc->grid_class.rowNewProc(tree);
	row->tree.level = 0;
	row->tree.expands = False;
	row->tree.isExpanded = True;
	row->tree.hasSiblings = False;
	row->tree.stringWidth = 0;
	row->tree.stringWidthValid = False;
	return (XmLGridRow)row;
	}

/* Only to be called through Grid class */
static void
_GetRowValueMask(XmLGridWidget g,
                 char *s,
                 long *mask)
        {
	XmLGridWidgetClass sc;
	static XrmQuark qLevel, qExpands, qIsExpanded;
	static int quarksValid = 0;
	XrmQuark q;
 
	sc = (XmLGridWidgetClass)xmlTreeWidgetClass->core_class.superclass;
	sc->grid_class.getRowValueMaskProc(g, s, mask);
	if (!quarksValid)
		{
		qLevel = XrmStringToQuark(XmNrowLevel);
		qExpands = XrmStringToQuark(XmNrowExpands);
		qIsExpanded = XrmStringToQuark(XmNrowIsExpanded);
		quarksValid = 1;
		}
	q = XrmStringToQuark(s);
	if (q == qLevel)
		*mask |= XmLTreeRowLevel;
	else if (q == qExpands)
		*mask |= XmLTreeRowExpands;
	else if (q == qIsExpanded)
		*mask |= XmLTreeRowIsExpanded;
	}

/* Only to be called through Grid class */
static void
_GetRowValue(XmLGridWidget g,
             XmLGridRow r,
             XtArgVal value,
             long mask)
	{
	XmLGridWidgetClass sc;
	XmLTreeRow row;

	sc = (XmLGridWidgetClass)xmlTreeWidgetClass->core_class.superclass;
	sc->grid_class.getRowValueProc(g, r, value, mask);
	row = (XmLTreeRow)r;
	switch (mask)
		{
		case XmLTreeRowLevel:
			*((int *)value) = row->tree.level;
			break;
		case XmLTreeRowExpands:
			*((Boolean *)value) = row->tree.expands;
			break;
		case XmLTreeRowIsExpanded:
			*((Boolean *)value) = row->tree.isExpanded;
			break;
		}
	}

/* Only to be called through Grid class */
static int
_SetRowValues(XmLGridWidget g,
              XmLGridRow r,
              long mask)
	{
	XmLGridWidgetClass sc;
	int needsResize;
	XmLTreeRow row;
	XmLTreeWidget t;

	sc = (XmLGridWidgetClass)xmlTreeWidgetClass->core_class.superclass;
	needsResize = sc->grid_class.setRowValuesProc(g, r, mask);
	t = (XmLTreeWidget)g;
	row = (XmLTreeRow)r;
	if ((mask & XmLGridRowHeight) && needsResize)
		t->tree.recalcTreeWidth = 1;
	if (mask & XmLTreeRowLevel)
		{
		row->tree.level = t->tree.rowLevel;
		t->tree.recalcTreeWidth = 1;
		t->grid.vertVisChangedHint = 1;
		needsResize = 1;
		}
	if (mask & XmLTreeRowExpands)
		{
		row->tree.expands = t->tree.rowExpands;
		t->grid.vertVisChangedHint = 1;
		needsResize = 1;
		}
	if (mask & XmLTreeRowIsExpanded)
		{
		row->tree.isExpanded = t->tree.rowIsExpanded;
		t->grid.vertVisChangedHint = 1;
		needsResize = 1;
		}
	return needsResize;
	}

/* Only to be called through Grid class */
static int
_SetCellValuesResize(XmLGridWidget g,
		      XmLGridRow row,
		      XmLGridColumn col,
		      XmLGridCell cell,
  		      long mask)
	{
	XmLGridWidgetClass sc;
	int needsResize;

	sc = (XmLGridWidgetClass)xmlTreeWidgetClass->core_class.superclass;
	needsResize = 0;
	if (col->grid.pos == g->grid.headingColCount &&
		row->grid.pos >= g->grid.headingRowCount &&
		row->grid.pos < g->grid.headingRowCount + g->grid.rowCount)
		{
		if (mask & XmLGridCellFontList)
			{
			row->grid.heightInPixelsValid = 0;
			((XmLTreeRow)row)->tree.stringWidthValid = False;
			col->grid.widthInPixelsValid = 0;
			needsResize = 1;
			}
		if (mask & XmLGridCellString)
			{
			((XmLTreeRow)row)->tree.stringWidthValid = False;
			col->grid.widthInPixelsValid = 0;
			needsResize = 1;
			}
		}
	if (sc->grid_class.setCellValuesResizeProc(g, row, col, cell, mask))
		needsResize = 1;
	return needsResize;
	}

/*
  Utility
*/

static void
GetManagerForeground(Widget w,
                      int offset,
                      XrmValue *value)
	{
	XmLTreeWidget t;

	t = (XmLTreeWidget)w;
	value->addr = (caddr_t)&t->manager.foreground;
	}

static void
CreateDefaultPixmaps(XmLTreeWidget t)
	{
	Display *dpy;
	Window win;
	XWindowAttributes attr;
	XColor color;
	Pixmap pixmap;
	Pixel pixel;
	XImage *image;
	int i, x, y;
	enum { white = 0, black = 1, yellow = 2, gray = 3 };
	static unsigned short colors[4][3] =
		{
			{ 65535, 65535, 65535 },
			{ 0,         0,     0 },
			{ 57344, 57344,     0 },
			{ 32768, 32768, 32768 },
		};
	static unsigned char fileMask_bits[] =
		{
		0xf8, 0x0f, 0xfc, 0x1f, 0xfc, 0x3f, 0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f,
		0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f,
		0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f, 0xf8, 0x7f
		};
	static unsigned char folderMask_bits[] =
		{
		0x00, 0x00, 0x00, 0x00, 0xe0, 0x07, 0xf0, 0x0f, 0xfc, 0x3f, 0xfe, 0x7f,
		0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
		0xfe, 0xff, 0xfe, 0xff, 0xfc, 0xff, 0xf8, 0x7f
		};
	static unsigned char folderOpenMask_bits[] =
		{
		0x00, 0x00, 0x00, 0x00, 0xe0, 0x07, 0xf0, 0x0f, 0xfc, 0x3f, 0xfe, 0x7f,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xfe, 0xff,
		0xfc, 0xff, 0xfc, 0xff, 0xf8, 0xff, 0xf0, 0x7f
		};
	static char icons[3][16][16] =
		{
			{
			"   GGGGGGGGG    ",
			"  GWWWWWWWWKK   ",
			"  GWWWWWWWWKWK  ",
			"  GWWWWWWWWKKKK ",
			"  GWWWWWWWWWWGK ",
			"  GWGGGGGGGWWGK ",
			"  GWWKKKKKKKWGK ",
			"  GWWWWWWWWWWGK ",
			"  GWGGGGGGGWWGK ",
			"  GWWKKKKKKKWGK ",
			"  GWWWWWWWWWWGK ",
			"  GWGGGGGGGWWGK ",
			"  GWWKKKKKKKWGK ",
			"  GWWWWWWWWWWGK ",
			"  GGGGGGGGGGGGK ",
			"   KKKKKKKKKKKK ",
			},
			{
			"                ",
			"                ",
			"     GGGGGG     ",
			"    GYYYYYYG    ",
			"  GGYYYYYYYYGG  ",
			" GWWWWWWWWWWWYG ",
			" GWYYYYYYYYYYYGK",
			" GWYYYYYYYYYYYGK",
			" GWYYYYYYYYYYYGK",
			" GWYYYYYYYYYYYGK",
			" GWYYYYYYYYYYYGK",
			" GWYYYYYYYYYYYGK",
			" GWYYYYYYYYYYYGK",
			" GYYYYYYYYYYYYGK",
			"  GGGGGGGGGGGGKK",
			"   KKKKKKKKKKKK ",
			},
			{
			"                ",
			"                ",
			"     GGGGGG     ",
			"    GYYYYYYG    ",
			"  GGYYYYYYYYGG  ",
			" GYYYYYYYYYYYYG ",
			" GYYYYYYYYYYYYGK",
			"GGGGGGGGGGGYYYGK",
			"GWWWWWWWWWYKYYGK",
			"GWYYYYYYYYYKYYGK",
			" GYYYYYYYYYYKYGK",
			" GYYYYYYYYYYKYGK",
			"  GYYYYYYYYYYKGK",
			"  GYYYYYYYYYYKGK",
			"   GGGGGGGGGGGKK",
			"    KKKKKKKKKKK ",
			},
		};

	dpy = XtDisplay(t);
	win = XtWindow(t);
	XGetWindowAttributes(dpy, win, &attr);
	t->tree.filePixmask = XCreatePixmapFromBitmapData(dpy, win,
		(char *)fileMask_bits, 16, 16, 1L, 0L, 1);
	t->tree.folderPixmask = XCreatePixmapFromBitmapData(dpy, win,
		(char *)folderMask_bits, 16, 16, 1L, 0L, 1);
	t->tree.folderOpenPixmask = XCreatePixmapFromBitmapData(dpy, win,
		(char *)folderOpenMask_bits, 16, 16, 1L, 0L, 1);
	for (i = 0; i < 4; i++)
		{
		color.red = colors[i][0];
		color.green = colors[i][1];
		color.blue = colors[i][2];
		color.flags = DoRed | DoGreen | DoBlue;
		if (XAllocColor(dpy, attr.colormap, &color))
			t->tree.pixColors[i] = color.pixel;
		else
			{
			color.flags = 0;
			XAllocColor(dpy, attr.colormap, &color);
			t->tree.pixColors[i] = color.pixel;
			}
		}
	image = XCreateImage(dpy, attr.visual, attr.depth, ZPixmap, 0,
		NULL, 16, 16, XBitmapPad(dpy), 0);
	if (!image)
		XmLWarning((Widget)t,
			"CreateDefaultPixmaps() - can't allocate image");
	else
		image->data = (char *)malloc(image->bytes_per_line * 16);
	for (i = 0; i < 3; i++)
		{
		pixmap = XCreatePixmap(dpy, win, 16, 16, attr.depth);
		for (x = 0; x < 16; x++)
			for (y = 0; y < 16; y++)
				{
				switch (icons[i][y][x])
					{
					case ' ':
						pixel = t->core.background_pixel;
						break;
					case 'W':
						pixel = t->tree.pixColors[white];
						break;
					case 'K':
						pixel = t->tree.pixColors[black];
						break;
					case 'Y':
						pixel = t->tree.pixColors[yellow];
						break;
					case 'G':
						pixel = t->tree.pixColors[gray];
						break;
					}
				XPutPixel(image, x, y, pixel);
				}
		if (image)
			XPutImage(dpy, pixmap, t->grid.gc, image, 0, 0, 0, 0, 16, 16);
		if (i == 0)
			t->tree.filePixmap = pixmap;
		else if (i == 1)
			t->tree.folderPixmap = pixmap;
		else
			t->tree.folderOpenPixmap = pixmap;
		}
	if (image)
		XDestroyImage(image);
	t->tree.defaultPixmapsCreated = 1;
	}

static XmLTreeWidget
WidgetToTree(Widget w,
             char *funcname)
	{
	char buf[256];

	if (!XmLIsTree(w))
		{
		sprintf(buf, "%s - widget not an XmLTree", funcname);
		XmLWarning(w, buf);
		return 0;
		}
	return (XmLTreeWidget)w;
	}

/*
   Public Functions
*/

Widget
XmLCreateTree(Widget parent,
              char *name,
              ArgList arglist,
              Cardinal argcount)
	{
	return XtCreateWidget(name, xmlTreeWidgetClass, parent,
		arglist, argcount);
	}

void
XmLTreeAddRow(Widget w,
              int level,
              Boolean expands,
              Boolean isExpanded,
              int position,
              Pixmap pixmap,
              Pixmap pixmask,
              XmString string)
	{
	XmLTreeRowDefinition row;

	row.level = level;
	row.expands = expands;
	row.isExpanded = isExpanded;
	row.pixmap = pixmap;
	row.pixmask = pixmask;
	row.string = string;
	XmLTreeAddRows(w, &row, 1, position);
	}

void 
XmLTreeAddRows(Widget w,
                XmLTreeRowDefinition *rows,
                int count,
                int position)
	{
	XmLTreeWidget t;
	XmLTreeRow row;
	int i, level;
	unsigned char layoutFrozen;

	t = WidgetToTree(w, "XmLTreeAddRows()");
	if (!t || count <= 0)
		return;
	if (position < 0 || position > t->grid.rowCount)
		position = t->grid.rowCount;
	layoutFrozen = t->grid.layoutFrozen;
	if (layoutFrozen == False)
		XtVaSetValues(w,
			XmNlayoutFrozen, True,
			NULL);
	XmLGridAddRows(w, XmCONTENT, position, count);
	for (i = 0; i < count; i++)
		{
		row = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, position + i);
		if (!row)
			continue;
		level = rows[i].level;
		if (level < 0)
			level = 0;
		row->tree.level = level;
		row->tree.expands = rows[i].expands;
		row->tree.isExpanded = rows[i].isExpanded;

		XtVaSetValues(w,
			XmNrow, position + i,
			XmNcolumn, 0,
			XmNcellString, rows[i].string,
			XmNcellPixmap, rows[i].pixmap,
			XmNcellPixmapMask, rows[i].pixmask,
			NULL);
		}
	if (layoutFrozen == False)
		XtVaSetValues(w,
			XmNlayoutFrozen, False,
			NULL);
	}



void
XmLTreeDeleteChildren(Widget w,
                      int row)
{
	XmLTreeWidget t;
	XmLTreeRow rowp;
	int ii, jj, level, rows;

	t = WidgetToTree(w, "XmLTreeDeleteChildren()");

	rowp = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, row);
    level = rowp->tree.level;

	rows = t->grid.rowCount;

	ii = row + 1;
	while (ii < rows)
	{
		rowp = (XmLTreeRow)XmLGridGetRow(w, XmCONTENT, ii);
		if (rowp->tree.level <= level)
			break;
		ii++;
	}
	jj = ii - row - 1;

    if (jj > 0)
        XmLGridDeleteRows(w, XmCONTENT, row + 1, jj);
}

