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
* Portions created by the Initial Developer are Copyright (C) 2004
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
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

#import "NSSplitView+Utils.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@implementation NSSplitView (CaminoExtensions)

//
// -leftWidth
//
// Returns the width (in pixels) of the left panel in the splitter
//
- (float)leftWidth
{
  if ([[self subviews] count] < 1)
    return 0.0;

  NSRect leftFrame = [[[self subviews] objectAtIndex:0] frame];
  return leftFrame.size.width;
}

//
// -setLeftWidth:
//
// Sets the width of the left frame to |newWidth| (in pixels) and the right frame
// to the rest of the space. Does nothing if there are less than two panes.
//
- (void)setLeftWidth:(float)newWidth;
{
  if ([[self subviews] count] < 2 || newWidth < 0)
    return;

  // Since we're just setting the widths of the frames (not their origins),
  // we can ignore the thickness of the slider itself.
  NSView* leftView = [[self subviews] objectAtIndex:0];
  NSView* rightView = [[self subviews] objectAtIndex:1];
  NSRect leftFrame = [leftView frame];
  NSRect rightFrame = [rightView frame];
  float totalWidth = leftFrame.size.width + rightFrame.size.width;
  
  leftFrame.size.width = newWidth;
  rightFrame.size.width = totalWidth - newWidth;
  [leftView setFrame:leftFrame];
  [rightView setFrame:rightFrame];
  [self setNeedsDisplay: YES];
}

@end
