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

/*
 * By the way - this is a total hack.  Somebody should really do this in
 * a more intelligent manner.
 */
#import "KindaSmartFolderManager.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "BookmarkManager.h"
#import "AddressBookManager.h"
#import "NetworkServices.h"
#import "BookmarksClient.h"

@interface KindaSmartFolderManager (Private) <NetworkServicesClient, BookmarksClient> 
-(void)addBookmark:(Bookmark *)aBookmark toSmartFolder:(BookmarkFolder *)aFolder;
-(void)removeBookmark:(Bookmark *)aBookmark fromSmartFolder:(BookmarkFolder *)aFolder;
-(void)checkForNewTop10:(Bookmark *)aBookmark;
-(void)setupAddressBook;
-(void)rebuildTop10List;
@end

const unsigned kNumTop10Items = 10;   // well, 10, duh!

@implementation KindaSmartFolderManager

-(id)initWithBookmarkManager:(BookmarkManager *)manager
{
  if ((self = [super init])) {
    // retain all our smart folders, just to be safe
    mTop10Folder = [[manager top10Folder] retain];
    mAddressBookFolder = [[manager addressBookFolder] retain];
    mRendezvousFolder = [[manager rendezvousFolder] retain];

    // client notifications
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:nil];
  }
  return self;
}

-(void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mTop10Folder release];
  [mAddressBookFolder release];
  [mRendezvousFolder release];
  [mAddressBookManager release];
  [super dealloc];
}

-(void)postStartupInitialization:(BookmarkManager *)manager
{
  // register for Rendezvous - if we need to
  if (mRendezvousFolder) {
    [NetworkServices sharedNetworkServices];
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(availableServicesChanged:) name:NetworkServicesAvailableServicesChanged object:nil];
    [nc addObserver:self selector:@selector(serviceResolved:) name:NetworkServicesResolutionSuccess object:nil];
  }

  if (mAddressBookFolder)
    [self setupAddressBook];

  [self rebuildTop10List];
}

-(void) setupAddressBook
{
  mAddressBookManager = [[AddressBookManager alloc] initWithFolder:mAddressBookFolder];
  if (mAddressBookManager)
    [mAddressBookFolder release];
}

//
// flush top 10 list & rebuild from scratch
//
-(void)rebuildTop10List
{
  unsigned i, count = [mTop10Folder count];
  for (i = 0; i < count; i++)
    [mTop10Folder deleteFromSmartFolderChildAtIndex:0];

  BookmarkManager *manager = [BookmarkManager sharedBookmarkManager];
  BookmarkItem* curItem;

  // We don't use rootBookmarks, because that will include the top10 folder,
  // so we do the menu and toolbar folders manually. This also allows us to
  // skip Rendezvous and Address Book folders, for which we don't store
  // visit counts anyway. However, we will skip any other custom bookmark
  // container folders that the user has created.
  NSEnumerator* bookmarksEnum = [[manager bookmarkMenuFolder] objectEnumerator];
  while ((curItem = [bookmarksEnum nextObject]))
  {
    if ([curItem isKindOfClass:[Bookmark class]])
      [self checkForNewTop10:curItem];
  }

  bookmarksEnum = [[manager toolbarFolder] objectEnumerator];
  while ((curItem = [bookmarksEnum nextObject]))
  {
    if ([curItem isKindOfClass:[Bookmark class]])
      [self checkForNewTop10:curItem];
  }
}

-(void)checkForNewTop10:(Bookmark *)aBookmark
{
  NSMutableArray* top10ItemsArray = [mTop10Folder childArray];

//  NSLog(@"checkForNewTop10 %@ (%d items)", aBookmark, [top10ItemsArray count]);

  // if it's already in the list
  unsigned curIndex   = [top10ItemsArray indexOfObjectIdenticalTo:aBookmark];
  unsigned visitCount = [aBookmark numberOfVisits];

  // if it's not in the list, and the visit count is zero, nothing to do
  if (curIndex == NSNotFound && visitCount == 0)
    return;

  // we assume the list is sorted here
  unsigned currentMinVisits = 1;
  if ([top10ItemsArray count] == kNumTop10Items)
    currentMinVisits = [[top10ItemsArray lastObject] numberOfVisits];

  if (curIndex != NSNotFound) // it's already in the list
  {
    if (visitCount < currentMinVisits)
    {
      // the item dropped off the list. rather than grovel for the next highest item, just rebuild the list
      // (this could be optimized)
      [self rebuildTop10List]; // XXX potential recursion!
    }
    else
    {
      // just resort
      [mTop10Folder sortChildrenUsingSelector:@selector(compareForTop10:sortDescending:)
                                  reverseSort:YES
                                     sortDeep:NO
                                     undoable:NO];
    }
  }
  else if (visitCount >= currentMinVisits)
  {
    // enter it into the list using insertion sort. it will go before other items with the same visit
    // count (thus maintaining the visit count/last visit sort).
    unsigned numItems = [top10ItemsArray count];
    int insertionIndex = -1;
      
    NSString* newItemURL = [aBookmark url];
    NSNumber* reverseSort = [NSNumber numberWithBool:YES];
    
    // we check the entire list to look for items with a duplicate url
    for (unsigned i = 0; i < numItems; i ++)
    {
      Bookmark* curChild = [top10ItemsArray objectAtIndex:i];
      if ([newItemURL isEqualToString:[curChild url]])
        return;

      // add before the first item with the same or lower visit count
      if (([aBookmark compareForTop10:curChild sortDescending:reverseSort] != NSOrderedDescending) && insertionIndex == -1)
        insertionIndex = i;
    }
    
    if (insertionIndex == -1 && [top10ItemsArray count] < kNumTop10Items)
      insertionIndex = [top10ItemsArray count];

    if (insertionIndex != -1)
    {
      [mTop10Folder insertIntoSmartFolderChild:aBookmark atIndex:insertionIndex];
      if ([top10ItemsArray count] > kNumTop10Items)
        [mTop10Folder deleteFromSmartFolderChildAtIndex:[top10ItemsArray count] - 1];
    }
  }
}


