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
*   Ben Goodger   <ben@netscape.com> (Original Author)
*   Simon Fraser  <sfraser@netscape.com>
*   David Haas    <haasd@cae.wisc.edu>
*/

#import "NSString+Utils.h"
#import "BookmarkInfoController.h"
#import "Bookmark.h"
#import "BookmarkFolder.h"

// determined through weeks of trial and error
#define kMaxLengthOfWindowTitle 49

@interface BookmarkInfoController(Private)

- (void)commitChanges:(id)sender;
- (void)updateUI:(BookmarkItem *)anItem;

@end;

@implementation BookmarkInfoController

/* BookmarkInfoController singelton */
static BookmarkInfoController *sharedBookmarkInfoController = nil;

+ (id)sharedBookmarkInfoController
{
  if (!sharedBookmarkInfoController) {
    sharedBookmarkInfoController = [[BookmarkInfoController alloc] initWithWindowNibName:@"BookmarkInfoPanel"];
  }
  return sharedBookmarkInfoController;
}

+ (void)closeBookmarkInfoController
{
  if (sharedBookmarkInfoController)
    [sharedBookmarkInfoController close];
}

- (id)initWithWindowNibName:(NSString *)windowNibName
{
  if ((self = [super initWithWindowNibName:@"BookmarkInfoPanel"]))
  {
    //custom field editor lets us undo our changes
    mFieldEditor = [[NSTextView alloc] init];
    [mFieldEditor setAllowsUndo:YES];
    [mFieldEditor setFieldEditor:YES];
  }
  return self;
}

- (void)windowDidLoad
{
  // keep a ref so that we can remove and add to the its superview with impunity
  [[mBookmarkView retain] removeFromSuperview];
  [[mFolderView retain] removeFromSuperview];
  // find the TabViewItems
  mBookmarkInfoTabView = [mTabView tabViewItemAtIndex:[mTabView indexOfTabViewItemWithIdentifier:@"bminfo"]];
  mBookmarkUpdateTabView = [mTabView tabViewItemAtIndex:[mTabView indexOfTabViewItemWithIdentifier:@"bmupdate"]];
  // Generic notifications for Bookmark Client - only care if there's a deletion
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
}

-(void)dealloc
{
  // this is never called
  if (self == sharedBookmarkInfoController)
    sharedBookmarkInfoController = nil;

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mBookmarkItem release];
  mBookmarkItem = nil;
  [mFieldEditor release];
  [mBookmarkView release];
  [mFolderView release];
  [super dealloc];
}

-(void)controlTextDidEndEditing: (NSNotification*) aNotification
{
  [self commitChanges:[aNotification object]];
  [[mFieldEditor undoManager] removeAllActions];
}

-(void)windowDidBecomeKey:(NSNotification*) aNotification
{
  if ([mDummyView contentView] == mBookmarkView) {
    NSTabViewItem *tabViewItem = [mTabView selectedTabViewItem];
    if (tabViewItem == mBookmarkInfoTabView)
      [[self window] makeFirstResponder:mBookmarkNameField];
    else if (tabViewItem == mBookmarkUpdateTabView)
      [[self window] makeFirstResponder:mClearNumberVisitsButton];
  }
  else {
    [[self window] makeFirstResponder:mFolderNameField];
  }
}

-(void)windowDidResignKey:(NSNotification*) aNotification
{
  [[self window] makeFirstResponder:[self window]];
  if (![[self window] isVisible])
    [self setBookmark:nil];
}

- (void)windowWillClose:(NSNotification *)aNotification
{
  [self commitChanges:nil];
  [self setBookmark:nil];
}

