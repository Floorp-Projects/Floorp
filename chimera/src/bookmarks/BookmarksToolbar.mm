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
*   Kathy Brade <brade@netscape.com>
*   David Haas  <haasd@cae.wisc.edu>
*/

#import "BookmarksToolbar.h"

#import "CHBrowserService.h"
#import "BookmarksButton.h"
#import "BrowserWindowController.h"
#import "BookmarksDataSource.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIAtom.h"

@interface BookmarksToolbar(Private)

- (void)cleanup;
- (void)registerForShutdownNotification;
- (void)setButtonInsertionPoint:(id <NSDraggingInfo>)sender;
- (NSRect)insertionRectForButton:(NSView*)aButton position:(int)aPosition;
- (BookmarksButton*)makeNewButtonWithElement:(nsIDOMElement*)element;

@end

@implementation BookmarksToolbar

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) )
  {
    [self registerForShutdownNotification];
    mButtons = [[NSMutableArray alloc] init];
    mDragInsertionButton = nil;
    mDragInsertionPosition = BookmarksService::CHInsertNone;
    mDrawBorder = YES;
    [self registerForDraggedTypes:[NSArray arrayWithObjects:@"MozURLType", @"MozBookmarkType", NSStringPboardType, NSURLPboardType, nil]];
    mIsShowing = YES;
  }
  return self;
}

- (void)initializeToolbar
{
  [self buildButtonList];
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self cleanup];
  [super dealloc];
}

- (void)cleanup
{
  [mButtons release];
  mButtons = nil;
}

- (void)registerForShutdownNotification
{
  [[NSNotificationCenter defaultCenter] addObserver:  self
                                        selector:     @selector(shutdown:)
                                        name:         XPCOMShutDownNotificationName
                                        object:       nil];
}

