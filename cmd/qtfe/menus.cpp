/* $Id: menus.cpp,v 1.1 1998/09/25 18:01:35 ramiro%netscape.com Exp $
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

#include <qkeycode.h>
#include <qmenubar.h>
#include <qpopmenu.h>

#include "callback.h"
#include "menus.h"
#include "QtBrowserContext.h"
#include "QtBookmarksContext.h"
#include "QtBookmarkMenu.h"
#include "QtContext.h"
#include "QtHistoryContext.h"

// general menus
static QPopupMenu* createNewMenu( QObject* receiver );
static QPopupMenu* createWindowMenu( QObject* receiver );
static QPopupMenu* createHelpMenu( QObject* receiver );
static QPopupMenu* createEncodingMenu( QObject* receiver );
static QPopupMenu* createBookmarksMenu( QObject* receiver );

// browser frame-specific menus
static QPopupMenu* createBrowserFrameFileMenu( QObject* receiver );
static QPopupMenu* createBrowserFrameEditMenu( QObject* receiver );
static QPopupMenu* createBrowserFrameViewMenu( QObject* receiver );
static QPopupMenu* createBrowserFrameGoMenu( QObject* receiver );

// bookmark frame-specific menus
static QPopupMenu* createBookmarkFrameFileMenu( QObject* receiver );
static QPopupMenu* createBookmarkFrameEditMenu( QObject* receiver );
static QPopupMenu* createBookmarkFrameViewMenu( QObject* receiver );

// history frame-specific menus
static QPopupMenu* createHistoryFrameFileMenu( QObject* receiver );
static QPopupMenu* createHistoryFrameEditMenu( QObject* receiver );
static QPopupMenu* createHistoryFrameViewMenu( QObject* receiver );



#define i18n( text ) text

#define SUBMENU( parent, id, label, submenu ) \
(parent)->insertItem( (label), (submenu), (id) );
#define MENUITEM( parent, id, label, receiver, func, accel ) \
(parent)->insertItem( (label), (id) ); \
(parent)->connectItem( (id), (receiver), \
		       (func) ) ; \
(parent)->setAccel( (accel), (id) );
#define MENUSEPARATOR( parent ) \
(parent)->insertSeparator();

void QtBrowserContext::populateMenuBar( QMenuBar* menuBar )
{ 
  SUBMENU( menuBar, MENU_BF_FILE, 
	   i18n( "&File" ), createBrowserFrameFileMenu( this ) );
  SUBMENU( menuBar, MENU_BF_EDIT,
	   i18n( "&Edit" ), createBrowserFrameEditMenu( this ) );
  SUBMENU( menuBar, MENU_BF_VIEW,
	   i18n( "&View" ), createBrowserFrameViewMenu( this ) );
  SUBMENU( menuBar, MENU_BF_GO,
	   i18n( "&Go" ), createBrowserFrameGoMenu( this ) );
  SUBMENU( menuBar, MENU_WINDOW,
	   i18n( "&Communicator" ), createWindowMenu( this ) );
  MENUSEPARATOR( menuBar );
  SUBMENU( menuBar, MENU_HELP,
	   i18n( "&Help" ), createHelpMenu( this ) );
}


void QtBookmarksContext::populateMenuBar( QMenuBar* menuBar )
{
  SUBMENU( menuBar, MENU_BMF_FILE, 
	   i18n( "&File" ), createBookmarkFrameFileMenu( this ) );
  SUBMENU( menuBar, MENU_BMF_EDIT,
	   i18n( "&Edit" ), createBookmarkFrameEditMenu( this ) );
  SUBMENU( menuBar, MENU_BMF_VIEW,
	   i18n( "&View" ), createBookmarkFrameViewMenu( this ) );
  SUBMENU( menuBar, MENU_WINDOW,
	   i18n( "&Communicator" ), createWindowMenu( this ) );
  MENUSEPARATOR( menuBar );
  SUBMENU( menuBar, MENU_HELP,
	   i18n( "&Help" ), createHelpMenu( this ) );
}

void QtHistoryContext::populateMenuBar(QMenuBar* menuBar)
{
  SUBMENU( menuBar, MENU_HIST_FILE, 
	   i18n( "&File" ), createHistoryFrameFileMenu( this ) );
  SUBMENU( menuBar, MENU_HIST_EDIT,
	   i18n( "&Edit" ), createHistoryFrameEditMenu( this ) );
  SUBMENU( menuBar, MENU_HIST_VIEW,
	   i18n( "&View" ), createHistoryFrameViewMenu( this ) );
  SUBMENU( menuBar, MENU_WINDOW,
	   i18n( "&Communicator" ), createWindowMenu( this ) );
  MENUSEPARATOR( menuBar );
  SUBMENU( menuBar, MENU_HELP,
	   i18n( "&Help" ), createHelpMenu( this ) );
}


static QPopupMenu* createNewMenu( QObject* receiver )
{
    QPopupMenu* newMenu = new QPopupMenu();

    MENUITEM( newMenu, MENU_NEW_NAVIGATORWINDOW,
	      i18n( "&Navigator Window" ), receiver,
	      SLOT( cmdOpenBrowser() ),
	      CTRL + Key_N );
    MENUITEM( newMenu, MENU_NEW_COMPOSEMESSAGE,
	      i18n( "New &Message" ), receiver,
	      SLOT( cmdComposeMessage() ),
	      CTRL + Key_M );
#ifdef EDITOR
    MENUSEPARATOR( newMenu );
    MENUITEM( newMenu, MENU_NEW_BLANKPAGE,
	      i18n( "Blank &Page" ), receiver,
	      SLOT( cmdNewBlank() ),
	      CTRL + SHIFT + Key_N );
    MENUITEM( newMenu, MENU_NEW_NEWTEMPLATE,
	      i18n( "New from &Template" ), receiver,
	      SLOT( cmdNewTemplate() ), 0 );
    MENUITEM( newMenu, MENU_NEW_NEWWIZARD,
	      i18n( "New from &Wizard" ), receiver,
	      SLOT( cmdNewWizard() ), 0 );
#endif	      

return newMenu;
}

static QPopupMenu* createBrowserFrameFileMenu( QObject* receiver )
{
    QPopupMenu* fileMenu = new QPopupMenu();

#if ( defined( MOZ_MAIL_NEWS ) || defined( EDITOR ) )
    SUBMENU( fileMenu, MENU_NEW, i18n( "&New" ), createNewMenu( receiver ) );
#else
    MENUITEM( fileMenu, MENU_BF_FILE_NAVIGATORWINDOW,
	      i18n( "&Navigator Window" ), receiver,
	      SLOT( cmdOpenBrowser() ),
	      CTRL + Key_N );
#endif
    MENUITEM( fileMenu, MENU_BF_FILE_OPENPAGE,
	      i18n( "&OpenPage..." ), receiver,
	      SLOT( cmdOpenPage() ),
	      CTRL + Key_O );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_BF_FILE_SAVEAS,
	      i18n( "&Save As..." ), receiver,
	      SLOT( cmdSaveAs() ),
	      CTRL + Key_S );
    MENUITEM( fileMenu, MENU_BF_FILE_SAVEFRAMEAS,
	      i18n( "Save &Frame As..." ), receiver,
	      SLOT( cmdSaveFrameAs() ), 0 );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_BF_FILE_SENDPAGE,
	      i18n( "Sen&d Page" ), receiver,
	      SLOT( cmdSendPage() ), 0 );
    MENUITEM( fileMenu, MENU_BF_FILE_SENDLINK,
	      i18n( "Send Lin&k" ), receiver,
	      SLOT( cmdSendLink() ), 0 );
    MENUSEPARATOR( fileMenu );
#ifdef EDITOR
    MENUITEM( fileMenu, MENU_BF_FILE_EDITPAGE,
	      i18n( "&Edit Page" ), receiver,
	      SLOT( cmdEditPage() ), 0 );
    MENUITEM( fileMenu, MENU_BF_FILE_EDITFRAME,
	      i18n( "Edit &Frame" ), receiver,
	      SLOT( cmdEditFrame() ), 0 );
#endif
    MENUITEM( fileMenu, MENU_BF_FILE_UPLOADFILE,
	      i18n( "&Upload File" ), receiver,
	      SLOT( cmdUploadFile() ), 0 );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_BF_FILE_PRINT,
	      i18n( "&Print" ), receiver,
	      SLOT( cmdPrint() ),
	      CTRL + Key_P );
    MENUITEM( fileMenu, MENU_BF_FILE_CLOSE,
	      i18n( "&Close" ), receiver,
	      SLOT( cmdClose() ),
	      CTRL + Key_W );
    MENUITEM( fileMenu, MENU_BF_FILE_QUIT,
	      i18n( "&Quit" ), receiver,
	      SLOT( cmdQuit() ),
	      CTRL + Key_Q );

return fileMenu;
}

static QPopupMenu* createBrowserFrameEditMenu( QObject* receiver )
{
    QPopupMenu* editMenu = new QPopupMenu();

    MENUITEM( editMenu, MENU_BF_EDIT_UNDO,
	      i18n( "&Undo" ), receiver,
	      SLOT( cmdUndo() ),
	      CTRL + Key_Z );
    MENUITEM( editMenu, MENU_BF_EDIT_REDO,
	      i18n( "&Redo" ), receiver,
	      SLOT( cmdRedo() ),
	      CTRL + Key_Y );
    MENUSEPARATOR( editMenu );
    MENUITEM( editMenu, MENU_BF_EDIT_CUT,
	      i18n( "Cu&t" ), receiver,
	      SLOT( cmdCut() ),
	      CTRL + Key_X );
    MENUITEM( editMenu, MENU_BF_EDIT_COPY,
	      i18n( "&Copy" ), receiver,
	      SLOT( cmdCopy() ),
	      CTRL + Key_C );
    MENUITEM( editMenu, MENU_BF_EDIT_PASTE,
	      i18n( "&Paste" ), receiver,
	      SLOT( cmdPaste() ),
	      CTRL + Key_V );
    MENUITEM( editMenu, MENU_BF_EDIT_SELECTALL,
	      i18n( "Select &all" ), receiver,
	      SLOT( cmdSelectAll() ),
	      CTRL + Key_A );
    MENUSEPARATOR( editMenu );
    MENUITEM( editMenu, MENU_BF_EDIT_FINDINOBJECT,
	      i18n( "&Find in Page..." ), receiver,
	      SLOT( cmdFindInObject() ),
	      CTRL + Key_F );
    MENUITEM( editMenu, MENU_BF_EDIT_FINDAGAIN,
	      i18n( "Find A&gain" ), receiver,
	      SLOT( cmdFindAgain() ),
	      CTRL + Key_G );
    MENUITEM( editMenu, MENU_BF_EDIT_SEARCHINTERNET,
	      i18n( "Search &Internet" ), receiver,
	      SLOT( cmdSearch() ), 0 );
#ifdef MOZ_MAIL_NEWS
    MENUITEM( editMenu, MENU_BF_EDIT_SEARCHADDRESS,
	      i18n( "Search Director&y" ), receiver,
	      SLOT( cmdSearchAddress() ), 0 );
#endif
    MENUSEPARATOR( editMenu );
    MENUITEM( editMenu, MENU_BF_EDIT_PREFERENCES,
	      i18n( "Pr&eferences" ), receiver,
	      SLOT( cmdEditPreferences() ), 0 );

    return editMenu;
}

static QPopupMenu* createBrowserFrameViewMenu( QObject* receiver )
{
    QPopupMenu* viewMenu = new QPopupMenu(); 

MENUITEM( viewMenu, 
	      MENU_BF_VIEW_TOGGLENAVIGATIONTOOLBAR, 
	      i18n( "Hide &Navigation Toolbar" ),  receiver,
	      SLOT( cmdToggleNavigationToolbar() ), 0 );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_TOGGLELOCATIONTOOLBAR, 
	      i18n( "Hide &Location Toolbar" ),  receiver,
	      SLOT( cmdToggleLocationToolbar() ), 0 );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_TOGGLEPERSONALTOOLBAR, 
	      i18n( "Hide &Personal Toolbar" ),  receiver,
	      SLOT( cmdTogglePersonalToolbar() ), 0 );
    MENUSEPARATOR( viewMenu );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_INCREASEFONT, 
	      i18n( "In&crease Font" ),  receiver,
	      SLOT( cmdIncreaseFont() ), CTRL + Key_Plus );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_DECREASEFONT, 
	      i18n( "&Decrease Font" ),  receiver,
	      SLOT( cmdDecreaseFont() ), CTRL + Key_Minus );
    MENUSEPARATOR( viewMenu );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_RELOAD, 
	      i18n( "&Reload" ),  receiver,
	      SLOT( cmdReload() ), CTRL + Key_R );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_SHOWIMAGES, 
	      i18n( "Sho&w Images" ),  receiver,
	      SLOT( cmdShowImages() ), CTRL + Key_I );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_REFRESH, 
	      i18n( "Re&fresh" ),  receiver,
	      SLOT( cmdRefresh() ), 0 );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_STOPLOADING, 
	      i18n( "&Stop Loading" ),  receiver,
	      SLOT( cmdStopLoading() ), Key_Escape );
    MENUSEPARATOR( viewMenu );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_VIEWPAGESOURCE, 
	      i18n( "Page So&urce" ),  receiver,
	      SLOT( cmdViewPageSource() ), 0 );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_VIEWPAGEINFO, 
	      i18n( "Page &Info" ),  receiver,
	      SLOT( cmdViewPageInfo() ), 0 );
    MENUITEM( viewMenu, 
	      MENU_BF_VIEW_PAGESERVICES, 
	      i18n( "Page Ser&vices" ),  receiver,
	      SLOT( cmdPageServices() ), 0 );
    SUBMENU( viewMenu, MENU_BF_VIEW_ENCODING, i18n( "&Encoding" ),
	     createEncodingMenu( receiver ) );

return viewMenu;
}

static QPopupMenu* createBrowserFrameGoMenu( QObject* receiver )
{
  QPopupMenu* goMenu = new QPopupMenu();

MENUITEM( goMenu,
	    MENU_BF_GO_BACK,
	    i18n( "&Back" ), receiver,
	    SLOT( cmdBack() ), CTRL + Key_Left );
  MENUITEM( goMenu,
	    MENU_BF_GO_FORWARD,
	    i18n( "&Forward" ), receiver,
	    SLOT( cmdForward() ), CTRL + Key_Right );
  MENUITEM( goMenu,
	    MENU_BF_GO_HOME,
	    i18n( "&Home" ), receiver,
	    SLOT( cmdHome() ), 0 );
  MENUSEPARATOR( goMenu );

return goMenu;
}

static QPopupMenu* createBookmarksMenu( QObject *o )
{
    QtBrowserContext *browser = 0;
    if ( o->inherits( "QtBrowserContext" ) )
	browser = (QtBrowserContext *) o;
    QtBookmarkMenu* bookmarksMenu = new QtBookmarkMenu( browser );
    return bookmarksMenu;
}

static QPopupMenu* createWindowMenu( QObject* receiver )
{
  QPopupMenu* windowMenu = new QPopupMenu();

MENUITEM( windowMenu,
	    MENU_WINDOW_OPENNAVCENTER,
	    i18n( "&Navigator" ), receiver,
	    SLOT( cmdOpenNavCenter() ), CTRL + Key_1
	    );
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENBROWSER,
	    i18n( "Open or Bring Up Browser" ),  receiver,
	    SLOT( cmdOpenOrBringUpBrowser() ), 0
	    );
#ifdef MOZ_MAIL_NEWS
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENINBOX,
	    i18n( "&Messenger Mailbox" ), receiver,
	    SLOT( cmdOpenInbox() ), CTRL + Key_2
	    );
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENNEWSGROUPS,
	    i18n( "&Collabra Discussions" ), receiver,
	    SLOT( cmdOpenNewsgroups() ), CTRL + Key_3
	    );
#endif
#ifdef EDITOR
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENEDITOR,
	    i18n( "&Page Composer" ), receiver,
	    SLOT( cmdOpenEditor() ), CTRL + Key_4
	    );
#endif
#ifdef MOZ_MAIL_NEWS
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENCONFERENCE,
	    i18n( "C&onference" ), receiver,
	    SLOT( cmdOpenConference() ), CTRL + Key_5
	    );
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENCALENDAR, receiver,
	    i18n( "Calenda&r" ),
	    SLOT( cmdCalendar() ), CTRL + Key_6
	    );
  MENUITEM( windowMenu,
	    MENU_WINDOW_HOSTONDEMAND, receiver,
	    i18n( "IBM Host On-Demand" ),
	    SLOT( cmdHostOnDemand() ), CTRL + Key_7
	    );
#endif
#ifdef MOZ_NETCAST
  MENUITEM( windowMenu,
	    MENU_WINDOW_NETCASTER, receiver,
	    i18n( "N&etcaster" ),
	    SLOT( cmdNetcaster() ), CTRL + Key_8
	    );
#endif
#ifdef MOZ_TASKBAR
  MENUSEPARATOR( windowMenu );
  MENUITEM( windowMenu,
	    MENU_WINDOW_TOGGLETASKBAR, receiver,
	    i18n( "Dock Component Bar" ),
	    SLOT( cmdToggleTaskbarShowing() ), 0
	    );
#endif
#ifdef MOZ_MAIL_NEWS
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENFOLDERS,
	    i18n( "Message Center" ), receiver,
	    SLOT( cmdOpenFolders() ), CTRL + SHIFT + Key_1
	    );
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENADDRESSBOOK,
	    i18n( "Adress Book" ), receiver,
	    SLOT( cmdOpenAddressBook() ), CTRL + SHIFT + Key_2
	    );
#endif
  SUBMENU( windowMenu, MENU_WINDOW_BOOKMARKS, 
	   i18n( "&Bookmarks" ),
	   createBookmarksMenu( receiver ) );
  MENUITEM( windowMenu,
	    MENU_WINDOW_OPENHISTORY,
	    i18n( "&History" ), receiver,
	    SLOT( cmdOpenHistory() ), CTRL + Key_H
	    );
#if JAVA
  MENUITEM( windowMenu,
	    MENU_WINDOW_JAVACONSOLE,
	    i18n( "&Java Console" ), receiver,
	    SLOT( cmdJavaConsole() ), 0
	    );
#endif
#ifndef NO_SECURITY
  MENUITEM( windowMenu,
	    MENU_WINDOW_SECURITY,
	    i18n( "&SecurityInfo" ), receiver,
	    SLOT( cmdViewSecurity() ), CTRL + SHIFT + Key_I
	    );
#endif
  MENUSEPARATOR( windowMenu );

return windowMenu;
};

QPopupMenu* createEncodingMenu( QObject* receiver )
{
  QPopupMenu* encodingMenu = new QPopupMenu();

  //debug( "Sorry, not implemented: encoding menu %p", receiver );

  return encodingMenu;
}

static QPopupMenu* createHelpMenu( QObject* receiver )
{
  QPopupMenu* helpMenu = new QPopupMenu();

#ifdef MOZ_COMMUNICATOR_ABOUT
  MENUITEM( helpMenu,
	    MENU_HELP_ABOUTNETSCAPE,
	    i18n( "&About Netscape" ), receiver,
	    SLOT( cmdAboutNetscape() ), 0
	    );
#else
  MENUITEM( helpMenu,
	    MENU_HELP_ABOUTMOZILLA,
	    i18n( "&About Mozilla" ), receiver,
	    SLOT( cmdAboutMozilla() ), 0
	    );
#endif

MENUITEM( helpMenu,
	    MENU_HELP_ABOUTQT,
	    i18n( "About &Qt" ), receiver,
	    SLOT( cmdAboutQt() ), 0 );

return helpMenu;
};



static QPopupMenu* createBookmarkFrameFileMenu( QObject* receiver )
{
    QPopupMenu* fileMenu = new QPopupMenu();

    SUBMENU( fileMenu, MENU_NEW, i18n( "&New" ), createNewMenu( receiver ) );
    MENUITEM( fileMenu, MENU_BMF_FILE_NEWBOOKMARK,
	      i18n( "New &Bookmark..." ), receiver,
	      SLOT( cmdNewBookmark() ), 0 );
    MENUITEM( fileMenu, MENU_BMF_FILE_NEWFOLDER,
	      i18n( "New F&older..." ), receiver,
	      SLOT( cmdNewFolder() ), 0 );
    MENUITEM( fileMenu, MENU_BMF_FILE_NEWSEPARATOR,
	      i18n( "New S&eparator" ), receiver,
	      SLOT( cmdNewSeparator() ), 0 );
    MENUITEM( fileMenu, MENU_BMF_FILE_OPENBOOKMARKFILE,
	      i18n( "Open Bookmarks &File" ), receiver,
	      SLOT( cmdOpenBookmarkFile() ), 0 );
    MENUITEM( fileMenu, MENU_BMF_FILE_IMPORT,
	      i18n( "Import..." ), receiver,
	      SLOT( cmdImport() ), 0 );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_BMF_FILE_SAVEAS,
	      i18n( "&Save As..." ), receiver,
	      SLOT( cmdSaveAs() ), CTRL + Key_S );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_BMF_FILE_OPENSELECTED,
	      i18n( "Go to Bookmar&k" ), receiver,
	      SLOT( cmdOpenSelected() ), CTRL + Key_O );
    MENUITEM( fileMenu, MENU_BMF_FILE_ADDTOTOOLBAR,
	      i18n( "&Add Selection to Toolbar" ), receiver,
	      SLOT( cmdOpenAddToToolbar() ), 0 );
    MENUITEM( fileMenu, MENU_BMF_FILE_MAKEALIAS,
	      i18n( "&Make Alias" ), receiver,
	      SLOT( cmdMakeAlias() ), 0 );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_BMF_FILE_CLOSE,
	      i18n( "&Close" ), receiver,
	      SLOT( cmdClose() ), CTRL + Key_W );
    MENUITEM( fileMenu, MENU_BMF_FILE_EXIT,
	      i18n( "&Quit" ), receiver,
	      SLOT( cmdExit() ), CTRL + Key_Q );

    return fileMenu;
}


static QPopupMenu* createBookmarkFrameEditMenu( QObject* receiver )
{
    QPopupMenu* editMenu = new QPopupMenu();

    MENUITEM( editMenu, MENU_BMF_EDIT_UNDO,
	      i18n( "&Undo" ), receiver,
	      SLOT( cmdUndo() ),
	      CTRL + Key_Z );
    MENUITEM( editMenu, MENU_BMF_EDIT_REDO,
	      i18n( "&Redo" ), receiver,
	      SLOT( cmdRedo() ),
	      CTRL + Key_Y );
    MENUSEPARATOR( editMenu );
    MENUITEM( editMenu, MENU_BMF_EDIT_CUT,
	      i18n( "Cu&t" ), receiver,
	      SLOT( cmdCut() ),
	      CTRL + Key_X );
    MENUITEM( editMenu, MENU_BMF_EDIT_COPY,
	      i18n( "&Copy" ), receiver,
	      SLOT( cmdCopy() ),
	      CTRL + Key_C );
    MENUITEM( editMenu, MENU_BMF_EDIT_PASTE,
	      i18n( "&Paste" ), receiver,
	      SLOT( cmdPaste() ),
	      CTRL + Key_V );
    MENUITEM( editMenu, MENU_BMF_EDIT_DELETE,
	      i18n( "&Delete" ), receiver,
	      SLOT( cmdDelete() ),
	      CTRL + Key_D );
    MENUITEM( editMenu, MENU_BMF_EDIT_SELECTALL,
	      i18n( "Select &all" ), receiver,
	      SLOT( cmdSelectAll() ),
	      CTRL + Key_A );
    MENUSEPARATOR( editMenu );
    MENUITEM( editMenu, MENU_BMF_EDIT_FINDINOBJECT,
	      i18n( "&Find in Page..." ), receiver,
	      SLOT( cmdFindInObject() ),
	      CTRL + Key_F );
    MENUITEM( editMenu, MENU_BMF_EDIT_FINDAGAIN,
	      i18n( "Find A&gain" ), receiver,
	      SLOT( cmdFindAgain() ),
	      CTRL + Key_G );
    MENUITEM( editMenu, MENU_BMF_EDIT_SEARCHINTERNET,
	      i18n( "Search &Internet" ), receiver,
	      SLOT( cmdSearch() ), 0 );
#ifdef MOZ_MAIL_NEWS
    MENUITEM( editMenu, MENU_BMF_EDIT_SEARCHADDRESS,
	      i18n( "Search Director&y" ), receiver,
	      SLOT( cmdSearchAddress() ), 0 );
#endif
    MENUSEPARATOR( editMenu );
    MENUITEM( editMenu, MENU_BMF_EDIT_PREFERENCES,
	      i18n( "Pr&eferences" ), receiver,
	      SLOT( cmdBookmarkProperties() ), 0 );

    editMenu->setItemEnabled( MENU_BMF_EDIT_SELECTALL, FALSE );

    return editMenu;
}


static QPopupMenu* createBookmarkFrameViewMenu( QObject* receiver )
{
    QPopupMenu* viewMenu = new QPopupMenu();

    MENUITEM( viewMenu, MENU_BMF_VIEW_SORTBYTITLE,
	      i18n( "by &Title" ), receiver,
	      SLOT( cmdSortByTitle() ), 0 );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SORTBYLOCATION,
	      i18n( "by &Location" ), receiver,
	      SLOT( cmdSortByLocation() ), 0 );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SORTBYDATELASTVISITED,
	      i18n( "by Date Last &Visited" ), receiver,
	      SLOT( cmdSortByDateLastVisited() ), 0 );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SORTBYDATECREATED,
	      i18n( "by Date &Created" ), receiver,
	      SLOT( cmdSortByDateCreated() ), 0 );
    MENUSEPARATOR( viewMenu );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SORTASCENDING,
	      i18n( "Sort &Ascending" ), receiver,
	      SLOT( cmdSortAscending() ), 0 );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SORTDESCENDING,
	      i18n( "Sort &Descending" ), receiver,
	      SLOT( cmdSortDescending() ), 0 );
    MENUSEPARATOR( viewMenu );
    MENUITEM( viewMenu, MENU_BMF_VIEW_BOOKMARKUP,
	      i18n( "Move Up" ), receiver,
	      SLOT( cmdBookmarkUp() ), SHIFT + Key_Up );
    MENUITEM( viewMenu, MENU_BMF_VIEW_BOOKMARKDOWN,
	      i18n( "Move Down" ), receiver,
	      SLOT( cmdBookmarkDown() ), SHIFT + Key_Down );
    MENUSEPARATOR( viewMenu );
    MENUITEM( viewMenu, MENU_BMF_VIEW_BOOKMARKWHATSNEW,
	      i18n( "Update &Bookmarks" ), receiver,
	      SLOT( cmdBookmarkWhatsNew() ), 0 );
    MENUSEPARATOR( viewMenu );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SETTOOLBARFOLDER,
	      i18n( "Set as Toolbar &Folder" ), receiver,
	      SLOT( cmdSetToolbarFolder() ), 0 );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SETNEWBOOKMARKFOLDER,
	      i18n( "Set as Folder for &New Bookmarks" ), receiver,
	      SLOT( cmdSetNewBookmarkFolder() ), 0 );
    MENUITEM( viewMenu, MENU_BMF_VIEW_SETBOOKMARKMENUFOLDER,
	      i18n( "Set as Bookmark &Menu" ), receiver,
	      SLOT( cmdSetBookmarkMenuFolder() ), 0 );

    return viewMenu;
}

//
// History menus
//

static QPopupMenu* createHistoryFrameFileMenu( QObject* receiver )
{
    QPopupMenu* fileMenu = new QPopupMenu();

    SUBMENU( fileMenu, MENU_NEW, i18n( "&New" ), createNewMenu( receiver ) );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_HIST_FILE_SAVEAS,
	      i18n( "&Save As..." ), receiver,
	      SLOT( cmdSaveAs() ), CTRL + Key_S );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_HIST_FILE_GOTOPAGE,
	      i18n( "Go to Page..." ), receiver,
	      SLOT( cmdGotoPage() ), CTRL + Key_O);
    MENUITEM( fileMenu, MENU_HIST_FILE_ADDBOOKMARK,
	      i18n( "&Add Bookmark" ), receiver,
	      SLOT( cmdAddBookmark() ), CTRL + Key_K);
    MENUITEM( fileMenu, MENU_HIST_FILE_ADDTOOLBAR,
	      i18n( "Add Page to Toolbar" ), receiver,
	      SLOT( cmdAddToToolbar() ), 0 );
    MENUSEPARATOR( fileMenu );
    MENUITEM( fileMenu, MENU_HIST_FILE_CLOSE,
	      i18n( "&Close" ), receiver,
	      SLOT( cmdClose() ), CTRL + Key_W );
    MENUITEM( fileMenu, MENU_HIST_FILE_EXIT,
	      i18n( "&Quit" ), receiver,
	      SLOT( cmdQuit() ), CTRL + Key_Q );

    return fileMenu;
}


static QPopupMenu* createHistoryFrameEditMenu( QObject* receiver )
{
    QPopupMenu* editMenu = new QPopupMenu();

    MENUITEM( editMenu, MENU_HIST_EDIT_UNDO,
	      i18n( "&Undo" ), receiver,
	      SLOT( cmdUndo() ),
	      CTRL + Key_Z );
    MENUITEM( editMenu, MENU_HIST_EDIT_REDO,
	      i18n( "&Redo" ), receiver,
	      SLOT( cmdRedo() ),
	      CTRL + Key_Y );
    MENUSEPARATOR( editMenu );
    MENUITEM( editMenu, MENU_HIST_EDIT_CUT,
	      i18n( "Cu&t" ), receiver,
	      SLOT( cmdCut() ),
	      CTRL + Key_X );
    MENUITEM( editMenu, MENU_HIST_EDIT_COPY,
	      i18n( "&Copy" ), receiver,
	      SLOT( cmdCopy() ),
	      CTRL + Key_C );
    MENUITEM( editMenu, MENU_HIST_EDIT_PASTE,
	      i18n( "&Paste" ), receiver,
	      SLOT( cmdPaste() ),
	      CTRL + Key_V );
    MENUITEM( editMenu, MENU_HIST_EDIT_DELETE,
	      i18n( "&Delete" ), receiver,
	      SLOT( cmdDelete() ),
	      CTRL + Key_D );
    return editMenu;
}

static QPopupMenu* createHistoryFrameViewMenu( QObject* receiver )
{
    QPopupMenu* viewMenu = new QPopupMenu();
    viewMenu->setCheckable(TRUE);

    MENUITEM( viewMenu, MENU_HIST_VIEW_SORTBYTITLE,
	      i18n( "by &Title" ), receiver,
	      SLOT( cmdSortByTitle() ), 0 );
    MENUITEM( viewMenu, MENU_HIST_VIEW_SORTBYLOCATION,
	      i18n( "by &Location" ), receiver,
	      SLOT( cmdSortByLocation() ), 0 );
    MENUITEM( viewMenu, MENU_HIST_VIEW_SORTBYDATEFIRSTVISITED,
	      i18n( "by Date &First Visited" ), receiver,
	      SLOT( cmdSortByDateFirstVisited() ), 0 );
    MENUITEM( viewMenu, MENU_HIST_VIEW_SORTBYDATELASTVISITED,
	      i18n( "by Date Last &Visited" ), receiver,
	      SLOT( cmdSortByDateLastVisited() ), 0 );
    MENUITEM( viewMenu, MENU_HIST_VIEW_EXPIRATIONDATE,
	      i18n( "by &Expiration Date" ), receiver,
	      SLOT( cmdSortByExpirationDate() ), 0 );
    MENUITEM( viewMenu, MENU_HIST_VIEW_VISITCOUNT,
	      i18n( "by Visit &Count" ), receiver,
	      SLOT( cmdSortByVisitCount() ), 0 );
    MENUSEPARATOR(viewMenu);
    MENUITEM(viewMenu, MENU_HIST_VIEW_SORTASCENDING,
	     i18n( "Sort &Ascending" ), receiver,
	     SLOT( cmdSortAscending() ), 0);
    MENUITEM( viewMenu, MENU_HIST_VIEW_SORTDESCENDING,
	      i18n( "Sort &Descending" ), receiver,
	      SLOT( cmdSortDescending() ), 0 );
    return viewMenu;
}
