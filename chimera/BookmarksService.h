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
#import "CHBookmarksToolbar.h"
#import "CHExtendedOutlineView.h"

class BookmarksService;
class nsIAtom;

@interface BookmarksDataSource : NSObject
{
  BookmarksService* mBookmarks;

  IBOutlet id mOutlineView;	
  IBOutlet id mBrowserWindowController;
  IBOutlet id mAddButton;	
  IBOutlet id mDeleteButton;

  NSString* mCachedHref;
}

-(id) init;
-(void) dealloc;
-(void) windowClosing;

-(void) ensureBookmarks;

-(IBAction)addBookmark:(id)aSender;
-(void)endAddBookmark: (int)aCode;

-(IBAction)deleteBookmarks: (id)aSender;
-(void)deleteBookmark: (id)aItem;

-(IBAction)addFolder:(id)aSender;

-(void)addBookmark:(id)aSender useSelection:(BOOL)aSel isFolder:(BOOL)aIsFolder;

// Datasource methods.
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren;

@end

@interface BookmarkItem : NSObject
{
  nsIContent* mContentNode;
}

-(nsIContent*)contentNode;
-(void)setContentNode: (nsIContent*)aContentNode;
- (id)copyWithZone:(NSZone *)aZone;
@end

class nsIDOMHTMLDocument;

class BookmarksService
{
public:
  BookmarksService(BookmarksDataSource* aDataSource);
  BookmarksService(CHBookmarksToolbar* aToolbar);
  virtual ~BookmarksService();

  void AddObserver();
  void RemoveObserver();

  static void BookmarkAdded(nsIContent* aContainer, nsIContent* aChild);
  static void BookmarkChanged(nsIContent* aItem);
  static void BookmarkRemoved(nsIContent* aContainer, nsIContent* aChild);

  static void AddBookmarkToFolder(nsString& aURL, nsString& aTitle, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt);
  static void MoveBookmarkToFolder(nsIDOMElement* aBookmark, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt);
  static void DeleteBookmark(nsIDOMElement* aBookmark);
  
public:
  static void GetRootContent(nsIContent** aResult);
  static BookmarkItem* GetWrapperFor(nsIContent* aItem);
  static void FlushBookmarks();

  static void ConstructBookmarksMenu(NSMenu* aMenu, nsIContent* aContent);
  static void OpenMenuBookmark(BrowserWindowController* aController, id aMenuItem);
  static void AddMenuBookmark(NSMenu* aMenu, nsIContent* aParent, nsIContent* aChild, PRInt32 aIndex);
  static NSMenu* LocateMenu(nsIContent* aContent);

  static void ConstructAddBookmarkFolderList(NSPopUpButton* aPopup, BookmarkItem* aItem);
  
  static void EnsureToolbarRoot();

  static void ImportBookmarks(nsIDOMHTMLDocument* aHTMLDoc);
  
  static void GetTitleAndHrefForBrowserView(id aBrowserView, nsString& aTitle, nsString& aHref);
  static void OpenBookmarkGroup(id aTabView, nsIDOMElement* aFolder);

  static NSImage* CreateIconForBookmark(nsIDOMElement* aElement);
  
  static void DragBookmark(nsIDOMElement* aElement, NSView* aView, NSEvent* aEvent);
  static void CompleteBookmarkDrag(NSPasteboard* aPasteboard, nsIDOMElement* aFolderElt, nsIDOMElement* aBeforeElt, int aPosition);
  
  
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
  static nsIAtom* gBookmarkAtom;
  static nsIDocument* gBookmarks;
  static nsVoidArray* gInstances;
  static int CHInsertNone;
  static int CHInsertInto;
  static int CHInsertBefore;
  static int CHInsertAfter;
  
private:
  // There are three kinds of bookmarks data sources:
  // tree views (mDataSource), the personal toolbar (mToolbar)
  // and menus (gBookmarksMenu).
  CHBookmarksToolbar* mToolbar;
  BookmarksDataSource* mDataSource;
};
