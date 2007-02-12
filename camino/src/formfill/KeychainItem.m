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
 * The Original Code is Camino code.
 *
 * The Initial Developer of the Original Code is
 * Stuart Morgan
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Morgan <stuart.morgan@gmail.com>
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

#import "KeychainItem.h"

@interface KeychainItem(Private)
- (KeychainItem*)initWithRef:(SecKeychainItemRef)ref;
- (void)loadKeychainData;
- (BOOL)setAttributeType:(SecKeychainAttrType)type toString:(NSString*)value;
- (BOOL)setAttributeType:(SecKeychainAttrType)type toValue:(void*)valuePtr withLength:(UInt32)length;
@end

@implementation KeychainItem

+ (KeychainItem*)keychainItemForHost:(NSString*)host
                                port:(UInt16)port
                            protocol:(SecProtocolType)protocol
                  authenticationType:(SecAuthenticationType)authType
{
  SecKeychainItemRef itemRef;
  const char* serverName = host ? [host UTF8String] : NULL;
  UInt32 serverNameLength = serverName ? strlen(serverName) : 0;
  OSStatus result = SecKeychainFindInternetPassword(NULL, serverNameLength, serverName, 0, NULL, 0, NULL,
                                                    0, NULL, port, protocol, authType,
                                                    NULL, NULL, &itemRef);
  if (result != noErr)
      return nil;

  return [[[KeychainItem alloc] initWithRef:itemRef] autorelease];
}

// Returns an array of all keychain items matching the given criteria.
+ (NSArray*)allKeychainItemsForHost:(NSString*)host
                               port:(UInt16)port
                           protocol:(SecProtocolType)protocol
                 authenticationType:(SecAuthenticationType)authType
                            creator:(OSType)creator
{
  SecKeychainAttribute attributes[5];
  unsigned int usedAttributes = 0;

  const char* hostString = [host UTF8String];
  if (hostString) {
    attributes[usedAttributes].tag = kSecServerItemAttr;
    attributes[usedAttributes].data = (void*)(hostString);
    attributes[usedAttributes].length = strlen(hostString);
    ++usedAttributes;
  }
  if (protocol) {
    attributes[usedAttributes].tag = kSecProtocolItemAttr;
    attributes[usedAttributes].data = (void*)(&protocol);
    attributes[usedAttributes].length = sizeof(protocol);
    ++usedAttributes;
  }
  if (authType) {
    attributes[usedAttributes].tag = kSecAuthenticationTypeItemAttr;
    attributes[usedAttributes].data = (void*)(&authType);
    attributes[usedAttributes].length = sizeof(authType);
    ++usedAttributes;
  }
  if (creator) {
    attributes[usedAttributes].tag = kSecCreatorItemAttr;
    attributes[usedAttributes].data = (void*)(&creator);
    attributes[usedAttributes].length = sizeof(creator);
    ++usedAttributes;
  }

  SecKeychainAttributeList searchCriteria;
  searchCriteria.count = usedAttributes;
  searchCriteria.attr = attributes;

  SecKeychainSearchRef searchRef;
  OSStatus status = SecKeychainSearchCreateFromAttributes(NULL, kSecInternetPasswordItemClass, &searchCriteria, &searchRef);
  if (status != noErr) {
    NSLog(@"Keychain search for host '%@' failed", host);
    return nil;
  }

  NSMutableArray* matchingItems = [NSMutableArray array];
  SecKeychainItemRef keychainItemRef;
  while ((status = SecKeychainSearchCopyNext(searchRef, &keychainItemRef)) == noErr) {
    [matchingItems addObject:[[[KeychainItem alloc] initWithRef:keychainItemRef] autorelease]];
  }
  CFRelease(searchRef);

  // Safari (and possibly others) store some things without ports, so we always
  // search without a port. If any results match the port we wanted then
  // discard all the other results; if not discard any non-generic entries
  if (port != kAnyPort) {
    NSMutableArray* exactMatches = [NSMutableArray array];
    for (int i = [matchingItems count] - 1; i >= 0; --i) {
      KeychainItem* item = [matchingItems objectAtIndex:i];
      if ([item port] == port)
        [exactMatches insertObject:item atIndex:0];
      else if ([item port] != kAnyPort)
        [matchingItems removeObjectAtIndex:i];
    }
    if ([exactMatches count] > 0)
      matchingItems = exactMatches;
  }

  return matchingItems;
}

