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
*    David Haas <haasd@cae.wisc.edu>
*
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

#import "BookmarkManager.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "NSString+Utils.h"

// Notification definitions
NSString* BookmarkFolderAdditionNotification = @"bf_add";
NSString* BookmarkFolderDeletionNotification = @"bf_del";
NSString* BookmarkFolderChildKey = @"bf_ck";
NSString* BookmarkFolderChildIndexKey = @"bf_ik";
NSString* BookmarkFolderDockMenuChangeNotificaton = @"bf_dmc";

@interface BookmarkFolder (Private)
// status stuff
-(unsigned) specialFlag;
-(void) setSpecialFlag:(unsigned)aNumber;
// ways to add a new bookmark
-(BOOL) addBookmarkFromNativeDict:(NSDictionary *)aDict;
-(BOOL) addBookmarkFromSafariDict:(NSDictionary *)aDict;
-(BOOL) addBookmarkFromXML:(CFXMLTreeRef)aTreeRef;
//ways to add a new bookmark folder
-(BOOL) addBookmarkFolderFromNativeDict:(NSDictionary *)aDict; //read in - adds sequentially
-(BOOL) addBookmarkFolderFromSafariDict:(NSDictionary *)aDict;
-(BOOL) addBookmarkFolderFromXML:(CFXMLTreeRef)aTreeRef;
// Deletes the bookmark or bookmark array
-(void) deleteBookmark:(Bookmark *)childBookmark;
-(void) deleteBookmarkFolder:(BookmarkFolder *)childArray;
  //Notification methods
-(void) itemAddedNote:(BookmarkItem *)theItem atIndex:(unsigned)anIndex;
-(void) itemRemovedNote:(BookmarkItem *)theItem;
-(void) dockMenuChanged:(NSNotification *)note;
// aids in searching
- (NSString*) expandKeyword:(NSString*)keyword inString:(NSString*)location;
- (NSArray *) folderItemWithClass:(Class)theClass;
- (BOOL) isString:(NSString *)searchString inBookmarkItem:(BookmarkItem *)anItem;

@end

@implementation BookmarkFolder

-(id) init
{
  if ((self = [super init]))
  {
    mChildArray = [[NSMutableArray alloc] init];
    mSpecialFlag = [[NSNumber alloc] initWithUnsignedInt:kBookmarkFolder];
    mIcon = [[NSImage imageNamed:@"folder"] retain];
  }
  return self;
}

-(id) copyWithZone:(NSZone *)zone
{
  if ([self isSmartFolder])
    return nil;
  id doppleganger = [super copyWithZone:zone];
  [doppleganger setSpecialFlag:[self specialFlag]];
  NSEnumerator *enumerator = [[self childArray] objectEnumerator];
  id anItem, aCopiedItem;
  //head fake the undomanager
  NSUndoManager *undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  [undoManager disableUndoRegistration];
  while ((anItem = [enumerator nextObject])) {
    aCopiedItem = [anItem copyWithZone:[anItem zone]];
    [doppleganger insertChild:aCopiedItem];
    [aCopiedItem release];
  }
  [undoManager enableUndoRegistration];
  return doppleganger;
}

-(void)dealloc
{
  // NOTE: we're no longer clearning the undo stack here because its pointless and is a potential crasher
  if ([mSpecialFlag unsignedIntValue] & kBookmarkDockMenuFolder)
    [[NSNotificationCenter defaultCenter] removeObserver:self name:BookmarkFolderDockMenuChangeNotificaton object:nil];

  // set our all children to have a null parent.
  // important if they've got timers running.
  NSEnumerator *enumerator = [mChildArray objectEnumerator];
  id kid;
  while ((kid = [enumerator nextObject]))
    [kid setParent:nil];
  [mChildArray release];
  [mSpecialFlag release];
  [super dealloc];
}

// do this so we can start update timers in bookmarks after load
-(void) scheduleTimer
{
  if (![self isSmartFolder])
    [[self childArray] makeObjectsPerformSelector:@selector(scheduleTimer)];
}

-(void) refreshIcon
{
  if (![self isSmartFolder])
    [[self childArray] makeObjectsPerformSelector:@selector(refreshIcon)];
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
    if ([anItem isKindOfClass:[Bookmark class]])
      [urlArray addObject:[(Bookmark *)anItem url]];
  }
  return urlArray;
}

