/****************************************************************************
** $Id: qtlabelled.h,v 1.1 1998/09/25 18:01:40 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef QLABELLED_H
#define QLABELLED_H

#ifndef QT_H
#include "qframe.h"
#endif // QT_H

class QtLabelledPrivate;

class QtLabelled : public QFrame
{
    Q_OBJECT
public:
    QtLabelled( QWidget *parent=0, const char *name=0 );
    QtLabelled( const char *, QWidget *parent=0, const char *name=0 );
    ~QtLabelled();

    virtual const char* labelText() const;
    QWidget* label() const;
    virtual void setLabel( const char * );
    void setLabel( QWidget* );

    int alignment() const;
    virtual void setAlignment( int );

    bool event( QEvent * );

protected:
    virtual void childEvent( QChildEvent * );
    virtual void resizeEvent( QResizeEvent * );
    virtual int labelMargin() const;

private:
    QtLabelledPrivate* d;
    void init();
    void layout();
    void resetFrameRect();

private:	// Disabled copy constructor and operator=
    QtLabelled( const QtLabelled & );
    QtLabelled &operator=( const QtLabelled & );
};


#endif // QLABELLED_H
