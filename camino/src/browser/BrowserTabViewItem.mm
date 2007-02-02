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

#import "NSString+Utils.h"
#import "NSBezierPath+Utils.h"
#import "NSPasteboard+Utils.h"

#import "BrowserTabViewItem.h"
#import "BrowserTabView.h"

#import "CHBrowserView.h"
#import "MainController.h"
#import "BrowserWindowController.h"
#import "TruncatingTextAndImageCell.h"
#import "TabButtonCell.h"
#import "RolloverImageButton.h"

NSString* const kTabWillChangeNotifcation = @"kTabWillChangeNotifcation";

// truncate menuitem title to the same width as the bookmarks menu
const int kMenuTruncationChars = 60;

@interface BrowserTabViewItem(Private)
- (void)setTag:(int)tag;
- (void)relocateTabContents:(NSRect)inRect;
- (BOOL)draggable;
@end

#pragma mark -

// a container view for the items in the tab view item. We use a subclass of
// NSView to handle drag and drop, and context menus
@interface BrowserTabItemContainerView : NSView
{
  BrowserTabViewItem*           mTabViewItem;
  TruncatingTextAndImageCell*   mLabelCell;
  TabButtonCell*                mTabButtonCell;
  
  BOOL           mIsDropTarget;
  BOOL           mSelectTabOnMouseUp;
}

- (TruncatingTextAndImageCell*)labelCell;
- (TabButtonCell*)tabButtonCell;

- (void)showDragDestinationIndicator;
- (void)hideDragDestinationIndicator;

@end

@implementation BrowserTabItemContainerView

- (id)initWithFrame:(NSRect)frameRect andTabItem:(BrowserTabViewItem*)tabViewItem
{
  if ( (self = [super initWithFrame:frameRect]) )
  {
    mTabViewItem = (BrowserTabViewItem*)tabViewItem;

    mLabelCell = [[TruncatingTextAndImageCell alloc] init];
    [mLabelCell setControlSize:NSSmallControlSize];		// doesn't work?
    [mLabelCell setImagePadding:0.0];
    [mLabelCell setImageSpace:2.0];
    mTabButtonCell = [[TabButtonCell alloc] initFromTabViewItem:mTabViewItem];
    
    [self registerForDraggedTypes:[NSArray arrayWithObjects:
        kCaminoBookmarkListPBoardType, kWebURLsWithTitlesPboardType,
        NSStringPboardType, NSFilenamesPboardType, NSURLPboardType, nil]];
  }
  return self;
}

- (void)dealloc
{
  [mLabelCell release];
  [mTabButtonCell release];
  // needs to be nil so that [super dealloc]'s call to setMenu doesn't cause us to call setMenu on an invalid object
  mTabButtonCell = nil;
  [super dealloc];
}

- (TruncatingTextAndImageCell*)labelCell
{
  return mLabelCell;
}

- (TabButtonCell *)tabButtonCell
{
  return mTabButtonCell;
}

// allow clicks in background windows to switch tabs
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
  return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
  return NO;
}

- (void)showDragDestinationIndicator
{
  if (!mIsDropTarget)
  {
    mIsDropTarget = YES;
    [self setNeedsDisplay:YES];
  }
}

- (void)hideDragDestinationIndicator
{
  if (mIsDropTarget)
  {
    mIsDropTarget = NO;
    [self setNeedsDisplay:YES];
  }
}

- (BOOL)shouldAcceptDragFrom:(id)sender
{
  if ((sender == self) || (sender == mTabViewItem))
    return NO;
  
  NSWindowController *windowController = [[[mTabViewItem view] window] windowController];
  if ([windowController isMemberOfClass:[BrowserWindowController class]])
  {
    if (sender == [(BrowserWindowController *)windowController proxyIconView])
      return NO;
  }
  
  return YES;
}


#pragma mark -

