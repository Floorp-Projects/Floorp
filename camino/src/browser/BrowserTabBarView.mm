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

#import "BrowserTabBarView.h"
#import "BrowserTabViewItem.h"
#import "TabButtonCell.h"

@interface BrowserTabBarView (PRIVATE)
-(void)layoutButtons;
-(void)loadImages;
-(void)drawTabBarBackgroundInRect:(NSRect)rect withActiveTabRect:(NSRect)tabRect;
-(TabButtonCell*)buttonAtPoint:(NSPoint)clickPoint;
-(void)registerTabButtonsForTracking;
-(void)unregisterTabButtonsForTracking;
-(void)initOverflowMenu;
-(NSRect)tabsRect;
@end

static NSImage *gBackgroundImage = nil;
static NSImage *gTabButtonDividerImage = nil;
static NSImage *gTabOverflowImage = nil;
static const float kTabBarDefaultHeight = 22.0;

@implementation BrowserTabBarView

static const int kTabBarMargin = 5;       // left/right margin for tab bar
static const float kMinTabWidth = 100.0;   // the smallest tabs that will be drawn
static const float kMaxTabWidth = 175.0;  // the widest tabs that will be drawn

static const int kTabDragThreshold = 3;   // distance a drag must go before we start dnd

static const float kOverflowButtonWidth = 16;
static const float kOverflowButtonHeight = 16;
static const int kOverflowButtonMargin = 1;

-(id)initWithFrame:(NSRect)frame 
{
  self = [super initWithFrame:frame];
  if (self) {
    mActiveTabButton = nil;
    mOverflowButton = nil;
    mOverflowTabs = NO;
    // this will not likely have any result here
    [self rebuildTabBar];
    [self registerForDraggedTypes:[NSArray arrayWithObjects: @"MozURLType", @"MozBookmarkType", NSStringPboardType,
                                                              NSFilenamesPboardType, NSURLPboardType, nil]];
  }
  return self;
}

-(void)awakeFromNib
{
  // this needs to be called again since our tabview should be non-nil now
  [self rebuildTabBar];
}

-(void)dealloc
{
  [mTrackingCells release];
  [mActiveTabButton release];
  [mOverflowButton release];
  [mOverflowMenu release];
  [super dealloc];
}

-(void)drawRect:(NSRect)rect 
{
  // this should only occur the first time any instance of this view is drawn
  if (!gBackgroundImage || !gTabButtonDividerImage)
    [self loadImages];
  
  // determine the frame of the active tab button and fill the rest of the bar in with the background
  NSRect activeTabButtonFrame = [mActiveTabButton frame];
  NSRect tabsRect = [self tabsRect];
  // if any of the active tab would be outside the drawable area, fill it in
  if (NSMaxX(activeTabButtonFrame) > NSMaxX(tabsRect)) activeTabButtonFrame.size.width = 0.0;
  [self drawTabBarBackgroundInRect:rect withActiveTabRect:activeTabButtonFrame];
  NSArray *tabItems = [mTabView tabViewItems];
  NSEnumerator *tabEnumerator = [tabItems objectEnumerator];
  BrowserTabViewItem *tab = [tabEnumerator nextObject];
  TabButtonCell *prevButton = nil;
  while (tab != nil) {
    TabButtonCell *tabButton = [tab tabButtonCell];
    BrowserTabViewItem *nextTab = [tabEnumerator nextObject];
    
    NSRect tabButtonFrame = [tabButton frame];
    if (NSIntersectsRect(tabButtonFrame,rect) && NSMaxX(tabButtonFrame) <= NSMaxX(tabsRect))
      [tabButton drawWithFrame:tabButtonFrame inView:self];
    // draw the first divider.
    if ((prevButton == nil) && ([tab tabState] != NSSelectedTab))
        [gTabButtonDividerImage compositeToPoint:NSMakePoint(tabButtonFrame.origin.x - [gTabButtonDividerImage size].width, tabButtonFrame.origin.y)
                                      operation:NSCompositeSourceOver];
    prevButton = tabButton;
    tab = nextTab;
  }
}

-(void)setFrame:(NSRect)frameRect
{
  [super setFrame:frameRect];
  // tab buttons probably need to be resized if the frame changes
  [self unregisterTabButtonsForTracking];
  [self layoutButtons];
  [self registerTabButtonsForTracking];
}

