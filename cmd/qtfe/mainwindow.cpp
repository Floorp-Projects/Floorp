/* $Id: mainwindow.cpp,v 1.1 1998/09/25 18:01:35 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include "mainwindow.h"

qtfeMainWindow::qtfeMainWindow()
    : QMainWindow( 0, "QtMozilla main window" )
{
  /*
    QPixmap icon;

QToolBar * fileTools = new QToolBar( this, "file operations" );
    addToolBar( fileTools, "Standard" );
    QToolButton * tmp;

icon = QPixmap( fileopen );
    tmp = new QToolButton( icon, "Open File", 0,
			   this, SLOT(load()),
			   fileTools, "open file" );

icon = QPixmap( filesave );
    tmp = new QToolButton( icon, "Save File", 0,
			   this, SLOT(save()),
			   fileTools, "save file" );

icon = QPixmap( fileprint );
    tmp = new QToolButton( icon, "Print File", 0,
			   this, SLOT(print()),
			   fileTools, "print file" );

QPopupMenu * file = new QPopupMenu();
    menuBar()->insertItem( "&File", file );

file->insertItem( "New", this, SLOT(newDoc()), CTRL+Key_N );
    file->insertItem( "Open", this, SLOT(load()), CTRL+Key_O );
    file->insertItem( "Save", this, SLOT(save()), CTRL+Key_S );
    file->insertSeparator();
    file->insertItem( "Print", this, SLOT(print()), CTRL+Key_P );
    file->insertSeparator();
    file->insertItem( "Close", this, SLOT(closeDoc()), CTRL+Key_W );
    file->insertItem( "Quit", qApp, SLOT(quit()), CTRL+Key_Q );

e = new QMultiLineEdit( this, "editor" );
    e->setFocus();
    setCentralWidget( e );
    statusBar()->message( "Ready", 2000 );
  */
}
