/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 2002
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*    David Haas <haasd@cae.wisc.edu>
*
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

#import "BookmarkImportDlgController.h"
#import "BookmarkManager.h"
#import "BookmarkFolder.h"

@interface BookmarkImportDlgController (Private)

-(void) tryAddImportFromBrowser: (NSString *) aBrowserName withBookmarkPath: (NSString *) aPath;
-(NSString *) getSaltedBookmarkPathForProfile: (NSString *) aPath;

@end

@implementation BookmarkImportDlgController

-(void) windowDidLoad
{
  [self buildAvailableFileList];
}

// Check for common webbrower bookmark files and, if they exist, add import buttons.
-(void) buildAvailableFileList
{
  NSString *mozPath;
  
  // Remove everything but the "Select a File..." option, on the off-chance that someone brings
  // up the import dialog, throws away a profile, then brings up the import dialog again
  while ([mBrowserListButton numberOfItems] > 1)
    [mBrowserListButton removeItemAtIndex:0];
  
  [self tryAddImportFromBrowser:@"iCab" withBookmarkPath:@"~/Library/Preferences/iCab Preferences/Hotlist.html"];
  [self tryAddImportFromBrowser:@"Omniweb" withBookmarkPath:@"~/Library/Application Support/Omniweb/Bookmarks.html"];
  [self tryAddImportFromBrowser:@"Internet Explorer" withBookmarkPath:@"~/Library/Preferences/Explorer/Favorites.html"];
  [self tryAddImportFromBrowser:@"Safari" withBookmarkPath:@"~/Library/Safari/Bookmarks.plist"];
  
  mozPath = [self getSaltedBookmarkPathForProfile:@"~/Library/Mozilla/Profiles/default/"];
  if (mozPath)
    [self tryAddImportFromBrowser:@"Netscape/Mozilla" withBookmarkPath:mozPath];
  
  // Try Firefox from the new ~/Library/Firefox location, then ~/Library/Phoenix if that fails
  mozPath = [self getSaltedBookmarkPathForProfile:@"~/Library/Firefox/Profiles/default/"];
  if (!mozPath)
    mozPath = [self getSaltedBookmarkPathForProfile:@"~/Library/Phoenix/Profiles/default/"];
  if (mozPath)
    [self tryAddImportFromBrowser:@"Mozilla Firefox" withBookmarkPath:mozPath];
  
  [mBrowserListButton selectItemAtIndex:0];
  [mBrowserListButton synchronizeTitleAndSelectedItem];
}

// Checks for the existence of the specified bookmarks file, and adds an import option for
// the given browser if the file is found.
-(void) tryAddImportFromBrowser: (NSString *) aBrowserName withBookmarkPath: (NSString *) aPath
{
  NSFileManager *fm = [NSFileManager defaultManager];
  NSString *fullPathString = [aPath stringByStandardizingPath];
  if ([fm fileExistsAtPath:fullPathString]) {
    [mBrowserListButton insertItemWithTitle:aBrowserName atIndex:0];
    NSMenuItem *browserItem = [mBrowserListButton itemAtIndex:0];
    [browserItem setTarget:self];
    [browserItem setAction:@selector(nullAction:)];
    [browserItem setRepresentedObject:fullPathString];
  }
}

// Given a Mozilla-like profile, returns the bookmarss.html file in the salt directory,
// or nil if no salt directory is found
-(NSString *) getSaltedBookmarkPathForProfile: (NSString *) aPath
{
  NSFileManager *fm = [NSFileManager defaultManager];
  id aTempItem;
  NSString *fullPathString = [aPath stringByStandardizingPath];
  if ([fm fileExistsAtPath:fullPathString]) {
    NSArray *saltArray = [fm directoryContentsAtPath:fullPathString];
    NSEnumerator *enumerator = [saltArray objectEnumerator];
    while ((aTempItem = [enumerator nextObject])) {
      if (![aTempItem hasPrefix:@"."])
        return [fullPathString stringByAppendingFormat:@"/%@/bookmarks.html",aTempItem];
    }
  }
  return nil;
}

// keeps browsers turned on
-(IBAction) nullAction:(id)aSender
{
}

-(IBAction) cancel:(id)aSender
{
  [NSApp stopModal];
  [NSApp endSheet:[self window]];
  [[self window] orderOut:self];

}
-(IBAction) import:(id)aSender
{
  [NSApp stopModal];
  [NSApp endSheet:[self window]];
  [[self window] orderOut:self];
  NSMenuItem *selectedItem = [mBrowserListButton selectedItem];
  BookmarkFolder *importFolder = [[[BookmarkManager sharedBookmarkManager] rootBookmarks] addBookmarkFolder];
  NSString *titleString;
  if ([[selectedItem title] isEqualToString:@"Internet Explorer"])
    titleString = [[NSString alloc] initWithString:NSLocalizedString(@"Imported IE Favorites",@"Imported IE Favorites")];
  else
    titleString = [[NSString alloc] initWithFormat:NSLocalizedString(@"Imported %@ Bookmarks",@"Imported %@ Bookmarks"),[selectedItem title]];
  [importFolder setTitle:titleString];
  [titleString release];
  [[BookmarkManager sharedBookmarkManager] importBookmarks:[selectedItem representedObject] intoFolder:importFolder];
}

-(IBAction) loadOpenPanel:(id)aSender
{
  [NSApp stopModal];
  [NSApp endSheet:[self window]];
  [[self window] orderOut:self];
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseFiles: YES];
  [openPanel setCanChooseDirectories: NO];
  [openPanel setAllowsMultipleSelection: NO];
  NSArray* array = [NSArray arrayWithObjects: @"htm",@"html",@"xml", @"plist",nil];
  int result = [openPanel runModalForDirectory: nil
                                          file: nil
                                         types: array];
  if (result == NSOKButton) {
    NSString *pathToFile = [[openPanel filenames] objectAtIndex:0];
    BookmarkManager *bm = [BookmarkManager sharedBookmarkManager];
    BookmarkFolder *importFolder = [[bm rootBookmarks] addBookmarkFolder];
    [importFolder setTitle:NSLocalizedString(@"Imported Bookmarks",@"Imported Bookmarks")];
    [bm importBookmarks:pathToFile intoFolder:importFolder];
  }
}

@end
