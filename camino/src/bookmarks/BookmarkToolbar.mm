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
* Contributor(s):
*   David Hyatt <hyatt@apple.com> (Original Author)
*   Kathy Brade <brade@netscape.com>
*   David Haas <haasd@cae.wisc.edu>
*
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

#import "BookmarkToolbar.h"

#import "CHBrowserService.h"
#import "BookmarkButton.h"
#import "BookmarkManager.h"
#import "BrowserWindowController.h"
#import "Bookmark.h"
#import "BookmarkFolder.h"
#import "NSArray+Utils.h"


#define CHInsertNone 0
#define CHInsertInto  1
#define CHInsertBefore  2
#define CHInsertAfter  3

@interface BookmarkToolbar(Private)

- (void)setButtonInsertionPoint:(id <NSDraggingInfo>)sender;
- (NSRect)insertionRectForButton:(NSView*)aButton position:(int)aPosition;
- (BookmarkButton*)makeNewButtonWithItem:(BookmarkItem*)aItem;
- (void)managerStarted:(NSNotification*)inNotify;
@end

@implementation BookmarkToolbar

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) )
  {
    mButtons = [[NSMutableArray alloc] init];
    mDragInsertionButton = nil;
    mDragInsertionPosition = CHInsertNone;
    mDrawBorder = YES;
    [self registerForDraggedTypes:[NSArray arrayWithObjects:@"MozURLType", @"MozBookmarkType", NSStringPboardType, NSURLPboardType, nil]];
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
        name:[BookmarkManager managerStartedNotification] object:nil];
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

  for (unsigned int i = 0; i < count; i ++)
  {
    BookmarkButton* button = [self makeNewButtonWithItem:[toolbar objectAtIndex:i]];
    [self addSubview: button];
    [mButtons addObject: button];
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
  [self addSubview: button];
  [mButtons insertObject: button atIndex: aIndex];
  if ([self isShown])
    [self reflowButtonsStartingAtIndex: aIndex];
}

-(void)editButton:(BookmarkItem*)aItem
{
  int count = [mButtons count];
  for (int i = 0; i < count; i++)
  {
    BookmarkButton* button = [mButtons objectAtIndex: i];
    if ([button BookmarkItem] == aItem)
    {
      [button setBookmarkItem: aItem];
      if (count > i && [self isShown])
        [self reflowButtonsStartingAtIndex: i];
      break;
    }
  }
  [self setNeedsDisplay:YES];
}

