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
#import "BrowserWindowController.h"
#import "MVPreferencesController.h"
#import "SplashScreenWindow.h"
#import "FindDlgController.h"
#import "PreferenceManager.h"
#import "SharedMenusObj.h"
#import "NetworkServices.h"

@class BookmarksMenu;
@class KeychainService;
@class NetworkServices;

@interface MainController : NSObject 
{
    IBOutlet NSApplication* mApplication;
    
    // The following two items are used by the filter list when saving files.
    IBOutlet NSView*        mFilterView;
    IBOutlet NSPopUpButton* mFilterList;
    
    // IBOutlet NSMenuItem*    mOfflineMenuItem;
    IBOutlet NSMenuItem*    mCloseWindowMenuItem;
    IBOutlet NSMenuItem*    mCloseTabMenuItem;
    IBOutlet NSMenuItem*    mToggleSidebarMenuItem;

    IBOutlet NSMenu*        mGoMenu;
    IBOutlet NSMenu*        mBookmarksMenu;
    IBOutlet NSMenu*        mDockMenu;
    IBOutlet NSMenu*        mServersSubmenu;
	
    IBOutlet NSMenuItem*    mBookmarksToolbarMenuItem;
    IBOutlet NSMenuItem*    mAddBookmarkMenuItem;
    IBOutlet NSMenuItem*    mCreateBookmarksFolderMenuItem;
    IBOutlet NSMenuItem*    mCreateBookmarksSeparatorMenuItem;
    
    BOOL                    mOffline;

    SplashScreenWindow*   	mSplashScreen;
    
    PreferenceManager*    	mPreferenceManager;

    BookmarksMenu*          mMenuBookmarks;
    BookmarksMenu*          mDockBookmarks;
    
    KeychainService*        mKeychainService;

    FindDlgController*      mFindDialog;

    MVPreferencesController* mPreferencesController;

    NSString*               mStartURL;
    
    SharedMenusObj*         mSharedMenusObj;
    NetworkServices*        mNetworkServices;
}

// File menu actions.
-(IBAction) newWindow:(id)aSender;
-(IBAction) openFile:(id)aSender;
-(IBAction) openLocation:(id)aSender;
-(IBAction) savePage:(id)aSender;
-(IBAction) pageSetup:(id)aSender;
-(IBAction) printPage:(id)aSender;
-(IBAction) toggleOfflineMode:(id)aSender;
-(IBAction) sendURL:(id)aSender;

// Edit menu actions.
-(IBAction) findInPage:(id)aSender;
-(IBAction) findAgain:(id)aSender;
-(IBAction) getInfo:(id)aSender;

// Go menu actions.
-(IBAction) goBack:(id)aSender;
-(IBAction) goForward:(id)aSender;
-(IBAction) goHome:(id)aSender;
-(IBAction) doSearch:(id)aSender;
-(IBAction) previousTab:(id)aSender;
-(IBAction) nextTab:(id)aSender;

// Local servers submenu
-(IBAction) aboutServers:(id)aSender;
-(IBAction) connectToServer:(id)aSender;

// View menu actions.
-(IBAction) toggleSidebar:(id)sender;
-(IBAction) toggleBookmarksToolbar:(id)aSender;
-(IBAction) doReload:(id)aSender;
-(IBAction) doStop:(id)aSender;
-(IBAction) biggerTextSize:(id)aSender;
-(IBAction) smallerTextSize:(id)aSender;
-(IBAction) viewSource:(id)aSender;
-(IBAction) manageSidebar: (id)aSender;

// Bookmarks menu actions.
-(IBAction) importBookmarks:(id)aSender;
-(IBAction) exportBookmarks:(id)aSender;
-(IBAction) addBookmark:(id)aSender;
-(IBAction) openMenuBookmark:(id)aSender;
-(IBAction) addFolder:(id)aSender;
-(IBAction) addSeparator:(id)aSender;

//Window menu actions
-(IBAction) newTab:(id)aSender;
-(IBAction) closeTab:(id)aSender;

//Help menu actions
-(IBAction) infoLink:(id)aSender;
-(IBAction) feedbackLink:(id)aSender;

-(BrowserWindowController*)openBrowserWindowWithURL: (NSString*)aURL andReferrer: (NSString*)aReferrer;
- (void)openNewWindowOrTabWithURL:(NSString*)inURLString andReferrer:(NSString*)aReferrer;

- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)inHaveTabs;
- (void)adjustCloseTabMenuItemKeyEquivalent:(BOOL)inHaveTabs;
- (void)fixCloseMenuItemKeyEquivalents;

- (void)adjustBookmarksMenuItemsEnabling:(BOOL)inBrowserWindowFrontmost;

-(NSWindow*)getFrontmostBrowserWindow;

- (MVPreferencesController *)preferencesController;
- (void)displayPreferencesWindow:sender;
- (PreferenceManager *)preferenceManager;
- (BOOL)isMainWindowABrowserWindow;

// if the main window is a browser window, return its controller, otherwise nil
- (BrowserWindowController*)getMainWindowBrowserController;

- (IBAction)showAboutBox:(id)sender;

+ (NSImage*)createImageForDragging:(NSImage*)aIcon title:(NSString*)aTitle;

- (void)updatePrebinding;
- (void)prebindFinished:(NSNotification *)aNotification;

- (void)pumpGeckoEventQueue;

@end
