/* $Id: QtBookmarkMenu.cpp,v 1.1 1998/09/25 18:01:27 ramiro%netscape.com Exp $
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

#include "QtBookmarkMenu.h"
#include "QtBookmarksContext.h"
#include "QtBrowserContext.h"
#include "menus.h"
#include <bkmks.h>
#include <qpopmenu.h>
#include <time.h>
#include <qkeycode.h>
#include <qptrdict.h>

#define i18n(txt) txt

class QtBMEntryDict : public QIntDict<void>
{
public:
    QtBMEntryDict( int size = 17 ) : QIntDict<void>(size) {}
    ~QtBMEntryDict() {}
    void  insert( long k, const BM_Entry *d )
		    { QIntDict<void>::insert( k,(void*)d); }
    void  replace( long k, const BM_Entry *d )
		    { QIntDict<void>::replace( k,(void*)d); }
    BM_Entry *take( long k )
                    { return (BM_Entry*) QIntDict<void>::take(k); }
    BM_Entry *find( long k )	const
                    { return (BM_Entry*) QIntDict<void>::find(k); }
    BM_Entry *operator[]( long k ) const
                    { return (BM_Entry*) QIntDict<void>::operator[](k); }
};

static QPtrDict<QtBookmarkMenu> menuDict;

QtBookmarkMenu::QtBookmarkMenu( QtBrowserContext *b,
				QWidget *p, const char *wName )
    : QPopupMenu( p, wName )
{
    root      = 0;
    browser   = b;
    dirty     = TRUE;
    entryDict = new QtBMEntryDict;
    menuDict.insert( this, this );
    connect( this, SIGNAL(highlighted(int)), SLOT(highlightedSlot(int)) );
    connect( this, SIGNAL(activated(int)),   SLOT(activatedSlot(int)) );
    connect( this, SIGNAL(aboutToShow()),   SLOT(beforeShow()) );
}

QtBookmarkMenu::~QtBookmarkMenu()
{
    delete entryDict;
    menuDict.remove( this );
}

void QtBookmarkMenu::insertGuide()
{
    QPopupMenu *tmp = new QPopupMenu();
    insertItem( *getBMPixmap( QtBMFolderItem, FALSE ), "Guide", tmp );
    tmp->insertItem( "The &Internet", this, SLOT(guideI()) );
    tmp->insertItem( "&People", this, SLOT(guideP()) );
    tmp->insertItem( "&Yellow Pages", this, SLOT(guideY()) );
    tmp->insertItem( "What's &New", this, SLOT(guideN()) );
    tmp->insertItem( "What's &Cool", this, SLOT(guideC()) );
}

void QtBookmarkMenu::populate()
{
    clear();
    if ( browser ) {
	insertItem( i18n( "&Add Bookmark" ), this, SLOT(cmdAddBookmark()), 
		    ALT + Key_K, MENU_BOOKMARK_ADD );
	setAccel( MENU_BOOKMARK_ADD, ALT+Key_K );
    }
    insertItem( i18n( "&Edit Bookmarks" ), QtBookmarksContext::ptr(),
		SLOT(cmdEditBookmarks()), ALT + Key_B, MENU_BOOKMARK_EDIT );
    int idCounter = MENU_BMF_BOOKMARKS_OFFSET;
    insertSeparator();
    insertGuide();
    insertItem( i18n( "Troll Tech" ), this, SLOT(trollTech()), idCounter++ );
    insertSeparator();
    idCounter++;
    populate( this, QtBookmarksContext::ptr()->rootEntry(), &idCounter);
    if ( width() > 250 )
	resize( 250, height() );
}

void QtBookmarkMenu::clear()
{
    entryDict->clear();
    QPopupMenu::clear();
}

void QtBookmarkMenu::cmdAddBookmark()
{
    if ( browser ) {
	BM_Entry* entry;
	MWContext *c = browser->mwContext();
	entry = BM_NewUrl( c->title ? c->title : c->url, c->url, 0, time(0) );

	BM_Entry *tmp = BM_GetChildren(QtBookmarksContext::ptr()->rootEntry());
	BM_Entry *prev = 0;
	while( tmp ) {
	    prev = tmp;
	    tmp = BM_GetNext(tmp);
	}
	BM_InsertItemAfter( QtBookmarksContext::ptr()->mwContext(),prev,entry);
	BM_SaveBookmarks( QtBookmarksContext::ptr()->mwContext(), 0 );
    }
}

void QtBookmarkMenu::highlightedSlot( int id )
{
    BM_Entry *entry = entryDict->find( id );
    if ( entry ) {
	const char *url = BM_GetAddress( entry );
	if ( browser )
	    browser->setMessage( url );
    }
}

void QtBookmarkMenu::activatedSlot( int id )
{
    BM_Entry *entry = entryDict->find( id );
    if ( entry ) {
	const char *url = BM_GetAddress( entry );
	URL_Struct * urlStruct = NET_CreateURLStruct(url,NET_DONT_RELOAD);
	if ( browser )
	    browser->getURL( urlStruct );
        else
	    QtContext::makeNewWindow( urlStruct, 0, 0, 0 );
    }
}

void QtBookmarkMenu::invalidate()
{
    QPtrDictIterator<QtBookmarkMenu> it(menuDict);
    while( it.current() ) {
	it.current()->dirty = TRUE;
	++it;
    }
}

void QtBookmarkMenu::beforeShow()
{
    if ( dirty ) {
	populate();
	dirty = FALSE;
    }
}

void QtBookmarkMenu::goTo( const char *url)
{
    URL_Struct * urlStruct = NET_CreateURLStruct( url, NET_DONT_RELOAD );
    if ( browser )
	browser->getURL( urlStruct );
    else
	QtContext::makeNewWindow( urlStruct, 0, 0, 0 );
}

void QtBookmarkMenu::trollTech()
{
    goTo( "http://www.troll.no" );
}

void QtBookmarkMenu::guideI()
{
    goTo( "http://guide.netscape.com/" );
}

void QtBookmarkMenu::guideP()
{
    goTo( "http://guide.netscape.com/guide/people.html" );
}

void QtBookmarkMenu::guideY()
{
    goTo( "http://guide.netscape.com/guide/yellow_pages.html" );
}

void QtBookmarkMenu::guideN()
{
    goTo( "http://guide.netscape.com/guide/whats_new.html" );
}

void QtBookmarkMenu::guideC()
{
    goTo( "http://guide.netscape.com/guide/whats_cool.html" );
}

void QtBookmarkMenu::populate( QPopupMenu *popup, BM_Entry *entry,
			       int *idCounter )
{
    BM_Entry *child = BM_GetChildren( entry );

    while( child ) {
	if ( BM_IsHeader( child ) ) {
	    QPopupMenu *tmp = new QPopupMenu();
	    connect( tmp, SIGNAL(highlighted(int)),
		     SLOT(highlightedSlot(int)) );
	    connect( tmp, SIGNAL(activated(int)),
		     SLOT(activatedSlot(int)) );
	    popup->insertItem( *getBMPixmap( QtBMFolderItem, FALSE ),
			       BM_GetName(child), tmp );
	    populate( tmp, child, idCounter );
	} else if ( BM_IsSeparator( child) ) {
	    popup->insertSeparator();
	} else if ( BM_IsUrl( child ) ) {
	    popup->insertItem( *getBMPixmap( QtBMBookmarkItem, FALSE ),
			       BM_GetName(child), *idCounter );
	    entryDict->insert( *idCounter, child );
	    (*idCounter)++;
	}
	child = BM_GetNext( child );
    }
}
