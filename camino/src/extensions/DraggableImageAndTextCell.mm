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


@interface DraggableImageAndTextCell(Private)

- (void)clearCachedTruncatedLabel;

@end

@implementation DraggableImageAndTextCell

+ (BOOL)prefersTrackingUntilMouseUp
{
  return YES;
}

- (id)initTextCell:(NSString*)aString
{
  if ((self = [super initTextCell:aString]))
  {
    mLabelStringWidth = -1;
    mImagePadding = 0;
    mImageSpace = 2;
    mImageAlpha = 1.0;
    mIsDraggable = NO;
    mClickHoldTimeoutSeconds = 60.0 * 60.0 * 24.0;
    mLastClickHoldTimedOut = NO;
  }
  return self;
}

- (void)dealloc
{
  [mImage release];
  [mTruncLabelString release];
  [super dealloc];
}

- copyWithZone:(NSZone *)zone
{
    DraggableImageAndTextCell *cell = (DraggableImageAndTextCell *)[super copyWithZone:zone];
    cell->mImage = [mImage retain];
    cell->mTruncLabelString = nil;
    cell->mLabelStringWidth = -1;
    return cell;
}

/*
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
  NSRect textRect = cellFrame;
  NSRect imageRect;

  // we always reserve space for the image, even if there isn't one
  // assume the image rect is always square
  float imageWidth = NSHeight(cellFrame) - 2 * mImagePadding;
  NSDivideRect(cellFrame, &imageRect, &textRect, imageWidth, NSMinXEdge);
  
  if (mImage)
  {
    NSRect imageSrcRect = NSZeroRect;
    imageSrcRect.size = [mImage size];    
    [mImage drawInRect:NSInsetRect(imageRect, mImagePadding, mImagePadding)
          fromRect:imageSrcRect operation:NSCompositeSourceOver fraction:mImageAlpha];
  }
  
  // remove image space
  NSDivideRect(textRect, &imageRect, &textRect, mImageSpace, NSMinXEdge);

  int          cellWidth       = (int)NSWidth(textRect);
  NSDictionary *cellAttributes = [[self attributedTitle] attributesAtIndex:0 effectiveRange:nil];

  if (mLabelStringWidth != cellWidth || !mTruncLabelString)
  {
    [mTruncLabelString release];
    mTruncLabelString = [[NSMutableString alloc] initWithString:[self title]];
    [mTruncLabelString truncateToWidth:cellWidth at:kTruncateAtEnd withAttributes:cellAttributes];
    mLabelStringWidth = cellWidth;
  }
  
  [mTruncLabelString drawInRect:textRect withAttributes:cellAttributes];
}
*/

- (void)setStringValue:(NSString *)aString
{
  if (![aString isEqualToString:[self stringValue]])
  	[self clearCachedTruncatedLabel];

  [super setStringValue:aString];
}

- (void)setAttributedStringValue:(NSAttributedString *)attribStr
{
  if (![attribStr isEqualToAttributedString:[self attributedStringValue]])
  	[self clearCachedTruncatedLabel];
  
  [super setAttributedStringValue:attribStr];
}

- (void)setTitle:(NSString *)aString
{
  if (![aString isEqualToString:[self stringValue]])
  	[self clearCachedTruncatedLabel];

  [super setTitle:aString];
}

/*
- (void)setImage:(NSImage *)anImage 
{
  if (anImage != mImage)
  {
    [mImage release];
    mImage = [anImage retain];
  }
}

- (NSImage *)image
{
  return mImage;
}
*/

- (void)setImagePadding:(float)padding
{
  mImagePadding = padding;
}

- (void)setImageSpace:(float)space
{
  mImageSpace = space;
}

- (void)setImageAlpha:(float)alpha
{
  mImageAlpha = alpha;
}

- (BOOL)labelTruncates
{
  return mTruncateLabel;
}

- (void)setLabelTruncates:(BOOL)inTruncates
{
  if (mTruncateLabel != inTruncates)
    [self clearCachedTruncatedLabel];

	mTruncateLabel = inTruncates;
}


- (void)clearCachedTruncatedLabel
{
  [mTruncLabelString release];
  mTruncLabelString = nil;
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

- (BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView *)controlView
{
  if (!mIsDraggable)
    return [super startTrackingAt:startPoint inView:controlView];

  mTrackingStart = startPoint;
  return YES; //[super startTrackingAt:startPoint inView:controlView];
}

- (BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView *)controlView
{
  if (!mIsDraggable)
    return [super continueTracking:lastPoint at:currentPoint inView:controlView];

  return [DraggableImageAndTextCell prefersTrackingUntilMouseUp];		// XXX fix me?
}

// called when the mouse leaves the cell, or the mouse button was released
- (void)stopTracking:(NSPoint)lastPoint at:(NSPoint)stopPoint inView:(NSView *)controlView mouseIsUp:(BOOL)flag
{
  [super stopTracking:lastPoint at:stopPoint inView:controlView mouseIsUp:flag];
}

#define kDragThreshold 4.0

- (BOOL)trackMouse:(NSEvent *)theEvent inRect:(NSRect)cellFrame ofView:(NSView *)controlView untilMouseUp:(BOOL)untilMouseUp
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
  
  while(1)
  {
    NSEvent* event = [NSApp nextEventMatchingMask:
            (NSLeftMouseDraggedMask | NSLeftMouseUpMask)
              untilDate:clickHoldBailTime
              inMode:NSEventTrackingRunLoopMode
              dequeue:YES];
    if (!event)
    {
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
  return YES;		// XXX fix me
}

@end

