/* $Id: QtBookmarkButton.cpp,v 1.1 1998/09/25 18:01:27 ramiro%netscape.com Exp $
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

#include "QtBookmarkButton.h"
#include "QtBookmarkMenu.h"
#include "QtContext.h"
#include "qtoolbar.h"

QtBookmarkButton::QtBookmarkButton( const QPixmap & pm, const char * textLabel,
				    QtContext *context,
				    QToolBar * parent, const char * name )
    : QToolButton( parent, name )
{
    setPixmap( pm );
    setUsesBigPixmap( FALSE );
    setToggleButton( TRUE );
    setTextLabel( textLabel );

    if ( context->inherits( "QtBrowserContext" ) )
	menu = new QtBookmarkMenu( (QtBrowserContext*)context );
    else
	menu = new QtBookmarkMenu();
    connect( this, SIGNAL(toggled(bool)), SLOT(beenToggled(bool)) );
    menu->installEventFilter( this );
    connect( menu, SIGNAL(destroyed()),
	     this, SLOT(menuDied()) );
}

QtBookmarkButton::~QtBookmarkButton()
{
    delete menu;
}

void QtBookmarkButton::beenToggled( bool up )
{
    if ( up )
	menu->popup( mapToGlobal( rect().bottomLeft()));
    else
	menu->hide();
}


bool QtBookmarkButton::eventFilter( QObject *o, QEvent *e )
{
    if ( o->inherits("QPopupMenu") && e->type() == Event_Hide )
	setOn( FALSE );
    if ( o->inherits("QLabel") && e->type() == Event_MouseButtonPress ) 
       toggle();
    return FALSE;
}




void QtBookmarkButton::menuDied()
{
    menu = 0;
}
