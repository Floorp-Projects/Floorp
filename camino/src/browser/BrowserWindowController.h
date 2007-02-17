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
#import "BrowserWrapper.h"
#import "Find.h"
#import "MainController.h"

class nsIURIFixup;
class nsIBrowserHistory;
class nsIDOMEvent;
class nsIDOMNode;
class nsIWebNavigation;

class BWCDataOwner;

//
// ThrobberHandler
//
// A helper class that handles animating the throbber when it's alive. It starts
// automatically when you init it. To get it to stop, call |stopThrobber|. Calling
// |release| is not enough because the timer used to animate the images holds a strong
// ref back to the handler so it won't go away unless you break that cycle manually with
// |stopThrobber|.
//
// This class must be separate from BrowserWindowController else the
// same thing will happen there and the timer will cause it to stay alive and continue
// loading the webpage even though the window has gone away.
//
@interface ThrobberHandler : NSObject
{
  NSTimer* mTimer;
  NSArray* mImages;
  unsigned int mFrame;
}

// public
- (id)initWithToolbarItem:(NSToolbarItem*)inButton images:(NSArray*)inImages;
- (void)stopThrobber;

// internal
- (void)startThrobber;
- (void)pulseThrobber:(id)aSender;

@end

#pragma mark -

typedef enum
{
  eNewTabEmpty,
  eNewTabAboutBlank,
  eNewTabHomepage
  
} ENewTabContents;

typedef enum
{
  eAppendTabs,
  eReplaceTabs,
  eReplaceFromCurrentTab
	  
} ETabOpenPolicy;

@class CHBrowserView;
@class BookmarkViewController;
@class BookmarkToolbar;
@class BrowserTabView;
@class PageProxyIcon;
@class BrowserContentView;
@class BrowserTabViewItem;
@class AutoCompleteTextField;
@class SearchTextField;
@class ExtendedSplitView;


@interface BrowserWindowController : NSWindowController<Find, BrowserUIDelegate, BrowserUICreationDelegate>
{
  IBOutlet BrowserTabView*    mTabBrowser;
  IBOutlet ExtendedSplitView* mLocationToolbarView;     // parent splitter of location and search, strong
  IBOutlet AutoCompleteTextField* mURLBar;
  IBOutlet NSTextField*       mStatus;
  IBOutlet NSProgressIndicator* mProgress;              // STRONG reference
  IBOutlet NSWindow*          mLocationSheetWindow;
  IBOutlet NSTextField*       mLocationSheetURLField;
  IBOutlet NSView*            mStatusBar;     // contains the status text, progress bar, and lock
  IBOutlet PageProxyIcon*     mProxyIcon;
  IBOutlet BrowserContentView*  mContentView;
  
  IBOutlet BookmarkToolbar*     mPersonalToolbar;

  IBOutlet SearchTextField*     mSearchBar;
  IBOutlet SearchTextField*     mSearchSheetTextField;
  IBOutlet NSWindow*            mSearchSheetWindow;
  
  // Context menu outlets.
  IBOutlet NSMenu*              mPageMenu;
  IBOutlet NSMenu*              mImageMenu;
  IBOutlet NSMenu*              mInputMenu;
  IBOutlet NSMenu*              mLinkMenu;
  IBOutlet NSMenu*              mMailToLinkMenu;
  IBOutlet NSMenu*              mImageLinkMenu;
  IBOutlet NSMenu*              mImageMailToLinkMenu;
  IBOutlet NSMenu*              mTabMenu;
  IBOutlet NSMenu*              mTabBarMenu;

  // Context menu item outlets
  IBOutlet NSMenuItem*          mBackItem;
  IBOutlet NSMenuItem*          mForwardItem;
  IBOutlet NSMenuItem*          mCopyItem;
  
  BOOL mInitialized;

  NSString* mPendingURL;
  NSString* mPendingReferrer;
  BOOL mPendingActivate;
  BOOL mPendingAllowPopups;
  
  BrowserWrapper*               mBrowserView;   // browser wrapper of frontmost tab

  BOOL mMoveReentrant;
  BOOL mClosingWindow;

  BOOL mShouldAutosave;
  BOOL mShouldLoadHomePage;

  BOOL mWindowClosesQuietly;  // if YES, don't warn on multi-tab window close
  
  unsigned int mChromeMask; // Indicates which parts of the window to show (e.g., don't show toolbars)

  // Needed for correct window zooming
  NSRect mLastFrameSize;
  BOOL mShouldZoom;

  // C++ object that holds owning refs to XPCOM objects (and related data)
  BWCDataOwner*               mDataOwner;
  
