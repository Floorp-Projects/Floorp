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


const float STFSymbolXOffset = 4.0;
const float STFSymbolYOffset = 4.0;

@implementation SearchTextFieldCell

- (id)initWithCoder:(NSCoder *)coder
{
  if ((self = [super initWithCoder:coder]))
  {
    [coder decodeValueOfObjCType:@encode(BOOL) at:&mHasPopUpButton];
    mPopUpButtonCell = [[coder decodeObject] retain];
    mLeftImage    = [[coder decodeObject] retain];
    mMiddleImage  = [[coder decodeObject] retain];
    mRightImage   = [[coder decodeObject] retain];
    mPopUpImage   = [[coder decodeObject] retain];
    mNoPopUpImage = [[coder decodeObject] retain];
    mCancelImage  = [[coder decodeObject] retain];
    [coder decodeValueOfObjCType:@encode(BOOL) at:&mShouldShowCancelButton];
    [coder decodeValueOfObjCType:@encode(BOOL) at:&mShouldShowSelectedPopUpItem];
    [self setStringValue:[coder decodeObject]];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
  [super encodeWithCoder:coder];

  [coder encodeValueOfObjCType:@encode(BOOL) at:&mHasPopUpButton];
  [coder encodeObject:mPopUpButtonCell];
  [coder encodeObject:mLeftImage];
  [coder encodeObject:mMiddleImage];
  [coder encodeObject:mRightImage];
  [coder encodeObject:mPopUpImage];
  [coder encodeObject:mNoPopUpImage];
  [coder encodeObject:mCancelImage];
  [coder encodeValueOfObjCType:@encode(BOOL) at:&mShouldShowCancelButton];
  [coder encodeValueOfObjCType:@encode(BOOL) at:&mShouldShowSelectedPopUpItem];
  [coder encodeObject:[self stringValue]];
}

- (id)initTextCell:(NSString *)aString
{
  if ((self = [super initTextCell:aString]))
  {
    mLeftImage    = [[NSImage imageNamed:@"SearchLeft"] retain];
    mMiddleImage  = [[NSImage imageNamed:@"SearchMiddle"] retain];
    mRightImage   = [[NSImage imageNamed:@"SearchRight"] retain];
    mPopUpImage   = [[NSImage imageNamed:@"SearchPopUp"] retain];
    mNoPopUpImage = [[NSImage imageNamed:@"SearchNoPopUp"] retain];
    mCancelImage  = [[NSImage imageNamed:@"SearchCancel"] retain];
  
    mHasPopUpButton = YES;
    mShouldShowCancelButton = NO;
    mShouldShowSelectedPopUpItem = YES;

    mPopUpButtonCell = [[STFPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES];
    [mPopUpButtonCell addItemWithTitle:@""];

    [self setEditable:YES];
    [self setScrollable:YES];
  }
  
  return self;
}


- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [mPopUpButtonCell release];

  [mLeftImage release];
  [mRightImage release];
  [mMiddleImage release];
  [mPopUpImage release];
  [mNoPopUpImage release];
  [mCancelImage release];

  [super dealloc];
}

// override to clear out the cancel button if given an empty value
- (void)setStringValue:(NSString *)aString
{
  if ([aString length] == 0)
    mShouldShowCancelButton = NO;
  [super setStringValue: aString];
}


- (BOOL)hasPopUpButton
{
  return mHasPopUpButton;
}


- (void)setHasPopUpButton:(BOOL)aBoolean
{
  mHasPopUpButton = aBoolean;
}


- (STFPopUpButtonCell *)popUpButtonCell
{
  return mPopUpButtonCell;
}


- (void)showSelectedPopUpItem:(BOOL)shouldShow
{
  NSString *newTitle = [mPopUpButtonCell titleOfSelectedItem];

  if (shouldShow && mShouldShowSelectedPopUpItem) {
    if (mHasPopUpButton && newTitle && [[self stringValue] isEqualToString:@""]) {
      [self setTextColor:[NSColor disabledControlTextColor]];
      [self setStringValue:newTitle];
    }
  } else
    [self setTextColor:[NSColor controlTextColor]];

  mShouldShowSelectedPopUpItem = shouldShow;
}


- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  BOOL wasShowingFirstResponder = [self showsFirstResponder];

  // Draw the left end of the widget
  NSPoint leftImageOrigin = cellFrame.origin;
  leftImageOrigin.y += [mLeftImage size].height;
  [mLeftImage compositeToPoint:leftImageOrigin operation:NSCompositeSourceOver];

  // Draw the right end of the widget
  NSPoint rightImageOrigin = cellFrame.origin;
  rightImageOrigin.x += cellFrame.size.width - [mRightImage size].width;
  rightImageOrigin.y += [mRightImage size].height;
  [mRightImage compositeToPoint:rightImageOrigin operation:NSCompositeSourceOver];

  // Draw the middle section
  NSRect middleImageRect = cellFrame;
  NSSize middleImageSize = [mMiddleImage size];
  
  middleImageRect.origin.x += [mLeftImage size].width;
  middleImageRect.size.width  = cellFrame.size.width - ([mLeftImage size].width + [mRightImage size].width);
  middleImageRect.size.height = middleImageSize.height;
  [mMiddleImage setFlipped:YES];

  NSRect srcRect = NSMakeRect(0, 0, middleImageSize.width, middleImageSize.height);
  [mMiddleImage drawInRect:middleImageRect fromRect:srcRect operation:NSCompositeSourceOver fraction:1.0f];

  // If we have a popUp button, draw it on the widget
  NSPoint popUpImageOrigin = cellFrame.origin;
  popUpImageOrigin.x += STFSymbolXOffset;
  popUpImageOrigin.y += [mPopUpImage size].height + STFSymbolXOffset;
   
  if (mHasPopUpButton) 
    [mPopUpImage compositeToPoint:popUpImageOrigin operation:NSCompositeSourceOver];
  else
    [mNoPopUpImage compositeToPoint:popUpImageOrigin operation:NSCompositeSourceOver];

  // If we should show the cancel button, draw the button in the proper rect
  if (mShouldShowCancelButton)
  {
    NSPoint cancelImageOrigin = cellFrame.origin;
    cancelImageOrigin.x += cellFrame.size.width - [mCancelImage size].width -
      STFSymbolXOffset;
    cancelImageOrigin.y += [mCancelImage size].height + STFSymbolYOffset;
    [mCancelImage compositeToPoint:cancelImageOrigin operation:NSCompositeSourceOver];
  }

  [self showSelectedPopUpItem:([self controlView] && ![[self controlView] isFirstResponder])];

  // Invalidate the focus ring to draw/undraw it depending on if we're in the
  // key window. This will cause us to redraw the entire text field cell every time
  // the insertion point flashes, but only then (it used to redraw any time anything in 
  // the chrome changed).
  if (wasShowingFirstResponder) {
    [controlView setKeyboardFocusRingNeedsDisplayInRect:cellFrame];
    if ([[controlView window] isKeyWindow]) {
      [NSGraphicsContext saveGraphicsState];
      
      NSSetFocusRingStyle(NSFocusRingOnly);
      [[self fieldOutlinePathForRect:cellFrame] fill];
      
      [NSGraphicsContext restoreGraphicsState];
    }
  }
  
  // Draw the text field
  [self setShowsFirstResponder:NO];
  [super drawWithFrame:[self textFieldRectFromRect:cellFrame] inView:controlView];
  [self setShowsFirstResponder:wasShowingFirstResponder];
}


