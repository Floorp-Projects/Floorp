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
* Nick Kreeger
* Portions created by the Initial Developer are Copyright (C) 2006
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*   Nick Kreeger <nick.kreeger@park.edu>
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

#import "AppListMenuFactory.h"
#import "NSWorkspace+Utils.h"

NSString* const kUserChosenBrowserUserDefaultsKey = @"UserChosenBrowsers";
NSString* const kUserChosenFeedViewerUserDefaultsKey = @"UserChosenFeedViewer";
NSString* const kDefaultFeedViewerChanged = @"DefaultFeedViewerChanged";

@interface AppListMenuFactory(Private)

-(NSArray*)webBrowsersList;
-(NSArray*)feedAppsList;
-(NSMenuItem*)menuItemForAppURL:(NSURL*)inURL 
                   withBundleID:(NSString*)inBundleID 
                      andAction:(SEL)inAction
                      andTarget:(id)inTarget;

@end

//
// CompareBundleIDAppDisplayNames()
//
// This is for sorting the web browser bundle ID list by display name.
//
static int CompareBundleIDAppDisplayNames(id a, id b, void *context)
{
  NSURL* appURLa = nil;
  NSURL* appURLb = nil;
  
  if ((LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)a, NULL, NULL, (CFURLRef*)&appURLa) == noErr) &&
      (LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)b, NULL, NULL, (CFURLRef*)&appURLb) == noErr))
  {
    NSString *aName = [[NSWorkspace sharedWorkspace] displayNameForFile:appURLa];
    NSString *bName = [[NSWorkspace sharedWorkspace] displayNameForFile:appURLb];
    
    [appURLa release];
    [appURLb release];
    
    if (aName && bName)
      return [aName compare:bName];
  }
  
  // this shouldn't ever happen, and if it does we handle it correctly when building the menu.
  // there is no way to flag an error condition here and the sort fuctions return void
  return NSOrderedSame;
}

@implementation AppListMenuFactory

static AppListMenuFactory* sAppListMenuFactoryInstance = nil;

//
// +sharedFeedMenuFactory:
//
// Return the shared static instance of AppListMenuFactory.
//
+(AppListMenuFactory*)sharedAppListMenuFactory
{
  return sAppListMenuFactoryInstance ? sAppListMenuFactoryInstance : sAppListMenuFactoryInstance = [[self alloc] init];
}

//
// -buildFeedAppsMenuForPopup: withAction: withSelectAction: withTarget:
//
// Build a NSMenu for the available feed viewing applications, set the menu of 
// the passed in NSPopUpButton and select the default application in the menu.
//
-(void)buildFeedAppsMenuForPopup:(NSPopUpButton*)inPopupButton 
                       andAction:(SEL)inAction 
                 andSelectAction:(SEL)inSelectAction
                       andTarget:(id)inTarget
{
  NSArray* feedApps = [self feedAppsList];
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@"FeedApps"] autorelease];
  NSString* defaultFeedViewerID = [[NSWorkspace sharedWorkspace] defaultFeedViewerIdentifier];

  [menu addItem:[NSMenuItem separatorItem]];
  
  BOOL insertedDefaultApp = NO;
  BOOL shouldInsertSeperatorAtEnd = NO;
  NSEnumerator* feedAppsEnum = [feedApps objectEnumerator];
  NSString* curBundleID = nil;
  while ((curBundleID = [feedAppsEnum nextObject])) {
    // Don't add Safari.
    if ([curBundleID isEqualToString:@"com.apple.Safari"])
      continue;
    
    NSURL* appURL = [[NSWorkspace sharedWorkspace] urlOfApplicationWithIdentifier:curBundleID];
    if (!appURL)
      continue;
    
    NSMenuItem* menuItem = [self menuItemForAppURL:appURL
                                      withBundleID:curBundleID 
                                         andAction:inAction 
                                         andTarget:inTarget];
    
    // If this is the default feed app insert it in the first list
    if (defaultFeedViewerID && [curBundleID isEqualToString:defaultFeedViewerID]) {
      [menu insertItem:menuItem atIndex:0];
      insertedDefaultApp = YES;
    }
    else {
      [menu addItem:menuItem];
      shouldInsertSeperatorAtEnd = YES;
    }
  }
  
  // The user selected an application that is not registered for "feed://"
  // or has no application selected
  if (!insertedDefaultApp) {
    NSURL* defaultFeedAppURL = nil;
    if (defaultFeedViewerID && ([NSWorkspace isTigerOrHigher] || ![defaultFeedViewerID isEqualToString:@"com.apple.Safari"]))
      defaultFeedAppURL = [[NSWorkspace sharedWorkspace] urlOfApplicationWithIdentifier:defaultFeedViewerID];
    if (defaultFeedAppURL) {
      NSMenuItem* menuItem = [self menuItemForAppURL:defaultFeedAppURL
                                        withBundleID:defaultFeedViewerID
                                           andAction:nil 
                                           andTarget:inTarget];
      [menu insertItem:menuItem atIndex:0];
    }
    // Since we couldn't find a default application, add a blank menu item.
    else {
      NSMenuItem* dummyItem = [[NSMenuItem alloc] init];
      [dummyItem setTitle:@""];
      [menu insertItem:dummyItem atIndex:0];
      [dummyItem release];
    }
  }
  
  // allow the user to select a feed application
  if (shouldInsertSeperatorAtEnd)
    [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* selectItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Select...", nil)
                                                      action:inSelectAction 
                                               keyEquivalent:@""];
  [selectItem setTarget:inTarget];
  [menu addItem:selectItem];
  [selectItem release];
  
  [inPopupButton setMenu:menu];
  [inPopupButton selectItemAtIndex:0];
}