-(void)removeButton:(BookmarkItem*)aItem
{
  int count = [mButtons count];
  for (int i = 0; i < count; i++)
  {
    BookmarkButton* button = [mButtons objectAtIndex: i];
    if ([button BookmarkItem] == aItem)
    {
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
#define kBookmarkButtonVerticalPadding    1.0
#define kBookmarkToolbarBottomPadding     1.0

-(void)reflowButtonsStartingAtIndex: (int)aIndex
{
  if (![self isShown])
    return;

  // coordinates for this view are flipped, making it easier to lay out from top left
  // to bottom right.
  float oldHeight = [self frame].size.height;
  int   count         = [mButtons count];
  float curRowYOrigin = 0.0;
  float curX          = kBookmarkButtonHorizPadding;

  for (int i = 0; i < count; i ++)
  {
    BookmarkButton* button = [mButtons objectAtIndex: i];
    NSRect           buttonRect;

    if (i < aIndex)
    {
      buttonRect = [button frame];
      curRowYOrigin = NSMinY(buttonRect) - kBookmarkButtonVerticalPadding;
      curX = NSMaxX(buttonRect) + kBookmarkButtonHorizPadding;
    }
    else
    {
      [button sizeToFit];
      float width = [button frame].size.width;

      if (width > kMaxBookmarkButtonWidth)
        width = kMaxBookmarkButtonWidth;

      buttonRect = NSMakeRect(curX, curRowYOrigin + kBookmarkButtonVerticalPadding, width, kBookmarkButtonHeight);
      curX += NSWidth(buttonRect) + kBookmarkButtonHorizPadding;

      if (NSMaxX(buttonRect) > NSWidth([self bounds]))
      {
        curRowYOrigin += (kBookmarkButtonHeight + 2 * kBookmarkButtonVerticalPadding);
        buttonRect = NSMakeRect(kBookmarkButtonHorizPadding, curRowYOrigin + kBookmarkButtonVerticalPadding, width, kBookmarkButtonHeight);
        curX = NSWidth(buttonRect);
      }

      [button setFrame: buttonRect];
    }
  }

  float computedHeight = curRowYOrigin + (kBookmarkButtonHeight + 2 * kBookmarkButtonVerticalPadding + kBookmarkToolbarBottomPadding);

  // our size has changed, readjust our view's frame and the content area
  if (computedHeight != oldHeight)
  {
    [super setFrame: NSMakeRect([self frame].origin.x, [self frame].origin.y + (oldHeight - computedHeight),
                                [self frame].size.width, computedHeight)];
    [self setNeedsDisplay:YES];

    // tell the superview to resize its subviews
    [[self superview] resizeSubviewsWithOldSize:[[self superview] frame].size];
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
  NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Create New Folder...",@"Create New Folder...") action:@selector(addFolder:) keyEquivalent:[NSString string]];
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

  NSView* foundView = [self hitTest:superviewLoc];
  if (foundView && [foundView isMemberOfClass:[BookmarkButton class]])
  {
    BookmarkButton* targetButton = foundView;
    BookmarkItem* targetItem 	= [targetButton BookmarkItem];

    if ([targetItem isKindOfClass:[BookmarkFolder class]])
    {
      mDragInsertionButton = targetButton;
      mDragInsertionPosition = CHInsertInto;
    }
    else if (targetButton != sourceButton)
    {
      NSPoint	localLocation = [[self superview] convertPoint:superviewLoc toView:foundView];
      mDragInsertionButton = targetButton;
      if (localLocation.x < NSWidth([targetButton bounds]) / 2.0)
        mDragInsertionPosition = CHInsertBefore;
      else
        mDragInsertionPosition = CHInsertAfter;
    }
  }
  else
  {
    // throw it in at the end
    mDragInsertionButton   = ([mButtons count] > 0) ? [mButtons objectAtIndex:[mButtons count] - 1] : 0;
    mDragInsertionPosition = CHInsertAfter;
  }
}

- (BOOL)dropDestinationValid:(id <NSDraggingInfo>)sender
{
  NSPasteboard* draggingPasteboard = [sender draggingPasteboard];
  NSArray* types = [draggingPasteboard types];
  BookmarkManager *bmManager = [BookmarkManager sharedBookmarkManager];
  BookmarkFolder* toolbar = [bmManager toolbarFolder];

  if (!toolbar)
    return NO;

  if ([types containsObject: @"MozBookmarkType"]) {
    NSArray *draggedItems = [NSArray pointerArrayFromDataArrayForMozBookmarkDrop:[draggingPasteboard propertyListForType: @"MozBookmarkType"]];
    BookmarkItem* destItem = nil;

    if (mDragInsertionPosition == CHInsertInto) {
      // drop onto folder
      destItem = [mDragInsertionButton BookmarkItem];
    }
    else if (mDragInsertionPosition == CHInsertBefore ||
             mDragInsertionPosition == CHInsertAfter) { // drop onto toolbar
      destItem = toolbar;
    }
    if (![bmManager isDropValid:draggedItems toFolder:destItem])
      return NO;
  }
  else if ([types containsObject:NSStringPboardType]) {
    // validate the string is a real url before allowing
    NSURL* testURL = [NSURL URLWithString:[draggingPasteboard stringForType:NSStringPboardType]];
    return (testURL != nil);
  }

  return YES;
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

  return NSDragOperationGeneric;
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

  return NSDragOperationGeneric;
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
    toolbar = (BookmarkFolder *)[mDragInsertionButton BookmarkItem];
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

  NSArray	*draggedTypes = [[sender draggingPasteboard] types];

  if ([draggedTypes containsObject:@"MozBookmarkType"]) {
    NSArray *draggedItems = [NSArray pointerArrayFromDataArrayForMozBookmarkDrop:[[sender draggingPasteboard] propertyListForType: @"MozBookmarkType"]];
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
  else if ([draggedTypes containsObject:@"MozURLType"]) {
    NSDictionary* proxy = [[sender draggingPasteboard] propertyListForType: @"MozURLType"];
    [toolbar addBookmark:[proxy objectForKey:@"title"] url:[proxy objectForKey:@"url"] inPosition:index isSeparator:NO];
    dropHandled = YES;
  }
  else if ([draggedTypes containsObject:NSStringPboardType]) {
    NSString* draggedText = [[sender draggingPasteboard] stringForType:NSStringPboardType];
    [toolbar addBookmark:draggedText url:draggedText inPosition:index isSeparator:NO];
    dropHandled = YES;
  }
  else if ([draggedTypes containsObject: NSURLPboardType]) {
    NSURL*	urlData = [NSURL URLFromPasteboard:[sender draggingPasteboard]];
    [toolbar addBookmark:[urlData absoluteString] url:[urlData absoluteString] inPosition:index isSeparator:NO];
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
  [self editButton:[aNote object]];
}

@end
