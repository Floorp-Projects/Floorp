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

#import "NSString+Utils.h"
#import "NSArray+Utils.h"

#import "BookmarkManager.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"

// Notification definitions
NSString* const BookmarkFolderAdditionNotification      = @"bf_add";
NSString* const BookmarkFolderDeletionNotification      = @"bf_del";
NSString* const BookmarkFolderChildKey                  = @"bf_ck";
NSString* const BookmarkFolderChildIndexKey             = @"bf_ik";
NSString* const BookmarkFolderDockMenuChangeNotificaton = @"bf_dmc";


struct BookmarkSortData
{
  SEL       mSortSelector;
  NSNumber* mReverseSort;

  SEL       mSecondarySortSelector;
  NSNumber* mSecondaryReverseSort;
};


static int BookmarkItemSort(id firstItem, id secondItem, void* context)
{
  BookmarkSortData* sortData = (BookmarkSortData*)context;
  int comp = (int)[firstItem performSelector:sortData->mSortSelector withObject:secondItem withObject:sortData->mReverseSort];

  if (comp == 0 && sortData->mSecondarySortSelector)
    comp = (int)[firstItem performSelector:sortData->mSecondarySortSelector withObject:secondItem withObject:sortData->mSecondaryReverseSort];

  return comp;
}


@interface BookmarksEnumerator : NSEnumerator
{
  BookmarkFolder*   mRootFolder;
  
  BookmarkFolder*   mCurFolder;
  int               mCurChildIndex;   // -1 means "self"
}

- (id)initWithRootFolder:(BookmarkFolder*)rootFolder;

- (id)nextObject;
- (NSArray *)allObjects;

@end;

#pragma mark -


@interface BookmarkFolder (Private)

// status stuff
-(unsigned) specialFlag;
-(void) setSpecialFlag:(unsigned)aNumber;

// ways to add a new bookmark
-(BOOL) addBookmarkFromNativeDict:(NSDictionary *)aDict;
-(BOOL) addBookmarkFromSafariDict:(NSDictionary *)aDict;
-(BOOL) addBookmarkFromXML:(CFXMLTreeRef)aTreeRef;

// ways to add a new bookmark folder
-(BOOL) addBookmarkFolderFromNativeDict:(NSDictionary *)aDict; //read in - adds sequentially
-(BOOL) addBookmarkFolderFromSafariDict:(NSDictionary *)aDict;
-(BOOL) addBookmarkFolderFromXML:(CFXMLTreeRef)aTreeRef settingToolbar:(BOOL)setupToolbar;

// deletes the bookmark or bookmark array
-(BOOL) deleteBookmark:(Bookmark *)childBookmark;
-(BOOL) deleteBookmarkFolder:(BookmarkFolder *)childArray;

// notification methods
-(void) itemAddedNote:(BookmarkItem *)theItem atIndex:(unsigned)anIndex;
-(void) itemRemovedNote:(BookmarkItem *)theItem;
-(void) itemChangedNote:(BookmarkItem *)theItem;
-(void) dockMenuChanged:(NSNotification *)note;

// aids in searching
- (NSString*)expandURL:(NSString*)url withString:(NSString*)searchString;
- (NSArray *)folderItemsWithClass:(Class)theClass;

// used for undo
- (void)arrangeChildrenWithOrder:(NSArray*)inChildArray;

@end

#pragma mark -

@implementation BookmarksEnumerator

- (id)initWithRootFolder:(BookmarkFolder*)rootFolder
{
  if ((self = [super init]))
  {
    mRootFolder = [rootFolder retain];
    mCurFolder = mRootFolder;
    mCurChildIndex = -1;
  }
  return self;
}

- (void)dealloc
{
  [mRootFolder release];
  [super dealloc];
}

- (void)setupNextForItem:(BookmarkItem*)inItem
{
  if ([inItem isKindOfClass:[BookmarkFolder class]])
  {
    mCurFolder = (BookmarkFolder*)inItem;
    mCurChildIndex = 0;   // start on its children next time
  }
  else
    ++mCurChildIndex;
}

- (id)nextObject
{
  if (!mCurFolder) return nil;    // if we're done, keep it that way
  
  if (mCurChildIndex == -1)
  {
    [self setupNextForItem:mCurFolder];
    return mCurFolder;
  }
  
  NSArray* curFolderChildren = [mCurFolder childArray];
  BookmarkItem* curChild = [curFolderChildren safeObjectAtIndex:mCurChildIndex];
  if (curChild)
  {
    [self setupNextForItem:curChild];
    return curChild;
  }
  
  // we're at the end of this folder.
  while (1)
  {
    if (mCurFolder == mRootFolder)  // we're done
    {
      mCurFolder = nil;
      break;
    }

    // back up to parent folder, next index
    BookmarkFolder* newCurFolder = [mCurFolder parent];
    mCurChildIndex = [newCurFolder indexOfObject:mCurFolder] + 1;
    mCurFolder = newCurFolder;
    if (mCurChildIndex < (int)[newCurFolder count])
    {
      BookmarkItem* nextItem = [mCurFolder objectAtIndex:mCurChildIndex];
      [self setupNextForItem:nextItem];
      return nextItem;
    }
  }
  
  return nil;
}

- (NSArray*)allObjects
{
  NSMutableArray* allObjectsArray = [NSMutableArray array];

  BookmarksEnumerator* bmEnum = [[[BookmarksEnumerator alloc] initWithRootFolder:mRootFolder] autorelease];
  id curItem;
  while ((curItem = [bmEnum nextObject]))
    [allObjectsArray addObject:curItem];
  
  return allObjectsArray;
}

@end

#pragma mark -

@implementation BookmarkFolder

-(id) init
{
  if ((self = [super init]))
  {
    mChildArray   = [[NSMutableArray alloc] init];
    mSpecialFlag  = kBookmarkFolder;
    mIcon         = nil;    // load lazily
  }
  return self;
}

-(id) initWithIdentifier:(NSString*)inIdentifier
{
  if ([self init])
  {
    mIdentifier = [inIdentifier retain];
  }
  return self;
}

-(id) copyWithZone:(NSZone *)zone
{
  if ([self isSmartFolder])
    return nil;

  id folderCopy = [super copyWithZone:zone];
  unsigned folderFlags = ([self specialFlag] & ~kBookmarkDockMenuFolder);   // don't copy dock menu flag
  [folderCopy setSpecialFlag:folderFlags];
  // don't copy the identifier, since it should be unique
  
  NSEnumerator *enumerator = [[self childArray] objectEnumerator];
  id anItem, aCopiedItem;
  // head fake the undomanager
  NSUndoManager *undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  [undoManager disableUndoRegistration];
  while ((anItem = [enumerator nextObject])) {
    aCopiedItem = [anItem copyWithZone:[anItem zone]];
    [folderCopy appendChild:aCopiedItem];
    [aCopiedItem release];
  }
  [undoManager enableUndoRegistration];

  return folderCopy;
}

-(void)dealloc
{
  // NOTE: we're no longer clearning the undo stack here because its pointless and is a potential crasher
  if (mSpecialFlag & kBookmarkDockMenuFolder)
    [[NSNotificationCenter defaultCenter] removeObserver:self];

  // set our all children to have a null parent.
  // important if they've got timers running.
  NSEnumerator *enumerator = [mChildArray objectEnumerator];
  id kid;
  while ((kid = [enumerator nextObject]))
    [kid setParent:nil];

  [mChildArray release];
  [mIdentifier release];

  [super dealloc];
}