+ (KeychainItem*)addKeychainItemForHost:(NSString*)host
                                   port:(UInt16)port
                               protocol:(SecProtocolType)protocol
                     authenticationType:(SecAuthenticationType)authType
                           withUsername:(NSString*)username
                               password:(NSString*)password
{
  const char* serverName = [host UTF8String];
  UInt32 serverLength = serverName ? strlen(serverName) : 0;
  const char* accountName = [username UTF8String];
  UInt32 accountLength = accountName ? strlen(accountName) : 0;
  const char* passwordData = [password UTF8String];
  UInt32 passwordLength = passwordData ? strlen(passwordData) : 0;
  SecKeychainItemRef keychainItemRef;
  OSStatus result = SecKeychainAddInternetPassword(NULL, serverLength, serverName, 0, NULL,
                                                   accountLength, accountName, 0, NULL,
                                                   (UInt16)port, protocol, authType,
                                                   passwordLength, passwordData, &keychainItemRef);
  if (result != noErr) {
    NSLog(@"Couldn't add keychain item");
    return nil;
  }

  KeychainItem* item = [[[KeychainItem alloc] initWithRef:keychainItemRef] autorelease];
  return item;
}

- (KeychainItem*)initWithRef:(SecKeychainItemRef)ref
{
  if ((self = [super init])) {
    mKeychainItemRef = ref;
    mDataLoaded = NO;
  }
  return self;
}

- (void)dealloc
{
  if (mKeychainItemRef)
    CFRelease(mKeychainItemRef);
  [mUsername release];
  [mPassword release];
  [mHost release];
  [mComment release];
  [super dealloc];
}

- (void)loadKeychainData
{
  if (!mKeychainItemRef)
    return;
  SecKeychainAttributeInfo attrInfo;
  UInt32 tags[7];
  tags[0] = kSecAccountItemAttr;
  tags[1] = kSecServerItemAttr;
  tags[2] = kSecPortItemAttr;
  tags[3] = kSecProtocolItemAttr;
  tags[4] = kSecAuthenticationTypeItemAttr;
  tags[5] = kSecCreatorItemAttr;
  tags[6] = kSecCommentItemAttr;
  attrInfo.count = sizeof(tags)/sizeof(UInt32);
  attrInfo.tag = tags;
  attrInfo.format = NULL;

  SecKeychainAttributeList *attrList;
  UInt32 passwordLength;
  char* passwordData;
  OSStatus result = SecKeychainItemCopyAttributesAndData(mKeychainItemRef, &attrInfo, NULL, &attrList,
                                                         &passwordLength, (void**)(&passwordData));

  [mUsername autorelease];
  mUsername = nil;
  [mPassword autorelease];
  mPassword = nil;
  [mHost autorelease];
  mHost = nil;
  [mComment autorelease];
  mComment = nil;

  if (result != noErr) {
    NSLog(@"Couldn't load keychain data");
    mUsername = [[NSString alloc] init];
    mPassword = [[NSString alloc] init];
    mHost = [[NSString alloc] init];
    mComment = [[NSString alloc] init];
    return;
  }

  for (unsigned int i = 0; i < attrList->count; i++) {
    SecKeychainAttribute attr = attrList->attr[i];
    if (attr.tag == kSecAccountItemAttr)
      mUsername = [[NSString alloc] initWithBytes:(char*)(attr.data) length:attr.length encoding:NSUTF8StringEncoding];
    else if (attr.tag == kSecServerItemAttr)
      mHost = [[NSString alloc] initWithBytes:(char*)(attr.data) length:attr.length encoding:NSUTF8StringEncoding];
    else if (attr.tag == kSecCommentItemAttr)
      mComment = [[NSString alloc] initWithBytes:(char*)(attr.data) length:attr.length encoding:NSUTF8StringEncoding];
    else if (attr.tag == kSecPortItemAttr)
      mPort = *((UInt16*)(attr.data));
    else if (attr.tag == kSecProtocolItemAttr)
      mProtocol = *((SecProtocolType*)(attr.data));
    else if (attr.tag == kSecAuthenticationTypeItemAttr)
      mAuthenticationType = *((SecAuthenticationType*)(attr.data));
    else if (attr.tag == kSecCreatorItemAttr)
      mCreator = attr.data ? *((OSType*)(attr.data)) : 0;
  }
  mPassword = [[NSString alloc] initWithBytes:passwordData length:passwordLength encoding:NSUTF8StringEncoding];
  SecKeychainItemFreeAttributesAndData(attrList, (void*)passwordData);
  mDataLoaded = YES;
}

