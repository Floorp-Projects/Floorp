/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   Outliner.cpp -- Outliner class hack.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   */



#include "xpassert.h"
#include "xp_mem.h"

#include "Outliner.h"
#include "Outlinable.h"

#if defined(USE_MOTIF_DND)
#include "OutlinerDrop.h"
#endif /* USE_MOTIF_DND */

#include "xfe.h"
#include "icondata.h"
#include "prefapi.h"
#include "xp_qsort.h"

#include <Xfe/XfeP.h>

#if !defined(USE_MOTIF_DND)
#include <Xm/Label.h>
#endif /* USE_MOTIF_DND */

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

extern "C" {

#include "XmL/GridP.h"

};

// please change this if you change any of the *parent.gif images.
#define PIPEIMAGESIZE 16

#if defined(USE_MOTIF_DND)
#undef NONMOTIF_DND
#else
#define NONMOTIF_DND
#endif /* USE_MOTIF_DND */

#ifdef FREEIF
#undef FREEIF
#endif
#define FREEIF(obj) do { if (obj) { XP_FREE (obj); obj = 0; }} while (0)

#define MIN_COLUMN_WIDTH 6	/* So that the user can manipulate it -- make it the same as the grid's minimum */
#define MAX_COLUMN_WIDTH 10000	/* So that we don't overflow any 16-bit
								   numbers. */

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#include "xpgetstr.h"
extern int XFE_SHOW_COLUMN;
extern int XFE_HIDE_COLUMN;

fe_icon XFE_Outliner::closedParentIcon = { 0 };
fe_icon XFE_Outliner::openParentIcon = { 0 };
fe_icon XFE_Outliner::showColumnIcon = { 0 };
fe_icon XFE_Outliner::showColumnInsensIcon = { 0 };
fe_icon XFE_Outliner::hideColumnIcon = { 0 };
fe_icon XFE_Outliner::hideColumnInsensIcon = { 0 };

#if !defined(USE_MOTIF_DND)
fe_dnd_Source _xfe_dragsource = {0};
#endif /* USE_MOTIF_DND */

//
#if defined(DEBUG_tao_)
#define XDBG(x) x
#else
#define XDBG(x) 
#endif

#if defined(DEBUG_tao)
#define TRACE(str)   printf("\n At %s: %d, %s", __FILE__, __LINE__, str)
#else
#define TRACE(str)   
#endif

void
XFE_Outliner::tip_cb(Widget w, XtPointer clientData, XtPointer cb_data)
{
	XFE_Outliner *obj = (XFE_Outliner *) clientData;

	obj->tipCB(w, cb_data);
}

void
XFE_Outliner::tipCB(Widget /* w */, XtPointer cb_data)
{
	XFE_TipStringCallbackStruct* cbs = (XFE_TipStringCallbackStruct*)cb_data;
	char *strFromView = 0;
	XP_FREEIF(m_doc_msg_buf);
	XP_FREEIF(m_tip_msg_buf);

	//
	// cbs->x <= -10 is not really possible to happen!
	//
	if (cbs->x <= -10 || 
		cbs->x >= m_totalLines ||
		cbs->y < 0 ||
		cbs->y >= m_numcolumns)
		return;

	int sourcecol = m_columnIndex[cbs->y];
	if (cbs->reason == XFE_DOCSTRING) {
		strFromView = m_outlinable->getCellDocString(cbs->x, sourcecol);
		
		m_doc_msg_buf = XP_STRDUP(strFromView?strFromView:"");
		*cbs->string = m_doc_msg_buf;
		cbs->x = -10;
		cbs->y = -10;
	} else if (cbs->reason == XFE_TIPSTRING) {
		if (cbs->y >= m_numvisible && m_hideColumnsAllowed) {
			if (cbs->x != -1) {
				m_tip_msg_buf = XP_STRDUP("");
				*cbs->string = m_tip_msg_buf;
				return;
			}/* if */

			if (cbs->y == m_numvisible) {
				/* show header */
				strFromView = XP_GetString(XFE_SHOW_COLUMN);
			}/* if */
			else {
				/* hide header */
				strFromView = XP_GetString(XFE_HIDE_COLUMN);
			}/* else */
		}/* if */
		else
			strFromView = m_outlinable->getCellTipString(cbs->x, sourcecol);

		m_tip_msg_buf = XP_STRDUP(strFromView?strFromView:"");
		*cbs->string = m_tip_msg_buf;

		XRectangle rect;
		if (XmLGridRowColumnToXY(m_widget, 
								 (cbs->x < 0)?XmHEADING:XmCONTENT, 
								 (cbs->x < 0)?0:cbs->x,  
								 XmCONTENT, 
								 cbs->y, 
								 False, 
								 &rect) < 0) {
			cbs->x = -10;
			cbs->y = -10;
		}/* if */
		else {
			XDBG(printf("\n tipCB: (total,r,c,w,h)=(%d,%d,%d,%d,%d)\n",
				   m_totalLines, cbs->x, cbs->y, rect.width, rect.height));
			cbs->x = rect.x + textStart(cbs->y, rect.width);
			cbs->y = rect.y + rect.height;
		}/* else */
	}
}

int XFE_Outliner::textStart(int col, int maxX)
{
	fe_icon *icon;
	int sourcecol = m_columnIndex[col];
	int startX = 0;
	icon = m_outlinable->getColumnIcon(sourcecol);

	if (sourcecol == m_pipeColumn) {
		OutlinerAncestorInfo * ancestorInfo;
		int depth;

		m_outlinable->getTreeInfo(NULL, NULL, &depth, &ancestorInfo);
		
		for (int i = 0; i < depth; i ++) {
			if ( ( i + 1 ) * PIPEIMAGESIZE <= maxX )
				if (ancestorInfo && ancestorInfo[ i ].has_next) {
					// the the vertical pipe for this line's ancestors.
					startX += PIPEIMAGESIZE;
				}
				else {
					// don't know about this block.. must ask will.
					startX += PIPEIMAGESIZE;
				}
		}/* for i */

		if (ancestorInfo) 
			startX += PIPEIMAGESIZE;
    }/* if pipeCol */

	/* if there's an icon, we need to push the text over. */
	if (icon) {
		startX = startX + icon->width + 4;
	}/* if */
	return startX;
}

XP_Bool XFE_Outliner::isColTextFit(char *str, int row, int col)
{
	XDBG(printf("\n XFE_Outliner::isColTextFit, row=%d, col=%d, str=%s\n", 
				row, col, str));
	if (str && XP_STRLEN(str)) {

		XRectangle rect;
		if (XmLGridRowColumnToXY(m_widget, 
								 (row < 0)?XmHEADING:XmCONTENT, 
								 (row < 0)?0:row,  
								 XmCONTENT, 
								 col, 
								 False, 
								 &rect) < 0) {
			return False;
		}/* if */
		else {
			char *style = 
				(row < 0)?
				(char*)styleToTag(m_outlinable->getColumnHeaderStyle(col)):
				(char*)styleToTag(m_outlinable->getColumnStyle(col));
			
			XmString xmstr = XmStringCreate(str, style);
			XmFontList fontList;
			XtVaGetValues(m_widget,
						  XmNfontList, &fontList,
						  NULL);
			Dimension strWidth = XmStringWidth(fontList, xmstr);
			XmStringFree(xmstr);
			strWidth += textStart(col, rect.width);
			XDBG(printf("\n (str, strWidth, rect.width, row, col)=(%s, %d, %d, %d, %d)\n", 
						str, strWidth, rect.width, row, col));
			return (strWidth < (rect.width-1));
		}/* else */
	}/* if */
	else
		return True;
}/* XFE_Outliner::getStringWidth */

//

XFE_Outliner::XFE_Outliner(const char *name,
						   XFE_Outlinable *o,
						   XFE_Component *toplevel_component,
						   Widget widget_parent, 
						   Boolean constantSize, 
						   XP_Bool hasHeadings,
						   int num_columns, 
						   int num_visible,
						   int *column_widths,
						   char *geom_prefname)
	: XFE_Component(toplevel_component)
{
	m_lastRow = -2;
	m_lastCol = -2;
	m_inGrid = False;

	m_tip_msg_buf = 0;
	m_doc_msg_buf = 0;

	Widget outline;
	Arg av[10];
	int ac = 0;
	short avgwidth, avgheight;
	XmFontList fontList;
	int i;
	XP_Bool defaultGeometryOK = FALSE;

	m_geominfo = NULL;
	m_prefname = NULL;

	if (geom_prefname)
		{
			m_prefname = XP_STRDUP(geom_prefname);
			PREF_CopyCharPref(geom_prefname, &m_geominfo);
		}

	m_visibleLine = -1;
	m_DataRow = -1;
	m_draggc = NULL;
	m_dragrow = -1;

#if !defined(USE_MOTIF_DND)
	m_dragtype = FE_DND_NONE;
#else
	m_dragTargetFunc = NULL;
	m_dropTargetFunc = NULL;
	m_dragIconDataFunc = NULL;
	m_outlinerdrag = NULL;
#endif /* USE_MOTIF_DND */

	m_activity = 0;
	m_lastx = -999;
	m_lasty = -999;
#if !defined(USE_MOTIF_DND)
	m_lastmotionx = 0;
	m_lastmotiony = 0;
#endif /* USE_MOTIF_DND */
	m_totalLines = 0;
	m_numcolumns = num_columns;
	m_numvisible = num_visible;
	m_pipeColumn = -1;
	m_sortColumn = -1;
	m_resizeColumn = -1;
	m_visibleTimer = 0;

	m_hideColumnsAllowed = False;
	m_descendantSelectAllowed = False;
	m_isFocus = False;

	if (constantSize)
		{
			XtSetArg(av[ac], XmNhorizontalSizePolicy, XmCONSTANT); ac++;
		}
	else
		{
			XtSetArg(av[ac], XmNhorizontalSizePolicy, XmVARIABLE); ac++;
		}

	XtSetArg(av[ac], XmNautoSelect, False); ac++;
	XtSetArg(av[ac], XmNtraversalOn, False); ac++;

	outline = XmLCreateGrid(widget_parent, (char*)name, av, ac);

	Pixel bgColor;
	XtVaGetValues(outline, XmNbackground, &bgColor,  0);
	XtVaSetValues(outline,
				  XmNborderWidth, 2,
				  XmNborderColor,  bgColor,
				  XmNcellDefaults, True,
				  XmNcellLeftBorderType, XmBORDER_NONE,
				  XmNcellRightBorderType, XmBORDER_NONE,
				  XmNcellTopBorderType, XmBORDER_NONE,
				  XmNcellBottomBorderType, XmBORDER_NONE,
				  XmNcellAlignment, XmALIGNMENT_LEFT,
				  0);

	m_columnwidths = (int*)XP_CALLOC(m_numcolumns, sizeof(int));

	// whether or not columns are resizable
	m_columnResizable = (XP_Bool*)XP_CALLOC(m_numcolumns, sizeof(XP_Bool));
	memset(m_columnResizable, True, sizeof(XP_Bool) * m_numcolumns);
  
	m_columnIndex = (int*) XP_ALLOC(sizeof(int) * (m_numcolumns));

#if defined(USE_MOTIF_DND)
	XtAddEventHandler(outline,
					  ButtonPressMask | ButtonReleaseMask,
					  False,
					  buttonEventHandler, this);
#else
	XtInsertEventHandler(outline, ButtonPressMask | ButtonReleaseMask
						 | PointerMotionMask, False,
						 buttonEventHandler, this, XtListHead);
#endif
	XtAddCallback(outline, XmNcellDrawCallback, celldrawCallback, this);
  
	m_buttonfunc_data = XP_NEW_ZAP(OutlineButtonFuncData);
	m_flippyfunc_data = XP_NEW_ZAP(OutlineFlippyFuncData);

#if !defined(USE_MOTIF_DND)
	// initialize the drag/drop stuff
	m_dragtype = FE_DND_NONE;
	m_dragicon = 0;
	m_sourcedropfunc = 0;
#endif

	// initialize the selection stuff
	m_selBlocked = False;
	m_selectedIndices = new int[5];
	m_selectedItems = NULL;
	m_selectedItemCount = 0;
	m_selectedCount = 0;
	m_selectedSize = 5;
	m_firstSelectedIndex = (unsigned int)(1 << 31) - 1; // INT_MAX
	m_lastSelectedIndex = -1;
	m_selectionDirection = 1; // safe default.

	// initialize the magic list change (finished, starting) depth.
	m_listChangeDepth = 0;

	// keep track of our peer
	m_outlinable = o;

	// and set our base widget
	setBaseWidget(outline);

#if defined(USE_MOTIF_DND)
	m_outlinerdrop = new XFE_OutlinerDrop(m_widget, this);
	m_outlinerdrop->enable();
#endif /* USE_MOTIF_DND */

	defaultGeometryOK = setDefaultGeometry();

	if (hasHeadings)
		XmLGridAddRows(m_widget, XmHEADING, 0, 1);

	if (!defaultGeometryOK)
		{
			int i;

			D(printf ("default geometry is NOT OK!\n");)

			m_numvisible = num_visible;
			for (i = 0; i < m_numcolumns; i ++)
				m_columnIndex[i] = i;

			XtVaGetValues(outline, XmNfontList, &fontList, NULL);
			XmLFontListGetDimensions(fontList, &avgwidth, &avgheight, TRUE);
		}

	XmLGridAddColumns(outline, XmCONTENT, -1, m_numvisible);

	for (i = 0; i < m_numcolumns; i ++)
		{
			if (!defaultGeometryOK)
				m_columnwidths[i] = column_widths[i] * avgwidth;
			
			if (i < m_numvisible)
				XtVaSetValues(m_widget,
							  XmNcolumn, i,
							  XmNcolumnSizePolicy, XmCONSTANT,
							  XmNcolumnWidth, m_columnwidths[i],
							  0);
		}

	if (!defaultGeometryOK)
		{
			// since it wasn't ok before, we can be assured that it is now.
			// Save it so we won't do the above stuff again.
			rememberGeometry();
		}

	XtVaSetValues(m_widget, XmNallowColumnResize, True, 0);

	XtAddCallback(m_widget, XmNresizeCallback, resizeCallback, this);

	// Tooltip
	fe_AddTipStringCallback(outline, XFE_Outliner::tip_cb, this);

	// create our icons, if we need to.
	if (!closedParentIcon.pixmap)
		{
			Pixel bg_pixel;

			XtVaGetValues(m_widget, XmNbackground, &bg_pixel, 0);

			fe_NewMakeIcon(m_toplevel->getBaseWidget(),
						   toplevel_component->getFGPixel(),
						   bg_pixel,
						   &closedParentIcon,
						   NULL, 
						   cparent.width, cparent.height,
						   cparent.mono_bits, cparent.color_bits, 
						   cparent.mask_bits, FALSE);

			fe_NewMakeIcon(m_toplevel->getBaseWidget(),
						   toplevel_component->getFGPixel(),
						   bg_pixel,
						   &openParentIcon,
						   NULL, 
						   oparent.width, oparent.height,
						   oparent.mono_bits, oparent.color_bits,
						   oparent.mask_bits, FALSE);

			fe_NewMakeIcon(m_toplevel->getBaseWidget(),
						   toplevel_component->getFGPixel(),
						   bg_pixel,
						   &showColumnIcon,
						   NULL, 
						   showcolumn.width, showcolumn.height,
						   showcolumn.mono_bits, showcolumn.color_bits,
						   showcolumn.mask_bits, FALSE);

			fe_NewMakeIcon(m_toplevel->getBaseWidget(),
						   toplevel_component->getFGPixel(),
						   bg_pixel,
						   &showColumnInsensIcon,
						   NULL, 
						   showcolumn_i.width, showcolumn_i.height,
						   showcolumn_i.mono_bits, showcolumn_i.color_bits,
						   showcolumn_i.mask_bits, FALSE);

			fe_NewMakeIcon(m_toplevel->getBaseWidget(),
						   toplevel_component->getFGPixel(),
						   bg_pixel,
						   &hideColumnIcon,
						   NULL, 
						   hidecolumn.width, hidecolumn.height,
						   hidecolumn.mono_bits, hidecolumn.color_bits,
						   hidecolumn.mask_bits, FALSE);

			fe_NewMakeIcon(m_toplevel->getBaseWidget(),
						   toplevel_component->getFGPixel(),
						   bg_pixel,
						   &hideColumnInsensIcon,
						   NULL, 
						   hidecolumn_i.width, hidecolumn_i.height,
						   hidecolumn_i.mono_bits, hidecolumn_i.color_bits,
						   hidecolumn_i.mask_bits, FALSE);
		}
}

