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

#import "History.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIBrowserHistory.h"
#include "nsICacheService.h"

const int kDefaultExpireDays = 9;

@implementation OrgMozillaChimeraPreferenceHistory

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
  int expireDays = [self getIntPref:"browser.history_expire_days" withSuccess:&gotPref];
  if (!gotPref)
    expireDays = kDefaultExpireDays;
  
  [textFieldHistoryDays setIntValue:expireDays];
}

- (IBAction)historyDaysModified:(id)sender
{
  if (!mPrefService)
    return;
  
  if (sender == textFieldHistoryDays) {
    // If any non-numeric characters were entered make some noise and spit it out.
    if (([[textFieldHistoryDays stringValue] rangeOfCharacterFromSet:[[NSCharacterSet decimalDigitCharacterSet] invertedSet]]).length) {
      BOOL gotPref;
      int prefValue = [self getIntPref:"browser.history_expire_days" withSuccess:&gotPref];
      if (!gotPref)
        prefValue = kDefaultExpireDays;
      [textFieldHistoryDays setIntValue:prefValue];
      NSBeep ();
      return;
    }
  }
  
  [self setPref:"browser.history_expire_days" toInt:[sender intValue]];
}

// Clear the user's disk cache
-(IBAction) clearDiskCache:(id)aSender
{
  nsCOMPtr<nsICacheService> cacheServ ( do_GetService("@mozilla.org/network/cache-service;1") );
  if ( cacheServ )
    cacheServ->EvictEntries(nsICache::STORE_ANYWHERE);
}

// use the browser history service to clear out the user's global history
- (IBAction)clearGlobalHistory:(id)sender
{
  nsCOMPtr<nsIBrowserHistory> hist ( do_GetService("@mozilla.org/browser/global-history;2") );
  if ( hist )
    hist->RemoveAllPages();
}

@end
