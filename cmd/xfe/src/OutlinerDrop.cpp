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
   OutlinerDrop.cpp -- Drag and drop for our outliners.
   Created: Chris Toshok <toshok@netscape.com>, 10-Sep-97
   */



#include "structs.h"
#include "ntypes.h"
#include "xfe.h"
#include "OutlinerDrop.h"
#include "Outliner.h"
#include "Outlinable.h"

#if defined(DEBUG_toshok) || defined(DEBUG_Tao)
#define D(x) printf x
#else
#define D(x)
#endif

Atom XFE_OutlinerDrop::_XA_NETSCAPE_OUTLINER_COLUMN=None;

Atom XFE_OutlinerDrop::_XA_NETSCAPE_BOOKMARK=None;

Atom XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_SERVER=None;
Atom XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_SERVER=None;
Atom XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_FOLDER=None;
Atom XFE_OutlinerDrop::_XA_NETSCAPE_NEWSGROUP=None;

Atom XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_MESSAGE=None;
Atom XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_MESSAGE=None;

Atom XFE_OutlinerDrop::_XA_NETSCAPE_PAB=None;
Atom XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV=None;

static void
initTargets(Widget widget)
{
  static int init_done = 0;

  D(("initTargets()\n"));

  if (init_done)
	return;

  init_done = 1;

  XFE_OutlinerDrop::_XA_NETSCAPE_OUTLINER_COLUMN =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_OUTLINER_COLUMN", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_BOOKMARK =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_BOOKMARK", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_SERVER =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_MAIL_SERVER", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_SERVER =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_NEWS_SERVER", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_FOLDER =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_MAIL_FOLDER", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_NEWSGROUP =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_NEWSGROUP", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_MESSAGE =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_MAIL_MESSAGE", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_MESSAGE =
	XmInternAtom(XtDisplay(widget), "_NETSCAPE_NEWS_MESSAGE", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_PAB =
	XmInternAtom(XtDisplay(widget), "_XA_NETSCAPE_PAB", FALSE);
  XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV =
	XmInternAtom(XtDisplay(widget), "_XA_NETSCAPE_DIRSERV", FALSE);
}

XFE_OutlinerDrop::XFE_OutlinerDrop(Widget parent, XFE_Outliner *outliner)
  : XFE_DropNetscape(parent)
{
  m_outliner = outliner;

  D(("XFE_OutlinerDrop::XFE_OutlinerDrop()\n"));

  initTargets(parent);
}

XFE_OutlinerDrop::~XFE_OutlinerDrop()
{
  D(("XFE_OutlinerDrop::~XFE_OutlinerDrop()\n"));
}

void
XFE_OutlinerDrop::targets()
{
  Atom *new_targets;
  int new_numTargets;
  int i;

  D(("XFE_OutlinerDrop::targets()\n"));

  m_outliner->getDropTargets(&_targets, &_numTargets);

  new_numTargets = _numTargets + 1;
  new_targets = new Atom[new_numTargets];

  for (i = 0; i < _numTargets; i ++)
	{
	  new_targets[i] = _targets[i];
	}

  new_targets[new_numTargets - 1] = _XA_NETSCAPE_OUTLINER_COLUMN;

  for (i = 0; i < new_numTargets; i ++)
	{
	  D(("  [%d] %s\n",i,XmGetAtomName(XtDisplay(_widget),new_targets[i])));
	}

  if (_targets)
	delete [] _targets;

  _numTargets = new_numTargets;
  _targets = new_targets;
}

void
XFE_OutlinerDrop::operations()
{
  _operations = (unsigned int)XmDROP_COPY;
}

int
XFE_OutlinerDrop::processTargets(Atom *targets,
								 const char **data,
								 int numItems)
{
  int i;
  int drop_column, drop_row;
  unsigned char rowtype, coltype;

  D(("XFE_OutlinerDrop::processTargets()\n"));

  _sameDragSource = TRUE; /* XXX */

  for (i = 0; i<numItems; i ++)
	{
	  if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
		continue;

	  D(("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(_widget),targets[i]),data[i]));
	  
	  if (XmLGridXYToRowColumn(m_outliner->getBaseWidget(),
							   _dropEventX, _dropEventY,
							   &rowtype, &drop_row,
							   &coltype, &drop_column) < 0)
		{
		  D(("Not dropping column in a valid position\n"));
		  return FALSE;
		}
	  
	  if (targets[i] == _XA_NETSCAPE_OUTLINER_COLUMN
		  && _sameDragSource)
		{
		  int column = atoi(data[i]);
		  
		  if (column != -1)
			{
			  if (drop_column != column)
				m_outliner->moveColumn(column, drop_column);
			  
			  return TRUE;
			}
		}
	}

  return m_outliner->processTargets(drop_row, drop_column,
									targets, data, numItems);
}

void
XFE_OutlinerDrop::dragIn()
{
  /* not much to do... */
  D(("XFE_OutlinerDrop::dragIn()\n"));
}

void
XFE_OutlinerDrop::dragOut()
{
  D(("XFE_OutlinerDrop::dragOut()\n"));
  m_outliner->outlineLine(-1);
}

void
XFE_OutlinerDrop::dragMotion()
{
  int row = m_outliner->XYToRow(_dropEventX, _dropEventY);

  if (row != -1) m_outliner->outlineLine(row);
}


/* XFE_OutlinerDrag */

XFE_OutlinerDrag::XFE_OutlinerDrag(Widget parent,
								   XFE_Outliner *outliner)
  : XFE_DragNetscape(parent)
{
  m_outliner = outliner;

  initTargets(parent);
}

XFE_OutlinerDrag::~XFE_OutlinerDrag()
{
}

void
XFE_OutlinerDrag::buttonPressCb(XButtonPressedEvent *event)
{
  /* we pass through the button press event if it's not on one of the
	 grid's resize controls. */
  if (!XmLGridPosIsResize(m_outliner->getBaseWidget(),
						  event->x,
						  event->y))
	XFE_DragNetscape::buttonPressCb(event);
}

fe_icon_data *
XFE_OutlinerDrag::createFakeHeaderIconData(int col)
{
  /* XXX ?? for now rely on the outlinable to give us the right icon.
	 In the future we should try and do what windows does -- drag a 
	 representation of the column header (or perhaps the entire column. */
  return m_outliner->getDragIconData(-1, col);
}

/* we return FALSE if we're dragging from a row/column that doesn't exist
   int the outliner (if we drag from a blank spot below any real rows.)
   This short circuits the drag. */
int
XFE_OutlinerDrag::dragStart(int x, int y)
{
  int col, row;
  unsigned char rowtype, coltype;
  
  if (XmLGridXYToRowColumn(m_outliner->getBaseWidget(),
						   x, y,
						   &rowtype, &row,
						   &coltype, &col) < 0)
	return FALSE;
  else
	return TRUE;
}

/* XFE_OutlinerDrag::dragStart is called before this method.  So, checking the
   return value of XmLGridXYToRowColumn is unnecessary -- a -1 would have short
   circuited the entire process before we made it to this method. 
*/
void
XFE_OutlinerDrag::targets()
{
  int row, col;
  unsigned char rowtype, coltype;

  D(("XFE_OutlinerDrag::targets()\n"));

  XmLGridXYToRowColumn(m_outliner->getBaseWidget(),
					   _dragStartX, _dragStartY,
					   &rowtype, &row,
					   &coltype, &col);

  m_dragcolumn = col;
  m_dragrow = row;

  /*
  ** if the user drags one of the headers, we never let the outlinable know about
  ** it -- we handle the entire process in this file.  The atom type is 
  ** _XA_NETSCAPE_OUTLINER_COLUMN and the data is the index of the column.
  ** the column's index is available in m_dragcolumn, and the row in m_dragrow.
  */
  if (rowtype == XmHEADING)
	{
	  if (_targets)
		delete [] _targets;

	  row = -1;
	  setDragIcon(createFakeHeaderIconData(col));

	  _numTargets = 2;
	  
	  _targets = new Atom[_numTargets];
	  
	  _targets[0] = XFE_OutlinerDrop::_XA_NETSCAPE_OUTLINER_COLUMN;
	}
  else
	{
	  setDragIcon(m_outliner->getDragIconData(row, col));
  
	  m_outliner->getDragTargets(row, col,
								 &_targets, &_numTargets);
	}
}

void
XFE_OutlinerDrag::operations()
{
  _operations = (unsigned int)XmDROP_COPY | XmDROP_MOVE;
}

char *
XFE_OutlinerDrag::getTargetData(Atom atom)
{
  /* if the atom's type is _XA_NETSCAPE_OUTLINER_COLUMN, we handle
	 it here.  Otherwise the outliner (or outlinable) handles it. */
  if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_OUTLINER_COLUMN
	  && m_dragcolumn != -1)
	{
	  char buf[10];
	  PR_snprintf(buf, 10, "%d", m_dragcolumn);
	  return (char*)XtNewString(buf);
	}
  else
	{
		/* TJC: added "return"; was missing??
		 */
		return m_outliner->dragConvert(atom);
	}
}

void
XFE_OutlinerDrag::deleteTarget()
{
}

void
XFE_OutlinerDrag::dragComplete()
{
}
