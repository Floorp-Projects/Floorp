/* $Id: QtBookmarksContext.cpp,v 1.1 1998/09/25 18:01:27 ramiro%netscape.com Exp $
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



/*

  This first approximation hack should be a good starting point. The
  QListView is very poorly synchronized with the BM api (bkmks.h), editing
  commands are therefore very poorly handled.

  This should not be too hard to fix.

  The bookmark file is also hardcoded to be the file
  ".netscape/bookmarks.html" under the users home directory.
*/

#include "QtBookmarksContext.h"
#include "toolbars.h"
#include "DialogPool.h"
#include "bkmks.h"

#include <qmainwindow.h>
#include <qmsgbox.h>
#include <qtimer.h>
#include <qscrollview.h>
#include <qapp.h>
#include <qlistview.h>
#include <qdir.h>

#include "prefapi.h"
#include "layers.h"
#include "icons.h"
#include "menus.h"
#include "QtBrowserContext.h"
#include "QtBookmarkEditDialog.h"
#include "QtBookmarkMenu.h"

#define i18n(x) x

class Folder;

const int bmIconHeight = 16;

typedef struct QtBMClipboard {
  void *buffer;
  int32 length;
};

class QtBMItem : public QListViewItem
{
public:
    QtBMItem( QListView * parent, BM_Entry *e );
    QtBMItem( Folder * parent, BM_Entry *e );

    const char * text( int column ) const;
    void setup();
    virtual QtBMItemType type() = 0;

    BM_Entry *bmEntry() { return entry; }
    virtual void paintCell( QPainter *, const QColorGroup&,
			    int column, int width, int alignment );
protected:
    BM_Entry *entry;
    Folder   *p;
};

class Folder: public QtBMItem
{
public:
    Folder( QListView * parent, BM_Entry *e );
    Folder( Folder * parent, BM_Entry *e );

    const char * text( int column ) const;

    void setOpen( bool );
    virtual QtBMItemType type() { return QtBMFolderItem; }

};

class Bookmark: public QtBMItem
{
public:
    Bookmark( Folder * parent, BM_Entry *e );

    const char * text( int column ) const;
    virtual QtBMItemType type() { return QtBMBookmarkItem; }
};

QtBMItem::QtBMItem( Folder * parent, BM_Entry *e )
    : QListViewItem( parent )
{
    p     = parent;
    entry = e;
}

static const char *oneLine( const char *txt )
{
    static QString tmp;

    tmp = txt;
    int hit = tmp.find( "\n" );
    if ( hit )
	return tmp = tmp.left( hit );
    return tmp;
}

QtBMItem::QtBMItem( QListView * parent, BM_Entry *e )
    : QListViewItem( parent )
{
    p     = 0;
    entry = e;
}

QPixmap *getBMPixmap( QtBMItemType type, bool isOpen )
{
    static Pixmap openFolder( "BM_FolderO" );
    static Pixmap closedFolder( "BM_Folder" );
    static Pixmap bookmark( "BM_Bookmark" );

    if ( type == QtBMFolderItem ) {
	if ( isOpen )
	    return &openFolder;
	else
	    return &closedFolder;
    }
    return &bookmark;
}


void QtBMItem::paintCell( QPainter *p, const QColorGroup &cg,
			  int column, int width, int /* alignment */ )
{
    if ( column == 0 ) {
	p->fillRect( 0, 0, width, height(), cg.base() );

	QPixmap *pm = getBMPixmap( type(), isOpen() );
	QListView *lv = listView();
	int xpos = lv ? lv->itemMargin() : 2;
	int ypos = (height() - pm->height())/2;
	p->drawPixmap( xpos, ypos, *pm );
	xpos += pm->width() + 2;

	const char * t = text( column );
	if ( t ) {
	    int marg = lv ? lv->itemMargin() : 2;

	    if ( isSelected() &&
		 (column==0 || listView()->allColumnsShowFocus()) ) {
		if ( listView()->style() == WindowsStyle ) {
		    p->fillRect( xpos - marg, 0, width - xpos + marg, height(),
				 QApplication::winStyleHighlightColor());
		    p->setPen( white ); // ###
		} else {
		    p->fillRect( xpos - marg, 0, width - xpos
				 + marg, height(), cg.text() );
		    p->setPen( cg.base() );
		}
	    } else {
		p->setPen( cg.text() );
	    }

	    p->drawText( xpos, 0, width - marg - xpos, height(),
			 AlignLeft + AlignVCenter, t );
	}	
    } else {
	QListViewItem::paintCell( p, cg, column, width, AlignLeft );
    }
}