-(NSArray *) allChildBookmarks
{
  NSMutableArray *anArray = [NSMutableArray array];
  [anArray addObjectsFromArray:[self folderItemWithClass:[Bookmark class]]];
  NSEnumerator *enumerator = [[self folderItemWithClass:[BookmarkFolder class]] objectEnumerator];
  BookmarkFolder *kid;
  while ((kid = [enumerator nextObject])) {
    NSArray *kidsBookmarks = [kid allChildBookmarks];
    [anArray addObjectsFromArray:kidsBookmarks];
  }
  return anArray;
}

-(BOOL) isSpecial
{
  if ([self isToolbar] || [self isRoot] || [self isSmartFolder] || [self isDockMenu])
    return YES;
  return NO;
}

-(BOOL) isToolbar
{
  if (([self specialFlag] & kBookmarkToolbarFolder) != 0)
    return YES;
  return NO;
}

-(BOOL) isRoot
{
  if (([self specialFlag] & kBookmarkRootFolder) != 0)
    return YES;
  return NO;
}


-(BOOL) isGroup
{
  if (([self specialFlag] & kBookmarkFolderGroup) != 0)
    return YES;
  return NO;
}

-(BOOL) isSmartFolder
{
  if (([self specialFlag] & kBookmarkSmartFolder) != 0)
    return YES;
  return NO;
}

-(BOOL) isDockMenu
{
  if (([self specialFlag] & kBookmarkDockMenuFolder) != 0)
    return YES;
  return NO;
}