//
// -buildBrowserAppsMenuForPopup:withAction:withSelectAction:withTarget:
//
// Build a NSMenu for the available web browser applications, set the menu of 
// the passed in NSPopUpButton and select the default application in the menu.
//
-(void)buildBrowserAppsMenuForPopup:(NSPopUpButton*)inPopupButton
                          andAction:(SEL)inAction
                    andSelectAction:(SEL)inSelectAction
                          andTarget:(id)inTarget
{
  NSArray* browsers = [self webBrowsersList];
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@"Browsers"] autorelease];
  NSString* caminoBundleID = [[NSBundle mainBundle] bundleIdentifier];
  
  // get current default browser's bundle ID
  NSString* currentDefaultBrowserBundleID = [[NSWorkspace sharedWorkspace] defaultBrowserIdentifier];
  
  // add separator first, current instance of Camino will be inserted before it
  [menu addItem:[NSMenuItem separatorItem]];
  
  // Set up new menu
  NSMenuItem* selectedBrowserMenuItem = nil;
  NSEnumerator* browserEnumerator = [browsers objectEnumerator];
  NSString* bundleID;
  while ((bundleID = [browserEnumerator nextObject])) {
    NSURL* appURL = [[NSWorkspace sharedWorkspace] urlOfApplicationWithIdentifier:bundleID];
    if (!appURL) {
      // see if it was supposed to find Camino and use our own path in that case,
      // otherwise skip this bundle ID
      if ([bundleID isEqualToString:caminoBundleID])
        appURL = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
      else
        continue;
    }
    
    NSMenuItem* menuItem = [self menuItemForAppURL:appURL
                                      withBundleID:bundleID 
                                         andAction:inAction 
                                         andTarget:inTarget];
    
    // if this item is Camino, insert it first in the list
    if ([bundleID isEqualToString:caminoBundleID])
      [menu insertItem:menuItem atIndex:0];
    else
      [menu addItem:menuItem];
    
    // if this item has the same bundle ID as the current default browser, select it
    if (currentDefaultBrowserBundleID && [bundleID isEqualToString:currentDefaultBrowserBundleID])
      selectedBrowserMenuItem = menuItem;
  }
  
  // allow user to select a browser
  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* selectItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Select...", nil)
                                                      action:inSelectAction 
                                               keyEquivalent:@""];
  [selectItem setTarget:inTarget];
  [menu addItem:selectItem];
  [selectItem release];
  
  [inPopupButton setMenu:menu];
  
  // set the correct selection
  [inPopupButton selectItem:selectedBrowserMenuItem];
}

//
// -createMenuItemForAppURL:withRepresentedObject:withAction:withTarget:
//
// Builds a menu item for the passed in values. The item will be autoreleased.
//
-(NSMenuItem*)menuItemForAppURL:(NSURL*)inURL 
                   withBundleID:(NSString*)inBundleID 
                      andAction:(SEL)inAction
                      andTarget:(id)inTarget
{
  NSMenuItem* menuItem = nil;
  NSString* displayName = [[NSWorkspace sharedWorkspace] displayNameForFile:inURL];
  
  if (displayName) {
    menuItem = [[NSMenuItem alloc] initWithTitle:displayName
                                          action:inAction 
                                   keyEquivalent:@""];
    NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile:[[inURL path] stringByStandardizingPath]];
    if (icon) {
      [icon setSize:NSMakeSize(16.0, 16.0)];
      [menuItem setImage:icon];
    }
    [menuItem setRepresentedObject:inBundleID];
    [menuItem setTarget:inTarget];
  }
  
  return [menuItem autorelease];
}