  // Throbber state variables.
  ThrobberHandler* mThrobberHandler;
  NSArray* mThrobberImages;

  // Funky field editor for URL bar
  NSTextView *mURLFieldEditor;
  
  // cached superview for progress meter so we know where to add/remove it. This
  // could be an outlet, but i figure it's easier to get it at runtime thereby saving
  // someone from messing up in the nib when making changes.
  NSView* mProgressSuperview;                // WEAK ptr
}

- (BrowserTabView*)getTabBrowser;
- (BrowserWrapper*)getBrowserWrapper;

- (void)loadURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer focusContent:(BOOL)focusContent allowPopups:(BOOL)inAllowPopups;
- (void)loadURL:(NSString*)aURLSpec;

- (void)focusURLBar;

- (void)showBlockedPopups:(nsIArray*)blockedSites whitelistingSource:(BOOL)shouldWhitelist;
- (void)blacklistPopupsFromURL:(NSString*)inURL;

  // call to update feed detection in a page
- (void)showFeedDetected:(BOOL)inDetected;
- (IBAction)openFeedPrefPane:(id)sender;

- (void)performAppropriateLocationAction;
- (IBAction)goToLocationFromToolbarURLField:(id)sender;
- (void)beginLocationSheet;
- (IBAction)endLocationSheet:(id)sender;
- (IBAction)cancelLocationSheet:(id)sender;

- (void)performAppropriateSearchAction;
- (void)focusSearchBar;
- (void)beginSearchSheet;
- (IBAction)endSearchSheet:(id)sender;
- (IBAction)cancelSearchSheet:(id)sender;

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize;

- (IBAction)viewSource:(id)aSender;			// focussed frame or page
- (IBAction)viewPageSource:(id)aSender;	// top-level page

- (void)saveDocument:(BOOL)focusedFrame filterView:(NSView*)aFilterView;
- (void)saveURL:(NSView*)aFilterView url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename;

- (IBAction)printDocument:(id)aSender;
- (IBAction)pageSetup:(id)aSender;
- (IBAction)performSearch:(id)aSender;
- (IBAction)searchForSelection:(id)aSender;
- (IBAction)sendURL:(id)aSender;
- (IBAction)sendURLFromLink:(id)aSender;

- (void)startThrobber;
- (void)stopThrobber;
- (void)clickThrobber:(id)aSender;

- (BOOL)validateActionBySelector:(SEL)action;
- (BOOL)performFindCommand;
- (BOOL)canMakeTextBigger;
- (BOOL)canMakeTextSmaller;
- (BOOL)canMakeTextDefaultSize;
- (IBAction)makeTextBigger:(id)aSender;
- (IBAction)makeTextSmaller:(id)aSender;
- (IBAction)makeTextDefaultSize:(id)aSender;

- (IBAction)getInfo:(id)sender;

- (BOOL)shouldShowBookmarkToolbar;

- (IBAction)manageBookmarks: (id)aSender;
- (IBAction)manageHistory: (id)aSender;

- (BOOL)bookmarkManagerIsVisible;
- (BOOL)canHideBookmarks;
- (BOOL)singleBookmarkIsSelected;

- (void)createNewTab:(ENewTabContents)contents;

- (IBAction)newTab:(id)sender;
- (IBAction)closeCurrentTab:(id)sender;
- (IBAction)previousTab:(id)sender;
- (IBAction)nextTab:(id)sender;

- (IBAction)closeSendersTab:(id)sender;
- (IBAction)closeOtherTabs:(id)sender;
- (IBAction)reloadAllTabs:(id)sender;
- (IBAction)reloadSendersTab:(id)sender;
- (IBAction)moveTabToNewWindow:(id)sender;

- (IBAction)back:(id)aSender;
- (IBAction)forward:(id)aSender;
- (IBAction)reload:(id)aSender;
- (IBAction)stop:(id)aSender;
- (IBAction)home:(id)aSender;
- (void)stopAllPendingLoads;

- (IBAction)reloadWithNewCharset:(NSString*)charset;
- (NSString*)currentCharset;

- (IBAction)frameToNewWindow:(id)sender;
- (IBAction)frameToNewTab:(id)sender;
- (IBAction)frameToThisWindow:(id)sender;

