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

const int kDefaultExpireDays = 9;

@implementation OrgMozillaChimeraPreferenceNavigation

- (void) dealloc
{
  NS_IF_RELEASE(mPrefService);
}

- (id) initWithBundle:(NSBundle *) bundle
{
  self = [super initWithBundle:bundle] ;
  
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  NS_ASSERTION(prefService, "Could not get pref service, pref panel left uninitialized");
  mPrefService = prefService.get();
  NS_IF_ADDREF(mPrefService);
  
  return self;
}

- (void)mainViewDidLoad
{
  if (!mPrefService)
    return;

  PRBool boolPref;
  PRInt32 intPref;
  nsresult rv = mPrefService->GetIntPref("browser.startup.page", &intPref);
  // Check for NS_FAILED because we don't want to falsely interpret
  // a failure as a preference set to 0.
  if (NS_SUCCEEDED(rv) && intPref == 0)
    [checkboxNewWindowBlank setState:YES];

  rv = mPrefService->GetIntPref("browser.tabs.startPage", &intPref);
  if (NS_SUCCEEDED(rv) && intPref == 0)
    [checkboxNewTabBlank setState:YES];

  rv = mPrefService->GetIntPref("browser.history_expire_days", &intPref);
  if (NS_FAILED(rv))
    intPref = kDefaultExpireDays;
  [textFieldHistoryDays setIntValue:intPref];
  [sliderHistoryDays setIntValue:intPref];

  rv = mPrefService->GetBoolPref("browser.tabs.opentabfor.middleclick", &boolPref);
  if (NS_SUCCEEDED(rv) && boolPref == PR_TRUE)
    [checkboxOpenTabs setState:YES];

  BOOL useSystemHomePage = NO;
  rv = mPrefService->GetBoolPref("chimera.use_system_home_page", &boolPref);
  if (NS_SUCCEEDED(rv) && boolPref == PR_TRUE)
    useSystemHomePage = YES;
  
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
  if (!mPrefService || sender != checkboxOpenTabs)
    return;

  mPrefService->SetBoolPref("browser.tabs.opentabfor.middleclick", [sender state] ? PR_TRUE : PR_FALSE);
}

- (IBAction)checkboxUseSystemHomePageClicked:(id)sender
{
  if (!mPrefService)
    return;
  
  BOOL useSystemHomePage = [sender state];

  // save the mozilla pref
  if (useSystemHomePage)
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage stringValue]];
  
  mPrefService->SetBoolPref("chimera.use_system_home_page", useSystemHomePage ? PR_TRUE : PR_FALSE);

  [textFieldHomePage setStringValue: [self getCurrentHomePage]];
  [textFieldHomePage setEnabled:!useSystemHomePage];
}

- (IBAction)checkboxStartPageClicked:(id)sender
{
  if (!mPrefService)
    return;

  char *prefName = NULL;
  if (sender == checkboxNewTabBlank)
    prefName = "browser.tabs.page";
  else if (sender == checkboxNewWindowBlank)
    prefName = "browser.startup.page";
  else
    return;

  if (prefName) 
    mPrefService->SetIntPref(prefName, [sender state] ? 0 : 1);
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
      PRInt32 intPref = kDefaultExpireDays;
      mPrefService->GetIntPref("browser.history_expire_days", &intPref);
      [textFieldHistoryDays setIntValue:intPref];
      NSBeep ();
      return;
    } else
      [sliderHistoryDays setIntValue:[textFieldHistoryDays intValue]];
  }

  mPrefService->SetIntPref("browser.history_expire_days", [sender intValue]);
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
  BOOL useSystemHomePage = NO;
  PRBool boolPref;
  nsresult rv = mPrefService->GetBoolPref("chimera.use_system_home_page", &boolPref);
  if (NS_SUCCEEDED(rv) && boolPref == PR_TRUE)
    return [self getSystemHomePage];
    
  return [self getPrefString: "browser.startup.homepage"];
}

// convenience routines for mozilla prefs
- (NSString*)getPrefString: (const char*)prefName
{
  NSMutableString *prefValue = [[[NSMutableString alloc] init] autorelease];
  
  char *buf = nsnull;
  nsresult rv = mPrefService->GetCharPref(prefName, &buf);
  if (NS_SUCCEEDED(rv) && buf) {
    [prefValue setString:[NSString stringWithCString:buf]];
    free(buf);
  }
  
  return prefValue;
}

- (void)setPref: (const char*)prefName toString:(NSString*)value
{
  if (mPrefService) {
    mPrefService->SetCharPref(prefName, [value cString]);
  }
}


@end
