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
 *  Simon Fraser <sfraser@netscape.com>
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

#import "NSString+Utils.h"
#import "NSBezierPath+Utils.h"
#import "NSPasteboard+Utils.h"

#import "BrowserTabViewItem.h"

#import "CHBrowserView.h"
#import "MainController.h"
#import "BrowserWindowController.h"

// we cannot use the spinner before 10.2, so don't allow it. This is the
// version of appkit in 10.2 (taken from the 10.3 SDK headers which we cannot use).
const double kJaguarAppKitVersion = 663;

@interface BrowserTabViewItem(Private)
- (void)setTag:(int)tag;
- (void)relocateTabContents:(NSRect)inRect;
- (BOOL)draggable;
@end

#pragma mark -

// XXX move this to a new file
@interface NSTruncatingTextAndImageCell : NSCell
{
  NSImage         *mImage;
  NSMutableString *mTruncLabelString;
  int             mLabelStringWidth;      // -1 if not known
  float           mImagePadding;
  float           mImageSpace;
  float           mImageAlpha;
  float           mRightGutter;           // leave space for an icon on the right
  BOOL            mImageIsVisible;
}

- (id)initTextCell:(NSString*)aString;
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;

- (void)setImagePadding:(float)padding;
- (void)setImageSpace:(float)space;
- (void)setImageAlpha:(float)alpha;
- (void)setRightGutter:(float)rightPadding;
- (void)setImageVisible:(BOOL)visible;

- (void)setImage:(NSImage *)anImage;
- (NSImage *)image;

@end

@implementation NSTruncatingTextAndImageCell

- (id)initTextCell:(NSString*)aString
{
  if ((self = [super initTextCell:aString]))
  {
    mLabelStringWidth = -1;
    mImagePadding = 0;
    mImageSpace = 2;
    mRightGutter = 0.0;
    mImageIsVisible = NO;
  }
  return self;
}

- (void)dealloc
{
  [mImage release];
  [mTruncLabelString release];
  [super dealloc];
}

- copyWithZone:(NSZone *)zone
{
    NSTruncatingTextAndImageCell *cell = (NSTruncatingTextAndImageCell *)[super copyWithZone:zone];
    cell->mImage = [mImage retain];
    cell->mTruncLabelString = nil;
    cell->mLabelStringWidth = -1;
    cell->mRightGutter = mRightGutter;
    cell->mImageIsVisible = mImageIsVisible;
    return cell;
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
  NSRect textRect = cellFrame;
  NSRect imageRect;

  // we always reserve space for the image, even if there isn't one
  // assume the image rect is always square
  float imageWidth = NSHeight(cellFrame) - 2 * mImagePadding;
  NSDivideRect(cellFrame, &imageRect, &textRect, imageWidth, NSMinXEdge);
  
  if (mImage && mImageIsVisible)
  {
    NSRect imageSrcRect = NSZeroRect;
    imageSrcRect.size = [mImage size];    
    [mImage drawInRect:NSInsetRect(imageRect, mImagePadding, mImagePadding)
          fromRect:imageSrcRect operation:NSCompositeSourceOver fraction:mImageAlpha];
  }
  
  // remove image space
  NSDivideRect(textRect, &imageRect, &textRect, mImageSpace, NSMinXEdge);

  int cellWidth = (int)NSWidth(textRect) - (int)mRightGutter;
  NSDictionary *cellAttributes = [[self attributedStringValue] attributesAtIndex:0 effectiveRange:nil];

  if (mLabelStringWidth != cellWidth || !mTruncLabelString)
  {
    [mTruncLabelString release];
    mTruncLabelString = [[NSMutableString alloc] initWithString:[self stringValue]];
    [mTruncLabelString truncateToWidth:cellWidth at:kTruncateAtEnd withAttributes:cellAttributes];
    mLabelStringWidth = cellWidth;
  }
  
  [mTruncLabelString drawInRect:textRect withAttributes:cellAttributes];
}

- (void)setStringValue:(NSString *)aString
{
  if (![aString isEqualToString:[self stringValue]])
  {
    [mTruncLabelString release];
    mTruncLabelString = nil;
  }
  [super setStringValue:aString];
}

- (void)setAttributedStringValue:(NSAttributedString *)attribStr
{
  if (![attribStr isEqualToAttributedString:[self attributedStringValue]])
  {
    [mTruncLabelString release];
    mTruncLabelString = nil;
  }
  [super setAttributedStringValue:attribStr];
}

