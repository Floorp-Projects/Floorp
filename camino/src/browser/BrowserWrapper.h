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

#import <Cocoa/Cocoa.h>
#import "CHBrowserView.h"

@class BrowserWindowController;
@class ToolTip;

@interface BrowserWrapper : NSView <CHBrowserListener, CHBrowserContainer>
{
  NSTextField*              mUrlbar;
  NSTextField*              mStatus;
  BrowserWindowController*  mWindowController;
  NSTabViewItem*            mTabItem;
  NSWindow*                 mWindow;

  NSImage*                  mSiteIconImage;    // current proxy icon image, which may be a site icon (favicon).
  NSString*                 mSiteIconURI;      // uri from  which we loaded the site icon	
  
    // the secure state of this browser. We need to hold it so that we can set
    // the global lock icon whenever we become the primary. Value is one of
    // security enums in nsIWebProgressListener.
  unsigned long             mSecureState;
    // the title associated with this tab's url. We need to hold it so that we
    // can set the window title whenever we become the primary. 
  NSString*                 mTitle;
    // the title we use for the tab. This differs for mTitle when the tab is loading
  NSString*                 mTabTitle;

  CHBrowserView*            mBrowserView;
  NSString*                 mDefaultStatusString;
  NSString*                 mLoadingStatusString;
  ToolTip*                  mToolTip;

  double                    mProgress;
  
  BOOL mIsPrimary;
  BOOL mIsBusy;
  BOOL mOffline;
  BOOL mListenersAttached; // We hook up our click and context menu listeners lazily.
                           // If we never become the primary view, we don't bother creating the listeners.
  BOOL mIsBookmarksImport; // This view was created for the purpose of importing bookmarks.  Upon
                           // completion, we need to do the import and then destroy ourselves.
  BOOL mActivateOnLoad;    // If set, activate the browser view when loading starts.
}

- (id)initWithTab:(NSTabViewItem*)aTab andWindow:(NSWindow*)aWindow;

- (IBAction)load:(id)sender;
- (void)awakeFromNib;
- (void)setFrame:(NSRect)frameRect;
- (CHBrowserView*)getBrowserView;
- (BOOL)isBusy;
- (BOOL)isEmpty;                      // is about:blank loaded?
- (void)windowClosed;

- (NSString*)getCurrentURLSpec;

- (void)loadURI:(NSString *)urlSpec referrer:(NSString*)referrer flags:(unsigned int)flags activate:(BOOL)activate;
- (void)makePrimaryBrowserView: (id)aUrlbar status: (id)aStatus
          windowController: (BrowserWindowController*)aWindowController;
- (void)disconnectView;
- (void)setTab: (NSTabViewItem*)tab;
- (NSTabViewItem*) tab;

- (NSWindow*)getNativeWindow;
- (NSMenu*)getContextMenu;
- (void)setIsBookmarksImport:(BOOL)aIsImport;

- (void)getTitle:(NSString **)outTitle andHref:(NSString**)outHrefString;

// CHBrowserListener messages
- (void)onLoadingStarted;
- (void)onLoadingCompleted:(BOOL)succeeded;
- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes;
- (void)onLocationChange:(NSString*)urlSpec;
- (void)onStatusChange:(NSString*)aMessage;
- (void)onSecurityStateChange:(unsigned long)newState;
- (void)onShowTooltip:(NSPoint)where withText:(NSString*)text;
- (void)onHideTooltip;

// CHBrowserContainer messages
- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type;
- (NSString *)title;
- (void)setTitle:(NSString *)title;
- (void)sizeBrowserTo:(NSSize)dimensions;
- (CHBrowserView*)createBrowserWindow:(unsigned int)mask;

@end
