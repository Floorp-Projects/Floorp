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
 *   David Hyatt <hyatt@mozilla.org> (Original Author)
 *   Kathy Brade <brade@netscape.com>
 *   David Haas <haasd@cae.wisc.edu>
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

#import "BookmarkToolbar.h"

#import "CHBrowserService.h"
#import "BookmarkButton.h"
#import "BookmarkManager.h"
#import "BrowserWindowController.h"
#import "Bookmark.h"
#import "BookmarkFolder.h"
#import "NSPasteboard+Utils.h"


#define CHInsertNone 0
#define CHInsertInto  1
#define CHInsertBefore  2
#define CHInsertAfter  3

@interface BookmarkToolbar(Private)

- (void)setButtonInsertionPoint:(id <NSDraggingInfo>)sender;
- (NSRect)insertionRectForButton:(NSView*)aButton position:(int)aPosition;
- (BookmarkButton*)makeNewButtonWithItem:(BookmarkItem*)aItem;
- (void)managerStarted:(NSNotification*)inNotify;
- (BOOL)anchorFoundAtPoint:(NSPoint)testPoint forButton:(NSButton*)sourceButton;
- (BOOL)anchorFoundScanningFromPoint:(NSPoint)testPoint withStep:(int)step;
@end

@implementation BookmarkToolbar

static const int kBMBarScanningStep = 5;

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) )
  {
    mButtons = [[NSMutableArray alloc] init];
    mDragInsertionButton = nil;
    mDragInsertionPosition = CHInsertNone;
    mDrawBorder = YES;
    [self registerForDraggedTypes:[NSArray arrayWithObjects: kCaminoBookmarkListPBoardType, kWebURLsWithTitlesPboardType, NSStringPboardType, NSURLPboardType, nil]];
    mIsShowing = YES;
    // Generic notifications for Bookmark Client
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(bookmarkAdded:) name:BookmarkFolderAdditionNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkIconChangedNotification object:nil];
    
    // register for notifications of when the BM manager starts up. Since it does it on a separate thread,
    // it can be created after we are and if we don't update ourselves, the bar will be blank. This
    // happens most notably when the app is launched with a 'odoc' or 'GURL' appleEvent.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(managerStarted:)
        name:kBookmarkManagerStartedNotification object:nil];
  }
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mButtons release];
  [super dealloc];
}

//
// - managerStarted:
//
// Notification callback from the bookmark manager. Build the button list.
//
- (void)managerStarted:(NSNotification*)inNotify
{
  [self buildButtonList];
}

- (void)drawRect:(NSRect)aRect
{
  if (mDrawBorder)
  {
    [[NSColor controlShadowColor] set];
    float height = [self bounds].size.height;
    NSRectFill(NSMakeRect(aRect.origin.x, height - 1.0, aRect.size.width, height));
  }

  // The buttons will paint themselves. Just call our base class method.
  [super drawRect: aRect];

  // draw a separator at drag n drop insertion point if there is one
  if (mDragInsertionPosition)
  {
    [[[NSColor controlShadowColor] colorWithAlphaComponent:0.6] set];
    NSRectFill([self insertionRectForButton:mDragInsertionButton position:mDragInsertionPosition]);
  }
}

//
// -buildButtonList
//
// this only gets called on startup OR window creation.  on the off chance that
// we're starting due to an appleevent from another program, we might call it twice.
// make sure nothing bad happens if we do that.
// 
-(void)buildButtonList
{
  BookmarkFolder* toolbar = [[BookmarkManager sharedBookmarkManager] toolbarFolder];

  // check if we've built the toolbar already and bail if we have
  const unsigned long count = [toolbar count];
  if ([mButtons count] == count)
    return;

  for (unsigned int i = 0; i < count; i++) {
    BookmarkButton* button = [self makeNewButtonWithItem:[toolbar objectAtIndex:i]];
    if (button) {
      [self addSubview:button];
      [mButtons addObject:button];
    }
  }
  if ([self isShown])
    [self reflowButtons];
}

- (void)resetButtonList
{
  int count = [mButtons count];
  for (int i = 0; i < count; i++)
  {
    BookmarkButton* button = [mButtons objectAtIndex: i];
    [button removeFromSuperview];
  }
  [mButtons removeAllObjects];
  [self setNeedsDisplay:YES];
}

-(void)addButton:(BookmarkItem*)aItem atIndex:(int)aIndex
{
  BookmarkButton* button = [self makeNewButtonWithItem:aItem];
  if (!button)
    return;
  [self addSubview: button];
  [mButtons insertObject: button atIndex: aIndex];
  if ([self isShown])
    [self reflowButtonsStartingAtIndex: aIndex];
}

