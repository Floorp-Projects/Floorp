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
 *   Max Horn <max@quendi.de> (Context menu, tooltip code, and editing)
 *   Josh Aas <josha@mac.com> (contextual menu fixups)
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

#import "ExtendedOutlineView.h"


static NSString* const kAutosaveSortColumnIdentifierKey = @"sortcolumn_id";
static NSString* const kAutosaveSortDirectionKey        = @"sort_descending";

@interface ExtendedOutlineView (Private)

- (void)_updateToolTipRect;

- (void)updateTableHeaderToMatchCurrentSort;
- (void)loadAutosaveSort;
- (void)saveAutosaveSort;
- (NSString*)autosaveDefaultsKey;

@end

@implementation ExtendedOutlineView

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) )
  {
    mDeleteAction = nil;
    mAllowsEditing = YES;
    mRowToBeEdited = mColumnToBeEdited = 0;
    
    // FIXME - this method is *never* called for items that are archived in a nib!
    // Luckily, object memory is zeroed, so mDeleteAction will be 0 anyway.
    // I recommend that this method just be removed.
  }
  return self;
}

- (void)awakeFromNib
{
  // Setup the initial NSToolTipRects
  [self _updateToolTipRect];
}

- (void)dealloc
{
  [self saveAutosaveSort];    // is this too late?

  [mAscendingSortingImage release];
  [mDescendingSortingImage release];

  [mSortColumnIdentifier release];

  [super dealloc];
}

-(void)setDeleteAction: (SEL)aDeleteAction
{
  mDeleteAction = aDeleteAction;
}

-(SEL)deleteAction
{
  return mDeleteAction;
}

-(void)setDelegate:(id)anObject
{
  [super setDelegate:anObject];
  mDelegateTooltipStringForItem = [anObject respondsToSelector:@selector(outlineView:tooltipStringForItem:)];
}

- (NSArray*)selectedItems
{
  NSMutableArray* itemsArray = [NSMutableArray arrayWithCapacity:[self numberOfSelectedRows]];
  
  NSEnumerator* rowEnum = [self selectedRowEnumerator];
  NSNumber* currentRow = nil;
  while ((currentRow = [rowEnum nextObject]))
  {
    id item = [self itemAtRow:[currentRow intValue]];
    [itemsArray addObject:item];
  }
  
  return itemsArray;
}

- (void)expandAllItems
{
  unsigned int row = 0;
  while (row < [self numberOfRows])
  {
    id curItem = [self itemAtRow:row];
    if ([self isExpandable:curItem])
      [self expandItem:curItem expandChildren:NO];

    ++row;
  }
}

-(void)keyDown:(NSEvent*)aEvent
{
  const unichar kForwardDeleteChar = 0xf728;   // couldn't find this in any cocoa header
  
  // check each char in the event array. it should be just 1 char, but
  // just in case we use a loop.
  int len = [[aEvent characters] length];
  for ( int i = 0; i < len; ++i ) {
    unichar c = [[aEvent characters] characterAtIndex:i];
    
    // Check for a certain set of special keys.    
    if (c == NSDeleteCharacter || c == NSBackspaceCharacter || c == kForwardDeleteChar) {
      // delete the bookmark
      if (mDeleteAction)
        [NSApp sendAction: mDeleteAction to: [self target] from: self];
      return;
    }
    else if (c == NSCarriageReturnCharacter) {
      // Start editing
      if ([self numberOfSelectedRows] == 1) {
        [self editColumn:0 row:[self selectedRow] withEvent:aEvent select:YES];
        return;
      }
    }
    else if (c == NSEnterCharacter
              || ([aEvent modifierFlags] & NSCommandKeyMask && c == NSDownArrowFunctionKey)) {
      // on enter or cmd-downArrow, open the item as a double-click
      [NSApp sendAction:[self doubleAction] to:[self target] from:self];
      return;
    }
    else if (c == NSLeftArrowFunctionKey || c == NSRightArrowFunctionKey)
    {
      BOOL expand = (c == NSRightArrowFunctionKey);
      if ([self numberOfSelectedRows] == 1) {
        int index = [self selectedRow];
        if (index == -1)
          return;
  
        id item = [self itemAtRow: index];
        if (!item)
          return;
  
        if (![self isExpandable: item])
          return;
        
        if (![self isItemExpanded: item] && expand)
          [self expandItem: item];
        else if ([self isItemExpanded: item] && !expand)
          [self collapseItem: item];
      }
    }
  } // foreach character

  [super keyDown: aEvent];
}