- (void)setImage:(NSImage *)anImage 
{
  if (anImage != mImage)
  {
    [mImage release];
    mImage = [anImage retain];
  }
}

- (void)setImageVisible:(BOOL)visible
{
  mImageIsVisible = visible;
}

- (NSImage *)image
{
  return mImage;
}

- (void)setImagePadding:(float)padding
{
  mImagePadding = padding;
}

- (void)setImageSpace:(float)space
{
  mImageSpace = space;
}

- (void)setImageAlpha:(float)alpha
{
  mImageAlpha = alpha;
}

- (void)setRightGutter:(float)rightPadding
{
  mRightGutter = rightPadding;
}

@end

#pragma mark -

// a container view for the items in the tab view item. We use a subclass of
// NSView to handle drag and drop, and context menus
@interface BrowserTabItemContainerView : NSView
{
  BrowserTabViewItem*           mTabViewItem;
  NSTruncatingTextAndImageCell* mLabelCell;
  
  BOOL           mIsDropTarget;
  BOOL           mSelectTabOnMouseUp;
}

- (NSTruncatingTextAndImageCell*)labelCell;

- (void)showDragDestinationIndicator;
- (void)hideDragDestinationIndicator;

@end

@implementation BrowserTabItemContainerView

- (id)initWithFrame:(NSRect)frameRect andTabItem:(NSTabViewItem*)tabViewItem
{
  if ( (self = [super initWithFrame:frameRect]) )
  {
    const float kCloseButtonWidth = 16.0;       // width of spinner/close button to right of text
    
    mTabViewItem = tabViewItem;

    mLabelCell = [[NSTruncatingTextAndImageCell alloc] init];
    [mLabelCell setControlSize:NSSmallControlSize];		// doesn't work?
    [mLabelCell setImagePadding:0.0];
    [mLabelCell setImageSpace:2.0];
    [mLabelCell setRightGutter:kCloseButtonWidth];
    
    [self registerForDraggedTypes:[NSArray arrayWithObjects:
        @"MozURLType", @"MozBookmarkType", NSStringPboardType, NSFilenamesPboardType, NSURLPboardType, nil]];
  }
  return self;
}

- (void)dealloc
{
  [mLabelCell release];
  [super dealloc];
}