-(void)shutdown: (NSNotification*)aNotification
{
  [self cleanup];
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
  if (newWindow)	// moving to window
    [[BookmarksManager sharedBookmarksManager] addBookmarksClient:self];  
  else						// leaving window
    [[BookmarksManager sharedBookmarksManagerDontAlloc] removeBookmarksClient:self];
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

-(void)buildButtonList
{
  // gToolbarRoot may be nil
  if (BookmarksService::gToolbarRoot)
  {
    // Build the buttons, and then lay them all out.
    nsCOMPtr<nsIDOMNode> child;
    BookmarksService::gToolbarRoot->GetFirstChild(getter_AddRefs(child));
    while (child) {
      nsCOMPtr<nsIDOMElement> childElt(do_QueryInterface(child));
      if (childElt) {
        BookmarksButton* button = [self makeNewButtonWithElement:childElt];
        [self addSubview: button];
        [mButtons addObject: button];
      }
  
      nsCOMPtr<nsIDOMNode> temp = child;
      temp->GetNextSibling(getter_AddRefs(child));
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
    BookmarksButton* button = [mButtons objectAtIndex: i];
    [button removeFromSuperview];
  }

  [mButtons removeAllObjects];
  [self setNeedsDisplay:YES];
}

-(void)addButton: (nsIDOMElement*)aElt atIndex: (int)aIndex
{
  BookmarksButton* button = [self makeNewButtonWithElement:aElt];
  [self addSubview: button];
  [mButtons insertObject: button atIndex: aIndex];
  if ([self isShown])
    [self reflowButtonsStartingAtIndex: aIndex];
}

-(void)editButton: (nsIDOMElement*)aElt
{
  int count = [mButtons count];
  for (int i = 0; i < count; i++) {
    BookmarksButton* button = [mButtons objectAtIndex: i];
    if ([button element] == aElt) {
      [button setElement: aElt];
      if (count > i && [self isShown])
        [self reflowButtonsStartingAtIndex: i];
      break;
    }
  }

  [self setNeedsDisplay:YES];
}

-(void)removeButton: (nsIDOMElement*)aElt
{
  int count = [mButtons count];
  for (int i = 0; i < count; i++) {
    BookmarksButton* button = [mButtons objectAtIndex: i];
    if ([button element] == aElt) {
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
#define kBookmarkButtonHorizPadding       2.0
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
    BookmarksButton* button = [mButtons objectAtIndex: i];
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
    BookmarksButton* button = [mButtons objectAtIndex:i];
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

-(NSMenu*)menuForEvent:(NSEvent*)aEvent
{
  // Make a copy of the context menu but change it to target us
  // FIXME - this will stop to work as soon as we add items to the context menu
  // that have different targets. In that case, we probably should add code to
  // BookmarksToolbar that manages the context menu for the CHBookmarksButtons.
  
  NSMenu* myMenu = [[[super menu] copy] autorelease];
  [[myMenu itemArray] makeObjectsPerformSelector:@selector(setTarget:) withObject: self];
  [myMenu update];
  return myMenu;
}

-(BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  // we'll only get here if the user clicked on empty space in the toolbar
  // in which case, Add Folder is the only thing they can do
  if (([aMenuItem action] == @selector(addFolder:)))
    return YES;

  return NO;
}

-(IBAction)addFolder:(id)aSender
{
  BookmarksManager* 		bmManager = [BookmarksManager sharedBookmarksManager];
  BookmarksDataSource* 	bmDataSource = [(BrowserWindowController*)[[self window] delegate] bookmarksDataSource];
  BookmarkItem* 				toolbarRootItem = [bmManager getWrapperForContent:[bmManager getToolbarRoot]];
  [bmDataSource addBookmark:aSender withParent:toolbarRootItem isFolder:YES URL:@"" title:@""]; 
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
  
  mDragInsertionButton = nsnull;
  mDragInsertionPosition = BookmarksService::CHInsertAfter;
  
  NSView* foundView = [self hitTest:superviewLoc];
  if (foundView && [foundView isMemberOfClass:[BookmarksButton class]])
  {
    BookmarksButton* targetButton = foundView;

    nsCOMPtr<nsIAtom> tagName;
    nsCOMPtr<nsIContent> contentNode = do_QueryInterface([targetButton element]);
    contentNode->GetTag(*getter_AddRefs(tagName));

    if (tagName == BookmarksService::gFolderAtom)
    {
      mDragInsertionButton = targetButton;
      mDragInsertionPosition = BookmarksService::CHInsertInto;
    }
    else if (targetButton != sourceButton)
    {
      NSPoint	localLocation = [[self superview] convertPoint:superviewLoc toView:foundView];
      
      mDragInsertionButton = targetButton;
      
      if (localLocation.x < NSWidth([targetButton bounds]) / 2.0)
        mDragInsertionPosition = BookmarksService::CHInsertBefore;
      else
        mDragInsertionPosition = BookmarksService::CHInsertAfter;
    }
  }
  else
  {
    // throw it in at the end
    mDragInsertionButton   = ([mButtons count] > 0) ? [mButtons objectAtIndex:[mButtons count] - 1] : 0;
    mDragInsertionPosition = BookmarksService::CHInsertAfter;
  }
}

- (BOOL)dropDestinationValid:(id <NSDraggingInfo>)sender
{
  NSPasteboard* draggingPasteboard = [sender draggingPasteboard];
  NSArray*      types  = [draggingPasteboard types];
  BOOL          isCopy = ([sender draggingSourceOperationMask] == NSDragOperationCopy);

  if (!BookmarksService::gToolbarRoot)
    return NO;

  if ([types containsObject: @"MozBookmarkType"]) 
  {
    NSArray *draggedIDs = [draggingPasteboard propertyListForType: @"MozBookmarkType"];
    
    nsCOMPtr<nsIContent> destinationContent;
    int index = 0;
    
    if (mDragInsertionPosition == BookmarksService::CHInsertInto)						// drop onto folder
    {
      nsCOMPtr<nsIDOMElement> parentElt = [mDragInsertionButton element];
      destinationContent = do_QueryInterface(parentElt);
      index = 0;
    }
    else if (mDragInsertionPosition == BookmarksService::CHInsertBefore ||
             mDragInsertionPosition == BookmarksService::CHInsertAfter)		// drop onto toolbar
    {
      nsCOMPtr<nsIDOMElement> toolbarRoot = BookmarksService::gToolbarRoot;
      destinationContent = do_QueryInterface(toolbarRoot);
      index = [mButtons indexOfObject: mDragInsertionButton];
      if (mDragInsertionPosition == BookmarksService::CHInsertAfter)
        ++index;
    }

    // we rely on IsBookmarkDropValid to filter out drops where the source
    // and destination are the same. 
    BookmarkItem* toolbarFolderItem = BookmarksService::GetWrapperFor(destinationContent);
    if (!BookmarksService::IsBookmarkDropValid(toolbarFolderItem, index, draggedIDs, isCopy)) {
      return NO;
    }
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
    mDragInsertionPosition = BookmarksService::CHInsertNone;
    return NSDragOperationNone;
  }
    
  return NSDragOperationGeneric;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  if (mDragInsertionPosition)
    [self setNeedsDisplayInRect:[self insertionRectForButton:mDragInsertionButton position:mDragInsertionPosition]];

  mDragInsertionButton = nil;
  mDragInsertionPosition = BookmarksService::CHInsertNone;
}

- (unsigned int)draggingUpdated:(id <NSDraggingInfo>)sender
{
  if (mDragInsertionPosition)
    [self setNeedsDisplayInRect:[self insertionRectForButton:mDragInsertionButton position:mDragInsertionPosition]];

  // we have to set the drag target before we can test for drop validation
  [self setButtonInsertionPoint:sender];
  
  if (![self dropDestinationValid:sender]) {
    mDragInsertionButton = nil;
    mDragInsertionPosition = BookmarksService::CHInsertNone;
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
  BookmarkItem* parent = nsnull;
  int index = 0;

  if (!BookmarksService::gToolbarRoot)
    return NO;
  
  if (mDragInsertionPosition == BookmarksService::CHInsertInto)							// drop onto folder
  {
    nsCOMPtr<nsIDOMElement> parentElt = [mDragInsertionButton element];
    nsCOMPtr<nsIContent> parentContent(do_QueryInterface(parentElt));
    parent = BookmarksService::GetWrapperFor(parentContent);
    index = 0;
  }
  else if (mDragInsertionPosition == BookmarksService::CHInsertBefore ||
           mDragInsertionPosition == BookmarksService::CHInsertAfter)				// drop onto toolbar
  {
    nsCOMPtr<nsIDOMElement> rootElt = BookmarksService::gToolbarRoot;
    nsCOMPtr<nsIContent> rootContent(do_QueryInterface(rootElt));
    parent = BookmarksService::GetWrapperFor(rootContent);
    index = [mButtons indexOfObject: mDragInsertionButton];
    if (index == NSNotFound)
      rootContent->ChildCount(index);
    else if (mDragInsertionPosition == BookmarksService::CHInsertAfter)
      index++;
  }
  else
  {
    mDragInsertionButton = nil;
    mDragInsertionPosition = BookmarksService::CHInsertNone;
    [self setNeedsDisplay:YES];
    return NO;
  }

  BOOL dropHandled = NO;
  BOOL isCopy = ([sender draggingSourceOperationMask] == NSDragOperationCopy);
    
  NSArray	*draggedTypes = [[sender draggingPasteboard] types];

  nsCOMPtr<nsIContent> beforeContent;
  [parent contentNode]->ChildAt(index, *getter_AddRefs(beforeContent));
  BookmarkItem* beforeItem = BookmarksService::GetWrapperFor(beforeContent);		// can handle nil content

  if ( [draggedTypes containsObject:@"MozBookmarkType"] )
  {
    NSArray *draggedItems = [[sender draggingPasteboard] propertyListForType: @"MozBookmarkType"];
    dropHandled = BookmarksService::PerformBookmarkDrop(parent, beforeItem, index, draggedItems, isCopy);
  }
  else if ( [draggedTypes containsObject:@"MozURLType"] )
  {
    NSDictionary* proxy = [[sender draggingPasteboard] propertyListForType: @"MozURLType"];
    dropHandled = BookmarksService::PerformProxyDrop(parent, beforeItem, proxy);
	}
  else if ( [draggedTypes containsObject:NSStringPboardType] )
  {
    NSString* draggedText = [[sender draggingPasteboard] stringForType:NSStringPboardType];
    // maybe fix URL drags to include the selected text as the title
    dropHandled = BookmarksService::PerformURLDrop(parent, beforeItem, draggedText, draggedText);
  }
  else if ([draggedTypes containsObject: NSURLPboardType])
  {
    NSURL*	urlData = [NSURL URLFromPasteboard:[sender draggingPasteboard]];
    dropHandled = BookmarksService::PerformURLDrop(parent, beforeItem, [urlData absoluteString], [urlData absoluteString]);
  }
  
  mDragInsertionButton = nil;
  mDragInsertionPosition = BookmarksService::CHInsertNone;
  [self setNeedsDisplay:YES];

  return dropHandled;
}

- (NSRect)insertionRectForButton:(NSView*)aButton position:(int) aPosition
{
  if (aPosition == BookmarksService::CHInsertInto) {
    return NSMakeRect([aButton frame].origin.x, [aButton frame].origin.y, [aButton frame].size.width, [aButton frame].size.height);
  } else if (aPosition == BookmarksService::CHInsertAfter) {
    return NSMakeRect([aButton frame].origin.x+[aButton frame].size.width, [aButton frame].origin.y, 2, [aButton frame].size.height);
  } else {// if (aPosition == BookmarksService::CHInsertBefore) {
    return NSMakeRect([aButton frame].origin.x - 2, [aButton frame].origin.y, 2, [aButton frame].size.height);
  }
}

- (BookmarksButton*)makeNewButtonWithElement:(nsIDOMElement*)element
{
	return [[[BookmarksButton alloc] initWithFrame: NSMakeRect(2, 1, 100, 17) element:element] autorelease];
}

#pragma mark -

- (void)bookmarkAdded:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
  //NSLog(@"Toolbar notified that %x added in %x, is root %d", bookmark, container, isRoot);
  nsCOMPtr<nsIContent>	toolbarRootContent = getter_AddRefs([[BookmarksManager sharedBookmarksManager] getToolbarRoot]);
  if (container == toolbarRootContent.get())
  {
    // We only care about changes that occur to the personal toolbar's immediate
    // children.
    PRInt32 index = -1;
    container->IndexOf(bookmark, index);
    nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(bookmark);
    [self addButton: elt atIndex:index];
  }
}

- (void)bookmarkRemoved:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
  nsCOMPtr<nsIContent>	toolbarRootContent = getter_AddRefs([[BookmarksManager sharedBookmarksManager] getToolbarRoot]);
  if (container == toolbarRootContent.get())
  {
    // We only care about changes that occur to the personal toolbar's immediate
    // children.
    nsCOMPtr<nsIDOMElement> childElt = do_QueryInterface(bookmark);
    [self removeButton: childElt];
  }
}

- (void)bookmarkChanged:(nsIContent*)bookmark
{
  nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(bookmark);
  [self editButton:elt];
}

- (void)specialFolder:(EBookmarksFolderType)folderType changedTo:(nsIContent*)newFolderContent
{
  if (folderType == eBookmarksFolderToolbar)
  {
    [self resetButtonList];
    [self buildButtonList];
  }
}

@end