- (NSString*)description
{
  return [NSString stringWithFormat:@"BookmarkFolder %08p, title %@", self, mTitle];
}

-(NSImage *)icon
{
  if (!mIcon)
    mIcon = [[NSImage imageNamed:@"folder"] retain];

  return mIcon;
}

//
// get/set properties
//

-(NSMutableArray *) childArray
{
  return mChildArray;
}

// only return bookmark URL's - if we've got folders, we ignore them.
-(NSArray *)childURLs
{
  NSMutableArray *urlArray = [NSMutableArray arrayWithCapacity:[self count]];
  NSEnumerator *enumerator = [[self childArray] objectEnumerator];
  id anItem;
  while ((anItem = [enumerator nextObject])) {
    if ([anItem isKindOfClass:[Bookmark class]] && ![(Bookmark *)anItem isSeparator])
      [urlArray addObject:[(Bookmark *)anItem url]];
  }
  return urlArray;
}

-(NSArray *) allChildBookmarks
{
  NSMutableArray *anArray = [NSMutableArray array];
  [anArray addObjectsFromArray:[self folderItemsWithClass:[Bookmark class]]];
  NSEnumerator *enumerator = [[self folderItemsWithClass:[BookmarkFolder class]] objectEnumerator];
  BookmarkFolder *kid;
  while ((kid = [enumerator nextObject])) {
    NSArray *kidsBookmarks = [kid allChildBookmarks];
    [anArray addObjectsFromArray:kidsBookmarks];
  }
  return anArray;
}

-(NSEnumerator*)objectEnumerator
{
  return [[[BookmarksEnumerator alloc] initWithRootFolder:self] autorelease];
}

-(void)setIdentifier:(NSString*)inIdentifier
{
  [mIdentifier autorelease];
  mIdentifier = [inIdentifier retain];
}

-(NSString*)identifier
{
  return mIdentifier;
}

-(BOOL) isSpecial
{
  return ([self isToolbar] || [self isRoot] || [self isSmartFolder] || [self isDockMenu]);
}

-(BOOL) isToolbar
{
  return ((mSpecialFlag & kBookmarkToolbarFolder) != 0);
}

-(BOOL) isRoot
{
  return ((mSpecialFlag & kBookmarkRootFolder) != 0);
}

-(BOOL) isGroup
{
  return ((mSpecialFlag & kBookmarkFolderGroup) != 0);
}

-(BOOL) isSmartFolder
{
  return ((mSpecialFlag & kBookmarkSmartFolder) != 0);
}

-(BOOL) isDockMenu
{
  return ((mSpecialFlag & kBookmarkDockMenuFolder) != 0);
}

-(unsigned) specialFlag
{
  return mSpecialFlag;
}


// world of hurt if you use this incorrectly.
-(void) setChildArray:(NSMutableArray *)aChildArray
{
  if (aChildArray != mChildArray) {
    [aChildArray retain];
    [mChildArray release]; //trashes everything the bookmark contained
    mChildArray = aChildArray;
  }
}

-(void) setIsGroup:(BOOL)aBool
{
  unsigned curVal = [self specialFlag];
  if (aBool) 
    curVal |= kBookmarkFolderGroup;
  else 
    curVal &= ~kBookmarkFolderGroup;
  [self setSpecialFlag:curVal];
}

-(void) setSpecialFlag:(unsigned)aFlag
{
  unsigned int oldFlag = mSpecialFlag;
  mSpecialFlag = aFlag;

  if ((oldFlag & kBookmarkFolderGroup) != (aFlag & kBookmarkFolderGroup)) {
    // only change the group/folder icon if we're changing that flag
    if ((aFlag & kBookmarkFolderGroup) != 0)
      [self setIcon:[NSImage imageNamed:@"groupbookmark"]];
    else
      [self setIcon:[NSImage imageNamed:@"folder"]];
  }
  if ((aFlag & kBookmarkDockMenuFolder) != 0) {
    // if we're the dock menu folder, tell the previous dock folder that 
    // they're no longer it and register for the same notification in
    // case it changes again.
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc postNotificationName:BookmarkFolderDockMenuChangeNotificaton object:self];
    [nc addObserver:self selector:@selector(dockMenuChanged:) name:BookmarkFolderDockMenuChangeNotificaton object:nil];
  }
}

-(void) setIsToolbar:(BOOL)aBool
{
  unsigned curVal = [self specialFlag];
  if (aBool) 
    curVal |= kBookmarkToolbarFolder;
  else
    curVal &= ~kBookmarkToolbarFolder;
  [self setSpecialFlag:curVal];
}

-(void) setIsRoot:(BOOL)aBool
{
  unsigned curVal = [self specialFlag];
  if (aBool) {
    if ([[self parent] isKindOfClass:[BookmarkFolder class]])
      return;
    curVal |= kBookmarkRootFolder;
  } else
    curVal &= ~kBookmarkRootFolder;
  [self setSpecialFlag:curVal];
}

-(void) setIsSmartFolder:(BOOL)aBool
{
  unsigned curVal = [self specialFlag];
  if (aBool) 
    curVal |= kBookmarkSmartFolder;
  else
    curVal &= ~kBookmarkSmartFolder;
  [self setSpecialFlag:curVal];
}

-(void) makeDockMenu:(id)sender
{
  [self setIsDockMenu:YES];
}

-(void) setIsDockMenu:(BOOL)aBool
{
  unsigned curVal = [self specialFlag];
  if (aBool) 
    curVal |= kBookmarkDockMenuFolder;
  else {
    curVal &= ~kBookmarkDockMenuFolder;
    [[NSNotificationCenter defaultCenter] removeObserver:self name:BookmarkFolderDockMenuChangeNotificaton object:nil];
  }
  [self setSpecialFlag:curVal];
}

//
// Adding children.
//
-(void) appendChild:(BookmarkItem *) aChild
{
  [self insertChild:aChild atIndex:[self count] isMove:NO];
}

-(void) insertChild:(BookmarkItem *)aChild atIndex:(unsigned)aPosition isMove:(BOOL)inIsMove
{
  [aChild setParent:self];
  unsigned insertPoint = [mChildArray count];
  if (insertPoint > aPosition)
    insertPoint = aPosition;

  [mChildArray insertObject:aChild atIndex:insertPoint];
  if (!inIsMove && ![self isSmartFolder])
  {
    NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
    [[undoManager prepareWithInvocationTarget:self] deleteChild:aChild];
    if (![undoManager isUndoing]) {
      if ([aChild isKindOfClass:[BookmarkFolder class]])
        [undoManager setActionName:NSLocalizedString(@"Add Folder", @"")];
      else if ([aChild isKindOfClass:[Bookmark class]]) {
        if (![(Bookmark  *)aChild isSeparator])
          [undoManager setActionName:NSLocalizedString(@"Add Bookmark", @"")];
        else
          [undoManager setActionName:NSLocalizedString(@"Add Separator", @"")];
      }
    } else {
      [aChild itemUpdatedNote:kBookmarkItemEverythingChangedMask];   // ??
    }
  }
  [self itemAddedNote:aChild atIndex:insertPoint];
}

//
// Smart folder utilities - we don't set ourself as parent
//
- (void)insertIntoSmartFolderChild:(BookmarkItem *)aItem
{
  if ([self isSmartFolder]) {
    [[self childArray] addObject:aItem];
    [self itemAddedNote:aItem atIndex:([self count] - 1)];
  }
}

