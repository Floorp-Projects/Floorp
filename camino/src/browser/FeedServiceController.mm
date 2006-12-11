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

#import "FeedServiceController.h"
#import "NSWorkspace+Utils.h"
#import "PreferenceManager.h"
#import "AppListMenuFactory.h"

@interface FeedServiceController(Private)

-(void)buildFeedAppsPopUp;
-(void)setDefaultFeedViewerWithIdentifier:(NSString*)inBundleID;
-(void)shouldWarnWhenOpeningFeed:(BOOL)inState;
-(void)showNotifyOpenExternalFeedAppModal:(NSWindow*)inParentWindow;
-(void)showNotifyNoFeedAppFoundModal:(NSWindow*)inParentWindow;
-(BOOL)validateAndRegisterDefaultFeedViewer:(NSString*)inBundleID;

@end

@implementation FeedServiceController

static FeedServiceController* sInstance = nil;

//
// -sharedFeedServiceController
//
// Return the static global instance of this class 
//
+(FeedServiceController*)sharedFeedServiceController
{
  return sInstance ? sInstance : sInstance = [[self alloc] init];
}

-(id)init
{
  if ((self = [super init])) {
    mFeedURI = nil;
    [NSBundle loadNibNamed:@"OpenFeed" owner:self];
  }

  return self;
}

-(void)awakeFromNib
{
  [mFeedAppsPopUp setAutoenablesItems:NO];
}

-(void)dealloc
{
  [super dealloc];
}

//
// -buildFeedAppsPopUp
//
// Use our utility class to populate our feed viewing applications popup.
//
-(void)buildFeedAppsPopUp
{
  [[AppListMenuFactory sharedAppListMenuFactory] buildFeedAppsMenuForPopup:mFeedAppsPopUp 
                                                                 andAction:nil 
                                                           andSelectAction:@selector(selectFeedAppFromMenuItem:)
                                                                 andTarget:self];
}

//
// -openPanelDidEndForOpenExternalFeedApp:
//
// The callback method for the open panel from the "open external feed app"
// sheet. If the user selected a new application, register it, then rebuild
// the feed apps popup button.
//
-(void)openPanelDidEndForOpenExternalFeedApp:(NSOpenPanel*)sheet returnCode:(int)returnCode contextInfo:(void*)contextInfo
{
  if (returnCode == NSOKButton) {
    NSURL* itemURL = [[sheet URLs] objectAtIndex:0];
    NSBundle* appBundle = [NSBundle bundleWithPath:[itemURL path]];
    if (!appBundle)
      return;
    
    [self setDefaultFeedViewerWithIdentifier:[appBundle bundleIdentifier]];
    
    [self buildFeedAppsPopUp];
  }
  
  // The open action was cancelled, re-select the default application
  else
    [mFeedAppsPopUp selectItemAtIndex:0];
}

//
// -openPanelDidEndForNoFeedAppFound:
//
// The callback method for the open panel from the "no feed apps found"
// sheet. If the user selects an application, register it as the default
// then open the associated feed url.
//
-(void)openPanelDidEndForNoFeedAppFound:(NSOpenPanel*)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  [NSApp endSheet:mNotifyNoFeedAppFound];
  
  if (returnCode == NSOKButton) {
    NSString* chosenBundleID = [[NSWorkspace sharedWorkspace] identifierForBundle:[[sheet URLs] objectAtIndex:0]];
    if (chosenBundleID) {
      // add this browser to a list of apps we should always consider as browsers
      NSMutableArray* userChosenBundleIDs = [NSMutableArray arrayWithCapacity:2];
      [userChosenBundleIDs addObjectsFromArray:[[NSUserDefaults standardUserDefaults] objectForKey:kUserChosenFeedViewerUserDefaultsKey]];
      if (![userChosenBundleIDs containsObject:chosenBundleID]) {
        [userChosenBundleIDs addObject:chosenBundleID];
        [[NSUserDefaults standardUserDefaults] setObject:userChosenBundleIDs forKey:kUserChosenFeedViewerUserDefaultsKey];
      }
      // set the default feed viewer
      [self setDefaultFeedViewerWithIdentifier:chosenBundleID];
      
      // Open the URL with the freshly selected application
      [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:mFeedURI]];
    }
  }
  
  // since we are now opening the feed, check to see if the warn when open pref has changed
  if (returnCode == NSAlertDefaultReturn) {
    [NSApp stopModalWithCode:returnCode];
    [self shouldWarnWhenOpeningFeed:NO];
  }
}

//
// -selectFeedAppFromMenuItem:
//
// Action method for selecting a feed application from the popup button from
// the "feed apps found" dialog.
//
-(IBAction)selectFeedAppFromMenuItem:(id)sender
{
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseDirectories:NO];
  [openPanel setAllowsMultipleSelection:NO];
  [openPanel beginSheetForDirectory:nil
                               file:nil
                              types:[NSArray arrayWithObject:@"app"]
                     modalForWindow:mNotifyOpenExternalFeedApp
                      modalDelegate:self
                     didEndSelector:@selector(openPanelDidEndForOpenExternalFeedApp:returnCode:contextInfo:)
                        contextInfo:nil];
}

