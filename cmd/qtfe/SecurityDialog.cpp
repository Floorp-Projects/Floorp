/* $Id: SecurityDialog.cpp,v 1.1 1998/09/25 18:01:30 ramiro%netscape.com Exp $
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

#include "SecurityDialog.h"

#include <qtbuttonrow.h>
#include <qchkbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbt.h>

#include <fe_proto.h>

#define i18n(x) x

SecurityDialog::SecurityDialog( int state, char* prefs_toggle,
				QWidget* parent, const char* name ) :
    QDialog( parent, name, true )
{
    char* prefs = prefs_toggle;

    QVBoxLayout* vlayout = new QVBoxLayout( this );

    QLabel* label = new QLabel( this );
    Bool cancel_p = true;
    Bool cancel_default_p = false;

    vlayout->addWidget( label, 2 );

    if( prefs_toggle ) {
	QCheckBox* prefsCB = new QCheckBox( i18n( "Show this Alert Next Time" ),
					    this );
	prefsCB->adjustSize();
	vlayout->addWidget( prefsCB );
	connect( prefsCB, SIGNAL( toggled( bool ) ),
		 this, SLOT( prefsCBToggled( bool ) ) );
    }

    QtButtonRow* buttonRow = new QtButtonRow( this );
    vlayout->addWidget( buttonRow );

    QPushButton* continuePB = new QPushButton( buttonRow );
    connect( continuePB, SIGNAL( clicked() ),
	     this, SLOT( accept() ) );
    QPushButton* cancelPB = new QPushButton( buttonRow );
    connect( cancelPB, SIGNAL( clicked() ),
	     this, SLOT( reject() ) );

    switch (state)
	{
	case SD_INSECURE_POST_FROM_SECURE_DOC:
	    label->setText( i18n( "Warning! Although this document is secure, any information you\n"
			    "submit is insecure and could be observed by a third party while\n"
			    "in transit. If you are submitting passwords, credit card numbers,\n"
			    "or other information you would like to keep private, it would be\n"
			    "safer for you to cancel the submission." ) );
	    continuePB->setText( i18n( "Confirm Submission" ) );
	    cancelPB->setText( i18n( "Cancel Submission" ) );
	    cancel_p = true;
	    cancel_default_p = true;
	    break;
	case SD_INSECURE_POST_FROM_INSECURE_DOC:
	    label->setText( i18n( "The information you submit is insecure and could be observed by\n"
			    "a third party while in transit.  If you are submitting passwords,\n"
			    "credit card numbers, or other information you would like to keep\n"
			    "private, it would be safer for you to cancel the submission." ) );
	    continuePB->setText( i18n( "Confirm Submission" ) );
	    cancelPB->setText( i18n( "Cancel Submission" ) );
	    cancel_p = true;
	    break;
	case SD_ENTERING_SECURE_SPACE:
	    label->setText( i18n( "You have requested a secure document. The document and any information\n"
				  "you send back are encrypted for privacy while in transit.\n"
				  "For more information on security choose Page Info from the View menu." ) );
	    continuePB->setText( i18n( "Confirm Loading" ) );
	    cancelPB->setText( i18n( "Cancel Loading" ) );
	    cancel_p = false;
	    break;
	case SD_LEAVING_SECURE_SPACE:
	    label->setText( i18n( "You have requested an insecure document. The document and any information\n"
				  "you send back could be observed by a third party while in transit.\n"
				  "For more information on security choose Page Info from the View menu." ) );

	    continuePB->setText( i18n( "Confirm Loading" ) );
	    cancelPB->setText( i18n( "Cancel Loading" ) );
	    cancel_p = true;
	    break;
	case SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN:
	    label->setText( i18n( "You have requested a secure document that contains some insecure "
				  "information.\n\n"
				  "The insecure information will not be shown.\n"
				  "For more information on security choose Page Info from the View menu." ) );
	    continuePB->setText( i18n( "OK" ) );
	    cancel_p = false;
	    break;
	case SD_REDIRECTION_TO_INSECURE_DOC:
	    label->setText( i18n( "Warning! You have requested an insecure document that was\n"
				  "originally designated a secure document (the location has been\n"
				  "redirected from a secure to an insecure document). The document\n"
				  "and any information you send back could be observed by a third\n"
				  "party while in transit." ) );
	    continuePB->setText( i18n( "Confirm Loading" ) );
	    cancelPB->setText( i18n( "Cancel Loading" ) );
	    cancel_p = true;
	    break;
	case SD_REDIRECTION_TO_SECURE_SITE:
	    label->setText( i18n( "Warning! Your connection has been redirected to a different\n"
				  "site. You may not be connected to the site that you originally\n"
				  "tried to reach." ) );
	    continuePB->setText( i18n( "Confirm Loading" ) );
	    cancelPB->setText( i18n( "Cancel Loading" ) );
	    cancel_p = true;
	    break;
	default:
	    abort ();
	}

    label->adjustSize();
    continuePB->adjustSize();

    //  bad hack (even in QtMozilla - sigh...)
    if( !cancel_p )
	delete cancelPB;
    else {
	cancelPB->adjustSize();
	if( cancel_default_p )
	    cancelPB->setDefault( true );
    }

#ifndef NO_HELP
    QPushButton* helpPB = new QPushButton( i18n( "&Help" ), buttonRow );
    helpPB->adjustSize();
#endif

    vlayout->activate();

    resize( 0, 0 );
}


void SecurityDialog::prefsCBToggled( bool value )
{
    *prefs = value;
}