XFE_Outliner::XFE_Outliner()
{
	m_selBlocked = False;
	m_outlinable = NULL;
}

XFE_Outliner::~XFE_Outliner()
{
	D(printf ("In XFE_Outliner::~XFE_Outliner()\n");)
	FREEIF(m_columnwidths);
	FREEIF(m_columnIndex);
	FREEIF(m_columnResizable);
	FREEIF(m_prefname);
	FREEIF(m_selectedItems);
	FREEIF(m_geominfo);
	//
	XP_FREEIF(m_doc_msg_buf);
	XP_FREEIF(m_tip_msg_buf);

	// if our outlinable still has line data allocated,
	// tell it to release it.   
	if (m_DataRow != -1)
		m_outlinable->releaseLineData();

#if defined(USE_MOTIF_DND)
    if (m_outlinerdrop)
        delete m_outlinerdrop;
#endif /* USE_MOTIF_DND */
}

void
XFE_Outliner::setPipeColumn(int column)
{
	XP_ASSERT(column >= 0 && column < m_numcolumns);

	m_pipeColumn = column;
}

int
XFE_Outliner::getPipeColumn()
{
	return m_pipeColumn;
}

void
XFE_Outliner::setSortColumn(int column, EOutlinerSortDirection direction)
{
	XP_ASSERT(column >= -1 && column < m_numcolumns);

	m_sortColumn = column;
	m_sortDirection = direction;
}

int
XFE_Outliner::getSortColumn()
{
	return m_sortColumn;
}

void
XFE_Outliner::toggleSortDirection()
{
	if (m_sortDirection == OUTLINER_SortAscending)
		m_sortDirection = OUTLINER_SortDescending;
	else
		m_sortDirection = OUTLINER_SortAscending;
}

EOutlinerSortDirection
XFE_Outliner::getSortDirection()
{
	return m_sortDirection;
}

void
XFE_Outliner::setColumnResizable(int column,
								 XP_Bool resizable)
{
	XP_ASSERT(column >= 0 && column < m_numcolumns);
	int column_index;

	for (column_index = 0; column_index < m_numcolumns; column_index ++)
		if (m_columnIndex[ column_index ] == column)
			break;

	setColumnResizableByIndex(column_index, resizable);
}

XP_Bool
XFE_Outliner::getColumnResizable(int column)
{
	XP_ASSERT(column >= 0 && column < m_numcolumns);
	int column_index;

	for (column_index = 0; column_index < m_numcolumns; column_index ++)
		if (m_columnIndex[ column_index ] == column)
			break;

	return getColumnResizableByIndex(column_index);
}

void
XFE_Outliner::setColumnWidth(int column,
							 int width)
{
	XP_ASSERT(column >= 0 && column < m_numcolumns);
	int column_index;

	for (column_index = 0; column_index < m_numcolumns; column_index ++)
		if (m_columnIndex[ column_index ] == column)
			break;

	setColumnWidthByIndex(column_index, width);
}

int
XFE_Outliner::getColumnWidth(int column)
{
	XP_ASSERT(column >= 0 && column < m_numcolumns);
	int column_index;

	for (column_index = 0; column_index < m_numcolumns; column_index ++)
		if (m_columnIndex[ column_index ] == column)
			break;

	return getColumnWidthByIndex(column_index);
}


// Return the number of visible rows in the grid, including header rows.
int XFE_Outliner::getNumContextAndHeaderRows()
{
	XP_ASSERT(m_widget != 0);
	
	int numContentRows = 0;
	int numHeadingRows = 0;
	XtVaGetValues(m_widget,
				  XmNrows,        &numContentRows,
				  XmNheadingRows, &numHeadingRows,
				  0);

	return numContentRows + numHeadingRows;
}


/*
** Resize the grid to show all rows, but enforce a
** minimum and maximum number of rows.
*/
void XFE_Outliner::showAllRowsWithRange(int minRows, int maxRows)
{
	XP_ASSERT(m_widget != 0);
	
	int numRows = this->getNumContextAndHeaderRows();
	if((numRows > minRows)&&(numRows < maxRows)) {
		// We are within the range, show them all.
		XtVaSetValues(m_widget,
					  XmNvisibleRows, numRows,
					  0);
	} else if(numRows >= maxRows) {
		// Maximum number of rows.
		XtVaSetValues(m_widget,
					  XmNvisibleRows, maxRows,
					  0);
	} else if(numRows <= minRows) {
		// Minimum number of rows.
		XtVaSetValues(m_widget,
					  XmNvisibleRows, minRows,
					  0);		
	} else {
		XP_ASSERT(0);
	}
}

void
XFE_Outliner::setBlockSel(XP_Bool b)
{
	m_selBlocked = b;
}

void
XFE_Outliner::scroll2Item(int index)
{
	if (index >=0 && index < m_totalLines) {
		selectItemExclusive(index);
		makeVisible(index);
	}/* if */
	else
		deselectAllItems();
}

void
XFE_Outliner::setMultiSelectAllowed(Boolean flag)
{
	XP_ASSERT(m_widget != 0);

	if (flag)
		{
			XtVaSetValues(m_widget,
						  XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
						  0);
		}
	else
		{
			XtVaSetValues(m_widget, 
						  XmNselectionPolicy, XmSELECT_BROWSE_ROW, 
						  0);
		}
}

Boolean
XFE_Outliner::getMultiSelectAllowed()
{
	unsigned char selectionPolicy;

	XP_ASSERT(m_widget != 0);

	XtVaGetValues(m_widget,
				  XmNselectionPolicy, &selectionPolicy,
				  NULL);

	return (selectionPolicy == XmSELECT_MULTIPLE_ROW);
}

void
XFE_Outliner::setDescendantSelectAllowed(XP_Bool flag)
{
	m_descendantSelectAllowed = flag;
}

XP_Bool
XFE_Outliner::getDescendantSelectAllowed()
{
	return m_descendantSelectAllowed;
}

void
XFE_Outliner::setHideColumnsAllowed(XP_Bool flag)
{
	if (m_hideColumnsAllowed != flag)
		{
			if (flag)
				{
					// show the hide/show buttons.
					XtVaSetValues(m_widget,
								  XmNlayoutFrozen, True,
								  NULL);

					applyDelta(-1 * (12 + 12));

					XmLGridAddColumns(m_widget,XmCONTENT, m_numvisible, 2);

					XtVaSetValues(m_widget,
								  XmNcolumn, m_numvisible,
								  XmNcolumnWidth, 12, //XX magic number alert !!!
								  XmNcolumnSizePolicy, XmCONSTANT,
								  XmNcolumnResizable, False, // can't resize the show/hide columns.
								  NULL);

					XtVaSetValues(m_widget,
								  XmNcolumn, m_numvisible + 1,
								  XmNcolumnWidth, 12, //XX magic number alert !!!
								  XmNcolumnSizePolicy, XmCONSTANT,
								  XmNcolumnResizable, False, // can't resize the show/hide columns.
								  NULL);

					XtVaSetValues(m_widget,
								  XmNlayoutFrozen, False,
								  NULL);
				}
			else
				{
					// remove the hide/show buttons.
					XmLGridDeleteColumns(m_widget, XmCONTENT, m_numvisible, 2);

					applyDelta(12 + 12);

				}

			m_hideColumnsAllowed = flag;
		}
}

XP_Bool
XFE_Outliner::getHideColumnsAllowed()
{
	return m_hideColumnsAllowed;
}

#if !defined(USE_MOTIF_DND)
void
XFE_Outliner::setDragType(fe_dnd_Type dragtype, fe_icon *dragicon, 
						  XFE_Component *drag_source, fe_dnd_SourceDropFunc sourcedropfunc)
{
	m_dragtype = dragtype;
	m_dragicon = dragicon;
	m_source = drag_source;
	m_sourcedropfunc = sourcedropfunc;
}

fe_dnd_Type
XFE_Outliner::getDragType()
{
	return m_dragtype;
}

fe_icon *
XFE_Outliner::getDragIcon()
{
	return m_dragicon;
}

fe_dnd_SourceDropFunc
XFE_Outliner::getSourceDropFunc()
{
	return m_sourcedropfunc;
}

#else
void
XFE_Outliner::enableDragDrop(void *this_ptr,
							 FEGetDropTargetFunc drop_target_func,
							 FEGetDragTargetFunc drag_target_func,
							 FEGetDragIconDataFunc drag_icon_func,
							 FEConvertDragDataFunc drag_conv_func,
							 FEProcessTargetsFunc process_targets_func)
{
  m_dragsource = this_ptr;
  m_dropTargetFunc = drop_target_func;
  m_dragTargetFunc = drag_target_func;
  m_dragIconDataFunc = drag_icon_func;
  m_dragConvFunc = drag_conv_func;
  m_processTargetsFunc = process_targets_func;

  if (!m_outlinerdrag)
	m_outlinerdrag = new XFE_OutlinerDrag(m_widget, this);

  /* update the dropsite's idea of its targets. */
  m_outlinerdrop->update();
}