- (NSTruncatingTextAndImageCell*)labelCell
{
  return mLabelCell;
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

- (void)drawRect:(NSRect)aRect
{
  [mLabelCell drawWithFrame:[self bounds] inView:self];
  
  if (mIsDropTarget)
  {
    NSRect	hilightRect = NSOffsetRect(NSInsetRect([self bounds], 1.0, 0), -1.0, 0);
    NSBezierPath* dropTargetOutline = [NSBezierPath bezierPathWithRoundCorneredRect:hilightRect cornerRadius:4.0];
    [[NSColor colorWithCalibratedWhite:0.0 alpha:0.15] set];
    [dropTargetOutline fill];
  }
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

- (unsigned int)draggingUpdated:(id <NSDraggingInfo>)sender
{
  if (![self shouldAcceptDragFrom:[sender draggingSource]]) {
    [self hideDragDestinationIndicator];
    return NSDragOperationNone;
  }

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

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)flag
{
	return NSDragOperationGeneric;
}

// NSResponder methods
- (void)mouseDown:(NSEvent *)theEvent
{
  NSRect  iconRect   = NSMakeRect(0, 0, 16, 16);
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
  [[self nextResponder] mouseDown:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
  if (mSelectTabOnMouseUp)
  {
    [[mTabViewItem tabView] selectTabViewItem:mTabViewItem];
    mSelectTabOnMouseUp = NO;
  }
  
  [[self nextResponder] mouseUp:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent
{
  NSRect  iconRect   = NSMakeRect(0, 0, 16, 16);
  NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

  if (!NSPointInRect(localPoint, iconRect) || ![mTabViewItem draggable])
  {
    [[self nextResponder] mouseDragged:theEvent];
    return;
  }
  
  mSelectTabOnMouseUp = NO;

  CHBrowserView* browserView = (CHBrowserView*)[mTabViewItem view];
  
  NSString     *url = [browserView getCurrentURLSpec];
  NSString     *title = [mLabelCell stringValue];
  NSString     *cleanedTitle = [title stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];

  NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:self];
  [pboard setDataForURL:url title:cleanedTitle];
  
  NSPoint dragOrigin = [self frame].origin;
  dragOrigin.y += [self frame].size.height;
  
  [self dragImage: [MainController createImageForDragging:[mLabelCell image] title:title]
                    at:NSMakePoint(0, 0) offset:NSMakeSize(0, 0)
                    event:theEvent pasteboard:pboard source:self slideBack:YES];
}

- (void)setMenu:(NSMenu *)aMenu
{
  // set the tag of every menu item to the tab view item's tag,
  // so that the target of the menu commands know which one they apply to.
  for (int i = 0; i < [aMenu numberOfItems]; i ++)
    [[aMenu itemAtIndex:i] setTag:[mTabViewItem tag]];

  [super setMenu:aMenu];
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
                            initWithFrame:NSMakeRect(0, 0, 100, 16) andTabItem:self];

    // create progress wheel. keep a strong ref as view goes in and out of view hierarchy. We
    // cannot use |NSProgressIndicatorSpinningStyle| on 10.1, so don't bother even creating it
    // and let all the calls to it be no-ops elsewhere in this class (prevents clutter, imho).
    if (NSAppKitVersionNumber >= kJaguarAppKitVersion) {
      mProgressWheel = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)];
      [mProgressWheel setStyle:NSProgressIndicatorSpinningStyle];
      [mProgressWheel setUsesThreadedAnimation:YES];
      [mProgressWheel setDisplayedWhenStopped:NO];
      [mProgressWheel setAutoresizingMask:NSViewMaxXMargin];
    }
    else
      mProgressWheel = nil;

    // create close button. keep a strong ref as view goes in and out of view hierarchy
    mCloseButton = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)];
    [mCloseButton setImage:[BrowserTabViewItem closeIcon]];
    [mCloseButton setAlternateImage:[BrowserTabViewItem closeIconPressed]];
    [mCloseButton setImagePosition:NSImageOnly];
    [mCloseButton setBezelStyle:NSShadowlessSquareBezelStyle];
    [mCloseButton setBordered:NO];
    [mCloseButton setButtonType:NSMomentaryChangeButton];
    [mCloseButton setTarget:self];
    [mCloseButton setAction:@selector(closeTab)];
    [mCloseButton setAutoresizingMask:NSViewMinXMargin];
    [mTabContentsView addSubview:mCloseButton];

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

- (void)closeTab
{
	[[self view] windowClosed];
	[mCloseButton removeFromSuperview];
	[mProgressWheel removeFromSuperview];
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

  [super setLabel:label];
}

- (NSString*)label
{
  return [[mTabContentsView labelCell] stringValue];
}

-(void)setTabIcon:(NSImage *)newIcon
{
  [super setTabIcon:newIcon];
  [[mTabContentsView labelCell] setImage:mTabIcon];
  [mTabContentsView setNeedsDisplay:YES];
}

- (void)setTabIcon:(NSImage *)newIcon isDraggable:(BOOL)draggable
{
  [self setTabIcon:newIcon];
  mDraggable = draggable;
  [[mTabContentsView labelCell] setImageAlpha:(draggable ? 1.0 : 0.6)];  
}

#pragma mark -

- (void)startLoadAnimation
{
  // supress the tab icon while the spinner is over it
  [[mTabContentsView labelCell] setImageVisible: NO];
  [mTabContentsView setNeedsDisplay:YES];
  
  // add spinner to tab view and start animation
  [mTabContentsView addSubview:mProgressWheel];
  [mProgressWheel startAnimation:self];
}

- (void)stopLoadAnimation
{
  // show the tab icon
  [[mTabContentsView labelCell] setImageVisible: YES];
  [mTabContentsView setNeedsDisplay:YES];
  
  // stop animation and remove spinner from tab view
  [mProgressWheel stopAnimation:self];
  [mProgressWheel removeFromSuperview];
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

#define NO_TOOLTIPS_ON_TABS 1

#ifdef NO_TOOLTIPS_ON_TABS
// bug 168719 covers crashes in AppKit after using a lot of tabs because
// the tooltip code internal to NSTabView/NSTabViewItem gets confused and
// tries to set a tooltip for a (probably) deallocated object. Since we can't
// easily get into the guts, all we can do is disable tooltips to fix this
// topcrash by stubbing out the NSTabViewItem's method that sets up the
// toolip rects.
//
// It is my opinion that this is Apple's bug, but just try proving that to them.
-(void)_resetToolTipIfNecessary
{
  // no-op
}
#endif

@end
