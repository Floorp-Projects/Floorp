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

-(void) tryAddImportFromBrowser: (NSString *) aBrowserName withBookmarkPath: (NSString *) aPath;
-(void) tryOmniWeb5Import;
-(void) buildButtonForBrowser:(NSString *) aBrowserName withObject:(id)anObject;
-(NSString *) getSaltedBookmarkPathForProfile: (NSString *) aPath;
-(void) beginImportFrom:(NSString *) aPath intoFolder:(BookmarkFolder *) aFolder;
-(void) beginOmniWeb5ImportFrom:(NSArray *)anArray intoFolder:(BookmarkFolder *)aFolder;
-(void) finishImport:(BOOL)success fromFile:(NSString *)aFile intoFolder:(BookmarkFolder*)aFolder;

@end

#pragma mark -

@implementation BookmarkImportDlgController

-(void) windowDidLoad
{
  [self buildAvailableFileList];
  [[self window] center];
}

// Check for common webbrower bookmark files and, if they exist, add import buttons.
-(void) buildAvailableFileList
{
  NSString *mozPath;
  
  // Remove everything but the separator and "Select a file..." option, on the off-chance that someone brings
  // up the import dialog, throws away a profile, then brings up the import dialog again
  while ([mBrowserListButton numberOfItems] > 2)
    [mBrowserListButton removeItemAtIndex:0];
  
  [self tryAddImportFromBrowser:@"iCab" withBookmarkPath:@"~/Library/Preferences/iCab Preferences/Hotlist.html"];
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
-(void) tryAddImportFromBrowser: (NSString *) aBrowserName withBookmarkPath: (NSString *) aPath
{
  NSFileManager *fm = [NSFileManager defaultManager];
  NSString *fullPathString = [aPath stringByStandardizingPath];
  if ([fm fileExistsAtPath:fullPathString]) {
    [self buildButtonForBrowser:aBrowserName withObject:fullPathString];
  }
}

// Special treatment for OmniWeb 5
-(void) tryOmniWeb5Import
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
  while ( (aPath = [enumerator nextObject]) ) {
    fullPathString = [aPath stringByStandardizingPath];
    if ([fm fileExistsAtPath:fullPathString]) {
      [haveFiles addObject:fullPathString];
    }
  }
  if ([haveFiles count] > 0)
  {
    [self buildButtonForBrowser:@"OmniWeb 5" withObject:haveFiles];
  }
}

// Given a Mozilla-like profile, returns the bookmarks.html file in the salt directory,
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

-(void) buildButtonForBrowser:(NSString *) aBrowserName withObject:(id)aThingy
{
  [mBrowserListButton insertItemWithTitle:aBrowserName atIndex:0];
  NSMenuItem *browserItem = [mBrowserListButton itemAtIndex:0];
  [browserItem setTarget:self];
  [browserItem setAction:@selector(nullAction:)];
  [browserItem setRepresentedObject:aThingy];  
}

// keeps browsers turned on
-(IBAction) nullAction:(id)aSender
{
}

-(IBAction) cancel:(id)aSender
{
  [[self window] orderOut:self];

}

-(IBAction) import:(id)aSender
{
  // XXX show a spinner, disable the button, postpone to next event loop (to give UI time to update)
  NSMenuItem *selectedItem = [mBrowserListButton selectedItem];
  BookmarkFolder *importFolder = [[[BookmarkManager sharedBookmarkManager] rootBookmarks] addBookmarkFolder];
  NSString *titleString;
  if ([[selectedItem title] isEqualToString:@"Internet Explorer"])
    titleString = [[NSString alloc] initWithString:NSLocalizedString(@"Imported IE Favorites", @"")];
  else
    titleString = [[NSString alloc] initWithFormat:NSLocalizedString(@"Imported %@ Bookmarks", @""), [selectedItem title]];
  [importFolder setTitle:titleString];
  [titleString release];
  // Stupid OmniWeb 5 gets its own import function
  if ( [[selectedItem title] isEqualToString:@"OmniWeb 5"] ) {
    [self beginOmniWeb5ImportFrom:[selectedItem representedObject] intoFolder:importFolder];
  }
  else {
    [self beginImportFrom:[selectedItem representedObject] intoFolder:importFolder];
  }
}

