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
*   David Haas <haasd@cae.wisc.edu>
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

#import "ExtendedTableView.h"


@implementation ExtendedTableView

-(void)setDeleteAction: (SEL)aDeleteAction
{
  mDeleteAction = aDeleteAction;
}

-(SEL)deleteAction
{
  return mDeleteAction;
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
  }
  [super keyDown: aEvent];
}

-(NSMenu *)menuForEvent:(NSEvent *)theEvent
{
  int rowIndex;
  NSPoint point;
  point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  rowIndex = [self rowAtPoint:point];
  if (rowIndex >= 0) {
    [self abortEditing];
    id delegate = [self delegate];
    if (![self isRowSelected:rowIndex]) {
      if ([delegate respondsToSelector:@selector(tableView:shouldSelectRow:)]) {
        if (![delegate tableView:self shouldSelectRow:rowIndex])
          return [self menu];
      }
    }
    if ([delegate respondsToSelector:@selector(tableView:contextMenuForRow:)])
      return [delegate tableView:self contextMenuForRow:rowIndex];
  }
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

@end
