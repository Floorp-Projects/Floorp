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

#import "BookmarksController.h"
#import "ImageAndTextCell.h"
#import "BookmarksDataSource.h"
#import "HistoryDataSource.h"


@interface BookmarksController(PRIVATE)
- (void) setCanEditSelectedContainerContents:(BOOL)inCanEdit;
@end


@implementation BookmarksController

//
// - windowDidLoad
//
// Perform some extra initialization when the window finishes loading
// that we can't do in IB.
//
- (void)windowDidLoad
{
  // hide the search panel
  
  // the standard table item doesn't handle text and icons. Replace it
  // with a custom cell that does.
  ImageAndTextCell* imageAndTextCell = [[[ImageAndTextCell alloc] init] autorelease];
  [imageAndTextCell setEditable: YES];
  [imageAndTextCell setWraps: NO];
  NSTableColumn* itemNameColumn = [mItemPane tableColumnWithIdentifier: @"Name"];
  [itemNameColumn setDataCell:imageAndTextCell];
  NSTableColumn* containerNameColumn = [mContainerPane tableColumnWithIdentifier: @"Name"];
  [containerNameColumn setDataCell:imageAndTextCell];

  // set up the font on the item view to be smaller
  NSArray* columns = [mItemPane tableColumns];
  if ( columns ) {
    int numColumns = [columns count];
    NSFont* smallerFont = [NSFont systemFontOfSize:11];
    for ( int i = 0; i < numColumns; ++i )
      [[[columns objectAtIndex:i] dataCell] setFont:smallerFont];
  }
}

//
// - splitViewDidResizeSubviews:
//
// Called when one of the views got resized. We want to ensure that the "add bookmark
// item" button gets lined up with the left edge of the item panel. If the container/item
// split was the one that changed, move it accordingly
//
- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
  const int kButtonGutter = 8;
  
  if ( [notification object] == mContainersSplit ) {
    // get the position of the item view relative to the window and set the button
    // to that X value. Yes, this will fall down if the bookmark view is inset from the window
    // but i think we can safely assume it won't be.
    NSRect windowRect = [mItemPane convertRect:[mItemPane bounds] toView:nil];
    NSRect newButtonLocation = [mAddItemButton frame];
    newButtonLocation.origin.x = windowRect.origin.x;
    [mAddItemButton setFrame:newButtonLocation];
    [mAddItemButton setNeedsDisplay:YES];
    
    // offset by the width of the button and the gutter and we've got the location
    // of the add folder button next to it.
    newButtonLocation.origin.x += newButtonLocation.size.width + kButtonGutter;
    [mAddFolderButton setFrame:newButtonLocation];
    [mAddFolderButton setNeedsDisplay:YES];
  }
}

//
// - splitView:canCollapseSubview:
//
// Called when appkit wants to ask if it can collapse a subview. The only subview
// of our splits that we allow to be hidden is the search panel.
//
- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview
{
  BOOL retVal = NO;
  // subview will be a NSScrollView, so we have to get the superview of the
  // search pane for comparison.
  if ( sender == mItemSearchSplit && subview == [mSearchPane superview] )
    retVal = YES;
  return retVal;
}


- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedCoord ofSubviewAt:(int)offset
{
  const int kMinimumContainerSplitWidth = 150;
  float retVal = proposedCoord;
  if ( sender == mContainersSplit )
    retVal = kMinimumContainerSplitWidth;
  return retVal;
}

- (void) focus
{
  [[mItemPane window] makeFirstResponder:mItemPane];
}


- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
  int numRows = 0;
  if ( tableView == mContainerPane ) {
    // hack for now, history and bookmarks
    numRows = 2;
  }
  else if ( tableView == mSearchPane ) {
    // hack just to display something.
    numRows = 5;
  }
  return numRows;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
  id itemString = nil;
  if ( tableView == mContainerPane ) {
    if ( row == kBookmarksMenuContainer )
      itemString = NSLocalizedString(@"Bookmarks Menu", @"Bookmarks Menu");
    else
      itemString = NSLocalizedString(@"History", @"History");
  }
  else if ( tableView == mSearchPane ) {
    if ( [[tableColumn identifier] isEqualToString:@"Name"] )
      itemString = @"<Search result here>";
    else
      itemString = @"<URL result here>";
  }
  return itemString;
}


- (void)tableView:(NSTableView *)inTableView willDisplayCell:(id)inCell forTableColumn:(NSTableColumn *)inTableColumn row:(int)inRowIndex
{
  if ( inTableView == mContainerPane ) {
    if ( inRowIndex == kBookmarksMenuContainer )
      [inCell setImage:[NSImage imageNamed:@"bookicon"]];
    else if ( inRowIndex == kHistoryContainer )
      [inCell setImage:[NSImage imageNamed:@"historyicon"]];
  }
}

- (IBAction) changeContainer:(id)aSender
{
  [self selectContainer:[aSender clickedRow]];
}

- (void) setCanEditSelectedContainerContents:(BOOL)inCanEdit
{
    [mItemPane setAllowsEditing:inCanEdit];
    
    // update buttons
    [mAddItemButton setEnabled:inCanEdit];
    [mAddFolderButton setEnabled:inCanEdit];
    [mInfoButton setEnabled:inCanEdit];
}

- (void) selectContainer:(int)inRowIndex
{
  [mContainerPane selectRow:inRowIndex byExtendingSelection:NO];
  if ( inRowIndex == kBookmarksMenuContainer ) {
    [mItemPane setDataSource:mBookmarksSource];
    [mItemPane setDelegate:mBookmarksSource];
    [mBookmarksSource ensureBookmarks];
    [mBookmarksSource restoreFolderExpandedStates];
    [self setCanEditSelectedContainerContents:YES];
    
    [mItemPane setDoubleAction: @selector(openBookmark:)];
    [mItemPane setDeleteAction: @selector(deleteBookmarks:)];
  }
  else if ( inRowIndex == kHistoryContainer ) {
    [mItemPane setDataSource:mHistorySource];
    [mItemPane setDelegate:mHistorySource];
    [mHistorySource ensureDataSourceLoaded];
    [mItemPane reloadData];
    [self setCanEditSelectedContainerContents:NO];

    [mItemPane setDoubleAction: @selector(openHistoryItem:)];
    [mItemPane setDeleteAction: @selector(deleteHistoryItems:)];
  }
}

- (void) selectLastContainer
{
  // we need to call selectContainer: in order to get all the appropriate
  // stuff hooked up.
  [self selectContainer:[mContainerPane selectedRow]];
}

@end

