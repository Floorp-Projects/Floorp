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
#import "CHSplashScreenWindow.h"
#import "FindDlgController.h"
#import "CHPreferenceManager.h"

class BookmarksService;

@interface MainController : NSObject 
{
    IBOutlet id mApplication;
    
    // The following two items are used by the filter list when saving files.
    IBOutlet id mFilterView;
    IBOutlet id mFilterList;
    
    IBOutlet id mOfflineMenuItem;
    
    // The bookmarks menu.
    IBOutlet id mBookmarksMenu;

    IBOutlet id mBookmarksToolbarMenuItem;
    
    BOOL mOffline;

    CHSplashScreenWindow *mSplashScreen;
    
    CHPreferenceManager* mPreferenceManager;

    BookmarksService *mMenuBookmarks;
    
    FindDlgController* mFindDialog;

    MVPreferencesController	*preferencesController;

    NSString* mStartURL;
}

-(void)dealloc;

// File menu actions.
-(IBAction) newWindow:(id)aSender;
-(IBAction) openFile:(id)aSender;
-(IBAction) openLocation:(id)aSender;
-(IBAction) savePage:(id)aSender;
-(IBAction) printPreview:(id)aSender;
-(IBAction) printPage:(id)aSender;
-(IBAction) toggleOfflineMode:(id)aSender;

// Edit menu actions.
-(IBAction) findInPage:(id)aSender;
-(IBAction) findAgain:(id)aSender;

// Go menu actions.
-(IBAction) goBack:(id)aSender;
-(IBAction) goForward:(id)aSender;
-(IBAction) goHome:(id)aSender;
-(IBAction) previousTab:(id)aSender;
-(IBAction) nextTab:(id)aSender;

// View menu actions.
-(IBAction) toggleBookmarksToolbar:(id)aSender;
-(IBAction) doReload:(id)aSender;
-(IBAction) doStop:(id)aSender;
-(IBAction) biggerTextSize:(id)aSender;
-(IBAction) smallerTextSize:(id)aSender;
-(IBAction) viewSource:(id)aSender;

// Bookmarks menu actions.
-(IBAction) importBookmarks:(id)aSender;
-(IBAction) addBookmark:(id)aSender;
-(IBAction) openMenuBookmark:(id)aSender;
-(IBAction) manageBookmarks: (id)aSender;
-(IBAction) addFolder:(id)aSender;
-(IBAction) addSeparator:(id)aSender;

//Window menu actions
-(IBAction) newTab:(id)aSender;
-(IBAction) closeTab:(id)aSender;

-(BrowserWindowController*)openBrowserWindowWithURL: (NSString*)aURL;

- (MVPreferencesController *)preferencesController;
- (void)displayPreferencesWindow:sender;

- (IBAction)showAboutBox:(id)sender;

+ (NSImage*)createImageForDragging:(NSImage*)aIcon title:(NSString*)aTitle;

- (void)updatePrebinding;
- (void)prebindFinished:(NSNotification *)aNotification;

@end
