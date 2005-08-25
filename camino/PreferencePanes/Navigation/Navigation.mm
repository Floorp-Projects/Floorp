/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   william@dell.wisner.name (William Dell Wisner)
 *   joshmoz@gmail.com (Josh Aas)
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

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>

#import "Navigation.h"
#import "NSString+Utils.h"

const int kDefaultExpireDays = 9;
static NSString* const kUserChosenBrowserUserDefaultsKey = @"UserChosenBrowsers";

// these functions are undocumented - even if we're not using them all
// its best to leave them here since they are not documented anywhere else
extern "C" {
  /* Returns the NSURL for application currently set as the default for the given URL scheme.  The
  NSURL must be released by the caller. */
  extern OSStatus _LSCopyDefaultSchemeHandlerURL(NSString *scheme, NSURL **appURL);
  extern OSStatus _LSSetDefaultSchemeHandlerURL(NSString *scheme, NSURL *appURL);
  extern OSStatus _LSSaveAndRefresh(void); // inform the finder and others of changes
  extern OSStatus _LSCopyAllApplicationURLs(CFArrayRef *apps);
  extern OSStatus _LSSetWeakBindingForType(OSType inType,
                                           OSType inCreator,
                                           CFStringRef inExtension,
                                           LSRolesMask inRoleMask,
                                           const FSRef* inBindingRef);
  /* Returns by reference the NSURLs of the applications regsistered to handle the given URL.  For the
    "file" protocol, this will use the type/extension/etc info for the file (which must exist for
    this to work), and for any other type of URLs this will return applications registered to handle
    that URL scheme.  The resulting NSArray must be released by the caller. */
  /* Panther has a public LSCopyApplicationURLsForURL() function that would be better to use */
  extern OSStatus _LSCopyApplicationURLsForItemURL(NSURL *inURL, LSRolesMask inRoleMask, NSArray **outApps);
}

@interface OrgMozillaChimeraPreferenceNavigation(Private)

- (NSString*)getCurrentHomePage;
+ (NSArray*)getWebBrowserList;
+ (NSString*)displayNameForURL:(NSURL*)url;
-(void)updateDefaultBrowserMenu;

@end

// this is for sorting the web browser bundle ID list by display name
int compareBundleIDAppDisplayNames(id a, id b, void *context)
{
  NSURL* appURLa = nil;
  NSURL* appURLb = nil;
  
  if ((LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)a, NULL, NULL, (CFURLRef*)&appURLa) == noErr) &&
      (LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)b, NULL, NULL, (CFURLRef*)&appURLb) == noErr))
  {
    NSString *aName = [OrgMozillaChimeraPreferenceNavigation displayNameForURL:appURLa];
    NSString *bName = [OrgMozillaChimeraPreferenceNavigation displayNameForURL:appURLb];
    return [aName compare:bName];
  }
  
  // this shouldn't ever happen, and if it does we handle it correctly when building the menu.
  // there is no way to flag an error condition here and the sort fuctions return void
  return NSOrderedSame;
}

@implementation OrgMozillaChimeraPreferenceNavigation

+ (NSString*)displayNameForURL:(NSURL*)url
{
  NSString *name;
  LSCopyDisplayNameForURL((CFURLRef)url, (CFStringRef *)&name);
  return [name autorelease];
}

+ (NSString*)bundleIDForURL:(NSURL*)url
{
    NSBundle *tmpBundle = [NSBundle bundleWithPath:[[url path] stringByStandardizingPath]];
    if (tmpBundle) {
      NSString *tmpBundleID = [tmpBundle bundleIdentifier];
      if (tmpBundleID && ([tmpBundleID length] > 0)) {
        return tmpBundleID;
      }
    }
    return nil;
}

- (id)initWithBundle:(NSBundle *)bundle
{
  if ((self = [super initWithBundle:bundle])) {
    [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObject:[NSArray array] forKey:kUserChosenBrowserUserDefaultsKey]];
  }
  return self;
}

- (void)dealloc
{
  [super dealloc];
}

- (void)mainViewDidLoad
{
  if (!mPrefService)
    return;

  BOOL gotPref;
  
  // 0: blank page. 1: home page. 2: last page visited. Our behaviour here should
  // match what the browser does when the prefs don't exist.
  if (([self getIntPref:"browser.startup.page" withSuccess:&gotPref] == 1) || !gotPref)
    [checkboxNewWindowBlank setState:YES];
  
  if (([self getIntPref:"browser.tabs.startPage" withSuccess:&gotPref] == 1))
    [checkboxNewTabBlank setState:YES];

  [textFieldHomePage setStringValue:[self getCurrentHomePage]];
  
  // set up default browser menu
  [self updateDefaultBrowserMenu];
}

