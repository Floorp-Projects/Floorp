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
* By the way - this is a total hack.  Somebody should really do this in
* a more intelligent manner.
*
* ***** END LICENSE BLOCK ***** */
#import "KindaSmartFolderManager.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "BookmarkManager.h"
#import "NetworkServices.h"
#import "BookmarksClient.h"

@interface KindaSmartFolderManager (Private) <NetworkServicesClient, BookmarksClient> 
-(void)addBookmark:(Bookmark *)aBookmark toSmartFolder:(BookmarkFolder *)aFolder;
-(void)removeBookmark:(Bookmark *)aBookmark fromSmartFolder:(BookmarkFolder *)aFolder;
-(void)setupBrokenBookmarks;
-(void)checkForNewTop10:(Bookmark *)aBookmark;
-(void)setupAddressBook;
-(void)rebuildTop10List;
@end

@implementation KindaSmartFolderManager

-(id)initWithBookmarkManager:(BookmarkManager *)manager
{
  if ((self = [super init])) {
    // retain all our smart folders, just to be safe
    mBrokenBookmarkFolder = [[manager brokenLinkFolder] retain];
    mTop10Folder = [[manager top10Folder] retain];
    mAddressBookFolder = [[manager addressBookFolder] retain];
    mRendezvousFolder = [[manager rendezvousFolder] retain];
    mFewestVisits = 0;
    // client notifications
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkIconChangedNotification object:nil];
  }
  return self;
}

-(void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mBrokenBookmarkFolder release];
  [mTop10Folder release];
  [mAddressBookFolder release];
  [mRendezvousFolder release];
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
  if (mBrokenBookmarkFolder)
    [self setupBrokenBookmarks];
  // get top 10 list started
  NSArray *bookmarkArray = [[manager rootBookmarks] allChildBookmarks];
  unsigned i, j = [bookmarkArray count];
  for (i=0; i < j; i++) {
    Bookmark *aBookmark = [bookmarkArray objectAtIndex:i];
    [self checkForNewTop10:aBookmark];
  }
}

-(void) setupBrokenBookmarks
{
  BookmarkManager *manager = [BookmarkManager sharedBookmarkManager];
  NSArray *bookmarkArray = [[manager rootBookmarks] allChildBookmarks];
  unsigned i, j = [bookmarkArray count];
  for (i=0; i < j; i++) {
    Bookmark *aBookmark = [bookmarkArray objectAtIndex:i];
    if ([aBookmark isSick])
      [self addBookmark:aBookmark toSmartFolder:mBrokenBookmarkFolder];
  }
}

// when 10.1 support is dropped, most of "init" method of AddressBookManager goes here.
// we'd also need to add that class' fillAddressBook method to this class
-(void) setupAddressBook
{
  NSBundle *appBundle = [NSBundle mainBundle];
  NSString *addressBookManagerBundlePath = [[appBundle resourcePath] stringByAppendingPathComponent:@"AddressBookManager.bundle"];
  NSBundle *addressBookBundle = [NSBundle bundleWithPath:addressBookManagerBundlePath];
  Class principalClass = [addressBookBundle principalClass];
  mAddressBookManager = [[principalClass alloc] initWithFolder:mAddressBookFolder];
  if (mAddressBookManager)
    [mAddressBookFolder release];
}

//
// flush top 10 list & rebuild from scratch
//
-(void)rebuildTop10List
{
  unsigned i, count = [mTop10Folder count];
  for (i=0;i < count;i++)
    [mTop10Folder deleteFromSmartFolderChildAtIndex:0];
  mFewestVisits = 0;
  // get top 10 list started
  BookmarkManager *manager = [BookmarkManager sharedBookmarkManager];
  NSArray *bookmarkArray = [[manager rootBookmarks] allChildBookmarks];
  count = [bookmarkArray count];
  for (i=0; i < count; i++) {
    Bookmark *aBookmark = [bookmarkArray objectAtIndex:i];
    [self checkForNewTop10:aBookmark];
  }
}


