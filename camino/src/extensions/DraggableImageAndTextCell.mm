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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
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

#import "NSString+Utils.h"

#import "DraggableImageAndTextCell.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_3
@class NSShadow;
NSString* const NSShadowAttributeName = @"NSShadow";
#endif

@interface DraggableImageAndTextCell(Private)
- (NSAttributedString*)savedStandardTitle;
- (void)setSavedStandardTitle:(NSAttributedString*)title;
@end

@implementation DraggableImageAndTextCell

+ (BOOL)prefersTrackingUntilMouseUp
{
  return YES;
}

- (id)initTextCell:(NSString*)aString
{
  if ((self = [super initTextCell:aString])) {
    mIsDraggable = NO;
    mClickHoldTimeoutSeconds = 60.0 * 60.0 * 24.0;
    mLastClickHoldTimedOut = NO;
  }
  return self;
}

- (void)dealloc
{
  [mSavedStandardTitle release];
  [super dealloc];
}

- (NSAttributedString*)savedStandardTitle
{
  return mSavedStandardTitle;
}

- (void)setSavedStandardTitle:(NSAttributedString*)title
{
  [mSavedStandardTitle autorelease];
  mSavedStandardTitle = [title retain];
}

// Overridden to give the title text a drop shadow when the button is highlighted
- (void)highlight:(BOOL)highlighted withFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
  if ([[self title] length] > 0) {
    if (highlighted) {
      [self setSavedStandardTitle:[self attributedTitle]];
      
      NSMutableDictionary* info = [[[self savedStandardTitle] attributesAtIndex:0 effectiveRange:NULL] mutableCopy];
      NSShadow* shadow = [[NSShadow alloc] init];
      [shadow setShadowBlurRadius:1];
      [shadow setShadowOffset:NSMakeSize(0, -1)];
      [shadow setShadowColor:[NSColor colorWithCalibratedRed:0.0 green:0.0 blue:0.0 alpha:0.6]];
      [info setObject:shadow forKey:NSShadowAttributeName];
      [shadow release];
      NSAttributedString* shadowedTitle = [[NSAttributedString alloc] initWithString:[self title] attributes:info];
      [info release];
      [self setAttributedTitle:shadowedTitle];
      [shadowedTitle release];
    }
    else {
      [self setAttributedTitle:[self savedStandardTitle]];
      [self setSavedStandardTitle:nil];
    }
  }
  
  [super highlight:highlighted withFrame:cellFrame inView:controlView];
}

- (void)setClickHoldTimeout:(float)timeoutSeconds
{
  mClickHoldTimeoutSeconds = timeoutSeconds;
}

- (BOOL)lastClickHoldTimedOut
{
  return mLastClickHoldTimedOut;
}

- (void)setClickHoldAction:(SEL)inAltAction
{
  mClickHoldAction = inAltAction;
}

#pragma mark -

- (BOOL)isDraggable
{
  return mIsDraggable;
}

- (void)setDraggable:(BOOL)inDraggable
{
  mIsDraggable = inDraggable;
}

- (BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView*)controlView
{
  if (!mIsDraggable)
    return [super startTrackingAt:startPoint inView:controlView];

  mTrackingStart = startPoint;
  return YES; //[super startTrackingAt:startPoint inView:controlView];
}

- (BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView*)controlView
{
  if (!mIsDraggable)
    return [super continueTracking:lastPoint at:currentPoint inView:controlView];

  return [DraggableImageAndTextCell prefersTrackingUntilMouseUp];  // XXX fix me?
}

// called when the mouse leaves the cell, or the mouse button was released
- (void)stopTracking:(NSPoint)lastPoint at:(NSPoint)stopPoint inView:(NSView*)controlView mouseIsUp:(BOOL)flag
{
  [super stopTracking:lastPoint at:stopPoint inView:controlView mouseIsUp:flag];
}

#define kDragThreshold 4.0

- (BOOL)trackMouse:(NSEvent*)theEvent inRect:(NSRect)cellFrame ofView:(NSView*)controlView untilMouseUp:(BOOL)untilMouseUp
{
  mLastClickHoldTimedOut = NO;

  if (!mIsDraggable)
    return [super trackMouse:theEvent inRect:cellFrame ofView:controlView untilMouseUp:untilMouseUp];

  NSPoint firstWindowLocation = [theEvent locationInWindow];
  NSPoint lastWindowLocation  = firstWindowLocation;
  NSPoint curWindowLocation   = firstWindowLocation;
  NSEventType lastEvent       = (NSEventType)0;
  NSDate* clickHoldBailTime   = [NSDate dateWithTimeIntervalSinceNow:mClickHoldTimeoutSeconds];

  if (![self startTrackingAt:curWindowLocation inView:controlView])
    return NO;

  while(1) {
    NSEvent* event = [NSApp nextEventMatchingMask:(NSLeftMouseDraggedMask | NSLeftMouseUpMask)
                                        untilDate:clickHoldBailTime
                                           inMode:NSEventTrackingRunLoopMode
                                          dequeue:YES];
    if (!event) {
      mLastClickHoldTimedOut = YES;
      break;
    }

    curWindowLocation = [event locationInWindow];
    lastEvent = [event type];

    if (![self continueTracking:lastWindowLocation at:curWindowLocation inView:controlView])
      return NO;

    // Tracking process
    if (([event type] == NSLeftMouseDragged) &&
        (fabs(firstWindowLocation.x - curWindowLocation.x) > kDragThreshold ||
         fabs(firstWindowLocation.y - curWindowLocation.y) > kDragThreshold))
    {
      break;
    }

    if ([event type] == NSLeftMouseUp)
      break;

    lastWindowLocation = curWindowLocation;
  }  

  if (lastEvent == NSLeftMouseUp)
    [(NSControl*)controlView sendAction:[self action] to:[self target]];
  else if (mLastClickHoldTimedOut && mClickHoldAction) {
    [self stopTracking:lastWindowLocation at:curWindowLocation inView:controlView mouseIsUp:NO];
    [(NSControl*)controlView sendAction:mClickHoldAction to:[self target]];
    return YES;
  }

  [self stopTracking:lastWindowLocation at:curWindowLocation inView:controlView mouseIsUp:(lastEvent == NSLeftMouseUp)];
  return YES;  // XXX fix me
}

@end