-(unsigned) specialFlag
{
  return [mSpecialFlag unsignedIntValue];
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
  unsigned oldFlag = [mSpecialFlag unsignedIntValue];
  [mSpecialFlag release];
  mSpecialFlag = [[NSNumber alloc] initWithUnsignedInt:aFlag];
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

-(void) setIsDockMenu:(BOOL)aBool;
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
-(void) insertChild:(BookmarkItem *) aChild
{
  [self insertChild:aChild atIndex:[self count] isMove:NO];
}

-(void) insertChild:(BookmarkItem *)aChild atIndex:(unsigned)aPosition isMove:(BOOL)aBool
{
  [aChild setParent:self];
  unsigned insertPoint = [mChildArray count];
  if (insertPoint > aPosition)
    insertPoint = aPosition;
  [mChildArray insertObject:aChild atIndex:insertPoint];
  if (!aBool && ![self isSmartFolder]) {
    NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
    [[undoManager prepareWithInvocationTarget:self] deleteChild:aChild];
    if (![undoManager isUndoing]) {
      if ([aChild isKindOfClass:[BookmarkFolder class]])
        [undoManager setActionName:NSLocalizedString(@"Add Folder",@"Add Folder")];
      else if ([aChild isKindOfClass:[Bookmark class]]) {
        if (![(Bookmark  *)aChild isSeparator])
          [undoManager setActionName:NSLocalizedString(@"Add Bookmark",@"Add Bookmark")];
        else
          [undoManager setActionName:NSLocalizedString(@"Add Separator",@"Add Separator")];
      }
    } else {
      [aChild itemUpdatedNote];
    }
  }
  [self itemAddedNote:aChild atIndex:insertPoint];
}

//
// Smart folder utilities - we don't set ourself as parent
//
-(void) insertIntoSmartFolderChild:(BookmarkItem *)aItem
{
  if ([self isSmartFolder]) {
    [[self childArray] addObject:aItem];
    [self itemAddedNote:aItem atIndex:([self count]-1)];
  }
}
-(void) deleteFromSmartFolderChildAtIndex:(unsigned)index
{
  if ([self isSmartFolder]) {
    BookmarkItem *item = [[[self childArray] objectAtIndex:index] retain];
    [[self childArray] removeObjectAtIndex:index];
    [self itemRemovedNote:[item autorelease]];
  }   
}

//
// Adding bookmarks
//

-(Bookmark *)addBookmark;
{
  if (![self isRoot]) {
    Bookmark* theBookmark = [[Bookmark alloc] init];
    [self insertChild:theBookmark];
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
  return [[self addBookmark] readCaminoXML:aTreeRef];
}

-(Bookmark *) addBookmark:(NSString *)aTitle url:(NSString *)aURL inPosition:(unsigned)aIndex isSeparator:(BOOL)aBool
{
  return [self addBookmark:aTitle inPosition:aIndex keyword:[NSString string] url:aURL description:[NSString string] lastVisit:[NSDate date] status:kBookmarkOKStatus isSeparator:aBool];
}

// full bodied addition
-(Bookmark *) addBookmark:(NSString *)aTitle inPosition:(unsigned)aPosition keyword:(NSString *)aKeyword url:(NSString *)aURL description:(NSString *)aDescription lastVisit:(NSDate *)aDate status:(unsigned)aStatus isSeparator:(BOOL)aSeparator
{
  if (![self isRoot]) {
    Bookmark *theBookmark = [[Bookmark alloc] init];
    [self insertChild:theBookmark atIndex:aPosition isMove:NO];
    [theBookmark release];
    [theBookmark setTitle:aTitle];
    [theBookmark setKeyword:aKeyword];
    [theBookmark setUrl:aURL];
    [theBookmark setDescription:aDescription];
    [theBookmark setLastVisit:aDate];
    [theBookmark setStatus:aStatus];
    [theBookmark setIsSeparator:aSeparator];
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
  [self insertChild:theFolder];
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

-(BOOL) addBookmarkFolderFromXML:(CFXMLTreeRef)aTreeRef
{
  return [[self addBookmarkFolder] readCaminoXML:aTreeRef];
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
  //Couple sanity checks
  if ([aChild isKindOfClass:[BookmarkFolder class]]) {
    if ([aNewParent isChildOfItem:aChild])
      return;
    else if ([(BookmarkFolder *)aChild isToolbar] && ![aNewParent isRoot])
      return;
    else if ([(BookmarkFolder *)aChild isSmartFolder] && ![aNewParent isRoot])
      return;
  } else if ([aChild isKindOfClass:[Bookmark class]]){
    if ([aNewParent isRoot])
      return;
    if ((isSeparator = [(Bookmark *)aChild isSeparator])) {
      BookmarkFolder *menuFolder = [[BookmarkManager sharedBookmarkManager] bookmarkMenuFolder];
      if ((aNewParent != menuFolder) && (![aNewParent isChildOfItem: menuFolder]))
        return;
    }
  }
  [undoManager beginUndoGrouping];
  // What we do depends on if we're moving into a new folder, or just
  // rearranging ourself a bit.
  if (self !=aNewParent) {
    [aNewParent insertChild:aChild atIndex:aIndex isMove:YES];
    // DO NOT call deleteChild here.  Just quietly remove it from the array.
    [mChildArray removeObjectAtIndex:curIndex];
    [self itemRemovedNote:aChild];
  } else {
    [aChild retain];
    [mChildArray removeObjectAtIndex:curIndex];
    [self itemRemovedNote:aChild];
    if (curIndex < aIndex)
      --aIndex;
    [self insertChild:aChild atIndex:aIndex isMove:YES];
    [aChild release];
  }
  [[undoManager prepareWithInvocationTarget:aNewParent] moveChild:aChild toBookmarkFolder:self atIndex:curIndex];
  [undoManager endUndoGrouping];
  if ([aChild isKindOfClass:[BookmarkFolder class]])
    [undoManager setActionName:NSLocalizedString(@"Move Folder",@"Move Folder")];
  else if (!isSeparator)
    [undoManager setActionName:NSLocalizedString(@"Move Bookmark",@"Move Bookmark")];
  else
    [undoManager setActionName:NSLocalizedString(@"Move Separator",@"Move Separator")];
}

-(void) copyChild:(BookmarkItem *)aChild toBookmarkFolder:(BookmarkFolder *)aNewParent atIndex:(unsigned)aIndex
{
  if ([aNewParent isRoot] && [aChild isKindOfClass:[Bookmark class]])
    return;
  BookmarkItem *copiedChild = [aChild copyWithZone:nil];
  if (copiedChild) {
    [aNewParent insertChild:copiedChild atIndex:aIndex isMove:NO];
    [copiedChild release];
    NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
    if ([aChild isKindOfClass:[BookmarkFolder class]])
      [undoManager setActionName:NSLocalizedString(@"Copy Folder",@"Copy Bookmark Folder")];
    else
      [undoManager setActionName:NSLocalizedString(@"Copy Bookmark",@"Copy Bookmark")];
  }
}

//
// Deleting children
//
-(BOOL) deleteChild:(BookmarkItem *)aChild
{
  BOOL itsDead = NO;
  if ([[self childArray] indexOfObjectIdenticalTo:aChild] !=NSNotFound) {
    if ([aChild isKindOfClass:[Bookmark class]])
      [self deleteBookmark:(Bookmark *)aChild];
    else if ([aChild isKindOfClass:[BookmarkFolder class]])
      [self deleteBookmarkFolder:(BookmarkFolder *)aChild];
    itsDead = YES;
  } else { //with current setup, shouldn't ever hit this code path.
    NSEnumerator *enumerator = [[self childArray] objectEnumerator];
    id anObject;
    while (!itsDead && (anObject = [enumerator nextObject])) {
      if ([anObject isKindOfClass:[BookmarkFolder class]])
        itsDead = [anObject deleteChild:aChild];
    }
  }
  return itsDead;
}

-(void) deleteBookmark:(Bookmark *)aChild
{
  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  //record undo
  [[undoManager prepareWithInvocationTarget:self] insertChild:aChild atIndex:[[self childArray] indexOfObjectIdenticalTo:aChild] isMove:NO];
  [aChild setParent:nil];
  [[self childArray] removeObject:aChild];
  if (![undoManager isUndoing] && ![self isSmartFolder]) {
    if (![aChild isSeparator])
      [undoManager setActionName:NSLocalizedString(@"Delete Bookmark",@"Delete Bookmark")];
    else
      [undoManager setActionName:NSLocalizedString(@"Delete Separator",@"Delete Separator")];
  }
  // send message & clean up - undo manager has retained aChild, which prevents crash
  [self itemRemovedNote:aChild];
}

-(void) deleteBookmarkFolder:(BookmarkFolder *)aChild
{
  NSUndoManager* undoManager = [[BookmarkManager sharedBookmarkManager] undoManager];
  //Make sure it's not a special array - redundant, but oh well.
  if ([aChild isSpecial])
    return;
  unsigned size = [aChild count];
  [undoManager beginUndoGrouping];
  while (size > 0)
    [aChild deleteChild:[aChild objectAtIndex:--size]];
  //record undo
  [[undoManager prepareWithInvocationTarget:self] insertChild:aChild atIndex:[[self childArray] indexOfObjectIdenticalTo:aChild] isMove:NO];
  // close undo group
  [undoManager endUndoGrouping];
  [aChild setParent:nil];
  [[self childArray] removeObject:aChild];
  if (![undoManager isUndoing])
    [undoManager setActionName:NSLocalizedString(@"Delete Folder",@"Delete Bookmark Folder")];
  // send message  - undo manager has retained aChild, which prevents crash
  [self itemRemovedNote:aChild];
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

//
// build submenu
//
-(void)buildFlatFolderList:(NSMenu *)menu depth:(unsigned)depth
{
  NSEnumerator *children = [mChildArray objectEnumerator];
  NSMutableString *paddedTitle;
  id aKid;
  while ((aKid = [children nextObject])) {
    if ([aKid isKindOfClass:[BookmarkFolder class]]) {
      if (![aKid isSmartFolder]) {
        paddedTitle = [[aKid title] mutableCopyWithZone:nil];
        if ([paddedTitle length] > 80)
          [paddedTitle stringByTruncatingTo:80 at:kTruncateAtMiddle];
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle: paddedTitle action: NULL keyEquivalent: @""];
        [paddedTitle release];
        [menuItem setRepresentedObject:aKid];
        NSImage *curIcon = [aKid icon];
        NSSize iconSize = [curIcon size];
        NSImage *shiftedIcon = [[NSImage alloc] initWithSize:NSMakeSize(depth*(iconSize.width), iconSize.height)];
        [shiftedIcon lockFocus];
        [curIcon drawInRect:NSMakeRect(([shiftedIcon size].width-iconSize.width),0,iconSize.width,iconSize.height) fromRect:NSMakeRect(0,0,iconSize.width,iconSize.height) operation:NSCompositeCopy fraction:1];
        [shiftedIcon unlockFocus];
        [menuItem setImage:shiftedIcon];
        [shiftedIcon release];
        [menu addItem:menuItem];
        [menuItem release];
        [aKid buildFlatFolderList:menu depth:(depth+1)];
      }
    }
  }
}

//
// searching/keywords processing
//
-(NSArray *)resolveKeyword:(NSString *)aString
{
  // see if it's us
  if ([[self keyword] isEqualToString:aString])
    return [self childURLs];
  // see if it's us after an expansion
  NSRange spaceRange = [aString rangeOfString:@" "];
  NSString *firstWord = nil;
  NSString *secondWord = nil;
  if (spaceRange.location != NSNotFound) {
    firstWord = [aString substringToIndex:spaceRange.location];
    secondWord = [aString substringFromIndex:(spaceRange.location + spaceRange.length)];
    if ([[self keyword] isEqualToString:firstWord]) {
      NSMutableArray *urlArray = (NSMutableArray *)[self childURLs];
      int i, j=[urlArray count];
      for (i = 0; i < j; i++) {
        NSString *newURL = [self expandKeyword:secondWord inString:[urlArray objectAtIndex:i]];
        [urlArray replaceObjectAtIndex:i withObject:newURL];
      }
      return urlArray;
    }
  }
  // see if it's one of our kids
  NSArray *childArray = nil;
  NSEnumerator* enumerator = [[self childArray] objectEnumerator];
  id aKid;
  while ((aKid = [enumerator nextObject])) {
    if ([aKid isKindOfClass:[Bookmark class]]) {
      if ([[aKid keyword] isEqualToString:aString])
        return [NSArray arrayWithObject:[aKid url]];
      if (firstWord) {
        if ([[aKid keyword] isEqualToString:firstWord])
          return [NSArray arrayWithObject:[self expandKeyword:secondWord inString:[aKid url]]];
      }
    }
    else if ([aKid isKindOfClass:[BookmarkFolder class]]) {
      childArray = [aKid resolveKeyword:aString];
      if (childArray)
        return childArray;
    }
  }
  return nil;
}

- (NSString*)expandKeyword:(NSString*)keyword inString:(NSString*)location
{
  NSRange matchRange = [location rangeOfString:@"%s"];
  if (matchRange.location != NSNotFound)
  {
    NSString* resultString = [NSString stringWithFormat:@"%@%@%@",
      [location substringToIndex:matchRange.location],
      keyword,
      [location substringFromIndex:(matchRange.location + matchRange.length)]];
    return resultString;
  }
  return location;
}

-(NSSet *) bookmarksWithString:(NSString *)searchString
{
  NSMutableSet *foundSet = [NSMutableSet set];
  // see if we match
  if ([self isString:searchString inBookmarkItem:self])
    [foundSet addObject:self];
  // see if our kids match
  NSEnumerator *enumerator = [[self childArray] objectEnumerator];
  id aKid;
  while ((aKid = [enumerator nextObject])) {
    if ([aKid isKindOfClass:[Bookmark class]]) {
      if ([self isString:searchString inBookmarkItem:aKid])
        [foundSet addObject:aKid];
    } else if ([aKid isKindOfClass:[BookmarkFolder class]]) {
      NSSet *childFoundSet = [aKid bookmarksWithString:searchString];
      if ([childFoundSet count] > 0) {
        NSEnumerator *kidEnumerator = [childFoundSet objectEnumerator];
        id aKidThing;
        while ((aKidThing = [kidEnumerator nextObject]))
          [foundSet addObject:aKidThing];
      }
    }
  }
  return foundSet;
}

-(BOOL) isString:(NSString *)searchString inBookmarkItem:(BookmarkItem *)anItem
{
  BOOL stringFound = NO;
  if (([[anItem title] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound) ||
      ([[anItem keyword] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound) ||
      ([[anItem description] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound))
    stringFound = YES;
  if (!stringFound && [anItem isKindOfClass:[Bookmark class]]) {
    if ([[(Bookmark *)anItem url] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound)
      stringFound = YES;
  }
  return stringFound;
}

//
// Notification stuff
//

-(void) itemAddedNote:(BookmarkItem *)theItem atIndex:(unsigned)anIndex
{
  if (![BookmarkItem allowNotifications]) return;
  
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:theItem,BookmarkFolderChildKey,
    [NSNumber numberWithUnsignedInt:anIndex],BookmarkFolderChildIndexKey,nil];
  NSNotification *note = [NSNotification notificationWithName:BookmarkFolderAdditionNotification object:self userInfo:dict];
  [nc postNotification:note];
}

-(void) itemRemovedNote:(BookmarkItem *)theItem
{
  if (![BookmarkItem allowNotifications]) return;
  
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  NSDictionary *dict = [NSDictionary dictionaryWithObject:theItem forKey:BookmarkFolderChildKey];
  NSNotification *note = [NSNotification notificationWithName:BookmarkFolderDeletionNotification object:self userInfo:dict];
  [nc postNotification:note];
}

-(void) dockMenuChanged:(NSNotification *)note
{
  if (([self isDockMenu]) && ([note object] != self))
    [self setIsDockMenu:NO];
}


//
// reading/writing from/to disk
//
#pragma mark -

-(BOOL) readNativeDictionary:(NSDictionary *)aDict
{
  id aKid;
  NSEnumerator *enumerator;
  BOOL noErr = YES;
  [self setTitle:[aDict objectForKey:BMTitleKey]];
  [self setDescription:[aDict objectForKey:BMFolderDescKey]];
  [self setKeyword:[aDict objectForKey:BMFolderKeywordKey]];
  unsigned int flag = [[aDict objectForKey:BMFolderTypeKey] unsignedIntValue];
  // on the off chance we've imported somebody else's bookmarks after startup,
  // we need to clear any super special flags on it.  if we have a shared bookmark manager,
  // we're not in startup, so clear things out.
  if ([BookmarkManager sharedBookmarkManager]) {
    if ((flag & kBookmarkRootFolder) != 0)
      flag &= ~kBookmarkRootFolder;
    if ((flag & kBookmarkToolbarFolder) != 0)
      flag &= ~kBookmarkToolbarFolder;
    if ((flag & kBookmarkSmartFolder) != 0)
      flag &= ~kBookmarkSmartFolder;
  }
  [self setSpecialFlag:flag];
  enumerator = [[aDict objectForKey:BMChildrenKey] objectEnumerator];
  while ((aKid = [enumerator nextObject]) && noErr) {
    if ([aKid objectForKey:BMChildrenKey])
      noErr = [self addBookmarkFolderFromNativeDict:(NSDictionary *)aKid];
    else 
      noErr = [self addBookmarkFromNativeDict:(NSDictionary *)aKid];
  }
  return noErr;
}

-(BOOL) readSafariDictionary:(NSDictionary *)aDict
{
  id aKid;
  NSEnumerator *enumerator;
  BOOL noErr = YES;
  [self setTitle:[aDict objectForKey:BMTitleKey]];
  enumerator = [[aDict objectForKey:BMChildrenKey] objectEnumerator];
  while ((aKid = [enumerator nextObject]) && noErr) {
    if ([[aKid objectForKey:SafariTypeKey] isEqualToString:SafariLeaf])
      noErr = [self addBookmarkFromSafariDict:(NSDictionary *)aKid];
    else if ([[aKid objectForKey:SafariTypeKey] isEqualToString:SafariList])
      noErr = [self addBookmarkFolderFromSafariDict:(NSDictionary *)aKid];
    // might also be a WebBookmarkTypeProxy - we'll ignore those
  }
  if ([[aDict objectForKey:SafariAutoTab] boolValue])
    [self setIsGroup:YES];
  return noErr;
}

-(BOOL) readCaminoXML:(CFXMLTreeRef)aTreeRef
{
  CFXMLTreeRef childTreeRef;
  CFXMLNodeRef myNode, childNodeRef;
  CFXMLElementInfo* elementInfoPtr;
  BOOL noErr = YES;
  myNode = CFXMLTreeGetNode(aTreeRef);
  if (myNode) {
    // Process our info - we load info into tempMuteString,
    // then send a cleaned up version to temp string, which gets
    // dropped into appropriate variable
    if (CFXMLNodeGetTypeCode(myNode)==kCFXMLNodeTypeElement){
      elementInfoPtr = (CFXMLElementInfo*)CFXMLNodeGetInfoPtr(myNode);
      if (elementInfoPtr) {
        NSDictionary* attribDict = (NSDictionary*)elementInfoPtr->attributes;
        [self setTitle:[[attribDict objectForKey:CaminoNameKey] stringByRemovingAmpEscapes]];
        [self setDescription:[[attribDict objectForKey:CaminoDescKey] stringByRemovingAmpEscapes]];
        if ([[attribDict objectForKey:CaminoTypeKey] isEqualToString:CaminoToolbarKey])
          [self setIsToolbar:YES];
        if ([[attribDict objectForKey:CaminoGroupKey] isEqualToString:CaminoTrueKey])
          [self setIsGroup:YES];
        // Process children
        unsigned i = 0;
        while((childTreeRef = CFTreeGetChildAtIndex(aTreeRef,i++)) && noErr) {
          childNodeRef = CFXMLTreeGetNode(childTreeRef);
          if (childNodeRef) {
            NSString *tempString = (NSString *)CFXMLNodeGetString(childNodeRef);
            if ([tempString isEqualToString:CaminoBookmarkKey])
              noErr = [self addBookmarkFromXML:childTreeRef];
            else if ([tempString isEqualToString:CaminoFolderKey])
              noErr = [self addBookmarkFolderFromXML:childTreeRef];
          } 
        }
      } else  {
        NSLog(@"BookmarkFolder: readCaminoXML- elementInfoPtr null - children not imported");
        noErr = NO;
      }
    } else {
      NSLog(@"BookmarkFolder: readCaminoXML - should be, but isn't a CFXMLNodeTypeElement");
      noErr = NO;
    }
  } else {
    NSLog(@"BookmarkFolder: readCaminoXML - myNode null - bookmark not imported");
    noErr = NO;
  }
  return noErr;
}


-(NSDictionary *)writeNativeDictionary
{
  if (![self isSmartFolder]) {
    id item;
    NSMutableArray* children = [NSMutableArray array];
    NSEnumerator* enumerator = [mChildArray objectEnumerator];
    //get chillins first
    while ((item = [enumerator nextObject])) {
      id aDict = [item writeNativeDictionary];
      if (aDict)
        [children addObject:aDict];
    }
    return [NSDictionary dictionaryWithObjectsAndKeys:
      mTitle,BMTitleKey,
      mDescription,BMFolderDescKey,
      mKeyword,BMFolderKeywordKey,
      mSpecialFlag,BMFolderTypeKey,
      children,BMChildrenKey, nil];
  }
  return nil;
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
  for (unsigned i = 0;i<aPad;i++) 
    [padString insertString:@"    " atIndex:0];
  // Normal folders
  if ((![self isToolbar] && ![self isRoot] && ![self isSmartFolder])) {
    NSString *formatString;
    if ([self isGroup])
      formatString = @"%@<DT><H3 FOLDER_GROUP=\"true\">%@</H3>\n%@<DL><p>\n";
    else
      formatString = @"%@<DT><H3>%@</H3>\n%@<DL><p>\n";
    htmlString = [NSString stringWithFormat:formatString, padString,[mTitle stringByAddingAmpEscapes],padString];
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
     return [NSString string];
  NSEnumerator* enumerator = [mChildArray objectEnumerator];
  //get chillins first
  while ((item = [enumerator nextObject]))
    htmlString = [htmlString stringByAppendingString:[item writeHTML:aPad+1]];
  return [htmlString stringByAppendingString:[NSString stringWithFormat:@"%@</DL><p>\n",padString]];
}

#pragma mark -
//
// Scripting methods - thanks for the sketch example, Apple
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

- (NSArray *)folderItemWithClass:(Class)theClass {
  NSArray *kiddies = [self childArray];
  NSMutableArray *result = [NSMutableArray array];
  unsigned i, c = [kiddies count];
  id curItem;
  for (i=0; i<c; i++) {
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
  return [self folderItemWithClass:[Bookmark class]];
}

-(NSArray *)childFolders
{
  return [self folderItemWithClass:[BookmarkFolder class]];
}

// all the insert, add, remove, replace methods for the 3 sets of items in a bookmark array
-(void) insertInChildArray:(BookmarkItem *)aItem atIndex:(unsigned)aIndex
{
  NSLog(@"insert child array item = %@, index = %i",[aItem title],aIndex);
  [self insertChild:aItem atIndex:aIndex isMove:NO];
}

-(void) addInChildArray:(BookmarkItem *)aItem
{
  NSLog(@"add child array item = %@",[aItem title]);
  [self insertChild:aItem];
}

-(void) removeFromChildArrayAtIndex:(unsigned)aIndex
{
  NSLog(@"remove child array item = %i",aIndex);
  BookmarkItem* aKid = [[self childArray] objectAtIndex:aIndex];
  [self deleteChild:aKid];
}

-(void)replaceInChildArray:(BookmarkItem *)aItem atIndex:(unsigned)aIndex
{
  NSLog(@"replace child array item = %@, index = %i",[aItem title],aIndex);
  [self removeFromChildArrayAtIndex:aIndex];
  [self insertChild:aItem atIndex:aIndex isMove:NO];
}
//
// child bookmark stuff
//
-(void) insertInChildBookmarks:(Bookmark *)aItem atIndex:(unsigned)aIndex
{
  NSLog(@"in insert bookmarks title = %@, index = %i",[aItem title], aIndex);
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
  NSLog(@"in add bookmark title = %@",[aItem title]);
  NSArray *bookmarkArray = [self childBookmarks];
  Bookmark *aBookmark = [bookmarkArray lastObject];
  unsigned index = [[self childArray] indexOfObject:aBookmark];
  [self insertChild:aItem atIndex:++index isMove:NO];
}

-(void) removeFromChildBookmarksAtIndex:(unsigned)aIndex
{
  NSLog(@"in remove bookmark taking index = %i",aIndex);
  Bookmark* aKid = [[self childBookmarks] objectAtIndex:aIndex];
  [self deleteChild:aKid];
}

-(void)replaceInChildBookmarks:(Bookmark *)aItem atIndex:(unsigned)aIndex
{
  NSLog(@"in replace bookmark title = %@, index = %i",[aItem title],aIndex);
  [self removeFromChildBookmarksAtIndex:aIndex];
  [self insertInChildBookmarks:aItem atIndex:aIndex];
}
//
// child bookmark folder stuff
//
-(void) insertInChildFolders:(BookmarkFolder *)aItem atIndex:(unsigned)aIndex
{
  NSLog(@"in insert folder title = %@, index = %i",[aItem title], aIndex);

  NSArray *folderArray = [self childFolders];
  BookmarkFolder *aFolder;
  unsigned realIndex;
  if (aIndex < [folderArray count])  {
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
  NSLog(@"in add folder title = %@",[aItem title]);

  NSArray *folderArray = [self childFolders];
  BookmarkFolder *aFolder = [folderArray lastObject];
  unsigned index = [[self childArray] indexOfObject:aFolder];
  [self insertChild:aItem atIndex:++index isMove:NO];
}

-(void) removeFromChildFoldersAtIndex:(unsigned)aIndex
{
  NSLog(@"in remove folder taking index = %i",aIndex);
  BookmarkFolder* aKid = [[self childFolders] objectAtIndex:aIndex];
  [self deleteChild:aKid];
}

-(void)replaceInChildFolders:(BookmarkFolder *)aItem atIndex:(unsigned)aIndex
{
  NSLog(@"in replace folder title = %@, index = %i",[aItem title],aIndex);
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
        [baseKey isEqualToString:@"childFolders"] ){
      unsigned baseIndex;
      id baseObject = [baseSpec objectsByEvaluatingWithContainers:self];
      if ([baseObject isKindOfClass:[NSArray class]]) {
        int baseCount = [baseObject count];
        if (baseCount == 0) 
          baseObject = nil;
        else {
          if (relPos == NSRelativeBefore){
            baseObject = [baseObject objectAtIndex:0];
          } else {
            baseObject = [baseObject objectAtIndex:(baseCount-1)];
          }
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
      while ((baseIndex >= 0) && (baseIndex < kiddiesCount)) {
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