- (void)insertIntoSmartFolderChild:(BookmarkItem *)aItem atIndex:(unsigned)inIndex
{
  if ([self isSmartFolder]) {
    [[self childArray] insertObject:aItem atIndex:inIndex];
    [self itemAddedNote:aItem atIndex:inIndex];
  }
}

- (void)deleteFromSmartFolderChildAtIndex:(unsigned)index
{
  if ([self isSmartFolder]) {
    BookmarkItem *item = [[[self childArray] objectAtIndex:index] retain];
    [[self childArray] removeObjectAtIndex:index];
    [self itemRemovedNote:[item autorelease]];
  }   
}

// move the given items together, sorted according to the sort selector.
// inChildItems is assumed to be in the same order as the children
- (void)arrangeChildItems:(NSArray*)inChildItems usingSelector:(SEL)inSelector reverseSort:(BOOL)inReverse
{
  NSMutableArray* orderedItems = [NSMutableArray arrayWithArray:inChildItems];

  BookmarkSortData sortData = { inSelector, [NSNumber numberWithBool:inReverse], NULL, nil };
  [orderedItems sortUsingFunction:BookmarkItemSort context:&sortData];
  
  // now build a new child array
  NSMutableArray* newChildOrder = [[mChildArray mutableCopy] autorelease];
  
  // save the index of the first (which won't change, since it's the first)
  int firstChangedIndex = [newChildOrder indexOfObject:[inChildItems firstObject]];
  if (firstChangedIndex == -1) return;    // something went wrong
  
  [newChildOrder removeObjectsInArray:inChildItems];
  [newChildOrder replaceObjectsInRange:NSMakeRange(firstChangedIndex, 0) withObjectsFromArray:orderedItems];
  
  // this makes it undoable
  [self arrangeChildrenWithOrder:newChildOrder];
}

- (void)sortChildrenUsingSelector:(SEL)inSelector reverseSort:(BOOL)inReverse sortDeep:(BOOL)inDeep undoable:(BOOL)inUndoable
{
  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  if (inUndoable)
  {
    [undoManager beginUndoGrouping];
    // record undo, back to existing order
    [[undoManager prepareWithInvocationTarget:self] arrangeChildrenWithOrder:[NSArray arrayWithArray:mChildArray]];
  }
  
  BookmarkSortData sortData = { inSelector, [NSNumber numberWithBool:inReverse], NULL, nil };
  [mChildArray sortUsingFunction:BookmarkItemSort context:&sortData];
  
  // notify
  [self itemChangedNote:self];
  
  if (inDeep)
  {
    NSEnumerator *enumerator = [mChildArray objectEnumerator];
    id childItem;
    while ((childItem = [enumerator nextObject]))
    {
      if ([childItem isKindOfClass:[BookmarkFolder class]])
        [childItem sortChildrenUsingSelector:inSelector reverseSort:inReverse sortDeep:inDeep undoable:inUndoable];
    }
  }

  if (inUndoable)
  {
    [undoManager endUndoGrouping];
    [undoManager setActionName:NSLocalizedString(@"Arrange Bookmarks", @"")];
  }
}

// used for undo
- (void)arrangeChildrenWithOrder:(NSArray*)inChildArray
{
  // The undo manager automatically puts redo items in a group, so we don't have to
  // (undoing sorting of nested folders will work OK)
  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];

  // record undo, back to existing order
  [[undoManager prepareWithInvocationTarget:self] arrangeChildrenWithOrder:[NSArray arrayWithArray:mChildArray]];

  if ([mChildArray count] != [inChildArray count])
  {
    NSLog(@"arrangeChildrenWithOrder: unmatched child array sizes");
    return;
  }

  // Cheating here. The in array simply contains the items in the order we want,
  // so use it.
  [mChildArray removeAllObjects];
  [mChildArray addObjectsFromArray:inChildArray];

  // notify
  [self itemChangedNote:self];

  [undoManager setActionName:NSLocalizedString(@"Arrange Bookmarks", @"")];
}

//
// Adding bookmarks
//

// XXX fix to nicely autorelease etc.
-(Bookmark *)addBookmark
{
  if (![self isRoot]) {
    Bookmark* theBookmark = [[Bookmark alloc] init];
    [self appendChild:theBookmark];
    [theBookmark release]; //retained on insert
    return theBookmark;
  }
  return nil;
}

// adding from native plist
-(BOOL) addBookmarkFromNativeDict:(NSDictionary *)aDict
{
  return [[self addBookmark] readNativeDictionary:aDict];
}

-(BOOL) addBookmarkFromSafariDict:(NSDictionary *)aDict
{
  return [[self addBookmark] readSafariDictionary:aDict];
}

// add from camino xml
-(BOOL) addBookmarkFromXML:(CFXMLTreeRef)aTreeRef
{
  return [[self addBookmark] readCaminoXML:aTreeRef settingToolbar:NO];
}

-(Bookmark *) addBookmark:(NSString *)aTitle url:(NSString *)aURL inPosition:(unsigned)aIndex isSeparator:(BOOL)aBool
{
  if ([aTitle length] == 0)
    aTitle = aURL;
  return [self addBookmark:aTitle inPosition:aIndex keyword:@"" url:aURL description:@"" lastVisit:[NSDate date] status:kBookmarkOKStatus isSeparator:aBool];
}

// full bodied addition
-(Bookmark *) addBookmark:(NSString *)aTitle inPosition:(unsigned)aPosition keyword:(NSString *)aKeyword url:(NSString *)aURL description:(NSString *)aDescription lastVisit:(NSDate *)aDate status:(unsigned)aStatus isSeparator:(BOOL)aSeparator
{
  if (![self isRoot]) {
    Bookmark *theBookmark = [[Bookmark alloc] init];
    [theBookmark setTitle:aTitle];
    [theBookmark setKeyword:aKeyword];
    [theBookmark setUrl:aURL];
    [theBookmark setItemDescription:aDescription];
    [theBookmark setLastVisit:aDate];
    [theBookmark setStatus:aStatus];
    [theBookmark setIsSeparator:aSeparator];
    [self insertChild:theBookmark atIndex:aPosition isMove:NO];
    [theBookmark release];
    return theBookmark;
  }
  return nil;
}

//
// Adding arrays
//
// used primarily for parsing of html bookmark files.
-(BookmarkFolder *)addBookmarkFolder // adds to end
{
  BookmarkFolder *theFolder = [[BookmarkFolder alloc] init];
  [self appendChild:theFolder];
  [theFolder release]; // retained on insert
  return theFolder;
}

// from native plist file
-(BOOL) addBookmarkFolderFromNativeDict:(NSDictionary *)aDict
{
  return [[self addBookmarkFolder] readNativeDictionary:aDict];
}

-(BOOL) addBookmarkFolderFromSafariDict:(NSDictionary *)aDict
{
  return [[self addBookmarkFolder] readSafariDictionary:aDict];
}

-(BOOL) addBookmarkFolderFromXML:(CFXMLTreeRef)aTreeRef settingToolbar:(BOOL)setupToolbar
{
  return [[self addBookmarkFolder] readCaminoXML:aTreeRef settingToolbar:setupToolbar];
}

// normal add while programming running
-(BookmarkFolder *)addBookmarkFolder:(NSString *)aName inPosition:(unsigned)aPosition isGroup:(BOOL)aFlag
{
  BookmarkFolder *theFolder = [[[BookmarkFolder alloc] init] autorelease];
  [theFolder setTitle:aName];
  if (aFlag)
    [theFolder setIsGroup:aFlag];
  [self insertChild:theFolder atIndex:aPosition isMove:NO];
  return theFolder;
}