//
// -selectFeedAppFromButton:
//
// Action method for selecting a feed application from the "feed apps not found" dialog.
//
-(IBAction)selectFeedAppFromButton:(id)sender
{
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseDirectories:NO];
  [openPanel setAllowsMultipleSelection:NO];
  [openPanel beginSheetForDirectory:nil
                               file:nil
                              types:[NSArray arrayWithObject:@"app"]
                     modalForWindow:mNotifyNoFeedAppFound
                      modalDelegate:self
                     didEndSelector:@selector(openPanelDidEndForNoFeedAppFound:returnCode:contextInfo:)
                        contextInfo:nil];
}

//
// -cancelOpenFeed:
//
// Action method for the cancel button, notifies Camino not to open the feed.
//
-(IBAction)cancelOpenFeed:(id)sender
{
  [NSApp endSheet:[sender window]];
}

//
// -openFeed:
//
// Action method to open the feed. If something other than the default RSS app 
// is selected, set the default rss app for feeds before opening. This button 
// is on the "feed apps found dialog".
//
-(IBAction)openFeed:(id)sender
{
  NSMenuItem* selectedApp = [mFeedAppsPopUp selectedItem];
  
  // Only close the sheet and open the feed if the registration was successful.
  if ([self validateAndRegisterDefaultFeedViewer:[selectedApp representedObject]]) {
    [NSApp endSheet:[sender window]];
    [self shouldWarnWhenOpeningFeed:NO];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:mFeedURI]];
  }
}

//
// -closeSheetAndContinue:
//
// Action method used when warning the user that no feed applications where found.
//
-(IBAction)closeSheetAndContinue:(id)sender
{
  [NSApp endSheet:[sender window]];
  [self shouldWarnWhenOpeningFeed:([mDoNotShowDialogCheckbox state] == NSOnState)];
}

//
// -shouldWarnWhenOpeningFeed:
//
// Sets the state of the warn when opening pref.
//
-(void)shouldWarnWhenOpeningFeed:(BOOL)inState
{
  [[PreferenceManager sharedInstance] setPref:"camino.warn_before_opening_feed" toBoolean:inState];
}

//
// -showFeedWillOpenDialog:
//
// Run the sheet based on if the user has a registered RSS app with launch services.
//
-(void)showFeedWillOpenDialog:(NSWindow*)inParent feedURI:(NSString*)inFeedURI
{
  // Set the URL of the feed so we can open it if the user chooses to do so
  mFeedURI = inFeedURI;
  
  // no feeds exist for the user, run the appropriate dialog to notify the user
  if ([self feedAppsExist]) {
    [self buildFeedAppsPopUp];
    [self showNotifyOpenExternalFeedAppModal:inParent];
  }
  else
    [self showNotifyNoFeedAppFoundModal:inParent];
}

//
// -showNotifyOpenExternalFeedAppModal:
//
// Show the "notify opening external feed application" sheet.
//
-(void)showNotifyOpenExternalFeedAppModal:(NSWindow*)inParentWindow
{
  [NSApp beginSheet:mNotifyOpenExternalFeedApp modalForWindow:inParentWindow
      modalDelegate:self didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

//
// -showNotifyNoFeedAppFoundModal:
//
// Show the notify "no feed applications found" sheet.
//
-(void)showNotifyNoFeedAppFoundModal:(NSWindow*)inParentWindow
{
  [NSApp beginSheet:mNotifyNoFeedAppFound modalForWindow:inParentWindow
      modalDelegate:self didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

//
// -sheedDidEnd:
//
// Close the sheet.
//
- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  [sheet orderOut:self];
}

//
// -feedAppsExist:
//
// Let the caller know if feed applications exist.
//
-(BOOL)feedAppsExist
{
  NSSet* feedApps = [[NSWorkspace sharedWorkspace] installedFeedViewerIdentifiers];

  if (feedApps) {
    if ([feedApps count] == 0 || ([feedApps count] == 1 && [feedApps containsObject:@"com.apple.safari"]))
      return NO;
    else
      return YES;
  }
  
  return NO;
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
  NSBundle* defaultAppID = [NSBundle bundleWithIdentifier:[[NSWorkspace sharedWorkspace] defaultFeedViewerIdentifier]];
  BOOL succeeded = YES;
  
  // if the user selected something other than the default application
  if (![inBundleID isEqualToString:[defaultAppID bundleIdentifier]]) {
    NSURL* appURL = [[NSWorkspace sharedWorkspace] urlOfApplicationWithIdentifier:inBundleID];
    if (appURL) {
      NSBundle* appBundle = [NSBundle bundleWithPath:[appURL path]];
      [[NSWorkspace sharedWorkspace] setDefaultFeedViewerWithIdentifier:[appBundle bundleIdentifier]];
    }
    // could not find information for the selected app, warn the user
    else {
      succeeded = NO;
      NSRunAlertPanel(NSLocalizedString(@"Application does not exist", nil),
                      NSLocalizedString(@"FeedAppDoesNotExistExplanation", nil),
                      NSLocalizedString(@"OKButtonText", nil), nil, nil);
    }
  }
  
  return succeeded;
}  

@end