//
// -webBrowserList:
//
// Retuns an array of web browser application bundle id's that are registered with 
// Launch Services, and those that have been hand picked by the user.
//
-(NSArray*)webBrowsersList
{
  NSArray* installedBrowsers = [[NSWorkspace sharedWorkspace] installedBrowserIdentifiers];
  NSMutableSet* browsersSet = [NSMutableSet setWithArray:installedBrowsers];
  
  // add user chosen browsers to list
  [browsersSet addObjectsFromArray:[[NSUserDefaults standardUserDefaults] objectForKey:kUserChosenBrowserUserDefaultsKey]];
  
  return [[browsersSet allObjects] sortedArrayUsingFunction:&CompareBundleIDAppDisplayNames context:NULL];
}

//
// -feedAppsList:
//
// Retuns an array of feed viewing application bundle id's that are registered with 
// Launch Services for "feed://", and those that have been hand picked by the user.
//
-(NSArray*)feedAppsList
{
  NSSet* installedFeedApps = [[NSWorkspace sharedWorkspace] installedFeedViewerIdentifiers];
  NSMutableSet* feedAppsSet = [NSMutableSet setWithSet:installedFeedApps];
  
  // add user chosen feed apps to the list
  [feedAppsSet addObjectsFromArray:[[NSUserDefaults standardUserDefaults] objectForKey:kUserChosenFeedViewerUserDefaultsKey]];
  
  return [[feedAppsSet allObjects] sortedArrayUsingFunction:&CompareBundleIDAppDisplayNames context:NULL];
}

//
// -attemptDefaultFeedViewerRegistration:
//
// Set the default feed viewing application with Launch Services.
// This is used over the NSWorkspace+Utils method when an application
// was chosen from menu that could possibly contain stale data.
//
-(BOOL)validateAndRegisterDefaultFeedViewer:(NSString*)inBundleID
{
  NSString* defaultAppID = [[NSWorkspace sharedWorkspace] defaultFeedViewerIdentifier];
  BOOL succeeded = NO;
  
  // if the user selected something other than the default application
  if (defaultAppID && ![inBundleID isEqualToString:defaultAppID]) {
    NSURL* appURL = [[NSWorkspace sharedWorkspace] urlOfApplicationWithIdentifier:inBundleID];
    if (appURL) {
      [[NSWorkspace sharedWorkspace] setDefaultFeedViewerWithIdentifier:inBundleID];
      succeeded = YES;
    }
    // could not find information for the selected app, warn the user
    else {
      NSRunAlertPanel(NSLocalizedString(@"Application does not exist", nil),
                      NSLocalizedString(@"FeedAppDoesNotExistExplanation", nil),
                      NSLocalizedString(@"OKButtonText", nil), nil, nil);
    }
  }
  
  return succeeded;
}  

//
// -attemptDefaultBrowserRegistration:
//
// Set the default web browser with Launch Services.
// This is used over the NSWorkspace+Utils method when an application
// was chosen from menu that could possibly contain stale data.
//
-(BOOL)validateAndRegisterDefaultBrowser:(NSString*)inBundleID
{
  NSString* defaultBrowserID = [[NSWorkspace sharedWorkspace] defaultBrowserIdentifier];
  BOOL succeeded = NO;
  
  // only set a new item if the user selected something other than the default application
  if (defaultBrowserID && ![inBundleID isEqualToString:defaultBrowserID]) {
    NSURL* appURL = [[NSWorkspace sharedWorkspace] urlOfApplicationWithIdentifier:inBundleID];
    if (appURL) {
      [[NSWorkspace sharedWorkspace] setDefaultBrowserWithIdentifier:inBundleID];
      succeeded = YES;
    }
    // could not find information for the selected app, warn the user
    else {
      NSRunAlertPanel(NSLocalizedString(@"Application does not exist", nil),
                      NSLocalizedString(@"BrowserDoesNotExistExplanation", nil),
                      NSLocalizedString(@"OKButtonText", nil), nil, nil);
    }
  }
  
  return succeeded;
}

@end