// if we start spending too much time here, we could build an NSDictionary map
-(BookmarkItem *)itemWithUUID:(NSString*)uuid
{
  // see if we match
  if ([[self UUID] isEqualToString:uuid])
    return self;

  // see if our kids match
  NSEnumerator *enumerator = [[self childArray] objectEnumerator];
  id childItem;
  while ((childItem = [enumerator nextObject]))
  {
    if ([childItem isKindOfClass:[Bookmark class]])
    {
      if ([[childItem UUID] isEqualToString:uuid])
        return childItem;
    }
    else if ([childItem isKindOfClass:[BookmarkFolder class]])
    {
      // recurse
      id foundItem = [childItem itemWithUUID:uuid];
      if (foundItem)
        return foundItem;
    }
  }

  return nil;
}

//
// Moving & Copying children
//

-(void) moveChild:(BookmarkItem *)aChild toBookmarkFolder:(BookmarkFolder *)aNewParent atIndex:(unsigned)aIndex
{
  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  unsigned curIndex = [mChildArray indexOfObjectIdenticalTo:aChild];
  if (curIndex == NSNotFound)
    return;

  BOOL isSeparator = NO;
  // A few of sanity checks
  if ([aChild isKindOfClass:[BookmarkFolder class]])
  {
    if ([aNewParent isChildOfItem:aChild])
      return;
    
    if ([(BookmarkFolder *)aChild isToolbar] && ![aNewParent isRoot])
      return;
    
    if ([(BookmarkFolder *)aChild isSmartFolder] && ![aNewParent isRoot])
      return;
  }
  else if ([aChild isKindOfClass:[Bookmark class]])
  {
    if ([aNewParent isRoot])
      return;
    isSeparator = [(Bookmark *)aChild isSeparator];
  }

  [undoManager beginUndoGrouping];

  // What we do depends on if we're moving into a new folder, or just
  // rearranging ourself a bit.
  if (aNewParent != self)
  {
    [aNewParent insertChild:aChild atIndex:aIndex isMove:YES];
    // DO NOT call deleteChild here.  Just quietly remove it from the array.
    [mChildArray removeObjectAtIndex:curIndex];
    [self itemRemovedNote:aChild];
  }
  else
  {
    [aChild retain];
    [mChildArray removeObjectAtIndex:curIndex];
    [self itemRemovedNote:aChild];
    if (curIndex < aIndex)
      --aIndex;
    else
      ++curIndex; // so that redo works

    [self insertChild:aChild atIndex:aIndex isMove:YES];
    [aChild release];
  }

  [[undoManager prepareWithInvocationTarget:aNewParent] moveChild:aChild toBookmarkFolder:self atIndex:curIndex];
  [undoManager endUndoGrouping];

  if ([aChild isKindOfClass:[BookmarkFolder class]])
    [undoManager setActionName:NSLocalizedString(@"Move Folder", @"")];
  else if (isSeparator)
    [undoManager setActionName:NSLocalizedString(@"Move Separator", @"")];
  else
    [undoManager setActionName:NSLocalizedString(@"Move Bookmark", @"")];
}

-(BookmarkItem*) copyChild:(BookmarkItem *)aChild toBookmarkFolder:(BookmarkFolder *)aNewParent atIndex:(unsigned)aIndex
{
  if ([aNewParent isRoot] && [aChild isKindOfClass:[Bookmark class]])
    return nil;

  BookmarkItem *copiedChild = [aChild copyWithZone:nil];
  if (!copiedChild)
    return nil;

  [aNewParent insertChild:copiedChild atIndex:aIndex isMove:NO];
  [copiedChild release];

  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  if ([aChild isKindOfClass:[BookmarkFolder class]])
    [undoManager setActionName:NSLocalizedString(@"Copy Folder", @"")];
  else
    [undoManager setActionName:NSLocalizedString(@"Copy Bookmark", @"")];

  return copiedChild;
}

//
// Deleting children
//
-(BOOL) deleteChild:(BookmarkItem *)aChild
{
  BOOL itsDead = NO;
  if ([[self childArray] indexOfObjectIdenticalTo:aChild] != NSNotFound)
  {
    if ([aChild isKindOfClass:[Bookmark class]])
      itsDead = [self deleteBookmark:(Bookmark *)aChild];
    else if ([aChild isKindOfClass:[BookmarkFolder class]])
      itsDead = [self deleteBookmarkFolder:(BookmarkFolder *)aChild];
  }
  else
  {
    //with current setup, shouldn't ever hit this code path.
    NSEnumerator *enumerator = [[self childArray] objectEnumerator];
    id anObject;
    while (!itsDead && (anObject = [enumerator nextObject]))
    {
      if ([anObject isKindOfClass:[BookmarkFolder class]])
        itsDead = [anObject deleteChild:aChild];
    }
  }
  return itsDead;
}

-(BOOL) deleteBookmark:(Bookmark *)aChild
{
  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  //record undo
  [[undoManager prepareWithInvocationTarget:self] insertChild:aChild atIndex:[[self childArray] indexOfObjectIdenticalTo:aChild] isMove:NO];

  [aChild setParent:nil];
  [[aChild retain] autorelease];   // let it live for the notifications (don't rely on undo manager to keep it alive)
  [[self childArray] removeObject:aChild];

  if (![undoManager isUndoing] && ![self isSmartFolder]) {
    if ([aChild isSeparator])
      [undoManager setActionName:NSLocalizedString(@"Delete Separator", @"")];
    else
      [undoManager setActionName:NSLocalizedString(@"Delete Bookmark", @"")];
  }

  [self itemRemovedNote:aChild];
  return YES;
}

-(BOOL) deleteBookmarkFolder:(BookmarkFolder *)aChild
{
  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  //Make sure it's not a special array - redundant, but oh well.
  if ([aChild isSpecial])
    return NO;

  [undoManager beginUndoGrouping];

  // remove all the children first, so notifications are sent out about their removal
  unsigned int numChildren = [aChild count];
  while (numChildren > 0)
    [aChild deleteChild:[aChild objectAtIndex:--numChildren]];

  // record undo
  [[undoManager prepareWithInvocationTarget:self] insertChild:aChild atIndex:[[self childArray] indexOfObjectIdenticalTo:aChild] isMove:NO];
  [undoManager endUndoGrouping];

  [aChild setParent:nil];
  [[aChild retain] autorelease];   // let it live for the notifications (don't rely on undo manager to keep it alive)
  [[self childArray] removeObject:aChild];

  if (![undoManager isUndoing])
    [undoManager setActionName:NSLocalizedString(@"Delete Folder", @"")];

  [self itemRemovedNote:aChild];
  return YES;
}

//
// Make us look like an array methods
//
// figure out # of top level children
-(unsigned)count
{
  return [mChildArray count];
}

-(id) objectAtIndex:(unsigned)index
{
  if ([mChildArray count] > index)
    return [mChildArray objectAtIndex:index];
  return nil;
}

-(unsigned)indexOfObject:(id)object
{
  return [mChildArray indexOfObject:object];
}

-(unsigned)indexOfObjectIdenticalTo:(id)object
{
  return [mChildArray indexOfObjectIdenticalTo:object];
}

- (id)savedSpecialFlag
{
  return [NSNumber numberWithUnsignedInt:mSpecialFlag];
}