- (void) didUnselect
{
  if (!mPrefService)
    return;
  
  [self setPref:"browser.startup.homepage" toString:[textFieldHomePage   stringValue]];
  
  // ensure that the prefs exist
  [self setPref:"browser.startup.page"   toInt:[checkboxNewWindowBlank state] ? 1 : 0];
  [self setPref:"browser.tabs.startPage" toInt:[checkboxNewTabBlank state] ? 1 : 0];
}

- (IBAction)checkboxStartPageClicked:(id)sender
{
  if (!mPrefService)
    return;

  char *prefName = NULL;
  if (sender == checkboxNewTabBlank)
    prefName = "browser.tabs.startPage";
  else if (sender == checkboxNewWindowBlank)
    prefName = "browser.startup.page";

  if (prefName)
    [self setPref:prefName toInt: [sender state] ? 1 : 0];
}

- (NSString*) getCurrentHomePage
{
  BOOL gotPref;
  
  return [self getStringPref:"browser.startup.homepage" withSuccess:&gotPref];
}

- (void)setDefaultBrowser:(NSString*)bundleID
{
  NSURL *browserURL = nil;
  if (LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)bundleID, NULL, NULL, (CFURLRef*)&browserURL) == noErr) {
    FSRef browserFSRef;
    CFURLGetFSRef((CFURLRef)browserURL, &browserFSRef);
    
    _LSSetDefaultSchemeHandlerURL(@"http", browserURL);
    _LSSetDefaultSchemeHandlerURL(@"https", browserURL);
    _LSSetDefaultSchemeHandlerURL(@"ftp", browserURL);
    _LSSetDefaultSchemeHandlerURL(@"gopher", browserURL);
    _LSSetWeakBindingForType(0, 0, (CFStringRef)@"htm", kLSRolesAll, &browserFSRef);
    _LSSetWeakBindingForType(0, 0, (CFStringRef)@"html", kLSRolesAll, &browserFSRef);
    _LSSetWeakBindingForType(0, 0, (CFStringRef)@"url", kLSRolesAll, &browserFSRef);
    _LSSaveAndRefresh();
  }
}

// called when the users changes the selection in the default browser menu
- (IBAction)defaultBrowserChange:(id)sender
{
  id repObject = [sender representedObject];
  if (repObject) {
    // only change default if the app still exists
    NSURL *appURL = nil;
    if (LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)repObject, NULL, NULL, (CFURLRef*)&appURL) == noErr) {
      if ([[NSFileManager defaultManager] isExecutableFileAtPath:[[appURL path] stringByStandardizingPath]]) {
        // set it as the default browser
        [self setDefaultBrowser:repObject];
        return;
      }
    }
    NSRunAlertPanel(NSLocalizedString(@"Application does not exist", @"Application does not exist"),
                    NSLocalizedString(@"BrowserDoesNotExistExplanation", @"BrowserDoesNotExistExplanation"),
                    NSLocalizedString(@"OKButtonText", @"OK"), nil, nil);
    // update the popup list to reflect the fact that the chosen browser does not exist
    [self updateDefaultBrowserMenu];
  } else {
    // its not a browser that was selected, so the user must want to select a browser
    NSOpenPanel *op = [NSOpenPanel openPanel];
    [op setCanChooseDirectories:NO];
    [op setAllowsMultipleSelection:NO];
    [op beginSheetForDirectory:nil
                          file:nil
                         types:[NSArray arrayWithObject:@"app"]
                modalForWindow:[defaultBrowserPopUp window]
                 modalDelegate:self
                didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
                   contextInfo:nil];
  }
}

