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


@implementation BookmarkItem
//Initialization
-(id) init
{
  if ((self = [super init]))
  {
//    NSString *tempString = [[NSString alloc] init];
    mParent = NULL;
    mTitle = [[NSString alloc] init];
    mKeyword = [[NSString alloc] init];
    mDescription = [[NSString alloc] init];
    // if we set the icon here, we will get a memory leak.  so don't.
    // subclass will provide icon.
    mIcon = NULL; 
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
  return doppleganger;
}

-(void)dealloc
{
  [mTitle release];
  [mDescription release];
  [mKeyword release];
  [mIcon release];
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

-(void) setIcon:(NSImage *)aIcon
{
  if (!aIcon)
    return;
  [aIcon retain];
  [mIcon release];
  mIcon = aIcon;
  [self itemUpdatedNote];
}

-(void) itemUpdatedNote
{
  NSNotification *note = [NSNotification notificationWithName:BookmarkItemChangedNotification object:self userInfo:nil];
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc postNotification:note];
  return;
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

-(NSString *)writeHTML:(unsigned)aPad
{
  return [NSString string];
}


@end