//
// build submenu
//
-(void)buildFlatFolderList:(NSMenu *)menu depth:(unsigned)depth
{
  NSEnumerator *children = [mChildArray objectEnumerator];
  id aKid;
  while ((aKid = [children nextObject])) {
    if ([aKid isKindOfClass:[BookmarkFolder class]]) {
      if (![aKid isSmartFolder]) {
        NSString* paddedTitle = [[aKid title] stringByTruncatingTo:80 at:kTruncateAtMiddle];
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle: paddedTitle action: NULL keyEquivalent: @""];
        [menuItem setRepresentedObject:aKid];

        NSImage *curIcon = [aKid icon];
        NSSize iconSize = [curIcon size];
        NSImage *shiftedIcon = [[NSImage alloc] initWithSize:NSMakeSize(depth*(iconSize.width), iconSize.height)];
        [shiftedIcon lockFocus];
        [curIcon drawInRect:NSMakeRect(([shiftedIcon size].width - iconSize.width), 0, iconSize.width, iconSize.height)
                   fromRect:NSMakeRect(0, 0, iconSize.width, iconSize.height)
                  operation:NSCompositeCopy
                   fraction:1];
        [shiftedIcon unlockFocus];
        [menuItem setImage:shiftedIcon];
        [shiftedIcon release];

        [menu addItem:menuItem];
        [menuItem release];

        [aKid buildFlatFolderList:menu depth:(depth + 1)];
      }
    }
  }
}

//
// searching/keywords processing
//
-(NSArray*)resolveKeyword:(NSString *)keyword withArgs:(NSString *)args
{
  // see if it's us
  if ([[self keyword] isEqualToString:keyword]) {
    NSMutableArray *urlArray = (NSMutableArray *)[self childURLs];
    int i, j=[urlArray count];
    for (i = 0; i < j; i++) {
      NSString *newURL = [self expandURL:[urlArray objectAtIndex:i] withString:args];
      [urlArray replaceObjectAtIndex:i withObject:newURL];
    }
    return urlArray;
  }
  // see if it's one of our kids
  NSEnumerator* enumerator = [[self childArray] objectEnumerator];
  id aKid;
  while ((aKid = [enumerator nextObject])) {
    if ([aKid isKindOfClass:[Bookmark class]]) {
      if ([[aKid keyword] isEqualToString:keyword])
        return [NSArray arrayWithObject:[self expandURL:[aKid url] withString:args]];
    }
    else if ([aKid isKindOfClass:[BookmarkFolder class]]) {
      // recurse into sub-folders
      NSArray *childArray = [aKid resolveKeyword:keyword withArgs:args];
      if (childArray)
        return childArray;
    }
  }
  return nil;
}

- (NSString*)expandURL:(NSString*)url withString:(NSString*)searchString
{
  NSRange matchRange = [url rangeOfString:@"%s"];
  if (matchRange.location != NSNotFound) {
    NSString* escapedString = [(NSString*)CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault,
                                                                                 (CFStringRef)searchString, 
                                                                                 NULL, 
                                                                                 // legal URL characters that should be encoded in search terms
                                                                                 CFSTR(";?:@&=+$,"),
                                                                                 kCFStringEncodingUTF8) autorelease];
    NSString* resultString = [NSString stringWithFormat:@"%@%@%@",
      [url substringToIndex:matchRange.location],
      escapedString,
      [url substringFromIndex:(matchRange.location + matchRange.length)]];
    return resultString;
  }
  return url;
}

- (NSSet*)bookmarksWithString:(NSString *)searchString inFieldWithTag:(int)tag
{
  NSMutableSet *foundSet = [NSMutableSet set];
  
  // see if our kids match
  NSEnumerator *enumerator = [[self childArray] objectEnumerator];
  id childItem;
  while ((childItem = [enumerator nextObject]))
  {
    if ([childItem isKindOfClass:[Bookmark class]])
    {
      if ([childItem matchesString:searchString inFieldWithTag:tag])
        [foundSet addObject:childItem];
    }
    else if ([childItem isKindOfClass:[BookmarkFolder class]])
    {
      // recurse, adding found items to the existing set
      NSSet *childFoundSet = [childItem bookmarksWithString:searchString inFieldWithTag:tag];
      [foundSet unionSet:childFoundSet];
    }
  }
  return foundSet;
}

- (BOOL)containsChildItem:(BookmarkItem*)inItem
{
  if (inItem == self)
    return YES;
  
  unsigned int numChildren = [mChildArray count];
  for (unsigned int i = 0; i < numChildren; i ++)
  {
    BookmarkItem* curChild = [mChildArray objectAtIndex:i];
    if (curChild == inItem)
      return YES;
    
    if ([curChild isKindOfClass:[BookmarkFolder class]])
    {
      if ([(BookmarkFolder *)curChild containsChildItem:inItem])
        return YES;
    }
  }
  
  return NO;
}

//
// Notification stuff
//

-(void) itemAddedNote:(BookmarkItem *)theItem atIndex:(unsigned)anIndex
{
  if ([[BookmarkManager sharedBookmarkManager] areChangeNotificationsSuppressed])
    return;
  
  NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
                                            theItem, BookmarkFolderChildKey,
           [NSNumber numberWithUnsignedInt:anIndex], BookmarkFolderChildIndexKey,
                                                     nil];

  NSNotification *note = [NSNotification notificationWithName:BookmarkFolderAdditionNotification object:self userInfo:dict];
  [[NSNotificationCenter defaultCenter] postNotification:note];
}

-(void) itemRemovedNote:(BookmarkItem *)theItem
{
  if ([[BookmarkManager sharedBookmarkManager] areChangeNotificationsSuppressed])
    return;
  
  NSDictionary *dict = [NSDictionary dictionaryWithObject:theItem forKey:BookmarkFolderChildKey];
  NSNotification *note = [NSNotification notificationWithName:BookmarkFolderDeletionNotification object:self userInfo:dict];
  [[NSNotificationCenter defaultCenter] postNotification:note];
}

-(void) itemChangedNote:(BookmarkItem *)theItem
{
  [self itemUpdatedNote:kBookmarkItemChildrenChangedMask];
}

-(void) dockMenuChanged:(NSNotification *)note
{
  if (([self isDockMenu]) && ([note object] != self))
    [self setIsDockMenu:NO];
}


#pragma mark -

//
// reading/writing from/to disk
//

-(BOOL) readNativeDictionary:(NSDictionary *)aDict
{
  [self setTitle:[aDict objectForKey:BMTitleKey]];
  [self setItemDescription:[aDict objectForKey:BMFolderDescKey]];
  [self setKeyword:[aDict objectForKey:BMFolderKeywordKey]];
  [self setUUID:[aDict objectForKey:BMUUIDKey]];

  unsigned int flag = [[aDict objectForKey:BMFolderTypeKey] unsignedIntValue];
  // on the off chance we've imported somebody else's bookmarks after startup,
  // we need to clear any super special flags on it.  if we have a shared bookmark manager,
  // we're not in startup, so clear things out.
  if ([[BookmarkManager sharedBookmarkManager] bookmarksLoaded]) {
    if ((flag & kBookmarkRootFolder) != 0)
      flag &= ~kBookmarkRootFolder;
    if ((flag & kBookmarkToolbarFolder) != 0)
      flag &= ~kBookmarkToolbarFolder;
    if ((flag & kBookmarkSmartFolder) != 0)
      flag &= ~kBookmarkSmartFolder;
  }
  [self setSpecialFlag:flag];

  NSEnumerator* enumerator = [[aDict objectForKey:BMChildrenKey] objectEnumerator];
  BOOL success = YES;
  id aKid;
  while ((aKid = [enumerator nextObject]) && success) {
    if ([aKid objectForKey:BMChildrenKey])
      success = [self addBookmarkFolderFromNativeDict:(NSDictionary *)aKid];
    else 
      success = [self addBookmarkFromNativeDict:(NSDictionary *)aKid];
  }
  return success;
}