/*!
  Displays default column texts. Called when columns don't contain data
*/
const char * QtBMItem::text( int column ) const
{
    return (column == 3) ? "-" : "---";
}

void QtBMItem::setup()
{
    if ( !p )
	setExpandable( TRUE );
    else
	setExpandable( BM_IsHeader(entry) && BM_GetChildren(entry) );
    setHeight( height() > bmIconHeight ? height() : bmIconHeight );
    QListViewItem::setup();
}

Folder::Folder( Folder * parent, BM_Entry * e )
    : QtBMItem( parent, e )
{
}

Folder::Folder( QListView * parent, BM_Entry *e )
    : QtBMItem( parent, e )
{
}

void Folder::setOpen( bool o )
{
    if ( o && childCount() == 0 ) {
	BM_Entry *child = BM_GetChildren( entry );

	while( child ) {
	    if ( BM_IsHeader( child ) ) {
		new Folder( this, child );
	    } else {
		new Bookmark( this, child ); // Simplification ###
	    }
	    child = BM_GetNext( child );
	}
    }
    QListViewItem::setOpen( o );
}

const char * Folder::text( int column ) const
{
    switch( column ) {
    case 0 : return oneLine(BM_GetName(entry)); break;
	/* loc = fe_ConvertToLocaleEncoding(INTL_DefaultWinCharSetID(NULL),
	   (unsigned char*)name); */
    case 3 : {
	const char *txt = oneLine(BM_PrettyAddedOnDate( entry ));
	return txt ? txt : "-";
	break;
      }
    }
    return QtBMItem::text( column ); // Display default value
}


Bookmark::Bookmark(  Folder * parent, BM_Entry * e )
    : QtBMItem( parent, e )
{
}

const char * Bookmark::text( int column ) const
{
    const char *txt = 0;
    switch( column ) {
    case 0 : txt = oneLine(BM_GetName(entry)); break;
	/* loc = fe_ConvertToLocaleEncoding(INTL_DefaultWinCharSetID(NULL),
	   (unsigned char*)name); */
    case 1 : txt = oneLine(BM_GetAddress( entry )); break;
    case 2 : txt = oneLine(BM_PrettyLastVisitedDate( entry )); break;
    case 3 : txt = oneLine(BM_PrettyAddedOnDate( entry )); break;
    };
    return txt ? txt : QtBMItem::text( column );
}

static QtBookmarksContext *globalBookmarkPtr = 0;

static void cleanupGlobalBookmarkPtr()
{
    delete globalBookmarkPtr;
    globalBookmarkPtr = 0;
}

QtBookmarksContext* QtBookmarksContext::ptr()
{
    if ( !globalBookmarkPtr ) {	
	MWContext* context = XP_NewContext();
	/*
	context->type = chrome ? chrome->type
			: parent ? parent->type
			: MWContextBookmarks
			*/
	context->type = MWContextBookmarks;
	globalBookmarkPtr = new QtBookmarksContext( context );
	qAddPostRoutine( cleanupGlobalBookmarkPtr );
    }
    return globalBookmarkPtr;
}

BM_Entry *QtBookmarksContext::rootEntry()
{
    if ( !bmDataLoaded )
	loadBookmarks();
    return BM_GetRoot(mwContext());
}



QtBookmarksContext::QtBookmarksContext( MWContext *cx )
 : QtContext(cx)
{
    bookmarkWidget    = 0;
    listView          = 0;
    editDialog        = 0;
    sortColumn	      = 0;
    sortAscending     = TRUE;
    bmDataLoaded      = FALSE;
    inBatch           = FALSE;
    clip              = new QtBMClipboard;
    if ( BM_InitializeBookmarksContext( cx ) < 0 ) {
	printf( "QtBookmarksContext::QtBookmarksContext: "
		"Cannot initialize bookmarks context.\n" );
    }
    BM_SetFEData( mwContext(), this );
}

QtBookmarksContext::~QtBookmarksContext()
{
    delete clip;
}

QtBookmarksContext* QtBookmarksContext::qt( MWContext* c )
{
    QtBookmarksContext *bm = (QtBookmarksContext*)BM_GetFEData( c );
    if ( !bm )
	debug("QtBookmarksContext::qt: NULL bookmark context data.");
    return bm;
}

