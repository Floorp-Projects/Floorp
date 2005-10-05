/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   David Haas <haasd@cae.wisc.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import "BookmarkItem.h"
#import "BookmarksClient.h"

//Special flags
enum {
  kBookmarkFolder         = 0,
  kBookmarkFolderGroup    = 1 << 0,
  kBookmarkRootFolder     = 1 << 1,
  kBookmarkToolbarFolder  = 1 << 2,
  kBookmarkSmartFolder    = 1 << 3,
  kBookmarkDockMenuFolder = 1 << 4
};


//root AE code: DB14
@class Bookmark;

@interface BookmarkFolder : BookmarkItem  //AE code: DBAE
{
  NSMutableArray* mChildArray;
  unsigned int    mSpecialFlag;
  NSString*       mIdentifier;    // only non-nil for "special" collection folders. not saved (yet)
}

-(id) init;   // designated initializer
-(id) initWithIdentifier:(NSString*)inIdentifier; // will get used for special folders
-(NSMutableArray *) childArray;
-(NSArray *) childURLs;
-(NSArray *) allChildBookmarks;

// enumerator for this folder and all its children (in depth-first order). not safe under
// tree changes during enumeration
-(NSEnumerator*)objectEnumerator;

-(void)setIdentifier:(NSString*)inIdentifier;
-(NSString*)identifier;

-(BOOL) isSpecial;
-(BOOL) isToolbar;
-(BOOL) isRoot;
-(BOOL) isGroup;
-(BOOL) isSmartFolder;
-(BOOL) isDockMenu;

-(void) setChildArray:(NSMutableArray *)aChildArray; //should be private?
-(void)	setIsGroup:(BOOL)aGroupFlag;    //AE code: DBAg
-(void) setIsRoot:(BOOL)aFlag;
-(void) setIsToolbar:(BOOL)aFlag;
-(void) setIsSmartFolder:(BOOL)aFlag;
-(void) setIsDockMenu:(BOOL)aFlag;
-(void) makeDockMenu:(id)sender;

// Things added to make it work sort of like an array
-(unsigned) count;
-(id) objectAtIndex:(unsigned)index;
-(unsigned)indexOfObject:(id)object;
-(unsigned)indexOfObjectIdenticalTo:(id)object;

// methods used for saving to files; are guaranteed never to return nil
- (id)savedSpecialFlag;

// ways to add a new bookmark
-(Bookmark *) addBookmark; //adds to end
-(Bookmark *) addBookmark:(NSString *)aTitle url:(NSString *)aURL inPosition:(unsigned)aIndex isSeparator:(BOOL)aBool;
-(Bookmark *) addBookmark:(NSString *)aTitle inPosition:(unsigned)aIndex keyword:(NSString *)aKeyword url:(NSString *)aURL description:(NSString *)aDescription lastVisit:(NSDate *)aDate  status:(unsigned)aStatus isSeparator:(BOOL)aBool;

// ways to add a new bookmark array
-(BookmarkFolder *) addBookmarkFolder; //adds to end
-(BookmarkFolder *) addBookmarkFolder:(NSString *)aTitle inPosition:(unsigned)aIndex isGroup:(BOOL)aFlag;

// finding items by uuid
-(BookmarkItem *)itemWithUUID:(NSString*)uuid;

// Moving & Copying & inserting bookmarks/bookmark arrays
-(void) appendChild:(BookmarkItem *)aChild;
-(void) insertChild:(BookmarkItem *)aChild atIndex:(unsigned)aIndex isMove:(BOOL)aBool;
-(void) moveChild:(BookmarkItem *)aChild toBookmarkFolder:(BookmarkFolder *)aNewParent atIndex:(unsigned)aIndex;
// returns the new child
-(BookmarkItem*) copyChild:(BookmarkItem *)aChild toBookmarkFolder:(BookmarkFolder *)aNewParent atIndex:(unsigned)aIndex;

// Used for deleting bookmarks/bookmark arrays
-(BOOL) deleteChild:(BookmarkItem *)aChild;

// Smart Folder only methods
-(void) insertIntoSmartFolderChild:(BookmarkItem *)aItem;
-(void) deleteFromSmartFolderChildAtIndex:(unsigned)index;

// generation menus
-(void) buildFlatFolderList:(NSMenu *)menu depth:(unsigned)pad;

// searching
-(NSArray*)resolveKeyword:(NSString *)keyword withArgs:(NSString *)args;
-(NSSet *) bookmarksWithString:(NSString *)searchString inFieldWithTag:(int)tag;
- (BOOL)containsChildItem:(BookmarkItem*)inItem;

// Scripting - should be a protocol we could use for these
// two, but i'm not sure which one, so we'll declare them here
// and avoid the compiler warning
-(NSArray *) indicesOfObjectsByEvaluatingRelativeSpecifier:(NSRelativeSpecifier *)relSpec;
-(NSArray *) indicesOfObjectsByEvaluatingRangeSpecifier:(NSRangeSpecifier *)rangeSpec;

@end