-(void)checkForNewTop10:(Bookmark *)aBookmark
{
  unsigned smallVisit = [aBookmark numberOfVisits];
  if (smallVisit == 0) {
    if ([mTop10Folder indexOfObjectIdenticalTo:aBookmark] != NSNotFound)
      //whoops.  we just cleared the visits on a top 10 item.  rebuild from scratch.
      [self rebuildTop10List];
    return;
  }
  if (([mTop10Folder indexOfObjectIdenticalTo:aBookmark] != NSNotFound))
    return;
  // cycle through list of children
  // find item with fewest visits, mark it for destruction
  // if the URL from the new bookmark is already present in the
  // list, we bail out
  NSMutableArray *childArray = [mTop10Folder childArray];
  unsigned i, kidVisit, count = [childArray count]; //j should be 10
  if ((count >=10) && (smallVisit < mFewestVisits))
    return;
  Bookmark *aKid = nil;
  NSString *newURL = [aBookmark url];
  int doomedKidIndex = -1;
  for (i=0; i< count; i++) {
    aKid = [childArray objectAtIndex:i];
    if ([newURL isEqualToString:[aKid url]])
      return;
    kidVisit = [aKid numberOfVisits];
    if (kidVisit == mFewestVisits)
      doomedKidIndex = i;
    else if (smallVisit > kidVisit)
      smallVisit = kidVisit;
  }
  if ((doomedKidIndex != -1) && (count >= 10))
    [mTop10Folder deleteFromSmartFolderChildAtIndex:doomedKidIndex];
  [mTop10Folder insertIntoSmartFolderChild:aBookmark];
  mFewestVisits = smallVisit;
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
  // empty the rendezvous folder
  unsigned i, count = [mRendezvousFolder count];
  for (i=0; i < count; i++)
    [mRendezvousFolder deleteChild:[mRendezvousFolder objectAtIndex:0]];
  NetworkServices *netserv = [note object];
  NSEnumerator* keysEnumerator = [netserv serviceEnumerator];
  NSMutableArray* servicesArray = [[NSMutableArray alloc] initWithCapacity:10];
  id key;
  while ((key = [keysEnumerator nextObject])) {
    NSDictionary* serviceDict = [NSDictionary dictionaryWithObjectsAndKeys:
      key, @"id",
      [netserv serviceName:[key intValue]], @"name",
      [netserv serviceProtocol:[key intValue]], @"protocol",
      nil];
    [servicesArray addObject:serviceDict];
  }
  if ([servicesArray count] != 0) {
    // sort on protocol, then name
    [servicesArray sortUsingFunction:SortByProtocolAndName context:NULL];
    unsigned count = [servicesArray count];
    for (unsigned int i = 0; i < count; i ++)
    {
      NSDictionary* serviceDict = [servicesArray objectAtIndex:i];
      NSString* itemName = [[serviceDict objectForKey:@"name"] stringByAppendingString:NSLocalizedString([serviceDict objectForKey:@"protocol"], @"")];
      Bookmark *aBookmark = [mRendezvousFolder addBookmark];
      [aBookmark setTitle:itemName];
      [aBookmark setParent:[serviceDict objectForKey:@"id"]];
    }
  }
  [servicesArray release];
}

- (void)serviceResolved:(NSNotification *)note
{
  NSDictionary *dict = [note userInfo];
  id aClient = [dict objectForKey:NetworkServicesClientKey];
  if ([aClient isKindOfClass:[Bookmark class]]) {
    Bookmark *aKid;
    NSEnumerator* enumerator = [[mRendezvousFolder childArray] objectEnumerator];
    while ((aKid = [enumerator nextObject])) {
      if (aKid == aClient)
      {
        [aClient setUrl:[dict objectForKey:NetworkServicesResolvedURLKey]];
        [aClient setParent:mRendezvousFolder];
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
// need to tell top 10 list, broken items
//
- (void)bookmarkRemoved:(NSNotification *)note
{
  BookmarkItem *anItem = [[note userInfo] objectForKey:BookmarkFolderChildKey];
  if (![anItem parent] && [anItem isKindOfClass:[Bookmark class]]) {
    [self removeBookmark:anItem fromSmartFolder:mBrokenBookmarkFolder];
    [self removeBookmark:anItem fromSmartFolder:mTop10Folder];
  }
}

- (void)bookmarkChanged:(NSNotification *)note
{
  BookmarkItem *anItem = [note object];
  if ([anItem isKindOfClass:[Bookmark class]]) {
    [self checkForNewTop10:anItem];
    // see what the status is
    if ([(Bookmark *)anItem isSick])
      [self addBookmark:anItem toSmartFolder:mBrokenBookmarkFolder];
    else {
      [self removeBookmark:anItem fromSmartFolder:mBrokenBookmarkFolder];
    }
  }
}

@end
