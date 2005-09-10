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
 *   David Hyatt <hyatt@mozilla.org> (Original Author)
 *   Max Horn <max@quendi.de> (Context menu & tooltip code)
 *   Simon Fraser <smfr@smfr.org>
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

#import <AppKit/AppKit.h>

@interface ExtendedOutlineView : NSOutlineView
{
  SEL           mDeleteAction;

  NSRect        mOldFrameRect;
  int           mOldRows;
  BOOL          mDelegateTooltipStringForItem;

  int           mRowToBeEdited, mColumnToBeEdited;
  BOOL          mAllowsEditing;
  
  // sorting support
  NSImage*      mAscendingSortingImage;
  NSImage*      mDescendingSortingImage;
  
  NSString*     mSortColumnIdentifier;
  BOOL          mDescendingSort;
  
  BOOL          mAutosaveSort;
}

-(void)setAllowsEditing:(BOOL)inAllow;

-(void)keyDown:(NSEvent*)aEvent;

-(void)setDeleteAction: (SEL)deleteAction;
-(SEL)deleteAction;

- (NSArray*)selectedItems;

- (void)expandAllItems;

-(void)_editItem:(id)item;
-(void)_cancelEditItem;

// note that setting these just affect the outline state, they don't alter the data source
- (NSString*)sortColumnIdentifier;
- (void)setSortColumnIdentifier:(NSString*)inColumnIdentifier;

- (BOOL)sortDescending;
- (void)setSortDescending:(BOOL)inDescending;

- (void)setAutosaveTableSort:(BOOL)autosave;
- (BOOL)autosaveTableSort;

// Clipboard functions
-(BOOL) validateMenuItem:(id)aMenuItem;
-(IBAction) copy:(id)aSender;
-(IBAction) delete:(id)aSender;
-(IBAction) paste:(id)aSender;
-(IBAction) cut:(id)aSender;

@end

@interface NSObject (CHOutlineViewDataSourceToolTips)
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)item;
@end

@interface NSObject (CHOutlneViewDataSourceInlineEditing)
- (BOOL)outlineView:(NSOutlineView*)inOutlineView columnHasIcon:(NSTableColumn*)inColumn;
@end

@interface NSObject (CHOutlineViewContextMenus)
- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItems:(NSArray*)items;
@end

@interface NSObject (CHOutlineViewDragSource)
- (unsigned int)outlineView:(NSOutlineView *)outlineView draggingSourceOperationMaskForLocal:(BOOL)localFlag;
@end

