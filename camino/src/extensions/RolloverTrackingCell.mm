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
* The Original Code is tab UI for Camino.
*
* The Initial Developer of the Original Code is 
* Geoff Beier.
* Portions created by the Initial Developer are Copyright (C) 2004
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*   Geoff Beier <me@mollyandgeoff.com>
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

#import "RolloverTrackingCell.h"

@implementation RolloverTrackingCell

-(void)dealloc
{
  [mUserData release];
  [super dealloc];
}

-(void)setFrame:(NSRect)newFrame
{
  mFrame = newFrame;
}

-(NSRect)frame
{
  return mFrame;
}

-(NSSize)size
{
  return mFrame.size;
}

-(BOOL)mouseWithin
{
  return mMouseWithin;
}

-(void)addTrackingRectInView:(NSView *)aView withFrame:(NSRect)trackingRect cursorLocation:(NSPoint)currentLocation
{
  if (mTrackingTag != 0)
    [self removeTrackingRectFromView: aView];
  mUserData = [[NSDictionary dictionaryWithObjectsAndKeys:aView, @"view", nil] retain];
  mMouseWithin = NSPointInRect(currentLocation, trackingRect);
  mTrackingTag = [aView addTrackingRect:trackingRect owner:self userData:mUserData assumeInside:mMouseWithin];
}

- (void)removeTrackingRectFromView:(NSView *)aView
{
  [aView removeTrackingRect:mTrackingTag];
  mTrackingTag = 0;
  [mUserData release];
  mUserData = nil;
}

- (void)mouseEntered:(NSEvent *)theEvent
{
  NSDictionary *userData = (NSDictionary *)[theEvent userData];
  NSView *view = [userData objectForKey:@"view"];
  mMouseWithin = YES;
  // only act on the mouseEntered if the view is active or accepts the first mouse click
  if ([[view window] isKeyWindow] || [view acceptsFirstMouse:theEvent]) {
    [view setNeedsDisplayInRect:[self frame]];
    // calling displayIfNeeded prevents the "lag" observed when displaying rollover events
    [view displayIfNeeded];
  }
}

- (void)mouseExited:(NSEvent*)theEvent
{
  NSDictionary *userData = (NSDictionary*)[theEvent userData];
  NSView *view = [userData objectForKey:@"view"];
  mMouseWithin = NO;
  // only act on the mouseExited if the view is active or accepts the first mouse click
  if ([[view window] isKeyWindow] || [view acceptsFirstMouse:theEvent]) {
	  [view setNeedsDisplayInRect:[self frame]];
    // calling displayIfNeeded prevents the "lag" observed when displaying rollover events
    [view displayIfNeeded];
  }
}

- (void)setDragTarget:(BOOL)isDragTarget
{
  mIsDragTarget = isDragTarget;
  // we may be getting this in lieu of a mouse enter/exit event
  mMouseWithin = isDragTarget;
}

- (BOOL)dragTarget
{
  return mIsDragTarget;
}
  
  

@end
