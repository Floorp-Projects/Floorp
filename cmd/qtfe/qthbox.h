/****************************************************************************
** $Id: qthbox.h,v 1.1 1998/09/25 18:01:39 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef QHBOX_H
#define QHBOX_H

#ifndef QT_H
#include "qwidget.h"
#endif // QT_H

class QBoxLayout;

class QtHBox : public QWidget
{
    Q_OBJECT
public:
    QtHBox( QWidget *parent=0, const char *name=0, WFlags f=0 );
    bool event( QEvent * );
    void addStretch();
    void pack();

protected:
    QtHBox( bool horizontal, QWidget *parent=0, const char *name=0, WFlags f=0 );
    virtual void childEvent( QChildEvent * );
private:
    void syncLayout();
    void doLayout();
    QBoxLayout *lay;
    bool packed;
};

#endif //QHBOX_H
