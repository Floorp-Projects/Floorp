/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Haas <haasd@cae.wisc.edu>
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

#import "BookmarkImportDlgController.h"
#import "BookmarkManager.h"
#import "BookmarkFolder.h"
#import "MainController.h"
#import "BrowserWindowController.h"
#import "BookmarkViewController.h"

@interface BookmarkImportDlgController (Private)

- (void)tryAddImportFromBrowser:(NSString *)aBrowserName withBookmarkPath:(NSString *)aPath;
- (void)tryOmniWeb5Import;
- (void)buildButtonForBrowser:(NSString *)aBrowserName withPathArray:(NSArray *)anArray;
- (NSString *)getSaltedBookmarkPathForProfile:(NSString *)aPath;
- (void)beginImportFrom:(NSArray *)aPath withTitles:(NSArray *)anArray;
- (void)beginOmniWeb5ImportFrom:(NSArray *)anArray;
- (void)finishImport:(BOOL)success fromFiles:(NSArray *)anArray;
- (void)finishThreadedImport:(BOOL)success fromFiles:(NSArray *)anArray;
- (void)showProgressView;
- (void)showImportView;

@end

#pragma mark -

@implementation BookmarkImportDlgController

- (void)windowDidLoad
{
  [self showImportView];
  [self buildAvailableFileList];
}

// Check for common webbrower bookmark files and, if they exist, add import buttons.
- (void)buildAvailableFileList
{
  NSString *mozPath;

  // Remove everything but the separator and "Select a file..." option, on the off-chance that someone brings
  // up the import dialog, throws away a profile, then brings up the import dialog again
  while ([mBrowserListButton numberOfItems] > 2)
    [mBrowserListButton removeItemAtIndex:0];

  [self tryAddImportFromBrowser:@"iCab" withBookmarkPath:@"~/Library/Preferences/iCab Preferences/Hotlist.html"];
  [self tryAddImportFromBrowser:@"iCab 3" withBookmarkPath:@"~/Library/Preferences/iCab Preferences/Hotlist3.html"];
  [self tryAddImportFromBrowser:@"Opera" withBookmarkPath:@"~/Library/Preferences/Opera Preferences/Bookmarks"];
  [self tryAddImportFromBrowser:@"OmniWeb 4" withBookmarkPath:@"~/Library/Application Support/Omniweb/Bookmarks.html"];
  // OmniWeb 5 has between 0 and 3 bookmark files.
  [self tryOmniWeb5Import];
  [self tryAddImportFromBrowser:@"Internet Explorer" withBookmarkPath:@"~/Library/Preferences/Explorer/Favorites.html"];
  [self tryAddImportFromBrowser:@"Safari" withBookmarkPath:@"~/Library/Safari/Bookmarks.plist"];

  mozPath = [self getSaltedBookmarkPathForProfile:@"~/Library/Mozilla/Profiles/default/"];
  if (mozPath)
    [self tryAddImportFromBrowser:@"Netscape/Mozilla" withBookmarkPath:mozPath];

  // Try Firefox from different locations in the reverse order of their introduction
  mozPath = [self getSaltedBookmarkPathForProfile:@"~/Library/Application Support/Firefox/Profiles/"];
  if (!mozPath)
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
- (void)tryAddImportFromBrowser:(NSString *)aBrowserName withBookmarkPath:(NSString *)aPath
{
  NSFileManager *fm = [NSFileManager defaultManager];
  NSString *fullPathString = [aPath stringByStandardizingPath];
  if ([fm fileExistsAtPath:fullPathString]) {
    [self buildButtonForBrowser:aBrowserName withPathArray:[NSArray arrayWithObject:fullPathString]];
  }
}

// Special treatment for OmniWeb 5
- (void)tryOmniWeb5Import
{
  NSArray *owFiles = [NSArray arrayWithObjects:
    @"~/Library/Application Support/OmniWeb 5/Bookmarks.html",
    @"~/Library/Application Support/OmniWeb 5/Favorites.html",
    @"~/Library/Application Support/OmniWeb 5/Published.html",
    nil];
  NSMutableArray *haveFiles = [NSMutableArray array];
  NSEnumerator *enumerator = [owFiles objectEnumerator];
  NSFileManager *fm = [NSFileManager defaultManager];
  NSString *aPath, *fullPathString;
  while ((aPath = [enumerator nextObject])) {
    fullPathString = [aPath stringByStandardizingPath];
    if ([fm fileExistsAtPath:fullPathString]) {
      [haveFiles addObject:fullPathString];
    }
  }
  if ([haveFiles count] > 0)
    [self buildButtonForBrowser:@"OmniWeb 5" withPathArray:haveFiles];
}

// Given a Mozilla-like profile, returns the bookmarks.html file in the salt directory,
// or nil if no salt directory is found
- (NSString *)getSaltedBookmarkPathForProfile:(NSString *)aPath
{
  NSFileManager *fm = [NSFileManager defaultManager];
  id aTempItem;
  NSString *fullPathString = [aPath stringByStandardizingPath];
  if ([fm fileExistsAtPath:fullPathString]) {
    NSArray *saltArray = [fm directoryContentsAtPath:fullPathString];
    NSEnumerator *enumerator = [saltArray objectEnumerator];
    while ((aTempItem = [enumerator nextObject])) {
      if (![aTempItem hasPrefix:@"."])
        return [fullPathString stringByAppendingFormat:@"/%@/bookmarks.html", aTempItem];
    }
  }
  return nil;
}

- (void)buildButtonForBrowser:(NSString *)aBrowserName withPathArray:(NSArray *)anArray
{
  [mBrowserListButton insertItemWithTitle:aBrowserName atIndex:0];
  NSMenuItem *browserItem = [mBrowserListButton itemAtIndex:0];
  [browserItem setTarget:self];
  [browserItem setAction:@selector(nullAction:)];
  [browserItem setRepresentedObject:anArray];
}

// keeps browsers turned on
- (IBAction)nullAction:(id)aSender
{
}

- (IBAction)cancel:(id)aSender
{
  [[self window] orderOut:self];

}

- (IBAction)import:(id)aSender
{
  NSMenuItem *selectedItem = [mBrowserListButton selectedItem];
  NSString *titleString;
  if ([[selectedItem title] isEqualToString:@"Internet Explorer"])
    titleString = [NSString stringWithString:NSLocalizedString(@"Imported IE Favorites", nil)];
  else
    titleString = [NSString stringWithFormat:NSLocalizedString(@"Imported %@ Bookmarks", nil), [selectedItem title]];
  // Stupid OmniWeb 5 gets its own import function
  if ([[selectedItem title] isEqualToString:@"OmniWeb 5"]) {
      [self beginOmniWeb5ImportFrom:[selectedItem representedObject]];
  }
  else {
    [self beginImportFrom:[selectedItem representedObject] withTitles:[NSArray arrayWithObject:titleString]];
  }
}

- (IBAction)loadOpenPanel:(id)aSender
{
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseFiles:YES];
  [openPanel setCanChooseDirectories:NO];
  [openPanel setAllowsMultipleSelection:NO];
  [openPanel setPrompt:@"Import"];
  NSArray* array = [NSArray arrayWithObjects:@"htm", @"html", @"xml", @"plist", nil];
  int result = [openPanel runModalForDirectory:nil
                                          file:nil
                                         types:array];
  if (result == NSOKButton) {
    NSString *pathToFile = [[openPanel filenames] objectAtIndex:0];
    [self beginImportFrom:[NSArray arrayWithObject:pathToFile]
               withTitles:[NSArray arrayWithObject:NSLocalizedString(@"Imported Bookmarks", nil)]];
  }
}