-(BOOL) readSafariDictionary:(NSDictionary *)aDict
{
  [self setTitle:[aDict objectForKey:BMTitleKey]];

  BOOL success = YES;
  NSEnumerator* enumerator = [[aDict objectForKey:BMChildrenKey] objectEnumerator];
  id aKid;
  while ((aKid = [enumerator nextObject]) && success)
  {
    if ([[aKid objectForKey:SafariTypeKey] isEqualToString:SafariLeaf])
      success = [self addBookmarkFromSafariDict:(NSDictionary *)aKid];
    else if ([[aKid objectForKey:SafariTypeKey] isEqualToString:SafariList])
      success = [self addBookmarkFolderFromSafariDict:(NSDictionary *)aKid];
    // might also be a WebBookmarkTypeProxy - we'll ignore those
  }

  if ([[aDict objectForKey:SafariAutoTab] boolValue])
    [self setIsGroup:YES];

  return success;
}

-(BOOL) readCaminoXML:(CFXMLTreeRef)aTreeRef settingToolbar:(BOOL)setupToolbar
{
  BOOL success = YES;
  CFXMLNodeRef myNode = CFXMLTreeGetNode(aTreeRef);
  if (myNode)
  {
    // Process our info - we load info into tempMuteString,
    // then send a cleaned up version to temp string, which gets
    // dropped into appropriate variable
    if (CFXMLNodeGetTypeCode(myNode) == kCFXMLNodeTypeElement)
    {
      CFXMLElementInfo* elementInfoPtr = (CFXMLElementInfo*)CFXMLNodeGetInfoPtr(myNode);
      if (elementInfoPtr)
      {
        NSDictionary* attribDict = (NSDictionary*)elementInfoPtr->attributes;
        [self setTitle:[[attribDict objectForKey:CaminoNameKey] stringByRemovingAmpEscapes]];
        [self setItemDescription:[[attribDict objectForKey:CaminoDescKey] stringByRemovingAmpEscapes]];

        if ([[attribDict objectForKey:CaminoTypeKey] isEqualToString:CaminoToolbarKey] && setupToolbar)
        {
          // we move the toolbar folder to its correct location later
          [self setIsToolbar:YES];
        }
        
        if ([[attribDict objectForKey:CaminoGroupKey] isEqualToString:CaminoTrueKey])
          [self setIsGroup:YES];

        // Process children
        unsigned int i = 0;
        CFXMLTreeRef childTreeRef;
        while ((childTreeRef = CFTreeGetChildAtIndex(aTreeRef,i++)) && success)
        {
          CFXMLNodeRef childNodeRef = CFXMLTreeGetNode(childTreeRef);
          if (childNodeRef) {
            NSString *tempString = (NSString *)CFXMLNodeGetString(childNodeRef);
            if ([tempString isEqualToString:CaminoBookmarkKey])
              success = [self addBookmarkFromXML:childTreeRef];
            else if ([tempString isEqualToString:CaminoFolderKey])
              success = [self addBookmarkFolderFromXML:childTreeRef settingToolbar:setupToolbar];
          } 
        }
      } else  {
        NSLog(@"BookmarkFolder: readCaminoXML- elementInfoPtr null - children not imported");
        success = NO;
      }
    } else {
      NSLog(@"BookmarkFolder: readCaminoXML - should be, but isn't a CFXMLNodeTypeElement");
      success = NO;
    }
  } else {
    NSLog(@"BookmarkFolder: readCaminoXML - myNode null - bookmark not imported");
    success = NO;
  }

  return success;
}

//
// -writeBookmarksMetadataToPath:
//
// Recursively tells each of its children to write out its spotlight metadata at the
// given path. Folders themselves aren't written to the metadata store.
//
-(void)writeBookmarksMetadataToPath:(NSString*)inPath
{
  if (![self isSmartFolder]) {
    id item;
    NSEnumerator* enumerator = [mChildArray objectEnumerator];
    // do it for each child
    while ((item = [enumerator nextObject]))
      [item writeBookmarksMetadataToPath:inPath];
  }
}

//
// -removeBookmarksMetadataFromPath
//
// Recursively tell each child to remove its spotlight metadata from the given path. Folders
// themselves have no metadata.
//
-(void)removeBookmarksMetadataFromPath:(NSString*)inPath
{
  if (![self isSmartFolder]) {
    id item;
    NSEnumerator* enumerator = [mChildArray objectEnumerator];
    // do it for each child
    while ((item = [enumerator nextObject]))
      [item removeBookmarksMetadataFromPath:inPath];
  }
}

-(NSDictionary *)writeNativeDictionary
{
  if ([self isSmartFolder])
    return nil;

  id item;
  NSMutableArray* children = [NSMutableArray arrayWithCapacity:[mChildArray count]];
  NSEnumerator* enumerator = [mChildArray objectEnumerator];
  //get chillins first
  while ((item = [enumerator nextObject])) {
    id aDict = [item writeNativeDictionary];
    if (aDict)
      [children addObject:aDict];
  }

  NSMutableDictionary* folderDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                            children, BMChildrenKey,
                   [self savedTitle], BMTitleKey,
                    [self savedUUID], BMUUIDKey,
             [self savedSpecialFlag], BMFolderTypeKey,
                                      nil];

  if ([[self itemDescription] length])
    [folderDict setObject:[self itemDescription] forKey:BMFolderDescKey]; 

  if ([[self keyword] length])
    [folderDict setObject:[self keyword] forKey:BMFolderKeywordKey]; 

  return folderDict;
}

-(NSDictionary *)writeSafariDictionary
{
  if (![self isSmartFolder]) {
    id item;
    NSMutableArray* children = [NSMutableArray array];
    NSEnumerator* enumerator = [mChildArray objectEnumerator];
    //get chillins first
    while ((item = [enumerator nextObject])) {
      id aDict = [item writeSafariDictionary];
      if (aDict)
        [children addObject:aDict];
    }
    NSString *titleString;
    if ([self isToolbar])
      titleString = @"BookmarksBar";
    else if ([[BookmarkManager sharedBookmarkManager] bookmarkMenuFolder] == self)
      titleString = @"BookmarksMenu";
    else
      titleString = [self title];

    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
      titleString,BMTitleKey,
      SafariList,SafariTypeKey,
      [self UUID],SafariUUIDKey,
      children,BMChildrenKey, nil];

    if ([self isGroup])
      [dict setObject:[NSNumber numberWithBool:YES] forKey:SafariAutoTab];
    return dict;
    // now we handle the smart folders
  } else {
    NSString *SafariProxyKey = @"WebBookmarkIdentifier";
    NSString *SafariProxyType = @"WebBookmarkTypeProxy";
    if ([[BookmarkManager sharedBookmarkManager] rendezvousFolder] == self) {
      return [NSDictionary dictionaryWithObjectsAndKeys:
        @"Rendezvous",BMTitleKey,
        @"Rendezvous Bookmark Proxy Identifier",SafariProxyKey,
        SafariProxyType,SafariTypeKey,
        [self UUID],SafariUUIDKey,
        nil];
    }
    else if ([[BookmarkManager sharedBookmarkManager] addressBookFolder] == self) {
      return [NSDictionary dictionaryWithObjectsAndKeys:
        @"Address Book",BMTitleKey,
        @"Address Book Bookmark Proxy Identifier",SafariProxyKey,
        SafariProxyType,SafariTypeKey,
        [self UUID],SafariUUIDKey,
        nil];
    }
    else if ([[BookmarkManager sharedBookmarkManager] historyFolder] == self) {
      return [NSDictionary dictionaryWithObjectsAndKeys:
        @"History",BMTitleKey,
        @"History Bookmark Proxy Identifier",SafariProxyKey,
        SafariProxyType,SafariTypeKey,
        [self UUID],SafariUUIDKey,
        nil];
    }
  }
  return nil;
}

