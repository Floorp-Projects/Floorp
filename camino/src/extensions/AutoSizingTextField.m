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
 * Calum Robinson.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <smfr@smfr.org>
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
#import "AutoSizingTextField.h"


@interface AutoSizingTextField(Private)

- (NSSize)sizeForFrame:(NSRect)inFrame;
- (void)adjustSize;

@end

#pragma mark -

@implementation AutoSizingTextField

- (void)setStringValue:(NSString*)inString
{
  [super setStringValue:inString];
  [self adjustSize];
}

- (void)setAttributedStringValue:(NSAttributedString *)inString
{
  [super setAttributedStringValue:inString];
  [self adjustSize];
}

- (void)setFrame:(NSRect)frameRect
{
  NSSize origSize = frameRect.size;
  frameRect.size = [self sizeForFrame:frameRect];

  if (![[self superview] isFlipped])
    frameRect.origin.y -= (frameRect.size.height - origSize.height);

  mSettingFrameSize = YES;
  [super setFrame:frameRect];
  mSettingFrameSize = NO;
}

- (void)setFrameSize:(NSSize)newSize
{
  if (mSettingFrameSize)
  {
    [super setFrameSize:newSize];
    return;
  }

  NSRect newFrame = [self frame];
  newFrame.size = newSize;
  [super setFrameSize:[self sizeForFrame:newFrame]];
}

- (NSSize)sizeForFrame:(NSRect)inFrame
{
  inFrame.size.height = 10000.0f;
  NSSize newSize = [[self cell] cellSizeForBounds:inFrame];
  // don't let it get zero height
  if (newSize.height == 0.0f)
  {
    NSFont* cellFont = [[self cell] font];
    float lineHeight = [cellFont ascender] - [cellFont descender];
    newSize.height = lineHeight;
  }
  
  newSize.width = inFrame.size.width;
  return newSize;
}

- (void)adjustSize
{
  // NSLog(@"%@ adjustSize (%@)", self, [self stringValue]);
  [self setFrameSizeMaintainingTopLeftOrigin:[self sizeForFrame:[self frame]]];
  //[self setFrameSize:[self sizeForFrame:[self frame]]];
}

@end
