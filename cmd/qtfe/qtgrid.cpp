/****************************************************************************
** $Id: qtgrid.cpp,v 1.1 1998/09/25 18:01:39 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include "qtgrid.h"
#include "qlayout.h"

/*!
  \class QtGrid qgrid.h
  \brief The QtGrid widget performs geometry management on its children

  \ingroup geomanagement

  The number of rows or columns is defined in the constructor. All its
  children will be placed and sized according to their sizeHint()s.

  \sa QtVBox and QtHBox
 */



/*!
  Constructs a grid widget with parent \a parent and name \a name. If \a d is
  \c Horizontal, \a n specifies the number of columns. If \a d is \c Vertical,
  \a n specifies the number of rows.
 */
QtGrid::QtGrid( int n, Direction d, QWidget *parent, const char *name, WFlags f )
    :QWidget( parent, name, f )
{
    if ( d == Horizontal ) {
	nCols = n;
	nRows = 1;
    } else {
	nCols = 1;
	nRows = n;
    }
    dir = d;
    lay = new QGridLayout( this, nRows, nCols, parent?0:5, 5, name ); //### border
    row = col = 0;
}



/*!
  Constructs a grid widget with parent \a parent and name \a name.
  \a n specifies the number of columns.
 */
QtGrid::QtGrid( int n, QWidget *parent, const char *name, WFlags f )
    :QWidget( parent, name, f )
{
    nCols = n;
    nRows = 1;
    dir = Horizontal;
    lay = new QGridLayout( this, nRows, nCols, parent?0:5, 5, name ); //### border
    row = col = 0;
}


/*!
  This function is called when the widget gets a new child or loses an old one.
 */
void QtGrid::childEvent( QChildEvent *c )
{
    if ( !c->inserted() )
	return;
    QWidget *w = c->child();
    QSize sh = w->sizeHint();
    if ( !sh.isEmpty() )
	w->setMinimumSize( sh );

    if ( row >= nRows || col >= nCols )
	lay->expand( row+1, col+1 );
    lay->addWidget( w, row, col );
    //    debug( "adding %d,%d", row, col );
    skip();
    if ( isVisible() )
	lay->activate();
}


/*!
  Provides childEvent() while waiting for Qt 2.0.
 */
bool QtGrid::event( QEvent *e ) {
    switch ( e->type() ) {
    case Event_ChildInserted:
    case Event_ChildRemoved:
	childEvent( (QChildEvent*)e );
	return TRUE;
    default:
	return QWidget::event( e );
    }
}



/*!
  Skips a position in the grid, leaving it empty.
*/

void QtGrid::skip()
{
    if ( dir == Horizontal ) {
	if ( col+1 < nCols ) {
	    col++;
	} else {
	    col = 0;
	    row++;
	}
    } else { //Vertical
	if ( row+1 < nRows ) {
	    row++;
	} else {
	    row = 0;
	    col++;
	}
    }
}
