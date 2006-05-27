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
 * The Original Code is Camino code.
 *
 * The Initial Developer of the Original Code is
 * Aaron Schulman <aschulm@umd.edu>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Schulman <aschulm@umd.edu>
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

#import "RolloverImageButton.h"

@interface RolloverImageButton (Private)
  - (void)updateImage:(BOOL)inIsInside;
  - (BOOL)isMouseInside;
  - (void)removeTrackingRectFromView:(NSView *)inView;
  - (void)updateTrackingRectInSuperview;
@end

@implementation RolloverImageButton

- (id)initWithFrame:(NSRect)inFrame
{
  if ((self = [super initWithFrame:inFrame])) {
    mImage = nil;
    mHoverImage = nil;
    mTrackingTag = -1;
  }
  return self;
}

- (void)dealloc
{
  [mImage release];
  [mHoverImage release];
  
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self];
  [super dealloc];
}

- (void)removeFromSuperview
{
  [self removeTrackingRectFromView:[self superview]];
  [super removeFromSuperview];
}

- (void)setEnabled:(BOOL)inStatus
{
  [super setEnabled:inStatus];
  if ([self isEnabled])
    [self updateTrackingRectInSuperview];
  else {
    [self updateImage:NO];
    [self removeTrackingRectFromView:[self superview]];
  }
}

- (void)viewDidMoveToWindow
{
  [self updateTrackingRectInSuperview];
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  // unregister the button from observering the current window just in case a tab moves from one window to another
  [nc removeObserver:self];
  if ([self window]) {
    [nc addObserver:self selector:@selector(handleWindowIsKey:)
          name:NSWindowDidBecomeKeyNotification object:[self window]];
    [nc addObserver:self selector:@selector(handleWindowResignKey:)
          name:NSWindowDidResignKeyNotification object:[self window]];
  }
}

- (void)setFrame:(NSRect)inFrame
{
  NSRect oldFrame = [self frame];
  [super setFrame:inFrame];
  if (!NSEqualRects(oldFrame, [self frame]))
    [self updateTrackingRectInSuperview];
}

- (void)setFrameOrigin:(NSPoint)inOrigin
{
  NSPoint oldOrigin = [self frame].origin;
  [super setFrameOrigin:inOrigin];
  if (!NSEqualPoints(oldOrigin,[self frame].origin))
    [self updateTrackingRectInSuperview];
}

- (void)setFrameSize:(NSSize)inSize
{
  NSSize oldSize = [self frame].size;
  [super setFrameSize:inSize];
  if (!NSEqualSizes(oldSize,[self frame].size))
    [self updateTrackingRectInSuperview];
}

- (void)mouseEntered:(NSEvent*)theEvent
{
  if (([[self window] isKeyWindow] || [self acceptsFirstMouse:theEvent])
    && [theEvent trackingNumber] == mTrackingTag)
      [self updateImage:YES];
}

- (void)mouseExited:(NSEvent*)theEvent
{
  if (([[self window] isKeyWindow] || [self acceptsFirstMouse:theEvent]) 
    && [theEvent trackingNumber] == mTrackingTag) 
      [self updateImage:NO];
}

- (void)mouseDown:(NSEvent*)theEvent
{
  [self updateImage:NO];
  [super mouseDown:theEvent];
  // update button's image based on location of mouse after button has been released
  [self updateImage:[self isMouseInside]];
}

- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent
{
  return NO;
}

- (void)setImage:(NSImage*)inImage
{
  if (mImage != inImage) {
    [mImage release];
    mImage = [inImage retain];
    [self updateImage:[self isMouseInside]];
  }
}

- (void)setHoverImage:(NSImage*)inImage
{
  if (mHoverImage != inImage) {
    [mHoverImage release];
    mHoverImage = [inImage retain];
    [self updateImage:[self isMouseInside]];
  }
}

- (void)handleWindowIsKey:(NSWindow *)inWindow
{
  [self updateImage:[self isMouseInside]];
}

- (void)handleWindowResignKey:(NSWindow *)inWindow
{
  [self updateImage:NO];
}

- (void)resetCursorRects
{
  [self updateTrackingRectInSuperview];
}

@end

@implementation RolloverImageButton (Private)

- (void)updateImage:(BOOL)inIsInside
{
  if (inIsInside) {
    if ([self isEnabled])
      [super setImage:mHoverImage];
  }
  else
    [super setImage:mImage];
}

- (BOOL)isMouseInside
{
  NSPoint mousePointInWindow = [[self window] convertScreenToBase:[NSEvent mouseLocation]];
  NSPoint mousePointInView = [[self superview] convertPoint:mousePointInWindow fromView:nil];
  return NSMouseInRect(mousePointInView, [self frame], NO);
}

- (void)removeTrackingRectFromView:(NSView *)inView
{
  [inView removeTrackingRect:mTrackingTag];
  mTrackingTag = -1;
}

- (void)updateTrackingRectInSuperview
{
  if (mTrackingTag != -1)
    [self removeTrackingRectFromView:[self superview]];
  BOOL mouseInside = [self isMouseInside];
  mTrackingTag = [[self superview] addTrackingRect:[self frame] owner:self userData:nil assumeInside:mouseInside];
  [self updateImage:mouseInside];
}

@end