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
#import "CHBrowserWrapper.h"
#import "CHFind.h"
#import "CHBookmarksToolbar.h"

class nsIDOMEvent;
class nsIDOMNode;


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


@class BookmarksDataSource;
@class CHHistoryDataSource;
@class CHExtendedTabView;

@interface BrowserWindowController : NSWindowController<CHFind>
{
  IBOutlet CHExtendedTabView* mTabBrowser;
  IBOutlet NSDrawer*          mSidebarDrawer;
  IBOutlet NSTabView*         mSidebarTabView;
  IBOutlet NSTabView*         mSidebarSourceTabView;
  IBOutlet NSView*            mLocationToolbarView;
  IBOutlet NSTextView*        mURLBar;
  IBOutlet NSTextField*       mStatus;
  IBOutlet NSProgressIndicator* mProgress;
  IBOutlet NSImageView*       mLock;
  IBOutlet NSWindow*          mLocationSheetWindow;
  IBOutlet NSTextField*       mLocationSheetURLField;
  IBOutlet NSView*            mStatusBar;     // contains the status text, progress bar, and lock

  IBOutlet id                   mSidebarBrowserView;  // currently unused
  IBOutlet BookmarksDataSource* mSidebarBookmarksDataSource;
  IBOutlet CHHistoryDataSource* mHistoryDataSource;

  IBOutlet CHBookmarksToolbar*  mPersonalToolbar;

  IBOutlet NSWindow*            mAddBookmarkSheetWindow;
  IBOutlet NSTextField*         mAddBookmarkTitleField;
  IBOutlet NSPopUpButton*       mAddBookmarkFolderField;
  IBOutlet NSButton*            mAddBookmarkCheckbox;

  // Context menu outlets.
  IBOutlet NSMenu*              mPageMenu;
  IBOutlet NSMenu*              mImageMenu;
  IBOutlet NSMenu*              mInputMenu;
  IBOutlet NSMenu*              mLinkMenu;
  IBOutlet NSMenu*              mImageLinkMenu;

  // Context menu item outlets
  IBOutlet NSMenuItem*          mBackItem;
  IBOutlet NSMenuItem*          mForwardItem;
  
  NSToolbarItem*                mLocationToolbarItem;
  NSToolbarItem*                mSidebarToolbarItem;

  BOOL mInitialized;
  NSString* mPendingURL;
  NSString* mPendingReferrer;

  CHBrowserWrapper* mBrowserView;

  BOOL mMoveReentrant;
  NSModalSession mModalSession;

  BOOL mShouldAutosave;
  BOOL mShouldLoadHomePage;

  BOOL mDrawerCachedFrame;
  NSRect mCachedFrameBeforeDrawerOpen; // This is used by the drawer to figure out if the window should
                                       // be returned to its original position when the drawer closes.
  NSRect mCachedFrameAfterDrawerOpen;

  unsigned int mChromeMask; // Indicates which parts of the window to show (e.g., don't show toolbars)

  // Context menu members.
  int mContextMenuFlags;
  nsIDOMEvent* mContextMenuEvent;
  nsIDOMNode* mContextMenuNode;

  // Cached bookmark ds used when adding through a sheet
  id mCachedBMDS;

  // Throbber state variables.
  ThrobberHandler* mThrobberHandler;
  NSArray* mThrobberImages;
}

- (void)dealloc;

-(id)getTabBrowser;
-(CHBrowserWrapper*)getBrowserWrapper;

- (void)loadURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer;
- (void)updateLocationFields:(NSString *)locationString;
- (void)updateToolbarItems;
- (void)focusURLBar;

    // call to update the image of the lock icon with a value from nsIWebProgressListener
- (void)updateLock:(unsigned int)securityState;

- (void)performAppropriateLocationAction;
- (IBAction)goToLocationFromToolbarURLField:(id)sender;
- (void)beginLocationSheet;
- (IBAction)endLocationSheet:(id)sender;
- (IBAction)cancelLocationSheet:(id)sender;

- (IBAction)cancelAddBookmarkSheet:(id)sender;
- (IBAction)endAddBookmarkSheet:(id)sender;
- (void)cacheBookmarkDS: (id)aDS;

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize;

- (IBAction)viewSource:(id)aSender;

- (void)saveDocument: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList;
- (void)saveURL: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
            url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename;
- (IBAction)printDocument:(id)aSender;
- (void)printPreview;
- (IBAction)performSearch:(id)aSender;

- (void)startThrobber;
- (void)stopThrobber;
- (void)clickThrobber:(id)aSender;

-(void) biggerTextSize;
-(void) smallerTextSize;

- (BOOL)shouldShowBookmarkToolbar;

- (void)addBookmarkExtended: (BOOL)aIsFromMenu isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle;
- (IBAction)manageBookmarks: (id)aSender;
- (void)importBookmarks: (NSString*)aURLSpec;
- (IBAction)toggleSidebar:(id)aSender;

- (void)newTab:(BOOL)allowHomepage;
- (void)closeTab;
- (void)previousTab;
- (void)nextTab;

- (IBAction)back:(id)aSender;
- (IBAction)forward:(id)aSender;
- (IBAction)reload:(id)aSender;
- (IBAction)stop:(id)aSender;
- (IBAction)home:(id)aSender;

-(void)enterModalSession;

-(void)openNewWindowWithURL: (NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG;
-(void)openNewWindowWithGroup: (nsIDOMElement*)aFolderElement loadInBackground: (BOOL)aLoadInBG;
-(void)openNewTabWithURL: (NSString*)aURLSpec referrer: (NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG;

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
-(void)openLinkInNewWindowOrTab: (BOOL)aUseWindow;

- (IBAction)savePageAs:(id)aSender;
- (IBAction)saveLinkAs:(id)aSender;
- (IBAction)saveImageAs:(id)aSender;

- (IBAction)bookmarkPage: (id)aSender;
- (IBAction)bookmarkLink: (id)aSender;

- (IBAction)copyLinkLocation:(id)aSender;

- (IBAction)viewOnlyThisImage:(id)aSender;

- (CHBookmarksToolbar*) bookmarksToolbar;

- (BOOL) isResponderGeckoView:(NSResponder*) responder;

// called when the internal window focus has changed
// this allows us to dispatch activate and deactivate events as necessary
- (void) focusChangedFrom:(NSResponder*) oldResponder to:(NSResponder*) newResponder;

// Called to get cached versions of our security icons
+ (NSImage*) insecureIcon;
+ (NSImage*) secureIcon;
+ (NSImage*) brokenIcon;

// cache the toolbar defaults we parse from a plist
+ (NSArray*) toolbarDefaults;

@end

