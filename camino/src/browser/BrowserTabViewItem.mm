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

@interface NSTruncatingTextCell : NSCell
{
  NSMutableString *mTruncLabelString;
  int             mLabelStringWidth;      // -1 if not known
}

- (id)initTextCell:(NSString*)aString;
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;

@end

@implementation NSTruncatingTextCell

- (id)initTextCell:(NSString*)aString
{
  if ((self = [super initTextCell:aString]))
  {
    mTruncLabelString = nil;
    mLabelStringWidth = -1;
  }
  return self;
}

- (void)dealloc
{
  [mTruncLabelString release];
  [super dealloc];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
  int          cellWidth       = (int)NSWidth(cellFrame);
  NSDictionary *cellAttributes = [[self attributedStringValue] attributesAtIndex:0 effectiveRange:nil];

  if (mLabelStringWidth != cellWidth || !mTruncLabelString)
  {
    [mTruncLabelString release];
    mTruncLabelString = [[NSMutableString alloc] initWithString:[self stringValue]];
    [mTruncLabelString truncateToWidth:cellWidth at:kTruncateAtEnd withAttributes:cellAttributes];
    mLabelStringWidth = cellWidth;
  }
  
  [mTruncLabelString drawInRect:cellFrame withAttributes:cellAttributes];
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

@end

#pragma mark -

// a container view for the items in the tab view item. We use a subclass of
// NSView to handle drag and drop
@interface BrowserTabItemContainerView : NSView
{
  NSTabViewItem* mTabViewItem;
  NSImageCell*   mIconCell;
  NSCell*        mLabelCell;
  
  BOOL           mIsDropTarget;
}

- (NSCell*)iconCell;
- (NSCell*)labelCell;

- (void)showDragDestinationIndicator;
- (void)hideDragDestinationIndicator;

@end

@implementation BrowserTabItemContainerView

- (id)initWithFrame:(NSRect)frameRect andTabItem:(NSTabViewItem*)tabViewItem
{
  if ( (self = [super initWithFrame:frameRect]) )
  {
    mTabViewItem = tabViewItem;

    mIconCell  = [[NSImageCell alloc] initImageCell:nil];
    [mIconCell setImageAlignment:NSImageAlignCenter];
    [mIconCell setImageScaling:NSScaleProportionally];

    mLabelCell = [[NSTruncatingTextCell alloc] init];
    [mLabelCell setControlSize:NSSmallControlSize];		// doesn't work?
    
    //[mIconCell setCellAttribute:NSChangeBackgroundCell to:(NSChangeGrayCellMask | NSChangeBackgroundCellMask)];

    [self registerForDraggedTypes:[NSArray arrayWithObjects:
        @"MozURLType", @"MozBookmarkType", NSStringPboardType, NSFilenamesPboardType, nil]];
  }
  return self;
}

- (void)dealloc
{
  NSLog(@"BrowserTabItemContainerView dealloc");

  [mIconCell release];
  [mLabelCell release];
  [super dealloc];
}

- (NSCell*)iconCell
{
  return mIconCell;
}

- (NSCell*)labelCell
{
  return mLabelCell;
}


- (void)drawRect:(NSRect)aRect
{
  NSRect tabRect = [self frame];
  
  [mIconCell  drawWithFrame: NSMakeRect(1, 1, 14, 14) inView:self];
  [mLabelCell drawWithFrame: NSMakeRect(18, 0, tabRect.size.width - 18, 16) inView:self];
  
  if (mIsDropTarget)
  {
    NSRect	hilightRect = NSOffsetRect(NSInsetRect(aRect, 1.0, 0), -1.0, 0);
    NSBezierPath* dropTargetOutline = [NSBezierPath bezierPathWithRoundCorneredRect:hilightRect cornerRadius:4.0];

    //[[[NSColor colorForControlTint:NSDefaultControlTint] colorWithAlphaComponent:0.15] set];
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
  BOOL accepted =  [[mTabViewItem tabView] performDragOperation:sender];
  return accepted;
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
  
  [[self nextResponder] mouseDown:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
  [[self nextResponder] mouseUp:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent
{
  NSRect  iconRect   = NSMakeRect(0, 0, 16, 16);
  NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

  if (!NSPointInRect(localPoint, iconRect))
    return;
  
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
  
  [self dragImage: [MainController createImageForDragging:[mIconCell image] title:title]
                    at:NSMakePoint(0, 0) offset:NSMakeSize(0, 0)
                    event:theEvent pasteboard:pboard source:self slideBack:YES];
}

@end

#pragma mark -

@interface BrowserTabViewItem(Private)
- (void)buildTabContents;
- (void)relocateTabContents:(NSRect)inRect;
@end

@implementation BrowserTabViewItem

-(id)initWithIdentifier:(id)identifier withTabIcon:(NSImage *)tabIcon
{
  if ( (self = [super initWithIdentifier:identifier withTabIcon:tabIcon]) )
  {
    mTabContentsView = [[BrowserTabItemContainerView alloc]
                            initWithFrame:NSMakeRect(0, 0, 100, 16) andTabItem:self];
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
  [[mTabContentsView iconCell] setImage:mTabIcon];  
}

@end