/*
 * Intercept changes to the window frame so we can update our tooltip rects
 */
- (void)setFrameOrigin:(NSPoint)newOrigin
{
  [super setFrameOrigin:newOrigin];
  [self _updateToolTipRect];
}

- (void)setFrameSize:(NSSize)newSize
{
  [super setFrameSize:newSize];
  [self _updateToolTipRect];
}

- (void)setFrame:(NSRect)frameRect
{
  [super setFrame:frameRect];
  [self _updateToolTipRect];
}

/*
 * Implement the informal NSToolTipOwner protocol to allow tooltips
 * on a per-item level.
 */
- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
  NSString *result = nil;
  int rowIndex = [self rowAtPoint:point];
  if (rowIndex >= 0) {
    id delegate = [self delegate];
    id item = [self itemAtRow:rowIndex];
    
    if (item && mDelegateTooltipStringForItem)
      result = [delegate outlineView:self tooltipStringForItem:item];
  }
  return result;
}

/*
 * Return a context menu depending on which item was clicked.
 */
- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  int rowIndex = [self rowAtPoint:point];
  
  if (rowIndex >= 0)
  {
    // There seems to be a bug in AppKit; selectRow is supposed to
    // abort editing, but it doesn't, thus we do it manually.
    [self abortEditing];
    
    id item = [self itemAtRow:rowIndex];
    if (!item) return nil;    // someone might want a menu on the blank area
    
    id delegate = [self delegate];

    // if we click on a selected item, don't deselect other stuff.
    // otherwise, select just the current item.
    
    // Make sure the item is the only selected one
    if (![self isRowSelected:rowIndex])
    {
      BOOL shouldSelect = ![delegate respondsToSelector:@selector(outlineView:shouldSelectItem:)] ||
                          [delegate outlineView:self shouldSelectItem:item];
      if (!shouldSelect)
        return nil;   // can't select it, so bail

      [self selectRow:rowIndex byExtendingSelection:NO];
    }
        
    if ([delegate respondsToSelector:@selector(outlineView:contextMenuForItems:)])
      return [delegate outlineView:self contextMenuForItems:[self selectedItems]];
  }
  else
  {
    [self deselectAll:self];
  }
  
  // Just return the default context menu
  return [self menu];
}

//
// -textDidEndEditing:
//
// Called when the object we're editing is done. The default behavior is to
// select another editable item, but that's not the behavior we want. We just
// want to keep the selection on what was being editing.
//
- (void)textDidEndEditing:(NSNotification *)aNotification
{
  // Fake our own notification. We pretend that the editing was canceled due to a
  // mouse click. This prevents outlineviw from selecting another cell for editing.
  NSDictionary *userInfo = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:NSIllegalTextMovement] forKey:@"NSTextMovement"];
  NSNotification *fakeNotification = [NSNotification notificationWithName:[aNotification name] object:[aNotification object] userInfo:userInfo];
  
  [super textDidEndEditing:fakeNotification];
  
  // Make ourself first responder again
  [[self window] makeFirstResponder:self];
}


- (void)_cancelEditItem
{
  mRowToBeEdited = -1;
  [[self class] cancelPreviousPerformRequestsWithTarget:self selector:@selector(editItem:) object:nil];
}


//
// Start editing a given item. Used to start editing delayed.
//
- (void)_editItem:(id)dummy
{
  int row = mRowToBeEdited;

  // Cancel any other scheduled edits+  [self _cancelEditItem];
  
  // Only start the editing if the selection didn't change in the meantime
  // (e.g. because arrow keys were used to change it).
  if (row >= 0 && row == [self selectedRow])
    [self editColumn:mColumnToBeEdited row:row withEvent:nil select:YES];    
}

