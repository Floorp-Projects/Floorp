/****************************************************************************
** $Id: qtbuttonrow.h,v 1.1 1998/09/25 18:01:38 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef QBUTTONROW_H
#define QBUTTONROW_H

#ifndef QT_H
#include "qwidget.h"
#endif // QT_H

class QGManager;
class QChain;

class QtButtonRow : public QWidget
{
    Q_OBJECT
public:
    QtButtonRow( QWidget *parent=0, const char *name=0 );
    bool event( QEvent * );
    void show();
protected:
    virtual void childEvent( QChildEvent * );
    virtual void layoutEvent( QEvent * );
private:
    void recalc();
    QGManager *gm;
    QChain *par;
    QChain *ser;
    QSize prefSize;
    bool first;
};
#endif //QBUTTONROW_H