void
XFE_Outliner::getDropTargets(Atom **targets,
							 int *numtargets)
{
  if (m_dropTargetFunc == NULL)
	{
	  *targets = NULL;
	  *numtargets = 0;
	}
  else
	{
	  (*m_dropTargetFunc)(m_outlinable, targets, numtargets);
	}
}

void
XFE_Outliner::getDragTargets(int row, int column,
							 Atom **targets,
							 int *numtargets)
{
  if (m_dragTargetFunc == NULL)
	{
	  *targets = NULL;
	  *numtargets = 0;
	}
  else
	{
	  (*m_dragTargetFunc)(m_dragsource, row, column, targets, numtargets);
	}
}

fe_icon_data *
XFE_Outliner::getDragIconData(int row, int column)
{
  if (m_dragIconDataFunc == NULL)
	return NULL;
  else
	return (*m_dragIconDataFunc)(m_dragsource, row, column);
}

char *
XFE_Outliner::dragConvert(Atom atom)
{
  if (m_dragConvFunc == NULL)
	return NULL;
  else
	return (*m_dragConvFunc)(m_dragsource, atom);
}


int
XFE_Outliner::processTargets(int row, int col,
							 Atom *targets,
							 const char **data,
							 int numItems)
{
  if (m_processTargetsFunc == NULL)
	return FALSE;
  else
	return (*m_processTargetsFunc)(m_dragsource, row, col,
								   targets, data, numItems);
}
#endif /* USE_MOTIF_DND */

XP_Bool
XFE_Outliner::setDefaultGeometry()
{
	int i;
	char* ptr;
	char* ptr2;
	char* ptr3;
	int value;
	int width;
	int height = 0;

	for (i = 0; i < m_numcolumns; i ++)
		m_columnIndex[i] = i;

	ptr = m_geominfo;

	// first parse the height of the outliner in pixel
	if (ptr != NULL)
		{
			ptr2 = XP_STRCHR(ptr, 'x');
			if (ptr2) *ptr2 = '\0';
			height = atoi(ptr);

			if (height > 0 && height < MAX_COLUMN_WIDTH)
				{
					*ptr2 = 'x';

					ptr = ptr2 + 1;
				}
			else
				{
					FREEIF(m_geominfo);
					ptr = m_geominfo;
				}

		}

	// then parse the number of visible columns
	if (ptr != NULL)
		{
			ptr2 = XP_STRCHR(ptr, ')');
			if (ptr2) *ptr2 = '\0';
			ptr3 = XP_STRCHR(ptr, '(');
			if (ptr3)
				{
					m_numvisible = atoi(ptr3 + 1);
					
					if (m_numvisible <= 0 || m_numvisible > m_numcolumns)
						m_numvisible = m_numcolumns;
					
					*ptr3 = '(';
					*ptr2 = ')';
					ptr = ptr2 + 1;
				}
			else
				{
					FREEIF(m_geominfo);
					ptr = m_geominfo;
				}
		}
	
	for (i=0 ; i<m_numcolumns ; i++) 
		{
			if (ptr == NULL) 
				{
					FREEIF(m_geominfo);
					break;
				}
      
			ptr2 = XP_STRCHR(ptr, ';');
			if (ptr2) *ptr2 = '\0';
			ptr3 = XP_STRCHR(ptr, ':');
			if (!ptr3) 
				{
					FREEIF(m_geominfo);
					break;
				}
			*ptr3 = '\0';

			for (value = 0 ; value < m_numcolumns ; value++) 
				{
					if (m_outlinable->getColumnName(value) == NULL
						|| strcmp(m_outlinable->getColumnName(value), ptr) == 0)
						break;
				}

			if (value >= m_numcolumns) 
				{
					FREEIF(m_geominfo);
					break;
				}

			m_columnIndex[i] = value;
			width = atoi(ptr3 + 1);
			*ptr3 = ':';

			if (width < MIN_COLUMN_WIDTH) width = MIN_COLUMN_WIDTH;
			if (width > MAX_COLUMN_WIDTH) width = MAX_COLUMN_WIDTH;

			m_columnwidths[ i ] = width;

			if (ptr2) *ptr2++ = ';';
			ptr = ptr2;
		}

	if (m_geominfo) 
		{
			/* Check that every column was mentioned, and no duplicates
			   occurred. */
			int* check = (int*)XP_CALLOC(m_numcolumns, sizeof(int));
			for (i=0 ; i<m_numcolumns ; i++) check[i] = 0;
			for (i=0 ; i<m_numcolumns ; i++) 
				{
					int w = m_columnIndex[i];
					if (w < 0 || w > m_numcolumns || check[w]) 
						{
							FREEIF(m_geominfo);
							break;
						}
					check[w] = 1;
				}
			XP_FREE(check);
		}

	if (m_geominfo != NULL)
		XtVaSetValues(m_widget,
					  XmNheight, height,
					  NULL);

	return (m_geominfo != NULL);
}

void
XFE_Outliner::setInFocus(XP_Bool infocus)
{
   Pixel bgColor, fgColor;

   XtVaGetValues(m_widget, 
		XmNbackground, &bgColor, 
		XmNforeground, &fgColor, 0);
   if ( infocus )
	XtVaSetValues(m_widget, XmNborderColor,
		 fgColor, 0);
   else
	XtVaSetValues(m_widget, XmNborderColor,
		 bgColor, 0);

   m_isFocus = infocus;

}

XP_Bool
XFE_Outliner::isFocus()
{
	return m_isFocus;
}

void
XFE_Outliner::selectItem(int index)
{
	XP_ASSERT(m_widget != 0);

	/*
	** invalidate the line without calling invalidateLine, since
	** that function calls XmLGridRedrawRow, and we're already
	** going to redraw it with the selectrow call below.
	*/
	if (m_DataRow != -1)
		m_outlinable->releaseLineData();

	m_DataRow = -1;

	XmLGridSelectRow(m_widget, index, False);

	addSelection(index);
}


void
XFE_Outliner::selectItemExclusive(int index)
{
	XP_ASSERT(m_widget != 0);

	deselectAllItems();

	selectItem(index);
}

void
XFE_Outliner::deselectItem(int index)
{
	XP_ASSERT(m_widget != 0);

	/*
	** invalidate the line without calling invalidateLine, since
	** that function calls XmLGridRedrawRow, and we're already
	** going to redraw it with the selectrow call below.
	*/
	if (m_DataRow != -1)
		m_outlinable->releaseLineData();

	m_DataRow = -1;

	XmLGridDeselectRow(m_widget, index, False);

	if (m_selectedCount)
		removeSelection(index);
}

void
XFE_Outliner::toggleSelected(int index)
{
	XP_ASSERT(m_widget != 0);

	if (isSelected(index))
		deselectItem(index);
	else
		selectItem(index);
}

/*
** Here, we either expand or contract the selected area by
** following rules.
**
** 1) if new_index < first_index, select from new_index to last_index.
** 2) if new_index > last_index, select from first_index to new_index.
** 3) if new_index > first_index && new_index < last_index
**    
**    3a)  if new_index > (last_index + first_index) / 2, select
**         from new_index to last_index.
**    3b)  if new_index < (last_index + first_index) / 2, select
**         from first_index to new_index.
**
** funkygroovycool
*/
void
XFE_Outliner::trimOrExpandSelection(int new_index)
{
	// always do something reasonable...
	if (m_selectedCount == 0)
		selectItem(new_index);
	else
		{
			if (m_selectionDirection == -1)
				{
					if (new_index < m_lastSelectedIndex)
						{
							if (new_index < m_firstSelectedIndex)
								selectRangeByIndices(m_firstSelectedIndex, new_index);
							else if (new_index > m_firstSelectedIndex)
								deselectRangeByIndices(new_index - 1, m_firstSelectedIndex);

							/* don't need to do anything if
							   new_index == m_firstSelectedIndex */
						}
					else
						{
							deselectRangeByIndices(m_lastSelectedIndex - 1, m_firstSelectedIndex);
							selectRangeByIndices(m_lastSelectedIndex, new_index);
						}
				}
			else
				{
					if (new_index > m_firstSelectedIndex)
						{
							if (new_index > m_lastSelectedIndex)
								selectRangeByIndices(m_lastSelectedIndex, new_index);
							else if (new_index < m_lastSelectedIndex)
								deselectRangeByIndices(new_index + 1, m_lastSelectedIndex);

							/* don't need to do anything if 
							   new_index == m_lastSelectedIndex */
						}
					else
						{
							deselectRangeByIndices(m_firstSelectedIndex + 1, m_lastSelectedIndex);
							selectRangeByIndices(m_firstSelectedIndex, new_index);
						}
				}
		}

}

void
XFE_Outliner::selectAllItems()
{
	XP_ASSERT(m_widget != 0);

	XmLGridSelectAllRows(m_widget, False);

	for (int i = 0; i < m_totalLines; i ++)
		{
			selectItem(i);
		}
	
	m_selectionDirection = 1;
}

void
XFE_Outliner::deselectAllItems()
{
	XP_ASSERT(m_widget != 0);

	XmLGridDeselectAllRows(m_widget, False);

	deselectAll();
}

void
XFE_Outliner::makeVisible(int index)
{
	int firstrow, lastrow;
	Dimension height, shadowthickness;

	if (index < 0) 
		return;

	if (m_visibleTimer) 
		{
			m_visibleLine = index;
			return;
		}

	XtVaGetValues(m_widget,
				  XmNscrollRow, &firstrow, 
				  XmNheight, &height,
				  XmNshadowThickness, &shadowthickness, 
				  NULL);

	height -= shadowthickness;

	for (lastrow = firstrow + 1 ; ; lastrow++) 
		{
			XRectangle rect;

			if (XmLGridRowColumnToXY(m_widget,
									 XmCONTENT, lastrow,
									 XmCONTENT, 0,
									 False, &rect) < 0)
				{
					break;
				}
			if (rect.y + rect.height >= height) break;
		}
  
	if (index >= firstrow && index < lastrow) 
		return;

	firstrow = index - ((lastrow - firstrow) / 2);
  
	if (firstrow < 0) 
		firstrow = 0;
  
	XtVaSetValues(m_widget,
				  XmNscrollRow, firstrow, 
				  NULL);
}

void
XFE_Outliner::invalidate()
{
	if (m_DataRow != -1)
		m_outlinable->releaseLineData();

	m_DataRow = -1;

	XmLGridRedrawAll(m_widget);
}

void
XFE_Outliner::invalidateLine(int line)
{
	if (m_DataRow != -1)
		m_outlinable->releaseLineData();

	m_DataRow = -1;

	XmLGridRedrawRow(m_widget, XmCONTENT, line);
}

void
XFE_Outliner::invalidateLines(int start, int count)
{
	int i;

	for (i = start; i < start + count; i ++)
		invalidateLine(i);
}

void
XFE_Outliner::invalidateHeaders()
{
	if (m_DataRow != -1)
		m_outlinable->releaseLineData();

	m_DataRow = -1;

	XmLGridRedrawRow(m_widget, XmHEADING, 0);
}

int
XFE_Outliner::numResizableVisibleColumns(int starting_at)
{
	int i;
	int count = 0;

	for (i = starting_at; i < m_numvisible; i ++)
		{
			if (getColumnResizableByIndex(i))
				count ++;
		}

	return count;
}

/*
** applyDelta is one of the grossest functions ever, in my opinion.
**
** The idea is that we give it a delta and a column to start applying it at,
** and it will start there and divide up the delta evenly (well, using
** percentages) across all the resizable columns from the given one to the
** right-most one.  It will try to resize them all enough to get rid of the
** delta, but in the event that it can't distribute the delta entirely, it
** returns the amount left over.  This is used by resize() method below to fix
** up the resized column.
**
** That being said, the way things are done here is icky.  We build up an
** array of delta_struct's, one for each resizable column we're going to be
** dealing with.  Then we sort them so that lower indices have smaller widths.
** This is so that if we can't apply enough of the delta to a smaller width
** column, it'll get pushed off to the larger ones (well, the largest one -- 
** the last one in the array.)  We make up for all the multiplicative errors 
** -- and instances of not being able to fully apply a column delta with the 
** last column in the array.  Since it's the largest, we have the best chance
** of doing it there.
*/
typedef struct delta_struct
{
	int column_index;
	int column_width;
} delta_struct;

static int compare_deltas(const void *d1, const void *d2)
{
	delta_struct *delta1 = (delta_struct *)d1;
	delta_struct *delta2 = (delta_struct *)d2;

	return delta1->column_width - delta2->column_width;
}

