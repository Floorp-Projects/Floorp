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

#include <Foundation/NSUserDefaults.h>
#include <mach-o/dyld.h>

#import "MainController.h"
#import "BrowserWindowController.h"
#import "BookmarksService.h"
#import "nsCocoaBrowserService.h"
#import "CHAboutBox.h"
#import "CHUserDefaults.h"

#include "nsCOMPtr.h"
#include "nsEmbedAPI.h"

#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIPref.h"
#include "nsIChromeRegistry.h"


#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif

static const char* ioServiceContractID = "@mozilla.org/network/io-service;1";

@implementation MainController

-(id)init
{
    if ( (self = [super init]) ) {
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        if ([defaults boolForKey:USER_DEFAULTS_AUTOREGISTER_KEY]) {
            // This option causes us to simply initialize embedding and exit.
            NSString *path = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
            setenv("MOZILLA_FIVE_HOME", [path fileSystemRepresentation], 1);

#ifdef _BUILD_STATIC_BIN
            NSGetStaticModuleInfo = app_getModuleInfo;
#endif

            if (NS_SUCCEEDED(NS_InitEmbedding(nsnull, nsnull))) {
                // Register new chrome
                nsCOMPtr<nsIChromeRegistry> chromeReg =
                  do_GetService("@mozilla.org/chrome/chrome-registry;1");
                if (chromeReg) {
                  chromeReg->CheckForNewChrome();
                  chromeReg = 0;
                }
                NS_TermEmbedding();
            }

            [NSApp terminate:self];
            return self;
        }

        NSString* url = [defaults stringForKey:USER_DEFAULTS_URL_KEY];
        mStartURL = url ? [url retain] : nil;
        mSplashScreen = [[CHSplashScreenWindow alloc] splashImage:nil withFade:YES withStatusRect:NSMakeRect(0,0,0,0)];
        mFindDialog = nil;
        mMenuBookmarks = nil;
        
        [NSApp setServicesProvider:self];

    }
    return self;
}

-(void)dealloc
{
  [mFindDialog release];
  [super dealloc];
#if DEBUG
  NSLog(@"Main controller died");
#endif
}

-(void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
#ifdef _BUILD_STATIC_BIN
  [self updatePrebinding];
#endif
  // initialize if we haven't already.
  [self preferenceManager];

  // don't open a new browser window if we already have one
  // (for example, from an GetURL Apple Event)
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (!browserWindow)
    [self newWindow: self];
  
  [mSplashScreen close];

  [mBookmarksMenu setAutoenablesItems: NO];
  mMenuBookmarks = new BookmarksService((BookmarksDataSource*)nil);
  mMenuBookmarks->AddObserver();
  mMenuBookmarks->ConstructBookmarksMenu(mBookmarksMenu, nsnull);
  BookmarksService::gMainController = self;
    
  // Initialize offline mode.
  mOffline = NO;
  nsCOMPtr<nsIIOService> ioService(do_GetService(ioServiceContractID));
  if (!ioService)
    return;
  PRBool offline = PR_FALSE;
  ioService->GetOffline(&offline);
  mOffline = offline;
    
  // Set the menu item's text to "Go Online" if we're currently
  // offline.
  if (mOffline)
    [mOfflineMenuItem setTitle: @"Go Online"];	// XXX localize me
}

-(IBAction)newWindow:(id)aSender
{
  // If we have a key window, have it autosave its dimensions before
  // we open a new window.  That ensures the size ends up matching.
  NSWindow* mainWindow = [mApplication mainWindow];
  if ( mainWindow && [[mainWindow windowController] respondsToSelector:@selector(autosaveWindowFrame)] )
    [[mainWindow windowController] autosaveWindowFrame];

  // Now open the new window.
  NSString* homePage = mStartURL ? mStartURL : [mPreferenceManager homePage:YES];
  BrowserWindowController* controller = [self openBrowserWindowWithURL:homePage andReferrer:nil];

  if ([homePage isEqualToString: @"about:blank"])
    [controller focusURLBar];
  else
    [[[controller getBrowserWrapper] getBrowserView] setActive: YES];

  // Only load the command-line specified URL for the first window we open
  if (mStartURL) {
    [mStartURL release];
    mStartURL = nil;
  }
}	

-(IBAction)newTab:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController newTab:NO];
}