- (NSString*)username
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mUsername;
}

- (NSString*)password
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mPassword;
}

- (void)setUsername:(NSString*)username password:(NSString*)password
{
  SecKeychainAttribute user;
  user.tag = kSecAccountItemAttr;
  const char* usernameString = [username UTF8String];
  user.data = (void*)usernameString;
  user.length = user.data ? strlen(user.data) : 0;
  SecKeychainAttributeList attrList;
  attrList.count = 1;
  attrList.attr = &user;
  const char* passwordData = [password UTF8String];
  UInt32 passwordLength = passwordData ? strlen(passwordData) : 0;
  if (SecKeychainItemModifyAttributesAndData(mKeychainItemRef, &attrList, passwordLength, passwordData) != noErr)
    NSLog(@"Couldn't update keychain item user and password for %@", username);
  else {
    [mUsername autorelease];
    mUsername = [username copy];
    [mPassword autorelease];
    mPassword = [password copy];
  }
}

- (NSString*)host
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mHost;
}

- (void)setHost:(NSString*)host
{
  if ([self setAttributeType:kSecServerItemAttr toString:host]) {
    [mHost autorelease];
    mHost = [host copy];
  }
  else {
    NSLog(@"Couldn't update keychain item host");
  }
}

- (UInt16)port
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mPort;
}

- (void)setPort:(UInt16)port
{
  if ([self setAttributeType:kSecPortItemAttr toValue:&port withLength:sizeof(UInt16)])
    mPort = port;
  else
    NSLog(@"Couldn't update keychain item port");
}

- (SecProtocolType)protocol
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mProtocol;
}

- (void)setProtocol:(SecProtocolType)protocol
{
  if ([self setAttributeType:kSecProtocolItemAttr toValue:&protocol withLength:sizeof(SecProtocolType)])
    mProtocol = protocol;
  else
    NSLog(@"Couldn't update keychain item protocol");
}

- (SecAuthenticationType)authenticationType
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mAuthenticationType;
}

- (void)setAuthenticationType:(SecAuthenticationType)authType
{
  if ([self setAttributeType:kSecAuthenticationTypeItemAttr toValue:&authType withLength:sizeof(SecAuthenticationType)])
    mAuthenticationType = authType;
  else
    NSLog(@"Couldn't update keychain item auth type");
}

- (OSType)creator
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mCreator;
}

- (void)setCreator:(OSType)creator
{
  if ([self setAttributeType:kSecCreatorItemAttr toValue:&creator withLength:sizeof(OSType)])
    mCreator = creator;
  else
    NSLog(@"Couldn't update keychain item creator");
}

- (NSString*)comment
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mComment;
}

- (void)setComment:(NSString*)comment
{
  if ([self setAttributeType:kSecCommentItemAttr toString:comment]) {
    [mComment autorelease];
    mComment = [comment copy];
  }
  else {
    NSLog(@"Couldn't update keychain item comment");
  }
}

- (BOOL)setAttributeType:(SecKeychainAttrType)type toString:(NSString*)value {
  const char* cString = [value UTF8String];
  UInt32 length = cString ? strlen(cString) : 0;
  return [self setAttributeType:type toValue:(void*)cString withLength:length];
}

- (BOOL)setAttributeType:(SecKeychainAttrType)type toValue:(void*)valuePtr withLength:(UInt32)length
{
  SecKeychainAttribute attr;
  attr.tag = type;
  attr.data = valuePtr;
  attr.length = length;
  SecKeychainAttributeList attrList;
  attrList.count = 1;
  attrList.attr = &attr;
  return (SecKeychainItemModifyAttributesAndData(mKeychainItemRef, &attrList, 0, NULL) == noErr);
}

- (void)removeFromKeychain
{
  SecKeychainItemDelete(mKeychainItemRef);
}

@end
