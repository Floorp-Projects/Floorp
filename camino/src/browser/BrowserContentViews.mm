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
 *		Simon Fraser <sfraser@netscape.com>
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

#import "BrowserContentViews.h"

#import "BookmarkToolbar.h"
#import "BrowserTabView.h"
#import "BrowserTabBarView.h"


/*
  These various content views are required to deal with several non-standard sizing issues
  in the bookmarks toolbar and browser tab view.
  
  First, the bookmarks toolbar can expand downwards as it reflows its buttons. The 
  BrowserContentView is required to deal with this, shrinking the BrowserContainerView
  to accomodate it.
  
  Second, we have to play tricks with the BrowserTabView, shifting it around
  when showing and hiding tabs, and expanding it outside the bounds of its
  enclosing control to clip out the shadows. The BrowserContainerView exists
  to handle this, and to draw background in extra space that is created as
  a result.
  
  Note that having this code overrides the autoresize behaviour of these views
  as set in IB.
 ______________
 | Window
 | 
 | 
 |  _________________________________________________________________
    | BrowserContentView                                            |
    | ____________________________________________________________  |
    | | BookmarkToolbar                                          | |
    | |___________________________________________________________| |
    |                                                               |
    | ____________________________________________________________  |
    | | BrowserContainerView                                      | |
    | |                  _______  ________                        | |
    | | ________________/       \/        \_____________________  | |
    | | | BrowserTabView                                        | | |
    | | |                                                       | | |
    | | |                                                       | | |
    | | |                                                       | | |
    | | |                                                       | | |
    | | |                                                       | | |
    | | |                                                       | | |
    | | |_______________________________________________________| | |
    | |___________________________________________________________| |
    | ____________________________________________________________  |
    | | Status bar                                                | |
    | |___________________________________________________________| |
    |_______________________________________________________________|
    
*/

@implementation BrowserContentView

- (void) dealloc
{
  // release the browser content area if it's not visible since we
  // had to retain it to keep it around outside the view hierarchy.
  if ([self isBookmarkManagerVisible])
    [mBrowserContainerView release];
  
  [super dealloc];
}

- (void)awakeFromNib
{
  // at load time, the browser is the main content area. this can
  // change if the user shows the bookmark manager.
  mCurrentContentView = mBrowserContainerView;
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldFrameSize
{
  float bmToolbarHeight = 0.0;
  float statusBarHeight = 0.0;

  if (mBookmarksToolbar)
  {
    // first resize the toolbar, which can reflow to a different height
    [mBookmarksToolbar resizeWithOldSuperviewSize: oldFrameSize];
      
    // make sure the toolbar doesn't get pushed off the top. This view is not flipped,
    // so the top is MaxY
    if (NSMaxY([mBookmarksToolbar frame]) > NSMaxY([self bounds]))
    {
      NSRect	newFrame = [mBookmarksToolbar frame];
      newFrame = NSOffsetRect(newFrame, 0, NSMaxY([self bounds]) - NSMaxY([mBookmarksToolbar frame]));
      [mBookmarksToolbar setFrame:newFrame];
    }
    
    bmToolbarHeight = NSHeight([mBookmarksToolbar frame]);
  }
  
  if (mStatusBar)
  {
    statusBarHeight = NSHeight([mStatusBar frame]);
    NSRect statusRect = [self bounds];
    statusRect.size.height = statusBarHeight;
    [mStatusBar setFrame:statusRect];
  }
  
  // figure out how much space is left for the browser view
  NSRect browserRect = [self bounds];
  // subtract bm toolbar
  browserRect.size.height -= bmToolbarHeight;
  
  // subtract status bar
  browserRect.size.height -= statusBarHeight;
  browserRect.origin.y   += statusBarHeight;
  
  // resize our current content area, whatever it may be. We will
  // take care of resizing the other view when we toggle it to
  // match the size to avoid taking the hit of resizing it when it's
  // not visible.
  [mCurrentContentView setFrame:browserRect];
  // NSLog(@"resizing to %f %f", browserRect.size.width, browserRect.size.height);
}


- (void)willRemoveSubview:(NSView *)subview
{
  if (subview == mBookmarksToolbar)
    mBookmarksToolbar = nil;
  else if (subview == mStatusBar)
    mStatusBar = nil;

  [super willRemoveSubview:subview];
}

- (void)didAddSubview:(NSView *)subview
{
  // figure out if mStatusBar or mBookmarksToolbar has been added back?
  [super didAddSubview:subview];
}

- (IBAction) toggleBookmarkManager:(id)sender
{
  NSView* newView = [self isBookmarkManagerVisible] ? mBrowserContainerView : mBookmarkManagerView;
  
  // detach the old view
  [mCurrentContentView retain];
  [mCurrentContentView removeFromSuperview];
  
  // add in the new view. Need to resize it _after_ we've added it because
  // our tab view optimizes away resizes to content views when they're not
  // visible.
  [self addSubview:newView];
  [newView setFrame:[mCurrentContentView frame]];
  [newView release];
  mCurrentContentView = newView;
  
  // don't worry about who has focus, the BWC will take care of that.
}

//
// -isBookmarkManagerVisible
//
// YES if the bookmark manager is currently visible in this window.
//
- (BOOL) isBookmarkManagerVisible
{
  return mCurrentContentView == mBookmarkManagerView;
}

@end

#pragma mark -

@implementation BrowserContainerView

- (void)resizeSubviewsWithOldSize:(NSSize)oldFrameSize
{
  NSRect adjustedRect = [self bounds];
  // mTabView will have set the appropriate size by now
  adjustedRect.size.height -= [mTabBarView frame].size.height;  
  [mTabView setFrame:adjustedRect];
  
  NSRect tbRect = adjustedRect;
  tbRect.size.height = [mTabBarView frame].size.height;
  tbRect.origin.x = 0;
  tbRect.origin.y = NSMaxY(adjustedRect);
  [mTabBarView setFrame:tbRect];
}

@end

#pragma mark -

@implementation BookmarkManagerView

- (BOOL) isOpaque
{
  return YES;
}

- (void)drawRect:(NSRect)aRect
{
  [[NSColor windowBackgroundColor] set];
  NSRectFill(aRect);
  [[NSColor lightGrayColor] set];
  NSFrameRect([self bounds]);
}

@end


