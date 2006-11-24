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
- (BOOL)setAttributeType:(SecKeychainAttrType)type toValue:(void*)valuePtr withLength:(UInt32)length;
@end

@implementation KeychainItem

+ (KeychainItem*)keychainItemForHost:(NSString*)host port:(UInt16)port protocol:(SecProtocolType)protocol authenticationType:(SecAuthenticationType)authType
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
  [super dealloc];
}

- (void)loadKeychainData
{
  if (!mKeychainItemRef)
    return;
  SecKeychainAttributeInfo attrInfo;
  UInt32 tags[3];
  tags[0] = kSecAccountItemAttr;
  tags[1] = kSecProtocolItemAttr;
  tags[2] = kSecAuthenticationTypeItemAttr;
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

  if (result != noErr) {
    NSLog(@"Couldn't load keychain data");
    mUsername = [[NSString alloc] init];
    mPassword = [[NSString alloc] init];
    return;
  }

  for (unsigned int i = 0; i < attrList->count; i++) {
    SecKeychainAttribute attr = attrList->attr[i];
    if (attr.tag == kSecAccountItemAttr)
      mUsername = [[NSString alloc] initWithBytes:(char*)(attr.data) length:attr.length encoding:NSUTF8StringEncoding];
    else if (attr.tag == kSecProtocolItemAttr)
      mProtocol = *((SecProtocolType*)(attr.data));
    else if (attr.tag == kSecAuthenticationTypeItemAttr)
      mAuthenticationType = *((SecAuthenticationType*)(attr.data));
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

- (SecProtocolType)protocol
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mProtocol;
}

- (void)setProtocol:(SecProtocolType)protocol
{
  if(![self setAttributeType:kSecProtocolItemAttr toValue:&protocol withLength:sizeof(SecProtocolType)])
    NSLog(@"Couldn't update keychain item protocol");
  else
    mProtocol = protocol;
}

- (SecAuthenticationType)authenticationType
{
  if (!mDataLoaded)
    [self loadKeychainData];
  return mAuthenticationType;
}

- (void)setAuthenticationType:(SecAuthenticationType)authType
{
  if(![self setAttributeType:kSecAuthenticationTypeItemAttr toValue:&authType withLength:sizeof(SecAuthenticationType)])
    NSLog(@"Couldn't update keychain item auth type");
  else
    mAuthenticationType = authType;
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
