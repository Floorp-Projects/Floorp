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
*   David Hyatt <hyatt@netscape.com> (Original Author)
*   Max Horn <max@quendi.de> (Context menu, tooltip code, and editing)
*   Josh Aas <josha@mac.com> (contextual menu fixups)
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

#import "ExtendedOutlineView.h"

@interface ExtendedOutlineView (Private)
- (void)_updateToolTipRect;
@end

@implementation ExtendedOutlineView

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) ) {
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
- (void)setFrameOrigin:(NSPoint)newOrigin;
{
  [super setFrameOrigin:newOrigin];
  [self _updateToolTipRect];
}

- (void)setFrameSize:(NSSize)newSize;
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
  id item;
  int rowIndex;
  NSPoint point;
  point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  rowIndex = [self rowAtPoint:point];
  
  if (rowIndex >= 0) {
    // There seems to be a bug in AppKit; selectRow is supposed to
    // abort editing, but it doesn't, thus we do it manually.
    [self abortEditing];
    
    item = [self itemAtRow:rowIndex];
    if (item) {
      id delegate = [self delegate];
      // Make sure the item is the only selected one
      if (![delegate respondsToSelector:@selector(outlineView:shouldSelectItem:)]
          || [delegate outlineView:self shouldSelectItem:item]) {
        [self deselectAll:self]; // we don't want anything but what was right-clicked selected
        [self selectRow:rowIndex byExtendingSelection:NO];
      }
      
      if ([delegate respondsToSelector:@selector(outlineView:contextMenuForItem:)])
        return [delegate outlineView:self contextMenuForItem:item];
    } else {
      // no item, no context menu
      return nil;
    }
  }
  else {
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
    // this a bit hackishly and assume the cell image is set and has width 20,
    // and there is a gap of 3 pixels between image and label.
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    int clickedRow = [self rowAtPoint:point];
    mColumnToBeEdited = [self columnAtPoint:point];
    if (clickedRow >= 0 && mColumnToBeEdited >= 0)
    {
      NSRect rect = [self frameOfCellAtColumn:mColumnToBeEdited row:clickedRow];
      rect.size.width = 20.0 + 3.0;
      wasClickInTextPartOfCell = ! NSPointInRect(point, rect);
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

-(void)setAllowsEditing:(BOOL)inAllow
{
  mAllowsEditing = inAllow;
}

@end


@implementation ExtendedOutlineView (Private)

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

@end
