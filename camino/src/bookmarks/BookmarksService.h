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


class nsIAtom;
class nsIContent;
class nsIDOMElement;
class nsIDOMHTMLDocument;

@class BookmarksDataSource;
@class BookmarksToolbar;
@class BookmarkItem;
@class BrowserWindowController;

typedef enum
{
  eBookmarksFolderNormal,		// any folder
  eBookmarksFolderRoot,
  eBookmarksFolderToolbar,
  eBookmarksFolderDockMenu
  
} EBookmarksFolderType;


typedef enum
{
  eBookmarkItemBookmark = 1,
  eBookmarkItemSeparator,		// not supported yet
  eBookmarkItemFolder,
  eBookmarkItemGroup
} EBookmarkItemType;


@protocol BookmarksClient

- (void)bookmarkAdded:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot;
- (void)bookmarkRemoved:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot;
- (void)bookmarkChanged:(nsIContent*)bookmark;

- (void)specialFolder:(EBookmarksFolderType)folderType changedTo:(nsIContent*)newFolderContent;

@end

class BookmarksService
{
public:

  static void Init();			// reads bookmarks
  static void Shutdown();
    
  static void GetRootContent(nsIContent** aResult);
  static BookmarkItem* GetRootItem();
  static BookmarkItem* GetWrapperFor(nsIContent* aItem);
  static BookmarkItem* GetWrapperFor(PRUint32 contentID);
  
  static void ReadBookmarks();
  static void FlushBookmarks();

  static nsresult ExportBookmarksToHTML(const nsAString& inFilePath);
  
  
  static void SetDockMenuRoot(nsIContent* inDockRootContent);
  static void SetToolbarRoot(nsIContent* inToolbarRootContent);
    
  static NSImage* CreateIconForBookmark(nsIDOMElement* aElement, PRBool useSiteIcon = PR_FALSE);

  static void ImportBookmarks(nsIDOMDocument* aDoc);
  
  static bool GetContentForKeyword(NSString* aKeyword, nsIContent** outFoundContent);

  static bool IsBookmarkDropValid(BookmarkItem* proposedParent, int index, NSArray* draggedIDs, bool isCopy);
  static bool PerformBookmarkDrop(BookmarkItem* parent, BookmarkItem* beforeItem, int index, NSArray* draggedIDs, bool doCopy);
  static bool PerformProxyDrop(BookmarkItem* parentItem, BookmarkItem* beforeItem, NSDictionary* data);
  
  static bool PerformURLDrop(BookmarkItem* parentItem, BookmarkItem* beforeItem, NSString* title, NSString* url);

public: 	/* but not to be used outside of BoomarksService.cpp */

  static void BookmarkAdded(nsIContent* aContainer, nsIContent* aChild, bool shouldFlush = true);
  static void BookmarkRemoved(nsIContent* aContainer, nsIContent* aChild, bool shouldFlush = true);
  static void BookmarkChanged(nsIContent* aItem, bool shouldFlush = true);

  static void DeleteBookmark(nsIDOMElement* aBookmark);

protected:

  static void NotifyBookmarkAdded(nsIContent* aContainer, nsIContent* aChild, PRBool aChangeRoot);
  static void NotifyBookmarkRemoved(nsIContent* aContainer, nsIContent* aChild, PRBool aChangeRoot);

  static void SpecialBookmarkChanged(EBookmarksFolderType aFolderType, nsIContent* aNewContent);

  static void AddBookmarkToFolder(const nsString& aURL, const nsString& aTitle, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt);
  static void MoveBookmarkToFolder(nsIDOMElement* aBookmark, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt);

  static nsresult SaveBookmarksToFile(const nsAString& inFileName);

  static bool FindFolderWithAttribute(const nsAString& inAttribute, const nsAString& inValue, nsIDOMElement** foundElt);
  static bool DoAncestorsIncludeNode(BookmarkItem* bookmark, BookmarkItem* searchItem);

  static void EnsureToolbarRoot();

public:

  static nsIDOMElement* gToolbarRoot;
  static nsIDOMElement* gDockMenuRoot;

  static nsIAtom* gFolderAtom;
  static nsIAtom* gSeparatorAtom;
  static nsIAtom* gNameAtom;
  static nsIAtom* gHrefAtom;
  static nsIAtom* gKeywordAtom;
  static nsIAtom* gDescriptionAtom;
  static nsIAtom* gBookmarkAtom;
  static nsIAtom* gOpenAtom;
  static nsIAtom* gGroupAtom;
  
  static int CHInsertNone;
  static int CHInsertInto;
  static int CHInsertBefore;
  static int CHInsertAfter;  

protected:

  static BOOL     gBookmarksFileReadOK;

};



@interface BookmarkItem : NSObject
{
  nsIContent* mContentNode;
  NSImage*    mSiteIcon;
}

- (id)copyWithZone:(NSZone *)aZone;

- (nsIContent*)contentNode;		// does *not* addref (inconsistent)
- (void)setContentNode: (nsIContent*)aContentNode;
- (void)setSiteIcon:(NSImage*)image;

- (void)itemChanged:(BOOL)flushBookmarks;
- (void)remove;

- (NSString*)name;
- (NSString*)url;
- (NSString*)keyword;
- (NSString*)descriptionString;

- (NSImage*)siteIcon;
- (NSNumber*)contentID;
- (int)intContentID;
- (BOOL)isFolder;
- (BOOL)isGroup;

- (BOOL)isToobarRoot;
- (BOOL)isDockMenuRoot;

- (BookmarkItem*)parentItem;

@end


// singleton bookmarks manager object

@interface BookmarksManager : NSObject<BookmarksClient>
{
  BOOL  mUseSiteIcons;		// only affects the bookmarks toolbar now
}

+ (BookmarksManager*)sharedBookmarksManager;
+ (BookmarksManager*)sharedBookmarksManagerDontAlloc;

- (void)addBookmarksClient:(id<BookmarksClient>)client;
- (void)removeBookmarksClient:(id<BookmarksClient>)client;

- (BookmarkItem*)getWrapperForContent:(nsIContent*)item;
- (BookmarkItem*)getWrapperForID:(int)contentID;
- (BookmarkItem*)getWrapperForNumber:(NSNumber*)contentIDNum;

- (nsIContent*)getRootContent;		// addrefs return value
- (nsIContent*)getToolbarRoot;		// addrefs return value
- (nsIContent*)getDockMenuRoot;		// addrefs return value

- (nsIDOMDocument*)getBookmarksDocument;	// addrefs

- (NSArray*)getBookmarkGroupURIs:(BookmarkItem*)item;

// returns an array of strings (usually just one, except for tab groups with keywords)
- (NSArray*)resolveBookmarksKeyword:(NSString*)locationString;

- (void)addNewBookmark:(NSString*)url title:(NSString*)title withParent:(nsIContent*)parent;
- (void)addNewBookmarkFolder:(NSString*)title withParent:(nsIContent*)parent;

// itemsArray is an array of NSDictionaries, with "href" and "title" entries
- (void)addNewBookmarkGroup:(NSString*)titleString items:(NSArray*)itemsArray withParent:(nsIContent*)parent;

- (void)loadProxyImageFor:(id)requestor withURI:(NSString*)inURIString;
- (void)updateProxyImage:(NSImage*)image forSiteIcon:(NSString*)inSiteIconURI;

- (void)buildFlatFolderList:(NSMenu*)menu fromRoot:(nsIContent*)rootContent;

- (BOOL)useSiteIcons;

@end


