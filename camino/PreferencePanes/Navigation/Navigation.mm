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
*   joshmoz@gmail.com (Josh Aas)
*/

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#import "Navigation.h"
#import "NSString+Utils.h"

const int kDefaultExpireDays = 9;

@interface OrgMozillaChimeraPreferenceNavigation(Private)

- (NSString*)getInternetConfigString:(ConstStr255Param)icPref;
- (NSString*)getSystemHomePage;
- (NSString*)getCurrentHomePage;

@end

@implementation OrgMozillaChimeraPreferenceNavigation

- (id)initWithBundle:(NSBundle *)bundle
{
  self = [super initWithBundle:bundle];
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

  BOOL useSystemHomePage = [self getBooleanPref:"chimera.use_system_home_page" withSuccess:&gotPref] && gotPref;  
  if (useSystemHomePage)
    [textFieldHomePage setEnabled:NO];

  [checkboxUseSystemHomePage setState:useSystemHomePage];
  [textFieldHomePage setStringValue:[self getCurrentHomePage]];
}

- (void) didUnselect
{
  if (!mPrefService)
    return;
  
  // only save the home page pref if it's not the system one
  if (![checkboxUseSystemHomePage state])
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage   stringValue]];
  
  // ensure that the prefs exist
  [self setPref:"browser.startup.page" toInt: [checkboxNewWindowBlank state] ? 1 : 0];
  [self setPref:"browser.tabs.startPage" toInt: [checkboxNewTabBlank state] ? 1 : 0];
}

- (IBAction)checkboxUseSystemHomePageClicked:(id)sender
{
  if (!mPrefService)
    return;
  
  BOOL useSystemHomePage = [sender state];

  // save the mozilla pref
  if (useSystemHomePage)
    [self setPref:"browser.startup.homepage" toString:[textFieldHomePage stringValue]];
  
  [self setPref:"chimera.use_system_home_page" toBoolean:useSystemHomePage];
  [textFieldHomePage setStringValue:[self getCurrentHomePage]];

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

- (NSString*)getInternetConfigString:(ConstStr255Param)icPref
{
  NSString* resultString = @"";
  ICInstance icInstance = NULL;
  
  // it would be nice to use PreferenceManager, but I don't want to drag
  // all that code into the plugin
  OSStatus error = ICStart(&icInstance, 'CHIM');
  if (error != noErr) {
    NSLog(@"Error from ICStart");
    return resultString;
  }
  
  ICAttr dummyAttr;
  Str255 homePagePStr;
  long prefSize = sizeof(homePagePStr);
  error = ICGetPref(icInstance, icPref, &dummyAttr, homePagePStr, &prefSize);
  if (error == noErr)
    resultString = [NSString stringWithCString:(const char*)&homePagePStr[1] length:homePagePStr[0]];
  else
    NSLog(@"Error getting pref from Internet Config");
  
  ICStop(icInstance);
  
  return resultString;
}

- (NSString*) getSystemHomePage
{
  return [self getInternetConfigString:kICWWWHomePage];
}

- (NSString*) getCurrentHomePage
{
  BOOL gotPref;
  
  if ([self getBooleanPref:"chimera.use_system_home_page" withSuccess:&gotPref] && gotPref)
    return [self getSystemHomePage];
    
  return [self getStringPref:"browser.startup.homepage" withSuccess:&gotPref];
}

@end