/*
 * Handle mouse clicks, allowing them to start inline editing.
 * We start editing under two conditions:
 * 1) For clicks together with the alt/option modifier key immediatly
 * 2) For clicks on already selected items after a short delay
 * Rule 2 only is enabled if the outline view is first responder of the main
 * window, otherwise odd behaviour takes placed when the user clicks into 
 * a background window on a selected item - editing would start, which is
 * not the correct result (at least if we want to match Finder). 
 *
 * There are some other catchfalls and quirks we have to consider, for details
 * read the comments in the code.
 */
- (void)mouseDown:(NSEvent *)theEvent
{
  // the data isn't always allowed to be edited (eg, rendevous or history). Bail
  // if we've been told to not allow editing.
  if ( !mAllowsEditing ) {
    [super mouseDown:theEvent];
    return;
  }

  // Record some state information before calling the super implementation,
  // as these might change. E.g. if we are not yet first repsonder, the click
  // might make us first responder.
  BOOL wasFirstResponder = ([[self window] firstResponder] == self);
  BOOL wasMainWindow = [[self window] isMainWindow];
  BOOL wasClickInTextPartOfCell = NO;
  int oldEditRow = [self editedRow];
  int oldRow = ([self numberOfSelectedRows] == 1) ? [self selectedRow] : -1;

  // Now call the super implementation. It will only return after the mouseUp
  // occured, since it does drag&drop handling etc.
  [super mouseDown:theEvent];

  // If this was a double click, we cancel any scheduled edit requests and return
  if ([theEvent clickCount] > 1) {
    [self _cancelEditItem];
    return;
  }
  
  // Detect if the selection changed (ignoring multi-selections). We can't rely on
  // selectedRow because if the user clicks and drags the selection doesn't change. We
  // instead just find the clicked row ourselves from the mouse location.
  int newRow = ([self numberOfSelectedRows] == 1) ? [self rowAtPoint:[self convertPoint:[theEvent locationInWindow] fromView:nil]] : -1;

  // If the selection did change, we need to cancel any scheduled edit requests.
  if (oldRow != newRow && oldRow != -1)
    [self _cancelEditItem];

  // Little trick: if editing was already in progress, then the field editor
  // will be first responder. For our purposes this is the same as if we 
  // were first responder, so pretend it were so.
  if (oldEditRow > -1)
    wasFirstResponder = YES;

  // If we already were first responder of the main window, and the click was
  // inside a row and it was the left mouse button, then we investigate further
  // and check if we need to start editing.
  if (wasFirstResponder && wasMainWindow && newRow >= 0 && ([theEvent type] == NSLeftMouseDown)) {

    // Check whether the click was inside the text part of a cell. For now, we do
    // this a bit hackishly and assume the cell image has width 20, and there is a
    // gap of 3 pixels between image and label.
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    int clickedRow = [self rowAtPoint:point];
    mColumnToBeEdited = [self columnAtPoint:point];
    id dataSource = [self dataSource];
    BOOL offsetForIcon = NO;
    if ([dataSource respondsToSelector:@selector(outlineView:columnHasIcon:)]) {
      NSTableColumn* tableCol = [[self tableColumns] objectAtIndex:mColumnToBeEdited];
      offsetForIcon = [dataSource outlineView:self columnHasIcon:tableCol];
    }
    if (clickedRow >= 0 && mColumnToBeEdited >= 0)
    {
      NSRect rect = [self frameOfCellAtColumn:mColumnToBeEdited row:clickedRow];
      if (offsetForIcon) {
        rect.size.width = 20.0 + 3.0;
        wasClickInTextPartOfCell = ! NSPointInRect(point, rect);
      }
      else
        wasClickInTextPartOfCell = NSPointInRect(point, rect);
    }
    
    // Do not start editing for clicks on the icon part (to match Finder's behaviour).
    if (wasClickInTextPartOfCell) {
  
      if ([theEvent modifierFlags] & NSAlternateKeyMask)
      {
        // If the alt key was pressed, start editing right away
        mRowToBeEdited = newRow;
        [self _editItem:nil];
      }
      else if (oldRow == newRow) {
        // If the click was into an already selected row, start editing
        // after a short (1 second) delay - unless it gets canceled before
        // of course, e.g. by a click someplace else.
        mRowToBeEdited = newRow;
        [self performSelector:@selector(_editItem:) withObject:nil afterDelay:1.0];
      }
    }
  }
}

