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
#import "BrowserWrapper.h"
#import "Find.h"

class nsIURIFixup;
class nsIBrowserHistory;
class nsIDOMEvent;
class nsIDOMNode;
class nsIWebNavigation;

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

@class BookmarkViewController;
@class BookmarkToolbar;
@class HistoryDataSource;
@class BrowserTabView;
@class PageProxyIcon;
@class BrowserContentView;
@class BrowserTabViewItem;
@class AutoCompleteTextField;
@class SearchTextField;

@interface BrowserWindowController : NSWindowController<Find>
{
  IBOutlet BrowserTabView*    mTabBrowser;
  IBOutlet NSView*            mLocationToolbarView;
  IBOutlet AutoCompleteTextField* mURLBar;
  IBOutlet NSTextField*       mStatus;
  IBOutlet NSProgressIndicator* mProgress;              // STRONG reference
  IBOutlet NSImageView*       mLock;
  IBOutlet NSButton*          mPopupBlocked;
  IBOutlet NSWindow*          mLocationSheetWindow;
  IBOutlet NSTextField*       mLocationSheetURLField;
  IBOutlet NSView*            mStatusBar;     // contains the status text, progress bar, and lock
  IBOutlet PageProxyIcon*     mProxyIcon;
  IBOutlet BrowserContentView*  mContentView;
  
  IBOutlet BookmarkViewController* mBookmarkViewController;
  IBOutlet BookmarkToolbar*    mPersonalToolbar;
  IBOutlet HistoryDataSource*   mHistoryDataSource;
  IBOutlet NSWindow*            mAddBookmarkSheetWindow;
  IBOutlet NSTextField*         mAddBookmarkTitleField;
  IBOutlet NSPopUpButton*       mAddBookmarkFolderField;
  IBOutlet NSButton*            mAddBookmarkCheckbox;

  IBOutlet SearchTextField*     mSearchBar;
  IBOutlet SearchTextField*     mSearchSheetTextField;
  IBOutlet NSWindow*            mSearchSheetWindow;
  
  // Context menu outlets.
  IBOutlet NSMenu*              mPageMenu;
  IBOutlet NSMenu*              mImageMenu;
  IBOutlet NSMenu*              mInputMenu;
  IBOutlet NSMenu*              mLinkMenu;
  IBOutlet NSMenu*              mImageLinkMenu;
  IBOutlet NSMenu*              mTabMenu;

  // Context menu item outlets
  IBOutlet NSMenuItem*          mBackItem;
  IBOutlet NSMenuItem*          mForwardItem;
  IBOutlet NSMenuItem*          mCopyItem;
  
  NSToolbarItem*                mSidebarToolbarItem;
  NSToolbarItem*                mBookmarkToolbarItem;

  BOOL mInitialized;
  BOOL mBookmarkViewControllerInitialized;       // have we fully set up the bookmarks view, done lazily
  NSString* mPendingURL;
  NSString* mPendingReferrer;
  BOOL mPendingActivate;
  
  BrowserWrapper* mBrowserView;

  BOOL mMoveReentrant;
  BOOL mClosingWindow;

  BOOL mShouldAutosave;
  BOOL mShouldLoadHomePage;


  unsigned int mChromeMask; // Indicates which parts of the window to show (e.g., don't show toolbars)

  // Context menu members.
  int mContextMenuFlags;
  nsIDOMEvent* mContextMenuEvent;
  nsIDOMNode* mContextMenuNode;

  // Cached bookmark ds used when adding through a sheet
  BookmarkViewController* mCachedBMVC;

  // Throbber state variables.
  ThrobberHandler* mThrobberHandler;
  NSArray* mThrobberImages;

  // Funky field editor for URL bar
  NSTextView *mURLFieldEditor;
  
  // cached superview for progress meter so we know where to add/remove it. This
  // could be an outlet, but i figure it's easier to get it at runtime thereby saving
  // someone from messing up in the nib when making changes.
  NSView* mProgressSuperview;                // WEAK ptr
  NSView* mPopupBlockSuperview;              // WEAK ptr
  
  nsIURIFixup* mURIFixer;                   // [STRONG] should be nsCOMPtr, but can't
  nsIBrowserHistory* mGlobalHistory;        // [STRONG] should be nsCOMPtr, but can't
  
  // saving window titles when opening the bookmark manager
  NSString* mSavedTitle;
}

- (void)dealloc;

- (id)getTabBrowser;
- (BOOL)newTabsAllowed;
- (BrowserWrapper*)getBrowserWrapper;

- (void)loadURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer activate:(BOOL)activate;
- (void)updateLocationFields:(NSString *)locationString;
- (void)updateSiteIcons:(NSImage *)siteIconImage;
- (void)loadingStarted;
- (void)loadingDone;
- (void)focusURLBar;

    // call to update the image of the lock icon with a value from nsIWebProgressListener