// NSDraggingDestination destination methods
- (unsigned int)draggingEntered:(id <NSDraggingInfo>)sender
{
  if (![self shouldAcceptDragFrom:[sender draggingSource]])
    return NSDragOperationNone;

  [self showDragDestinationIndicator];
  return NSDragOperationGeneric;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  [self hideDragDestinationIndicator];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
  return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  [self hideDragDestinationIndicator];

  if (![self shouldAcceptDragFrom:[sender draggingSource]])
    return NO;

  // let the tab view handle it
  return [[mTabViewItem tabView] performDragOperation:sender];
}

#pragma mark -

// NSDraggingSource methods

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
  if (isLocal)
    return (NSDragOperationGeneric | NSDragOperationCopy);

  return (NSDragOperationGeneric | NSDragOperationCopy | NSDragOperationLink);
}

// NSResponder methods
- (void)mouseDown:(NSEvent *)theEvent
{
  NSRect  iconRect   = [self convertRect: [mLabelCell imageFrame] fromView: nil];
  NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
  
  // this is a bit evil. Because the tab view's mouseDown captures the mouse, we'll
  // never get to mouseDragged if we allow the next responder (the tab view) to
  // handle the mouseDown. This prevents dragging from background tabs. So we break
  // things slightly by intercepting the mouseDown on the icon, so that our mouseDragged
  // gets called. If the users didn't drag, we select the tab in the mouse up.
  if (NSPointInRect(localPoint, iconRect) && [mTabViewItem draggable])
  {
    mSelectTabOnMouseUp = YES;
    return;		// we want dragging
  }
  
  mSelectTabOnMouseUp = NO;
  [[NSNotificationCenter defaultCenter] postNotificationName:kTabWillChangeNotifcation object:mTabViewItem];
  [[mTabViewItem tabView] selectTabViewItem:mTabViewItem];
}

- (void)mouseUp:(NSEvent *)theEvent
{
  if (mSelectTabOnMouseUp)
  {
    [[NSNotificationCenter defaultCenter] postNotificationName:kTabWillChangeNotifcation object:mTabViewItem];
    [[mTabViewItem tabView] selectTabViewItem:mTabViewItem];
    mSelectTabOnMouseUp = NO;
  }
  
  [[self nextResponder] mouseUp:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent
{
  NSRect  iconRect   = [self convertRect: [mLabelCell imageFrame] fromView: nil];//NSMakeRect(0, 0, 16, 16);
  NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

  if (!NSPointInRect(localPoint, iconRect) || ![mTabViewItem draggable])
  {
    [[self nextResponder] mouseDragged:theEvent];
    return;
  }

  // only initiate the drag if the original mousedown was in the right place... implied by mSelectTabOnMouseUp
  if (mSelectTabOnMouseUp)
  {
    mSelectTabOnMouseUp = NO;

    CHBrowserView* browserView = (CHBrowserView*)[mTabViewItem view];
    
    NSString     *url = [browserView getCurrentURI];
    NSString     *title = [mLabelCell stringValue];
    NSString     *cleanedTitle = [title stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];
    
    NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:self];
    [pboard setDataForURL:url title:cleanedTitle];
    
    NSPoint dragOrigin = [self frame].origin;
    dragOrigin.y += [self frame].size.height;
    
    [self dragImage: [MainController createImageForDragging:[mLabelCell image] title:title]
                 at:iconRect.origin offset:NSMakeSize(0.0, 0.0)
              event:theEvent pasteboard:pboard source:self slideBack:YES];
  }
}

- (void)setMenu:(NSMenu *)aMenu
{
  // set the tag of every menu item to the tab view item's tag,
  // so that the target of the menu commands know which one they apply to.
  for (int i = 0; i < [aMenu numberOfItems]; i ++)
    [[aMenu itemAtIndex:i] setTag:[mTabViewItem tag]];

  [super setMenu:aMenu];
  [mTabButtonCell setMenu:aMenu];
}

@end

#pragma mark -

@implementation BrowserTabViewItem

-(id)initWithIdentifier:(id)identifier withTabIcon:(NSImage *)tabIcon
{
  if ( (self = [super initWithIdentifier:identifier withTabIcon:tabIcon]) )
  {
    static int sTabItemTag = 1; // used to uniquely identify tabs for context menus
  
    [self setTag:sTabItemTag];
    sTabItemTag++;

    mTabContentsView = [[BrowserTabItemContainerView alloc]
                            initWithFrame:NSMakeRect(0, 0, 0, 0) andTabItem:self];

    // create progress wheel. keep a strong ref as view goes in and out of view hierarchy. We
    // cannot use |NSProgressIndicatorSpinningStyle| on 10.1, so don't bother even creating it
    // and let all the calls to it be no-ops elsewhere in this class (prevents clutter, imho).
#if USE_PROGRESS_SPINNER
// the progress spinner causes content to shear when scrolling because of
// redraw problems on jaguar and panther. Removing until we can fix it. (bug 203349)
    mProgressWheel = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)];
    [mProgressWheel setStyle:NSProgressIndicatorSpinningStyle];
    [mProgressWheel setUsesThreadedAnimation:YES];
    [mProgressWheel setDisplayedWhenStopped:NO];
    [mProgressWheel setAutoresizingMask:NSViewMaxXMargin];
#endif

    // create close button. keep a strong ref as view goes in and out of view hierarchy
    mCloseButton = [[RolloverImageButton alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)];
    [mCloseButton setTitle:NSLocalizedString(@"CloseTabButtonTitle", @"")];   // doesn't show, but used for accessibility
    [mCloseButton setImage:[BrowserTabViewItem closeIcon]];
    [mCloseButton setAlternateImage:[BrowserTabViewItem closeIconPressed]];
    [mCloseButton setHoverImage:[BrowserTabViewItem closeIconHover]];
    [mCloseButton setImagePosition:NSImageOnly];
    [mCloseButton setBezelStyle:NSShadowlessSquareBezelStyle];
    [mCloseButton setBordered:NO];
    [mCloseButton setButtonType:NSMomentaryChangeButton];
    [mCloseButton setTarget:self];
    [mCloseButton setAction:@selector(closeTab:)];
    [mCloseButton setAutoresizingMask:NSViewMinXMargin];
    
    // create a menu item, to be used when there are more tabs than screen real estate. keep a strong ref
    // since it will be added to and removed from the menu repeatedly
    mMenuItem = [[NSMenuItem alloc] initWithTitle:[self label] action:@selector(selectTab:) keyEquivalent:@""];
    [mMenuItem setTarget:self];

    [[self tabView] setAutoresizesSubviews:YES];

    mDraggable = NO;
  }
  return self;
}

