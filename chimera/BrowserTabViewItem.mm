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

#import "BrowserTabViewItem.h"

#import "CHBrowserView.h"
#import "MainController.h"
#import "BrowserWindowController.h"


@interface BrowserTabViewItem(Private)
- (void)buildTabContents;
- (void)relocateTabContents:(NSRect)inRect;
- (BOOL)draggable;
@end

#pragma mark -

@interface NSBezierPath (ChimeraBezierPathUtils)

+ (NSBezierPath*)bezierPathWithRoundCorneredRect:(NSRect)rect cornerRadius:(float)cornerRadius;

@end

@implementation NSBezierPath (ChimeraBezierPathUtils)

+ (NSBezierPath*)bezierPathWithRoundCorneredRect:(NSRect)rect cornerRadius:(float)cornerRadius
{
  float maxRadius = cornerRadius;
  if (NSWidth(rect) / 2.0 < maxRadius)
    maxRadius = NSWidth(rect) / 2.0;

  if (NSHeight(rect) / 2.0 < maxRadius)
    maxRadius = NSHeight(rect) / 2.0;

  NSBezierPath*	newPath = [NSBezierPath bezierPath];
  [newPath moveToPoint:NSMakePoint(NSMinX(rect) + maxRadius, NSMinY(rect))];

  [newPath appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(rect) - maxRadius, NSMinY(rect) + maxRadius)
      radius:maxRadius startAngle:270.0 endAngle:0.0];

  [newPath appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(rect) - maxRadius, NSMaxY(rect) - maxRadius)
      radius:maxRadius startAngle:0.0 endAngle:90.0];

  [newPath appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(rect) + maxRadius, NSMaxY(rect) - maxRadius)
      radius:maxRadius startAngle:90.0 endAngle:180.0];

  [newPath appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(rect) + maxRadius, NSMinY(rect) + maxRadius)
      radius:maxRadius startAngle:180.0 endAngle:270.0];
  
  [newPath closePath];
  
  return newPath;
}

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
}

- (id)initTextCell:(NSString*)aString;
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;

- (void)setImagePadding:(float)padding;
- (void)setImageSpace:(float)space;
- (void)setImageAlpha:(float)alpha;

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
  
  if (mImage)
  {
    NSRect imageSrcRect = NSZeroRect;
    imageSrcRect.size = [mImage size];    
    [mImage drawInRect:NSInsetRect(imageRect, mImagePadding, mImagePadding)
          fromRect:imageSrcRect operation:NSCompositeSourceOver fraction:mImageAlpha];
  }
  
  // remove image space
  NSDivideRect(textRect, &imageRect, &textRect, mImageSpace, NSMinXEdge);

  int          cellWidth       = (int)NSWidth(textRect);
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

@end

#pragma mark -

// a container view for the items in the tab view item. We use a subclass of
// NSView to handle drag and drop
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
    mTabViewItem = tabViewItem;

    mLabelCell = [[NSTruncatingTextAndImageCell alloc] init];
    [mLabelCell setControlSize:NSSmallControlSize];		// doesn't work?
    [mLabelCell setImagePadding:0.0];
    [mLabelCell setImageSpace:2.0];
    
    [self registerForDraggedTypes:[NSArray arrayWithObjects:
        @"MozURLType", @"MozBookmarkType", NSStringPboardType, NSFilenamesPboardType, nil]];
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
    if (sender == [windowController proxyIconView])
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

  NSArray      *dataVals = [NSArray arrayWithObjects: url, cleanedTitle, nil];
  NSArray      *dataKeys = [NSArray arrayWithObjects: @"url", @"title", nil];
  NSDictionary *data = [NSDictionary dictionaryWithObjects:dataVals forKeys:dataKeys];

  NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [pboard declareTypes:[NSArray arrayWithObjects:@"MozURLType", NSURLPboardType, NSStringPboardType, nil] owner:self];
  
  // MozURLType
  [pboard setPropertyList:data forType: @"MozURLType"];
  // NSURLPboardType type
  [[NSURL URLWithString:url] writeToPasteboard: pboard];
  // NSStringPboardType
  [pboard setString:url forType: NSStringPboardType];
  
  NSPoint dragOrigin = [self frame].origin;
  dragOrigin.y += [self frame].size.height;
  
  [self dragImage: [MainController createImageForDragging:[mLabelCell image] title:title]
                    at:NSMakePoint(0, 0) offset:NSMakeSize(0, 0)
                    event:theEvent pasteboard:pboard source:self slideBack:YES];
}

@end

#pragma mark -

@implementation BrowserTabViewItem

-(id)initWithIdentifier:(id)identifier withTabIcon:(NSImage *)tabIcon
{
  if ( (self = [super initWithIdentifier:identifier withTabIcon:tabIcon]) )
  {
    mTabContentsView = [[BrowserTabItemContainerView alloc]
                            initWithFrame:NSMakeRect(0, 0, 100, 16) andTabItem:self];
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
  [super dealloc];
}

- (NSView*)tabItemContentsView
{
  return mTabContentsView;
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

- (NSSize)sizeOfLabel:(BOOL)shouldTruncateLabel
{
  return [super sizeOfLabel:shouldTruncateLabel];
}

- (void)setLabel:(NSString *)label
{
  NSAttributedString* labelString = [[NSAttributedString alloc] initWithString:label attributes:mLabelAttributes];
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
}

- (void)setTabIcon:(NSImage *)newIcon isDraggable:(BOOL)draggable
{
  [self setTabIcon:newIcon];
  mDraggable = draggable;
  [[mTabContentsView labelCell] setImageAlpha:(draggable ? 1.0 : 0.6)];  
}

@end