-(void)updateButton:(BookmarkItem*)aItem
{
  int count = [mButtons count];
  // XXX nasty linear search
  for (int i = 0; i < count; i++)
  {
    BookmarkButton* button = [mButtons objectAtIndex: i];
    if ([button bookmarkItem] == aItem)
    {
      BOOL needsReflow = NO;
      [button bookmarkChanged:&needsReflow];
      if (needsReflow && count > i && [self isShown])
        [self reflowButtonsStartingAtIndex: i];
      break;
    }
  }
  [self setNeedsDisplay:YES];
}

-(void)removeButton:(BookmarkItem*)aItem
{
  int count = [mButtons count];
  for (int i = 0; i < count; i++) {
    BookmarkButton* button = [mButtons objectAtIndex: i];
    if ([button bookmarkItem] == aItem) {
      [mButtons removeObjectAtIndex: i];
      [button removeFromSuperview];
      if (count > i && [self isShown])
        [self reflowButtonsStartingAtIndex: i];
      break;
    }
  }
  [self setNeedsDisplay:YES];
}

-(void)reflowButtons
{
  [self reflowButtonsStartingAtIndex: 0];
}

#define kBookmarkButtonHeight            16.0
#define kMinBookmarkButtonWidth          16.0
#define kMaxBookmarkButtonWidth         150.0
#define kBookmarkButtonHorizPadding       5.0
#define kBookmarkToolbarTopPadding        1.0
#define kBookmarkButtonVerticalPadding    1.0
#define kBookmarkToolbarBottomPadding     2.0

-(void)reflowButtonsStartingAtIndex: (int)aIndex
{
  if (![self isShown])
    return;

  // coordinates for this view are flipped, making it easier to lay out from top left
  // to bottom right.
  float oldHeight     = [self frame].size.height;
  int   count         = [mButtons count];
  float curRowYOrigin = kBookmarkToolbarTopPadding;
  float curX          = kBookmarkButtonHorizPadding;

  for (int i = 0; i < count; i++) {
    BookmarkButton* button = [mButtons objectAtIndex:i];
    BookmarkItem *item = [button bookmarkItem];
    NSRect buttonRect;
    if ([item isKindOfClass:[Bookmark class]] && [(Bookmark *)item isSeparator]){
      continue;
    }
    else if (i < aIndex) {
      buttonRect = [button frame];
      curRowYOrigin = NSMinY(buttonRect) - kBookmarkButtonVerticalPadding;
      curX = NSMaxX(buttonRect) + kBookmarkButtonHorizPadding;
    }
    else {
      [button sizeToFit];
      float width = [button frame].size.width;

      if (width > kMaxBookmarkButtonWidth)
        width = kMaxBookmarkButtonWidth;

      buttonRect = NSMakeRect(curX, curRowYOrigin + kBookmarkButtonVerticalPadding, width, kBookmarkButtonHeight);
      curX += NSWidth(buttonRect) + kBookmarkButtonHorizPadding;

      if (NSMaxX(buttonRect) > NSWidth([self bounds])) {
        // jump to the next line
        curX = kBookmarkButtonHorizPadding;
        curRowYOrigin += (kBookmarkButtonHeight + 2 * kBookmarkButtonVerticalPadding);
        buttonRect = NSMakeRect(curX, curRowYOrigin + kBookmarkButtonVerticalPadding, width, kBookmarkButtonHeight);
        curX += NSWidth(buttonRect) + kBookmarkButtonHorizPadding;
      }

      [button setFrame:buttonRect];
    }
  }

  float computedHeight = curRowYOrigin + (kBookmarkButtonHeight + 2 * kBookmarkButtonVerticalPadding + kBookmarkToolbarBottomPadding);

  // our size has changed, readjust our view's frame and the content area
  if (computedHeight != oldHeight) {
    [super setFrame: NSMakeRect([self frame].origin.x, [self frame].origin.y + (oldHeight - computedHeight),
                                [self frame].size.width, computedHeight)];

    // tell the superview to resize its subviews
    [[self superview] resizeSubviewsWithOldSize:[[self superview] frame].size];
  }

  [self setNeedsDisplay:YES];
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
  int reflowStart = 0;

  // find out where we need to start reflowing
  for (int i = 0; i < count; i ++)
  {
    BookmarkButton* button = [mButtons objectAtIndex:i];
    NSRect           buttonFrame = [button frame];

    if ((NSMaxX(buttonFrame) > NSMaxX(aRect)) ||       // we're overhanging the right
        (NSMaxY(buttonFrame) > kBookmarkButtonHeight)) // we're on the second row
    {
      reflowStart = i;
      break;
    }
  }

  [self reflowButtonsStartingAtIndex:reflowStart];
}