-(id)initWithIdentifier:(id)identifier
{
  return [self initWithIdentifier:identifier withTabIcon:nil];
}

-(void)dealloc
{	
  // We can either be closing a single tab here, in which case we need to remove our view
  // from the superview, or the tab view may be closing, in which case it has already
  // removed all its subviews.
  [mTabContentsView removeFromSuperview];   // may be noop
  [mTabContentsView release];               // balance our init
  [mProgressWheel release];
  [mCloseButton release];
  [mMenuItem release];
  [super dealloc];
}

- (NSView*)tabItemContentsView
{
  return mTabContentsView;
}

- (void)setTag:(int)tag
{
  mTag = tag;
}

- (int)tag
{
  return mTag;
}

- (void)closeTab:(id)sender
{
	[[self view] windowClosed];
	[[self tabView] removeTabViewItem:self];
}

- (void)updateTabVisibility:(BOOL)nowVisible
{
  if (nowVisible)
  {
    if (![mTabContentsView superview])
      [[self tabView] addSubview:mTabContentsView];  
  }
  else
  {
    if ([mTabContentsView superview])
      [mTabContentsView removeFromSuperview];
  }
}

- (void)relocateTabContents:(NSRect)inRect
{
  [mTabContentsView setFrame:inRect];
  [mProgressWheel setFrame:NSMakeRect(0, 0, 16, 16)];
  [mCloseButton setFrame:NSMakeRect(inRect.size.width - 16, 0, 16, 16)];
}

- (BOOL)draggable
{
  return mDraggable;
}