- (void)setAllowsEditing:(BOOL)inAllow
{
  mAllowsEditing = inAllow;
}

#pragma mark -

- (void)updateTableHeaderToMatchCurrentSort
{
  if (!mAscendingSortingImage)
  {
    // magic image names
    mAscendingSortingImage  = [[NSImage imageNamed:@"NSAscendingSortIndicator"] retain];
    mDescendingSortingImage = [[NSImage imageNamed:@"NSDescendingSortIndicator"] retain];
  }

  NSArray*        columns     = [self tableColumns];
  NSTableColumn*  sortColumn  = [self tableColumnWithIdentifier:mSortColumnIdentifier];
  unsigned int    i, numCols  = [columns count];

  for (i = 0; i < numCols; i ++)
    [self setIndicatorImage:nil inTableColumn:[columns objectAtIndex:i]];

  if (sortColumn)
  {
    [self setIndicatorImage:(mDescendingSort ? mDescendingSortingImage : mAscendingSortingImage) inTableColumn:sortColumn];
    [self setHighlightedTableColumn:sortColumn];
  }
  else
  {
    [self setHighlightedTableColumn:nil];
  }
}

- (NSString*)sortColumnIdentifier
{
  return mSortColumnIdentifier;
}

- (void)setSortColumnIdentifier:(NSString*)inColumnIdentifier
{
  [mSortColumnIdentifier autorelease];
  mSortColumnIdentifier = [inColumnIdentifier retain];
  
  // forward to the data source
  // this assumes that this data source is only used for this outliner,
  // and not others (breaks clean model/view separation)
  if ([[self dataSource] respondsToSelector:@selector(setSortColumnIdentifier:)])
    [[self dataSource] setSortColumnIdentifier:mSortColumnIdentifier];

  [self updateTableHeaderToMatchCurrentSort];
}

- (BOOL)sortDescending
{
  return mDescendingSort;
}

- (void)setSortDescending:(BOOL)inDescending
{
  mDescendingSort = inDescending;

  // forward to the data source.
  // this assumes that this data source is only used for this outliner,
  // and not others (breaks clean model/view separation)
  if ([[self dataSource] respondsToSelector:@selector(setSortDescending:)])
    [[self dataSource] setSortDescending:mDescendingSort];

  [self updateTableHeaderToMatchCurrentSort];
}

- (void)setAutosaveTableSort:(BOOL)autosave
{
  if (autosave != mAutosaveSort)
  {
    mAutosaveSort = autosave;
    if (mAutosaveSort)
      [self loadAutosaveSort];
  }
}

- (BOOL)autosaveTableSort
{
  return mAutosaveSort;
}

- (void)loadAutosaveSort
{
  NSString* defaultsKey = [self autosaveDefaultsKey];
  if (!defaultsKey) return;
  NSDictionary* prefs = [[NSUserDefaults standardUserDefaults] dictionaryForKey:defaultsKey];
  if (prefs)
  {
    NSString* sortCol = [prefs objectForKey:kAutosaveSortColumnIdentifierKey];
    if (sortCol && [self columnWithIdentifier:sortCol] != -1)
      [self setSortColumnIdentifier:sortCol];
  
    BOOL descending = [[prefs objectForKey:kAutosaveSortDirectionKey] boolValue];
    [self setSortDescending:descending];
  }
}