int
XFE_Outliner::applyDelta(int delta, int starting_at)
{
	int current_column;
	int num_resizable = numResizableVisibleColumns(starting_at);
	int total_resizable_column_width = 0;
	int delta_distributed = 0;
	delta_struct *deltas;
	int i;

	D(printf ("Applying a delta of %d from column %d to column %d\n",
			  delta,
			  starting_at,
			  m_numvisible - 1);)

	if (num_resizable == 0)
	  return delta;

	deltas = (delta_struct*)XP_CALLOC(num_resizable, sizeof(delta_struct));
	i = 0;
	for (current_column = starting_at; current_column < m_numvisible; current_column ++)
		if (getColumnResizableByIndex(current_column))
			{
				deltas[i].column_index = current_column;
				deltas[i].column_width = getColumnWidthByIndex(current_column);
				total_resizable_column_width += deltas[i].column_width;
				i++;
			}

	XP_ASSERT(total_resizable_column_width > 0);
	if (total_resizable_column_width <= 0)
		{
			XP_FREE(deltas);
			return delta;
		}

	XP_QSORT(deltas, num_resizable, sizeof(delta_struct), compare_deltas);

	current_column = starting_at;

	for (i = 0; i < num_resizable; i ++)
		{
			int orig_column_width = deltas[i].column_width;
			int column_width;
			int column_delta = delta * ((float)orig_column_width / total_resizable_column_width);
			
			D(printf ("Column %d (Before delta = %d).  Percentage of delta applied = %f (%d pixels)\n",
					  deltas[i].column_index,
					  orig_column_width,
					  (float)orig_column_width / total_resizable_column_width,
					  column_delta);)
				
			if (i == num_resizable - 1)
				{
					/* make up for our cumulative errors with the last column */
					D(printf ("making up for the differences in column %d by adding %d to its width.\n",
							  deltas[i].column_index, delta - delta_distributed);)
					
					column_delta += delta - delta_distributed - column_delta;
				}

			column_width = orig_column_width + column_delta;
			
			if (column_width < MIN_COLUMN_WIDTH)
				{
					column_width = MIN_COLUMN_WIDTH;
				
					column_delta = column_width - orig_column_width;
				}
			else if (column_width > MAX_COLUMN_WIDTH)
				{
					column_width = MAX_COLUMN_WIDTH;

					column_delta = column_width - orig_column_width;
				}
			
			delta_distributed += column_delta;

			D(printf ("After delta = %d\n", column_width);)
			setColumnWidthByIndex(deltas[i].column_index, column_width);
#if defined(DEBUG_tao_)
			printf("\n>>XFE_Outliner::applyDelta:setColumnWidthByIndex(i=%d, ind=%d, wid=%d)", 
				   i, deltas[i].column_index, column_width);
#endif
		}

	XP_FREE(deltas);
	return delta - delta_distributed;
}

void
XFE_Outliner::hideColumn()
{
	Dimension width;
			
	m_numvisible --;

	XP_ASSERT(m_numvisible > 0);
	width = getColumnWidthByIndex(m_numvisible);

	D(printf ("I'm hiding a column of width %d\n", width);)

	XtVaSetValues(m_widget,
				  XmNlayoutFrozen, True,
				  NULL);

	XmLGridDeleteColumns(m_widget, XmCONTENT, m_numvisible, 1);
	
	applyDelta(width);

	XtVaSetValues(m_widget,
				  XmNlayoutFrozen, False,
				  NULL);

	rememberGeometry();
}

void
XFE_Outliner::showColumn()
{
	int width;

	XP_ASSERT(m_numvisible < m_numcolumns);

	width = getColumnWidthByIndex( m_numvisible );

	D(printf ("I'm showing a column of width %d\n", width);)

	XtVaSetValues(m_widget,
				  XmNlayoutFrozen, True,
				  NULL);

	if (m_resizeColumn != -1 && m_columnIndex [ m_resizeColumn ] < m_numvisible)
		{
			Dimension column_width;
			
			column_width = getColumnWidthByIndex(m_columnIndex[ m_resizeColumn] );
			
			setColumnWidthByIndex(m_columnIndex[ m_resizeColumn ],
								  column_width - width);
		}
	else
		{
			applyDelta(-1 * width);
		}
   
	XmLGridRow rowp;
	XmLGridColumn colp;

	rowp = XmLGridGetRow(m_widget, XmHEADING, 0);
	colp = XmLGridGetColumn(m_widget, XmCONTENT, m_numvisible);

	Pixel row_cell_bg;

	XtVaGetValues(m_widget,
				  XmNrowPtr, rowp,
				  XmNcolumnPtr, colp,
				  XmNcellBackground, &row_cell_bg,
				  NULL);

	XmLGridAddColumns(m_widget, XmCONTENT, m_numvisible, 1);

	XtVaSetValues(m_widget,
				  XmNcolumn, m_numvisible,
				  XmNcolumnSizePolicy, XmCONSTANT,
				  XmNcolumnWidth, width,
				  XmNcolumnResizable, m_columnResizable[ m_numvisible ],
				  NULL);

	XtVaSetValues(m_widget,
				  XmNlayoutFrozen, False,
				  NULL);

	XtVaSetValues(m_widget,
				  XmNrowType, XmHEADING,
				  XmNcellBackground, row_cell_bg,
				  NULL);

	m_numvisible ++;

	rememberGeometry();
}

void
XFE_Outliner::moveColumn(int column_to_move,
						 int destination)
{
	int delta;
	int tmp;
	int tmp_width;
	XP_Bool tmp_resizable;
	int i;

	if (column_to_move < destination)
		delta = 1;
	else
		delta = -1;

	/* save off all information about the column number we're moving */
	tmp = m_columnIndex[ column_to_move ];
	tmp_width = m_columnwidths[ column_to_move ];
	tmp_resizable = m_columnResizable[ column_to_move ];

	/* move all the other columns out of the way. */
	for (i = column_to_move; i != destination; i += delta)
		{
			m_columnIndex[ i ] = m_columnIndex[ i + delta ];
			m_columnwidths[ i ] = m_columnwidths[ i + delta ];
			m_columnResizable[ i ] = m_columnResizable[ i + delta ];
		}

	/* replace the column we moved in the place we moved it to */
	m_columnIndex[ destination ] = tmp;
	m_columnwidths[ destination ] = tmp_width;
	m_columnResizable[ destination ] = tmp_resizable;

	XmLGridMoveColumns(m_widget, destination, column_to_move, 1);

	rememberGeometry();
}

int
XFE_Outliner::getSelection(const int **indices, int *count)
{
	if (indices)
		*indices = m_selectedIndices;

	if (count)
		*count = m_selectedCount;

	return m_selectedCount;
}

int
XFE_Outliner::getTotalLines()
{
	return m_totalLines;
}

int
XFE_Outliner::XYToRow(int x, int y, XP_Bool *nearbottom)
{
	int row;
	int column;
	unsigned char rowtype;
	unsigned char coltype;

	if (x < 0 || y < 0 ||
		x >= _XfeWidth(m_widget) || y >= _XfeHeight(m_widget)) 
		{
			return -1;
		}

	if (XmLGridXYToRowColumn(m_widget, x, y, &rowtype, &row,
							 &coltype, &column) < 0) {
		return -1;
	}

	if (rowtype != XmCONTENT || coltype != XmCONTENT) 
      return -1;

	if (nearbottom) {
		int row2;
		*nearbottom = (XmLGridXYToRowColumn(m_widget, x, y + 5,
                                            &rowtype, &row2,
                                            &coltype, &column) >= 0 &&
					   row2 > row);
	}
	return row;
}

void
XFE_Outliner::addSelection(int selected)
{
	int i;

	// if it's already selected, just return.
	for (i = 0; i < m_selectedCount; i ++) 
		{
			if ( m_selectedIndices [ i ] == selected )
				{
					return;
				}
		}

	if (i >= m_selectedSize)
		{
			// Resize storage array (exponential growth)
			int *tmp = new int [ m_selectedSize * 2 ];
			memcpy (tmp, m_selectedIndices, m_selectedSize * sizeof(int));
			delete [] m_selectedIndices;
			m_selectedIndices = tmp;
			m_selectedSize *= 2;
		}
 
	m_selectedIndices [ i ] = selected;
	m_selectedCount ++;

	if (selected > m_lastSelectedIndex)
		m_lastSelectedIndex = selected;
	if (selected < m_firstSelectedIndex)
		m_firstSelectedIndex = selected;

    D(printf ("m_firstSelectedIndex = %d\n", m_firstSelectedIndex);)
	D(printf ("m_lastSelectedIndex = %d\n", m_lastSelectedIndex);)

	invalidateLine( selected );
}

void
XFE_Outliner::removeSelection(int selected)
{
	int i, j;

	for (i = 0, j = 0; i < m_selectedCount; i ++) 
		{
			if (m_selectedIndices [ i ] == selected)
				{
					invalidateLine( selected );
				}
			else
				{
					m_selectedIndices [ j ] = m_selectedIndices [ i ];
					j ++;
				}
		}

	m_selectedCount = j;

	m_firstSelectedIndex = (unsigned int)(1 << 31) - 1;
	m_lastSelectedIndex = -1;

	for (j = 0; j < m_selectedCount; j ++)
		{
			if (m_selectedIndices [ j ] > m_lastSelectedIndex)
				m_lastSelectedIndex = m_selectedIndices [ j ];
			if (m_selectedIndices [ j ] < m_firstSelectedIndex)
				m_firstSelectedIndex = m_selectedIndices [ j ];
		}

    D(printf ("m_firstSelectedIndex = %d\n", m_firstSelectedIndex);)
	D(printf ("m_lastSelectedIndex = %d\n", m_lastSelectedIndex);)
}

void
XFE_Outliner::deselectAll()
{
	m_selectedCount = 0;
	m_firstSelectedIndex = (unsigned int)(1 << 31) - 1;
	m_lastSelectedIndex = -1;
	m_selectionDirection = 1;
}

void
XFE_Outliner::deselectRangeByIndices(int selection_begin,
									 int selection_end)
{
	int i;

	if (selection_begin > selection_end)
		{
			int tmp;
			tmp = selection_end;
			selection_end = selection_begin;
			selection_begin = tmp;
		}

	for (i = selection_begin; i <= selection_end; i ++)
		{
			deselectItem(i);
		}
}

Boolean
XFE_Outliner::isSelected (int line)
{
	int i;

	for (i = 0; i < m_selectedCount; i ++) 
		{
			if ( m_selectedIndices[ i ] == line)
				{
					return TRUE;
				}
		}

	return FALSE;
}

void
XFE_Outliner::selectRangeByIndices(int selection_begin, int selection_end)
{
	int i;

	/*
	** we need to keep track of the direction the user last selected in
	** whether the first selected item was below or above the newest
	** selected item.)  This comes in handy in the trimOrExpandSelection
	** routine where we need to know which range to deselect.
	*/
	m_selectionDirection = XfeSgn(selection_end - selection_begin);

	if (selection_begin > selection_end)
		{
			int tmp;
			tmp = selection_end;
			selection_end = selection_begin;
			selection_begin = tmp;
		}

	for (i = selection_begin; i <= selection_end; i ++)
		{
			selectItem(i);
		}
}

Boolean
XFE_Outliner::insertLines(int start, int count)
{
	int i;
	XP_Bool firstSelectedDone = False;
	XP_Bool lastSelectedDone = False;

	// shift down the selected items (if they're after the start)
	for (i = 0; i < m_selectedCount; i ++) 
		{
			if (m_selectedIndices[ i ] >= start)
				{
					if (!firstSelectedDone && 
						m_selectedIndices[ i ] == m_firstSelectedIndex)
						{
							m_firstSelectedIndex += count;
							firstSelectedDone = True;
						}

					if (!lastSelectedDone &&
						m_selectedIndices[ i ] == m_lastSelectedIndex)
						{
							m_lastSelectedIndex += count;
							lastSelectedDone = True;
						}

					m_selectedIndices[ i ] += count;
				}
		}

	// now invalidate the new lines so they get drawn
	invalidateLines(start, m_totalLines - start + count);

	return FALSE;
}

Boolean
XFE_Outliner::deleteLines(int start, int count)
{
	Boolean res = FALSE;
	int i, j;
	XP_Bool firstSelectedDone = False;
	XP_Bool lastSelectedDone = False;


	for (i = 0, j = 0; i < m_selectedCount; i++) 
		{
			if ( m_selectedIndices[i] >= start ) 
				{
					if (m_selectedIndices[i] < (start + count)) 
						{
							res = TRUE;
							continue;
						} 
					else 
						{
							if (!firstSelectedDone && 
								m_selectedIndices[ i ] == m_firstSelectedIndex)
								{
									m_firstSelectedIndex -= count;
									firstSelectedDone = True;
								}
							
							if (!lastSelectedDone &&
								m_selectedIndices[ i ] == m_lastSelectedIndex)
								{
									m_lastSelectedIndex -= count;
									lastSelectedDone = True;
								}

							m_selectedIndices[i] -= count; // Shift into gap
						}
				}
			m_selectedIndices[j] = m_selectedIndices[i];
			j++;
		}

	invalidateLines(start, m_totalLines - start + count);

	return FALSE;
}

