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
*   David Hyatt <hyatt@netscape.com> (Original Author)
*   Max Horn <max@quendi.de> (Context menu & tooltip code)
*/

#import "CHExtendedOutlineView.h"

@interface CHExtendedOutlineView (Private)
- (void)_updateToolTipRect;
@end

@implementation CHExtendedOutlineView

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) ) {
    mDeleteAction = 0;
    // FIXME - this method is *never* called for items that are archived in a nib!
    // Luckily, object memory is zeroed, so mDeleteAction will be 0 anyway.
    // I recommend that this method just be removed.
  }
  return self;
}

- (void)awakeFromNib {
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
  // check each char in the event array. it should be just 1 char, but
  // just in case we use a loop.
  int len = [[aEvent characters] length];
  for ( int i = 0; i < len; ++i ) {
    unichar c = [[aEvent characters] characterAtIndex:i];
    
    // Check for a certain set of special keys.
    
    if (c == NSDeleteCharacter || c == NSBackspaceCharacter) {
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

  return [super keyDown: aEvent];
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
      
      // If the item was not selected, select it now
      if (![self isRowSelected:rowIndex]) {
        if (![delegate respondsToSelector:@selector(outlineView:shouldSelectItem:)]
            || [delegate outlineView:self shouldSelectItem:item])
          [self selectRow:rowIndex byExtendingSelection:NO];
      }
      
      if ([delegate respondsToSelector:@selector(outlineView:contextMenuForItem:)])
        return [delegate outlineView:self contextMenuForItem:item];
    }
  }
  
  // Just return the default context menu
  return [self menu];
}

@end


@implementation CHExtendedOutlineView (Private)

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