- (void)commitChanges:(id)changedField
{
  NSTabViewItem *tabViewItem = nil;
  if ([mDummyView contentView] == mBookmarkView)
    tabViewItem = [mTabView selectedTabViewItem];
  BOOL isBookmark;
  if ((isBookmark = [mBookmarkItem isKindOfClass:[Bookmark class]])) {
    if ([(Bookmark *)mBookmarkItem isSeparator] || ![[mBookmarkItem parent] isKindOfClass:[BookmarkItem class]])
      return;
  }
  if (!changedField) {
    if ((tabViewItem == mBookmarkInfoTabView) && isBookmark) {
      [mBookmarkItem setTitle:[mBookmarkNameField stringValue]];
      [mBookmarkItem setDescription:[mBookmarkDescField stringValue]];
      [mBookmarkItem setKeyword:[mBookmarkKeywordField stringValue]];
      [(Bookmark *)mBookmarkItem setUrl:[mBookmarkLocationField stringValue]];
    }
    else if ([mDummyView contentView] == mFolderView && !isBookmark) {
      [mBookmarkItem setTitle:[mFolderNameField stringValue]];
      [mBookmarkItem setDescription:[mFolderDescField stringValue]];
      if ([(BookmarkFolder *)mBookmarkItem isGroup])
        [mBookmarkItem setKeyword:[mFolderKeywordField stringValue]];
    }
  }
  else if ((changedField == mBookmarkNameField) || (changedField == mFolderNameField))
    [mBookmarkItem setTitle:[changedField stringValue]];
  else if ((changedField == mBookmarkKeywordField) || (changedField == mFolderKeywordField))
    [mBookmarkItem setKeyword:[changedField stringValue]];
  else if ((changedField == mBookmarkDescField) || (changedField == mFolderDescField))
    [mBookmarkItem setDescription:[changedField stringValue]];
  else if ((changedField == mBookmarkLocationField) && isBookmark)
    [(Bookmark *)mBookmarkItem setUrl:[changedField stringValue]];
    
  [[mFieldEditor undoManager] removeAllActions];
}

// there's a bug on first load. I don't know why.  But this fixes it, so I'll leave it in.
-(IBAction)showWindow:(id)sender
{
  [self updateUI:mBookmarkItem];
  [super showWindow:sender];
}

- (IBAction)tabGroupCheckboxClicked:(id)sender
{
  if ([mBookmarkItem isKindOfClass:[BookmarkFolder class]])
    [(BookmarkFolder *)mBookmarkItem setIsGroup:[sender state] == NSOnState];
}

- (IBAction)dockMenuCheckboxClicked:(id)sender
{
  if ([mBookmarkItem isKindOfClass:[BookmarkFolder class]]) {
    [(BookmarkFolder *)mBookmarkItem setIsDockMenu:([sender state] == NSOnState)];
    [mDockMenuCheckbox setEnabled:NO];
  }
}

- (IBAction)clearVisitCount:(id)sender
{
  if ([mBookmarkItem isKindOfClass:[Bookmark class]])
    [(Bookmark *)mBookmarkItem setNumberOfVisits:0];
  [mNumberVisitsField setIntValue:0];
}

-(void)setBookmark: (BookmarkItem*) aBookmark
{
  // register for changes on the new bookmark
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self name:BookmarkItemChangedNotification object:nil];
  // to avoid a hard-to-find bug, we do UI stuff before setting
  if (aBookmark) {
    [self updateUI:aBookmark];
    [aBookmark retain];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:aBookmark];
  }
  [mBookmarkItem release];
  mBookmarkItem = aBookmark;
}

