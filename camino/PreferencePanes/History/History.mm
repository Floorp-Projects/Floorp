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

#import "History.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIBrowserHistory.h"
#include "nsICacheService.h"

const int kDefaultExpireDays = 9;

@interface OrgMozillaChimeraPreferenceHistory(Private)

- (void)doClearDiskCache;
- (void)doClearGlobalHistory;

@end

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
  NSBeginCriticalAlertSheet([self getLocalizedString:@"EmptyCacheTitle"],
                            [self getLocalizedString:@"EmptyButton"],
                            [self getLocalizedString:@"CancelButtonText"],
                            nil,
                            [textFieldHistoryDays window],    // any view will do
                            self,
                            @selector(clearDiskCacheSheetDidEnd:returnCode:contextInfo:),
                            nil,
                            NULL,
                            [self getLocalizedString:@"EmptyCacheMessage"]);
}

// use the browser history service to clear out the user's global history
- (IBAction)clearGlobalHistory:(id)sender
{
  NSBeginCriticalAlertSheet([self getLocalizedString:@"ClearHistoryTitle"],
                            [self getLocalizedString:@"ClearHistoryButton"],
                            [self getLocalizedString:@"CancelButtonText"],
                            nil,
                            [textFieldHistoryDays window],    // any view willl do
                            self,
                            @selector(clearGlobalHistorySheetDidEnd:returnCode:contextInfo:),
                            nil,
                            NULL,
                            [self getLocalizedString:@"ClearHistoryMessage"]);
}

#pragma mark -

- (void)clearDiskCacheSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  if (returnCode == NSAlertDefaultReturn)
  {
    [self doClearDiskCache];
  }
}

- (void)clearGlobalHistorySheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  if (returnCode == NSAlertDefaultReturn)
  {
    [self doClearGlobalHistory];
  }
}

- (void)doClearDiskCache
{
  nsCOMPtr<nsICacheService> cacheServ ( do_GetService("@mozilla.org/network/cache-service;1") );
  if ( cacheServ )
    cacheServ->EvictEntries(nsICache::STORE_ANYWHERE);
}

- (void)doClearGlobalHistory
{
  nsCOMPtr<nsIBrowserHistory> hist ( do_GetService("@mozilla.org/browser/global-history;2") );
  if ( hist )
    hist->RemoveAllPages();
}

@end