-(NSMenu*)menuForEvent:(NSEvent*)theEvent
{
  NSPoint clickPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  TabButtonCell *clickedTabButton = [self buttonAtPoint:clickPoint];
  // XXX should there be a menu if someone clicks in the tab bar where there is no tab?
  return (clickedTabButton) ? [clickedTabButton menu] : nil;
}

-(void)mouseDown:(NSEvent*)theEvent
{
  NSPoint clickPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  TabButtonCell *clickedTabButton = [self buttonAtPoint:clickPoint];

  mLastClickPoint = clickPoint;
  
  if (clickedTabButton && ![clickedTabButton willTrackMouse:theEvent inRect:[clickedTabButton frame] ofView:self])
    [[[clickedTabButton tabViewItem] tabItemContentsView] mouseDown:theEvent];
}

-(void)mouseUp:(NSEvent*)theEvent
{
  NSPoint clickPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  TabButtonCell * clickedTabButton = [self buttonAtPoint:clickPoint];
  if (clickedTabButton && ![clickedTabButton willTrackMouse:theEvent inRect:[clickedTabButton frame] ofView:self])
    [[[clickedTabButton tabViewItem] tabItemContentsView] mouseUp:theEvent];
}

-(void)mouseDragged:(NSEvent*)theEvent
{
  NSPoint clickPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  TabButtonCell *clickedTabButton = [self buttonAtPoint:clickPoint];
  if (clickedTabButton && 
      ![clickedTabButton willTrackMouse:theEvent inRect:[clickedTabButton frame] ofView:self])
      [[[clickedTabButton tabViewItem] tabItemContentsView] mouseDragged:theEvent];
  /* else if (!mDragStarted) {
    // XXX TODO: Handle dnd of tabs here
    if ((abs((int)(mLastClickPoint.x - clickPoint.x)) >= kTabDragThreshold) ||
        (abs((int)(mLastClickPoint.y - clickPoint.y)) >= kTabDragThreshold)) {
          NSLog(@"Here's where we'd handle the drag among friends rather than the drag manager");
    }*/
}

// returns the tab at the specified point
-(TabButtonCell*)buttonAtPoint:(NSPoint)clickPoint
{
  NSEnumerator *buttonEnumerator = nil;
  BrowserTabViewItem *tab = nil;
  NSArray *tabItems = [mTabView tabViewItems];
  NSEnumerator *tabEnumerator = [tabItems objectEnumerator];
  while (tab = [tabEnumerator nextObject]) {
    TabButtonCell *button = [tab tabButtonCell];
    if (NSPointInRect(clickPoint,[button frame]))
      return button;
  }
  return nil;
}

-(void) drawTabBarBackgroundInRect:(NSRect)rect withActiveTabRect:(NSRect)tabRect
{
  // draw tab bar background, omitting the selected Tab
  NSRect barFrame = [self bounds];
  NSRect fillRect;
  
  // first, fill to the left of the active tab
  fillRect = NSMakeRect(barFrame.origin.x, barFrame.origin.y, 
                        (tabRect.origin.x - barFrame.origin.x), barFrame.size.height);
  if (NSIntersectsRect(fillRect,rect)) {
    // make sure we're not drawing to the left or right of the actual rectangle we were asked to draw
    if (fillRect.origin.x < NSMinX(rect)) {
      fillRect.size.width -= NSMinX(rect) - fillRect.origin.x;
      fillRect.origin.x = NSMinX(rect);
    }
    if (NSMaxX(fillRect) > NSMaxX(rect))
      fillRect.size.width -= NSMaxX(fillRect) - NSMaxX(rect);
    [gBackgroundImage paintTiledInRect:fillRect];
  }
  // then fill to the right
  fillRect = NSMakeRect(NSMaxX(tabRect), barFrame.origin.y, 
                        (NSMaxX(barFrame) - NSMaxX(tabRect)), barFrame.size.height);
  if (NSIntersectsRect(fillRect,rect)) {
      // make sure we're not drawing to the left or right of the actual rectangle we were asked to draw
      if (fillRect.origin.x < NSMinX(rect)) {
        fillRect.size.width -= NSMinX(rect) - fillRect.origin.x;
        fillRect.origin.x = NSMinX(rect);
      }
      if (NSMaxX(fillRect) > NSMaxX(rect))
        fillRect.size.width -= NSMaxX(fillRect) - NSMaxX(rect);
        
      [gBackgroundImage paintTiledInRect:fillRect];
   }
}

