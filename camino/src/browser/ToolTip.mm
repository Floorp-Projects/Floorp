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
 * Contributor(s): Richard Schreyer
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

#import "ToolTip.h"
#import "NSScreen+Utils.h"

@interface ToolTip (ToolTipPrivateMethods)

@end

const float kBorderPadding = 2.0;

@implementation ToolTip

- (id)init
{
  
  self = [super init];
  if (self) {
    mPanel = [[NSPanel alloc] initWithContentRect: NSMakeRect(0.0, 0.0, 200.0, 200.0) styleMask: NSBorderlessWindowMask backing: NSBackingStoreBuffered defer: YES];
    
    // Create a textfield as the content of our new window.
    // Field occupies all but the top 2 and bottom 2 pixels of the panel (bug 149635)
    mTextField = [[NSTextField alloc] initWithFrame: NSMakeRect(0.0, kBorderPadding, 200.0, 200 - 2*kBorderPadding)];
    
    [[mPanel contentView] addSubview: mTextField];
    [mTextField release]; //window holds ref
    [mPanel setHasShadow: YES];
    [mPanel setBackgroundColor: [NSColor colorWithCalibratedRed: 1.0 green: 1.0 blue: .81 alpha: 1.0]];
    
    [mTextField setDrawsBackground: NO];
    [mTextField setEditable: NO];
    [mTextField setSelectable: NO];
    [mTextField setFont: [NSFont toolTipsFontOfSize: [NSFont smallSystemFontSize]]];
    [mTextField setBezeled: NO];
    [mTextField setBordered: NO];
  }
  return self;
}

- (void)dealloc
{
  [mPanel close];
  [mPanel release];
  [super dealloc];
}

- (void)showToolTipAtPoint:(NSPoint)point withString:(NSString*)string
{
  if ([string length] == 0)
    return;

  NSScreen* screen = [NSScreen screenForPoint: point];
  if (!screen)
    screen = [NSScreen mainScreen];

  if (screen) {
    NSRect screenFrame = [screen visibleFrame];
    NSSize screenSize = screenFrame.size;
    //  set up the panel
    [mTextField setStringValue: string];
    [mTextField sizeToFit];
    NSRect fieldFrame = [mTextField frame];
    NSSize textSize = fieldFrame.size;
    textSize.height += kBorderPadding + kBorderPadding;
    [mPanel setContentSize: textSize];
    
    // the given point is right where the mouse pointer is.  We want the tooltip's
    // top left corner somewhere below that, but not if that'll put it off the monitor. There
    // is no way that we can go off the top of the monitor because cocoa won't let us position a window
    // that way
    const int kVOffset = 20;
    if ( point.y - kVOffset - textSize.height > NSMinY(screenFrame) )
      point.y -= kVOffset;
    else
      point.y += kVOffset;
    [mPanel setFrameTopLeftPoint: point];
    
    //  if it goes off the edge of the screen, shift around to put it all on the screen

    float amountOffScreenX = NSMaxX(screenFrame) - NSMaxX([mPanel frame]);
    if (amountOffScreenX < 0) {
      NSRect movedFrame = [mPanel frame];
      movedFrame.origin.x += amountOffScreenX;
      [mPanel setFrame: movedFrame display: NO];
    }
    
    //  show the panel
    [mPanel orderFront: nil];
  }
}

- (void)closeToolTip
{
  [mPanel close];
}

@end