-(void)drawLabel:(BOOL)shouldTruncateLabel inRect:(NSRect)tabRect
{
  [self relocateTabContents:tabRect];
  mLastDrawRect = tabRect;
}

- (void)setLabel:(NSString *)label
{
  NSAttributedString* labelString = [[[NSAttributedString alloc] initWithString:label attributes:mLabelAttributes] autorelease];
  [[mTabContentsView labelCell] setAttributedStringValue:labelString];
  [mMenuItem setTitle:[label stringByTruncatingTo:kMenuTruncationChars at:kTruncateAtMiddle]];
  [(BrowserTabView *)[self tabView] refreshTab:self];

  [super setLabel:label];
}

- (NSString*)label
{
  return [[mTabContentsView labelCell] stringValue];
}

- (TruncatingTextAndImageCell *)labelCell
{
  return [mTabContentsView labelCell];
}

- (TabButtonCell *)tabButtonCell
{
  return [mTabContentsView tabButtonCell];
}

-(void)setTabIcon:(NSImage *)newIcon
{
  [super setTabIcon:newIcon];
  [[mTabContentsView labelCell] setImage:mTabIcon];
  [mMenuItem setImage:mTabIcon];
  [(BrowserTabView *)[self tabView] refreshTab:self];
}

- (void)setTabIcon:(NSImage *)newIcon isDraggable:(BOOL)draggable
{
  [self setTabIcon:newIcon];
  mDraggable = draggable;
  [[mTabContentsView labelCell] setImageAlpha:(draggable ? 1.0 : 0.6)];  
}

- (void)willBeRemoved:(BOOL)remove
{
  if (remove) {
    [mCloseButton removeFromSuperview];
    [mProgressWheel removeFromSuperview];
  }
}

#pragma mark -

- (void)startLoadAnimation
{
#if USE_PROGRESS_SPINNER
  // add spinner to tab view and start animation
  [[mTabContentsView labelCell] addProgressIndicator:mProgressWheel];
  [(BrowserTabView *)[self tabView] refreshTab:self];
  [mProgressWheel startAnimation:self];
#else
  // allow the favicon to display if there's no spinner
  [[mTabContentsView labelCell] setImageVisible: YES];
  [mTabContentsView setNeedsDisplay:YES];
#endif
}

- (void)stopLoadAnimation
{
  // show the tab icon
  [[mTabContentsView labelCell] setImageVisible: YES];
  [mTabContentsView setNeedsDisplay:YES];
  
#if USE_PROGRESS_SPINNER
  // stop animation and remove spinner from tab view
  [mProgressWheel stopAnimation:self];
  [[mTabContentsView labelCell] removeProgressIndicator];
  [(BrowserTabView *)[self tabView] refreshTab:self];
#endif
}

- (RolloverImageButton *) closeButton
{
  return mCloseButton;
}

- (NSMenuItem *) menuItem
{
  return mMenuItem;
}

- (void) selectTab:(id)sender
{
  [[NSNotificationCenter defaultCenter] postNotificationName:kTabWillChangeNotifcation object:self];
  [[self tabView] selectTabViewItem:self];
}

// called by delegate when a tab will be deselected
- (void) willDeselect
{
  [mMenuItem setState:NSOffState];
}
// called by delegate when a tab will be selected
- (void) willSelect
{
  [mMenuItem setState:NSOnState];
}

#pragma mark -

+ (NSImage*)closeIcon
{
  static NSImage* sCloseIcon = nil;
  if ( !sCloseIcon )
    sCloseIcon = [[NSImage imageNamed:@"tab_close"] retain];
  return sCloseIcon;
}

+ (NSImage*)closeIconPressed
{
  static NSImage* sCloseIconPressed = nil;
  if ( !sCloseIconPressed )
    sCloseIconPressed = [[NSImage imageNamed:@"tab_close_pressed"] retain];
  return sCloseIconPressed;
}

+ (NSImage*)closeIconHover
{
  static NSImage* sCloseIconHover = nil;
  if ( !sCloseIconHover )
    sCloseIconHover = [[NSImage imageNamed:@"tab_close_hover"] retain];
  return sCloseIconHover;
}

@end
