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
* Portions are Copyright (C) 2002 nibfile.com.
*
* Contributor(s):
* Matt Judy 	<matt@nibfile.com> 	(Original Author)
* David Haas 	<haasd@cae.wisc.edu>
* Simon Fraser <sfraser@netscape.com>
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

#import "IconTabViewItem.h"
#import "NSString+Utils.h"

//
// NSParagraphStyle has a line break mode which will automatically
// append ellipses to the end of a string.  Unfortunately, as of
// OS X 10.1.5, the header says it doesn't work yet.
//
#define BROKEN_NSLineBreakByTruncatingMiddle

static const int kMinTabsForSpacing = 4;		// with 1-4 tabs, each tab is 1/4 the tab view width

@interface IconTabViewItem(Private)
- (void)setupLabelAttributes;
@end;

@implementation IconTabViewItem

-(id)initWithIdentifier:(id)identifier withTabIcon:(NSImage *)tabIcon
{
  if ( (self = [super initWithIdentifier:identifier]) ) {
    [self setTabIcon:tabIcon];
    [self setupLabelAttributes];
    mLabelString = @"";
    mLabelStringWidth = -1;
  }
  return self;
}

-(id)initWithIdentifier:(id)identifier
{
  return [self initWithIdentifier:identifier withTabIcon:nil];
}

-(void)dealloc
{
  [mTabIcon release];
  [mLabelAttributes release];
  [mLabelString release];
  [mTruncLabelString release];
  [super dealloc];
}


- (void)setupLabelAttributes
{
  NSMutableParagraphStyle *labelParagraphStyle = [[NSMutableParagraphStyle alloc] init];
  [labelParagraphStyle setParagraphStyle:[NSParagraphStyle defaultParagraphStyle]];

#ifdef BROKEN_NSLineBreakByTruncatingMiddle
  [labelParagraphStyle setLineBreakMode:NSLineBreakByClipping];
#else
  [labelParagraphStyle setLineBreakMode:NSLineBreakByTruncatingMiddle];
#endif

  [labelParagraphStyle setAlignment:NSNaturalTextAlignment];

  NSFont *labelFont = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  mLabelAttributes  = [[NSDictionary alloc] initWithObjectsAndKeys:
                          labelFont, NSFontAttributeName,
                          labelParagraphStyle, NSParagraphStyleAttributeName,
                          nil];

  [labelParagraphStyle release];
}


- (NSSize)sizeOfLabel:(BOOL)shouldTruncateLabel
{
  int numTabs;
  float theWidth = 0;

  //if we've got text, size for # of tabs & amount of text. 
  // Accounts for icon size, if one present.
  if ([mLabelString length] > 0) {
    numTabs = [[self tabView] numberOfTabViewItems];
    if (numTabs < kMinTabsForSpacing)
      numTabs = kMinTabsForSpacing;
    theWidth = NSWidth([[self tabView] frame]) / numTabs - 16.0; // 16 works - don't know why. Maybe 8px on each side of the label?
    if (shouldTruncateLabel)
    {
      //I have really no clue what this is for.
      //it only gets set YES when the tabs have
      //reached the edge of the screen area.
      //I don't see any difference in behavior between putting
      //in -10 or -600 here.  But -1 instead of -10 makes the
      //tabs go off screen.
      theWidth -= 16.0;
    }
  }
  // If we don't have text, but DO have an icon, we'll have size 15.
  else if ([self tabIcon])
    theWidth = 15.0;

  return NSMakeSize(theWidth, 15.0);		// ugh, hard-coded height.
                                        // This doesn't seem to affect, the height, only the rect pass to
                                        // drawLabel.
}

-(void)drawLabel:(BOOL)shouldTruncateLabel inRect:(NSRect)tabRect
{
  if ([self tabIcon])
  {
    NSPoint drawPoint = NSMakePoint( (tabRect.origin.x), (tabRect.origin.y + 15.0) );
    [[self tabIcon] compositeToPoint:drawPoint operation:NSCompositeSourceOver];
    tabRect = NSMakeRect(NSMinX(tabRect) + 18.0,
                         NSMinY(tabRect),
                         NSWidth(tabRect) - 18.0,
                         NSHeight(tabRect));
  }
  
  // Check if we have to rebuild the cached label string - either because the
  // size changed, or the label changed, or because we never set the label before.
  int width = (int)NSWidth(tabRect);
  if (mLabelStringWidth != width || !mTruncLabelString) {
    [mTruncLabelString release];
    mTruncLabelString = [[NSMutableString alloc] initWithString:mLabelString];
    mLabelStringWidth = width;

#ifdef BROKEN_NSLineBreakByTruncatingMiddle
    // Do our own truncating until Apple implements LineBreakByTruncatingTail/Middle.
    // Then we can remove this code.
    [mTruncLabelString truncateToWidth:width at:kTruncateAtEnd withAttributes:mLabelAttributes];
#endif
  }

  [mTruncLabelString drawInRect:tabRect withAttributes:mLabelAttributes];
}

-(NSImage *)tabIcon
{
  return mTabIcon;
}

-(void)setTabIcon:(NSImage *)newIcon
{
  [mTabIcon autorelease];
  mTabIcon = [newIcon copy];
}

- (void)setLabel:(NSString *)label
{
  // We use our complete own label storage, since for some reasons NSTabViewItem
  // will start to slow us down a *lot* if the label becomes very long
  // (even though we don't use the original drawing/size code at all).
  if (![label isEqual:mLabelString]) {
    [mLabelString release];
    mLabelString = [label copy];
    mLabelStringWidth = -1;
    
    // we need to call setLabel on super to make some stuff happen, but the string
    // doesn't matter.
    [super setLabel:@"foo"];
  }
}

- (NSString *)label
{
	return mLabelString;
}

@end