-(IBAction)closeTab:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController closeTab];
}

-(IBAction) previousTab:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController previousTab];
}

-(IBAction) nextTab:(id)aSender;
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController nextTab];
}

-(IBAction) openFile:(id)aSender
{
    NSOpenPanel* openPanel = [[[NSOpenPanel alloc] init] autorelease];
    [openPanel setCanChooseFiles: YES];
    [openPanel setCanChooseDirectories: NO];
    [openPanel setAllowsMultipleSelection: NO];
    NSArray* array = [NSArray arrayWithObjects: @"htm",@"html",@"shtml",@"xhtml",@"xml",
                                                @"txt",@"text",
                                                @"gif",@"jpg",@"jpeg",@"png",@"bmp",
                                                nil];
    int result = [openPanel runModalForTypes: array];
    if (result == NSOKButton) {
        NSArray* urlArray = [openPanel URLs];
        if ([urlArray count] == 0)
            return;
        NSURL* url = [urlArray objectAtIndex: 0];
        // ----------------------
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];
        // ----------------------

        BrowserWindowController* browserController = [self getMainWindowBrowserController];
        if (browserController)
          [browserController loadURL:[url absoluteString] referrer:nil activate:YES];
        else
          [self openBrowserWindowWithURL:[url absoluteString] andReferrer:nil];
    }
}

-(IBAction) openLocation:(id)aSender
{
    NSWindow* browserWindow = [self getFrontmostBrowserWindow];
    if (!browserWindow) {
      [self openBrowserWindowWithURL: @"about:blank" andReferrer:nil];
      browserWindow = [mApplication mainWindow];
    }
    else if (![browserWindow isMainWindow]) {
      [browserWindow makeKeyAndOrderFront:self];
    }
    
    [[browserWindow windowController] performAppropriateLocationAction];
}

-(IBAction) savePage:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController saveDocument: mFilterView filterList: mFilterList];
}

-(IBAction) printPage:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController printDocument:aSender];
}

-(IBAction) printPreview:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController printPreview];
}

-(IBAction) toggleOfflineMode:(id)aSender
{
    nsCOMPtr<nsIIOService> ioService(do_GetService(ioServiceContractID));
    if (!ioService)
        return;
    PRBool offline = PR_FALSE;
    ioService->GetOffline(&offline);
    ioService->SetOffline(!offline);
    mOffline = !offline;
    
    // Update the menu item text.
    // Set the menu item's text to "Go Online" if we're currently
    // offline.
    if (mOffline)
        [mOfflineMenuItem setTitle: @"Go Online"];
    else
        [mOfflineMenuItem setTitle: @"Work Offline"];
        
    // Indicate that we are working offline.
    [[NSNotificationCenter defaultCenter] postNotificationName:@"offlineModeChanged" object:nil];
}

// Edit menu actions.


//
// -findInPage
//
// Called in response to "Find" in edit menu. Opens the find dialog. We only keep
// one around for the whole app to use, showing/hiding as we see fit.
//
-(IBAction) findInPage:(id)aSender
{
  if ( !mFindDialog )
     mFindDialog = [[FindDlgController alloc] initWithWindowNibName: @"FindDialog"];
  [mFindDialog showWindow:self];
}


//
// -findAgain
//
// Called in response to "Find Again" in edit menu. Tells the find controller
// to find the next occurrance of what's already been found.
//
-(IBAction) findAgain:(id)aSender
{
  if ( mFindDialog )
    [mFindDialog findAgain:aSender];
  else
    NSBeep();
}

-(IBAction) goBack:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController back: aSender]; 
}

-(IBAction) goForward:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController forward: aSender]; 
}

-(IBAction) doReload:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController reload: aSender]; 
}

-(IBAction) doStop:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController stop: aSender]; 
}

-(IBAction) goHome:(id)aSender
{
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (browserWindow) {
    if (![browserWindow isMainWindow])
      [browserWindow makeKeyAndOrderFront:self];
    
    [[browserWindow windowController] home: aSender];
  }
  else {
      [self newWindow:self];  
  }
}

