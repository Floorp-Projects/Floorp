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

#import <Cocoa/Cocoa.h>

@class BookmarksDataSource;
@class HistoryDataSource;
@class ExtendedOutlineView;

enum { kBookmarksMenuContainer, kHistoryContainer };

//
// BookmarksController
//
// A controller that handles everything that goes on w/in the embedded
// bookmarks manager.
//
@interface BookmarksController : NSObject
{
  IBOutlet NSButton* mAddItemButton;
  IBOutlet NSButton* mAddFolderButton;
  IBOutlet NSButton* mInfoButton;
  
  IBOutlet NSSplitView* mContainersSplit;       // vertical split
  IBOutlet NSSplitView* mItemSearchSplit;       // horizontal split

  IBOutlet NSTableView* mContainerPane;
  IBOutlet ExtendedOutlineView* mItemPane;
  IBOutlet NSTableView* mSearchPane;            // shows search results, can be hidden
  
  // data sources we can swap between depending on what the user clicks on
  IBOutlet BookmarksDataSource* mBookmarksSource;
  IBOutlet HistoryDataSource* mHistorySource;
}

// Set focus to something in the bookmark manager view
- (void) focus;

- (void) windowDidLoad;

// called when someone clicks a row on the container table. Changes the
// data in the item outline.
- (IBAction) changeContainer:(id)aSender;

// select either a particular container or the container that was previously
// selected the last time the manager was displayed.
- (void) selectContainer:(int)inRowIndex;
- (void) selectLastContainer;

// NSSplitView delegate methods
//- (void)splitView:(NSSplitView *)sender resizeSubviewsWithOldSize:(NSSize)oldSize;
- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedCoord ofSubviewAt:(int)offset;
//- (float)splitView:(NSSplitView *)sender constrainMaxCoordinate:(float)proposedCoord ofSubviewAt:(int)offset;
//- (void)splitViewWillResizeSubviews:(NSNotification *)notification;
- (void)splitViewDidResizeSubviews:(NSNotification *)notification;
- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview;
//- (float)splitView:(NSSplitView *)splitView constrainSplitPosition:(float)proposedPosition ofSubviewAt:(int)index;

// NSTableView delegate methods
- (void)tableView:(NSTableView *)aTableView willDisplayCell:(id)aCell forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;

// NSTableView data source for the container pane and search pane.
- (int)numberOfRowsInTableView:(NSTableView *)tableView;
- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row;
// optional
//- (void)tableView:(NSTableView *)tableView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn row:(int)row;
// optional - drag and drop support
//- (BOOL)tableView:(NSTableView *)tv writeRows:(NSArray*)rows toPasteboard:(NSPasteboard*)pboard;
//- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op;
//- (BOOL)tableView:(NSTableView*)tv acceptDrop:(id <NSDraggingInfo>)info row:(int)row dropOperation:(NSTableViewDropOperation)op;

@end
