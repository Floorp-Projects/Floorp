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
#import <Cocoa/Cocoa.h>

#import "Navigation.h"
#import "NSString+Utils.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIBrowserHistory.h"
#include "nsICacheService.h"
#include "nsILocalFileMac.h"
#include "nsDirectoryServiceDefs.h"

const int kDefaultExpireDays = 9;

@interface OrgMozillaChimeraPreferenceNavigation(Private)

- (NSString*)getInternetConfigString:(ConstStr255Param)icPref;
- (NSString*)getDownloadFolderDescription;
- (void)setupDownloadMenuWithPath:(NSString*)inDLPath;
- (NSString*)getSystemHomePage;
- (NSString*)getCurrentHomePage;
- (void)setDownloadFolder:(NSString*)inNewFolder;

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
  
  [checkboxLoadTabsInBackground setState:[self getBooleanPref:"browser.tabs.loadInBackground" withSuccess:&gotPref]];

  BOOL useSystemHomePage = [self getBooleanPref:"chimera.use_system_home_page" withSuccess:&gotPref] && gotPref;  
  if (useSystemHomePage)
    [textFieldHomePage setEnabled:NO];

  [checkboxUseSystemHomePage setState:useSystemHomePage];
  [textFieldHomePage   setStringValue: [self getCurrentHomePage]];
  
  [mEnableHelperApps setState:[self getBooleanPref:"browser.download.autoDispatch" withSuccess:&gotPref]];

  NSString* downloadFolderDesc = [self getDownloadFolderDescription];
  if ([downloadFolderDesc length] == 0)
    downloadFolderDesc = [self getLocalizedString:@"MissingDlFolder"];
  
  [self setupDownloadMenuWithPath:downloadFolderDesc];
  
//  [mDownloadFolder setStringValue:[self getDownloadFolderDescription]];
}

- (void) didUnselect
{
  if (!mPrefService)
    return;
  
  // only save the home page pref if it's not the system one
  if (![checkboxUseSystemHomePage state])
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage   stringValue]];
  
  // ensure that the prefs exist
  [self setPref:"browser.startup.page"   toInt: [checkboxNewWindowBlank state] ? 1 : 0];
  [self setPref:"browser.tabs.startPage" toInt: [checkboxNewTabBlank    state] ? 1 : 0];
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
    [self setPref: "browser.startup.homepage" toString: [textFieldHomePage   stringValue]];
  
  [self setPref:"chimera.use_system_home_page" toBoolean: useSystemHomePage];
  [textFieldHomePage   setStringValue: [self getCurrentHomePage]];

  [textFieldHomePage   setEnabled:!useSystemHomePage];
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
  nsCOMPtr<nsIBrowserHistory> hist ( do_GetService("@mozilla.org/browser/global-history;2") );
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
  UInt8 utf8path[MAXPATHLEN+1];
  ::FSRefMakePath(&folderRef, utf8path, MAXPATHLEN);
  return [NSString stringWithUTF8String:(const char*)utf8path];
}

//
// -setDownloadFolder:
//
// Sets the IC download pref to the given path
// NOTE: THIS DOES NOT WORK.
//
- (void)setDownloadFolder:(NSString*)inNewFolder
{
  if (!inNewFolder)
    return;

  // it would be nice to use PreferenceManager, but I don't want to drag
  // all that code into the plugin
  ICInstance icInstance = nil;
  OSStatus error = ::ICStart(&icInstance, 'CHIM');
  if (error != noErr)
    return;
  
  // make a ICFileSpec out of our path and shove it into IC. This requires
  // creating an FSSpec and an alias. We can't just bail on error because
  // we have to make sure we call ICStop() below.
  BOOL noErrors = NO;
  FSRef fsRef;
  Boolean isDir;
  AliasHandle alias = nil;
  FSSpec fsSpec;
  error = ::FSPathMakeRef((UInt8 *)[inNewFolder fileSystemRepresentation], &fsRef, &isDir);
  if (!error) {
    error = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNone, nil, nil, &fsSpec, nil);
    if (!error) {
      error = ::FSNewAlias(nil, &fsRef, &alias);
      if (!error)
        noErrors = YES;
    }
  }
  
  // copy the data out of our variables into the ICFileSpec and hand it to IC.
  if (noErrors) {
    long headerSize = offsetof(ICFileSpec, alias);
    long aliasSize = ::GetHandleSize((Handle)alias);
    ICFileSpec* realbuffer = (ICFileSpec*) calloc(headerSize + aliasSize, 1);
    realbuffer->fss = fsSpec;
    memcpy(&realbuffer->alias, *alias, aliasSize);
    ::ICSetPref(icInstance, kICDownloadFolder, kICAttrNoChange, (const void*)realbuffer, headerSize + aliasSize);
    free(realbuffer);
  }
  
  ::ICStop(icInstance);
}

