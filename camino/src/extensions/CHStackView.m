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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Calum Robinson.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Calum Robinson <calumr@mac.com>
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
 
 /*
  This class was inspired by the OAStack view from OmniGroup, but not copied directly. This
  implementation is a little simpler, which is fine. 
  
  It's like NSTableView, except the data source returns views instead of strings/images etc.
*/

#import "CHStackView.h"

NSString* StackViewReloadNotificationName  = @"ReloadStackView";
NSString* StackViewResizedNotificationName = @"StackViewResized";

@implementation CHStackView

- (id)initWithFrame:(NSRect)frameRect
{
  if ((self = [super initWithFrame:frameRect]))
  {
    // Register for notifications when one of the subviews changes size
    [[NSNotificationCenter defaultCenter] addObserver:self
                                            selector:@selector(reloadNotification:)
                                            name:StackViewReloadNotificationName
                                            object:nil];
  
  }
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)setDataSource:(id)aDataSource
{
  mDataSource = aDataSource;   // should this retain?
  
  [self reloadSubviews];
}

- (void)reloadNotification:(NSNotification *)notification
{
  [self reloadSubviews];
}

- (void)reloadSubviews
{
  NSRect newFrame  = [self frame];
  NSSize oldSize   = [self bounds].size;
  int i, subviewCount = [mDataSource subviewsForStackView:self];
    
  NSPoint nextOrigin = NSZeroPoint;
  
  // we'll maintain the width  of the stack view, assuming that it's
  // scaled by its superview.
  newFrame.size.height = 0.0;
  
  [[self subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
  
  for (i = 0; i < subviewCount; i++)
  {
    NSView *subview      = [mDataSource viewForStackView:self atIndex:i];
    NSRect  subviewFrame = [subview frame];
    
    unsigned int autoResizeMask = [subview autoresizingMask];
    if (autoResizeMask & NSViewWidthSizable)
      subviewFrame.size.width = newFrame.size.width;
    
    newFrame.size.height += NSHeight(subviewFrame);
    
    subviewFrame.origin = nextOrigin;
    [subview setFrame:subviewFrame];
    nextOrigin.y += NSHeight(subviewFrame);
    
    [self addSubview:subview];
    
    // If there are more subviews, add a separator
    if (i + 1 < subviewCount)
    {
      NSBox *separator = [[NSBox alloc] initWithFrame:
          NSMakeRect(nextOrigin.x, nextOrigin.y - 1.0, NSWidth([subview frame]), 1.0)];      
      [separator setBoxType:NSBoxSeparator];
      [separator setAutoresizingMask:NSViewWidthSizable];
      [self addSubview:separator];
      [separator release];
    }
  }
  
  NSRect newBounds = newFrame;
  newBounds.origin = NSZeroPoint;
  
  [self setFrame:newFrame];
  [self setNeedsDisplay:YES];

  [[NSNotificationCenter defaultCenter] postNotificationName:StackViewResizedNotificationName
              object:self
              userInfo:[NSDictionary dictionaryWithObjectsAndKeys:[NSValue valueWithSize:oldSize], @"oldsize", nil]];
}

- (BOOL)isFlipped
{
  return YES;
}

@end
