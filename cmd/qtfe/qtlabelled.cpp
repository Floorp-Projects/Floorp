/****************************************************************************
** $Id: qtlabelled.cpp,v 1.1 1998/09/25 18:01:40 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include "qtlabelled.h"
#include "qlayout.h"
#include "qlabel.h"

class QtLabelledPrivate {
public:
    QtLabelledPrivate(QtLabelled* parent)
    {
	QLabel *l = new QLabel(parent, "label");
	l->setMargin(2);

	label = l;
	child = 0;
	align = AlignTop;
	grid = 0;
    }

    QWidget* label;
    QWidget* child;
    int align;
    QGridLayout* grid;
};

QtLabelled::QtLabelled( QWidget *parent, const char *name ) :
    QFrame(parent, name)
{
    init();
}

QtLabelled::QtLabelled( const char *label, QWidget *parent, const char *name ) :
    QFrame(parent, name)
{
    init();
    setLabel(label);
}

void QtLabelled::init()
{
    setFrameStyle( QFrame::Box | QFrame::Sunken );
    setMargin(9);
    d = new QtLabelledPrivate(this);
    layout();
    resetFrameRect();
}

QtLabelled::~QtLabelled()
{
    delete d;
}

const char* QtLabelled::labelText() const
{
    if (d->label->inherits("QLabel"))
	return ((QLabel*)d->label)->text();
    else
	return 0;
}

QWidget* QtLabelled::label() const
{
    return d->label;
}

void QtLabelled::setLabel( const char *text )
{
    if (d->label->inherits("QLabel"))
	((QLabel*)d->label)->setText(text);
    resetFrameRect();
}

void QtLabelled::setLabel( QWidget* label )
{
    bool v = d->label->isVisible();
    delete d->label;
    d->label = label;
    recreate( this, 0, QPoint(0,0), v );
    layout();
}

int QtLabelled::alignment() const
{
    return d->align;
}

void QtLabelled::setAlignment( int align )
{
    if ( d->align != align ) {
	ASSERT( align == AlignTop || align == AlignLeft );
	d->align = align;
	layout();
	resetFrameRect();
	update();
    }
}

int QtLabelled::labelMargin() const
{
    if (d->label->inherits("QLabel"))
	return ((QLabel*)d->label)->margin();
    else
	return 0;
}

void QtLabelled::layout()
{
    delete d->grid;

    QSize sh = d->label->sizeHint();
    if (sh.isValid() && !sh.isEmpty())
	d->label->setFixedSize(sh);

    if ( d->align == AlignTop ) {
	d->grid = new QGridLayout(this,4,4);
	d->grid->addMultiCellWidget( d->label, 0, 0, 1, 2, AlignLeft );
	if (d->child) {
	    d->grid->addWidget( d->child, 2, 2, AlignLeft );
	    d->grid->setRowStretch( 2, 1 );
	    d->grid->setColStretch( 2, 1 );
	}
	d->grid->addRowSpacing( 1, margin()/2 );
	d->grid->addColSpacing( 0, frameWidth()-labelMargin() );
	d->grid->addColSpacing( 1, labelMargin() );
	d->grid->addRowSpacing( 3, frameWidth() );
	d->grid->addColSpacing( 3, frameWidth() );
    } else {
	d->grid = new QGridLayout(this,4,4);
	d->grid->addMultiCellWidget( d->label, 1, 2, 0, 0, AlignTop );
	if (d->child) {
	    d->grid->addWidget( d->child, 2, 2, AlignTop );
	    d->grid->setRowStretch( 2, 1 );
	    d->grid->setColStretch( 2, 1 );
	}
	d->grid->addColSpacing( 1, margin()/2 );
	d->grid->addRowSpacing( 0, frameWidth()-labelMargin() );
	d->grid->addRowSpacing( 1, labelMargin() );
	d->grid->addRowSpacing( 3, frameWidth() );
	d->grid->addColSpacing( 3, frameWidth() );
    }

    resetFrameRect();

    d->grid->activate();
}

void QtLabelled::resetFrameRect()
{
    QRect f = rect();

    if ( d->align == AlignTop ) {
	f.setTop( d->label->height()/2 );
	setFrameRect(f);
    } else {
	f.setLeft( d->label->width()/2 );
	setFrameRect(f);
    }
}

void QtLabelled::resizeEvent( QResizeEvent * )
{
    resetFrameRect();
}

void QtLabelled::childEvent( QChildEvent *e )
{
    if (d->label == e->child())
	return; // Yes, we know all about it.

    if (d->child && e->inserted())
	warning("QtLabelled should only have one child inserted.");

    if (e->inserted())
	d->child = e->child();
    else
	d->child = 0;

    layout();
}

/*!
  Provides childEvent() while waiting for Qt 2.0.
 */
bool QtLabelled::event( QEvent *e ) {
    switch ( e->type() ) {
    case Event_ChildInserted:
    case Event_ChildRemoved:
        childEvent( (QChildEvent*)e );
        return TRUE;
    default:
        return QWidget::event( e );
    }
}