-(NSString *)writeHTML:(unsigned)aPad
{
  id item = nil;
  NSString* htmlString = nil;

  NSMutableString *padString = [NSMutableString string];
  for (unsigned i = 0; i < aPad;i++) 
    [padString insertString:@"    " atIndex:0];

  // Normal folders
  if ((![self isToolbar] && ![self isRoot] && ![self isSmartFolder])) {
    NSString *formatString;
    if ([self isGroup])
      formatString = @"%@<DT><H3 FOLDER_GROUP=\"true\">%@</H3>\n%@<DL><p>\n";
    else
      formatString = @"%@<DT><H3>%@</H3>\n%@<DL><p>\n";
    htmlString = [NSString stringWithFormat:formatString, padString, [mTitle stringByAddingAmpEscapes], padString];
  }
  // Toolbar Folder
  else if ([self isToolbar])
    htmlString = [NSString stringWithFormat:@"%@<DT><H3 PERSONAL_TOOLBAR_FOLDER=\"true\">%@</H3>\n%@<DL><p>\n",
      padString,
      [mTitle stringByAddingAmpEscapes],
      padString];
  // Root Folder
  else if ([self isRoot]) 
    htmlString = [NSString stringWithFormat:@"%@\n%@\n%@\n%@\n\n<DL><p>\n",
      @"<!DOCTYPE NETSCAPE-Bookmark-file-1>",
      @"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">",
      @"<TITLE>Bookmarks</TITLE>",
      @"<H1>Bookmarks</H1>"];
   // Folder-Not-Appearing-In-This-File Folder (ie, smart folders)
   else if ([self isSmartFolder])
     return @"";

  NSEnumerator* enumerator = [mChildArray objectEnumerator];
  //get chillins first
  while ((item = [enumerator nextObject]))
    htmlString = [htmlString stringByAppendingString:[item writeHTML:aPad+1]];
  return [htmlString stringByAppendingString:[NSString stringWithFormat:@"%@</DL><p>\n", padString]];
}

#pragma mark -

//
// Scripting methods
//
- (NSScriptObjectSpecifier *)objectSpecifier
{
  id parent = [self parent];
  NSScriptObjectSpecifier *containerRef = [parent objectSpecifier];
  NSScriptClassDescription *arrayScriptClassDesc = (NSScriptClassDescription *)[NSClassDescription classDescriptionForClass:[BookmarkFolder class]];
  if ([parent isKindOfClass:[BookmarkFolder class]]) {
    unsigned index = [[parent childArray] indexOfObjectIdenticalTo:self];
    if (index != NSNotFound) {
      // need to get the appropriate class description
      return [[[NSIndexSpecifier allocWithZone:[self zone]] initWithContainerClassDescription:arrayScriptClassDesc containerSpecifier:containerRef key:@"childArray" index:index] autorelease];
    } else 
      return nil;
  } else  // root bookmark
    return [[[NSPropertySpecifier allocWithZone:[self zone]] initWithContainerSpecifier:containerRef key:@"rootArray"] autorelease];
  return nil;
}

- (NSArray *)folderItemsWithClass:(Class)theClass
{
  NSArray *kiddies = [self childArray];
  NSMutableArray *result = [NSMutableArray array];
  unsigned i, c = [kiddies count];
  id curItem;
  for (i = 0; i < c; i++) {
    curItem = [kiddies objectAtIndex:i];
    if ([curItem isKindOfClass:theClass]) {
      [result addObject:curItem];
    }
  }
  return result;
}

// both childBookmarks and childFolders are read-only functions.
-(NSArray *)childBookmarks
{
  return [self folderItemsWithClass:[Bookmark class]];
}

-(NSArray *)childFolders
{
  return [self folderItemsWithClass:[BookmarkFolder class]];
}

// all the insert, add, remove, replace methods for the 3 sets of items in a bookmark array
-(void) insertInChildArray:(BookmarkItem *)aItem atIndex:(unsigned)aIndex
{
  [self insertChild:aItem atIndex:aIndex isMove:NO];
}

-(void) addInChildArray:(BookmarkItem *)aItem
{
  [self appendChild:aItem];
}

-(void) removeFromChildArrayAtIndex:(unsigned)aIndex
{
  BookmarkItem* aKid = [[self childArray] objectAtIndex:aIndex];
  [self deleteChild:aKid];
}

-(void)replaceInChildArray:(BookmarkItem *)aItem atIndex:(unsigned)aIndex
{
  [self removeFromChildArrayAtIndex:aIndex];
  [self insertChild:aItem atIndex:aIndex isMove:NO];
}

//
// child bookmark stuff
//
-(void) insertInChildBookmarks:(Bookmark *)aItem atIndex:(unsigned)aIndex
{
  NSArray *bookmarkArray = [self childBookmarks];
  Bookmark *aBookmark;
  unsigned realIndex;
  if (aIndex < [bookmarkArray count])  {
    aBookmark = [bookmarkArray objectAtIndex:aIndex];
    realIndex = [[self childArray] indexOfObject:aBookmark];
  } else {
    aBookmark = [bookmarkArray lastObject];
    realIndex = 1 + [[self childArray] indexOfObject:aBookmark];
  }
  [self insertChild:aItem atIndex:realIndex isMove:NO];
}

-(void) addInChildBookmarks:(Bookmark *)aItem
{
  NSArray *bookmarkArray = [self childBookmarks];
  Bookmark *aBookmark = [bookmarkArray lastObject];
  unsigned index = [[self childArray] indexOfObject:aBookmark];
  [self insertChild:aItem atIndex:++index isMove:NO];
}

-(void) removeFromChildBookmarksAtIndex:(unsigned)aIndex
{
  Bookmark* aKid = [[self childBookmarks] objectAtIndex:aIndex];
  [self deleteChild:aKid];
}

-(void)replaceInChildBookmarks:(Bookmark *)aItem atIndex:(unsigned)aIndex
{
  [self removeFromChildBookmarksAtIndex:aIndex];
  [self insertInChildBookmarks:aItem atIndex:aIndex];
}

//
// child bookmark folder stuff
//
-(void) insertInChildFolders:(BookmarkFolder *)aItem atIndex:(unsigned)aIndex
{
  NSArray *folderArray = [self childFolders];
  BookmarkFolder *aFolder;
  unsigned realIndex;
  if (aIndex < [folderArray count]) {
    aFolder = [folderArray objectAtIndex:aIndex];
    realIndex = [[self childArray] indexOfObject:aFolder];
  } else {
    aFolder = [folderArray lastObject];
    realIndex = 1 + [[self childArray] indexOfObject:aFolder];
  }
  [self insertChild:aItem atIndex:realIndex isMove:NO];
}