- (NSString*)getInternetConfigString:(ConstStr255Param)icPref
{
  NSString*     resultString = @"";
  ICInstance		icInstance = NULL;
  
  // it would be nice to use PreferenceManager, but I don't want to drag
  // all that code into the plugin
  OSStatus error = ICStart(&icInstance, 'CHIM');
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


//
// clearDiskCache:
//
// Clear the user's disk cache
//
-(IBAction) clearDiskCache:(id)aSender
{
  nsCOMPtr<nsICacheService> cacheServ ( do_GetService("@mozilla.org/network/cache-service;1") );
  if ( cacheServ )
    cacheServ->EvictEntries(nsICache::STORE_ANYWHERE);
}

//
// -setupDownloadMenuWithPath:
//
// Given a full path to the d/l dir, display the leaf name and the finder icon associated
// with that folder in the first item of the download folder popup.
//
- (void)setupDownloadMenuWithPath:(NSString*)inDLPath
{
  NSMenuItem* placeholder = [mDownloadFolder itemAtIndex:0];
  if (!placeholder)
    return;
  
  // get the finder icon and scale it down to 16x16
  NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile:inDLPath];
  [icon setScalesWhenResized:YES];
  [icon setSize:NSMakeSize(16.0, 16.0)];

  // set the title to the leaf name and the icon to what we gathered above
  [placeholder setTitle:[inDLPath lastPathComponent]];
  [placeholder setImage:icon];
  
  // ensure first item is selected
  [mDownloadFolder selectItemAtIndex:0];
}

//
// -chooseDownloadFolder:
//
// display a file picker sheet allowing the user to set their new download folder
//
- (IBAction)chooseDownloadFolder:(id)sender
{
  NSString* oldDLFolder = [self getDownloadFolderDescription];
  NSOpenPanel* panel = [NSOpenPanel openPanel];
  [panel setCanChooseFiles:NO];
  [panel setCanChooseDirectories:YES];
  [panel setAllowsMultipleSelection:NO];
  [panel setPrompt:NSLocalizedString(@"ChooseDirectoryOKButton", @"")];
  
  [panel beginSheetForDirectory:oldDLFolder file:nil types:nil modalForWindow:[mDownloadFolder window]
           modalDelegate:self didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
           contextInfo:nil];
}

//
// -openPanelDidEnd:returnCode:contextInfo:
//
// called when the user closes the open panel sheet for selecting a new d/l folder.
// if they clicked ok, change the IC pref and re-display the new choice in the
// popup menu
//
- (void)openPanelDidEnd:(NSOpenPanel*)sheet returnCode:(int)returnCode contextInfo:(void*)contextInfo
{
  if (returnCode == NSOKButton) {
    // stuff path into pref
    NSString* newPath = [[sheet filenames] objectAtIndex:0];
    [self setDownloadFolder:newPath];
    
    // update the menu
    [self setupDownloadMenuWithPath:newPath];
  }
  else
    [mDownloadFolder selectItemAtIndex:0];
}
@end
