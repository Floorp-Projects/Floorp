/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Matt Judy.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Portions of code @2002 nibfile.com.
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

#import <Cocoa/Cocoa.h>

#import "BrowserTabViewItem.h"
@class BrowserTabBarView;

// notification sent when someone double-clicks on the background of the tab bar.
extern NSString* const kTabBarBackgroundDoubleClickedNotification;

@interface BrowserTabView : NSTabView
{
  BOOL mBarAlwaysVisible;
  BOOL mIsDropTarget;
  BOOL mLastClickIsPotentialDrag;
  BOOL mVisible; // YES if the view is in the hierarchy
  IBOutlet BrowserTabBarView * mTabBar;
  
  // if non-nil, the tab to jump back to when the currently visible tab is
  // closed.
  BrowserTabViewItem* mJumpbackTab;
}

+ (BrowserTabViewItem*)makeNewTabItem;

// get and set whether the tab bar is always visible, even when it gets down to a single tab. The 
// default is hide when displaying only one tab. 
- (BOOL)barAlwaysVisible;
- (void)setBarAlwaysVisible:(BOOL)newSetting;

- (void)addTabForURL:(NSString*)aURL referrer:(NSString*)aReferrer inBackground:(BOOL)inBackground;

- (BOOL)tabsVisible;

- (int)indexOfTabViewItemWithURL:(NSString*)aURL;
- (BrowserTabViewItem*)itemWithTag:(int)tag;
- (void)refreshTabBar:(BOOL)rebuild;
- (void)refreshTab:(BrowserTabViewItem*)tab;
- (BOOL)isVisible;
// inform the view that it will be shown or hidden; e.g. prior to showing or hiding the bookmarks
- (void)setVisible:(BOOL)show;
- (void)windowClosed;

// get and set the "jumpback tab", the tab that is jumped to when the currently
// visible tab is closed. Reset automatically when switching tabs or after
// jumping back. This isn't designed to be a tab history, it only lives for a very
// well-defined period.
- (void)setJumpbackTab:(BrowserTabViewItem*)inTab;
- (BrowserTabViewItem*)jumpbackTab;

@end
