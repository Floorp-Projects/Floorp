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

@implementation BookmarkImportDlgController

-(void) windowDidLoad
{
  [self buildAvailableFileList];
}

-(void) buildAvailableFileList
{
  // check for common webbrower bookmark files and, if they exist, add to button.
  NSFileManager *fm = [NSFileManager defaultManager];
  NSMenuItem *browserItem;
  NSEnumerator *enumerator;
  id aTempItem;
  // iCab
  NSString *pathString = [[NSString stringWithString:@"~/Library/Preferences/iCab Preferences/Hotlist.html"] stringByExpandingTildeInPath];
  if ([fm fileExistsAtPath:pathString]) {
    [mBrowserListButton insertItemWithTitle:@"iCab" atIndex:0];
    browserItem = [mBrowserListButton itemAtIndex:0];
    [browserItem setTarget:self];
    [browserItem setAction:@selector(nullAction:)];
    [browserItem setRepresentedObject:pathString];
  }
  // Firebird
  pathString = [[NSString stringWithString:@"~/Library/Phoenix/Profiles/default/"] stringByExpandingTildeInPath];
  if ([fm fileExistsAtPath:pathString]) {
    NSArray *saltArray = [fm directoryContentsAtPath:pathString];
    enumerator = [saltArray objectEnumerator];
    while ((aTempItem = [enumerator nextObject])) {
      if (![aTempItem hasPrefix:@"."])
        break;
    }
    if (aTempItem) {
      pathString = [pathString stringByAppendingFormat:@"/%@/bookmarks.html",aTempItem];
      if ([fm fileExistsAtPath:pathString]) {
        [mBrowserListButton insertItemWithTitle:@"Mozilla Firebird" atIndex:0];
        browserItem = [mBrowserListButton itemAtIndex:0];
        [browserItem setTarget:self];
        [browserItem setAction:@selector(nullAction:)];
        [browserItem setRepresentedObject:pathString];
      }
    }
  }

  // Netscape/Mozilla
  pathString = [[NSString stringWithString:@"~/Library/Mozilla/Profiles/default/"] stringByExpandingTildeInPath];
  if ([fm fileExistsAtPath:pathString]) {
    NSArray *saltArray = [fm directoryContentsAtPath:pathString];
    enumerator = [saltArray objectEnumerator];
    while ((aTempItem = [enumerator nextObject])) {
      if (![aTempItem hasPrefix:@"."])
        break;
    }
    if (aTempItem) {
      pathString = [pathString stringByAppendingFormat:@"/%@/bookmarks.html",aTempItem];
      if ([fm fileExistsAtPath:pathString]) {
        [mBrowserListButton insertItemWithTitle:@"Netscape/Mozilla" atIndex:0];
        browserItem = [mBrowserListButton itemAtIndex:0];
        [browserItem setTarget:self];
        [browserItem setAction:@selector(nullAction:)];
        [browserItem setRepresentedObject:pathString];
      }
    }
  }

  // Omniweb
  pathString = [[NSString stringWithString:@"~/Library/Application Support/Omniweb/Bookmarks.html"] stringByStandardizingPath];
  if ([fm fileExistsAtPath:pathString]) {
    [mBrowserListButton insertItemWithTitle:@"Omniweb" atIndex:0];
    browserItem = [mBrowserListButton itemAtIndex:0];
    [browserItem setTarget:self];
    [browserItem setAction:@selector(nullAction:)];
    [browserItem setRepresentedObject:pathString];
  }

  // IE
  pathString = [[NSString stringWithString:@"~/Library/Preferences/Explorer/Favorites.html"] stringByStandardizingPath];
  if ([fm fileExistsAtPath:pathString]) {
    [mBrowserListButton insertItemWithTitle:@"Internet Explorer" atIndex:0];
    browserItem = [mBrowserListButton itemAtIndex:0];
    [browserItem setTarget:self];
    [browserItem setAction:@selector(nullAction:)];
    [browserItem setRepresentedObject:pathString];
  }

  // Safari
 pathString = [[NSString stringWithString:@"~/Library/Safari/Bookmarks.plist"] stringByStandardizingPath];
  if ([fm fileExistsAtPath:pathString]) {
    [mBrowserListButton insertItemWithTitle:@"Safari" atIndex:0];
    browserItem = [mBrowserListButton itemAtIndex:0];
    [browserItem setTarget:self];
    [browserItem setAction:@selector(nullAction:)];
    [browserItem setRepresentedObject:pathString];
  }
  [mBrowserListButton selectItemAtIndex:0];
  [mBrowserListButton synchronizeTitleAndSelectedItem];
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
