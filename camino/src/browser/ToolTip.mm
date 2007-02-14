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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Schreyer
 *   Josh Aas <josh@mozilla.com>
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

#import "ToolTip.h"
#import "NSScreen+Utils.h"

@interface ToolTip (ToolTipPrivateMethods)

- (void)parentWindowDidResignKey:(NSNotification*)inNotification;

@end

const float kBorderPadding = 2.0;
const float kMaxTextFieldWidth = 250.0;
const float kVOffset = 20.0;

@implementation ToolTip

- (id)init
{
  if ((self = [super init]))
  {
    // the ref from -alloc is balanced by the -release in dealloc
    mTooltipWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0.0, 0.0, kMaxTextFieldWidth, 0.0)
                                         styleMask:NSBorderlessWindowMask
                                           backing:NSBackingStoreBuffered
                                             defer:YES];

    // we don't want closing the window to release it, because we aren't always in control
    // of the close (AppKit may do it on quit).
    [mTooltipWindow setReleasedWhenClosed:NO];
    
    // Create a textfield as the content of our new window.
    // Field occupies all but the top 2 and bottom 2 pixels of the panel (bug 149635)
    mTextView = [[NSTextView alloc] initWithFrame:NSMakeRect(0.0, kBorderPadding, kMaxTextFieldWidth, 0.0)];
    [[mTooltipWindow contentView] addSubview:mTextView];
    [mTextView release]; // window holds ref
    
    // set up the panel
    [mTooltipWindow setHasShadow:YES];
    [mTooltipWindow setBackgroundColor:[NSColor colorWithCalibratedRed:1.0 green:1.0 blue:0.81 alpha:1.0]];
    

    // set up the text view
    [mTextView setDrawsBackground:NO];
    [mTextView setEditable:NO];
    [mTextView setSelectable:NO];
    [mTextView setFont:[NSFont toolTipsFontOfSize:[NSFont smallSystemFontSize]]];
    [mTextView setMinSize:NSMakeSize(0.0, 0.0)];
    [mTextView setHorizontallyResizable:YES];
    [mTextView setVerticallyResizable:YES];
  }
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [mTooltipWindow close];   // we set the window not to release on -close
  [mTooltipWindow release];

  [super dealloc];
}

- (void)showToolTipAtPoint:(NSPoint)point withString:(NSString*)string overWindow:(NSWindow*)inWindow
{
  if ([string length] == 0)
    return;

  NSScreen* screen = [NSScreen screenForPoint:point];
  if (!screen)
    screen = [NSScreen mainScreen];

  if (!screen)
    return;

  // register for window losing key status notifications, so we can hide the tooltip
  // on window deactivation
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(parentWindowDidResignKey:)
                                               name:NSWindowDidResignKeyNotification
                                             object:inWindow];

  NSRect screenFrame = [screen visibleFrame];
  NSSize screenSize = screenFrame.size;

  // for some reason, text views suffer from hysteresis; the answer you get this time
  // depends on what you had in there before. so clear state first.
  [mTextView setString:@""];
  [mTextView setFrame:NSMakeRect(0, kBorderPadding, 0, 0)];

  // -sizeToFit sucks. For some reason it likes to wrap short words, so
  // we measure the text by hand and set that as the min width.
  NSSize stringSize = [string sizeWithAttributes:[NSDictionary dictionaryWithObject:[NSFont toolTipsFontOfSize:[NSFont smallSystemFontSize]] forKey:NSFontAttributeName]];
  float textViewWidth = ceil(stringSize.width);
  if (textViewWidth > kMaxTextFieldWidth)
    textViewWidth = kMaxTextFieldWidth;

  textViewWidth += 2.0 * 5.0;   // magic numbers required to make the text not wrap. No, this isn't -textContainerInset.
  
  // set up the text view
  [mTextView setMaxSize:NSMakeSize(kMaxTextFieldWidth, screenSize.height - 2 * kBorderPadding)]; // do this here since we know screen size
  [mTextView setString:string]; // do this after setting max size, before setting constrained frame size, 
                                // reset to max width - it will not grow horizontally when resizing, only vertically
  [mTextView setConstrainedFrameSize:NSMakeSize(kMaxTextFieldWidth, 0.0)];
  // to avoid wrapping when we don't want it, set the min width
  [mTextView setMinSize:NSMakeSize(textViewWidth, 0.0)];

  // finally, do the buggy sizeToFit
  [mTextView sizeToFit];
  
  // set the origin back where its supposed to be
  NSRect textViewFrame = [mTextView frame];
  [mTextView setFrame:NSMakeRect(0, kBorderPadding, textViewFrame.size.width, textViewFrame.size.height)];
  
  // size the panel correctly, taking border into account
  NSSize textSize = textViewFrame.size;
  textSize.height += kBorderPadding + kBorderPadding;
  [mTooltipWindow setContentSize:textSize];
  
  // We try to put the top left point right below the cursor. If that doesn't fit
  // on screen, put the bottom left point above the cursor.
  if (point.y - kVOffset - textSize.height > NSMinY(screenFrame)) {
    point.y -= kVOffset;
    [mTooltipWindow setFrameTopLeftPoint:point];
  }
  else {
    point.y += kVOffset / 2.5;
    [mTooltipWindow setFrameOrigin:point];
  }
  
  // if it doesn't fit on screen horizontally, adjust so that it does
  float amountOffScreenX = NSMaxX(screenFrame) - NSMaxX([mTooltipWindow frame]);
  if (amountOffScreenX < 0) {
    NSRect movedFrame = [mTooltipWindow frame];
    movedFrame.origin.x += amountOffScreenX;
    [mTooltipWindow setFrame:movedFrame display:NO];
  }

  // add as a child window    
  [inWindow addChildWindow:mTooltipWindow ordered:NSWindowAbove];
  // show the panel
  [mTooltipWindow orderFront:nil];
}

- (void)closeToolTip
{
  // we can get -closeToolTip even if we didn't show it
  if ([mTooltipWindow isVisible])
  {
    [[mTooltipWindow parentWindow] removeChildWindow:mTooltipWindow];
    [mTooltipWindow orderOut:nil];
  }

  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)parentWindowDidResignKey:(NSNotification*)inNotification
{
  [self closeToolTip];
}

@end