-(void)loadImages
{
  if (!gBackgroundImage) 
    gBackgroundImage = [[NSImage imageNamed:@"tab_bar_bg"] retain];
  if (!gTabButtonDividerImage) 
    gTabButtonDividerImage = [[NSImage imageNamed:@"tab_button_divider"] retain];
  if (!gTabOverflowImage)
    gTabOverflowImage = [[NSImage imageNamed:@"tab_overflow"] retain];
}

// construct the tab bar based on the current state of mTabView;
// should be called when tabs are first shown.
-(void)rebuildTabBar
{
  [self unregisterTabButtonsForTracking];
  [mActiveTabButton release];
  mActiveTabButton = [[(BrowserTabViewItem *)[mTabView selectedTabViewItem] tabButtonCell] retain];
  [self layoutButtons];
  [self registerTabButtonsForTracking];
}

- (void)windowClosed
{
  // remove all tracking rects because this view is implicitly retained when they're registered
  [self unregisterTabButtonsForTracking];
}

// allows tab button cells to react to mouse events
-(void)registerTabButtonsForTracking
{
  if ([self window]) {
    NSArray * tabItems = [mTabView tabViewItems];
    if(mTrackingCells) [self unregisterTabButtonsForTracking];
    mTrackingCells = [NSMutableArray arrayWithCapacity:[tabItems count]];
    [mTrackingCells retain];
    NSEnumerator *tabEnumerator = [tabItems objectEnumerator];
    
    NSPoint local = [[self window] convertScreenToBase:[NSEvent mouseLocation]];
    local = [self convertPoint:local fromView:nil];
    
    BrowserTabViewItem *tab = nil;
    while (tab = [tabEnumerator nextObject]) {
      TabButtonCell * tabButton = [tab tabButtonCell];
      if (tabButton) {
        [mTrackingCells addObject:tabButton];
        NSRect trackingRect = [tabButton frame];
        // only track tabs that are onscreen
        if (NSMaxX(trackingRect) <= NSMaxX([self tabsRect]))
          [tabButton addTrackingRectInView: self withFrame:trackingRect cursorLocation:local];
      }
    }
  }
}

// causes tab buttons to stop reacting to mouse events
-(void)unregisterTabButtonsForTracking
{
  if (mTrackingCells) {
    NSEnumerator *tabEnumerator = [mTrackingCells objectEnumerator];
    TabButtonCell *tab = nil;
    while (tab = (TabButtonCell *)[tabEnumerator nextObject])
      [tab removeTrackingRectFromView: self];
    [mTrackingCells release];
    mTrackingCells = nil;
  }
}
  
// returns the height the tab bar should be if drawn
-(float)tabBarHeight
{
  // this will be constant for now
  return kTabBarDefaultHeight;
}

-(BrowserTabViewItem *)tabViewItemAtPoint:(NSPoint)location
{
  TabButtonCell *button = [self buttonAtPoint:[self convertPoint:location fromView:nil]];
  return (button) ? [button tabViewItem] : nil;
}

