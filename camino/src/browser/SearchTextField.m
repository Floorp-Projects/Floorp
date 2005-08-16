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
 * The Original Code is SearchTextField UI code.
 *
 * The Initial Developer of the Original Code is
 * Prachi Gauriar.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Prachi Gauriar (pgauria@uark.edu)
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

#import "STFPopUpButtonCell.h"
#import "SearchTextFieldCell.h"
#import "SearchTextField.h"

@interface NSString(IfOnlyThisWereObjCPPSoWeCouldIncludeOurAdditions)
- (NSString *)stringByRemovingCharactersInSet:(NSCharacterSet*)characterSet;
@end

@implementation SearchTextField

+ (Class)cellClass
{
  return [SearchTextFieldCell class];
}


- (void)awakeFromNib
{
  [self setCell:[[[SearchTextFieldCell alloc] initTextCell:@""] autorelease]];
  if (mPopupMenu)
    [self setPopupMenu:mPopupMenu];

  [self registerForDraggedTypes:[NSArray arrayWithObject:NSStringPboardType]];
}


- (void)mouseDown:(NSEvent *)theEvent
{
  NSPoint pointInField = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  NSRect cellFrame = [self convertRect:[self frame] fromView:[self superview]];
  
  if (NSMouseInRect(pointInField, [[self cell] cancelButtonRectFromRect:cellFrame], NO))
  {
    [[self cell] cancelButtonClickedWithFrame:cellFrame inView:self];
    // fake the notification as close as we can
    [self textDidChange: [NSNotification notificationWithName: NSTextDidChangeNotification 
                                                       object: [self currentEditor]]]; 
  }
  else if (NSMouseInRect(pointInField, [[self cell] popUpButtonRectFromRect:cellFrame], NO))
  {
    [[self cell] popUpButtonClickedWithFrame:cellFrame inView:self];
    // send the notification?
  }
}


// Do not remove.  This method must be empty to setup cursor rects properly
- (void)resetCursorRects
{
}


- (BOOL)isFirstResponder
{
  NSResponder *first = ([self window] != nil) ? [[self window] firstResponder] : nil;

  while (first != nil && [first isKindOfClass:[NSView class]]) {
    first = [(NSView*)first superview];

    if (first == self)
      return YES;
  }

  return NO;
}


- (BOOL)becomeFirstResponder
{
  if ([[self textColor] isEqualTo:[NSColor disabledControlTextColor]]) {
    [self setTextColor:[NSColor controlTextColor]];
    [self setStringValue:@""];
  }

  return [super becomeFirstResponder];
}


- (void)selectText:(id)sender
{
  if ([[self textColor] isEqualTo:[NSColor disabledControlTextColor]]) {
    [self setTextColor:[NSColor controlTextColor]];
    [self setStringValue:@""];
  }

  [super selectText:sender];  
}

- (void)setIsSmall:(BOOL)small
{
  // XXX we need to provide smaller icons to make the small variant look better
  if (small)
  {
    NSFont* smallFont = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
    [[self cell] setFont:smallFont];
    [[[self cell] popUpButtonCell] setFont:smallFont];
  }
  else
  {
    NSFont* normalFont = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    [[self cell] setFont:normalFont];
    [[[self cell] popUpButtonCell] setFont:normalFont];
  }
}

- (BOOL)hasPopUpButton
{
  return [[self cell] hasPopUpButton];
}

- (void)setHasPopUpButton:(BOOL)aBoolean
{
  [[self cell] setHasPopUpButton:aBoolean];
}

- (void)textDidChange:(NSNotification *)aNotification
{
  [[self cell] searchSubmittedFromView:self];
  [super textDidChange: aNotification];
}

- (NSString *)titleOfSelectedPopUpItem
{
  return [[[self cell] popUpButtonCell]  titleOfSelectedItem];
}

// This method completely overrides NSTextField's textDidEndEditing: method
// No call to the superclass is made.  Check NSTextField docs to see requirements
// of this method
// This code is a modified form of the method of same name at:
// http://s.sudre.free.fr/Software/Source/WBSearchTextField.m

