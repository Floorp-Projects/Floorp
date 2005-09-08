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
 *   Josh Aas - josha@mac.com
 *   Nate Weaver (Wevah) - wevah@derailer.org
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

@class BookmarkMenu;
@class BookmarkItem;
@class BookmarkManager;
@class KeychainService;
@class BrowserWindowController;
@class SharedMenusObj;
@class PreferenceManager;
@class FindDlgController;
@class MVPreferencesController;


typedef enum EBookmarkOpenBehavior
{
  eBookmarkOpenBehaviorDefault,     // follow the prefs
  eBookmarkOpenBehaviorNewTabDefault,
  eBookmarkOpenBehaviorNewTabForeground,
  eBookmarkOpenBehaviorNewTabBackground,
  eBookmarkOpenBehaviorNewWindowDefault,
  eBookmarkOpenBehaviorNewWindowForeground,
  eBookmarkOpenBehaviorNewWindowBackground
};

@interface MainController : NSObject 
{
    IBOutlet NSApplication* mApplication;
    
    // The following item is added to NSSavePanels as an accessory view
    IBOutlet NSView*        mFilterView;
    IBOutlet NSView*        mExportPanelView;
    
    // IBOutlet NSMenuItem*    mOfflineMenuItem;
    IBOutlet NSMenuItem*    mCloseWindowMenuItem;
    IBOutlet NSMenuItem*    mCloseTabMenuItem;

    IBOutlet NSMenu*        mGoMenu;
    IBOutlet BookmarkMenu*  mBookmarksMenu;
    IBOutlet BookmarkMenu*  mDockMenu;
    IBOutlet NSMenu*        mServersSubmenu;

    IBOutlet NSMenu*        mTextEncodingsMenu;

    IBOutlet NSMenu*        mBookmarksHelperMenu; // not shown, used to get enable state
	
    IBOutlet NSMenuItem*    mBookmarksToolbarMenuItem;
    IBOutlet NSMenuItem*    mAddBookmarkMenuItem;
    IBOutlet NSMenuItem*    mCreateBookmarksFolderMenuItem;
    IBOutlet NSMenuItem*    mCreateBookmarksSeparatorMenuItem;  // unused
    IBOutlet NSMenuItem*    mShowAllBookmarksMenuItem;
    
    BOOL                    mOffline;
    BOOL                    mGeckoInitted;

    BookmarkMenu*           mMenuBookmarks;
    BookmarkMenu*           mDockBookmarks;
    
    KeychainService*        mKeychainService;

    FindDlgController*      mFindDialog;

    NSString*               mStartURL;
    
    SharedMenusObj*         mSharedMenusObj;
    NSMutableDictionary*    mCharsets;
}

-(IBAction)aboutWindow:(id)sender;

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
-(IBAction) toggleBookmarksToolbar:(id)aSender;
-(IBAction) doReload:(id)aSender;
-(IBAction) doStop:(id)aSender;
-(IBAction) biggerTextSize:(id)aSender;
-(IBAction) smallerTextSize:(id)aSender;
-(IBAction) viewSource:(id)aSender;
-(IBAction) manageBookmarks: (id)aSender;
-(IBAction) showHistory:(id)aSender;
-(IBAction) clearHistory:(id)aSender;
-(IBAction) reloadWithCharset:(id)aSender;
-(IBAction) toggleAutoCharsetDetection:(id)aSender;

// Bookmarks menu actions.
-(IBAction) importBookmarks:(id)aSender;
-(IBAction) exportBookmarks:(id)aSender;

-(IBAction) openMenuBookmark:(id)aSender;

// Window menu actions
-(IBAction) newTab:(id)aSender;
-(IBAction) closeTab:(id)aSender;
-(IBAction) downloadsWindow:(id)aSender;

// Help menu actions
-(IBAction) supportLink:(id)aSender;
-(IBAction) infoLink:(id)aSender;
-(IBAction) feedbackLink:(id)aSender;
-(IBAction) releaseNoteLink:(id)aSender;
-(IBAction) tipsTricksLink:(id)aSender;
-(IBAction) searchCustomizeLink:(id)aSender;

-(IBAction) aboutPlugins:(id)aSender;

-(IBAction) showCertificates:(id)aSender;

- (void)ensureGeckoInitted;

- (BrowserWindowController*)openBrowserWindowWithURL:(NSString*)aURL andReferrer:(NSString*)aReferrer behind:(NSWindow*)window allowPopups:(BOOL)inAllowPopups;
- (BrowserWindowController*)openBrowserWindowWithURLs:(NSArray*)urlArray behind:(NSWindow*)window allowPopups:(BOOL)inAllowPopups;

- (void)openNewWindowOrTabWithURL:(NSString*)inURLString andReferrer:(NSString*)aReferrer alwaysInFront:(BOOL)forceFront;

- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)inHaveTabs;
- (void)adjustCloseTabMenuItemKeyEquivalent:(BOOL)inHaveTabs;
- (void)fixCloseMenuItemKeyEquivalents;

- (void)adjustBookmarksMenuItemsEnabling;

- (NSView*)getSavePanelView;
- (NSWindow*)getFrontmostBrowserWindow;

- (void)loadBookmark:(BookmarkItem*)item withWindowController:(BrowserWindowController*)browserWindowController openBehavior:(EBookmarkOpenBehavior)behavior;

- (void)displayPreferencesWindow:(id)sender;
- (BOOL)isMainWindowABrowserWindow;

// if the main window is a browser window, return its controller, otherwise nil
- (BrowserWindowController*)getMainWindowBrowserController;

+ (NSImage*)createImageForDragging:(NSImage*)aIcon title:(NSString*)aTitle;

// used by export bookmarks popup to say what file extension should be used on the resulting
// bookmarks file.
-(IBAction) setFileExtension:(id)aSender;

// utility routine to test if a url is "blank" (either empty or about:blank)
+(BOOL)isBlankURL:(NSString*)inURL;

// security feature to reset browser
-(IBAction)resetBrowser:(id)sender;

// prompts the user to reset the cache, then does it
- (IBAction)emptyCache:(id)sender;

// OS feature checks
+ (BOOL)supportsSpotlight;
+ (BOOL)supportsBonjour;

@end
