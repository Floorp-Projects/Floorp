/****************************************************************************
** $Id: qthbox.cpp,v 1.1 1998/09/25 18:01:39 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include "qthbox.h"
#include "qlayout.h"
#include "qapp.h"

/*!
  \class QtHBox qhbox.h
  \brief The QtHBox widget performs geometry management on its children

  \ingroup geomanagement

  All its children will be placed beside each other and sized
  according to their sizeHint()s.

  \sa QtVBox and QtGrid

*/


/*!
  Constructs an hbox widget with parent \a parent and name \a name
 */
QtHBox::QtHBox( QWidget *parent, const char *name, WFlags f )
    :QWidget( parent, name, f )
{
    packed = FALSE;
    lay = new QHBoxLayout( this, parent?0:5, 5, name ); //### border
}


/*!
  Constructs a horizontal hbox if \a horizontal is TRUE, otherwise
  constructs a vertical hbox (also known as a vbox).

  This constructor is provided for the QtVBox class. You should never need
  to use it directly.
*/

 QtHBox::QtHBox( bool horizontal, QWidget *parent , const char *name, WFlags f )
    :QWidget( parent, name, f )
{
    packed = FALSE;
    lay = new QBoxLayout( this,
		       horizontal ? QBoxLayout::LeftToRight : QBoxLayout::Down,
			  parent?0:5, 5, name ); //### border
}



/*!
  This function is called when the widget gets a new child or loses an old one.
 */
void QtHBox::childEvent( QChildEvent *c )
{
    if ( !c->inserted() )
	return;
    QWidget *w = c->child();
    QSize sh = w->sizeHint();
    if ( !sh.isEmpty() )
	w->setMinimumSize( sh );
    lay->addWidget( w );
    if ( isVisible() )
	lay->activate();
}


/*!
  Provides childEvent() while waiting for Qt 2.0.
 */
bool QtHBox::event( QEvent *e ) {
    switch ( e->type() ) {
    case Event_ChildInserted:
    case Event_ChildRemoved:
	childEvent( (QChildEvent*)e );
	return TRUE;
    default:
	break;
    }
    bool result = QWidget::event( e );

    //### we have to do this after QGManager's event filter has done its job
    switch ( e->type() ) {
    case Event_ChildInserted:
    case Event_ChildRemoved:
    case Event_LayoutHint:
	doLayout();
    default:
	break;
    }
    return result;
}



/*!
  Adds infinitely stretchable space.
*/

void QtHBox::addStretch()
{
    syncLayout();
    lay->addStretch( 1 );
    if ( isVisible() )
	lay->activate();

}


/*!
  Get all ChildInserted events that are queued up for me
*/

void QtHBox::syncLayout()
{
    QApplication::sendPostedEvents( this, Event_ChildInserted );
}


/*!
  Resizes this widget to its minimum size and does not allow the user to
  resize it.
*/

void QtHBox::pack()
{
  syncLayout();
  lay->activate();
  setFixedSize( minimumSize() );
  packed = TRUE;
}


/*!
  Handles frozen layouts. To be called after layout is just activated.
*/

void QtHBox::doLayout()
{
    if ( packed )
	  setFixedSize( minimumSize() );
}