-(IBAction) doSearch:(id)aSender
{
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (browserWindow) {
    if (![browserWindow isMainWindow])
      [browserWindow makeKeyAndOrderFront:self];
    
    [[browserWindow windowController] performSearch: aSender];
  }
  else {
      [self newWindow:self];
      browserWindow = [self getFrontmostBrowserWindow];
      [[browserWindow windowController] performSearch: aSender];
  }
}

- (void)adjustBookmarksMenuItemsEnabling:(BOOL)inBrowserWindowFrontmost;
{
  [mAddBookmarkMenuItem 							setEnabled:inBrowserWindowFrontmost];
  [mCreateBookmarksFolderMenuItem 		setEnabled:inBrowserWindowFrontmost];
  [mCreateBookmarksSeparatorMenuItem 	setEnabled:NO];		// separators are not implemented yet
}

-(NSWindow*)getFrontmostBrowserWindow
{
  // for some reason, [NSApp mainWindow] doesn't always work, so we have to
  // do this manually
  NSArray		*windowList   = [NSApp orderedWindows];
  NSWindow	*foundWindow 	= NULL;

  for (unsigned int i = 0; i < [windowList count]; i ++)
  {
    NSWindow*	thisWindow = [windowList objectAtIndex:i];
    
    if ([[thisWindow windowController] isMemberOfClass:[BrowserWindowController class]] &&
       ([[thisWindow windowController] chromeMask] == 0))		// only get windows with full chrome
    {
      foundWindow = thisWindow;
      break;
    }
  }
  
  return foundWindow;
}

// open a new URL. This method always makes a new browser window
-(BrowserWindowController*)openBrowserWindowWithURL: (NSString*)aURL andReferrer: (NSString*)aReferrer
{
	BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];

  // The process of creating a new tab in this brand new window loads about:blank for us as a 
  // side effect of calling GetDocument(). We don't need to do it again.
  if ( [aURL isEqualToString:@"about:blank"] )
    [browser disableLoadPage];
  else
    [browser loadURL: aURL referrer:aReferrer activate:YES];
  [browser showWindow: self];
  return browser;
}

// open a new URL, observing the prefs on how to behave
- (void)openNewWindowOrTabWithURL:(NSString*)inURLString andReferrer: (NSString*)aReferrer
{
  // make sure we're initted
  [self preferenceManager];
  
  PRBool reuseWindow = PR_FALSE;
  PRBool loadInBackground = PR_FALSE;
  
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  if ( prefService ) {
    prefService->GetBoolPref("browser.always_reuse_window", &reuseWindow);
    prefService->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);
  }
  
  // reuse the main window if there is one. The user may have closed all of 
  // them or we may get this event at startup before we've had time to load
  // our window.
  BrowserWindowController* controller = NULL;
  
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (reuseWindow && browserWindow) {
    controller = [browserWindow windowController];
    [controller openNewTabWithURL:inURLString referrer:aReferrer loadInBackground:loadInBackground];
  }
  else {
    // should use BrowserWindowController openNewWindowWithURL, but that method
    // really needs to be on the MainController
    controller = [self openBrowserWindowWithURL: inURLString andReferrer:aReferrer];
  }
  
  [[[controller getBrowserWrapper] getBrowserView] setActive: YES];
}


-(void)applicationWillTerminate: (NSNotification*)aNotification
{
#if DEBUG
    NSLog(@"Termination notification");
#endif

    // Autosave one of the windows.
    [[[mApplication mainWindow] windowController] autosaveWindowFrame];
    
    mMenuBookmarks->RemoveObserver();
    delete mMenuBookmarks;
    mMenuBookmarks = nsnull;
    
    // Release before calling TermEmbedding since we need to access XPCOM
    // to save preferences
    [mPreferenceManager release];
    
    nsCocoaBrowserService::TermEmbedding();
    
    [self autorelease];
}

