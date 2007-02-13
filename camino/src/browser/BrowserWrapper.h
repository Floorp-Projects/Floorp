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
#import "CHBrowserView.h"

@class BrowserWindowController;
@class ToolTip;
@class AutoCompleteTextField;
@class RolloverImageButton;

class nsIMutableArray;
class nsIArray;

// 
// The BrowserWrapper communicates with the UI via this delegate.
// The delegate will be nil for background tabs.
// 
@protocol BrowserUIDelegate

- (void)loadingStarted;
- (void)loadingDone:(BOOL)activateContent;

- (void)setLoadingActive:(BOOL)active;
// a progress value of 0.0 will set the meter to its indeterminate state
- (void)setLoadingProgress:(float)progress;

- (void)updateWindowTitle:(NSString*)title;
- (void)updateStatus:(NSString*)status;

- (void)updateLocationFields:(NSString*)url ignoreTyping:(BOOL)ignoreTyping;
- (void)updateSiteIcons:(NSImage*)icon ignoreTyping:(BOOL)ignoreTyping;

- (void)showPopupBlocked:(BOOL)blocked;
- (void)configurePopupBlocking;
- (void)unblockAllPopupSites:(nsIArray*)inSites;
- (void)showSecurityState:(unsigned long)state;
- (void)showFeedDetected:(BOOL)inDetected;

- (BOOL)userChangedLocationField;

- (void)contentViewChangedTo:(NSView*)inView forURL:(NSString*)inURL;

@end

//
// The BrowserWrapper requests UI to be created via this delegate. Unlike the
// |BrowserUIDelegate|, this delegate is always present even when in a 
// background tab.
//
@protocol BrowserUICreationDelegate

// create a new tab in the window associated with this wrapper and get it's
// browser view without loading anything into it. why is the name so funny? BWC
// already has openNewTab: and createNewTab: methods...
- (CHBrowserView*)createNewTabBrowser:(BOOL)inLoadInBg;

@end


// 
// ContentViewProvider
// 
// This is used to allow clients (the browser window controller) to cause certain
// URLs to replace the browser view witih a custom view (like bookmarks).
// 
@protocol ContentViewProvider

// supply a view for the given url, or return nil to ignore this request
- (NSView*)provideContentViewForURL:(NSString*)inURL;
// notification that the given view from this provider has been inserted
// for the given url
- (void)contentView:(NSView*)inView usedForURL:(NSString*)inURL;

@end


// 
// BrowserWrapper
// 
// This is a container view for the browser content area that can be
// nested inside of tab view items. It's mostly agnostic about its
// container, communicating with it via a BrowserUIDelegate delegate
// (although some of the code assumes that the [window delegate] implements
// particular methods).
// 
// ContentViewProviders can be registered, and they can replace the content
// view with one of their own (this is used for bookmarks).
// 

@interface BrowserWrapper : NSView <CHBrowserListener, CHBrowserContainer>
{
  NSWindow*                 mWindow;           // the window we are or will be in (never nil)
  
  // XXX the BrowserWrapper really shouldn't know anything about the tab that it's in
  NSTabViewItem*            mTabItem;

  NSImage*                  mSiteIconImage;    // current proxy icon image, which may be a site icon (favicon).
  NSString*                 mSiteIconURI;      // uri from  which we loaded the site icon	
  
    // the secure state of this browser. We need to hold it so that we can set
    // the global lock icon whenever we become the primary. Value is one of
    // security enums in nsIWebProgressListener.
  unsigned long             mSecureState;
    // the display title for this page (which may be different than the page title)
  NSString*                 mDisplayTitle;
    // array of popupevents that have been blocked. We can use them to reconstruct the popups
    // later. If nil, no sites are blocked. Cleared after each new page.
  nsIMutableArray*          mBlockedSites;
  NSMutableArray*           mFeedList;        // list of feeds found on page

  CHBrowserView*            mBrowserView;     // retained
  ToolTip*                  mToolTip;
  NSMutableArray*           mStatusStrings;   // current status bar messages, STRONG

  IBOutlet NSView*          mBlockedPopupView;   // loaded on demand, can be nil, STRONG
  IBOutlet RolloverImageButton* mBlockedPopupCloseButton; 

  double                    mProgress;
  