void QtBookmarksContext::createWidget()
{
    if ( bookmarkWidget )
	return;
    if ( !loadBookmarks() ) {
	printf( "QtBookmarksContext::createWidget:"
		"Cannot load bookmarks.\n" );
    }
    bookmarkWidget = new QMainWindow( 0, "Main bookmark widget" );
    // use default geometry, etc. as if we are the main widget
    qApp->setMainWidget(bookmarkWidget);
    // but there isn't one - multiple browsers all equal.
    qApp->setMainWidget(0);
    populateMenuBar( bookmarkWidget->menuBar() );

    listView = new QListView( bookmarkWidget, "Bookmark listview" );

    listView->setRootIsDecorated( TRUE );
    listView->setFocusPolicy( QWidget::StrongFocus );
    listView->setSorting( -1  );
    listView->setCaption( i18n("QtMozilla Bookmarks") );
    listView->addColumn( i18n("Name"), 150 );
    listView->addColumn( i18n("Location"), 150 );
    listView->addColumn( i18n("Last Visited"), 100 );
    listView->addColumn( i18n("Created On"), 100 );
    bookmarkWidget->resize( 500, 600 );

    connect( listView, SIGNAL(doubleClicked(QListViewItem*)),
		       SLOT(activateItem(QListViewItem*)) );

    connect( listView, SIGNAL(returnPressed(QListViewItem*)),
		       SLOT(activateItem(QListViewItem*)) );

    connect( listView, SIGNAL(selectionChanged(QListViewItem*)),
		       SLOT(newSelected(QListViewItem*)) );

    connect( listView,
	     SIGNAL(rightButtonPressed(QListViewItem*,const QPoint&,int)),
	     SLOT(rightClick(QListViewItem*,const QPoint&,int)) );

    Folder *root = new Folder( listView, BM_GetRoot(mwContext())  );
    root->setOpen( TRUE );

    bookmarkWidget->setCentralWidget( listView );
    bookmarkWidget->show();
}

void QtBookmarksContext::invalidateListView()
{
    if ( listView ) {
	listView->clear();
	Folder *root = new Folder( listView, BM_GetRoot(mwContext())  );
	root->setOpen( TRUE );
    }
}

void QtBookmarksContext::createEditDialog()
{
    if ( editDialog )
	return;
    editDialog = new QtBookmarkEditDialog( this );
    editDialog->setFixedWidth( 400 );
    editDialog->show();
}

bool QtBookmarksContext::loadBookmarks( const char *fName )
{
  QString url(512);		// XFE uses 512 here.
  XP_StatStruct st;
  int result;
  QString fileName = fName;
  if ( fileName.isNull() ) {
      char *buf;
      if ( PREF_CopyCharPref( "browser.bookmark_file", &buf ) == PREF_OK ) {
	  fileName = buf;
	  free(buf);
      }
  }
#if defined(XP_UNIX)
  if ( fileName.isNull() ) { //### should get this from backend instead
      QDir d = QDir::home();
      fileName = d.filePath( ".netscape/bookmarks.html" );
  }
#endif
  result = XP_Stat( fileName, &st, xpBookmarks);

  if (result == -1 && errno == ENOENT && !fileName.isEmpty() ) {
    printf ("Since the bookmarks file does not exist, "
	    "try to copy the default one, eh, well, not implemented.\n");
    /*    char *defaultBM = fe_getDefaultBookmarks();

	  if (defaultBM) {
	  copyBookmarksFile(filename, defaultBM);
	  XP_FREE(defaultBM);
	  }*/
    return FALSE;
  }
  url.sprintf( "file:%s", (const char *) fileName );

  BM_ReadBookmarksFromDisk( mwContext(), fileName.data(), url);
  bmDataLoaded = TRUE;
  return TRUE;
}

void QtBookmarksContext::cmdEditBookmarks()
{
    createWidget();
    bookmarkWidget->show();
    bookmarkWidget->raise();
}


void QtBookmarksContext::editNewEntry( BM_Entry *newEntry )
{
    createEditDialog();
    BM_Entry *entry = currentEntry();
    if ( !entry )
	entry = rootEntry();
    BM_InsertItemAfter( mwContext(), entry, newEntry);

    editDialog->show();
    editDialog->raise();
    editDialog->setEntry( newEntry );
}

void QtBookmarksContext::cmdNewBookmark()
{
    createWidget();
    editNewEntry( BM_NewUrl( "", "", 0, 0 ) );
}

void QtBookmarksContext::cmdNewFolder()
{
    createWidget();
    editNewEntry( BM_NewHeader( "" ) );
}

void QtBookmarksContext::cmdNewSeparator()
{
    createWidget();
    BM_ObeyCommand( mwContext(), BM_Cmd_InsertSeparator );
}