// Bookmarks menu actions.
-(IBAction) importBookmarks:(id)aSender
{
  // IE favorites: ~/Library/Preferences/Explorer/Favorites.html
  // Omniweb favorites: ~/Library/Application Support/Omniweb/Bookmarks.html
  // For now, open the panel to IE's favorites.
  NSOpenPanel* openPanel = [[[NSOpenPanel alloc] init] autorelease];
  [openPanel setCanChooseFiles: YES];
  [openPanel setCanChooseDirectories: NO];
  [openPanel setAllowsMultipleSelection: NO];
  NSArray* array = [NSArray arrayWithObjects: @"htm",@"html",@"xml", nil];
  int result = [openPanel runModalForDirectory: @"~/Library/Preferences/Explorer/"
                                          file: @"Favorites.html"
                                         types: array];
  if (result == NSOKButton) {
    NSArray* urlArray = [openPanel URLs];
    if ([urlArray count] == 0)
      return;
    NSURL* url = [urlArray objectAtIndex: 0];

    NSWindow* browserWindow = [self getFrontmostBrowserWindow];
    if (!browserWindow) {
      [self newWindow: self];
      browserWindow = [mApplication mainWindow];
    }
    
    [[browserWindow windowController] importBookmarks: [url absoluteString]];
  }
}

-(IBAction) addBookmark:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController addBookmarkExtended: YES isFolder: NO URL:nil title:nil];
}

-(IBAction) addFolder:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController addBookmarkExtended: YES isFolder: YES URL:nil title:nil];
}

-(IBAction) addSeparator:(id)aSender
{
  NSLog(@"Separators not implemented yet");
}

-(IBAction) openMenuBookmark:(id)aSender
{
    NSWindow* browserWindow = [self getFrontmostBrowserWindow];
    if (!browserWindow) {
        [self openBrowserWindowWithURL: @"about:blank" andReferrer:nil];
        browserWindow = [mApplication mainWindow];
    }

    BookmarksService::OpenMenuBookmark([browserWindow windowController], aSender);
}

-(IBAction)manageBookmarks: (id)aSender
{
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (!browserWindow) {
    [self newWindow:self];
    browserWindow = [mApplication mainWindow];
  }

  [[browserWindow windowController] manageBookmarks: aSender];
}

- (CHPreferenceManager *)preferenceManager
{
  if (!mPreferenceManager)
    mPreferenceManager = [[CHPreferenceManager sharedInstance] retain];
  return mPreferenceManager;
}

- (MVPreferencesController *)preferencesController
{
    if (!preferencesController) {
        preferencesController = [[MVPreferencesController sharedInstance] retain];
    }
    return preferencesController;
}

- (void)displayPreferencesWindow:sender
{
    [[self preferencesController] showPreferences:nil] ;
}

- (IBAction)showAboutBox:(id)sender
{
    [[CHAboutBox sharedInstance] showPanel:sender];
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
/*
  // On the off chance that we're getting this message off a launch,
  // we may not have initialized our embedded mozilla.  So check here,
  // and if it's not initialized, do it now.
  [self preferenceManager];

  // Make sure it's a browser window b/c "about box" can also become main window.
  if ([self isMainWindowABrowserWindow]) {
    [[[mApplication mainWindow] windowController] loadURL:[[NSURL fileURLWithPath:filename] absoluteString]];
  }
  else {
    [self openBrowserWindowWithURL:[[NSURL fileURLWithPath:filename] absoluteString]];
  }
*/
  [self openNewWindowOrTabWithURL:[[NSURL fileURLWithPath:filename] absoluteString] andReferrer:nil];
  
  return YES;    
}

- (IBAction)biggerTextSize:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController biggerTextSize];
}

- (IBAction)smallerTextSize:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController smallerTextSize];
}

-(IBAction) viewSource:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController viewSource: aSender];
}

-(BOOL)isMainWindowABrowserWindow
{
  // see also getFrontmostBrowserWindow
  return [[[mApplication mainWindow] windowController] isMemberOfClass:[BrowserWindowController class]];
}

- (BrowserWindowController*)getMainWindowBrowserController
{
  NSWindowController* mainWindowController = [[mApplication mainWindow] windowController];
  if (mainWindowController && [mainWindowController isMemberOfClass:[BrowserWindowController class]])
    return (BrowserWindowController*)mainWindowController;
  
  return nil;
}

- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)inHaveTabs
{
  // capitalization of the key equivalent affects whether the shift modifer is used.
  [mCloseWindowMenuItem setKeyEquivalent: inHaveTabs ? @"W" : @"w"];
}

