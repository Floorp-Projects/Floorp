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
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Simon Fraser <smfr@smfr.org>
 *
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
#import "ExtendedSplitView.h"

@interface ExtendedSplitView(Private)

- (NSString*)defaultsKey;
- (void)restoreSplitterPosition;
- (void)saveSplitterPosition;
- (float)relevantDimensionForView:(NSView*)inView;
- (void)setRelevantDimensionForView:(NSView*)inView to:(float)inSize;

@end

#pragma mark -

@implementation ExtendedSplitView

- (id)initWithFrame:(NSRect)frame
{
  if ((self = [super initWithFrame:frame]))
  {
    // we'll default to saving (if we have an autosaveName)
    mAutosaveSplitterPosition = YES;
  }
  return self;
}

- (void)dealloc
{
  [mAutosaveName release];
  [super dealloc];
}

- (void)setAutosaveName:(NSString *)name
{
  if (![mAutosaveName isEqualToString:name])
  {
    [mAutosaveName autorelease];
    mAutosaveName = [name retain];
    [self restoreSplitterPosition];
  }
}

- (NSString *)autosaveName
{
  return mAutosaveName;
}

- (BOOL)autosaveSplitterPosition
{
  return mAutosaveSplitterPosition;
}

- (void)setAutosaveSplitterPosition:(BOOL)inAutosave
{
  mAutosaveSplitterPosition = inAutosave;
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
  // if we're leaving the window, save the splitter position
  if (!newWindow)
  {
    [self saveSplitterPosition];
  }
}

- (float)relevantDimensionForView:(NSView*)inView
{
  if ([self isVertical])
    return NSWidth([inView frame]);

  return NSHeight([inView frame]);
}

- (void)setRelevantDimensionForView:(NSView*)inView to:(float)inSize
{
  NSSize frameSize = [inView frame].size;
  if ([self isVertical])
    frameSize.width = inSize;
  else
    frameSize.height = inSize;

  [inView setFrameSize:frameSize];
}

- (NSString*)defaultsKey
{
  return [NSString stringWithFormat:@"ExtendedSplitView Sizes ", mAutosaveName];
}

- (void)restoreSplitterPosition
{
  if (mAutosaveSplitterPosition && [mAutosaveName length] > 0)
  {
    NSArray* splitterArray = [[NSUserDefaults standardUserDefaults] arrayForKey:[self defaultsKey]];
    if (!splitterArray)
      return;

    unsigned int numSubviews = [[self subviews] count];
    if (numSubviews - 1 != [splitterArray count])
      return;   // number of subviews doesn't match what we saved (which is num views - 1)

    float spaceForSubviews = [self relevantDimensionForView:self] - (numSubviews - 1) * [self dividerThickness];

    float spaceUsed = 0.0f;
    unsigned int i;
    for (i = 0; i < numSubviews - 1; i ++)
    {
      NSView* curView   = [[self subviews] objectAtIndex:i];
      id curItem = [splitterArray objectAtIndex:i];
      if (![curItem isKindOfClass:[NSNumber class]])
        break;

      float curItemProportion = [curItem floatValue];
      float curItemSpace = curItemProportion * spaceForSubviews;
      spaceUsed += curItemSpace;

      [self setRelevantDimensionForView:curView to:curItemSpace];
    }

    // now fix up the last view, otherwise -adjustSubviews will mess stuff up
    float spaceLeft = spaceForSubviews - spaceUsed;
    [self setRelevantDimensionForView:[self lastSubview] to:spaceLeft];

    [self adjustSubviews];
  }  
}

- (void)saveSplitterPosition
{
  if (mAutosaveSplitterPosition && [mAutosaveName length] > 0)
  {
    NSMutableArray* splitterArray = [NSMutableArray array];
    
    unsigned int numSubviews = [[self subviews] count];
    float spaceForSubviews = [self relevantDimensionForView:self] - (numSubviews - 1) * [self dividerThickness];

    // save the proportions of the total width (or height) for each view, subtracting splitter widths
    // we only need to save N-1, since the last view takes up the remaining space
    unsigned int i;
    for (i = 0; i < numSubviews - 1; i ++)
    {
      NSView* curView = [[self subviews] objectAtIndex:i];
      float thisViewSpace = [self relevantDimensionForView:curView];

      // save the proportion taken by this view
      [splitterArray addObject:[NSNumber numberWithFloat:(thisViewSpace / spaceForSubviews)]];
    }    

    [[NSUserDefaults standardUserDefaults] setObject:splitterArray forKey:[self defaultsKey]];
  }
}


@end // ExtendedSplitView
