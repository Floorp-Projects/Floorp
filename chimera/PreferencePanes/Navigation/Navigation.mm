/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the Mozilla browser.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation. Portions created by Netscape are
* Copyright (C) 2002 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   william@dell.wisner.name (William Dell Wisner)
*/

#import <Carbon/Carbon.h>

#import "Navigation.h"

#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPref.h"
#include "nsIBrowserHistory.h"
#include "nsICacheService.h"

const int kDefaultExpireDays = 9;

@implementation OrgMozillaChimeraPreferenceNavigation


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
  
  int expireDays = [self getIntPref:"browser.history_expire_days" withSuccess:&gotPref];
  if (!gotPref)
    expireDays = kDefaultExpireDays;

  [textFieldHistoryDays setIntValue:expireDays];
  [sliderHistoryDays setIntValue:expireDays];

  [checkboxOpenTabs setState:[self getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:&gotPref]];
  [checkboxOpenTabsForAEs setState:[self getBooleanPref:"browser.always_reuse_window" withSuccess:&gotPref]];
  [checkboxLoadTabsInBackground setState:[self getBooleanPref:"browser.tabs.loadInBackground" withSuccess:&gotPref]];

  BOOL useSystemHomePage = [self getBooleanPref:"chimera.use_system_home_page" withSuccess:&gotPref] && gotPref;  
  if (useSystemHomePage)
    [textFieldHomePage setEnabled:NO];

  [checkboxUseSystemHomePage setState:useSystemHomePage];
  [textFieldHomePage setStringValue: [self getCurrentHomePage]];
}

- (void) didUnselect
{
  if (!mPrefService)
    return;
  
  // only save the home page pref if it's not the system one
  if (![checkboxUseSystemHomePage state])
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage stringValue]];
  
  // ensure that the prefs exist
  [self setPref:"browser.startup.page"   toInt: [checkboxNewWindowBlank state] ? 1 : 0];
  [self setPref:"browser.tabs.startPage" toInt: [checkboxNewTabBlank    state] ? 1 : 0];
}

- (IBAction)openSystemInternetPanel:(id)sender
{
  if ([[NSWorkspace sharedWorkspace] openFile:@"/System/Library/PreferencePanes/Internet.prefPane"] == NO) {
    // XXXw. pop up a dialog warning that System Preferences couldn't be launched?
    NSLog(@"Failed to launch System Preferences.");
  }
}

- (IBAction)checkboxClicked:(id)sender
{
  if (!mPrefService)
    return;

  if (sender == checkboxOpenTabs) {
    [self setPref:"browser.tabs.opentabfor.middleclick" toBoolean:[sender state]];
  }
  else if (sender == checkboxOpenTabsForAEs) {
    [self setPref:"browser.always_reuse_window" toBoolean:[sender state]];
  }
  else if (sender == checkboxLoadTabsInBackground) {
    [self setPref:"browser.tabs.loadInBackground" toBoolean:[sender state]];
  }
}

- (IBAction)checkboxUseSystemHomePageClicked:(id)sender
{
  if (!mPrefService)
    return;
  
  BOOL useSystemHomePage = [sender state];

  // save the mozilla pref
  if (useSystemHomePage)
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage stringValue]];
  
  [self setPref:"chimera.use_system_home_page" toBoolean: useSystemHomePage];
  [textFieldHomePage setStringValue: [self getCurrentHomePage]];
  [textFieldHomePage setEnabled:!useSystemHomePage];
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

- (IBAction)historyDaysModified:(id)sender
{
  if (!mPrefService)
    return;

  if (sender == sliderHistoryDays)
    [textFieldHistoryDays setIntValue:[sliderHistoryDays intValue]];
  else if (sender == textFieldHistoryDays) {
    // If any non-numeric characters were entered make some noise and spit it out.
    if (([[textFieldHistoryDays stringValue] rangeOfCharacterFromSet:[[NSCharacterSet decimalDigitCharacterSet] invertedSet]]).length) {
      BOOL gotPref;
      int prefValue = [self getIntPref:"browser.history_expire_days" withSuccess:&gotPref];
      if (!gotPref)
        prefValue = kDefaultExpireDays;
      [textFieldHistoryDays setIntValue:prefValue];
      [sliderHistoryDays setIntValue:prefValue];
      NSBeep ();
      return;
    } else
      [sliderHistoryDays setIntValue:[textFieldHistoryDays intValue]];
  }

  [self setPref:"browser.history_expire_days" toInt:[sender intValue]];
}


//
// clearGlobalHistory:
//
// use the browser history service to clear out the user's global history
//
- (IBAction)clearGlobalHistory:(id)sender
{
  nsCOMPtr<nsIBrowserHistory> hist ( do_GetService("@mozilla.org/browser/global-history;1") );
  if ( hist )
    hist->RemoveAllPages();
}

- (NSString*) getSystemHomePage
{
  NSMutableString* homePageString = [[[NSMutableString alloc] init] autorelease];
  
  ICInstance		icInstance = NULL;
  OSStatus 			error;
  
  // it would be nice to use CHPreferenceManager, but I don't want to drag
  // all that code into the plugin
  error = ICStart(&icInstance, 'CHIM');
  if (error != noErr) {
    NSLog(@"Error from ICStart");
    return @"";
  }
  
  ICAttr	dummyAttr;
  Str255	homePagePStr;
  long		prefSize = sizeof(homePagePStr);
  error = ICGetPref(icInstance, kICWWWHomePage, &dummyAttr, homePagePStr, &prefSize);
  if (error == noErr)
    [homePageString setString: [NSString stringWithCString: (const char*)&homePagePStr[1] length:homePagePStr[0]]];
  else
    NSLog(@"Error getting home page from Internet Config");
  
  ICStop(icInstance);
  
  return homePageString;
}

- (NSString*) getCurrentHomePage
{
  BOOL gotPref;
  
  if ([self getBooleanPref:"chimera.use_system_home_page" withSuccess:&gotPref] && gotPref)
    return [self getSystemHomePage];
    
  return [self getStringPref: "browser.startup.homepage" withSuccess:&gotPref];
}


//
// clearDiskCache:
//
// Clear the user's disk cache
//
-(IBAction) clearDiskCache:(id)aSender
{
  nsCOMPtr<nsICacheService> cacheServ ( do_GetService("@mozilla.org/network/cache-service;1") );
  if ( cacheServ )
    cacheServ->EvictEntries(nsICache::STORE_ON_DISK);
}

@end
