/* $Id: OpenDialog.cpp,v 1.2 1998/09/25 23:30:38 cls%seawood.org Exp $
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
 * The Initial Developer of this code under the NPL is Kalle
 * Dalheimer.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#include "OpenDialog.h"
#include "QtBrowserContext.h"

#include <qfiledlg.h>
#include <qpushbt.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlined.h>
#include <qframe.h>
#include <qtbuttonrow.h>

#define i18n( x ) x

OpenDialog::OpenDialog( QtBrowserContext* _context, QWidget* parent,
			const char* name ) :
    QDialog( parent, name, true ),
    context( _context )
{
    QVBoxLayout* vlayout = new QVBoxLayout( this, 6 );

    QLabel* label = new QLabel( i18n( "Enter the World Wide Web location (URL) or specify\n"
				      "the local file you would like to open:" ), this );
    label->setFixedSize( label->sizeHint() );
    vlayout->addWidget( label );

    QHBoxLayout* hlayout = new QHBoxLayout();
    vlayout->addLayout( hlayout );

    edit = new QLineEdit( this );
    edit->setFixedHeight( edit->sizeHint().height() );
    edit->setFocus();
    hlayout->addWidget( edit );

    QPushButton* choosePB = new QPushButton( i18n( "Choose &File..." ), this );
    choosePB->setFixedSize( choosePB->sizeHint() );
    connect( choosePB, SIGNAL( clicked() ),
	     this, SLOT( chooseFile() ) );
    hlayout->addWidget( choosePB );

    QFrame* line = new QFrame( this, "line", 0, true );
    line->setFrameStyle( QFrame::HLine | QFrame::Sunken );
    line->setFixedHeight( 12 ); // Arnt said so...
    vlayout->addWidget( line );

    QtButtonRow* buttonrow = new QtButtonRow( this );

#ifdef MOZ_MAIL_NEWS
    QPushButton* openNavigatorPB = new QPushButton( i18n( "Open In &Navigator" ),
						    buttonrow );
#else
    QPushButton* openNavigatorPB = new QPushButton( i18n( "&OK" ),
						    buttonrow );
#endif
    openNavigatorPB->setFixedSize( openNavigatorPB->sizeHint() );
    connect( openNavigatorPB, SIGNAL( clicked() ),
	     this, SLOT( openInNavigator() ) );
    connect( openNavigatorPB, SIGNAL( clicked() ),
	     this, SLOT( accept() ) );
#ifdef EDITOR
    QPushButton* openComposerPB = new QPushButton( i18n( "Open In &Composer" ),
						    buttonrow );
    openComposerPB->setFixedSize( openComposerPB->sizeHint() );
    connect( openComposerPB, SIGNAL( clicked() ),
	     this, SLOT( openInComposer() ) );
    connect( openComposerPB, SIGNAL( clicked() ),
	     this, SLOT( accept() ) );
#endif

    QPushButton* clearPB = new QPushButton( i18n( "&Clear" ),
					    buttonrow );
    clearPB->setFixedSize( clearPB->sizeHint() );
    connect( clearPB, SIGNAL( clicked() ),
	     this, SLOT( clear() ) );

    QPushButton* cancelPB = new QPushButton( i18n( "Close" ),
					     buttonrow );
    cancelPB->setFixedSize( cancelPB->sizeHint() );
    connect( cancelPB, SIGNAL( clicked() ),
	     this, SLOT( reject() ) );

#ifdef EDITOR
    if( context->mwContext()->type == MWContextEditor)
	openComposerPB->setDefault( true );
    else
#endif
	openNavigatorPB->setDefault( true );

    vlayout->addWidget( buttonrow );
    vlayout->activate();
    resize( 1, 1 );
}


void OpenDialog::chooseFile()
{
    //#warning It would be nicer to have a correct caption. Kalle
    QString file = QFileDialog::getOpenFileName();
    if( !file.isEmpty() )
	edit->setText( file );
}

void OpenDialog::openInComposer()
{
#ifdef EDITOR
    if( !strlen( edit->text() ) )
	return;

    context->editorEdit( 0, edit->text() );
#endif
}

void OpenDialog::openInNavigator()
{
    if( !strlen( edit->text() ) )
	return;

    context->browserGetURL( edit->text() );
}

void OpenDialog::clear()
{
    edit->setText( "" );
}
