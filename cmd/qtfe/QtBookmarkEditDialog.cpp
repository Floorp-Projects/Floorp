/* $Id: QtBookmarkEditDialog.cpp,v 1.1 1998/09/25 18:01:27 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Eirik Eng.
 * Further developed by Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik
 * Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include "QtBookmarkEditDialog.h"

#include <qtbuttonrow.h>
#include <qchkbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbt.h>
#include <qlined.h>
#include <qmlined.h>

#include <qthbox.h>
#include <qtgrid.h>
#include "QtBookmarksContext.h"

#include "bkmks.h"

#define i18n(x) x

QtBookmarkEditDialog::QtBookmarkEditDialog( QtBookmarksContext* p,
					const char* wName ) :
    QtVBox( 0, wName )
{
    parent = p;
    entry  = 0;

    QtGrid *g = new QtGrid( 2, QtGrid::Horizontal, this );

    QLabel* nameLabel = new QLabel( i18n("Name:"), g );
    nameLabel->setAlignment( AlignRight );
    name = new QLineEdit( g );
    name->setFixedHeight( nameLabel->sizeHint().height() );

    QLabel* locationLabel = new QLabel( i18n("Location:"), g );
    locationLabel->setAlignment( AlignRight );
    location = new QLineEdit( g );
    location->setFixedHeight( locationLabel->sizeHint().height() );

    QLabel* descriptionLabel = new QLabel( i18n("Description:"), g );
    descriptionLabel->setAlignment( AlignRight | AlignVCenter );
    description = new QMultiLineEdit( g );
    description->setFixedVisibleLines( 5 );

    QtButtonRow* buttonRow = new QtButtonRow( this );

    QPushButton* okPB = new QPushButton( i18n("OK"), buttonRow );
    connect( okPB, SIGNAL( clicked() ), this, SLOT( okSlot() ) );
    QPushButton* cancelPB = new QPushButton( i18n( "Cancel" ), buttonRow );
    connect( cancelPB, SIGNAL( clicked() ), this, SLOT( cancelSlot() ) );

}


void QtBookmarkEditDialog::setEntry( BM_Entry *e )
{
    save();
    entry = e;
    name       ->setText( BM_GetName(entry) );
    location   ->setText( BM_GetAddress(entry) );
    description->setText( BM_GetDescription(entry) );
}

void QtBookmarkEditDialog::clear()
{
    name->setText( "" );
    description->setText( "" );
    location->setText( "" );
    entry = 0;
}

void QtBookmarkEditDialog::save()
{
    if ( !entry )
	return;
    BM_SetName( parent->mwContext(), entry, name->text() );
    BM_SetAddress( parent->mwContext(), entry, location->text() );
    BM_SetDescription( parent->mwContext(), entry, description->text() );
    BM_SaveBookmarks( QtBookmarksContext::ptr()->mwContext(), 0 );
}

void QtBookmarkEditDialog::okSlot()
{
    save();
    emit okClicked();
    hide();
}

void QtBookmarkEditDialog::cancelSlot()
{
    clear();
    emit cancelClicked();
    hide();
}