void 
XFE_Outliner::setColumnResizableByIndex(int column_index,
										XP_Bool resizable)
{
	if (column_index < m_numvisible)
		{
			XtVaSetValues(m_widget,
						  XmNcolumn, column_index,
						  XmNcolumnResizable, resizable,
						  NULL);
		}

	m_columnResizable[ column_index ] = resizable;
}

XP_Bool
XFE_Outliner::getColumnResizableByIndex(int column_index)
{
	return m_columnResizable[ column_index ];
}

void
XFE_Outliner::setColumnWidthByIndex(int column_index, int width)
{
	// clamp the width to within acceptable limits.
	if (width < MIN_COLUMN_WIDTH)
		width = MIN_COLUMN_WIDTH;
	else if (width > MAX_COLUMN_WIDTH)
		width = MAX_COLUMN_WIDTH;

	if (column_index < m_numvisible)
		{
			XtVaSetValues(m_widget,
						  XmNcolumn, column_index,
						  XmNcolumnSizePolicy, XmCONSTANT,
						  XmNcolumnWidth, width,
						  NULL);
		}

	m_columnwidths[ column_index ] = width;
}

int
XFE_Outliner::getColumnWidthByIndex(int column_index)
{
	return m_columnwidths[ column_index ];
}

void
XFE_Outliner::rememberGeometry()
{
	int i;
	char *ptr;
	int length;
	Dimension height;

	FREEIF(m_geominfo);

	length = m_numcolumns * 25 + 40; /* ### got a better size in mind? */

	XtVaGetValues(m_widget,
				  XmNheight, &height,
				  NULL);

	m_geominfo = (char*)XP_ALLOC(length);
	ptr = m_geominfo;

	PR_snprintf(ptr, m_geominfo + length - ptr,
				"%dx(%d)", height, m_numvisible);
	ptr += strlen(ptr);

	for (i = 0; i < m_numcolumns; i ++)
		{
			PR_snprintf(ptr, m_geominfo + length - ptr,
						"%s:%d;", 
						m_outlinable->getColumnName(m_columnIndex[i]), 
						getColumnWidthByIndex(i));

			ptr += strlen(ptr);
		}

	if (ptr > m_geominfo) *ptr = '\0';

	if (m_prefname)
		{
			D(printf ("PREF[%s] = %s\n", m_prefname, m_geominfo);)
			PREF_SetCharPref(m_prefname, m_geominfo);
		}

	FREEIF(m_geominfo);
}

void
XFE_Outliner::PixmapDraw(Pixmap pixmap, Pixmap mask,
						 int pixmapWidth, int pixmapHeight, unsigned char alignment, GC gc,
						 XRectangle *rect, XRectangle *clipRect, Pixel bg_pixel)
{
	Display *dpy;
	Window win;
	int needsClip;
	int x, y, width, height;

	XP_ASSERT(pixmap != (Pixmap)0);

	if (pixmap == XmUNSPECIFIED_PIXMAP) return;
	if (rect->width <= 4 || rect->height <= 4) return;
	if (clipRect->width < 3 || clipRect->height < 3) return;
	if (!XtIsRealized(m_widget)) return;
	dpy = XtDisplay(m_widget);
	win = XtWindow(m_widget);

	width = pixmapWidth;
	height = pixmapHeight;
	if (!width || !height) {
		alignment = XmALIGNMENT_TOP_LEFT;
		width = clipRect->width - 4;
		height = clipRect->height - 4;
	}

	if (alignment == XmNONE)
		{
			x = rect->x;
		}
	else if (alignment == XmALIGNMENT_TOP_LEFT ||
			 alignment == XmALIGNMENT_LEFT ||
			 alignment == XmALIGNMENT_BOTTOM_LEFT) 
		{
			x = rect->x + 2;
		} 
	else if (alignment == XmALIGNMENT_TOP ||
			 alignment == XmALIGNMENT_CENTER ||
			 alignment == XmALIGNMENT_BOTTOM) 
		{
			x = rect->x + ((int)rect->width - width) / 2;
		} 
	else 
		{
			x = rect->x + rect->width - width - 2;
		}

	if (alignment == XmNONE)
		{
			y = rect->y;
		}
	if (alignment == XmALIGNMENT_TOP ||
		alignment == XmALIGNMENT_TOP_LEFT ||
		alignment == XmALIGNMENT_TOP_RIGHT) 
		{
			y = rect->y + 2;
		} 
	else if (alignment == XmALIGNMENT_LEFT ||
			 alignment == XmALIGNMENT_CENTER ||
			 alignment == XmALIGNMENT_RIGHT) 
		{
			y = rect->y + ((int)rect->height - height) / 2;
		} 
	else 
		{
			y = rect->y + rect->height - height - 2;
		}

	needsClip = 1;
	if (clipRect->x <= x &&
		clipRect->y <= y &&
		clipRect->x + clipRect->width >= x + width &&
		clipRect->y + clipRect->height >= y + height) {
		needsClip = 0;
	}

	if (needsClip) 
		{
			Pixmap tmp_pixmap;
			GC tmp_gc;
			XWindowAttributes attr;

			XGetWindowAttributes(dpy, win, &attr);
			tmp_pixmap = XCreatePixmap(dpy, win, clipRect->width, clipRect->height, attr.depth);
			tmp_gc = XCreateGC(dpy, tmp_pixmap, 0, NULL);

			XSetForeground(dpy, tmp_gc, bg_pixel);

			XFillRectangle(dpy, tmp_pixmap, tmp_gc, 0, 0, clipRect->width + 1, clipRect->height + 1);

			if (mask)
				{
					XSetClipMask(dpy, tmp_gc, mask);
					XSetClipOrigin(dpy, tmp_gc, 0, 0);
				}

			XCopyArea(dpy, pixmap, tmp_pixmap, tmp_gc, 0, 0, width, height, 0, 0);

			XFreeGC(dpy, tmp_gc);

			XSetClipRectangles(dpy, gc, 0, 0, clipRect, 1, Unsorted);

			XSetGraphicsExposures(dpy, gc, False);
			XCopyArea(dpy, tmp_pixmap, win, gc, 0, 0, width, height, x, y);
			XSetGraphicsExposures(dpy, gc, True);

			XSetClipMask(dpy, gc, None);

			XFreePixmap(dpy, tmp_pixmap);
		} 
	else
		{
			if (mask)
				{
					XSetClipMask(dpy, gc, mask);
					XSetClipOrigin(dpy, gc, x, y);
				}
			XSetGraphicsExposures(dpy, gc, False);
			XCopyArea(dpy, pixmap, win, gc, 0, 0, width, height, x, y);
			XSetGraphicsExposures(dpy, gc, True);

			if (mask) 
				{
					XSetClipMask(dpy, gc, None);
				}
		}
}

EOutlinerPipeType
XFE_Outliner::getPipeType(XP_Bool expandable,
						  XP_Bool expanded,
						  int depth,
						  OutlinerAncestorInfo *ancestorInfo)
{
	if (expandable)
		{
			if (ancestorInfo->has_prev && ancestorInfo->has_next)
				return (expanded ? PIPE_OPENMIDDLEPARENT : PIPE_CLOSEDMIDDLEPARENT);
			else if (ancestorInfo->has_prev)
				return (expanded ? PIPE_OPENBOTTOMPARENT : PIPE_CLOSEDBOTTOMPARENT);
			else if (ancestorInfo->has_next && depth)
				return (expanded ? PIPE_OPENMIDDLEPARENT : PIPE_CLOSEDMIDDLEPARENT);
			else if (ancestorInfo->has_next)
				return (expanded ? PIPE_OPENTOPPARENT : PIPE_CLOSEDTOPPARENT);
			else if (depth)
				return (expanded ? PIPE_OPENBOTTOMPARENT : PIPE_CLOSEDBOTTOMPARENT);
			else
				return (expanded ? PIPE_OPENSINGLEPARENT : PIPE_CLOSEDSINGLEPARENT);
		}
	else /* !expandable */
		{
			if (ancestorInfo->has_prev && ancestorInfo->has_next)
				return PIPE_MIDDLEITEM;
			else if (ancestorInfo->has_prev)
				return PIPE_BOTTOMITEM;
			else if (ancestorInfo->has_next && depth)
				return PIPE_MIDDLEITEM;
			else if (ancestorInfo->has_next)
				return PIPE_TOPITEM;
			else if (depth)
				return PIPE_BOTTOMITEM;
		}

	return PIPE_EMPTYITEM;
}

void
XFE_Outliner::drawDottedLine(GC gc,
							 XRectangle *clipRect,
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
				if (((x & 1) == (y & 1)) ||
					x < clipRect->x ||
					x >= (clipRect->x + (int)clipRect->width) ||
					y < clipRect->y ||
					y >= (clipRect->y + (int)clipRect->height))
					continue;
				points[i].x = x;
				points[i].y = y;
				if (++i < 100)
					continue;
				XDrawPoints(XtDisplay(m_widget), XtWindow(m_widget), gc, points, i, CoordModeOrigin);
				i = 0;
			}
	if (i)
		XDrawPoints(XtDisplay(m_widget), XtWindow(m_widget), gc, points, i, CoordModeOrigin);
}

void
XFE_Outliner::pipeDraw(XmLGridCallbackStruct *call)
{
    /*
	** we draw the pipes necessary for this line, and adjust the 
	** rectangles within the callback struct to reflect the new,
	** probably smaller, area.
	*/
	XmLGridDrawStruct *draw = call->drawInfo;
	Boolean drawSelected = draw->drawSelected;
	OutlinerAncestorInfo * ancestorInfo;
	int depth;
	int i;
	int maxX = draw->cellRect->width;
	XP_Bool expanded, expandable;

	m_outlinable->getTreeInfo(&expandable, &expanded, &depth, &ancestorInfo);

	XSetForeground(XtDisplay(m_widget), draw->gc, draw->foreground);

	/* save space to draw the descendantselect stuff. */
	if (m_descendantSelectAllowed)
		{
			draw->cellRect->x += PIPEIMAGESIZE; // XXX
			draw->cellRect->width -= PIPEIMAGESIZE;
		}

	for (i = 0; i < depth; i ++)
		if ( ( i + 1 ) * PIPEIMAGESIZE <= maxX )
			if (ancestorInfo && ancestorInfo[ i ].has_next) 
				{
					// the the vertical pipe for this line's ancestors.
					drawDottedLine(draw->gc,
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y + draw->cellRect->height);
			 
					draw->cellRect->x += PIPEIMAGESIZE;
					draw->cellRect->width -= PIPEIMAGESIZE;
				}
			else
				{
					// don't know about this block.. must ask will.
					draw->cellRect->x += PIPEIMAGESIZE;
					draw->cellRect->width -= PIPEIMAGESIZE;
				}

	if (ancestorInfo) 
		{
			fe_icon *parent_icon;
			fe_icon *spool_icon = NULL;
			EOutlinerPipeType pipeType = getPipeType(expandable, expanded, depth, &ancestorInfo[depth]);

			if (m_descendantSelectAllowed && depth == 0 /*&& expandable*/)
				spool_icon = m_outlinable->getColumnIcon(-1 /* special super secret column */);

			// draw the pipe specific to this line

			switch (pipeType)
				{
				case PIPE_OPENTOPPARENT:
				case PIPE_OPENMIDDLEPARENT:
				case PIPE_OPENSINGLEPARENT:
				case PIPE_OPENBOTTOMPARENT:
					parent_icon = &openParentIcon;
					break;
				case PIPE_CLOSEDTOPPARENT:
				case PIPE_CLOSEDMIDDLEPARENT:
				case PIPE_CLOSEDSINGLEPARENT:
				case PIPE_CLOSEDBOTTOMPARENT:
					parent_icon = &closedParentIcon;
					break;
				default:
					parent_icon = 0;
				}

			if (pipeType == PIPE_CLOSEDSINGLEPARENT
				|| pipeType == PIPE_OPENSINGLEPARENT)
				{
					drawDottedLine(draw->gc, 
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, 
								   draw->cellRect->y + draw->cellRect->height/2,
								   draw->cellRect->x + PIPEIMAGESIZE - 2,
								   draw->cellRect->y + draw->cellRect->height/2);
				}

			if (pipeType == PIPE_TOPITEM
				|| pipeType == PIPE_OPENTOPPARENT
				|| pipeType == PIPE_CLOSEDTOPPARENT)
				{
					drawDottedLine(draw->gc, 
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y + draw->cellRect->height/2,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y + draw->cellRect->height);

					drawDottedLine(draw->gc, 
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, 
								   draw->cellRect->y + draw->cellRect->height/2,
								   draw->cellRect->x + PIPEIMAGESIZE - 2,
								   draw->cellRect->y + draw->cellRect->height/2);

				}
			else if (pipeType == PIPE_BOTTOMITEM
					 || pipeType == PIPE_OPENBOTTOMPARENT
					 || pipeType == PIPE_CLOSEDBOTTOMPARENT)
				{
					drawDottedLine(draw->gc, 
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y + draw->cellRect->height/2);

					drawDottedLine(draw->gc, 
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, 
								   draw->cellRect->y + draw->cellRect->height/2,
								   draw->cellRect->x + PIPEIMAGESIZE - 2,
								   draw->cellRect->y + draw->cellRect->height/2);
				}
			else if (pipeType == PIPE_MIDDLEITEM
					 || pipeType == PIPE_OPENMIDDLEPARENT
					 || pipeType == PIPE_CLOSEDMIDDLEPARENT)
				{
					drawDottedLine(draw->gc, 
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y,
								   draw->cellRect->x + PIPEIMAGESIZE/2, draw->cellRect->y + draw->cellRect->height);
	  
					drawDottedLine(draw->gc, 
								   call->clipRect,
								   draw->cellRect->x + PIPEIMAGESIZE/2, 
								   draw->cellRect->y + draw->cellRect->height/2,
								   draw->cellRect->x + PIPEIMAGESIZE - 2,
								   draw->cellRect->y + draw->cellRect->height/2);
				}

			if (m_descendantSelectAllowed && spool_icon)
				{
				    // take back our saved space.
					draw->cellRect->x -= PIPEIMAGESIZE;
					draw->cellRect->width += PIPEIMAGESIZE;

					PixmapDraw(spool_icon->pixmap, spool_icon->mask, 
							   spool_icon->width, spool_icon->height,
							   XmNONE, draw->gc, draw->cellRect,
							   call->clipRect,
							   drawSelected ? draw->selectBackground : draw->background);

					draw->cellRect->x += PIPEIMAGESIZE;
					draw->cellRect->width -= PIPEIMAGESIZE;
				}

			if (parent_icon)
				{
					PixmapDraw(parent_icon->pixmap, parent_icon->mask, 
							   parent_icon->width, parent_icon->height,
							   XmNONE, draw->gc, draw->cellRect,
							   call->clipRect,
							   drawSelected ? draw->selectBackground : draw->background);
				}

#if 0
			if (pipeType != PIPE_EMPTYITEM)
				{
#endif
					draw->cellRect->x += PIPEIMAGESIZE;
					draw->cellRect->width -= PIPEIMAGESIZE;
#if 0
				}
#endif
		}
}