//
// if we don't already have it, add it
//
-(void)addBookmark:(Bookmark *)aBookmark toSmartFolder:(BookmarkFolder *)aFolder
{
  if (aFolder == nil) return; //if the smart folder isn't enabled, we're done
  unsigned index = [aFolder indexOfObjectIdenticalTo:aBookmark];
  if (index == NSNotFound)
    [aFolder insertIntoSmartFolderChild:aBookmark];
}

//
// if we have this item, remove it
// 
-(void)removeBookmark:(Bookmark *)anItem fromSmartFolder:(BookmarkFolder *)aFolder
{
  if (aFolder == nil) return; //if the smart folder isn't enabled, we're done
  unsigned index = [aFolder indexOfObjectIdenticalTo:anItem];
  if (index != NSNotFound)
    [aFolder deleteFromSmartFolderChildAtIndex:index];
}

#pragma mark -

static int SortByProtocolAndName(NSDictionary* item1, NSDictionary* item2, void *context)
{
  NSComparisonResult protocolCompare = [[item1 objectForKey:@"name"] compare:[item2 objectForKey:@"name"] options:NSCaseInsensitiveSearch];
  if (protocolCompare != NSOrderedSame)
    return protocolCompare;

  return [[item1 objectForKey:@"protocol"] compare:[item2 objectForKey:@"protocol"] options:NSCaseInsensitiveSearch];
}

- (void)availableServicesChanged:(NSNotification *)note
{
  [mRendezvousFolder setAccumulateUpdateNotifications:YES];

  // empty the rendezvous folder
  unsigned int i, count = [mRendezvousFolder count];
  for (i = 0; i < count; i++)
    [mRendezvousFolder deleteChild:[mRendezvousFolder objectAtIndex:0]];

  NetworkServices *netserv = [note object];
  NSEnumerator* keysEnumerator = [netserv serviceEnumerator];
  NSMutableArray* servicesArray = [[NSMutableArray alloc] initWithCapacity:10];
  id key;
  while ((key = [keysEnumerator nextObject]))
  {
    NSDictionary* serviceDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                                    key, @"id",
                   [netserv serviceName:[key intValue]], @"name",
               [netserv serviceProtocol:[key intValue]], @"protocol",
                                                         nil];
    [servicesArray addObject:serviceDict];
  }

  if ([servicesArray count] != 0)
  {
    // sort on protocol, then name
    [servicesArray sortUsingFunction:SortByProtocolAndName context:NULL];
    
    // make bookmarks
    unsigned int numServices = [servicesArray count];
    for (i = 0; i < numServices; i ++)
    {
      NSDictionary* serviceDict = [servicesArray objectAtIndex:i];
      NSString* itemName = [[serviceDict objectForKey:@"name"] stringByAppendingString:NSLocalizedString([serviceDict objectForKey:@"protocol"], @"")];
      
      RendezvousBookmark* serviceBookmark = [[RendezvousBookmark alloc] initWithServiceID:[[serviceDict objectForKey:@"id"] intValue]];
      [serviceBookmark setTitle:itemName];
      [mRendezvousFolder appendChild:serviceBookmark];
      [serviceBookmark release];
    }
  }
  [servicesArray release];

  [mRendezvousFolder setAccumulateUpdateNotifications:NO];
}

- (void)serviceResolved:(NSNotification *)note
{
  NSDictionary *dict = [note userInfo];
  id client = [dict objectForKey:NetworkServicesClientKey];
  if ([client isKindOfClass:[Bookmark class]])
  {
    // I'm not sure why we have to check to see that the client is a child
    // of the rendezvous folder. Maybe just see if it's a RendezvousBookmark?
    NSEnumerator* enumerator = [[mRendezvousFolder childArray] objectEnumerator];
    Bookmark *curChild;
    while ((curChild = [enumerator nextObject]))
    {
      if (curChild == client)
      {
        [client setUrl:[dict objectForKey:NetworkServicesResolvedURLKey]];
        [client setResolved:YES];
      }
    }
  }
  return;
}

- (void)serviceResolutionFailed:(NSNotification *)note
{
  return;
}


#pragma mark -

//
// BookmarkClient protocol
//
- (void)bookmarkAdded:(NSNotification *)note
{
}

//
// need to tell top 10 list
//
- (void)bookmarkRemoved:(NSNotification *)note
{
  BookmarkItem *anItem = [[note userInfo] objectForKey:BookmarkFolderChildKey];
  if (![anItem parent] && [anItem isKindOfClass:[Bookmark class]]) {
    [self removeBookmark:anItem fromSmartFolder:mTop10Folder];
  }
}

- (void)bookmarkChanged:(NSNotification *)note
{
  BOOL visitCountChanged = [BookmarkItem bookmarkChangedNotificationUserInfo:[note userInfo] containsFlags:kBookmarkItemNumVisitsChangedMask];
  if (visitCountChanged)
  {
    BookmarkItem *anItem = [note object];
    if ([anItem isKindOfClass:[Bookmark class]])
      [self checkForNewTop10:anItem];
  }
}

@end
