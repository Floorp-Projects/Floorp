/* $Id: QtBookmarksContext.h,v 1.1 1998/09/25 18:01:28 ramiro%netscape.com Exp $
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

#ifndef QTBOOKMARKSCONTEXT_H
#define QTBOOKMARKSCONTEXT_H

#include "QtContext.h"
#include <qpopmenu.h>
#include <qmainwindow.h>

class  QMainWindow;
class  QListView;
class  QListViewItem;

class  QtBrowserContext;
struct QtBMClipboard;
struct QtBookmarkEditDialog;
struct BM_FindInfo;

class QtBookmarksContext : public QtContext {
/* A Bookmark window */
    Q_OBJECT
public:
    virtual ~QtBookmarksContext();

    static QtBookmarksContext *qt( MWContext *c );

    static QtBookmarksContext *ptr();
    BM_Entry *rootEntry();

public slots:
    void cmdEditBookmarks();

    void cmdNewBookmark();
    void cmdNewFolder();
    void cmdNewSeparator();
    void cmdOpenBookmarkFile();
    void cmdImport();
    void cmdSaveAs();
    void cmdOpenSelected();
    void cmdOpenAddToToolbar();
    void cmdClose();
    void cmdExit();

    void cmdUndo();
    void cmdRedo();
    void cmdCut();
    void cmdCopy();
    void cmdPaste();
    void cmdDelete();
    void cmdSelectAll();
    void cmdFindInObject();
    void cmdFindAgain();
    void cmdSearch();
    void cmdSearchAddress();
    void cmdBookmarkProperties();

    void cmdSortByTitle();
    void cmdSortByLocation();
    void cmdSortByDateLastVisited();
    void cmdSortByDateCreated();
    void cmdSortAscending();
    void cmdSortDescending();
    void cmdBookmarkUp();
    void cmdBookmarkDown();
    void cmdBookmarkWhatsNew();
    void cmdSetToolbarFolder();
    void cmdSetNewBookmarkFolder();
    void cmdSetBookmarkMenuFolder();

public:
    // ***** BMFE_ funcs:

    void  refreshCells ( int32 first, int32 last, bool now );
    void  syncDisplay ();
    void  measureEntry ( BM_Entry* entry, uint32* width, uint32* height );
    void  setClipContents ( void* buffer, int32 length );
    void* getClipContents ( int32* length );
    void  freeClipContents ();
    void  openBookmarksWindow ();
    void  editItem ( BM_Entry* entry );
    void  entryGoingAway ( BM_Entry* entry );
    void  gotoBookmark ( const char* url, const char* target );
    void* openFindWindow ( BM_FindInfo* findInfo );
    void  scrollIntoView ( BM_Entry* entry );
    void  bookmarkMenuInvalid ();
    void  updateWhatsChanged ( const char* url, int32 done, int32 total,
			       const char* totaltime );
    void  finishedWhatsChanged ( int32 totalchecked,
				 int32 numreached, int32 numchanged );
    void  startBatch ();
    void  endBatch ();

    // ***** End BMFE_ funcs:

private slots:
    void activateItem( QListViewItem * );
    void rightClick( QListViewItem *, const QPoint &, int );
    void newSelected( QListViewItem * );

private:
    QtBookmarksContext( MWContext * );

    void changeSorting( int column, bool ascending );
    void createWidget();
    void invalidateListView();
    void createEditDialog();
    void initBMData();
    bool loadBookmarks( const char *fileName = 0 );
    BM_Entry *currentEntry();
    bool bmSelectHighlighted();
    void editNewEntry( BM_Entry* );

    // this method is not implemented in QtBrowserContext.cpp, but in
    // menus.cpp
    void populateMenuBar( QMenuBar* );

    QListView            *listView;
    QMainWindow	         *bookmarkWidget;
    QtBookmarkEditDialog *editDialog;
    QtBMClipboard        *clip;
    int	                  sortColumn;
    bool                  sortAscending;
    bool                  bmDataLoaded;
    bool                  inBatch;
};

enum  QtBMItemType { QtBMFolderItem, QtBMBookmarkItem };

QPixmap *getBMPixmap( QtBMItemType type, bool isOpen );

#endif
