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

#import <Foundation/Foundation.h>
#import <Security/Security.h>

@interface KeychainItem : NSObject
{
 @private
  SecKeychainItemRef mKeychainItemRef; // strong
  BOOL mDataLoaded;
  NSString* mUsername;                 // strong
  NSString* mPassword;                 // strong
  NSString* mHost;                     // strong
  NSString* mComment;                  // strong
  NSArray* mSecurityDomains;           // strong
  SecProtocolType mPort;
  SecProtocolType mProtocol;
  SecAuthenticationType mAuthenticationType;
  OSType mCreator;
}

// Returns the first keychain item matching the given criteria.
+ (KeychainItem*)keychainItemForHost:(NSString*)host
                                port:(UInt16)port
                            protocol:(SecProtocolType)protocol
                  authenticationType:(SecAuthenticationType)authType;

// Returns an array of all keychain items matching the given criteria.
// Pass 0/nil (or kAnyPort for port) to ignore a given field.
+ (NSArray*)allKeychainItemsForHost:(NSString*)host
                               port:(UInt16)port
                           protocol:(SecProtocolType)protocol
                 authenticationType:(SecAuthenticationType)authType
                            creator:(OSType)creator;

// Creates and returns a new keychain item
+ (KeychainItem*)addKeychainItemForHost:(NSString*)host
                                   port:(UInt16)port
                               protocol:(SecProtocolType)protocol
                     authenticationType:(SecAuthenticationType)authType
                           withUsername:(NSString*)username
                               password:(NSString*)password;

- (NSString*)username;
- (NSString*)password;
- (void)setUsername:(NSString*)username password:(NSString*)password;
- (NSString*)host;
- (void)setHost:(NSString*)host;
- (UInt16)port;
- (void)setPort:(UInt16)port;
- (SecProtocolType)protocol;
- (void)setProtocol:(SecProtocolType)protocol;
- (SecAuthenticationType)authenticationType;
- (void)setAuthenticationType:(SecAuthenticationType)authType;
- (OSType)creator;
- (void)setCreator:(OSType)creator;
- (NSString*)comment;
- (void)setComment:(NSString*)comment;
- (void)setSecurityDomains:(NSArray*)securityDomains;
- (NSArray*)securityDomains;

- (void)removeFromKeychain;

@end