void QtBookmarksContext::cmdOpenBookmarkFile()
{
    createWidget();
}

void QtBookmarksContext::cmdImport()
{
    createWidget();
}

void QtBookmarksContext::cmdSaveAs()
{
    createWidget();
}

void QtBookmarksContext::cmdOpenSelected()
{
    createWidget();
}

void QtBookmarksContext::cmdOpenAddToToolbar()
{
    createWidget();
}

void QtBookmarksContext::cmdClose()
{
    createWidget();
}

void QtBookmarksContext::cmdExit()
{
    createWidget();
}


void QtBookmarksContext::cmdUndo()
{
    createWidget();
    if ( bmSelectHighlighted() )
	BM_ObeyCommand( mwContext(), BM_Cmd_Undo );
}

void QtBookmarksContext::cmdRedo()
{
    createWidget();
    if ( bmSelectHighlighted() )
	BM_ObeyCommand( mwContext(), BM_Cmd_Redo );
}

void QtBookmarksContext::cmdCut()
{
    createWidget();
    if ( bmSelectHighlighted() )
	BM_ObeyCommand( mwContext(), BM_Cmd_Cut );
}

void QtBookmarksContext::cmdCopy()
{
    createWidget();
    if ( bmSelectHighlighted() )
	BM_ObeyCommand( mwContext(), BM_Cmd_Copy );
}

void QtBookmarksContext::cmdPaste()
{
    createWidget();
    if ( bmSelectHighlighted() )
	BM_ObeyCommand( mwContext(), BM_Cmd_Paste );
}


BM_Entry *QtBookmarksContext::currentEntry()
{
    createWidget();
    QtBMItem * current = (QtBMItem*)listView->currentItem();
    return current ? current->bmEntry() : 0;
}

bool QtBookmarksContext::bmSelectHighlighted()
{
    BM_Entry *entry = currentEntry();
    if ( entry ) {
	BM_SelectItem( mwContext(), entry, FALSE, FALSE, TRUE );
	return TRUE;
    } else {
	return FALSE;
    }
}

void QtBookmarksContext::cmdDelete()
{
    createWidget();
    if ( bmSelectHighlighted() )
	BM_ObeyCommand( mwContext(), BM_Cmd_Delete );
}

void QtBookmarksContext::cmdSelectAll()
{
    createWidget();
}

void QtBookmarksContext::cmdFindInObject()
{
    createWidget();
}

void QtBookmarksContext::cmdFindAgain()
{
    createWidget();
}

void QtBookmarksContext::cmdSearch()
{
    createWidget();
}

void QtBookmarksContext::cmdSearchAddress()
{
    createWidget();
}

void QtBookmarksContext::cmdBookmarkProperties()
{
    createWidget();
    QtBMItem * current = (QtBMItem*)listView->currentItem();
    if ( current ) {
	createEditDialog();
	editDialog->setEntry( current->bmEntry() );
    }
}

void QtBookmarksContext::changeSorting( int column, bool ascending )
{
    createWidget();
    sortColumn    = column;
    sortAscending = ascending;
    listView->setSorting( sortColumn, sortAscending );
}

void QtBookmarksContext::cmdSortByTitle()
{
    changeSorting( 0, sortAscending );
}

void QtBookmarksContext::cmdSortByLocation()
{
    changeSorting( 1, sortAscending );
}

void QtBookmarksContext::cmdSortByDateLastVisited()
{
    changeSorting( 2, sortAscending );
}

void QtBookmarksContext::cmdSortByDateCreated()
{
    changeSorting( 3, sortAscending );
}

void QtBookmarksContext::cmdSortAscending()
{
    changeSorting( sortColumn, TRUE );
}

void QtBookmarksContext::cmdSortDescending()
{
    changeSorting( sortColumn, FALSE );
}

void QtBookmarksContext::cmdBookmarkUp()
{
    createWidget();
}

void QtBookmarksContext::cmdBookmarkDown()
{
    createWidget();
}

void QtBookmarksContext::cmdBookmarkWhatsNew()
{
    createWidget();
}

void QtBookmarksContext::cmdSetToolbarFolder()
{
    createWidget();
}

void QtBookmarksContext::cmdSetNewBookmarkFolder()
{
    createWidget();
}

void QtBookmarksContext::cmdSetBookmarkMenuFolder()
{
    createWidget();
}

/*************** Implementation of BMFE_ funcs start here: ***************/

