/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Appkit/Appkit.h>
#include <Carbon/Carbon.h>

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsVoidArray.h"

#import "MainController.h"
#import "BookmarksToolbar.h"
#import "ExtendedOutlineView.h"

class nsIAtom;
class nsIDOMHTMLDocument;

@class BookmarksDataSource;
@class BookmarkItem;

// despite appearances, BookmarksService is not a singleton. We make one for the bookmarks menu,
// one each per BookmarksDataSource, and one per bookmarks toolbar. It relies on a bunch of global
// variables, which is evil.
class BookmarksService
{
public:
  BookmarksService(BookmarksDataSource* aDataSource);
  BookmarksService(BookmarksToolbar* aToolbar);
  virtual ~BookmarksService();

  void AddObserver();
  void RemoveObserver();

public:
  static void BookmarkAdded(nsIContent* aContainer, nsIContent* aChild, bool shouldFlush = true);
  static void BookmarkChanged(nsIContent* aItem, bool shouldFlush = true);
  static void BookmarkRemoved(nsIContent* aContainer, nsIContent* aChild, bool shouldFlush = true);

  static void AddBookmarkToFolder(nsString& aURL, nsString& aTitle, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt);
  static void MoveBookmarkToFolder(nsIDOMElement* aBookmark, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt);
  static void DeleteBookmark(nsIDOMElement* aBookmark);
  
  static void GetRootContent(nsIContent** aResult);
  static BookmarkItem* GetRootItem();
  static BookmarkItem* GetWrapperFor(nsIContent* aItem);
  static BookmarkItem* GetWrapperFor(PRUint32 contentID);
  
  static void ReadBookmarks();
  static void FlushBookmarks();

  static void ConstructBookmarksMenu(NSMenu* aMenu, nsIContent* aContent);
  static void OpenMenuBookmark(BrowserWindowController* aController, id aMenuItem);
  static void AddMenuBookmark(NSMenu* aMenu, nsIContent* aParent, nsIContent* aChild, PRInt32 aIndex);
  static NSMenu* LocateMenu(nsIContent* aContent);

  static void ConstructAddBookmarkFolderList(NSPopUpButton* aPopup, BookmarkItem* aItem);
  
  static NSImage* CreateIconForBookmark(nsIDOMElement* aElement);

  static void EnsureToolbarRoot();

  static void ImportBookmarks(nsIDOMHTMLDocument* aHTMLDoc);
  
  static void GetTitleAndHrefForBrowserView(id aBrowserView, nsString& aTitle, nsString& aHref);
  static void OpenBookmarkGroup(id aTabView, nsIDOMElement* aFolder);
  
  static NSString* ResolveKeyword(NSString* aKeyword);

  static BOOL DoAncestorsIncludeNode(BookmarkItem* bookmark, BookmarkItem* searchItem);
  static bool IsBookmarkDropValid(BookmarkItem* proposedParent, int index, NSArray* draggedIDs);
  static bool PerformBookmarkDrop(BookmarkItem* parent, int index, NSArray* draggedIDs);
  static bool PerformProxyDrop(BookmarkItem* parentItem, BookmarkItem* beforeItem, NSDictionary* data);
  
  static bool PerformURLDrop(BookmarkItem* parentItem, BookmarkItem* beforeItem, NSString* title, NSString* url);
  
public:
  // Global counter and pointers to our singletons.
  static PRUint32 gRefCnt;

  // A dictionary that maps from content IDs (which uniquely identify content nodes)
  // to Obj-C bookmarkItem objects.  These objects are handed back to UI elements like
  // the outline view.
  static NSMutableDictionary* gDictionary;
  static MainController* gMainController;
  static NSMenu* gBookmarksMenu;
  static nsIDOMElement* gToolbarRoot;
  static nsIAtom* gFolderAtom;
  static nsIAtom* gNameAtom;
  static nsIAtom* gHrefAtom;
  static nsIAtom* gKeywordAtom;
  static nsIAtom* gDescriptionAtom;
  static nsIAtom* gBookmarkAtom;
  static nsIAtom* gOpenAtom;
  static nsIAtom* gGroupAtom;
  static nsIDocument* gBookmarks;
  static BOOL     gBookmarksFileReadOK;
  static nsVoidArray* gInstances;
  static int CHInsertNone;
  static int CHInsertInto;
  static int CHInsertBefore;
  static int CHInsertAfter;
  
private:
  // There are three kinds of bookmarks data sources:
  // tree views (mDataSource), the personal toolbar (mToolbar)
  // and menus (gBookmarksMenu).
  BookmarksToolbar* mToolbar;
  BookmarksDataSource* mDataSource;
};



// singleton bookmarks manager object

@interface BookmarksManager : NSObject
{



}

+ (BookmarksManager*)sharedBookmarksManager;

- (void)loadProxyImageFor:(id)requestor withURI:(NSString*)inURIString;


@end