// sets the tab buttons to the largest kMinTabWidth <= size <= kMaxTabWidth where they all fit
// and calculates the frame for each.
-(void)layoutButtons
{
  const int numTabs = [mTabView numberOfTabViewItems];
  float tabWidth = kMaxTabWidth;

  // calculate the largest tabs that would fit... [self tabsRect] may not be correct until mOverflowTabs is set here.
  float maxWidth = floor((NSWidth([self bounds]) - (2*kTabBarMargin))/numTabs);
  // if tabs will overflow, leave space for the button
  if (maxWidth < kMinTabWidth) {
    mOverflowTabs = YES;
    NSRect tabsRect = [self tabsRect];
    for (int i = 1; i < numTabs; i++) {
      maxWidth = floor(NSWidth(tabsRect)/(numTabs - i));
      if (maxWidth >= kMinTabWidth) break;
    }
    // because the specific tabs which overflow may change, empty the menu and rebuild it as tabs are laid out
    [self initOverflowMenu];
  } else {
    mOverflowTabs = NO;
  }
  // if our tabs are currently larger than that, shrink them to the larger of kMinTabWidth or maxWidth
  if (tabWidth > maxWidth)
    tabWidth = (maxWidth > kMinTabWidth ? maxWidth : kMinTabWidth);
  // resize and position the tab buttons
  int xCoord = kTabBarMargin;
  NSArray *tabItems = [mTabView tabViewItems];
  NSEnumerator *tabEnumerator = [tabItems objectEnumerator];
  BrowserTabViewItem *tab = nil;
  TabButtonCell *prevTabButton = nil;
  while ((tab = [tabEnumerator nextObject])) {
    TabButtonCell *tabButtonCell = [tab tabButtonCell];
    NSSize buttonSize = [tabButtonCell size];
    buttonSize.width = tabWidth;
    buttonSize.height = kTabBarDefaultHeight;
    NSPoint buttonOrigin = NSMakePoint(xCoord,0);
    [tabButtonCell setFrame:NSMakeRect(buttonOrigin.x,buttonOrigin.y,buttonSize.width,buttonSize.height)];
	  // tell the button whether it needs to draw the right side dividing line
	  if ([tab tabState] == NSSelectedTab) {
		  [tabButtonCell setDrawDivider:NO];
		  [prevTabButton setDrawDivider:NO];
	  } else {
		  [tabButtonCell setDrawDivider:YES];
	  }
    // If the tab ran off the edge, suppress its close button, make sure the divider is drawn, and add it to the menu
	  if (buttonOrigin.x + buttonSize.width > NSMaxX([self tabsRect])) {
		  [tabButtonCell hideCloseButton];
      // push the tab off the edge of the view to keep it from grabbing clicks if there is an area
      // between the overflow menu and the last tab which is within tabsRect due to rounding
      [tabButtonCell setFrame:NSMakeRect(NSMaxX([self bounds]),buttonOrigin.y,buttonSize.width,buttonSize.height)];
      // if the tab prior to the overflow is not selected, it must draw a divider
      if([[prevTabButton tabViewItem] tabState] != NSSelectedTab) [prevTabButton setDrawDivider:YES];
      [mOverflowMenu addItem:[tab menuItem]];
    }
	  prevTabButton = tabButtonCell;
	  xCoord += (int)tabWidth;
  }
  // if tabs overflowed, position and display the overflow button
  if (mOverflowTabs) {
    [mOverflowButton setFrame:NSMakeRect(NSMaxX([self tabsRect]) + kOverflowButtonMargin,
                                         ([self tabBarHeight] - kOverflowButtonHeight)/2,kOverflowButtonWidth,kOverflowButtonHeight)];
    [self addSubview:mOverflowButton];
  } else {
    [mOverflowButton removeFromSuperview];
  }
  [self setNeedsDisplay:YES];
}

-(void)initOverflowMenu
{
  if (!mOverflowButton) {
    // if it hasn't been created yet, create an NSPopUpButton and retain a strong reference
    mOverflowButton = [[NSButton alloc] initWithFrame:NSMakeRect(0,0,kOverflowButtonWidth,kOverflowButtonHeight)];
    if (!gTabOverflowImage) [self loadImages];
    [mOverflowButton setImage:gTabOverflowImage];
    [mOverflowButton setImagePosition:NSImageOnly];
    [mOverflowButton setBezelStyle:NSShadowlessSquareBezelStyle];
    [mOverflowButton setBordered:NO];
    [[mOverflowButton cell] setHighlightsBy:NSNoCellMask];
    [mOverflowButton setTarget:self];
    [mOverflowButton setAction:@selector(overflowMenu:)];
    [(NSButtonCell *)[mOverflowButton cell]sendActionOn:NSLeftMouseDownMask];
  }
  if (!mOverflowMenu) {
    // create an empty NSMenu for later use and retain a strong reference
    mOverflowMenu = [[NSMenu alloc] init];
    [mOverflowMenu addItemWithTitle:@"" action:NULL keyEquivalent:@""];
  }
  // remove any items on the menu other than the dummy item
  for (int i = [mOverflowMenu numberOfItems]; i > 1; i--)
    [mOverflowMenu removeItemAtIndex:i-1];
}