void QtBookmarksContext::refreshCells ( int32 /*first*/, int32 /*last*/, bool /*now*/)
{
    if ( listView )
	listView->triggerUpdate(); // Not worth the trouble to search
}

void QtBookmarksContext::syncDisplay ()
{
    refreshCells( 1, BM_LAST_CELL, TRUE );
}

void QtBookmarksContext::measureEntry ( BM_Entry* entry,
				       uint32* width, uint32* height)
{
}

void QtBookmarksContext::setClipContents ( void* buffer, int32 length)
{
    freeClipContents();

    clip->buffer = XP_ALLOC(length);
    XP_MEMCPY(clip->buffer, buffer, length);
    clip->length = length;
}

void *QtBookmarksContext::getClipContents ( int32* length)
{
    if (length)
	*length = clip->length;

    return (clip->buffer);
    return 0;
}

void QtBookmarksContext::freeClipContents ()
{
    if (clip->buffer)
	XP_FREE(clip->buffer);

    clip->buffer = NULL;
    clip->length = 0;
}

void QtBookmarksContext::openBookmarksWindow ()
{
    createEditDialog();
}

void QtBookmarksContext::editItem ( BM_Entry* entry)
{
    createEditDialog();
    editDialog->show();
    editDialog->raise();
    editDialog->setEntry( entry );
}

void QtBookmarksContext::entryGoingAway ( BM_Entry* entry)
{
    if ( editDialog && entry == editDialog->bmEntry() ) {
	editDialog->clear();
    }
}

void QtBookmarksContext::gotoBookmark (
		const char* url, const char* target)
{
    QtContext::makeNewWindow( NET_CreateURLStruct(url,NET_DONT_RELOAD),0,0,0);
}

/*
void reuseBrowser( MWContext *context, URL_Struct *url )
{
    if (!context) {
	QtContext::makeNewWindow( url, 0, 0, 0 );
	return;
    }
    // otherwise, just get any other Browser context
    XP_List *context_list = XP_GetGlobalContextList();
    int n_context_list = XP_ListCount(context_list);
    for (i = 1; i <= n_context_list; i++) {
	MWContext * compContext = (MWContext *)XP_ListGetObjectNum(context_list, i);
	if (compContext->type == MWContextBrowser &&
			!compContext->is_grid_cell
			&& !CONTEXT_DATA(compContext)->hasCustomChrome
			&& compContext->pHelpInfo == NULL
			&& !(context->name != NULL && // don't use view source either
				 XP_STRNCMP(context->name, "view-source", 11) == 0)) {
			FE_RaiseWindow(compContext);
			return compContext;
		}
	}

*/

void *QtBookmarksContext::openFindWindow ( BM_FindInfo* findInfo)
{
    return 0;
}

void QtBookmarksContext::scrollIntoView ( BM_Entry* entry)
{
}

void QtBookmarksContext::bookmarkMenuInvalid ()
{
    QtBookmarkMenu::invalidate();
    //    invalidateListView();
}

void QtBookmarksContext::updateWhatsChanged (
			       const char* url, /* If NULL, just display
						   "Checking..." */
			       int32 done, int32 total,
			       const char* totaltime)
{
}

void QtBookmarksContext::finishedWhatsChanged ( int32 totalchecked,
				 int32 numreached, int32 numchanged)
{
}

void QtBookmarksContext::startBatch ()
{
    inBatch = TRUE;
}

void QtBookmarksContext::endBatch ()
{
    inBatch = FALSE;
}

/*************** Implementation of BMFE_ funcs ends here: ***************/

void QtBookmarksContext::activateItem( QListViewItem *i )
{
    QtBMItem *bmItem = (QtBMItem*)i;
    switch( bmItem->type() ) {
    case QtBMFolderItem:
	break;
    case QtBMBookmarkItem:
	BM_GotoBookmark( mwContext(), bmItem->bmEntry() );
	break;
    }
}

void QtBookmarksContext::rightClick( QListViewItem *i, const QPoint &p, int )
{
    QPopupMenu pop;
    pop.insertItem( "Edit bookmark" );

    if ( pop.exec(p) != -1 ) {
	QtBMItem *bmItem = (QtBMItem*)i;
	editItem( bmItem->bmEntry() );
    }
}

void QtBookmarksContext::newSelected( QListViewItem *i )
{
    if ( editDialog && editDialog->isVisible() ) {
	QtBMItem *bmItem = (QtBMItem*)i;
	if ( bmItem )
	    editDialog->setEntry( bmItem->bmEntry() );
    }
}