- (void)beginOmniWeb5ImportFrom:(NSArray *)anArray
{
  NSEnumerator *enumerator = [anArray objectEnumerator];
  NSMutableArray *titleArray= [NSMutableArray array];
  NSString* curFilename = nil;
  NSString *curPath = nil;
  while (curPath = [enumerator nextObject]) {
    curFilename = [curPath lastPathComponent];
    // What folder we import into depends on what OmniWeb file we're importing.
    if ([curFilename isEqualToString:@"Bookmarks.html"])
      [titleArray addObject:NSLocalizedString(@"Imported OmniWeb 5 Bookmarks", nil)];
    else if ([curFilename isEqualToString:@"Favorites.html"])
      [titleArray addObject:NSLocalizedString(@"OmniWeb Favorites", nil)];
    else if ([curFilename isEqualToString:@"Published.html"])
      [titleArray addObject:NSLocalizedString(@"OmniWeb Published", nil)];
  }
  [self beginImportFrom:anArray withTitles:titleArray];
}

- (void)beginImportFrom:(NSArray *)aPathArray withTitles:(NSArray *)aTitleArray
{
  [self showProgressView];
  NSDictionary *aDict = [NSDictionary dictionaryWithObjectsAndKeys:aPathArray, kBookmarkImportPathIndentifier,
    aTitleArray, kBookmarkImportNewFolderNameIdentifier, nil];
  [NSThread detachNewThreadSelector:@selector(importBookmarksThreadEntry:)
                           toTarget:[BookmarkManager sharedBookmarkManager]
                         withObject:aDict];
}

- (void)finishThreadedImport:(BOOL)success fromFile:(NSString *)aFile
{
  if (success) {
    BrowserWindowController* windowController = [(MainController *)[NSApp delegate] openBrowserWindowWithURL:@"about:bookmarks"
                                                                                                 andReferrer:nil
                                                                                                      behind:nil
                                                                                                 allowPopups:NO];
    BookmarkViewController*  bmController = [windowController bookmarkViewController];
    BookmarkFolder *rootFolder = [[BookmarkManager sharedBookmarkManager] rootBookmarks];
    BookmarkFolder *newFolder = [rootFolder objectAtIndex:([rootFolder count] - 1)];
    [bmController setItemToRevealOnLoad:newFolder];
  }
  else {
    NSBeginAlertSheet(NSLocalizedString(@"ImportFailureTitle", nil),  // title
                      @"",               // default button
                      nil,               // no cancel buttton
                      nil,               // no third button
                      [self window],     // window
                      self,              // delegate
                      @selector(alertSheetDidEnd:returnCode:contextInfo:),
                      nil,               // no dismiss sel
                      (void *)NULL,      // no context
                      [NSString stringWithFormat:NSLocalizedString(@"ImportFailureMessage", nil), aFile]
                      );
  }
  [[self window] orderOut:self];
  [self showImportView];
}

- (void)alertSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  [[self window] orderOut:self];
}

- (void)showProgressView
{
  NSSize viewSize = [mProgressView frame].size;
  [[self window] setContentView:mProgressView];
  [[self window] setContentSize:viewSize];
  [[self window] center];
  [mImportProgressBar setUsesThreadedAnimation:YES];
  [mImportProgressBar startAnimation:self];
}

- (void)showImportView
{
  [mImportProgressBar stopAnimation:self];
  NSSize viewSize = [mImportView frame].size;
  [[self window] setContentView:mImportView];
  [[self window] setContentSize:viewSize];
  [[self window] center];
}

@end
