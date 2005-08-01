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

@interface SearchTextFieldCell(PrivateMethods)

+ (NSImage *)cachedLeftImage;
+ (NSImage *)cachedMiddleImage;
+ (NSImage *)cachedRightImage;
+ (NSImage *)cachedPopUpImage;
+ (NSImage *)cachedNoPopUpImage;
+ (NSImage *)cachedCancelImage;

@end


@implementation SearchTextFieldCell

- (id)initWithCoder:(NSCoder *)coder
{
  [super initWithCoder:coder];

  [coder decodeValueOfObjCType:@encode(BOOL) at:&hasPopUpButton];
  popUpButtonCell = [[coder decodeObject] retain];
  _leftImage = [[coder decodeObject] retain];
  _middleImage = [[coder decodeObject] retain];
  _rightImage = [[coder decodeObject] retain];
  _popUpImage = [[coder decodeObject] retain];
  _noPopUpImage = [[coder decodeObject] retain];
  _cancelImage = [[coder decodeObject] retain];
  [coder decodeValueOfObjCType:@encode(BOOL) at:&_shouldShowCancelButton];
  [coder decodeValueOfObjCType:@encode(BOOL) at:&_shouldShowSelectedPopUpItem];
  [self setStringValue:[coder decodeObject]];

  return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
  [super encodeWithCoder:coder];

  [coder encodeValueOfObjCType:@encode(BOOL) at:&hasPopUpButton];
  [coder encodeObject:popUpButtonCell];
  [coder encodeObject:_leftImage];
  [coder encodeObject:_middleImage];
  [coder encodeObject:_rightImage];
  [coder encodeObject:_popUpImage];
  [coder encodeObject:_noPopUpImage];
  [coder encodeObject:_cancelImage];
  [coder encodeValueOfObjCType:@encode(BOOL) at:&_shouldShowCancelButton];
  [coder encodeValueOfObjCType:@encode(BOOL) at:&_shouldShowSelectedPopUpItem];
  [coder encodeObject:[self stringValue]];
}

- (id)initTextCell:(NSString *)aString
{
  [super initTextCell:aString];

  // why do expensive copies rather than just retaining?
  _leftImage    = [[SearchTextFieldCell cachedLeftImage] copy];
  _middleImage  = [[SearchTextFieldCell cachedMiddleImage] copy];
  _rightImage   = [[SearchTextFieldCell cachedRightImage] copy];
  _popUpImage   = [[SearchTextFieldCell cachedPopUpImage] copy];
  _noPopUpImage   = [[SearchTextFieldCell cachedNoPopUpImage] copy];
  _cancelImage  = [[SearchTextFieldCell cachedCancelImage] copy];

  hasPopUpButton = YES;
  _shouldShowCancelButton = NO;
  _shouldShowSelectedPopUpItem = YES;

  popUpButtonCell = [[STFPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES];
  [popUpButtonCell addItemWithTitle:@""];

  [self setEditable:YES];
  [self setScrollable:YES];

  return self;
}


- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [popUpButtonCell release];

  [_leftImage release];
  [_rightImage release];
  [_middleImage release];
  [_popUpImage release];
  [_noPopUpImage release];
  [_cancelImage release];

  [super dealloc];
}

// override to clear out the cancel button if given an empty value
- (void)setStringValue:(NSString *)aString
{
  if ([aString length] == 0)
    _shouldShowCancelButton = NO;
  [super setStringValue: aString];
}


- (BOOL)hasPopUpButton
{
  return hasPopUpButton;
}


- (void)setHasPopUpButton:(BOOL)aBoolean
{
  hasPopUpButton = aBoolean;
}


- (STFPopUpButtonCell *)popUpButtonCell
{
  return popUpButtonCell;
}


- (void)showSelectedPopUpItem:(BOOL)shouldShow
{
  NSString *newTitle = [[self  popUpButtonCell] titleOfSelectedItem];

  if (shouldShow && _shouldShowSelectedPopUpItem) {
    if (hasPopUpButton && newTitle && [[self stringValue] isEqualToString:@""]) {
      [self setTextColor:[NSColor disabledControlTextColor]];
      [self setStringValue:newTitle];
    }
  } else
    [self setTextColor:[NSColor controlTextColor]];

  _shouldShowSelectedPopUpItem = shouldShow;
}


- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  NSPoint _leftImageOrigin, _middleImageOrigin, _rightImageOrigin;
  NSPoint _popUpImageOrigin, _cancelImageOrigin;
  float _middleImageWidth;
  BOOL wasShowingFirstResponder = [self showsFirstResponder];

  // Draw the left end of the widget
  _leftImageOrigin = cellFrame.origin;
  _leftImageOrigin.y += [_leftImage size].height;
  [_leftImage compositeToPoint:_leftImageOrigin operation:NSCompositeSourceOver];

  // Draw the right end of the widget
  _rightImageOrigin = cellFrame.origin;
  _rightImageOrigin.x += cellFrame.size.width - [_rightImage size].width;
  _rightImageOrigin.y += [_rightImage size].height;
  [_rightImage compositeToPoint:_rightImageOrigin operation:NSCompositeSourceOver];

  // Draw the middle section
  _middleImageOrigin = cellFrame.origin;
  _middleImageOrigin.x += [_leftImage size].width;
  _middleImageOrigin.y += [_middleImage size].height;
  _middleImageWidth = cellFrame.size.width - ([_leftImage size].width + [_rightImage size].width);
  [_middleImage setSize:NSMakeSize(_middleImageWidth, [_middleImage size].height)];
  [_middleImage compositeToPoint:_middleImageOrigin operation:NSCompositeSourceOver];

  // If we have a popUp button, draw it on the widget
  _popUpImageOrigin = cellFrame.origin;
  _popUpImageOrigin.x += STFSymbolXOffset;
  _popUpImageOrigin.y += [_popUpImage size].height + STFSymbolXOffset;
   
  if (hasPopUpButton) 
    [_popUpImage compositeToPoint:_popUpImageOrigin operation:NSCompositeSourceOver];
  else
    [_noPopUpImage compositeToPoint:_popUpImageOrigin operation:NSCompositeSourceOver];

  // If we should show the cancel button, draw the button in the proper rect
  if (_shouldShowCancelButton) {
    _cancelImageOrigin = cellFrame.origin;
    _cancelImageOrigin.x += cellFrame.size.width - [_cancelImage size].width -
      STFSymbolXOffset;
    _cancelImageOrigin.y += [_cancelImage size].height + STFSymbolYOffset;
    [_cancelImage compositeToPoint:_cancelImageOrigin operation:NSCompositeSourceOver];
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
  _shouldShowSelectedPopUpItem = YES;
}