  id<BrowserUIDelegate>         mDelegate;         // not retained, NULL if in bg
  id<BrowserUICreationDelegate> mCreateDelegate;   // not retained, always present
  
  NSMutableDictionary*      mContentViewProviders;   // ContentViewProviders keyed by the url that shows them
  
  BOOL mIsBusy;
  BOOL mListenersAttached; // We hook up our click and context menu listeners lazily.
                           // If we never become the primary view, we don't bother creating the listeners.
  BOOL mActivateOnLoad;    // If set, activate the browser view when loading starts.
}

- (id)initWithTab:(NSTabViewItem*)aTab inWindow:(NSWindow*)window;
- (id)initWithFrame:(NSRect)frameRect inWindow:(NSWindow*)window;

// only the BrowserWrapper in the frontmost tab has a non-null delegate
- (void)setDelegate:(id<BrowserUIDelegate>)delegate;
- (id<BrowserUIDelegate>)delegate;
// all BrowserWrappers have one of these, even if in the bg.
- (void)setUICreationDelegate:(id<BrowserUICreationDelegate>)delegate;

- (void)windowClosed;

- (void)setFrame:(NSRect)frameRect resizingBrowserViewIfHidden:(BOOL)inResizeBrowser;

- (void)setBrowserActive:(BOOL)inActive;


// accessors
- (CHBrowserView*)getBrowserView;
- (BOOL)isBusy;
- (BOOL)isEmpty;                      // is about:blank loaded?
- (BOOL)isInternalURI;
- (BOOL)canReload;

- (NSString*)currentURI;
- (NSString*)displayTitle;
- (NSString*)pageTitle;
- (NSImage*)siteIcon;
- (NSString*)statusString;
- (float)loadingProgress;
- (BOOL)popupsBlocked;
- (BOOL)feedsDetected;
- (unsigned long)securityState;
- (NSArray*)feedList;

- (IBAction)configurePopupBlocking:(id)sender;
- (IBAction)unblockPopupSites:(id)sender;
- (IBAction)hideBlockedPopupView:(id)sender;

- (void)loadURI:(NSString *)urlSpec referrer:(NSString*)referrer flags:(unsigned int)flags focusContent:(BOOL)focusContent allowPopups:(BOOL)inAllowPopups;

- (void)didBecomeActiveBrowser;
- (void)willResignActiveBrowser;

- (void)setTab:(NSTabViewItem*)tab;
- (NSTabViewItem*) tab;

- (void)reload:(unsigned int)reloadFlags;
- (IBAction)reloadWithNewCharset:(NSString*)charset;
- (NSString*)currentCharset;

- (NSWindow*)nativeWindow;
- (NSMenu*)contextMenu;

// Custom view embedding
- (void)registerContentViewProvider:(id<ContentViewProvider>)inProvider forURL:(NSString*)inURL;
- (void)unregisterContentViewProviderForURL:(NSString*)inURL;
- (id)contentViewProviderForURL:(NSString*)inURL;

// CHBrowserListener messages
- (void)onLoadingStarted;
- (void)onLoadingCompleted:(BOOL)succeeded;
- (void)onProgressChange64:(long long)currentBytes outOf:(long long)maxBytes;
- (void)onProgressChange:(long)currentBytes outOf:(long)maxBytes;
- (void)onLocationChange:(NSString*)urlSpec isNewPage:(BOOL)newPage requestSucceeded:(BOOL)requestOK;
- (void)onStatusChange:(NSString*)aMessage;
- (void)onSecurityStateChange:(unsigned long)newState;
- (void)onShowTooltip:(NSPoint)where withText:(NSString*)text;
- (void)onHideTooltip;
- (void)onFeedDetected:(NSString*)inFeedURI feedTitle:(NSString*)inFeedTitle;

// CHBrowserContainer messages
- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type;
- (NSString *)title;
- (void)setTitle:(NSString *)title;
- (void)sizeBrowserTo:(NSSize)dimensions;
- (CHBrowserView*)createBrowserWindow:(unsigned int)mask;

@end


//
// interface InformationPanelView
//
// A placard-style view for showing additional information to the user.
// Drawn with a shaded background.
//

@interface InformationPanelView : NSView
{
  IBOutlet NSTextField *mPopupBlockedMessageTextField;
  float mVerticalPadding;
  float mMessageTextRightStrutLength;
}
- (void)verticallyCenterAllSubviews;

@end