- (BrowserWindowController*)openNewWindowWithURL: (NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG allowPopups:(BOOL)inAllowPopups;
- (void)openNewTabWithURL: (NSString*)aURLSpec referrer: (NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG allowPopups:(BOOL)inAllowPopups setJumpback:(BOOL)inSetJumpback;

- (CHBrowserView*)createNewTabBrowser:(BOOL)inLoadInBG;

- (void)openURLArray:(NSArray*)urlArray tabOpenPolicy:(ETabOpenPolicy)tabPolicy allowPopups:(BOOL)inAllowPopups;
- (void)openURLArrayReplacingTabs:(NSArray*)urlArray closeExtraTabs:(BOOL)closeExtra allowPopups:(BOOL)inAllowPopups;

-(BrowserTabViewItem*)createNewTabItem;

- (void)closeBrowserWindow:(BrowserWrapper*)inBrowser;

- (void)willShowPromptForBrowser:(BrowserWrapper*)inBrowser;
- (void)didDismissPromptForBrowser:(BrowserWrapper*)inBrowser;

-(void)autosaveWindowFrame;
-(void)disableAutosave;
-(void)disableLoadPage;

-(void)setChromeMask:(unsigned int)aMask;
-(unsigned int)chromeMask;

-(BOOL)hasFullBrowserChrome;

// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode;
- (NSMenuItem*)prepareAddToAddressBookMenuItem:(NSString*)emailAddress;
- (NSMenu*)contextMenu;
- (NSArray*)mailAddressesInContextMenuLinkNode;
- (NSString*)getContextMenuNodeHrefText;

// Context menu methods
- (IBAction)openLinkInNewWindow:(id)aSender;
- (IBAction)openLinkInNewTab:(id)aSender;
- (void)openLinkInNewWindowOrTab: (BOOL)aUseWindow;
- (IBAction)addToAddressBook:(id)aSender;
- (IBAction)copyAddressToClipboard:(id)aSender;

- (IBAction)savePageAs:(id)aSender;
- (IBAction)saveFrameAs:(id)aSender;
- (IBAction)saveLinkAs:(id)aSender;
- (IBAction)saveImageAs:(id)aSender;

- (IBAction)viewOnlyThisImage:(id)aSender;

- (IBAction)showPageInfo:(id)sender;
- (IBAction)showBookmarksInfo:(id)sender;
- (IBAction)showSiteCertificate:(id)sender;

- (IBAction)addBookmark:(id)aSender;
- (IBAction)addTabGroup:(id)aSender;
- (IBAction)addBookmarkWithoutPrompt:(id)aSender;
- (IBAction)addTabGroupWithoutPrompt:(id)aSender;
- (IBAction)addBookmarkForLink:(id)aSender;
- (IBAction)addBookmarkFolder:(id)aSender;
- (IBAction)addBookmarkSeparator:(id)aSender;

- (IBAction)copyLinkLocation:(id)aSender;
- (IBAction)copyImage:(id)sender;
- (IBAction)copyImageLocation:(id)sender;

- (BookmarkToolbar*) bookmarkToolbar;

- (NSProgressIndicator*) progressIndicator;
- (void) showProgressIndicator;
- (void) hideProgressIndicator;

- (BOOL)windowClosesQuietly;
- (void)setWindowClosesQuietly:(BOOL)inClosesQuietly;

// called when the internal window focus has changed
// this allows us to dispatch activate and deactivate events as necessary
- (void) focusChangedFrom:(NSResponder*) oldResponder to:(NSResponder*) newResponder;

// Called to get cached versions of our security icons
+ (NSImage*) insecureIcon;
+ (NSImage*) secureIcon;
+ (NSImage*) brokenIcon;

// cache the search engines and their search strings we parse from a plist
+ (NSDictionary *)searchURLDictionary;

// cache the toolbar defaults we parse from a plist
+ (NSArray*) toolbarDefaults;

// Get the load-in-background pref.  If possible, aSender's keyEquivalentModifierMask
// is used to determine the shift key's state.  Otherwise (if aSender doesn't respond to
// keyEquivalentModifierMask or aSender is nil) uses the current event's modifier flags.
+ (BOOL)shouldLoadInBackground:(id)aSender;

// Accessor to get the proxy icon view
- (PageProxyIcon *)proxyIconView;

// Accessor for the bm data source
- (BookmarkViewController *)bookmarkViewController;

// Browser view of the frontmost tab (nil if bookmarks are showing?)
- (CHBrowserView*)activeBrowserView;

// return a weak reference to the current web navigation object. Callers should
// not hold onto this for longer than the current call unless they addref it.
- (nsIWebNavigation*) currentWebNavigation;

// handle command-return in location or search field
- (BOOL)handleCommandReturn:(BOOL)aShiftIsDown;

// Load the item in the bookmark bar given by |inIndex| using the given behavior.
- (BOOL)loadBookmarkBarIndex:(unsigned short)inIndex openBehavior:(EBookmarkOpenBehavior)inBehavior;

// Reveal the bookmarkItem in the manager
- (void)revealBookmark:(BookmarkItem*)anItem;

@end
