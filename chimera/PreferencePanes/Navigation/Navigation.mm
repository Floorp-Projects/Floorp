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
#import "NSString+Utils.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPref.h"
#include "nsIBrowserHistory.h"
#include "nsICacheService.h"
#include "nsILocalFileMac.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"

const int kDefaultExpireDays = 9;

@interface OrgMozillaChimeraPreferenceNavigation(Private)

- (NSString*)getInternetConfigString:(Str255)icPref;
- (NSString*)getDownloadFolderDescription;

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
  
  int expireDays = [self getIntPref:"browser.history_expire_days" withSuccess:&gotPref];
  if (!gotPref)
    expireDays = kDefaultExpireDays;

  [textFieldHistoryDays setIntValue:expireDays];

  [radioOpenTabsForCommand selectCellWithTag:[self getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:&gotPref]];
  [radioOpenForAE selectCellWithTag:[self getIntPref:"browser.reuse_window" withSuccess:&gotPref]];
  
//  [checkboxOpenTabs setState:[self getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:&gotPref]];
//  [checkboxOpenTabsForAEs setState:[self getBooleanPref:"browser.always_reuse_window" withSuccess:&gotPref]];

  [checkboxLoadTabsInBackground setState:[self getBooleanPref:"browser.tabs.loadInBackground" withSuccess:&gotPref]];

  BOOL useSystemHomePage = [self getBooleanPref:"chimera.use_system_home_page" withSuccess:&gotPref] && gotPref;  
  if (useSystemHomePage)
  {
    [textFieldHomePage setEnabled:NO];
    [textFieldSearchPage setEnabled:NO];
  }

  [checkboxUseSystemHomePage setState:useSystemHomePage];
  [textFieldHomePage   setStringValue: [self getCurrentHomePage]];
  [textFieldSearchPage setStringValue: [self getCurrentSearchPage]];
  
  [mEnableHelperApps setState:[self getBooleanPref:"browser.download.autoDispatch" withSuccess:&gotPref]];

  NSString* downloadFolderDesc = [self getDownloadFolderDescription];
  if ([downloadFolderDesc length] == 0)
    downloadFolderDesc = [self getLocalizedString:@"MissingDlFolder"];
    
  [mDownloadFolder setStringValue:[self getDownloadFolderDescription]];
}

- (void) didUnselect
{
  if (!mPrefService)
    return;
  
  // only save the home page pref if it's not the system one
  if (![checkboxUseSystemHomePage state])
  {
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage   stringValue]];
    [self setPref: "chimera.search_page"      toString: [textFieldSearchPage stringValue]];
  }
  
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

  if (sender == radioOpenTabsForCommand) {
    [self setPref:"browser.tabs.opentabfor.middleclick" toBoolean:[[sender selectedCell] tag]];
  }
  else if (sender == radioOpenForAE) {
    [self setPref:"browser.reuse_window" toInt:[[sender selectedCell] tag]];
  }
  else if (sender == checkboxLoadTabsInBackground) {
    [self setPref:"browser.tabs.loadInBackground" toBoolean:[sender state]];
  }
  else if (sender == mEnableHelperApps) {
    [self setPref:"browser.download.autoDispatch" toBoolean:[sender state]];
  }
}


- (IBAction)checkboxUseSystemHomePageClicked:(id)sender
{
  if (!mPrefService)
    return;
  
  BOOL useSystemHomePage = [sender state];

  // save the mozilla pref
  if (useSystemHomePage)
  {
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage   stringValue]];
    [self setPref: "chimera.search_page"      toString: [textFieldSearchPage stringValue]];
  }
  
  [self setPref:"chimera.use_system_home_page" toBoolean: useSystemHomePage];
  [textFieldHomePage   setStringValue: [self getCurrentHomePage]];
  [textFieldSearchPage setStringValue: [self getCurrentSearchPage]];

  [textFieldHomePage   setEnabled:!useSystemHomePage];
  [textFieldSearchPage setEnabled:!useSystemHomePage];
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

- (NSString*)getDownloadFolderDescription
{
  NSString* downloadStr = @"";
  nsCOMPtr<nsIFile> downloadsDir;
  NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR, getter_AddRefs(downloadsDir));
	if (!downloadsDir)
    return downloadStr;

  nsCOMPtr<nsILocalFileMac> macDir = do_QueryInterface(downloadsDir);
  if (!macDir)
    return downloadStr;

  FSRef folderRef;
  nsresult rv = macDir->GetFSRef(&folderRef);
  if (NS_FAILED(rv))
    return downloadStr;

  FSCatalogInfo catInfo;
  HFSUniStr255 fileName;
  OSErr err = FSGetCatalogInfo(&folderRef, kFSCatInfoVolume, &catInfo, &fileName, NULL, NULL);
  if (err != noErr)
    return downloadStr;
    
  HFSUniStr255 volName;
  err = FSGetVolumeInfo(catInfo.volume, 0, NULL, kFSVolInfoNone, NULL, &volName, NULL);
  if (err != noErr)
    return downloadStr;

  NSString* fileNameStr   = [NSString stringWithCharacters:fileName.unicode length:fileName.length];
  NSString* volumeNameStr = [NSString stringWithCharacters:volName.unicode  length:volName.length];
  
	return [NSString stringWithFormat:[self getLocalizedString:@"DownloadFolderDesc"], fileNameStr, volumeNameStr];
}


- (NSString*)getInternetConfigString:(Str255)icPref
{
  NSString*     resultString = @"";
  ICInstance		icInstance = NULL;
  OSStatus 			error;
  
  // it would be nice to use PreferenceManager, but I don't want to drag
  // all that code into the plugin
  error = ICStart(&icInstance, 'CHIM');
  if (error != noErr) {
    NSLog(@"Error from ICStart");
    return resultString;
  }
  
  ICAttr	dummyAttr;
  Str255	homePagePStr;
  long		prefSize = sizeof(homePagePStr);
  error = ICGetPref(icInstance, icPref, &dummyAttr, homePagePStr, &prefSize);
  if (error == noErr)
    resultString = [NSString stringWithCString: (const char*)&homePagePStr[1] length:homePagePStr[0]];
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
    
  return [self getStringPref: "browser.startup.homepage" withSuccess:&gotPref];
}

- (NSString*)getSystemSearchPage
{
  return [self getInternetConfigString:kICWebSearchPagePrefs];
}

- (NSString*)getCurrentSearchPage
{
  BOOL gotPref;
  
  if ([self getBooleanPref:"chimera.use_system_home_page" withSuccess:&gotPref] && gotPref)
    return [self getSystemSearchPage];
    
  return [self getStringPref: "chimera.search_page" withSuccess:&gotPref];
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


- (IBAction)checkboxEnableHelperApps:(id)sender
{

}
@end
