/****************************************************************************
** $Id: qtvbox.h,v 1.1 1998/09/25 18:01:40 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef QVBOX_H
#define QVBOX_H

#ifndef QT_H
#include "qthbox.h"
#endif // QT_H

class QtVBox : public QtHBox
{
    Q_OBJECT
public:
    QtVBox( QWidget *parent=0, const char *name=0, WFlags f=0 );
};

#endif //QVBOX_H