- (void)popUpButtonClickedWithFrame:(NSRect)aFrame inView:(NSView *)aView
{
  aFrame.origin.y += 4.0;

  _shouldShowSelectedPopUpItem = NO;
  [popUpButtonCell performClickWithFrame:aFrame inView:aView];
}


- (void)searchSubmittedFromView:(NSView *)controlView
{
  _shouldShowCancelButton = [[self stringValue] isEqualToString:@""] ? NO : YES;

  [controlView setNeedsDisplay:YES];
}


// Returns a path around the widget
- (NSBezierPath *)fieldOutlinePathForRect:(NSRect)aRect
{
  NSBezierPath *fieldOutlinePath = [NSBezierPath bezierPath];
  NSPoint aPoint;

  // Start at the top left of the middle section
  aPoint.x = aRect.origin.x + [_leftImage size].width;
  aPoint.y = NSMaxY(aRect) - 2.0;
  [fieldOutlinePath moveToPoint:aPoint];

  // Trace to the top right of the middle section
  aPoint.x = NSMaxX(aRect) - [_rightImage size].width;
  [fieldOutlinePath lineToPoint:aPoint];

  // Trace around the right curve to the bottom right of the middle section
  aPoint.y = aRect.origin.y;
  [fieldOutlinePath curveToPoint:aPoint
                   controlPoint1:NSMakePoint(aPoint.x + 11.0, NSMaxY(aRect))
                   controlPoint2:NSMakePoint(aPoint.x + 11.0, aPoint.y)];

  // Trace to the bottom left of the middle section
  aPoint.x = aRect.origin.x + [_leftImage size].width;
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
  if (_shouldShowCancelButton) {
    NSRect cancelButtonRect, remainderRect;
    float width = [_cancelImage size].width + STFSymbolXOffset*2;

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
  if (hasPopUpButton) {
    NSRect popUpButtonRect, remainderRect;
    float width = 2 * STFSymbolXOffset + [_popUpImage size].width;

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
  float startWidth = [_popUpImage size].width + STFSymbolXOffset;

  // Make room for the cancel image and give it just a little extra padding (1.0)
  float endWidth = [_cancelImage size].width + STFSymbolXOffset + 1.0;
  

  // Cut off the cancel button
  NSDivideRect(aRect, &tempRect, &remainderRect, endWidth, NSMaxXEdge);

  // Cut off the popUp button
  NSDivideRect(remainderRect, &tempRect, &remainderRect, startWidth, NSMinXEdge);

  // Cut a little off the bottom so that the text is centered well
  NSDivideRect(remainderRect, &tempRect, &remainderRect, (float)3, NSMinYEdge);

  return remainderRect;
}

@end

@implementation SearchTextFieldCell(PrivateMethods)

+ (NSImage *)cachedLeftImage
{
  static NSImage *cachedLeftImage = nil;

  if (cachedLeftImage == nil)
    cachedLeftImage = [[NSImage imageNamed:@"SearchLeft"] retain];

  return cachedLeftImage;
}


+ (NSImage *)cachedMiddleImage
{
  static NSImage *cachedMiddleImage = nil;

  if (cachedMiddleImage == nil) {
    cachedMiddleImage = [[NSImage imageNamed:@"SearchMiddle"] retain];
    [cachedMiddleImage setScalesWhenResized:TRUE];
  }

  return cachedMiddleImage;
}


+ (NSImage *)cachedRightImage
{
  static NSImage *cachedRightImage = nil;

  if (cachedRightImage == nil)
    cachedRightImage = [[NSImage imageNamed:@"SearchRight"] retain];

  return cachedRightImage;
}


+ (NSImage *)cachedPopUpImage
{
  static NSImage *cachedPopUpImage = nil;

  if (cachedPopUpImage == nil)
    cachedPopUpImage = [[NSImage imageNamed:@"SearchPopUp"] retain];

  return cachedPopUpImage;
}

+ (NSImage *)cachedNoPopUpImage
{
  static NSImage *cachedNoPopUpImage = nil;
  
  if (cachedNoPopUpImage == nil)
    cachedNoPopUpImage = [[NSImage imageNamed:@"SearchNoPopUp"] retain];
  
  return cachedNoPopUpImage;
}


+ (NSImage *)cachedCancelImage
{
  static NSImage *cachedCancelImage = nil;

  if (cachedCancelImage == nil)
    cachedCancelImage = [[NSImage imageNamed:@"SearchCancel"] retain];

  return cachedCancelImage;
}

@end
