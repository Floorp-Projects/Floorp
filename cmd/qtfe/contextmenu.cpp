/* $Id: contextmenu.cpp,v 1.1 1998/09/25 18:01:32 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Arnt
 * Gulbrandsen.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#include "contextmenu.h"

#include <qapp.h>
#include <qclipbrd.h>
#include <qkeycode.h>
#include <qstring.h>
#include <qmsgbox.h>
#include <shist.h>

#include "QtBrowserContext.h"


RCSTAG("$Id: contextmenu.cpp,v 1.1 1998/09/25 18:01:32 ramiro%netscape.com Exp $");



ContextMenu::ContextMenu( QtBrowserContext * parent, const QPoint & pos,
			  const char * linkToURL, const char * imageURL )
    : QPopupMenu( 0 ),
      context( parent ),
      linkTo( linkToURL ),
      image (imageURL )
{
    move( pos );

    connect( parent, SIGNAL(destroyed()),
	     this, SLOT(goAway()) );

    insert( Back, "&Back", context, SLOT(cmdBack()), CTRL+Key_Left );
    setItemEnabled( Back, SHIST_CanGoBack( context->mwContext() ) );
    insert( Forward, "&Forward", context, SLOT(cmdForward()), CTRL+Key_Right );
    setItemEnabled( Forward, SHIST_CanGoForward( context->mwContext() ) );
    insert( Reload, "&Reload", context, SLOT(cmdReload()) );

    if ( !linkTo.isEmpty() ) {
	insertSeparator();
	insert( OpenLinkWindow, "&Open Link in New Window",
		this, SLOT(openLinkWindow()) );
    }

    insertSeparator();
    insert( ViewSource, "View Page So&urce",
	    context, SLOT(cmdViewPageSource()) );
    insert( ViewInfo, "&View Page Info",
	    context, SLOT(cmdViewPageInfo()) );
    if ( !image.isEmpty() )
	insert( ViewImage, "View I&mage", this, SLOT(viewImage()) );

    insertSeparator();
    insert( AddBookmark, "&Add Bookmark",
	    context, SLOT(cmdAddBookmark()), CTRL+Key_K );
    insert( SendPage, "Sen&d Page", this, SLOT(sendPage()) );

    if ( !image.isEmpty() || !linkTo.isEmpty() ) {
	insertSeparator();
	if ( !image.isEmpty() )
	    insert( SaveImageAs, "Save Image As...",
		    this, SLOT(saveImageAs()) );
	if ( !linkTo.isEmpty() ) {
	    insert( SaveLink, "&Save Link As...",
		    this, SLOT( saveLinkAs() ) );
	}
// #warning Background is missing. Kalle.
	if ( !linkTo.isEmpty() )
	    insert( CopyLinkLocation, "Cop&y Link Location", this,
		    SLOT(copyLinkLocation()) );
	if ( !image.isEmpty() )
	    insert( CopyImageLocation, "Cop&y Image Location",
		    this, SLOT(copyImageLocation()) );
    }
}


ContextMenu::~ContextMenu()
{
    // nothing
}


void ContextMenu::goAway()
{
    delete this;
}


void ContextMenu::insert( int id, const char * text,
			  QObject * receiver, const char * slot,
			  int accel )
{
    insertItem( text, id );
    connectItem( id, receiver, slot );
    if ( accel )
	setAccel( accel, id );
}


void ContextMenu::openLinkWindow()
{
    context->browserGetURL( linkTo );
    delete this;
}


void ContextMenu::sendPage()
{
    (void) QMessageBox::information( 0, "QtMozilla",
				     "Congratulations!\n\n"
				     "You found the e*ter e*g!\n",
				     "OK", "Find Others" );
    /* looking here doesn't help :) */
    delete this;
}


void ContextMenu::viewImage()
{
    context->browserGetURL( image );
    delete this;
}


void ContextMenu::saveLinkAs()
{
    if( linkTo.isEmpty() )
	  context->alert( tr( "Not over a link." ) );
    else
	  context->saveURL( NET_CreateURLStruct( linkTo, NET_DONT_RELOAD ) );
    delete this;
}

void ContextMenu::saveImageAs()
{
      if( image.isEmpty() )
		context->alert( tr( "Not over an image." ) );
	  else
		context->saveURL( NET_CreateURLStruct( image, NET_DONT_RELOAD ) );
      delete this;
}


void ContextMenu::copyLinkLocation()
{
    QApplication::clipboard()->setText( linkTo.data() );
    delete this;
}


void ContextMenu::copyImageLocation()
{
    QApplication::clipboard()->setText( image.data() );
    delete this;
}
