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
* The Initial Developer of the Original Code is Matt Judy.
*/

#import "CHExtendedTabView.h"

//////////////////////////
//     NEEDS IMPLEMENTED : Implement drag tracking for moving tabs around.
//  Implementation hints : Track drags ;)
//                       : Change tab controlTint to indicate drag location?
//				   		 : Move tab titles around when dragging.
//////////////////////////

@interface CHExtendedTabView (Private)
- (void)showOrHideTabsAsAppropriate;
@end

@implementation CHExtendedTabView

/******************************************/
/*** Initialization                     ***/
/******************************************/

- (id)initWithFrame:(NSRect)frameRect
{
    if ( (self = [super initWithFrame:frameRect]) ) {
        autoHides = YES;
    }
    return self;
}

- (void)awakeFromNib
{
    [self showOrHideTabsAsAppropriate];
}

/******************************************/
/*** Overridden Methods                 ***/
/******************************************/

- (BOOL)isOpaque
{
    if ( ([self tabViewType] == NSNoTabsBezelBorder) && (NSAppKitVersionNumber < 633) ) {
        return NO;
    } else {
        return [super isOpaque];
    }
}

- (void)addTabViewItem:(NSTabViewItem *)tabViewItem
{
    [super addTabViewItem:tabViewItem];
    [self showOrHideTabsAsAppropriate];
}

- (void)removeTabViewItem:(NSTabViewItem *)tabViewItem
{
    [super removeTabViewItem:tabViewItem];
    [self showOrHideTabsAsAppropriate];
}

- (void)insertTabViewItem:(NSTabViewItem *)tabViewItem atIndex:(int)index
{
    [super insertTabViewItem:tabViewItem atIndex:index];
    [self showOrHideTabsAsAppropriate];
}

/******************************************/
/*** Accessor Methods                   ***/
/******************************************/

- (BOOL)autoHides
{
    return autoHides;
}

- (void)setAutoHides:(BOOL)newSetting
{
    autoHides = newSetting;
}


/******************************************/
/*** Instance Methods                   ***/
/******************************************/

// 03-03-2002 mlj: Modifies tab view size and type appropriately... Fragile.
// Only to be used with the 2 types of tab view which we use in Chimera.
- (void)showOrHideTabsAsAppropriate
{
//    if ( autoHides == YES ) {
        if ( [[self tabViewItems] count] < 2) {
            if ( [self tabViewType] != NSNoTabsBezelBorder ) {
                [self setFrameSize:NSMakeSize( NSWidth([self frame]), NSHeight([self frame]) + 10 )];
            }
            [self setTabViewType:NSNoTabsBezelBorder];
        } else {
            if ( [self tabViewType] != NSTopTabsBezelBorder ) {
                [self setFrameSize:NSMakeSize( NSWidth([self frame]), NSHeight([self frame]) - 10 )];
            }
            [self setTabViewType:NSTopTabsBezelBorder];
        }
        [self display];
//    }
}



@end