- (void)editWithFrame:(NSRect)aRect inView:(NSView *)controlView editor:(NSText *)textObj
             delegate:(id)anObject event:(NSEvent *)theEvent
{
  // Make sure that the frame is the text field rect and pass it up to the superclass
  [super editWithFrame:[self textFieldRectFromRect:aRect]
                inView:controlView
                editor:textObj
              delegate:anObject
                 event:theEvent];
}


- (void)selectWithFrame:(NSRect)aRect inView:(NSView *)controlView editor:(NSText *)textObj
               delegate:(id)anObject start:(int)selStart length:(int)selLength
{
  // Make sure that the frame is the text field rect and pass it up to the superclass
  [super selectWithFrame:[self textFieldRectFromRect:aRect]
                  inView:controlView
                  editor:textObj
                delegate:anObject
                   start:selStart
                  length:selLength];
}


- (void)cancelButtonClickedWithFrame:(NSRect)aFrame inView:(NSView *)aView
{
  [self setStringValue:@""];            // clears the cancel button
  mShouldShowSelectedPopUpItem = YES;
}


- (void)popUpButtonClickedWithFrame:(NSRect)aFrame inView:(NSView *)aView
{
  aFrame.origin.y += 4.0;

  mShouldShowSelectedPopUpItem = NO;
  [mPopUpButtonCell performClickWithFrame:aFrame inView:aView];
}


- (void)searchSubmittedFromView:(NSView *)controlView
{
  mShouldShowCancelButton = [[self stringValue] isEqualToString:@""] ? NO : YES;

  [controlView setNeedsDisplay:YES];
}


