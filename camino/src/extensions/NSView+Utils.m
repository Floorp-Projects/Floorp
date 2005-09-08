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

#import "NSView+Utils.h"

BOOL CHCloseSizes(NSSize aSize, NSSize bSize, float slop)
{
  return (fabs(aSize.width - bSize.width) <= slop) &&
         (fabs(aSize.height - bSize.height) <= slop);
}

// mask has 3 bits: 0 = 1 scales, 1 = 2 scales, 2 = 3 scales.
static void RedistributeSpace(int resizeMask, float newWidth, /* in out */ float ioSpaces[3])
{
  float oldWidth = ioSpaces[0] + ioSpaces[1] + ioSpaces[2];

  if (resizeMask != 0)
  {
    switch (resizeMask)
    {
      // 1 scalable section
      case 1:	// first
        ioSpaces[0] += newWidth - oldWidth;			// it can go negative
        break;
      case 2:	// middle
        ioSpaces[1] += newWidth - oldWidth;			// it can go negative
        break;
      case 4:	// last
        ioSpaces[2] += newWidth - oldWidth;			// it can go negative
        break;
      
      // 2 scalable sections
      case 3:		// last fixed
        {
        float oldScaledWidth = ioSpaces[0] + ioSpaces[1];
        float newScaledWidth = (newWidth - ioSpaces[2]);
        ioSpaces[0] = newScaledWidth * (ioSpaces[0] / oldScaledWidth);
        ioSpaces[1] = newScaledWidth * (ioSpaces[1] / oldScaledWidth);
        break;
        }
      case 5:		// middle fixed
        {
        float oldScaledWidth = ioSpaces[0] + ioSpaces[2];
        float newScaledWidth = (newWidth - ioSpaces[1]);
        ioSpaces[0] = newScaledWidth * (ioSpaces[0] / oldScaledWidth);
        ioSpaces[2] = newScaledWidth * (ioSpaces[2] / oldScaledWidth);
        break;
        }
      case 6:		// first fixed
        {
        float oldScaledWidth = ioSpaces[1] + ioSpaces[2];
        float newScaledWidth = (newWidth - ioSpaces[0]);
        ioSpaces[1] = newScaledWidth * (ioSpaces[1] / oldScaledWidth);
        ioSpaces[2] = newScaledWidth * (ioSpaces[2] / oldScaledWidth);
        break;
        }
        
      // all scalable
      case 7:
        ioSpaces[0] *= newWidth / oldWidth;
        ioSpaces[1] *= newWidth / oldWidth;
        ioSpaces[2] *= newWidth / oldWidth;
        break;
    }
  }
  // otherwise nothing changes
}

@implementation NSView(CHViewUtils)

- (void)moveToView:(NSView*)destView resize:(BOOL)resize
{
  //resize &= [destView autoresizesSubviews];
  
  NSRect oldFrame       = [self frame];
  NSRect oldSuperBounds = [[self superview] bounds];
  
  [self retain];
  [self removeFromSuperview];
  
  [destView addSubview:self];
  [self release];
  
  [destView setAutoresizesSubviews:YES];
  
  if (resize)
  {
    unsigned int resizeMask = [self autoresizingMask];
    NSRect newFrame = oldFrame;
    
    if (resizeMask != NSViewNotSizable && !NSEqualRects([destView bounds], oldSuperBounds))
    {
      float spaces[3];
      // horizontal
      float newWidth = NSWidth([destView bounds]);
      spaces[0] = NSMinX(oldFrame);
      spaces[1] = NSWidth(oldFrame);
      spaces[2] = NSMaxX(oldSuperBounds) - NSMaxX(oldFrame);
      
      RedistributeSpace(resizeMask & 0x07, newWidth, spaces);
      newFrame.origin.x = spaces[0];
      newFrame.size.width = spaces[1];

      // vertical. we assume that the view should stick to the top of the destView here
      float newHeight = NSHeight([destView bounds]);
      spaces[0] = NSMinY(oldFrame);
      spaces[1] = NSHeight(oldFrame);
      spaces[2] = NSMaxY(oldSuperBounds) - NSMaxY(oldFrame);
      
      RedistributeSpace((resizeMask >> 3) & 0x07, newHeight, spaces);
      newFrame.origin.y = spaces[0];
      newFrame.size.height = spaces[1];
    }
    
    [self setFrame:newFrame];
    [self setNeedsDisplay:YES];
  }
}

- (NSView*)swapFirstSubview:(NSView*)newSubview
{
  NSView* existingSubview = [self firstSubview];
  if (existingSubview == newSubview)
    return nil;

  [existingSubview retain];
  [existingSubview removeFromSuperview];
  
  [self addSubview:newSubview];
  [newSubview setFrame:[self bounds]];

  return [existingSubview autorelease];
}

- (NSView*)firstSubview
{
  NSArray* subviews = [self subviews];
  if ([subviews count] > 0)
    return [[self subviews] objectAtIndex:0];
  return 0;
}

- (NSView*)lastSubview
{
  NSArray* subviews = [self subviews];
  unsigned int numSubviews = [subviews count];
  if (numSubviews > 0)
    return [[self subviews] objectAtIndex:numSubviews - 1];
  return 0;
}

- (void)removeAllSubviews
{
  // clone the array to avoid issues with the array changing during the enumeration
  NSArray* subviewsArray = [[self subviews] copy];
  [subviewsArray makeObjectsPerformSelector:@selector(removeFromSuperview)];
}

@end