- (IBAction)overflowMenu:(id)sender
{
  NSPopUpButtonCell *popupCell = [[[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES] autorelease];
  [popupCell setMenu:mOverflowMenu];
  [popupCell trackMouse:[NSApp currentEvent] inRect:[sender bounds] ofView:sender untilMouseUp:YES];
}

// returns an NSRect of the area where tab widgets may be drawn
-(NSRect)tabsRect
{
  NSRect rect = [self bounds];
  rect.origin.x += kTabBarMargin;
  rect.size.width -= 2 * kTabBarMargin + (mOverflowTabs ? kOverflowButtonWidth : 0.0);
  return rect;
}
    

#pragma mark -

// NSDraggingDestination destination methods
-(unsigned int)draggingEntered:(id <NSDraggingInfo>)sender
{
  TabButtonCell * button = [self buttonAtPoint:[self convertPoint:[sender draggingLocation] fromView:nil]];
  if (!button)
    return NSDragOperationGeneric;
  mDragDest = [[button tabViewItem] tabItemContentsView];
  mDragDestButton = button;
  unsigned int rv = [ mDragDest draggingEntered:sender];
  if (NSDragOperationNone != rv) {
    [button setDragTarget:YES];
    [self setNeedsDisplay:YES];
  }
  [self unregisterTabButtonsForTracking];
  return rv;
}

-(unsigned int)draggingUpdated:(id <NSDraggingInfo>)sender
{
  TabButtonCell * button = [self buttonAtPoint:[self convertPoint:[sender draggingLocation] fromView:nil]];
  if (!button) {
    if (mDragDestButton) {
      [mDragDestButton setDragTarget:NO];
      [self setNeedsDisplay:YES];
      mDragDestButton = nil;
    }
    return NSDragOperationGeneric;
  }
  mDragDest = [[button tabViewItem] tabItemContentsView];
  if (mDragDestButton != button) {
    [mDragDestButton setDragTarget:NO];
    [self setNeedsDisplay:YES];
    mDragDestButton = button;
  }
  unsigned int rv = [mDragDest draggingUpdated:sender];
  if (NSDragOperationNone != rv) {
    [button setDragTarget:YES];
    [self setNeedsDisplay:YES];
  }
  return rv;
}

-(void)draggingExited:(id <NSDraggingInfo>)sender
{
  if (mDragDestButton)
    [mDragDestButton setDragTarget:NO];
  [self setNeedsDisplay:YES];
  [self registerTabButtonsForTracking];
}

-(BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
  BOOL rv = [mDragDest prepareForDragOperation: sender];
  if (!rv) {
    if (mDragDestButton)
      [mDragDestButton setDragTarget:NO];
    [self setNeedsDisplay:YES];
  }
  return rv;
}

-(BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  TabButtonCell * button = [self buttonAtPoint:[self convertPoint:[sender draggingLocation] fromView:nil]];
  if (!button) {
    if (mDragDestButton)
      [mDragDestButton setDragTarget:NO];
    return [mTabView performDragOperation:sender];
  }
  [mDragDestButton setDragTarget:NO];
  [button setDragTarget:NO];
  [self setNeedsDisplay:YES];
  mDragDestButton = button;  
  mDragDest = [[button tabViewItem] tabItemContentsView];
  [self registerTabButtonsForTracking];
  return [mDragDest performDragOperation:sender];
}

@end

#pragma mark -
@implementation NSImage (BrowserTabBarViewAdditions)
//an addition to NSImage used by both BrowserTabBarView and TabButtonCell

//tiles an image in the specified rect... even though it should work vertically, these images
//will only look right tiled horizontally.
-(void)paintTiledInRect:(NSRect)rect
{
  NSSize imageSize = [self size];
  NSRect currTile = NSMakeRect(rect.origin.x, rect.origin.y, imageSize.width, imageSize.height);
  
  while (currTile.origin.y < NSMaxY(rect)) {
    while (currTile.origin.x < NSMaxX(rect)) {
      // clip the tile as needed
      if (NSMaxX(currTile) > NSMaxX(rect)) {
        currTile.size.width -= NSMaxX(currTile) - NSMaxX(rect);
      }
      if (NSMaxY(currTile) > NSMaxY(rect)) {
        currTile.size.height -= NSMaxY(currTile) - NSMaxY(rect);
      }
      NSRect imageRect = NSMakeRect(0, 0, currTile.size.width, currTile.size.height);
      [self compositeToPoint:currTile.origin fromRect:imageRect operation:NSCompositeSourceOver];
      currTile.origin.x += currTile.size.width;
    }
    currTile.origin.y += currTile.size.height;
  }
}

@end
