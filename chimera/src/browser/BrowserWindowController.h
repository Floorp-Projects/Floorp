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

@interface BrowserWindowController : NSWindowController<CHFind>
{
  IBOutlet id mTabBrowser;
  IBOutlet NSDrawer* mSidebarDrawer;
  IBOutlet id mSidebarTabView;
  IBOutlet id mSidebarSourceTabView;
  IBOutlet id mLocationToolbarView;
  IBOutlet NSTextField* mURLBar;
  IBOutlet id mStatus;
  IBOutlet id mProgress;
  IBOutlet id mLocationSheetWindow;
  IBOutlet id mLocationSheetURLField;

  IBOutlet id mSidebarBrowserView;
  IBOutlet id mSidebarBookmarksDataSource;

  IBOutlet CHBookmarksToolbar* mPersonalToolbar;

  IBOutlet id mAddBookmarkSheetWindow;
  IBOutlet id mAddBookmarkTitleField;
  IBOutlet id mAddBookmarkFolderField;
  IBOutlet id mAddBookmarkCheckbox;

  // Context menu outlets.
  IBOutlet id mPageMenu;
  IBOutlet id mImageMenu;
  IBOutlet id mInputMenu;
  IBOutlet id mLinkMenu;
  IBOutlet id mImageLinkMenu;

  NSToolbarItem *mLocationToolbarItem;
  NSToolbarItem *mSidebarToolbarItem;

  BOOL mInitialized;
  NSURL* mURL;

  CHBrowserWrapper* mBrowserView;

  BOOL mMoveReentrant;
  NSModalSession mModalSession;

  BOOL mShouldAutosave;

  BOOL mDrawerCachedFrame;
  NSRect mCachedFrameBeforeDrawerOpen; // This is used by the drawer to figure out if the window should
                                       // be returned to its original position when the drawer closes.
  NSRect mCachedFrameAfterDrawerOpen;

  int mChromeMask; // Indicates which parts of the window to show (e.g., don't show toolbars)

  // Context menu members.
  int mContextMenuFlags;
  nsIDOMEvent* mContextMenuEvent;
  nsIDOMNode* mContextMenuNode;

  // Cached bookmark ds used when adding through a sheet
  id mCachedBMDS;
}

- (void)dealloc;

-(id)getTabBrowser;
-(CHBrowserWrapper*)getBrowserWrapper;

- (void)loadURL:(NSURL*)aURL;
- (void)loadURLString:(NSString*)aStr;
- (void)updateLocationFields:(NSString *)locationString;
- (void)updateToolbarItems;
- (void)focusURLBar;

- (void)performAppropriateLocationAction;
- (IBAction)goToLocationFromToolbarURLField:(id)sender;
- (void)focusURLBar;
- (void)beginLocationSheet;
- (IBAction)endLocationSheet:(id)sender;

- (IBAction)cancelAddBookmarkSheet:(id)sender;
- (IBAction)endAddBookmarkSheet:(id)sender;
- (void)cacheBookmarkDS: (id)aDS;

- (IBAction)viewSource:(id)aSender;

- (void)saveDocument: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList;
- (void)saveURL: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
            url: (NSURL*)aURL suggestedFilename: (NSString*)aFilename;
- (void)printDocument;
- (void)printPreview;

//- (BOOL)findInPage:(NSString*)text;

-(void) biggerTextSize;
-(void) smallerTextSize;

- (void)addBookmarkExtended: (BOOL)aIsFromMenu isFolder:(BOOL)aIsFolder;
- (IBAction)manageBookmarks: (id)aSender;
- (void)importBookmarks: (NSURL*)aURL;
- (IBAction)toggleSidebar:(id)aSender;

- (void)newTab;
- (void)closeTab;
- (void)previousTab;
- (void)nextTab;

- (IBAction)back:(id)aSender;
- (IBAction)forward:(id)aSender;
- (IBAction)reload:(id)aSender;
- (IBAction)stop:(id)aSender;
- (IBAction)home:(id)aSender;

-(void)enterModalSession;

-(void)openNewWindowWithURL: (NSURL*)aURL loadInBackground: (BOOL)aLoadInBG;
-(void)openNewTabWithURL: (NSURL*)aURL loadInBackground: (BOOL)aLoadInBG;

-(void)autosaveWindowFrame;
-(void)disableAutosave;

-(void)setChromeMask:(int)aMask;

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

- (IBAction)viewOnlyThisImage:(id)aSender;

- (NSView*) bookmarksToolbar;
@end