void
XFE_Outliner::showHeaderClick()
{
	D(printf ("trying to show column\n");)
	if (m_numvisible < m_numcolumns)
		{
			showColumn();
			D(printf ("succeeded.\n");)
		}
}

void
XFE_Outliner::hideHeaderClick()
{
	D(printf ("trying to hide column\n");)
	if (m_numvisible > 1)
		{
			hideColumn();
			D(printf ("succeeded.\n");)
		}
}

void
XFE_Outliner::showHeaderDraw(XmLGridCallbackStruct *call)
{
	XmLGridDrawStruct *draw = call->drawInfo;
	Boolean drawSelected = draw->drawSelected;
	fe_icon *icon;

	_XmDrawShadows(XtDisplay(m_widget),
				   XtWindow(m_widget),
				   ((XmLGridWidget)m_widget)->manager.top_shadow_GC,
				   ((XmLGridWidget)m_widget)->manager.bottom_shadow_GC,
				   draw->cellRect->x, draw->cellRect->y,
				   draw->cellRect->width, draw->cellRect->height,
				   1, XmSHADOW_OUT);

	draw->cellRect->x += 1;
	draw->cellRect->width -= 2;
	draw->cellRect->y += 1;
	draw->cellRect->height -= 2;

	if (m_numvisible < m_numcolumns)
		icon = &showColumnIcon;
	else
		icon = &showColumnInsensIcon;
	PixmapDraw(icon->pixmap, icon->mask, 
			   icon->width, icon->height,
			   XmALIGNMENT_CENTER, draw->gc, draw->cellRect,
			   call->clipRect,
			   drawSelected ? draw->selectBackground : draw->background);
}

void
XFE_Outliner::hideHeaderDraw(XmLGridCallbackStruct *call)
{
	XmLGridDrawStruct *draw = call->drawInfo;
	Boolean drawSelected = draw->drawSelected;
	fe_icon *icon;

	_XmDrawShadows(XtDisplay(m_widget),
				   XtWindow(m_widget),
				   ((XmLGridWidget)m_widget)->manager.top_shadow_GC,
				   ((XmLGridWidget)m_widget)->manager.bottom_shadow_GC,
				   draw->cellRect->x, draw->cellRect->y,
				   draw->cellRect->width, draw->cellRect->height,
				   1, XmSHADOW_OUT);
  
	draw->cellRect->x += 1;
	draw->cellRect->width -= 2;
	draw->cellRect->y += 1;
	draw->cellRect->height -= 2;

	if (m_numvisible > 1)
		icon = &hideColumnIcon;
	else
		icon = &hideColumnInsensIcon;

	PixmapDraw(icon->pixmap, icon->mask, 
			   icon->width, icon->height,
			   XmALIGNMENT_CENTER, draw->gc, draw->cellRect,
			   call->clipRect,
			   drawSelected ? draw->selectBackground : draw->background);
}

void
XFE_Outliner::headerCellDraw(XmLGridCallbackStruct *call)
{
	XmLGridDrawStruct *draw = call->drawInfo;
	Boolean drawSelected = draw->drawSelected;
	char *cstr;
	char *style;
	fe_icon *icon;
	int sourcecol;
	XRectangle /*r,*/ textRect;
	XmString xmstr = NULL;

	sourcecol = m_columnIndex[call->column];
  
	style = (char*)styleToTag(m_outlinable->getColumnHeaderStyle(sourcecol));
	cstr = m_outlinable->getColumnHeaderText(sourcecol);
	icon = m_outlinable->getColumnHeaderIcon(sourcecol);
  
	/*
	** This is pretty much a hack for the mail/news stuff.
	** we need to keep two columns together, one that has an
	** icon and one that has text, so we just make them one
	** column, and handle the button clicks specially, and also
	** draw funky shadows in the case that a header has both
	** text and an icon.
	*/
	if (cstr && icon)
		{
			_XmDrawShadows(XtDisplay(m_widget),
						   XtWindow(m_widget),
						   ((XmLGridWidget)m_widget)->manager.top_shadow_GC,
						   ((XmLGridWidget)m_widget)->manager.bottom_shadow_GC,
						   draw->cellRect->x, draw->cellRect->y,
						   icon->width + 2, draw->cellRect->height,
						   1, XmSHADOW_OUT);
			_XmDrawShadows(XtDisplay(m_widget),
						   XtWindow(m_widget),
						   ((XmLGridWidget)m_widget)->manager.top_shadow_GC,
						   ((XmLGridWidget)m_widget)->manager.bottom_shadow_GC,
						   draw->cellRect->x + icon->width + 2, 
						   draw->cellRect->y,
						   draw->cellRect->width - (icon->width + 2),
						   draw->cellRect->height,
						   1, XmSHADOW_OUT);
		}
	else
		{
			_XmDrawShadows(XtDisplay(m_widget),
						   XtWindow(m_widget),
						   ((XmLGridWidget)m_widget)->manager.top_shadow_GC,
						   ((XmLGridWidget)m_widget)->manager.bottom_shadow_GC,
						   draw->cellRect->x, draw->cellRect->y,
						   draw->cellRect->width, draw->cellRect->height,
						   1, XmSHADOW_OUT);
		}

	draw->cellRect->x += 1;
	draw->cellRect->width -= 2;
	draw->cellRect->y += 1;
	draw->cellRect->height -= 2;

	if (icon)
		{
			PixmapDraw(icon->pixmap, icon->mask, 
					   icon->width, icon->height,
					   XmALIGNMENT_LEFT, draw->gc, draw->cellRect,
					   call->clipRect,
					   drawSelected ? draw->selectBackground : draw->background);
      
			textRect.x = draw->cellRect->x + icon->width + 4;
			textRect.y = draw->cellRect->y;
			textRect.width = draw->cellRect->width - icon->width - 4;
			textRect.height = draw->cellRect->height;
		}
	else
		{
			textRect = *draw->cellRect;
		}

	if (cstr)
		{
			xmstr = XmStringCreate(cstr, style);

			XSetForeground(XtDisplay(m_widget), draw->gc,
						   draw->foreground);

			XmLStringDraw(m_widget, xmstr, draw->stringDirection, draw->fontList, draw->alignment,
						  draw->gc, &textRect, call->clipRect);
		}

	if (xmstr) XmStringFree(xmstr);

	if (sourcecol == m_sortColumn
		&& textRect.width > textRect.height - 4)
		{
			int arrow_direction = (m_sortDirection == OUTLINER_SortAscending 
								   ? XmARROW_DOWN : XmARROW_UP);
			int arrow_size = textRect.height - 4;

			if (arrow_size > 0)
				{
					XSetForeground(XtDisplay(m_widget), draw->gc,
								   draw->background);
					
					_XmDrawArrow(XtDisplay(m_widget),
								 XtWindow(m_widget),
								 ((XmManagerWidget)m_widget)->manager.bottom_shadow_GC,
								 ((XmManagerWidget)m_widget)->manager.top_shadow_GC,
								 draw->gc,
								 textRect.x + textRect.width - arrow_size - 4,
								 textRect.y + (textRect.height / 2 - arrow_size / 2),
								 arrow_size,
								 arrow_size,
								 2,
								 arrow_direction);
				}
		}
}

void
XFE_Outliner::contentCellDraw(XmLGridCallbackStruct *call)
{
	XmLGridDrawStruct *draw = call->drawInfo;
	Boolean drawSelected = draw->drawSelected;
	char *cstr;
	XmString xmstr = NULL;
	fe_icon *icon;
	XRectangle textRect;
	int sourcecol;

	/*
	** This function will get called (potentially) many times for
	** one line -- once per cell.  We only do the aquireLineData
	** once, for the first cell.
	*/
	if (call->row != m_DataRow) 
		{
			// if the last line was valid, get rid of it.
			if (m_DataRow != -1)
				m_outlinable->releaseLineData();

			m_DataRow = call->row;

			// get the new line's info.
			if (! m_outlinable->acquireLineData(call->row) )
				{
					m_DataRow = -1;
					return;
				}
		}

	sourcecol = m_columnIndex[call->column];

	cstr = m_outlinable->getColumnText(sourcecol);
	icon = m_outlinable->getColumnIcon(sourcecol);

	if (sourcecol == m_pipeColumn)
		pipeDraw(call);

	/* if there's an icon, we need to push the text over. */
	if (icon)
		{
			PixmapDraw(icon->pixmap, icon->mask, 
					   icon->width, icon->height,
					   XmALIGNMENT_LEFT, draw->gc, draw->cellRect,
					   call->clipRect,
					   drawSelected ? draw->selectBackground : draw->background);
      
			textRect.x = draw->cellRect->x + icon->width + 4;
			textRect.y = draw->cellRect->y;
			textRect.width = draw->cellRect->width - icon->width - 4;
			textRect.height = draw->cellRect->height;
		}
	else
		{
			textRect = *draw->cellRect;
		}

	if (cstr)
		if (call->clipRect->width > 4) 
			{
				/* Impose some spacing between columns.  What a hack. ### */
				call->clipRect->width -= 4;
	
				xmstr = XmStringCreate(cstr, (char*)styleToTag(m_outlinable->getColumnStyle(sourcecol)));
	
				XSetForeground(XtDisplay(m_widget), draw->gc,
							   drawSelected ? draw->selectForeground : draw->foreground);

				XmLStringDraw(m_widget, xmstr, draw->stringDirection, draw->fontList, draw->alignment,
							  draw->gc, &textRect, call->clipRect);

				call->clipRect->width += 4;
			}

	if (call->row == m_dragrow && sourcecol >= 0)
		{
			int y;
			int x2 = draw->cellRect->x + draw->cellRect->width - 1;
			XP_Bool rightedge = FALSE;
			if (call->column == m_numcolumns - 1) 
				{
					rightedge = TRUE;
#ifdef TERRYS_RIGHT_COLUMN_WIDTH_HACK
					if (xmstr) 
						{
							int xx = draw->cellRect->x + XmStringWidth(draw->fontList, xmstr) + 4;
							if (x2 > xx) x2 = xx;
						}
#endif
				}

			if (m_draggc == NULL) 
				{
					XGCValues gcv;
#if 0
					Pixel foreground;
#endif
					gcv.foreground = drawSelected ? draw->selectForeground : draw->foreground;

					m_draggc = XCreateGC(XtDisplay(m_widget), XtWindow(m_widget),
										 GCForeground, &gcv);
				}
			y = draw->cellRect->y + draw->cellRect->height - 1;
			XDrawLine(XtDisplay(m_widget), XtWindow(m_widget), m_draggc,
					  draw->cellRect->x, y, x2, y);
			if (m_dragrowbox) 
				{
					int y0 = draw->cellRect->y;
					if (call->column == 0) 
						{
							XDrawLine(XtDisplay(m_widget), XtWindow(m_widget), m_draggc,
									  draw->cellRect->x, y0, draw->cellRect->x, y);
						}
					if (rightedge) 
						{
							XDrawLine(XtDisplay(m_widget), XtWindow(m_widget), m_draggc,
									  x2, y0, x2, y);
						}	
					XDrawLine(XtDisplay(m_widget), XtWindow(m_widget), m_draggc,
							  draw->cellRect->x, y0, x2, y0);
				}
		}
  
	if (xmstr) XmStringFree(xmstr);
}