- (void)textDidEndEditing:(NSNotification *)aNotification
{
  NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
  id cell = [self cell];
  BOOL showGreyText = YES;
  NSFormatter *formatter = [cell formatter];
  NSString* fieldValue = [NSString stringWithString:[[[self window] fieldEditor:YES forObject:self] string]];
  
  if (formatter == nil) 
    [cell setStringValue:fieldValue];
  else {
    // If we have a special formatter, transform our string based on that method
    id newObjectValue;
    NSString *error;
    SEL controlFailedMethod = @selector(control:didFailToFormatString:errorDescription:);

    if ([formatter getObjectValue:&newObjectValue forString:fieldValue errorDescription:&error])
      [cell setObjectValue:newObjectValue];
    else if ([[self delegate] respondsToSelector:controlFailedMethod] &&
             [[self delegate] control:self didFailToFormatString:fieldValue
                     errorDescription:error])
      [cell setStringValue:fieldValue];
  }

  [cell endEditing:[aNotification object]];

  // Get information from the notifcation dictionary so that we can decide
  // what to do in response to the user's action
  NSMutableDictionary* userInfo = [[NSMutableDictionary alloc] initWithDictionary:[aNotification userInfo]];
  [userInfo setObject:[aNotification object] forKey:@"NSFieldEditor"];
  [defaultCenter postNotificationName:NSControlTextDidEndEditingNotification
                               object:self
                             userInfo:userInfo];
  NSNumber* textMovement = [[aNotification userInfo] objectForKey:@"NSTextMovement"];
  [userInfo release];

  if (textMovement) {
    switch ([(NSNumber *)textMovement intValue]) {
      // If the user hit enter, send our action to our target
      case NSReturnTextMovement:
        [[self cell] searchSubmittedFromView:self];

        if ([self sendAction:[self action] to:[self target]] == NO) {
          NSEvent *event = [[self window] currentEvent];

          if ([self performKeyEquivalent:event] == NO &&
              [[self window] performKeyEquivalent:event] == NO)
            [self selectText:self];
        }
          break;

        // If the user hit tab, go to the next key view
      case NSTabTextMovement:
      {
        // we should be able to just select the next key view, but at some point we have
        // to break the cycle and kick the user off the toolbar. Do it here. Selecting
        // the window allows us to tab into the content area (assuming we're on a toolbar).
        BOOL isOnToolbar = NO;
        NSView* parent = [self superview];
        while (parent) {
          if ([parent isKindOfClass:[NSToolbar class]])
            isOnToolbar = YES;
          parent = [parent superview];
        }
        if (isOnToolbar) {
          // HACK: we're on a toolbar, make life not suck
          NSWindow* wind = [self window];
          [wind makeFirstResponder:wind];
        }
        else {
          // do the normal key loop thang
          [[self window] selectKeyViewFollowingView:self];
          if ([[self window] firstResponder] == [self window])
            [self selectText:self];
        }
      }
          break;

        // If the user hit shift-tab, go to the key view before us
      case NSBacktabTextMovement:
        [[self window] selectKeyViewPrecedingView:self];

        if ([[self window] firstResponder] == [self window])
          [self selectText:self];

          break;
      default:
        if ([self isFirstResponder])
          showGreyText = NO;
    }
  }

  [cell showSelectedPopUpItem:showGreyText];
}


- (void)addPopUpMenuItemsWithTitles:(NSArray *)itemTitles
{
  [[[self cell] popUpButtonCell] addItemsWithTitles:itemTitles];
}

- (void)setPopupMenu:(NSMenu*)menu
{
  [[[self cell] popUpButtonCell] setMenu:menu];
}

- (NSMenu*)popupMenu
{
  return [[[self cell] popUpButtonCell] menu];
}

- (void)selectPopupMenuItem:(NSMenuItem*)menuItem
{
  [[[self cell] popUpButtonCell] selectItem:menuItem];
}

- (NSMenuItem*)selectedPopupMenuItem
{
  return [[[self cell] popUpButtonCell] selectedItem];
}

#pragma mark -

//
// -draggingEntered:
//
// accept anything that's a string type.
//
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];
  NSPasteboard *pboard = [sender draggingPasteboard];
  if ([[pboard types] containsObject:NSStringPboardType]) {
    if (sourceDragMask & NSDragOperationCopy)
      return NSDragOperationCopy;
  }
  return NSDragOperationNone;
}

//
// -performDragOperation:
//
// For strings, strip off any bad chars (newlines, etc) and then execute the action immediately.
// No need to make the user go focus the text field and hit return.
//
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  NSPasteboard *pboard = [sender draggingPasteboard];
  NSString *dragString = nil;
  if ([[pboard types] containsObject:NSStringPboardType]) {
    dragString = [pboard stringForType:NSStringPboardType];
    // Clean the string on the off chance it has line breaks, etc.
    dragString = [dragString stringByRemovingCharactersInSet:[NSCharacterSet controlCharacterSet]];

    // execute the target/action. Set the control text color as that's the way that
    // the control knows that text was entered vs. the description string.
    [self setTextColor:[NSColor controlTextColor]];
    [[self cell] searchSubmittedFromView:self];
    [self setStringValue:dragString];
    [self sendAction:[self action] to:[self target]];
  }
  return YES;
}

@end
