/****************************************************************************
** $Id: qtvbox.cpp,v 1.1 1998/09/25 18:01:40 ramiro%netscape.com Exp $
**
** Copyright (C) 1992-1998 Troll Tech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include "qtvbox.h"
#include "qlayout.h"

/*!
  \class QtVBox qvbox.h
  \brief The QtVBox widget performs geometry management on its children

  \ingroup geomanagement

  All its children will be placed vertically and sized
  according to their sizeHint()s.

  \sa QtVBox and QtHBox */


/*!
  Constructs a vbox widget with parent \a parent and name \a name
 */
QtVBox::QtVBox( QWidget *parent, const char *name, WFlags f )
    :QtHBox( FALSE, parent, name, f )
{
}