void
XFE_Outliner::celldraw(XtPointer callData)
{
	XmLGridCallbackStruct* call = (XmLGridCallbackStruct*) callData;

	if (call->rowType == XmHEADING)
		if (m_hideColumnsAllowed
			&& call->column >= m_numvisible)
			{
				if (call->column == m_numvisible)
					showHeaderDraw(call);
				else
					hideHeaderDraw(call);
			}
		else
			{
				headerCellDraw(call);
			}
	else /* XmCONTENT */
		if (! m_hideColumnsAllowed
			|| call->column < m_numvisible)
			contentCellDraw(call);
}

void
XFE_Outliner::resize(XtPointer callData)
{
	XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct*)callData;
	int i;
	int width_of_visible_columns = 0;
	int width_of_grid = XtWidth(m_widget) - 2 * MGR_ShadowThickness(m_widget);
	int delta;

	/*
	** in the event of a widget resize, we do the following:
	** o All resizable, visible columns are resized equally.
	**
	** in the event of a column resize, we do the following:
	** o resize the leftmost resizable column to the right of the column
	**   that was resized larger or smaller.
	**
	** we don't allow the right hand side of the right most visible column to
	** be dragged.
	*/

	XP_ASSERT(m_widget);
	if(!m_widget)
		return;

	for (i = 0; i < m_numvisible + (m_hideColumnsAllowed ? 2 : 0); i ++)
		{
		  /*
		  ** don't change this to calls to getColumnWidthByIndex.  It doesn't
		  ** work
		  ** correctly in the face of the show/hide buttons.
		  */
			XmLGridColumn col;
			Dimension column_width = 0;
			
			col = XmLGridGetColumn(m_widget, XmCONTENT, i);
			XtVaGetValues(m_widget,
						  XmNcolumnPtr, col,
						  XmNcolumnWidth, &column_width,
						  NULL);
			
			width_of_visible_columns += column_width;
		}

	delta = width_of_grid - width_of_visible_columns;

	if (delta == 0)
	  {
		/*
		** even if the outliner doesn't change in width, we still need to
		** remember the geometry so that the height will come back the same
		*/
		rememberGeometry();
		return;
	  }

	/*
	** The grid resize handling is simple.  We just figure out the delta and
	** apply it to all columns of the grid. We also make sure to resize the
	** columns that are not visible, so that when they are next shown they
	** don't overwhelm the outliner.
	*/
	if (cbs->reason == XmCR_RESIZE_GRID)
		{
			D(	printf ("Inside XFE_Outliner::resize(GRID)\n");) 
			int num_resizable;
			
			D( printf ("Width of visible columns is %d.  Width of outliner is %d\n",
					   width_of_visible_columns, width_of_grid); )
					
			num_resizable = numResizableVisibleColumns();
			
			XtVaSetValues(m_widget,
						  XmNlayoutFrozen, True,
						  NULL);
			
			// apply the delta to the visible columns.
			applyDelta(delta);
			
			/*
			** now we resize the hidden columns so when they are unhidden
			** their size is in line with what we've been doing to the
			** outliner -- shrinking or expanding.  You dig?
			*/
			for (i = m_numvisible; i < m_numcolumns; i ++)
				{
					if (getColumnResizableByIndex(i))
						{
							m_columnwidths[ i ] += delta / m_numcolumns;
							
							if (m_columnwidths[ i ] < MIN_COLUMN_WIDTH)
								m_columnwidths[ i ] = MIN_COLUMN_WIDTH;
							if (m_columnwidths[ i ] > MAX_COLUMN_WIDTH)
								m_columnwidths[ i ] = MAX_COLUMN_WIDTH;
						}
				}
			
			XtVaSetValues(m_widget,
						  XmNlayoutFrozen, False,
						  NULL);
		}
	/*
	** The column resize handling is a bit more involved than the grid resize
	** handling.  The grid actually resizes the column before invoking this
	** callback, so we can figure out the delta the same way we do above.  So,
	** the delta will be negative if the user dragged the column to the right,
	** and positive if they dragged to the left.
	**
	** We apply the delta to all the columns to the right of the resized one,
	** and if their is any residual, unapplied delta we add it to the width of
	** the resized column.
	*/
	else if (cbs->reason == XmCR_RESIZE_COLUMN)
		{
			D(	printf ("Inside XFE_Outliner::resize(COLUMN, %d)\n", cbs->column);) 
			Dimension resize_column_width;
			int delta_left;

			XtVaSetValues(m_widget,
						  XmNlayoutFrozen, True,
						  NULL);
			
			delta_left = applyDelta(delta, cbs->column + 1);

			D(printf ("delta_left is %d, delta was %d\n", delta_left, delta);)

  		    /*
			** our m_columnwidths array isn't updated by the grid for the
			** resized column, so we have to use GetValues to get it.  It's
			** set below (taking into account any unapplied delta.
			*/
			{
				XmLGridColumn col;
				Dimension column_width = 0;
				
				col = XmLGridGetColumn(m_widget, XmCONTENT, cbs->column);
				XtVaGetValues(m_widget,
							  XmNcolumnPtr, col,
							  XmNcolumnWidth, &column_width,
							  NULL);
				
				resize_column_width = column_width;
			}

			if (delta_left != 0)
				resize_column_width += delta_left;

			setColumnWidthByIndex(cbs->column, resize_column_width);

			XtVaSetValues(m_widget,
						  XmNlayoutFrozen, False,
						  NULL);
		}
	else
		{
			XP_ASSERT(0);
		}

	rememberGeometry();
}

void
XFE_Outliner::outlineLine(int line)
{
	int old = m_dragrow;
	if (old == line && m_dragrowbox) return;
	m_dragrowbox = TRUE;
	m_dragrow = line;
	if (old >= 0) invalidateLine(old);
	if (line >= 0 && line != old) invalidateLine(line);
}

void
XFE_Outliner::underlineLine(int line)
{
	int old = m_dragrow;
	if (old == line && !m_dragrowbox) return;
	m_dragrowbox = FALSE;
	m_dragrow = line;
	if (old >= 0) invalidateLine(old);
	if (line >= 0 && line != old) invalidateLine(line);
}

void
XFE_Outliner::sendClick(XEvent *event,
						Boolean only_if_not_selected)
{
	int x = event->xbutton.x;
	int y = event->xbutton.y;
    int button = ((XButtonEvent *)event)->button;
	int numclicks = 1;
	unsigned char rowtype;
	unsigned char coltype;
	int row;
	int column;
	unsigned int state = event->xbutton.state;
	int sourcecol;
	XRectangle rect;

	if (!only_if_not_selected &&
		abs(x - m_lastx) < 5 && abs(y - m_lasty) < 5 &&
		(m_lastdowntime - m_lastuptime <=
		 XtGetMultiClickTime(XtDisplay(m_widget)))) {
		numclicks = 2;		/* ### Allow triple clicks? */
	}
	m_lastx = x;
	m_lasty = y;

	if (XmLGridXYToRowColumn(m_widget, x, y,
							 &rowtype, &row, &coltype, &column) >= 0) 
		{
            rect.x = 0;
            rect.y = 0;
			XmLGridRowColumnToXY(m_widget, rowtype, row, coltype, column,
								 False, &rect);

			if (rowtype == XmHEADING) row = -1;

			m_activity = ButtonPressMask;
			m_ignoreevents = True;

			if (column >= m_numvisible && m_hideColumnsAllowed)
				{
					if (row != -1)
						return;

					if (column == m_numvisible)
						{
							showHeaderClick();
						}
					else
						{
							hideHeaderClick();
						}
				}
			else
				{
					sourcecol = m_columnIndex[column];

					/* make the x and y coordinates local to the cell. */
					x -= rect.x;
					y -= rect.y;

					if (sourcecol == m_pipeColumn && !only_if_not_selected && row >= 0)
						{
							int depth;
							XP_Bool expandable;
							int flippy_offset = 0;

							m_flippyfunc_data->row = row;
							m_flippyfunc_data->do_selection = FALSE;
	      
							m_DataRow = row;
							m_outlinable->acquireLineData(m_DataRow);
							m_outlinable->getTreeInfo(&expandable, NULL, &depth, NULL);
							m_outlinable->releaseLineData();

							if (m_descendantSelectAllowed)
								flippy_offset += PIPEIMAGESIZE; // XXX

							flippy_offset += depth * PIPEIMAGESIZE;

							if (expandable)
								if (x >= flippy_offset && x <= flippy_offset + PIPEIMAGESIZE)
									{
										if (numclicks == 1) 
											{
												m_outlinable->Flippyfunc(m_flippyfunc_data);
											}
										
										return;
									}
								else if (m_descendantSelectAllowed && x > 0 && x < flippy_offset)
									{
										if (numclicks == 1)
											{
												m_flippyfunc_data->do_selection = TRUE;
												m_outlinable->Flippyfunc(m_flippyfunc_data);
											}

										return;
									}
							// else we drop out and hit the buttonfunc stuff.
						}
	  
					m_buttonfunc_data->x = x;
					m_buttonfunc_data->y = y;
					m_buttonfunc_data->row = row;
					m_buttonfunc_data->column = sourcecol;
					m_buttonfunc_data->clicks = numclicks;
					m_buttonfunc_data->shift = ((state & ShiftMask) != 0);
					m_buttonfunc_data->ctrl = ((state & ControlMask) != 0);
                    m_buttonfunc_data->button = button;
					// alt?  how are we supposed to do that?

					if (m_selBlocked)
						XBell(XtDisplay(m_widget), 100);
					else
						m_outlinable->Buttonfunc(m_buttonfunc_data);

				}
		}
}

void
XFE_Outliner::buttonEvent(XEvent *event, Boolean *c)
{
	int x = event->xbutton.x;
	int y = event->xbutton.y;
	unsigned char rowtype;
	unsigned char coltype;

	/* Fix UMR && 83752
	 */
	int row = 0;
	int column = 0;

	m_ignoreevents = False;

	switch (event->type)
		{
		case ButtonPress:
			/* Always ignore btn3. Btn3 is for popups. - dp */
			if (event->xbutton.button == 3) break;

			if (XmLGridXYToRowColumn(m_widget, x, y,
									 &rowtype, &row, &coltype, &column) >= 0) 
				{
					if (rowtype == XmHEADING) 
						{

							if (XmLGridPosIsResize(m_widget, x, y)) 
								{
									break;
								}
						}
					m_activity |= ButtonPressMask;
					m_ignoreevents = True;
#if !defined(USE_MOTIF_DND)
					m_dragcolumn = column;
#endif /* USE_MOTIF_DND */
				}

#if !defined(USE_MOTIF_DND)
			m_lastmotionx = x;
			m_lastmotiony = y;

			// Save this position off so we can create the drag widget
			// in the right place if we're going to drag.  Subtract the
			// (row, col) offset first.
			{
				XRectangle hotRect;
				int sourceDepth = 0;  // Source icon indented this many times.
				XmLGridRowColumnToXY(m_widget, 
									 rowtype, row,
									 coltype, column, 
									 False, &hotRect);
	
				// Calculate the outline-indent offset.
				m_DataRow = row;
				m_outlinable->acquireLineData(row);
				m_outlinable->getTreeInfo(NULL, NULL, &sourceDepth, NULL);
				m_outlinable->releaseLineData();

				D(printf("sourceDepth = %d\n", sourceDepth);)
	  
					// Remember to add one for the flippy.
					m_hotSpot_x = x - hotRect.x - ((sourceDepth+1)*PIPEIMAGESIZE);
					m_hotSpot_y = y - hotRect.y;
			} 

#endif /* USE_MOTIF_DND */
			m_lastdowntime = event->xbutton.time;
			break;

		case ButtonRelease:
			if (m_activity & ButtonPressMask)
#if defined(USE_MOTIF_DND)
			  sendClick(event, FALSE);
#else
				{
					if (m_activity & PointerMotionMask)
						{
							/* handle the drop */
							fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_DROP);
							fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_END);
	      
							destroyDragWidget();
						}
					else
						{
							sendClick(event, FALSE);
						}
				}