-(void)updateUI:(BookmarkItem *)aBookmark
{
  if (aBookmark) {
    //
    // setup for bookmarks
    //
    NSBox *newView;
    if ([aBookmark isKindOfClass:[Bookmark class]]) {
      newView = mBookmarkView;
      [mBookmarkNameField setStringValue: [aBookmark title]];
      [mBookmarkDescField setStringValue: [aBookmark description]];
      [mBookmarkKeywordField setStringValue: [aBookmark keyword]];
      [mBookmarkLocationField setStringValue: [(Bookmark *)aBookmark url]];
      [mNumberVisitsField setIntValue:[(Bookmark *)aBookmark numberOfVisits]];
      [mLastVisitField setStringValue: [[(Bookmark *)aBookmark lastVisit] descriptionWithCalendarFormat:[[mLastVisitField formatter] dateFormat] timeZone:[NSTimeZone localTimeZone] locale:nil]];
      NSString *statusString = nil;
      unsigned status = [(Bookmark *)aBookmark status];
      switch (status) {
        case (kBookmarkOKStatus):
        case (kBookmarkSpacerStatus):
          statusString = NSLocalizedString(@"OK",@"OK");
          break;
        case (kBookmarkBrokenLinkStatus):
          statusString = NSLocalizedString(@"Link Broken",@"Link Broken");
          break;
        case (kBookmarkMovedLinkStatus):
          statusString = NSLocalizedString(@"Link has Moved",@"Link has Moved");
          break;
        case (kBookmarkServerErrorStatus):
          statusString = NSLocalizedString(@"Server Unreachable",@"Server Unreachable");
          break;
        case (kBookmarkNeverCheckStatus):
          statusString = NSLocalizedString(@"Uncheckable",@"Uncheckable");
          break;
        default:
          statusString = [NSString string];
      }
      [mStatusField setStringValue:statusString];
      // if its parent is a smart folder or it's a menu separator,
      // we turn off all the fields.  if it isn't, then we turn them all on
      id parent = [aBookmark parent];
      if (([parent isKindOfClass:[BookmarkItem class]]) &&
          (![parent isSmartFolder]) &&
          (![(Bookmark *)aBookmark isSeparator]))
      {
        [mBookmarkNameField setEditable:YES];
        [mBookmarkDescField setEditable:YES];
        [mBookmarkKeywordField setEditable:YES];
        [mBookmarkLocationField setEditable:YES];
      } else
      {
        [mBookmarkNameField setEditable:NO];
        [mBookmarkDescField setEditable:NO];
        [mBookmarkKeywordField setEditable:NO];
        [mBookmarkLocationField setEditable:NO];
      }
    }
    //
    // Folders
    //
    else if ([aBookmark isKindOfClass:[BookmarkFolder class]]) {
      newView = mFolderView;
      if ([(BookmarkFolder *)aBookmark isGroup]) {
        [mTabgroupCheckbox setState:NSOnState];
      }
      else {
        [mTabgroupCheckbox setState:NSOffState];
      }
      [mFolderNameField setStringValue: [aBookmark title]];
      [mFolderKeywordField setStringValue: [aBookmark keyword]];
      [mFolderDescField setStringValue: [aBookmark description]];
      //
      // we can't just unselect dock menu - we have to pick a new one
      //
      if ([(BookmarkFolder *)aBookmark isDockMenu]) {
        [mDockMenuCheckbox setState:NSOnState];
        [mDockMenuCheckbox setEnabled:NO];
      } else {
        [mDockMenuCheckbox setState:NSOffState];
        [mDockMenuCheckbox setEnabled:YES];
      }
    }
    // Swap view if necessary
    if ([mDummyView contentView] != newView)
      [mDummyView setContentView:newView];
    // Header
    NSMutableString *truncatedTitle = [NSMutableString stringWithString:[aBookmark title]];
    [truncatedTitle truncateTo:kMaxLengthOfWindowTitle at:kTruncateAtEnd];
    NSString* infoForString = [NSString stringWithFormat:NSLocalizedString(@"BookmarkInfoTitle", @"Info for "), truncatedTitle];
    [[self window] setTitle: infoForString];
  }
}

-(BookmarkItem *)bookmark
{
  return mBookmarkItem;
}


-(NSText *)windowWillReturnFieldEditor:(NSWindow *)aPanel toObject:(id)aObject
{
  return mFieldEditor;
}

#pragma mark -

- (void)bookmarkAdded:(NSNotification *)aNote
{
}

- (void)bookmarkRemoved:(NSNotification *)aNote
{
  NSDictionary *dict = [aNote userInfo];
  BookmarkItem *item = [dict objectForKey:BookmarkFolderChildKey];
  if ((item == [self bookmark]) && ![item parent]) {
    [self setBookmark:nil];
    [[self window] close];
  }
}

// We're only registering for our current bookmark,
// so no need to make checks to see if it's the right one.
- (void)bookmarkChanged:(NSNotification *)aNote
{
  BookmarkItem *item = [aNote object];
  if ([item isKindOfClass:[Bookmark class]]) {
    [mNumberVisitsField setIntValue:[(Bookmark *)item numberOfVisits]];
    [mLastVisitField setStringValue: [[(Bookmark *)item lastVisit] descriptionWithCalendarFormat:[[mLastVisitField formatter] dateFormat] timeZone:[NSTimeZone localTimeZone] locale:nil]];
  }
}

@end
