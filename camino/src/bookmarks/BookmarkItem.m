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

#import "BookmarkItem.h"

// Notifications
NSString *BookmarkItemChangedNotification = @"bi_cg";
NSString *BookmarkIconChangedNotification = @"bicon_cg";

// all our saving/loading keys
// Safari & Camino plist keys
NSString *BMTitleKey = @"Title";
NSString *BMChildrenKey = @"Children";

// Camino plist keys
NSString *BMFolderDescKey = @"FolderDescription";
NSString *BMFolderTypeKey = @"FolderType";
NSString *BMFolderKeywordKey = @"FolderKeyword";
NSString *BMDescKey = @"Description";
NSString *BMStatusKey = @"Status";
NSString *BMURLKey = @"URL";
NSString *BMKeywordKey = @"Keyword";
NSString *BMLastVisitKey = @"LastVisitedDate";
NSString *BMNumberVisitsKey = @"VisitCount";

// safari keys
NSString *SafariTypeKey = @"WebBookmarkType";
NSString *SafariLeaf = @"WebBookmarkTypeLeaf";
NSString *SafariList = @"WebBookmarkTypeList";
NSString *SafariAutoTab = @"WebBookmarkAutoTab";
NSString *SafariUUIDKey = @"WebBookmarkUUID";
NSString *SafariURIDictKey = @"URIDictionary";
NSString *SafariBookmarkTitleKey = @"title";
NSString *SafariURLStringKey = @"URLString";

// camino XML keys
NSString *CaminoNameKey = @"name";
NSString *CaminoDescKey = @"description";
NSString *CaminoTypeKey = @"type";
NSString *CaminoKeywordKey = @"id";
NSString *CaminoURLKey = @"href";
NSString *CaminoToolbarKey = @"toolbar";
NSString *CaminoDockMenuKey = @"dockmenu";
NSString *CaminoGroupKey = @"group";
NSString *CaminoBookmarkKey = @"bookmark";
NSString *CaminoFolderKey = @"folder";
NSString *CaminoTrueKey = @"true";


@implementation BookmarkItem

static BOOL gSuppressAllUpdates = NO;

//Initialization
-(id) init
{
  if ((self = [super init]))
  {
    mParent = NULL;
    mTitle = [[NSString alloc] init]; //retain count +1
    mKeyword = [mTitle retain]; //retain count +2
    mDescription = [mTitle retain]; //retain count +3! and just 1 allocation.
    mUUID = nil;
    // if we set the icon here, we will get a memory leak.  so don't.
    // subclass will provide icon.
    mIcon = NULL;
    mAccumulateItemChangeUpdates = NO;
  }
  return self;
}
 
-(id) copyWithZone:(NSZone *)zone
{
  //descend from NSObject - so don't call super
  id doppleganger = [[[self class] allocWithZone:zone] init];
  [doppleganger setTitle:[self title]];
  [doppleganger setDescription:[self description]];
  [doppleganger setKeyword:[self keyword]];
  [doppleganger setParent:[self parent]];
  [doppleganger setIcon:[self icon]];
  // do NOT copy the UUID.  It wouldn't be "U" then, would it?
  return doppleganger;
}

-(void)dealloc
{
  [mTitle release];
  [mDescription release];
  [mKeyword release];
  [mIcon release];
  [mUUID release];
  [super dealloc];
}


// Basic properties
-(id) parent
{
  return mParent;
}

-(NSString *) title
{
  return mTitle;
}

-(NSString *) description
{
  return mDescription;
}

-(NSString *) keyword
{
  return mKeyword;
}

// if we ask for a UUID, it means we need
// one.  So generate it if it doesn't exist. 
-(NSString *) UUID
{
  if (!mUUID) {
    CFUUIDRef aUUID = CFUUIDCreate(kCFAllocatorDefault);
    if (aUUID) {
      mUUID =(NSString *)CFUUIDCreateString(kCFAllocatorDefault, aUUID);
      CFRelease (aUUID);
    }
  }
  return mUUID;
}

-(NSImage *)icon
{
  return mIcon;
}

-(BOOL) isChildOfItem:(BookmarkItem *)anItem
{
  if (![[self parent] isKindOfClass:[BookmarkItem class]])
    return NO;
  if ([self parent] == anItem)
    return YES;
  return [[self parent] isChildOfItem:anItem];
}

-(void) setParent:(id) aParent
{
  mParent = aParent;	// no reference on the parent, so it better not disappear on us.
}

-(void) setTitle:(NSString *)aTitle
{
  if (!aTitle)
    return;
  [aTitle retain];
  [mTitle release];
  mTitle = aTitle;
  [self itemUpdatedNote];
}

-(void) setDescription:(NSString *)aDescription
{
  if (!aDescription)
    return;
  [aDescription retain];
  [mDescription release];
  mDescription = aDescription;
}

- (void) setKeyword:(NSString *)aKeyword
{
  if (!aKeyword)
    return;
  [aKeyword retain];
  [mKeyword release];
  mKeyword = aKeyword;
}

-(void) setUUID:(NSString *)aUUID
{
  [aUUID retain];
  [mUUID release];
  mUUID = aUUID;
}

-(void) setIcon:(NSImage *)aIcon
{
  if (!aIcon)
    return;
  [aIcon retain];
  [mIcon release];
  mIcon = aIcon;
  
  if (![BookmarkItem allowNotifications]) return;
  NSNotification *note = [NSNotification notificationWithName:BookmarkIconChangedNotification
                                                       object:self userInfo:nil];
  [[NSNotificationCenter defaultCenter] postNotification:note];
}

// Prevents all NSNotification posting from any BookmarkItem.
// Useful for suppressing all the pointless notifications during load.
+(void) setSuppressAllUpdateNotifications:(BOOL)suppressUpdates
{
  gSuppressAllUpdates = suppressUpdates;
}

+(BOOL) allowNotifications
{
  return !gSuppressAllUpdates;
}

// Helps prevent spamming from itemUpdatedNote:
// calling with YES will prevent itemUpdatedNote from doing anything
// and calling with NO will restore itemUpdatedNote and then call it.
-(void) setAccumulateUpdateNotifications:(BOOL)accumulateUpdates
{
  mAccumulateItemChangeUpdates = accumulateUpdates;
  if (!mAccumulateItemChangeUpdates)
    [self itemUpdatedNote];   //fire an update to cover the updates that weren't sent
}

-(void) itemUpdatedNote
{
  if (gSuppressAllUpdates || mAccumulateItemChangeUpdates) return;
  
  NSNotification *note = [NSNotification notificationWithName:BookmarkItemChangedNotification object:self userInfo:nil];
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc postNotification:note];
}

// stub functions to avoid warning

-(void) refreshIcon
{
}

//Reading/writing to & from disk - all just stubs.

-(BOOL) readNativeDictionary:(NSDictionary *)aDict
{
  return NO;
}

-(BOOL) readSafariDictionary:(NSDictionary *)aDict
{
  return NO;
}

-(BOOL) readCaminoXML:(CFXMLTreeRef)aTreeRef
{
  return NO;
}

-(NSDictionary *)writeNativeDictionary
{
  return [NSDictionary dictionary];
}

-(NSDictionary *)writeSafariDictionary
{
  return [NSDictionary dictionary];
}

-(NSString *)writeHTML:(unsigned)aPad
{
  return [NSString string];
}


@end