- (void)adjustCloseTabMenuItemKeyEquivalent:(BOOL)inHaveTabs
{
  if (inHaveTabs) {
    [mCloseTabMenuItem setKeyEquivalent:@"w"];
    [mCloseTabMenuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
  }
  else {
    [mCloseTabMenuItem setKeyEquivalent:@""];
    [mCloseTabMenuItem setKeyEquivalentModifierMask:0];
  }
}

// see if we have a window with tabs open, and adjust the key equivalents for
// Close Tab/Close Window accordingly
- (void)fixCloseMenuItemKeyEquivalents
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController) {
    BOOL windowWithMultipleTabs = ([[browserController getTabBrowser] numberOfTabViewItems] > 1);    
    [self adjustCloseWindowMenuItemKeyEquivalent:windowWithMultipleTabs];
    [self adjustCloseTabMenuItemKeyEquivalent:windowWithMultipleTabs];
  }
}

-(BOOL)validateMenuItem: (NSMenuItem*)aMenuItem
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];

  // disable items that aren't relevant if there's no main browser window open
  SEL action = [aMenuItem action];

  //NSLog(@"MainController validateMenuItem for %@ (%s)", [aMenuItem title], action);

  if (action == @selector(newTab:) ||
        /* ... many more items go here ... */
        /* action == @selector(goHome:) || */			// always enabled
        /* action == @selector(doSearch:) || */		// always enabled
        action == @selector(printPage:) ||
        action == @selector(findInPage:) ||
        action == @selector(doReload:) ||
        action == @selector(biggerTextSize:) ||
        action == @selector(smallerTextSize:) ||
        action == @selector(viewSource:) ||
        action == @selector(savePage:)) {
    if (browserController)
      return YES;
    return NO;
  }

  // check if someone has previously done a find before allowing findAgain to be enabled
  if (action == @selector(findAgain:)) {
    if (browserController)
      return (mFindDialog && [[mFindDialog getSearchText] length] > 0);
    else
      return NO;
  }
  
  // check what the state of the personal toolbar should be, but only if there is a browser
  // window open. Popup windows that have the personal toolbar removed should always gray
  // out this menu.
  if (action == @selector(toggleBookmarksToolbar:)) {
    if (browserController) {
      NSView* bookmarkToolbar = [browserController bookmarksToolbar];
      if ( bookmarkToolbar ) {
        float height = [bookmarkToolbar frame].size.height;
        BOOL toolbarShowing = (height > 0);
        if (toolbarShowing)
          [mBookmarksToolbarMenuItem setTitle: NSLocalizedString(@"Hide Bookmarks Toolbar",@"")];
        else
          [mBookmarksToolbarMenuItem setTitle: NSLocalizedString(@"Show Bookmarks Toolbar",@"")];
        return YES;
      }
      else
        return NO;
    }
    else
      return NO;
  }

  // only activate if we've got multiple tabs open.
  if ((action == @selector(closeTab:) ||
       action == @selector (nextTab:) ||
       action == @selector (previousTab:)))
  {
    if (browserController && [[browserController getTabBrowser] numberOfTabViewItems] > 1)
      return YES;

    return NO;
  }

  if ( action == @selector(doStop:) ) {
    if (browserController)
      return [[browserController getBrowserWrapper] isBusy];
    else
      return NO;
  }
  if ( action == @selector(goBack:) || action == @selector(goForward:) ) {
    if (browserController) {
      CHBrowserView* browserView = [[browserController getBrowserWrapper] getBrowserView];
      if (action == @selector(goBack:))
        return [browserView canGoBack];
      if (action == @selector(goForward:))
        return [browserView canGoForward];
    }
    else
      return NO;
  }
  
  // default return
  return YES;
}

-(IBAction) toggleBookmarksToolbar:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (!browserController) return;

  float height = [[browserController bookmarksToolbar] frame].size.height;
  BOOL showToolbar = (BOOL)(!(height > 0));
  
  [[browserController bookmarksToolbar] showBookmarksToolbar: showToolbar];
  
  // save prefs here
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger: ((showToolbar) ? 0 : 1) forKey: USER_DEFAULTS_HIDE_PERS_TOOLBAR_KEY];
}

