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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<XmL/GridUtil.c>										*/
/*																		*/
/* Description:	XmLGrid misc utilities.  They are here in order not to	*/
/*	            continue bloating Grid.c beyond hope.					*/
/*																		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/* Created: 	Thu May 28 21:55:45 PDT 1998		 					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include "GridP.h"

#include <assert.h>

/*----------------------------------------------------------------------*/
/* extern */ int 
XmLGridGetRowCount(Widget w)
{
	XmLGridWidget g = (XmLGridWidget) w;

	assert( g != NULL );

#ifdef DEBUG_ramiro
	{
		int rows = 0;
		
		XtVaGetValues(w,XmNrows,&rows,NULL);

		assert( rows == g->grid.rowCount );
	}
#endif	

	return g->grid.rowCount;
}
/*----------------------------------------------------------------------*/
/* extern */ int 
XmLGridGetColumnCount(Widget w)
{
	XmLGridWidget g = (XmLGridWidget) w;

	assert( g != NULL );

#ifdef DEBUG_ramiro
	{
		int columns = 0;
		
		XtVaGetValues(w,XmNcolumns,&columns,NULL);

		assert( columns == g->grid.colCount );
	}
#endif	

	return g->grid.colCount;
}
/*----------------------------------------------------------------------*/
/* extern */ void 
XmLGridXYToCellTracking(Widget			widget, 
						int				x, /* input only args. */
						int				y, /* input only args. */
						Boolean *		m_inGrid, /* input/output args. */
						int *			m_lastRow, /* input/output args. */
						int *			m_lastCol, /* input/output args. */
						unsigned char *	m_lastRowtype, /* input/output args. */
						unsigned char *	m_lastColtype,/* input/output args. */
						int *			outRow, /* output only args. */
						int *			outCol, /* output only args. */
						Boolean *		enter, /* output only args. */
						Boolean *		leave) /* output only args. */
{
	int				m_totalLines = 0;
	int				m_numcolumns = 0;
	int				row = 0;
	int				column = 0;
	unsigned char	rowtype = XmCONTENT;
	unsigned char	coltype = XmCONTENT;
	
	if (0 > XmLGridXYToRowColumn(widget,
								 x,
								 y,
								 &rowtype,
								 &row,
								 &coltype,
								 &column)) 
	{
		/* In grid; but, not in any cells
		 */
		/* treat it as a leave
		 */
		*enter = FALSE;
		*leave = TRUE;		
		return;
	}/* if */
	
	m_totalLines = XmLGridGetRowCount(widget);
	m_numcolumns = XmLGridGetColumnCount(widget);

	if ((row < m_totalLines) &&
		(column < m_numcolumns) &&
		((*m_lastRow != row)||
		 (*m_lastCol != column) ||
		 (*m_lastRowtype != rowtype)||
		 (*m_lastColtype != coltype))) 
	{
		*outRow = (rowtype == XmHEADING)?-1:row;
		*outCol = column;
		
		if (*m_inGrid == False) 
		{
			*m_inGrid = True;
			
			/* enter a cell
			 */
			*enter = TRUE;
			*leave = FALSE;
			
		}/* if */
		else 
		{
			/* Cruising among cells
			 */
			*enter = TRUE;
			*leave = TRUE;
		}/* else */
		*m_lastRow = row;
		*m_lastCol = column;
		*m_lastRowtype = rowtype ;
		*m_lastColtype  = coltype ;
	}/* row /col in grid */				
}
/*----------------------------------------------------------------------*/