-(BOOL)isShown
{
  return mIsShowing;
}

-(void)setDrawBottomBorder:(BOOL)drawBorder
{
  if (mDrawBorder != drawBorder)
  {
    mDrawBorder = drawBorder;
    NSRect dirtyRect = [self bounds];
    dirtyRect.origin.y = dirtyRect.size.height - 1.0;
    dirtyRect.size.height = 1.0;
    [self setNeedsDisplayInRect:dirtyRect];
  }
}
//
// if the toolbar gets the message, we can only make a new folder.
// kinda dull.  but we'll do this on the fly.
//
-(NSMenu*)menuForEvent:(NSEvent*)aEvent
{
  NSMenu* myMenu = [[NSMenu alloc] initWithTitle:@"snookums"];
  NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Create New Folder...",@"Create New Folder...")
                                                    action:@selector(addFolder:)
                                             keyEquivalent:@""];
  [menuItem setTarget:self];
  [myMenu addItem:menuItem];
  [menuItem release];
  return [myMenu autorelease];
}

//
// context menu has only what we need
//
-(BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  return YES;
}

-(IBAction)addFolder:(id)aSender
{
  BookmarkFolder* toolbar = [[BookmarkManager sharedBookmarkManager] toolbarFolder];
  BookmarkFolder* aFolder = [toolbar addBookmarkFolder];
  [aFolder setTitle:NSLocalizedString(@"NewBookmarkFolder",@"New Folder")];
}

-(void)showBookmarksToolbar: (BOOL)aShow
{
  mIsShowing = aShow;

  if (!aShow)
  {
    [[self superview] setNeedsDisplayInRect:[self frame]];
    NSRect newFrame = [self frame];
    newFrame.origin.y += newFrame.size.height;
    newFrame.size.height = 0;
    [self setFrame: newFrame];

    // tell the superview to resize its subviews
    [[self superview] resizeSubviewsWithOldSize:[[self superview] frame].size];
  }
  else
  {
    [self reflowButtons];
    [self setNeedsDisplay:YES];
  }

}

- (void)setButtonInsertionPoint:(id <NSDraggingInfo>)sender
{
  NSPoint   dragLocation  = [sender draggingLocation];
  NSPoint   superviewLoc  = [[self superview] convertPoint:dragLocation fromView:nil]; // convert from window
  NSButton* sourceButton  = [sender draggingSource];

  mDragInsertionButton = nil;
  mDragInsertionPosition = CHInsertAfter;
  
  // check for a button at current location to use as an anchor
  if ([self anchorFoundAtPoint:superviewLoc forButton:sourceButton]) return;
  // otherwise see if there's a view around it to use as an anchor point
  if ([self anchorFoundScanningFromPoint:superviewLoc withStep:(kBMBarScanningStep * -1)]) return;
  if ([self anchorFoundScanningFromPoint:superviewLoc withStep:kBMBarScanningStep]) return;
  
  // if neither worked, it's probably the dead zone between lines.
  // treat that zone as the line above, and try everything again
  superviewLoc.y += kBMBarScanningStep;
  if ([self anchorFoundAtPoint:superviewLoc forButton:sourceButton]) return;
  if ([self anchorFoundScanningFromPoint:superviewLoc withStep:(kBMBarScanningStep * -1)]) return;
  if ([self anchorFoundScanningFromPoint:superviewLoc withStep:kBMBarScanningStep]) return;
  
  // if nothing works, just throw it in at the end
  mDragInsertionButton = ([mButtons count] > 0) ? [mButtons objectAtIndex:[mButtons count] - 1] : 0;
  mDragInsertionPosition = CHInsertAfter;
}

- (BOOL)anchorFoundAtPoint:(NSPoint)testPoint forButton:(NSButton*)sourceButton
{
  NSView* foundView = [self hitTest:testPoint];
  if (foundView && [foundView isMemberOfClass:[BookmarkButton class]]) {
    BookmarkButton* targetButton = foundView;
    
    // if over current position, leave mDragInsertButton unset but return success so nothing happens
    if (targetButton == sourceButton)
      return YES;
    
    mDragInsertionButton = targetButton;
    if ([[targetButton bookmarkItem] isKindOfClass:[BookmarkFolder class]])
      mDragInsertionPosition = CHInsertInto;
    else {
      NSPoint localLocation = [[self superview] convertPoint:testPoint toView:foundView];
      mDragInsertionPosition = (localLocation.x < NSWidth([targetButton bounds]) / 2.0) ? CHInsertBefore : CHInsertAfter;
    }
    return YES;
  }
  return NO;
}