-(IBAction) infoLink:(id)aSender
{
  NSString* pageToLoad = NSLocalizedStringFromTable(@"InfoPageDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"InfoPageDefault"])
    [self openNewWindowOrTabWithURL:pageToLoad andReferrer:nil];
}

-(IBAction) feedbackLink:(id)aSender
{
  NSString *pageToLoad = NSLocalizedStringFromTable(@"FeedbackPageDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"FeedbackPageDefault"])
    [self openNewWindowOrTabWithURL:pageToLoad andReferrer:nil];
}

+ (NSImage*)createImageForDragging:(NSImage*)aIcon title:(NSString*)aTitle
{
  NSImage* image;
  NSSize titleSize, imageSize;
  NSRect imageRect;
  NSDictionary* stringAttrs;
  
  // get the size of the new image we are creating
  titleSize = [aTitle sizeWithAttributes:nil];
  imageSize = NSMakeSize(titleSize.width + [aIcon size].width,
                         titleSize.height > [aIcon size].height ? 
                            titleSize.height : [aIcon size].height);
                        
  // create the image and lock drawing focus on it
  image = [[[NSImage alloc] initWithSize:imageSize] autorelease];
  [image lockFocus];
  
  // draw the image and title in image with translucency
  imageRect = NSMakeRect(0,0, [aIcon size].width, [aIcon size].height);
  [aIcon drawAtPoint: NSMakePoint(0,0) fromRect: imageRect operation:NSCompositeCopy fraction:0.8];
  
  stringAttrs = [NSDictionary dictionaryWithObject: [[NSColor textColor] colorWithAlphaComponent:0.8]
                  forKey: NSForegroundColorAttributeName];
  [aTitle drawAtPoint: NSMakePoint([aIcon size].width, 0) withAttributes: stringAttrs];
  
  [image unlockFocus]; 
  
  return image;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApp hasVisibleWindows:(BOOL)flag
{
  // If AppKit knows what to do, let it.
  if (flag)
    return YES;
    
  // If window available, wake it up. |mainWindow| should always be null.
  NSWindow* mainWindow = [mApplication mainWindow];
  if (!mainWindow)
    [self newWindow:self];
  else {												// Don't think this will ever happen, but just in case
    if ([[mainWindow windowController]respondsToSelector:@selector(showWindow:)])
      [[mainWindow windowController] showWindow:self];
    else
      [self newWindow:self];
  }

  return NO;
}

- (void) applicationDidChangeScreenParameters:(NSNotification *)aNotification
{
  [NSApp makeWindowsPerform:@selector(display) inOrder:YES];
}

- (void) updatePrebinding
{
  // Check our prebinding status.  If we didn't launch prebound,
  // fork the update script.

  if (!_dyld_launched_prebound()) {
    NSLog(@"Not prebound, launching update script");
    NSTask* aTask = [[NSTask alloc] init];
    NSArray* args = [NSArray arrayWithObject: @"redo-prebinding.sh"];

    [aTask setCurrentDirectoryPath:[[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent]];
    [aTask setLaunchPath:@"/bin/sh"];
    [aTask setArguments:args];

    [[NSNotificationCenter defaultCenter] addObserver: self
          selector:@selector(prebindFinished:)
          name:NSTaskDidTerminateNotification
          object: nil];

    [aTask launch];
  }
}

- (void)prebindFinished:(NSNotification *)aNotification
{
  [[aNotification object] release];
}
 
// services

- (void)openURL:(NSPasteboard *) pboard userData:(NSString *) userData error:(NSString **) error
{
  NSMutableString *urlString = [[[NSMutableString alloc] init] autorelease];
  if ( !urlString )
    return;

  NSArray* types = [pboard types];
  if (![types containsObject:NSStringPboardType]) {
    *error = NSLocalizedString(@"Error: couldn't open URL.",
                    @"pboard couldn't give URL string.");
    return;
  }
  NSString* pboardString = [pboard stringForType:NSStringPboardType];
  if (!pboardString) {
    *error = NSLocalizedString(@"Error: couldn't open URL.",
                    @"pboard couldn't give URL string.");
    return;
  }
  NSScanner* scanner = [NSScanner scannerWithString:pboardString];
  [scanner scanCharactersFromSet:[[NSCharacterSet whitespaceAndNewlineCharacterSet] invertedSet] intoString:&urlString];
  while(![scanner isAtEnd]) {
    NSString *tmpString;
    [scanner scanCharactersFromSet:[[NSCharacterSet whitespaceAndNewlineCharacterSet] invertedSet] intoString:&tmpString];
    [urlString appendString:tmpString];
  }
  
  [self openNewWindowOrTabWithURL:urlString andReferrer:nil];
}

@end
