/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the Mozilla browser.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation. Portions created by Netscape are
* Copyright (C) 2002 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   David Hyatt <hyatt@netscape.com> (Original Author)
*/

#import "CHBookmarksButton.h"
#import "CHBookmarksToolbar.h"
#import "BookmarksService.h"
#include "nsIDOMElement.h"

@implementation CHBookmarksToolbar

- (id)initWithFrame:(NSRect)frame {
  if ( (self = [super initWithFrame:frame]) ) {
    mBookmarks = nsnull;
    mButtons = [[NSMutableArray alloc] init];
  }
  return self;
}

-(void)initializeToolbar
{
  // Initialization code here.
  mBookmarks = new BookmarksService(self);
  mBookmarks->AddObserver();
  mBookmarks->EnsureToolbarRoot();
  [self buildButtonList];
}

-(void) dealloc
{
  [mButtons autorelease];
  mBookmarks->RemoveObserver();
  delete mBookmarks;
  [super dealloc];
}

- (void)drawRect:(NSRect)aRect {
  // Fill the background with our background color.
  //[[NSColor colorWithCalibratedWhite: 0.98 alpha: 1.0] set];
  //NSRectFill(aRect);

  //printf("The rect is: %f %f %f %f\n", aRect.origin.x, aRect.origin.y, aRect.size.width, aRect.size.height);
  
  if (aRect.origin.y + aRect.size.height ==
      [self bounds].size.height) {
    // The personal toolbar is 21 pixels tall.  The bottom two pixels
    // are a separator.
    [[NSColor colorWithCalibratedWhite: 0.90 alpha: 1.0] set];
    NSRectFill(NSMakeRect(aRect.origin.x, [self bounds].size.height-2, aRect.size.width, [self bounds].size.height));
  }

  // The buttons will paint themselves. Just call our base class method.
  [super drawRect: aRect];
}

-(void)buildButtonList
{
  // Build the buttons, and then lay them all out.
  nsCOMPtr<nsIDOMNode> child;
  BookmarksService::gToolbarRoot->GetFirstChild(getter_AddRefs(child));
  while (child) {
    nsCOMPtr<nsIDOMElement> childElt(do_QueryInterface(child));
    if (childElt) {
      CHBookmarksButton* button = [[[CHBookmarksButton alloc] initWithFrame: NSMakeRect(2, 1, 100, 17)] autorelease];
      [button setElement: childElt];
      [self addSubview: button];
      [mButtons addObject: button];
    }

    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
  }

  [self reflowButtons];
}

-(void)addButton: (nsIDOMElement*)aElt atIndex: (int)aIndex
{
  CHBookmarksButton* button = [[[CHBookmarksButton alloc] initWithFrame: NSMakeRect(2, 1, 100, 17)] autorelease];
  [button setElement: aElt];
  [self addSubview: button];
  [mButtons insertObject: button atIndex: aIndex];
  [self reflowButtonsStartingAtIndex: aIndex];
}

-(void)removeButton: (nsIDOMElement*)aElt
{
  int count = [mButtons count];
  for (int i = 0; i < count; i++) {
    CHBookmarksButton* button = [mButtons objectAtIndex: i];
    if ([button element] == aElt) {
      [mButtons removeObjectAtIndex: i];
      [button removeFromSuperview];
      if (count > i)
        [self reflowButtonsStartingAtIndex: i];
      break;
    }
  }

  [self setNeedsDisplay: YES];
}

-(void)reflowButtons
{
  [self reflowButtonsStartingAtIndex: 0];
}

-(void)reflowButtonsStartingAtIndex: (int)aIndex
{
  float oldHeight = [self frame].size.height;
  float computedHeight = 18;
  int count = [mButtons count];
  float currY = 1.0;
  float prevX = 2.0;
  if (aIndex > 0) {
    CHBookmarksButton* prevButton = [mButtons objectAtIndex: (aIndex-1)];
    prevX += [prevButton frame].origin.x + [prevButton frame].size.width;
    currY = [prevButton frame].origin.y;
  }
  for (int i = aIndex; i < count; i++) {
    CHBookmarksButton* button = [mButtons objectAtIndex: i];
    [button sizeToFit];
    float width = [button frame].size.width;
    float height = [button frame].size.height;
    if (width > 150)
      width = 150;
    if (height < 16)
      height = 16; // Our folder tiff is only 15 pixels for some reason.
    [button setFrame: NSMakeRect(prevX, currY, width, height)];

    prevX += [button frame].size.width + 2;

    if ([self bounds].size.width < prevX) {
      // The previous button didn't fit.  We need to make a new row.
      currY += 18;
      computedHeight += 18;
      if (computedHeight > oldHeight) {
        [self setFrame: NSMakeRect([self frame].origin.x, [self frame].origin.y+(oldHeight-computedHeight),
                                   [self frame].size.width, computedHeight)];
        [self setNeedsDisplay: YES];
      }
      
      prevX = 2;
      [button setFrame: NSMakeRect(prevX, currY, width, height)];
      prevX += [button frame].size.width + 2;
    }
    
    [button setNeedsDisplay: YES];
  }

  float currentHeight = [self frame].size.height;
  if (computedHeight != currentHeight) {
    [self setFrame: NSMakeRect([self frame].origin.x, [self frame].origin.y + (currentHeight - computedHeight),
                               [self frame].size.width, computedHeight)];
    [self setNeedsDisplay: YES];
  }
  
  float sizeChange = computedHeight - oldHeight;
  if (sizeChange != 0) {
    // We need to adjust the content area.
    NSView* view = [[[self window] windowController] getTabBrowser];
    [view setFrame: NSMakeRect([view frame].origin.x, [view frame].origin.y,
                               [view frame].size.width, [view frame].size.height - sizeChange)];
  }
}

-(BOOL)isFlipped
{
  return YES; // Use flipped coords, so we can layout out from top row to bottom row.
}

-(void)setFrame:(NSRect)aRect
{
  NSRect oldFrame = [self frame];
  [super setFrame:aRect];

  if (oldFrame.size.width == aRect.size.width || aRect.size.height == 0)
    return;

  int count = [mButtons count];
  if (count <= 2)
    return; // We have too few buttons to care.
  
  // Do some optimizations when we have only one row.
  if (aRect.size.height < 25) // We have only one row.
  {
    if (oldFrame.size.width < aRect.size.width)
      // We got bigger.  If we already only have one row, just bail.
      //	This will optimize for a common resizing case.
      return;
    else {
      // We got smaller.  Just go to the last button and see if it is outside
      // our bounds.
      CHBookmarksButton* button = [mButtons objectAtIndex:(count-1)];
      if ([button frame].origin.x + [button frame].size.width >
          [self bounds].size.width - 2) {
        // The button doesn't fit any more.  Reflow starting at this index.
        [self reflowButtonsStartingAtIndex:(count-1)];
      }
    }
  }
  else {
    // See if we got bigger or smaller.  We could gain or lose a row.
    [self reflowButtons];
  }
}

-(void)showBookmarksToolbar: (BOOL)aShow
{
  if (!aShow) {
    float height = [self bounds].size.height;
    [self setFrame: NSMakeRect([self frame].origin.x, [self frame].origin.y + height,
                               [self frame].size.width, 0)];
    // We need to adjust the content area.
    NSView* view = [[[self window] windowController] getTabBrowser];
    [view setFrame: NSMakeRect([view frame].origin.x, [view frame].origin.y,
                               [view frame].size.width, [view frame].size.height + height)];
  }
  //else
    // Reflowing the buttons will do the right thing.
  //  [self reflowButtons];
}

@end