- (BOOL)anchorFoundScanningFromPoint:(NSPoint)testPoint withStep:(int)step
{
  NSView* foundView;
  // scan horizontally until we find a bookmark or leave the view
  do {
    testPoint.x += step;
    foundView = [self hitTest:testPoint];
  } while (foundView && ![foundView isMemberOfClass:[BookmarkButton class]]);
  if (foundView && [foundView isMemberOfClass:[BookmarkButton class]]) {
    mDragInsertionButton = (BookmarkButton *)foundView;
    mDragInsertionPosition = (step < 0) ? CHInsertAfter : CHInsertBefore;
    return YES;
  }
  return NO;
}

- (BOOL)dropDestinationValid:(id <NSDraggingInfo>)sender
{
  NSPasteboard* draggingPasteboard = [sender draggingPasteboard];
  NSArray* types = [draggingPasteboard types];
  BookmarkManager *bmManager = [BookmarkManager sharedBookmarkManager];
  BookmarkFolder* toolbar = [bmManager toolbarFolder];

  if (!toolbar)
    return NO;

  if ([types containsObject: kCaminoBookmarkListPBoardType]) {
    NSArray *draggedItems = [BookmarkManager bookmarkItemsFromSerializableArray:[draggingPasteboard propertyListForType: kCaminoBookmarkListPBoardType]];
    BookmarkItem* destItem = nil;

    if (mDragInsertionButton == nil) {
      return NO;
    }
    else if (mDragInsertionPosition == CHInsertInto) {
      // drop onto folder
      destItem = [mDragInsertionButton bookmarkItem];
    }
    else if (mDragInsertionPosition == CHInsertBefore ||
             mDragInsertionPosition == CHInsertAfter) { // drop onto toolbar
      destItem = toolbar;
    }
    
    return [bmManager isDropValid:draggedItems toFolder:destItem];
  }

  return [draggingPasteboard containsURLData];
}

// NSDraggingDestination ///////////

- (unsigned int)draggingEntered:(id <NSDraggingInfo>)sender
{
  // we have to set the drag target before we can test for drop validation
  [self setButtonInsertionPoint:sender];

  if (![self dropDestinationValid:sender]) {
    mDragInsertionButton = nil;
    mDragInsertionPosition = CHInsertNone;
    return NSDragOperationNone;
  }

  NSDragOperation dragOpMask = [sender draggingSourceOperationMask];
  // see if the user forced copy by holding the appropriate modifier - the OS will AND the mask with the Copy flag
  if (dragOpMask == NSDragOperationCopy)
    return NSDragOperationCopy;
  if (dragOpMask & NSDragOperationGeneric)
    return NSDragOperationGeneric;
  
  return NSDragOperationNone;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  if (mDragInsertionPosition)
    [self setNeedsDisplayInRect:[self insertionRectForButton:mDragInsertionButton position:mDragInsertionPosition]];

  mDragInsertionButton = nil;
  mDragInsertionPosition = CHInsertNone;
}

