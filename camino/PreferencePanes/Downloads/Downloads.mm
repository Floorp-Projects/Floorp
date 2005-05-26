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

#import "Downloads.h"
#import "NSString+Utils.h"

#include "nsCOMPtr.h"
#include "nsILocalFileMac.h"
#include "nsDirectoryServiceDefs.h"

const int kDefaultExpireDays = 9;

@interface OrgMozillaChimeraPreferenceDownloads(Private)

- (NSString*)getInternetConfigString:(ConstStr255Param)icPref;
- (NSString*)getDownloadFolderDescription;
- (void)setupDownloadMenuWithPath:(NSString*)inDLPath;
- (void)setDownloadFolder:(NSString*)inNewFolder;

@end

// necessary because we're building against the 10.2 SDK and need to use some
// private APIs to get at a feature that's hidden in on 10.2
@interface NSOpenPanel(NewFolderExtensionsFor102SDK)
-(void)setCanCreateDirectories:(BOOL)inInclude;      // exists in 10.3 SDK
-(void)_setIncludeNewFolderButton:(BOOL)inInclude;   // exists on 10.2 as private api
@end

@implementation OrgMozillaChimeraPreferenceDownloads

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
  
  [mAutoCloseDLManager setState:![self getBooleanPref:"browser.download.progressDnldDialog.keepAlive" withSuccess:&gotPref]];
  [mEnableHelperApps setState:[self getBooleanPref:"browser.download.autoDispatch" withSuccess:&gotPref]];

  NSString* downloadFolderDesc = [self getDownloadFolderDescription];
  if ([downloadFolderDesc length] == 0)
    downloadFolderDesc = [self getLocalizedString:@"MissingDlFolder"];
  
  [self setupDownloadMenuWithPath:downloadFolderDesc];
  
//  [mDownloadFolder setStringValue:[self getDownloadFolderDescription]];
}

- (IBAction)checkboxClicked:(id)sender
{
  if (!mPrefService)
    return;

  if (sender == mAutoCloseDLManager) {
    [self setPref:"browser.download.progressDnldDialog.keepAlive" toBoolean:![sender state]];
  }
  if (sender == mEnableHelperApps) {
    [self setPref:"browser.download.autoDispatch" toBoolean:[sender state]];
  }
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

// Sets the IC download pref to the given path
// NOTE: THIS DOES NOT WORK.
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
  [placeholder setTitle:[[NSFileManager defaultManager] displayNameAtPath:inDLPath]];
  [placeholder setImage:icon];
  
  // ensure first item is selected
  [mDownloadFolder selectItemAtIndex:0];
}

// display a file picker sheet allowing the user to set their new download folder
- (IBAction)chooseDownloadFolder:(id)sender
{
  NSString* oldDLFolder = [self getDownloadFolderDescription];
  NSOpenPanel* panel = [NSOpenPanel openPanel];
  [panel setCanChooseFiles:NO];
  [panel setCanChooseDirectories:YES];
  [panel setAllowsMultipleSelection:NO];
  if ([panel respondsToSelector:@selector(setCanCreateDirectories:)])
    [panel setCanCreateDirectories:YES];
  else if ([panel respondsToSelector:@selector(_setIncludeNewFolderButton:)]) 	// try private API in 10.2
    [panel _setIncludeNewFolderButton:YES];
  [panel setPrompt:NSLocalizedString(@"ChooseDirectoryOKButton", @"")];
  
  [panel beginSheetForDirectory:oldDLFolder file:nil types:nil modalForWindow:[mDownloadFolder window]
           modalDelegate:self didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
           contextInfo:nil];
}

// called when the user closes the open panel sheet for selecting a new d/l folder.
// if they clicked ok, change the IC pref and re-display the new choice in the
// popup menu
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
