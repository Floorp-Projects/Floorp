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
 *   josh@mozilla.com (Josh Aas)
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

// handly stack-based class to start and stop an Internet Config session
class StInternetConfigSession
{
public:

  StInternetConfigSession(OSType inSignature)
  : mICInstance(NULL)
  , mStartedOK(false)
  {
    mStartedOK = (::ICStart(&mICInstance, inSignature) == noErr);
  }
  
  ~StInternetConfigSession()
  {
    if (mStartedOK)
      ::ICStop(mICInstance);
  }
  
  bool        Available() const { return mStartedOK;  }
  ICInstance  Instance() const  { return mICInstance; }

private:

  ICInstance    mICInstance;
  bool          mStartedOK;
};



@interface OrgMozillaChimeraPreferenceDownloads(Private)

- (NSString*)getDownloadFolderDescription;
- (void)setupDownloadMenuWithPath:(NSString*)inDLPath;
- (void)setDownloadFolder:(NSString*)inNewFolder;

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
  [mDownloadRemovalPolicy selectItem:[[mDownloadRemovalPolicy menu] itemWithTag:[self getIntPref:"browser.download.downloadRemoveAction" withSuccess:&gotPref]]];

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

// Sets the IC download pref to the given path. We write to Internet Config
// because Gecko reads from IC when getting NS_MAC_DEFAULT_DOWNLOAD_DIR.
- (void)setDownloadFolder:(NSString*)inNewFolder
{
  if (!inNewFolder)
    return;

  // it would be nice to use PreferenceManager, but I don't want to drag
  // all that code into the plugin
  StInternetConfigSession icSession('MOZC');
  if (!icSession.Available())
    return;
  
  // make a ICFileSpec out of our path and shove it into IC. This requires
  // creating an FSSpec and an alias.
  FSRef fsRef;
  Boolean isDir;
  OSStatus error = ::FSPathMakeRef((UInt8 *)[inNewFolder fileSystemRepresentation], &fsRef, &isDir);
  if (error != noErr)
    return;

  FSSpec fsSpec;
  error = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNone, nil, nil, &fsSpec, nil);
  if (error != noErr)
    return;

  AliasHandle alias = nil;
  error = ::FSNewAlias(nil, &fsRef, &alias);
  
  // copy the data out of our variables into the ICFileSpec and hand it to IC.
  if (error == noErr && alias)
  {
    long headerSize = offsetof(ICFileSpec, alias);
    long aliasSize = ::GetHandleSize((Handle)alias);
    ICFileSpec* realbuffer = (ICFileSpec*) calloc(headerSize + aliasSize, 1);
    realbuffer->fss = fsSpec;
    memcpy(&realbuffer->alias, *alias, aliasSize);
    ::ICSetPref(icSession.Instance(), kICDownloadFolder, kICAttrNoChange, (const void*)realbuffer, headerSize + aliasSize);
    free(realbuffer);
    ::DisposeHandle((Handle)alias);
  }
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
  [panel setCanCreateDirectories:YES];
  [panel setPrompt:NSLocalizedString(@"ChooseDirectoryOKButton", @"")];
  
  [panel beginSheetForDirectory:oldDLFolder file:nil types:nil modalForWindow:[mDownloadFolder window]
           modalDelegate:self didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
           contextInfo:nil];
}

//
// Set the download removal policy
//
- (IBAction)chooseDownloadRemovalPolicy:(id)sender
{
  // The three options in the popup contains tags 0-2, set the pref according to the 
  // selected menu item's tag.
  int selectedTagValue = [mDownloadRemovalPolicy selectedTag];
  [self setPref:"browser.download.downloadRemoveAction" toInt:selectedTagValue];
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