- (void)openPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  if (returnCode == NSOKButton) {
    NSString *chosenBundleID = [OrgMozillaChimeraPreferenceNavigation bundleIDForURL:[[sheet URLs] objectAtIndex:0]];
    if (chosenBundleID) {
      // add this browser to a list of apps we should always consider as browsers
      NSMutableArray *userChosenBundleIDs = [NSMutableArray arrayWithCapacity:2];
      [userChosenBundleIDs addObjectsFromArray:[[NSUserDefaults standardUserDefaults] objectForKey:kUserChosenBrowserUserDefaultsKey]];
      if (![userChosenBundleIDs containsObject:chosenBundleID]) {
        [userChosenBundleIDs addObject:chosenBundleID];
        [[NSUserDefaults standardUserDefaults] setObject:userChosenBundleIDs forKey:kUserChosenBrowserUserDefaultsKey];
      }
      // make it the default browser
      [self setDefaultBrowser:chosenBundleID];
    }
  }
  [self updateDefaultBrowserMenu];
}

// returns an array of web browser bundle IDs sorted by display name
+ (NSArray*)getWebBrowserList
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
      NSString *tmpBundleID = [OrgMozillaChimeraPreferenceNavigation bundleIDForURL:anApp];
      if (tmpBundleID)
        [browsersSet addObject:tmpBundleID];
    }
  }

  // add user chosen browsers to list
  [browsersSet addObjectsFromArray:[[NSUserDefaults standardUserDefaults] objectForKey:kUserChosenBrowserUserDefaultsKey]];

  // add default browser in case it hasn't been already
  NSURL *currSetURL = nil;
  if (_LSCopyDefaultSchemeHandlerURL(@"http", &currSetURL) == noErr)
    [browsersSet addObject:[OrgMozillaChimeraPreferenceNavigation bundleIDForURL:currSetURL]];
  [currSetURL release];
  
  NSMutableArray* browsers = [[browsersSet allObjects] mutableCopy];

  // sort by display name
  [browsers sortUsingFunction:&compareBundleIDAppDisplayNames context:NULL];

  return browsers;
}

-(void)updateDefaultBrowserMenu
{
  NSArray *browsers = [OrgMozillaChimeraPreferenceNavigation getWebBrowserList];
  NSMenu *menu = [[[NSMenu alloc] initWithTitle:@"Browsers"] autorelease];
  NSMenuItem *selectedBrowserMenuItem = nil;
  NSString *caminoBundleID = [[NSBundle mainBundle] bundleIdentifier];
  
  // get current default browser's bundle ID
  NSURL *currSetURL = nil;
  _LSCopyDefaultSchemeHandlerURL(@"http", &currSetURL);
  NSString *currentDefaultBrowserBundleID = [OrgMozillaChimeraPreferenceNavigation bundleIDForURL:currSetURL];
  [currSetURL release];
  
  // add separator first, current instance of Camino will be inserted before it
  [menu addItem:[NSMenuItem separatorItem]];

  // Set up new menu
  NSEnumerator *browserEnumerator = [browsers objectEnumerator];
  while (NSString *bundleID = [browserEnumerator nextObject]) {
    NSURL *appURL = nil;
    OSErr e = LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)bundleID,
                                       NULL, NULL, (CFURLRef*)&appURL);
    if (e != noErr) {
      // see if it was supposed to find Camino and use our own path in that case,
      // otherwise skip this bundle ID
      if ([bundleID isEqualToString:caminoBundleID])
        appURL = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
      else
        continue;
    }
    
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[OrgMozillaChimeraPreferenceNavigation displayNameForURL:appURL]
                                                  action:@selector(defaultBrowserChange:) keyEquivalent:@""];
    NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[[appURL path] stringByStandardizingPath]];
    [icon setSize:NSMakeSize(16.0, 16.0)];
    [item setImage:icon];
    [item setRepresentedObject:bundleID];
    [item setTarget:self];
    [item setEnabled:YES];
    
    // if this item is Camino, insert it first in the list
    if ([bundleID isEqualToString:caminoBundleID])
      [menu insertItem:item atIndex:0];
    else
      [menu addItem:item];
    
    // if this item has the same bundle ID as the current default browser, select it
    if ([bundleID isEqualToString:currentDefaultBrowserBundleID])
      selectedBrowserMenuItem = item;
    
    [item release];
  }
  
  // allow user to select a browser
  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* selectItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Select...", "Select...")
                                    action:@selector(defaultBrowserChange:) keyEquivalent:@""];
  [selectItem setRepresentedObject:nil];
  [selectItem setTarget:self];
  [selectItem setEnabled:YES];
  [menu addItem:selectItem];
  [selectItem release];
  
  [defaultBrowserPopUp setMenu:menu];

  // set the correct selection
  [defaultBrowserPopUp selectItem:selectedBrowserMenuItem];
}

@end
