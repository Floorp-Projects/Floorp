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
*/

#import "CHExtendedOutlineView.h"

@implementation CHExtendedOutlineView

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) ) {
    mDeleteAction = 0;
  }
  return self;
}

-(void)setDeleteAction: (SEL)aDeleteAction
{
  mDeleteAction = aDeleteAction;
}

-(SEL)deleteAction
{
  return mDeleteAction;
}

int kReturnKeyCode = 0x24;
int kDeleteKeyCode = 0x33;
int kLeftArrowKeyCode = 0x7B;
int kRightArrowKeyCode = 0x7C;

-(void)keyDown:(NSEvent*)aEvent
{
  // Check for a certain set of special keys.
  
  //NSDeleteFunctionKey
  if ([aEvent keyCode] == kDeleteKeyCode) {
    if (mDeleteAction)
      [NSApp sendAction: mDeleteAction to: [self target] from: self];
    return;
  }
  else if ([aEvent keyCode] == kReturnKeyCode) {
    // Override return to keep the goofy inline editing
    // from happening.
    if ([self numberOfSelectedRows] == 1)
      [NSApp sendAction: [self doubleAction] to: [self target] from: self];
    return;
  }
  else if ([aEvent keyCode] == kLeftArrowKeyCode ||
           [aEvent keyCode] == kRightArrowKeyCode)
  {
    BOOL expand = ([aEvent keyCode] == kRightArrowKeyCode);
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
  
  return [super keyDown: aEvent];
}

@end