#endif /* USE_MOTIF_DND */
			
			m_lastuptime = event->xbutton.time;
			m_activity = 0;

			break;

#if !defined(USE_MOTIF_DND)
#ifdef NONMOTIF_DND
		case MotionNotify:
			XmLGridXYToRowColumn(m_widget, x, y,
								 &rowtype, &row, &coltype, &column);
			
			if (!(m_activity & PointerMotionMask) &&
				(abs(x - m_lastmotionx) < 5 && abs(y - m_lastmotiony) < 5))
				{
					/* We aren't yet dragging, and the mouse hasn't moved enough for
					   this to be considered a drag. */

					break;
				}

			if (m_activity & ButtonPressMask) 
				{
					/* The type of stuff we're going to drag. */
					fe_dnd_Type drag_type;

					/* ok, the pointer moved while a button was held.
					 * we're gonna drag some stuff.
					 */
					m_ignoreevents = True;
	  
					if (!(m_activity & PointerMotionMask)) 
						{
							if (m_dragtype == FE_DND_NONE)
								{
									/* We don't do drag'n'drop in this context.  Do any any visibility
									   scrolling that we may have been putting off til the end of user
									   activity. */
									m_activity = 0;
		  
#ifdef notyet
									if (info->visibleLine >= 0 && info->visibleTimer == NULL) {
										fe_OutlineMakeVisible(info->widget, info->visibleLine);
										info->visibleLine = -1;
									}
#endif
									break;
								}
	      
							/* First time.  If the item we're pointing at isn't
							   selected, deliver a click message for it (which ought to
							   select it.) */
	      
							if (rowtype == XmCONTENT) 
								{
									/* Hack things so we click where the mouse down was, not where
									   the first notify event was.  Bleah.  But, only if the
									   item was previously unselected. */
									int actual_row = XYToRow(m_lastmotionx, m_lastmotiony);

									if (!isSelected(actual_row))
										{
											event->xbutton.x = m_lastmotionx;
											event->xbutton.y = m_lastmotiony;
											sendClick(event, TRUE);
										}
									drag_type = m_dragtype;
								}
							else // it happened in a column header.
								{
									// don't allow dragging of the show/hide buttons.
									if (m_dragcolumn >= m_numvisible)
										break;

									drag_type = FE_DND_COLUMN;
								}

	      
							// Create a drag source.
							// We want the mouse-down location, not the last one. 
							event->xbutton.x = x;
							event->xbutton.y = y;
							makeDragWidget(m_hotSpot_x, m_hotSpot_y, drag_type);
	      
							fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_START);
	      
							m_activity |= PointerMotionMask;
						}
	  
					fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_DRAG);
	  
					/* Now, force all the additional mouse motion events that are
					   lingering around on the server to come to us, so that Xt can
					   compress them away.  Yes, XSync really does improve performance
					   in this case, not hurt it. */
					XSync(XtDisplay(m_widget), False);
				}
      
			m_lastmotionx = x;
			m_lastmotiony = y;
			break;
#endif /* NONMOTIF_DND */
#endif /* !USE_MOTIF_DND */
		}
	if (m_ignoreevents) 
		*c = False;
}

#if !defined(USE_MOTIF_DND)
void
XFE_Outliner::makeDragWidget(int x, int y, fe_dnd_Type dnd_type)
{
	Visual *v = 0;
	Colormap cmap = 0;
	Cardinal depth = 0;
	Widget label;
	//  fe_icon *icon;
	Widget shell;
	Pixmap dragPixmap;

	D(printf("hot (x,y) = (%d,%d)\n", x, y);)

		if (_xfe_dragsource.widget) return;  

		shell = m_toplevel->getBaseWidget();

		XtVaGetValues (shell, XtNvisual, &v, XtNcolormap, &cmap,
					   XtNdepth, &depth, 0);

		_xfe_dragsource.type = dnd_type;

#if 0
		// This only works if we're dragging from the icon.
		// Dragging from some random part of the line gives some
		// really weird offset for the drag.  -mcafee
		_xfe_dragsource.hotx = x;
		_xfe_dragsource.hoty = y;
#else
		// Trying some fixed offset; offset of zero sucks.
	_xfe_dragsource.hotx = 8;
		_xfe_dragsource.hoty = 8;
#endif

		_xfe_dragsource.closure = dnd_type == FE_DND_COLUMN ? (void*)this : (void*)m_source;
		_xfe_dragsource.func = m_sourcedropfunc;

		XP_ASSERT(m_dragicon);
		if (m_dragicon == NULL) return;

		// XXX this needs to be something special when type == FE_DND_COLUMN
		dragPixmap = m_dragicon->pixmap;

		_xfe_dragsource.widget = XtVaCreateWidget("drag_win",
												  overrideShellWidgetClass,
												  m_widget,
												  XmNwidth, m_dragicon->width,
												  XmNheight, m_dragicon->height,
												  XmNvisual, v,
												  XmNcolormap, cmap,
												  XmNdepth, depth,
												  XmNborderWidth, 0,
												  NULL);
  
		label = XtVaCreateManagedWidget ("label",
										 xmLabelWidgetClass,
										 _xfe_dragsource.widget,
										 XmNlabelType, XmPIXMAP,
										 XmNlabelPixmap, dragPixmap,
										 NULL);
}

void
XFE_Outliner::destroyDragWidget()
{
	if (!_xfe_dragsource.widget) return;
	XtDestroyWidget (_xfe_dragsource.widget);
	_xfe_dragsource.widget = NULL;
}

void
XFE_Outliner::handleDragEvent(XEvent *event,
							  fe_dnd_Event type,
							  fe_dnd_Source *source)
{
	// only handle column drags that originated in this outliner.
	if (source->type != FE_DND_COLUMN || source->closure != this) return;

	switch (type)
		{
		case FE_DND_DROP:
			{
				unsigned char rowtype;
				unsigned char coltype;
				int row;
				int column;
				int x, y;

				D( printf ("Dropping column %d into an outliner\n", m_dragcolumn);)

					/* first we make sure that the drop happens within a valid row/column
					   pair. */
	
					translateFromRootCoords(event->xbutton.x_root, event->xbutton.y_root, &x, &y);
	
					D( printf (" Drop position is (%d,%d)\n", x, y); )

						if (XmLGridXYToRowColumn(m_widget, x, y,
												 &rowtype, &row, &coltype, &column) < 0)
							{
								D( printf ("Not dropping in a valid position\n");)
									return;
							}

						// don't allow dropping onto the show/hide columns */
						if (column >= m_numvisible)
						  return;

						/* we only need to actually do anything if they drop on a different
						   column than they started with. */
						if (column != m_dragcolumn) 
							moveColumn(m_dragcolumn, column);

						break;
			}
		}
}
#endif /* USE_MOTIF_DND */

void 
XFE_Outliner::celldrawCallback(Widget,
							   XtPointer clientData,
							   XtPointer callData)
{
	XFE_Outliner *obj = (XFE_Outliner*)clientData;

	obj->celldraw(callData);
}

void
XFE_Outliner::resizeCallback(Widget,
							 XtPointer clientData,
							 XtPointer callData)
{
	XFE_Outliner *obj = (XFE_Outliner*)clientData;

	obj->resize(callData);
}

void 
XFE_Outliner::buttonEventHandler(Widget,
								 XtPointer clientData,
								 XEvent *event,
								 Boolean *cont)
{
	XFE_Outliner *obj = (XFE_Outliner*)clientData;

	obj->buttonEvent(event, cont);
}

const char *
XFE_Outliner::styleToTag(EOutlinerTextStyle style)
{
	switch (style) 
		{
		case OUTLINER_Italic:
			return "ITALIC";
		case OUTLINER_Bold:
			return "BOLD";
		case OUTLINER_Default:
			return XmFONTLIST_DEFAULT_TAG;
		default:
			XP_ASSERT(0);
			return XmFONTLIST_DEFAULT_TAG;
		}
}

void
XFE_Outliner::change(int first, int length, int newnumrows)
{
	if (newnumrows != m_totalLines) 
		{
			int old_totalLines = m_totalLines;
      
			// update totalLines here, since adding the rows and 
			// redisplaying them might cause the outlinable to ask 
			// us how many lines we have, and we should return the 
			// new number.
			m_totalLines = newnumrows;

			if (newnumrows > old_totalLines) 
				{
					XmLGridAddRows(m_widget, XmCONTENT, -1,
								   m_totalLines - old_totalLines);
				} 
			else 
				{
					XmLGridDeleteRows(m_widget, XmCONTENT, m_totalLines,
									  old_totalLines - m_totalLines);
				}

			length = m_totalLines - first;
		}

	if (first == 0 && length == m_totalLines)
		{
			invalidate();
		} 
	else 
		{
			invalidateLines(first, length);
		}

	XFlush(XtDisplay(m_widget));
}

void 
XFE_Outliner::listChangeStarting(XP_Bool /*asynchronous*/,
								 MSG_NOTIFY_CODE notify,
								 MSG_ViewIndex /*where*/,
								 int32 /*num*/,
								 int32 /*totallines*/)
{
  /* we need to save off the selected items so they can be restored in
	 listChangeFinished. we only do this once for a given series of nested
	 changes */
  if (m_listChangeDepth == 0
	  /* don't need to save selection when items change their contents*/
	  && notify != MSG_NotifyChanged)
	{
	  saveSelection();
      
	  deselectAllItems();
	}
  
  m_listChangeDepth ++;
}

void
XFE_Outliner::listChangeFinished(XP_Bool /*asynchronous*/,
								 MSG_NOTIFY_CODE notify,
								 MSG_ViewIndex where,
								 int32 num, int32 totallines)
{
	m_listChangeDepth--;

	XP_ASSERT(m_listChangeDepth >= 0); // should _never_ be negative.

	switch (notify) {
	case MSG_NotifyChanged:
		invalidateLines(where, num);
		return;
	case MSG_NotifyNone:
#if 0
		if (MSG_GetPaneType(pane) == MSG_SEARCHPANE) /* For search dialog */ 
			if ( CONTEXT_DATA(context)->searchFinishedFunc)
				(*(CONTEXT_DATA(context)->searchFinishedFunc))(context);
		return;
#endif
		break;
	case MSG_NotifyInsertOrDelete: /*  Needed for search */
		break;
	case MSG_NotifyScramble:
	case MSG_NotifyAll:
		where = 0;
		num = totallines;
		break;
	}

	change(where, num, totallines);

	if (m_listChangeDepth == 0)
		restoreSelection();
}

int XFE_Outliner::getListChangeDepth() {
	return m_listChangeDepth;
}

Widget 
XFE_Outliner::getScroller() 
{
  Widget scroller;

  XtVaGetValues (m_widget, XmNverticalScrollBar, &scroller, 0);

  return scroller;
}


void
XFE_Outliner::saveSelection()
{
	if (!m_selectedItems)
		{
			int i;
      
			if (m_selectedCount)
				{
					m_selectedItems = (void**)XP_CALLOC(m_selectedCount,
														sizeof(void*));
	  
					for (i = 0; i < m_selectedCount; i ++)
						{
						  D( printf ("Saving selecting at index: %d\n",
									 m_selectedIndices[ i ]); )
 						  m_selectedItems[ i ] =
							m_outlinable->ConvFromIndex(m_selectedIndices[i]);
						}
					
					m_selectedItemCount = m_selectedCount;
				}
		}
}

void
XFE_Outliner::restoreSelection()
{
	if (m_selectedItems)
		{
			int i;

			for (i = 0; i < m_selectedItemCount; i ++)
				{
					int index = m_outlinable->ConvToIndex(m_selectedItems[i]);

					if (index > -1 && index < m_totalLines)
						{
							D( printf ("Restoring selecting at index: %d\n",
									   index); )

							selectItem(index);
						}
				}

			XP_FREE(m_selectedItems);
			m_selectedItemCount = 0;
			m_selectedItems = NULL;
		}
}

XFE_Outlinable *
XFE_Outliner::getOutlinable()
{
  return m_outlinable;
}