- (void)updateLock:(unsigned int)securityState;

    // call to update (show/hide) the image of the blocked-popup indicator and handle
    // the items of the popup un-blocker menu
- (void)showPopupBlocked:(BOOL)inBlocked;
- (void)buildPopupBlockerMenu:(NSNotification*)notifer;
- (IBAction)unblockSite:(id)sender;
- (IBAction)unblockAllSites:(id)sender;
- (IBAction)configurePopupBlocking:(id)sender;

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


- (IBAction)cancelAddBookmarkSheet:(id)sender;
- (IBAction)endAddBookmarkSheet:(id)sender;
- (void)cacheBookmarkVC:(BookmarkViewController *)aDS;

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize;

- (IBAction)viewSource:(id)aSender;			// focussed frame or page
- (IBAction)viewPageSource:(id)aSender;	// top-level page

- (void)saveDocument:(BOOL)focusedFrame filterView:(NSView*)aFilterView;
- (void)saveURL:(NSView*)aFilterView url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename;

- (IBAction)printDocument:(id)aSender;
- (IBAction)pageSetup:(id)aSender;
- (IBAction)performSearch:(id)aSender;
- (IBAction)sendURL:(id)aSender;

- (void)startThrobber;
- (void)stopThrobber;
- (void)clickThrobber:(id)aSender;

- (IBAction)biggerTextSize:(id)aSender;
- (IBAction)smallerTextSize:(id)aSender;

- (void)getInfo:(id)sender;
- (BOOL)canGetInfo;

- (BOOL)shouldShowBookmarkToolbar;

- (void)addBookmarkExtended: (BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle;
- (IBAction)manageBookmarks: (id)aSender;
- (IBAction)manageHistory: (id)aSender;
- (IBAction)toggleSidebar:(id)aSender;
- (BOOL)bookmarksAreVisible:(BOOL)inRequireSelection allowMultipleSelection:(BOOL)allowMultipleSelection;

- (void)createNewTab:(ENewTabContents)contents;

- (IBAction)newTab:(id)sender;
- (IBAction)closeCurrentTab:(id)sender;
- (IBAction)previousTab:(id)sender;
- (IBAction)nextTab:(id)sender;

- (IBAction)closeSendersTab:(id)sender;
- (IBAction)closeOtherTabs:(id)sender;
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

- (void)openNewWindowWithURL: (NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG;
- (void)openNewTabWithURL: (NSString*)aURLSpec referrer: (NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG;

- (void)openURLArray:(NSArray*)urlArray replaceExistingTabs:(BOOL)replaceExisting;

-(BrowserTabViewItem*)createNewTabItem;

- (void)closeBrowserWindow:(BrowserWrapper*)inBrowser;

-(void)autosaveWindowFrame;
-(void)disableAutosave;
-(void)disableLoadPage;

-(void)setChromeMask:(unsigned int)aMask;
-(unsigned int)chromeMask;

-(id)getAddBookmarkSheetWindow;
-(id)getAddBookmarkTitle;
-(id)getAddBookmarkFolder;
-(id)getAddBookmarkCheckbox;

// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode;
- (NSMenu*)getContextMenu;

// Context menu methods
- (IBAction)openLinkInNewWindow:(id)aSender;
- (IBAction)openLinkInNewTab:(id)aSender;
- (void)openLinkInNewWindowOrTab: (BOOL)aUseWindow;

- (IBAction)savePageAs:(id)aSender;
- (IBAction)saveFrameAs:(id)aSender;
- (IBAction)saveLinkAs:(id)aSender;
- (IBAction)saveImageAs:(id)aSender;

- (IBAction)viewOnlyThisImage:(id)aSender;

- (IBAction)bookmarkPage: (id)aSender;
- (IBAction)bookmarkLink: (id)aSender;

- (IBAction)copyLinkLocation:(id)aSender;
- (IBAction)copyImage:(id)sender;
- (IBAction)copyImageLocation:(id)sender;

- (BookmarkToolbar*) bookmarkToolbar;

- (NSProgressIndicator*) progressIndicator;
- (void) showProgressIndicator;
- (void) hideProgressIndicator;

- (BOOL) isResponderGeckoView:(NSResponder*) responder;

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

// Accessor to get the proxy icon view
- (PageProxyIcon *)proxyIconView;

// Accessor for the bm data source
- (BookmarkViewController *)bookmarkViewController;

- (void)toggleBookmarkManager:(id)sender;
- (void)ensureBrowserVisible:(id)sender;
- (NSString*)savedTitle;
- (void)setSavedTitle:(NSString *)aTitle;

// return a weak reference to the current web navigation object. Callers should
// not hold onto this for longer than the current call unless they addref it.
- (nsIWebNavigation*) currentWebNavigation;

@end