-(void) addInChildFolders:(BookmarkFolder *)aItem
{
  NSArray *folderArray = [self childFolders];
  BookmarkFolder *aFolder = [folderArray lastObject];
  unsigned index = [[self childArray] indexOfObject:aFolder];
  [self insertChild:aItem atIndex:++index isMove:NO];
}

-(void) removeFromChildFoldersAtIndex:(unsigned)aIndex
{
  BookmarkFolder* aKid = [[self childFolders] objectAtIndex:aIndex];
  [self deleteChild:aKid];
}

-(void)replaceInChildFolders:(BookmarkFolder *)aItem atIndex:(unsigned)aIndex
{
  [self removeFromChildFoldersAtIndex:aIndex];
  [self insertInChildFolders:aItem atIndex:aIndex];
}

//
// These next 3 methods swiped almost exactly out of sketch.app by apple.
// look there for an explanation if you're confused.
//
- (NSArray *)indicesOfObjectsByEvaluatingObjectSpecifier:(NSScriptObjectSpecifier *)specifier
{
  if ([specifier isKindOfClass:[NSRangeSpecifier class]]) 
    return [self indicesOfObjectsByEvaluatingRangeSpecifier:(NSRangeSpecifier *)specifier];
  else if ([specifier isKindOfClass:[NSRelativeSpecifier class]])
    return [self indicesOfObjectsByEvaluatingRelativeSpecifier:(NSRelativeSpecifier *)specifier];
  // If we didn't handle it, return nil so that the default object specifier evaluation will do it.
  return nil;  
}

- (NSArray *)indicesOfObjectsByEvaluatingRelativeSpecifier:(NSRelativeSpecifier *)relSpec
{
  NSString *key = [relSpec key];
  if ([key isEqualToString:@"childBookmarks"] ||
      [key isEqualToString:@"childArray"] ||
      [key isEqualToString:@"childFolders"] )
  {
    NSScriptObjectSpecifier *baseSpec = [relSpec baseSpecifier];
    NSString *baseKey = [baseSpec key];
    NSArray *kiddies = [self childArray];
    NSRelativePosition relPos = [relSpec relativePosition];
    if (baseSpec == nil)
      return nil;

    if ([kiddies count] == 0)
      return [NSArray array];

    if ([baseKey isEqualToString:@"childBookmarks"] ||
        [baseKey isEqualToString:@"childArray"] ||
        [baseKey isEqualToString:@"childFolders"] )
    {
      unsigned baseIndex;
      id baseObject = [baseSpec objectsByEvaluatingWithContainers:self];
      if ([baseObject isKindOfClass:[NSArray class]]) {
        int baseCount = [baseObject count];
        if (baseCount == 0) 
          baseObject = nil;
        else {
          if (relPos == NSRelativeBefore)
            baseObject = [baseObject objectAtIndex:0];
          else
            baseObject = [baseObject objectAtIndex:(baseCount-1)];
        }
      }

      if (!baseObject) 
        // Oops.  We could not find the base object.
        return nil;

      baseIndex = [kiddies indexOfObjectIdenticalTo:baseObject];
      if (baseIndex == NSNotFound) 
        // Oops.  We couldn't find the base object in the child array.  This should not happen.
        return nil;

      NSMutableArray *result = [NSMutableArray array];
      BOOL keyIsArray = [key isEqualToString:@"childArray"];
      NSArray *relKeyObjects = (keyIsArray ? nil : [self valueForKey:key]);
      id curObj;
      unsigned curKeyIndex, kiddiesCount = [kiddies count];
      if (relPos == NSRelativeBefore)
          baseIndex--;
      else 
          baseIndex++;

      while ((baseIndex >= 0) && (baseIndex < kiddiesCount))
      {
        if (keyIsArray) {
          [result addObject:[NSNumber numberWithInt:baseIndex]];
          break;
        } else {
          curObj = [kiddies objectAtIndex:baseIndex];
          curKeyIndex = [relKeyObjects indexOfObjectIdenticalTo:curObj];
          if (curKeyIndex != NSNotFound) {
            [result addObject:[NSNumber numberWithInt:curKeyIndex]];
            break;
          }
        }

        if (relPos == NSRelativeBefore)
          baseIndex--;
        else
          baseIndex++;
      }
      return result;
    }
  }
  return nil;
}

- (NSArray *)indicesOfObjectsByEvaluatingRangeSpecifier:(NSRangeSpecifier *)rangeSpec
{
  NSString *key = [rangeSpec key];
  if ([key isEqualToString:@"childBookmarks"] ||
      [key isEqualToString:@"childArray"] ||
      [key isEqualToString:@"childFolders"] )
  {
    NSScriptObjectSpecifier *startSpec = [rangeSpec startSpecifier];
    NSScriptObjectSpecifier *endSpec = [rangeSpec endSpecifier];
    NSString *startKey = [startSpec key];
    NSString *endKey = [endSpec key];
    NSArray *kiddies = [self childArray];

    if ((startSpec == nil) && (endSpec == nil))
      return nil;
    if ([kiddies count] == 0) 
      return [NSArray array];

    if ((!startSpec || [startKey isEqualToString:@"childBookmarks"] || [startKey isEqualToString:@"childArray"] || [startKey isEqualToString:@"childFolders"]) && (!endSpec || [endKey isEqualToString:@"childBookmarks"] || [endKey isEqualToString:@"childArray"] || [endKey isEqualToString:@"childFolders"]))
    {
      unsigned startIndex;
      unsigned endIndex;
      if (startSpec) {
        id startObject = [startSpec objectsByEvaluatingSpecifier];
        if ([startObject isKindOfClass:[NSArray class]]) {
          if ([startObject count] == 0)
            startObject = nil;
          else
            startObject = [startObject objectAtIndex:0];
        }
        if (!startObject)
          return nil;
        startIndex = [kiddies indexOfObjectIdenticalTo:startObject];
        if (startIndex == NSNotFound)
          return nil;
      } else
        startIndex = 0;

      if (endSpec) {
        id endObject = [endSpec objectsByEvaluatingSpecifier];
        if ([endObject isKindOfClass:[NSArray class]]) {
          unsigned endObjectsCount = [endObject count];
          if (endObjectsCount == 0)
            endObject = nil;
          else 
            endObject = [endObject objectAtIndex:(endObjectsCount-1)];
        }
        if (!endObject)
          return nil;
        endIndex = [kiddies indexOfObjectIdenticalTo:endObject];
        if (endIndex == NSNotFound)
          return nil;
      } else 
        endIndex = [kiddies count] - 1;

      if (endIndex < startIndex) {
        int temp = endIndex;
        endIndex = startIndex;
        startIndex = temp;
      }
      
      NSMutableArray *result = [NSMutableArray array];
      BOOL keyIsArray = [key isEqual:@"childArray"];
      NSArray *rangeKeyObjects = (keyIsArray ? nil : [self valueForKey:key]);
      id curObj;
      unsigned curKeyIndex, i;
      for (i=startIndex; i<=endIndex; i++) {
        if (keyIsArray)
          [result addObject:[NSNumber numberWithInt:i]];
        else {
          curObj = [kiddies objectAtIndex:i];
          curKeyIndex = [rangeKeyObjects indexOfObjectIdenticalTo:curObj];
          if (curKeyIndex != NSNotFound)
            [result addObject:[NSNumber numberWithInt:curKeyIndex]];
        }
      }
      return result;
    }
  }
  return nil;
}

@end;