// Returns a path around the widget
- (NSBezierPath *)fieldOutlinePathForRect:(NSRect)aRect
{
  NSBezierPath *fieldOutlinePath = [NSBezierPath bezierPath];
  NSPoint aPoint;

  // Start at the top left of the middle section
  aPoint.x = aRect.origin.x + [mLeftImage size].width;
  aPoint.y = NSMaxY(aRect) - 2.0;
  [fieldOutlinePath moveToPoint:aPoint];

  // Trace to the top right of the middle section
  aPoint.x = NSMaxX(aRect) - [mRightImage size].width;
  [fieldOutlinePath lineToPoint:aPoint];

  // Trace around the right curve to the bottom right of the middle section
  aPoint.y = aRect.origin.y;
  [fieldOutlinePath curveToPoint:aPoint
                   controlPoint1:NSMakePoint(aPoint.x + 11.0, NSMaxY(aRect))
                   controlPoint2:NSMakePoint(aPoint.x + 11.0, aPoint.y)];

  // Trace to the bottom left of the middle section
  aPoint.x = aRect.origin.x + [mLeftImage size].width;
  [fieldOutlinePath lineToPoint:aPoint];

  // Trace around the left curve to the top left of the middle section, i.e. the beginning
  aPoint.y = NSMaxY(aRect) - 2.0;
  [fieldOutlinePath curveToPoint:aPoint
                   controlPoint1:NSMakePoint(aPoint.x - 11.0, aRect.origin.y)
                   controlPoint2:NSMakePoint(aPoint.x - 11.0, aPoint.y)];

  [fieldOutlinePath closePath];

  return fieldOutlinePath;
}


// Returns a rect for where the cancel button should be
- (NSRect)cancelButtonRectFromRect:(NSRect)aRect
{
  // If we have a cancel button ready, return the rect for
  // where it should be. Else return an empty rect.
  if (mShouldShowCancelButton) {
    NSRect cancelButtonRect, remainderRect;
    float width = [mCancelImage size].width + STFSymbolXOffset*2;

    NSDivideRect(aRect, &cancelButtonRect, &remainderRect, width, NSMaxXEdge);

    return cancelButtonRect;
  } else
    return NSZeroRect;
}

// Returns a rect for where the popUp button should be
- (NSRect)popUpButtonRectFromRect:(NSRect)aRect
{
  // If we are supposed to show the popUp button, return the
  // rect for where it should be. Else return an empty rect.
  if (mHasPopUpButton) {
    NSRect popUpButtonRect, remainderRect;
    float width = 2 * STFSymbolXOffset + [mPopUpImage size].width;

    NSDivideRect(aRect, &popUpButtonRect, &remainderRect, width, NSMinXEdge);

    return popUpButtonRect;
  } else
    return NSZeroRect;
}

// Returns a rect for where the text field should be
- (NSRect)textFieldRectFromRect:(NSRect)aRect
{
  NSRect remainderRect, tempRect;

  // If we have a popup, make a little room for its icon, else just make enough
  // room for the left end cap
  float startWidth = [mPopUpImage size].width + STFSymbolXOffset;

  // Make room for the cancel image and give it just a little extra padding (1.0)
  float endWidth = [mCancelImage size].width + STFSymbolXOffset + 1.0;
  

  // Cut off the cancel button
  NSDivideRect(aRect, &tempRect, &remainderRect, endWidth, NSMaxXEdge);

  // Cut off the popUp button
  NSDivideRect(remainderRect, &tempRect, &remainderRect, startWidth, NSMinXEdge);

  // Cut a little off the bottom so that the text is centered well
  NSDivideRect(remainderRect, &tempRect, &remainderRect, (float)3, NSMinYEdge);

  return remainderRect;
}

- (void)setControlSize:(NSControlSize)inSize
{
  NSControlSize oldSize = [self controlSize];
  [super setControlSize:inSize];
  [mPopUpButtonCell setControlSize:inSize];
  
  if (oldSize != inSize)
  {
    [mLeftImage autorelease];
    [mMiddleImage autorelease];
    [mRightImage autorelease];
    [mPopUpImage autorelease];
    [mNoPopUpImage autorelease];
    [mCancelImage autorelease];

    if (inSize == NSSmallControlSize)
    {
      [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

      mLeftImage    = [[NSImage imageNamed:@"SearchLeft_small"] retain];
      mMiddleImage  = [[NSImage imageNamed:@"SearchMiddle_small"] retain];
      mRightImage   = [[NSImage imageNamed:@"SearchRight_small"] retain];
      mPopUpImage   = [[NSImage imageNamed:@"SearchPopUp_small"] retain];
      mNoPopUpImage = [[NSImage imageNamed:@"SearchNoPopUp_small"] retain];
      mCancelImage  = [[NSImage imageNamed:@"SearchCancel_small"] retain];
    }
    else
    {
      [self setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];

      mLeftImage    = [[NSImage imageNamed:@"SearchLeft"] retain];
      mMiddleImage  = [[NSImage imageNamed:@"SearchMiddle"] retain];
      mRightImage   = [[NSImage imageNamed:@"SearchRight"] retain];
      mPopUpImage   = [[NSImage imageNamed:@"SearchPopUp"] retain];
      mNoPopUpImage = [[NSImage imageNamed:@"SearchNoPopUp"] retain];
      mCancelImage  = [[NSImage imageNamed:@"SearchCancel"] retain];
    }
  }
}

@end
