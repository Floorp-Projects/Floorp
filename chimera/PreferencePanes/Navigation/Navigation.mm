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

  rv = mPrefService->GetIntPref("chimera.new_tab_page", &intPref);
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

	char *buf = nsnull;
	rv = mPrefService->GetCharPref("browser.startup.homepage", &buf);
	if (NS_SUCCEEDED(rv) && buf)
	{
		[textFieldHomePage setStringValue:[NSString stringWithCString:buf]];
		free(buf);
	}

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
	NSLog(@"Homepage clicked");
}

- (IBAction)checkboxStartPageClicked:(id)sender
{
  if (!mPrefService)
    return;

  char *prefName = NULL;
  if (sender == checkboxNewTabBlank)
    prefName = "chimera.new_tab_page";
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

@end