- (unsigned int)draggingUpdated:(id <NSDraggingInfo>)sender
{
  if (mDragInsertionPosition)
    [self setNeedsDisplayInRect:[self insertionRectForButton:mDragInsertionButton position:mDragInsertionPosition]];

  // we have to set the drag target before we can test for drop validation
  [self setButtonInsertionPoint:sender];

  if (![self dropDestinationValid:sender]) {
    mDragInsertionButton = nil;
    mDragInsertionPosition = CHInsertNone;
    return NSDragOperationNone;
  }

  if (mDragInsertionPosition)
    [self setNeedsDisplayInRect:[self insertionRectForButton:mDragInsertionButton position:mDragInsertionPosition]];

  NSDragOperation dragOpMask = [sender draggingSourceOperationMask];
  // see if the user forced copy by holding the appropriate modifier - the OS will AND the mask with the Copy flag
  if (dragOpMask == NSDragOperationCopy)
    return NSDragOperationCopy;
  if (dragOpMask & NSDragOperationGeneric)
    return NSDragOperationGeneric;
  
  return NSDragOperationNone;  
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
  return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  unsigned index = 0;
  BookmarkFolder* toolbar = [[BookmarkManager sharedBookmarkManager] toolbarFolder];
  if (!toolbar)
    return NO;

  if (mDragInsertionPosition == CHInsertInto) { // drop onto folder
    toolbar = (BookmarkFolder *)[mDragInsertionButton bookmarkItem];
    index = [toolbar count];
  }
  else if (mDragInsertionPosition == CHInsertBefore ||
           mDragInsertionPosition == CHInsertAfter) { // drop onto toolbar
    index = [mButtons indexOfObjectIdenticalTo:mDragInsertionButton];
    if (index == NSNotFound)
      index = [toolbar count];
    else if (mDragInsertionPosition == CHInsertAfter)
      index++;
  }
  else {
    mDragInsertionButton = nil;
    mDragInsertionPosition = CHInsertNone;
    [self setNeedsDisplay:YES];
    return NO;
  }

  BOOL dropHandled = NO;
  BOOL isCopy = ([sender draggingSourceOperationMask] == NSDragOperationCopy);

  NSArray *draggedTypes = [[sender draggingPasteboard] types];

  if ([draggedTypes containsObject:kCaminoBookmarkListPBoardType]) {
    NSArray *draggedItems = [BookmarkManager bookmarkItemsFromSerializableArray:[[sender draggingPasteboard] propertyListForType: kCaminoBookmarkListPBoardType]];
    // added sequentially, so use reverse object enumerator to preserve order.
    NSEnumerator *enumerator = [draggedItems reverseObjectEnumerator];
    id aKid;
    while ((aKid = [enumerator nextObject])) {
      if (isCopy)
        [[aKid parent] copyChild:aKid toBookmarkFolder:toolbar atIndex:index];
      else
        [[aKid parent] moveChild:aKid toBookmarkFolder:toolbar atIndex:index];
    }
    dropHandled = YES;
  }
  else if ([[sender draggingPasteboard] containsURLData]) {
    NSArray* urls = nil;
    NSArray* titles = nil;
    [[sender draggingPasteboard] getURLs:&urls andTitles:&titles];
    
    // Add in reverse order to preserve order
    for ( int i = [urls count] - 1; i >= 0; --i )
      [toolbar addBookmark:[titles objectAtIndex:i] url:[urls objectAtIndex:i] inPosition:index isSeparator:NO];
    dropHandled = YES;
  }

  mDragInsertionButton = nil;
  mDragInsertionPosition = CHInsertNone;
  [self setNeedsDisplay:YES];
  return dropHandled;
}

- (NSRect)insertionRectForButton:(NSView*)aButton position:(int) aPosition
{
  if (aPosition == CHInsertInto) {
    return NSMakeRect([aButton frame].origin.x, [aButton frame].origin.y, [aButton frame].size.width, [aButton frame].size.height);
  } else if (aPosition == CHInsertAfter) {
    return NSMakeRect([aButton frame].origin.x+[aButton frame].size.width, [aButton frame].origin.y, 2, [aButton frame].size.height);
  } else {// if (aPosition == BookmarksService::CHInsertBefore) {
    return NSMakeRect([aButton frame].origin.x - 2, [aButton frame].origin.y, 2, [aButton frame].size.height);
  }
  }

- (BookmarkButton*)makeNewButtonWithItem:(BookmarkItem*)aItem
{
  return [[[BookmarkButton alloc] initWithFrame: NSMakeRect(2, 1, 100, 17) item:aItem] autorelease];
}

#pragma mark -

- (void)bookmarkAdded:(NSNotification *)aNote
{
  BookmarkFolder *anArray = [aNote object];
  if (![anArray isEqual:[[BookmarkManager sharedBookmarkManager] toolbarFolder]])
    return;
  NSDictionary *dict = [aNote userInfo];
  [self addButton:[dict objectForKey:BookmarkFolderChildKey] atIndex:[[dict objectForKey:BookmarkFolderChildIndexKey] unsignedIntValue]];
}

- (void)bookmarkRemoved:(NSNotification *)aNote
{
  BookmarkFolder *aFolder = [aNote object];
  if (![aFolder isEqual:[[BookmarkManager sharedBookmarkManager] toolbarFolder]])
    return;
  NSDictionary *dict = [aNote userInfo];
  [self removeButton:[dict objectForKey:BookmarkFolderChildKey]];
}

- (void)bookmarkChanged:(NSNotification *)aNote
{
  [self updateButton:[aNote object]];
}

@end
