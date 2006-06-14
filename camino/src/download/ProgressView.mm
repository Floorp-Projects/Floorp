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
 * Josh Aas.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josha@mac.com>
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


#import "ProgressView.h"


NSString* const kDownloadInstanceSelectedNotificationName = @"DownloadInstanceSelected";
NSString* const kDownloadInstanceOpenedNotificationName   = @"DownloadInstanceOpened";
NSString* const kDownloadInstanceCancelledNotificationName = @"DownloadInstanceCancelled";

@interface ProgressView(Private)

-(BOOL)isSelected;
-(void)setSelected:(BOOL)inSelected;

@end


@implementation ProgressView

- (id)initWithFrame:(NSRect)frame
{
  self = [super initWithFrame:frame];
  if (self) {
    mLastModifier = kNoKey;
    mProgressController = nil;
  }
  return self;
}

-(void)drawRect:(NSRect)rect
{
  if ([self isSelected]) {
    [[NSColor selectedTextBackgroundColor] set];
  }
  else {
    [[NSColor whiteColor] set];
  }
  [NSBezierPath fillRect:[self bounds]];
}

-(void)mouseDown:(NSEvent*)theEvent
{
  unsigned int mods = [theEvent modifierFlags];
  mLastModifier = kNoKey;
  BOOL shouldSelect = YES;
  // set mLastModifier to any relevant modifier key
  if (!((mods & NSShiftKeyMask) && (mods & NSCommandKeyMask))) {
    if (mods & NSShiftKeyMask) {
      mLastModifier = kShiftKey;
    }
    else if (mods & NSCommandKeyMask) {
      if ([self isSelected])
        shouldSelect = NO;
      mLastModifier = kCommandKey;
    }
  }
  [self setSelected:shouldSelect];
  [[NSNotificationCenter defaultCenter] postNotificationName:kDownloadInstanceSelectedNotificationName object:self];

  // after we've processed selection and modifiers, see if it's a double-click. If so, 
  // send a notification off to the controller which will handle it accordingly. Doing it after
  // processing the modifiers allows someone to shift-dblClick and open all selected items
  // in the list in one action.
  if ([theEvent type] == NSLeftMouseDown && [theEvent clickCount] == 2)
    [[NSNotificationCenter defaultCenter] postNotificationName:kDownloadInstanceOpenedNotificationName object:self];
}

-(int)lastModifier
{
  return mLastModifier;
}

- (BOOL)isSelected
{
  // make sure the controller is not nil before checking if it is selected
  if (!mProgressController)
    return NO;
  
  return [mProgressController isSelected];
}

-(void)setSelected:(BOOL)inSelected
{
  // make sure the controller is not nil before setting its selected state
  if (!mProgressController)
    return;
  
  [mProgressController setSelected:inSelected];
}

-(void)setController:(ProgressViewController*)controller
{
  // Don't retain since this view will only exist if its controller does
  mProgressController = controller;
}

-(ProgressViewController*)getController
{
  return mProgressController;
}

-(NSMenu*)menuForEvent:(NSEvent*)theEvent
{  
  // if the item is unselected, select it and deselect everything else before displaying the contextual menu
  if (![self isSelected]) {
    mLastModifier = kNoKey; // control is only special because it means its contextual menu time
    [self setSelected:YES];
    [self display]; // change selection immediately
    [[NSNotificationCenter defaultCenter] postNotificationName:kDownloadInstanceSelectedNotificationName object:self];
  }
  return [[self getController] contextualMenu];
}

-(BOOL)performKeyEquivalent:(NSEvent*)theEvent
{
  // Catch a command-period key down event and send the cancel request
  if (([theEvent type] == NSKeyDown && 
      ([theEvent modifierFlags] & NSCommandKeyMask) != 0) && 
      [[theEvent characters] isEqualToString:@"."]) 
  {
    [[NSNotificationCenter defaultCenter] postNotificationName:kDownloadInstanceCancelledNotificationName object:self];
    return YES;
  }
  
  return [super performKeyEquivalent:theEvent];
}

-(NSView*)hitTest:(NSPoint)aPoint
{
  if (NSMouseInRect(aPoint, [self frame], YES)) {
    return self;
  }
  else {
    return nil;
  }
}

@end
