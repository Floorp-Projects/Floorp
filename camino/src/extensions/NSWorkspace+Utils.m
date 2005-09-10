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
 *   Simon Fraser <smfr@smfr.org>
 *   Josh Aas <josha@mac.com>
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

#import "NSWorkspace+Utils.h"

@implementation NSWorkspace(CaminoDefaultBrowserAdditions)

- (NSArray*)installedBrowserIdentifiers
{
  // use a set for automatic duplicate elimination
  NSMutableSet* browsersSet = [NSMutableSet setWithCapacity:10];
  
  // using LSCopyApplicationURLsForURL would be nice (its not hidden),
  // but it only exists on Mac OS X >= 10.3
  NSArray* apps;
  _LSCopyApplicationURLsForItemURL([NSURL URLWithString:@"http:"], kLSRolesViewer, &apps);
  [apps autorelease];

  // Put all the browsers into a new array, but only if they also support https and have a bundle ID we can access
  NSEnumerator *appEnumerator = [apps objectEnumerator];
  NSURL* anApp;
  while ((anApp = [appEnumerator nextObject])) {
    Boolean canHandleHTTPS;
    if ((LSCanURLAcceptURL((CFURLRef)[NSURL URLWithString:@"https:"], (CFURLRef)anApp, kLSRolesAll, kLSAcceptDefault, &canHandleHTTPS) == noErr) &&
        canHandleHTTPS) {
      NSString *tmpBundleID = [self identifierForBundle:anApp];
      if (tmpBundleID)
        [browsersSet addObject:tmpBundleID];
    }
  }

  // add default browser in case it hasn't been already
  NSString* currentBrowser = [self defaultBrowserIdentifier];
  if (currentBrowser)
    [browsersSet addObject:currentBrowser];
  
  return [browsersSet allObjects];
}

- (NSString*)defaultBrowserIdentifier
{
  return [self identifierForBundle:[self defaultBrowserURL]];
}

- (NSURL*)defaultBrowserURL
{
  NSURL *currSetURL = nil;
  if (_LSCopyDefaultSchemeHandlerURL(@"http", &currSetURL) == noErr)
    return [currSetURL autorelease];

  return nil;
}

- (void)setDefaultBrowserWithIdentifier:(NSString*)bundleID
{
  NSURL* browserURL = [self urlOfApplicationWithIdentifier:bundleID];
  if (browserURL)
  {
    FSRef browserFSRef;
    CFURLGetFSRef((CFURLRef)browserURL, &browserFSRef);
    
    _LSSetDefaultSchemeHandlerURL(@"http", browserURL);
    _LSSetDefaultSchemeHandlerURL(@"https", browserURL);
    _LSSetDefaultSchemeHandlerURL(@"ftp", browserURL);
    _LSSetDefaultSchemeHandlerURL(@"gopher", browserURL);
    _LSSetWeakBindingForType(0, 0, CFSTR("htm"),  kLSRolesAll, &browserFSRef);
    _LSSetWeakBindingForType(0, 0, CFSTR("html"), kLSRolesAll, &browserFSRef);
    _LSSetWeakBindingForType(0, 0, CFSTR("url"),  kLSRolesAll, &browserFSRef);
    _LSSaveAndRefresh();
  }
}

- (NSURL*)urlOfApplicationWithIdentifier:(NSString*)bundleID
{
  NSURL* appURL = nil;
  if (LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)bundleID, NULL, NULL, (CFURLRef*)&appURL) == noErr)
    return [appURL autorelease];

  return nil;
}

- (NSString*)identifierForBundle:(NSURL*)inBundleURL
{
  if (!inBundleURL) return nil;

  NSBundle* tmpBundle = [NSBundle bundleWithPath:[[inBundleURL path] stringByStandardizingPath]];
  if (tmpBundle)
  {
    NSString* tmpBundleID = [tmpBundle bundleIdentifier];
    if (tmpBundleID && ([tmpBundleID length] > 0)) {
      return tmpBundleID;
    }
  }
  return nil;
}

- (NSString*)displayNameForFile:(NSURL*)inFileURL
{
  NSString *name;
  LSCopyDisplayNameForURL((CFURLRef)inFileURL, (CFStringRef *)&name);
  return [name autorelease];
}

@end