- (void)saveAutosaveSort
{
  NSString* defaultsKey = [self autosaveDefaultsKey];
  if (!defaultsKey) return;
  if (![self sortColumnIdentifier]) return;

  NSDictionary* prefs = [NSDictionary dictionaryWithObjectsAndKeys:
                         [self sortColumnIdentifier], kAutosaveSortColumnIdentifierKey,
     [NSNumber numberWithBool:[self sortDescending]], kAutosaveSortDirectionKey,
                                                      nil];

  [[NSUserDefaults standardUserDefaults] setObject:prefs forKey:defaultsKey];
}

- (NSString*)autosaveDefaultsKey
{
  NSString* asName = [self autosaveName];
  if (!asName) return nil;
  
  return [@"ExtendedOutlineView Sort " stringByAppendingString:asName];
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
  if (!newWindow)
  {
    // view is leaving window: save sort
    [self saveAutosaveSort];
  }
  [super viewWillMoveToWindow:newWindow];
}

// on jaguar, delegates of NSOutlineView don't receive outlineView:didClickTableColumn messages,
// so we work around this by overriding an internal NSTableView method
- (void)_sendDelegateDidClickColumn:(int)column
{
  if ([self delegate] != nil && [[self delegate] respondsToSelector:@selector(outlineView:didClickTableColumn:)])
  {
    [[self delegate] outlineView:self didClickTableColumn:[[self tableColumns] objectAtIndex:column]];
  }
  else
  {  
    [super _sendDelegateDidClickColumn:column];
  }
}

/*
 * Set up tooltip rects for every row, but only if the frame size or
 * the number of rows changed since the last invocation.
 */
- (void)_updateToolTipRect
{
  static NSRect oldFrameRect;
  static int oldRows = 0;
  NSRect frameRect;
  int rows;
  
  // Only set tooltip rects if the delegate implements outlineView:tooltipStringForItem:
  if (!mDelegateTooltipStringForItem)
    return;

  frameRect = [self frame];
  rows = [self numberOfRows];
  
  // Check if rows or frame changed
  if (rows != oldRows || !NSEqualRects(oldFrameRect, frameRect))
  {
    int i;
    NSRect rect;
    
    // Remove all old NSToolTipRects
    [self removeAllToolTips];

    // Add a NSToolTipRect for each row
    for (i = 0; i < rows; ++i)
    {
      rect = [self rectOfRow:i];
      [self addToolTipRect:rect owner:self userData:NULL];
    }
  }
  
  // Save the current values
  oldRows = rows;
  oldFrameRect = frameRect;
}

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)localFlag
{
  if ([[self delegate] respondsToSelector:@selector(outlineView:draggingSourceOperationMaskForLocal:)])
    return [[self delegate] outlineView:self draggingSourceOperationMaskForLocal:localFlag];

  return NSDragOperationGeneric;
}

#pragma mark -

// Support clipboard actions if our delegate implements them
-(BOOL) validateMenuItem:(id)aMenuItem
{
  SEL action = [aMenuItem action];

  // XXX we should probably try to call validateMenuItem: on the delegate too
  if (action == @selector(delete:))
    return [[self delegate] respondsToSelector:@selector(delete:)];
  else if (action == @selector(copy:))
    return [[self delegate] respondsToSelector:@selector(copy:)];
  else if (action == @selector(paste:))
    return [[self delegate] respondsToSelector:@selector(paste:)];
  else if (action == @selector(cut:))
    return [[self delegate] respondsToSelector:@selector(cut:)];

  return YES;
}

-(IBAction) copy:(id)aSender
{
  if ([[self delegate] respondsToSelector:@selector(copy:)])
    [[self delegate] copy:aSender];
}

-(IBAction) paste:(id)aSender
{
  if ([[self delegate] respondsToSelector:@selector(paste:)])
    [[self delegate] paste:aSender];
}

-(IBAction) delete:(id)aSender
{
  if ([[self delegate] respondsToSelector:@selector(delete:)])
    [[self delegate] delete:aSender];
}

-(IBAction) cut:(id)aSender
{
  if ([[self delegate] respondsToSelector:@selector(cut:)])
    [[self delegate] cut:aSender];
}

@end