-(IBAction) loadOpenPanel:(id)aSender
{
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseFiles: YES];
  [openPanel setCanChooseDirectories: NO];
  [openPanel setAllowsMultipleSelection: NO];
  [openPanel setPrompt: @"Import"];
  NSArray* array = [NSArray arrayWithObjects: @"htm",@"html",@"xml", @"plist",nil];
  int result = [openPanel runModalForDirectory: nil
                                          file: nil
                                         types: array];
  if (result == NSOKButton) {
    NSString *pathToFile = [[openPanel filenames] objectAtIndex:0];
    BookmarkFolder *importFolder = [[[BookmarkManager sharedBookmarkManager] rootBookmarks] addBookmarkFolder];
    [importFolder setTitle:NSLocalizedString(@"Imported Bookmarks",@"Imported Bookmarks")];
    [self beginImportFrom:pathToFile intoFolder:importFolder];
  }
}

-(void) beginOmniWeb5ImportFrom:(NSArray *)anArray intoFolder:(BookmarkFolder *) aFolder
{
  [mCancelButton setEnabled:NO];
  [mImportButton setEnabled:NO];
  
  NSEnumerator *enumerator = [anArray objectEnumerator];

  NSString* curFilename = nil;
  
  BOOL success = TRUE;
  NSString *curPath;
  while ( success && (curPath = [enumerator nextObject] ) )
  {
    curFilename = [curPath lastPathComponent];
    // What folder we import into depends on what OmniWeb file we're importing.
    if ([curFilename isEqualToString:@"Bookmarks.html"] )
    {
      success = [[BookmarkManager sharedBookmarkManager] importBookmarks:curPath intoFolder:aFolder];
    }
    else if ([curFilename isEqualToString:@"Favorites.html"] )
    {
      BookmarkFolder *favoriteFolder = [aFolder addBookmarkFolder];
      [favoriteFolder setTitle:NSLocalizedString(@"OmniWeb Favorites", @"OmniWeb Favorites")];
      success = [[BookmarkManager sharedBookmarkManager] importBookmarks:curPath intoFolder:favoriteFolder];
    }
    else if ([curFilename isEqualToString:@"Published.html"])
    {
      BookmarkFolder *publishedFolder = [aFolder addBookmarkFolder];
      [publishedFolder setTitle:NSLocalizedString(@"OmniWeb Published", @"OmniWeb Published")];
      success = [[BookmarkManager sharedBookmarkManager] importBookmarks:curPath intoFolder:publishedFolder];
    }    
  }
  
  [mCancelButton setEnabled:YES];
  [mImportButton setEnabled:YES];

  // Non-rigorous reporting of errors
  [self finishImport:success fromFile:curFilename intoFolder:aFolder];
}

-(void) beginImportFrom:(NSString *) aPath intoFolder:(BookmarkFolder *) aFolder
{
  [mCancelButton setEnabled:NO];
  [mImportButton setEnabled:NO];  
  
  BOOL success = [[BookmarkManager sharedBookmarkManager] importBookmarks:aPath intoFolder:aFolder];
  
  [mCancelButton setEnabled:YES];
  [mImportButton setEnabled:YES];
  
  [self finishImport:success fromFile:[aPath lastPathComponent] intoFolder:aFolder];
  
}

-(void) finishImport:(BOOL)success fromFile:(NSString *)aFile intoFolder:(BookmarkFolder*)aFolder
{
  //show them the imported bookmarks if import succeeded
  if (success)
  {
    [[self window] orderOut:self];
    BrowserWindowController* windowController = [(MainController *)[NSApp delegate] openBrowserWindowWithURL:@"about:bookmarks" andReferrer:nil behind:nil allowPopups:NO];
    BookmarkViewController*  bmController = [windowController bookmarkViewController];
    [bmController setItemToRevealOnLoad:aFolder];
  }
  else
  {
    NSBeginAlertSheet(NSLocalizedString(@"ImportFailureTitle", @"Import Failed"),  // title
                      @"",               // default button
                      nil,               // no cancel buttton
                      nil,               // no third button
                      [self window],     // window
                      self,              // delegate
                      @selector(alertSheetDidEnd:returnCode:contextInfo:),
                      nil,               // no dismiss sel
                      (void *)NULL,      // no context
                      [NSString stringWithFormat:NSLocalizedString(@"ImportFailureMessage", @"The file '%@' is not a supported bookmark file type."), aFile]
                      );
  }
}

-(void) alertSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  [[self window] orderOut:self];
}

@end
