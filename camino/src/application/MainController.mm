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
 *   Josh Aas - josha@mac.com
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

#include <mach-o/dyld.h>
#include <sys/utsname.h>

#import "NSString+Utils.h"

#import "ChimeraUIConstants.h"
#import "MainController.h"
#import "BrowserWindow.h"
#import "BrowserWindowController.h"
#import "BookmarkMenu.h"
#import "Bookmark.h"
#import "BookmarkFolder.h"
#import "BookmarkInfoController.h"
#import "BookmarkManager.h"
#import "BookmarkToolbar.h"
#import "RunLoopMessenger.h"
#import "CHBrowserService.h"
#import "UserDefaults.h"
#import "KeychainService.h"
#import "RemoteDataProvider.h"
#import "ProgressDlgController.h"
#import "JSConsole.h"
#import "NetworkServices.h"
#import "MVPreferencesController.h"
#import "FindDlgController.h"
#import "PreferenceManager.h"
#import "SharedMenusObj.h"

#include "nsBuildID.h"
#include "nsCOMPtr.h"
#include "nsEmbedAPI.h"
#include "nsString.h"

#include "nsIWebBrowserChrome.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIPref.h"
#include "nsIChromeRegistry.h"
#include "nsIObserverService.h"
#include "nsIGenericFactory.h"
#include "nsIEventQueueService.h"

#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif

extern const nsModuleComponentInfo* GetAppComponents(unsigned int * outNumComponents);

static const char* ioServiceContractID = "@mozilla.org/network/io-service;1";

// Constants on how to behave when we are asked to open a URL from
// another application. These are values of the "browser.reuse_window" pref.
const int kOpenNewWindowOnAE = 0;
const int kOpenNewTabOnAE = 1;
const int kReuseWindowOnAE = 2;

// Because of bug #2751274, on Mac OS X 10.1.x the sender for this action method when called from
// a dock menu item is always the NSApplication object, not the actual menu item.  This ordinarily 
// makes it impossible to take action based on which menu item was selected, because we don't know
// which menu item called our action method. We have a workaround
// in this sample for the bug (using NSInvocations), but we set up a #define here for the AppKit
// version that is fixed (in a future release of Mac OS X) so that the code can automatically switch
// over to doing things the right way when the time comes.
#define kFixedDockMenuAppKitVersion 632

@interface MainController(Private)<NetworkServicesClient>

- (void)setupStartpage;

@end


@implementation MainController

-(id)init
{
  if ( (self = [super init]) )
  {
    // ensure that we're at least on 10.1.5 for some OS crash fixes wrt Java
    long version = 0;
    ::Gestalt(gestaltSystemVersion, &version);
    if (version < 0x00001015) {
      NSString* appName = NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil);
      NSString* alert = [NSString stringWithFormat: NSLocalizedString(@"RequiredVersionNotMetTitle", @""), appName];
      NSString* message = [NSString stringWithFormat: NSLocalizedString(@"RequiredVersionNotMet", @""), appName];
      NSString* quit = NSLocalizedString(@"AlreadyRunningButton",@"");
      NSRunAlertPanel(alert, message, quit, nil, nil);
      [NSApp terminate:self];
    }
    
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:USER_DEFAULTS_AUTOREGISTER_KEY]) 
    {
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

    mFindDialog = nil;
    mMenuBookmarks = nil;
    
    [NSApp setServicesProvider:self];
    // Initialize shared menu support
    mSharedMenusObj = [[SharedMenusObj alloc] init];
  }
  return self;
}

-(void)dealloc
{
  [mCharsets release];
  
  // Terminate shared menus
  [mSharedMenusObj release];

  [mFindDialog release];
  [mKeychainService release];
  [super dealloc];
#if DEBUG
  NSLog(@"Main controller died");
#endif
}

-(void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
  // initialize if we haven't already.
  PreferenceManager *pm = [PreferenceManager sharedInstance];

  // start bookmarks
  RunLoopMessenger *mainThreadRunLoopMessenger = [[RunLoopMessenger alloc] init];
  [NSThread detachNewThreadSelector:@selector(startBookmarksManager:) toTarget:[BookmarkManager class] withObject:mainThreadRunLoopMessenger];
  [mainThreadRunLoopMessenger release]; //bookmark manager retains this

  [self setupStartpage];

  // register our app components with the embed layer
  unsigned int numComps = 0;
  const nsModuleComponentInfo* comps = GetAppComponents(&numComps);
  CHBrowserService::RegisterAppComponents(comps, numComps);

  // don't open a new browser window if we already have one
  // (for example, from an GetURL Apple Event)
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (!browserWindow)
    [self newWindow: self];
  
  // Initialize offline mode.
  mOffline = NO;
  nsCOMPtr<nsIIOService> ioService(do_GetService(ioServiceContractID));
  if (!ioService)
    return;
  PRBool offline = PR_FALSE;
  ioService->GetOffline(&offline);
  mOffline = offline;
  
  // Initialize the keychain service.
  mKeychainService = [KeychainService instance];
  
  // bring up the JS console service
  BOOL success;
  if ([pm getBooleanPref:"chimera.log_js_to_console" withSuccess:&success])
    [JSConsole sharedJSConsole];

  BOOL doingRendezvous = NO;
  
  if ([pm getBooleanPref:"chimera.enable_rendezvous" withSuccess:&success])
  {
    // are we on 10.2.3 or higher? The DNS resolution stuff is broken before 10.2.3
    long systemVersion;
    OSErr err = ::Gestalt(gestaltSystemVersion, &systemVersion);
    if ((err == noErr) && (systemVersion >= 0x00001023))
    {
      NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
      [nc addObserver:self selector:@selector(availableServicesChanged:) name:NetworkServicesAvailableServicesChanged object:nil];
      [nc addObserver:self selector:@selector(serviceResolved:) name:NetworkServicesResolutionSuccess object:nil];
      [nc addObserver:self selector:@selector(serviceResolutionFailed:) name:NetworkServicesResolutionFailure object:nil];
      doingRendezvous = YES;
    }
  }
  
  if (!doingRendezvous)
  {
    // remove rendezvous items
    int itemIndex;
    while ((itemIndex = [mGoMenu indexOfItemWithTag:kRendezvousRelatedItemTag]) != -1)
      [mGoMenu removeItemAtIndex:itemIndex];
  }
  
  // load up the charset dictionary with keys and menu titles.
  NSString* charsetPath = [NSBundle pathForResource:@"Charset" ofType:@"dict" inDirectory:[[NSBundle mainBundle] bundlePath]];
  mCharsets = [[NSDictionary dictionaryWithContentsOfFile:charsetPath] retain];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
  return [[ProgressDlgController sharedDownloadController] allowTerminate];
}

-(void)applicationWillTerminate: (NSNotification*)aNotification
{
#if DEBUG
  NSLog(@"App will terminate notification");
#endif
  
  [NetworkServices shutdownNetworkServices];

  [[BookmarkManager sharedBookmarkManager] shutdown];

  // Autosave one of the windows.
  [[[mApplication mainWindow] windowController] autosaveWindowFrame];
  
  // Cancel outstanding site icon loads
  [[RemoteDataProvider sharedRemoteDataProvider] cancelOutstandingRequests];
  
  // make sure the info window is closed
  [BookmarkInfoController closeBookmarkInfoController];
    
  // Release before calling TermEmbedding since we need to access XPCOM
  // to save preferences
  [mPreferencesController release];
  
  CHBrowserService::TermEmbedding();
  
  [self autorelease];
}

- (NSMenu *)applicationDockMenu:(NSApplication *)sender
{
  return mDockMenu;
}

- (void)setupStartpage
{
  // only do this if no url was specified in the command-line
  if (mStartURL) return;
  
  // for non-nightly builds, show a special start page
  PreferenceManager* prefManager = [PreferenceManager sharedInstance];

  NSString* vendorSubString = [prefManager getStringPref:"general.useragent.vendorSub" withSuccess:NULL];
  if (![vendorSubString hasSuffix:@"+"])
  {
    // has the user seen this already?
    NSString* startPageRev = [prefManager getStringPref:"browser.startup_page_override.version" withSuccess:NULL];
    if (![vendorSubString isEqualToString:startPageRev])
    {
      NSString* startPage = NSLocalizedStringFromTable( @"StartPageDefault", @"WebsiteDefaults", nil);
      if ([startPage length] && ![startPage isEqualToString:@"StartPageDefault"])
      {
        [mStartURL release];
        mStartURL = [startPage retain];
      }
      
      // set the pref to say they've seen it
      [prefManager setPref:"browser.startup_page_override.version" toString:vendorSubString];
    }
  }
}

//
// setupBookmarkMenus
//
// Needs to be called at startup after we've initialized the bookmark service. Currently
// it's called on a delayed call after we run through the event loop once, but still
// on the main thread.
//
- (void)setupBookmarkMenus:(BookmarkManager *)BookmarkManager
{
  [mBookmarksMenu setAutoenablesItems: NO];

  // menubar bookmarks
  int firstBookmarkItem = [mBookmarksMenu indexOfItemWithTag:kBookmarksDividerTag] + 1;
  mMenuBookmarks = [[BookmarkMenu alloc] initWithMenu: mBookmarksMenu
                                               firstItem: firstBookmarkItem
                                    rootBookmarkFolder: [BookmarkManager bookmarkMenuFolder]];

  // dock bookmarks. Note that we disable them on 10.1 because of a bug noted here:
  // http://developer.apple.com/samplecode/Sample_Code/Cocoa/DeskPictAppDockMenu.htm
  if (NSAppKitVersionNumber >= kFixedDockMenuAppKitVersion)
  {
    [mDockMenu setAutoenablesItems:NO];
    int firstBookmarkItem = [mDockMenu indexOfItemWithTag:kBookmarksDividerTag] + 1;
    mDockBookmarks = [[BookmarkMenu alloc] initWithMenu: mDockMenu
                                                 firstItem: firstBookmarkItem
                                      rootBookmarkFolder: [BookmarkManager dockMenuFolder]];
  }
  else
  {
    // just empty the menu
    while ([mDockMenu numberOfItems] > 0)
      [mDockMenu removeItemAtIndex:0];
  }
}

// a central place for bookmark opening logic.
- (void)loadBookmark:(BookmarkItem*)item withWindowController:(BrowserWindowController*)browserWindowController openBehavior:(EBookmarkOpenBehavior)behavior
{
  if (!browserWindowController)
    browserWindowController = [self getMainWindowBrowserController];
  
  BOOL openInNewWindow = (browserWindowController == nil);
  BOOL openInNewTab = NO;
  BOOL newTabInBackground = NO;
  
  BOOL loadNewTabsInBackgroundPref = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
  
  NSWindow* behindWindow = nil;

  switch (behavior)
  {
    case eBookmarkOpenBehaviorDefault:
      {
        BOOL cmdKeyDown = (GetCurrentKeyModifiers() & cmdKey) != 0;
        if (cmdKeyDown)
          if ([[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL])
          {
            openInNewTab = YES;
            newTabInBackground = loadNewTabsInBackgroundPref;
          }
          else
          {
            openInNewWindow = YES;
            if (loadNewTabsInBackgroundPref)
              behindWindow = [browserWindowController window];
          }
      }
      break;

    case eBookmarkOpenBehaviorNewTabDefault:
      openInNewTab = YES;
      newTabInBackground = browserWindowController && loadNewTabsInBackgroundPref;
      break;

    case eBookmarkOpenBehaviorNewTabForeground:
      openInNewTab = YES;
      break;

    case eBookmarkOpenBehaviorNewTabBackground:
      openInNewTab = YES;
      newTabInBackground = YES;
      break;

    case eBookmarkOpenBehaviorNewWindowDefault:
      openInNewWindow = YES;
      if (loadNewTabsInBackgroundPref)
        behindWindow = [browserWindowController window];
      break;

    case eBookmarkOpenBehaviorNewWindowForeground:
      openInNewWindow = YES;
      break;

    case eBookmarkOpenBehaviorNewWindowBackground:
      openInNewWindow = YES;
      behindWindow = [browserWindowController window];
      break;
  }
  
  if ([item isKindOfClass:[Bookmark class]])
  {
    if (openInNewWindow)
      [self openBrowserWindowWithURL:[(Bookmark *)item url] andReferrer:nil behind:behindWindow];
    else if (openInNewTab)
      [browserWindowController openNewTabWithURL:[(Bookmark *)item url] referrer:nil loadInBackground:newTabInBackground];
    else
      [browserWindowController loadURL:[(Bookmark *)item url] referrer:nil activate:YES];
  }
  else if ([item isKindOfClass:[BookmarkFolder class]])
  {
    if (openInNewWindow)
      [self openBrowserWindowWithURLs:[(BookmarkFolder *)item childURLs] behind:behindWindow];
    else if (openInNewTab)
      [browserWindowController openURLArray:[(BookmarkFolder *)item childURLs] replaceExistingTabs:NO];
    else
      [browserWindowController openURLArray:[(BookmarkFolder *)item childURLs] replaceExistingTabs:YES];
  }
}


+(BOOL) isBlankURL:(NSString*)inURL
{
  BOOL isBlank = NO;
  if ([inURL isEqualToString: @"about:blank"] || [inURL isEqualToString: @""])
    isBlank = YES;
  return isBlank;
}

-(IBAction)newWindow:(id)aSender
{
  // If we have a key window, have it autosave its dimensions before
  // we open a new window.  That ensures the size ends up matching.
  NSWindow* mainWindow = [mApplication mainWindow];
  if ( mainWindow && [[mainWindow windowController] respondsToSelector:@selector(autosaveWindowFrame)] )
    [[mainWindow windowController] autosaveWindowFrame];

  // Now open the new window.
  NSString* homePage = mStartURL ? mStartURL : [[PreferenceManager sharedInstance] homePage:YES];
  BrowserWindowController* controller = [self openBrowserWindowWithURL:homePage andReferrer:nil behind:nil];

  if ([MainController isBlankURL:homePage])
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
    [browserController newTab:aSender];
}

-(IBAction)closeTab:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController closeCurrentTab:aSender];
}

-(IBAction) previousTab:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController previousTab:aSender];
}

-(IBAction) nextTab:(id)aSender;
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController nextTab:aSender];
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
          [self openBrowserWindowWithURL:[url absoluteString] andReferrer:nil behind:nil];
    }
}

-(IBAction) openLocation:(id)aSender
{
    NSWindow* browserWindow = [self getFrontmostBrowserWindow];
    if (!browserWindow) {
      [self openBrowserWindowWithURL: @"about:blank" andReferrer:nil behind:nil];
      browserWindow = [mApplication mainWindow];
    }
    else if (![browserWindow isMainWindow] || ![browserWindow isKeyWindow]) {
      [browserWindow makeKeyAndOrderFront:self];
    }
    
    [[browserWindow windowController] performAppropriateLocationAction];
}

-(IBAction) savePage:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController saveDocument:NO filterView:[self getSavePanelView]];
}

-(IBAction) pageSetup:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController pageSetup:aSender];
}

-(IBAction) printPage:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController printDocument:aSender];
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
/*
    if (mOffline)
        [mOfflineMenuItem setTitle: @"Go Online"];
    else
        [mOfflineMenuItem setTitle: @"Work Offline"];
*/
        
    // Indicate that we are working offline.
    [[NSNotificationCenter defaultCenter] postNotificationName:@"offlineModeChanged" object:nil];
}

- (IBAction)sendURL:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController sendURL:aSender];
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
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController findInPage:NO];
  else
    NSBeep();
}

// Called in response to "Find Previous" in edit menu.  
-(IBAction) findPrevious:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController findInPage:YES];
  else
    NSBeep();
}

-(IBAction) getInfo:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController getInfo: aSender]; 
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

-(IBAction) reloadWithCharset:(id)aSender
{
  // Figure out which charset to tell gecko to load based on the sender's tag. There
  // is guaranteed to only be 1 key that matches this tag, so we just take the first one.
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController) {
    NSArray* charsetList = [mCharsets allKeysForObject:[NSNumber numberWithInt:[aSender tag]]];
    NS_ASSERTION([charsetList count] == 1, "OOPS, multiply defined charsets in plist");
    [browserController reloadWithNewCharset:[charsetList objectAtIndex:0]];
  }
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
  } else {
      [self newWindow:self];
      browserWindow = [self getFrontmostBrowserWindow];
  }

  [[browserWindow windowController] performAppropriateSearchAction];
}

-(IBAction) downloadsWindow:(id)aSender
{
	[[ProgressDlgController sharedDownloadController] showWindow:aSender];
}

//
// -adjustBookmarksMenuItemsEnabling:
//
// We've turned off auto-enabling for the bookmarks menu because of the unknown
// number of bookmarks in the list so we have to manage it manually. This routine
// should be called whenever a window goes away, becomes main, or is no longer main.
//
- (void)adjustBookmarksMenuItemsEnabling:(BOOL)inBrowserWindowFrontmost
{
  [mAddBookmarkMenuItem setEnabled:inBrowserWindowFrontmost];
  [mCreateBookmarksFolderMenuItem setEnabled:inBrowserWindowFrontmost];
  [mCreateBookmarksSeparatorMenuItem setEnabled:YES];
  [mToggleSidebarMenuItem setEnabled:YES];     // always enabled.
  
  // We need the frontmost browser for the case of the dl/about window
  // is the main so we can ensure the "show/hide all bookmarks" has the correct
  // state for that window. Unfortunately, we can't rely on |-getFrontmostBrowserWindow| in all
  // cases, such as when a window has just been opened. As a result, first
  // try |-getMainWindowBrowserController| and if that fails use fFBW as a fallback.   
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (!browserController)
    browserController = (BrowserWindowController*)[[self getFrontmostBrowserWindow] windowController];
    
  if (browserController) {
    if ([browserController bookmarksAreVisible:NO allowMultipleSelection:NO])
      [mToggleSidebarMenuItem setTitle:NSLocalizedString(@"Hide All Bookmarks", @"")];
    else
      [mToggleSidebarMenuItem setTitle:NSLocalizedString(@"Show All Bookmarks", @"")];
  }
  else
    [mToggleSidebarMenuItem setTitle:NSLocalizedString(@"Show All Bookmarks", @"")];
}

- (NSView*)getSavePanelView
{
  if (!mFilterView)
    [NSBundle loadNibNamed:@"AccessoryViews" owner:self];
  return mFilterView;
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
    
    // not all browser windows are created equal. We only consider those with
    // an empty chrome mask, or ones with a toolbar, status bar, and resize control
    // to be real top-level browser windows for purposes of saving size and 
    // loading urls in. Others are popups and are transient.
    if ([[thisWindow windowController] isMemberOfClass:[BrowserWindowController class]]) {
      unsigned int chromeMask = [[thisWindow windowController] chromeMask];
      if (chromeMask == 0 || 
            (chromeMask & nsIWebBrowserChrome::CHROME_TOOLBAR &&
              chromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR &&
              chromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE)) {
        foundWindow = thisWindow;
        break;
      }
    }
  }

  return foundWindow;
}

- (BrowserWindowController*)openBrowserWindowWithURL:(NSString*)aURL andReferrer:(NSString*)aReferrer behind:(NSWindow*)window
{
  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];

  if (window)
  {
    BrowserWindow* browserWin = [browser window];
    [browserWin setSuppressMakeKeyFront:YES];	// prevent gecko focus bringing the window to the front
    [browserWin orderWindow: NSWindowBelow relativeTo: [window windowNumber]];
    [browserWin setSuppressMakeKeyFront:NO];
  }
  else
  {
    [browser showWindow: self];
  }

  // The process of creating a new tab in this brand new window loads about:blank for us as a 
  // side effect of calling GetDocument(). We don't need to do it again.
  if ([MainController isBlankURL:aURL])
    [browser disableLoadPage];
  else
    [browser loadURL: aURL referrer:aReferrer activate:YES];

  return browser;
}

- (BrowserWindowController*)openBrowserWindowWithURLs:(NSArray*)urlArray behind:(NSWindow*)window
{
  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];

  if (window)
  {
    BrowserWindow* browserWin = [browser window];
    [browserWin setSuppressMakeKeyFront:YES];	// prevent gecko focus bringing the window to the front
    [browserWin orderWindow: NSWindowBelow relativeTo: [window windowNumber]];
    [browserWin setSuppressMakeKeyFront:NO];
  }
  else
  {
    [browser showWindow: self];
  }

  [browser openURLArray:urlArray replaceExistingTabs:YES];
  return browser;
}

// open a new URL, observing the prefs on how to behave
- (void)openNewWindowOrTabWithURL:(NSString*)inURLString andReferrer: (NSString*)aReferrer
{
  // make sure we're initted
  [PreferenceManager sharedInstance];
  
  PRInt32 reuseWindow = 0;
  PRBool loadInBackground = PR_FALSE;
  
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  if ( prefService ) {
    prefService->GetIntPref("browser.reuse_window", &reuseWindow);
    prefService->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);
  }
  
  // reuse the main window (if there is one) based on the pref in the 
  // tabbed browsing panel. The user may have closed all of 
  // them or we may get this event at startup before we've had time to load
  // our window.
  BrowserWindowController* controller = (BrowserWindowController*)[[self getFrontmostBrowserWindow] windowController];
  if (reuseWindow > kOpenNewWindowOnAE && controller) {
    if (reuseWindow == kOpenNewTabOnAE) {
      // if we have room for a new tab, open one, otherwise open a new window. if
      // we don't do this, and just reuse the current tab, people will lose the urls
      // as they get replaced in the current tab.
      if ([controller newTabsAllowed])
        [controller openNewTabWithURL:inURLString referrer:aReferrer loadInBackground:loadInBackground];
      else
        controller = [self openBrowserWindowWithURL: inURLString andReferrer:aReferrer behind:nil];
    }
    else
      [controller loadURL: inURLString referrer:nil activate:YES];
  }
  else {
    // should use BrowserWindowController openNewWindowWithURL, but that method
    // really needs to be on the MainController
    controller = [self openBrowserWindowWithURL: inURLString andReferrer:aReferrer behind:nil];
  }
  
  [[[controller getBrowserWrapper] getBrowserView] setActive: YES];
}

// Bookmarks menu actions.
-(IBAction) importBookmarks:(id)aSender
{
  [[BookmarkManager sharedBookmarkManager] startImportBookmarks];
}

// helper for exportBookmarks function
-(IBAction) setFileExtension:(id)aSender
{
  if ([[aSender title] isEqualToString:@"HTML"])
    [[aSender representedObject] setRequiredFileType:@"html"];
  else
    [[aSender representedObject] setRequiredFileType:@"plist"];
}

-(IBAction) exportBookmarks:(id)aSender
{
  NSSavePanel* savePanel = [NSSavePanel savePanel];
  [savePanel setPrompt:NSLocalizedString(@"Export", @"Export")];
  [savePanel setRequiredFileType:@"html"];
  
  // get an accessory view for HTML or Safari .plist output
  if (!mExportPanelView)
    [NSBundle loadNibNamed:@"AccessoryViews" owner:self];
  NSPopUpButton *button = [mExportPanelView viewWithTag:1001];
  [[button itemAtIndex:0] setRepresentedObject:savePanel];
  [[button itemAtIndex:1] setRepresentedObject:savePanel];
  [savePanel setAccessoryView:mExportPanelView];
  
  // start the save panel
  int saveResult = [savePanel runModalForDirectory:nil file:NSLocalizedString(@"ExportedBookmarkFile", @"Exported Bookmarks")];
  int selectedButton = [button indexOfSelectedItem];
  if (saveResult != NSFileHandlingPanelOKButton)
    return;
  if (0 == selectedButton)
    [[BookmarkManager sharedBookmarkManager] writeHTMLFile:[savePanel filename]];
  else
    [[BookmarkManager sharedBookmarkManager] writeSafariFile:[savePanel filename]];
}

-(IBAction) addBookmark:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController addBookmarkExtended: NO URL:nil title:nil];
}

-(IBAction) addFolder:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController addBookmarkExtended: YES URL:nil title:nil];
}

-(IBAction) addSeparator:(id)aSender
{
  NSLog(@"Separators not implemented yet");
}

-(IBAction) openMenuBookmark:(id)aSender
{	
  if ([aSender isMemberOfClass:[NSApplication class]])
    return;		// 10.1. Don't do anything.

  BookmarkItem*  item = [aSender representedObject];
  [self loadBookmark:item withWindowController:[self getMainWindowBrowserController] openBehavior:eBookmarkOpenBehaviorDefault];
}

//
// -showHistory:
//
// show the history in the bookmark manager. Creates a new window if
// one isn't already there. history isn't a toggle, hence the name.
//
-(IBAction) showHistory:(id)aSender
{
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (!browserWindow) {
    [self newWindow:self];
    browserWindow = [mApplication mainWindow];
  }

  [[browserWindow windowController] manageHistory: aSender];
}

//
// manageBookmarks:
//
// toggle the bookmark manager (creating a new window if needed)
//
-(IBAction)manageBookmarks: (id)aSender
{
  NSWindow* browserWindow = [self getFrontmostBrowserWindow];
  if (!browserWindow) {
    [self newWindow:self];
    browserWindow = [mApplication mainWindow];
  }

  [[browserWindow windowController] manageBookmarks: aSender];
}

- (MVPreferencesController *)preferencesController
{
    if (!mPreferencesController) {
        mPreferencesController = [[MVPreferencesController sharedInstance] retain];
    }
    return mPreferencesController;
}

- (void)displayPreferencesWindow:sender
{
    [[self preferencesController] showPreferences:nil] ;
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
    [self openBrowserWindowWithURL:[[NSURL fileURLWithPath:filename] absoluteString behind:nil]];
  }
*/
  [self openNewWindowOrTabWithURL:[[NSURL fileURLWithPath:filename] absoluteString] andReferrer:nil];
  
  return YES;    
}

- (IBAction)biggerTextSize:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController biggerTextSize:aSender];
}

- (IBAction)smallerTextSize:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController smallerTextSize:aSender];
}

-(IBAction) viewSource:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (browserController)
    [browserController viewPageSource: aSender];		// top-level page, not focussed frame
}

-(BOOL)isMainWindowABrowserWindow
{
  // see also getFrontmostBrowserWindow. That will always return a browser
  // window if one exists. This will only return one if it is frontmost.
  return [[[mApplication mainWindow] windowController] isMemberOfClass:[BrowserWindowController class]];
}

- (BrowserWindowController*)getMainWindowBrowserController
{
  // note that [NSApp mainWindow] will return NULL if we are not
  // frontmost
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

  // Handle the encoding menu first, since there are many of those items. They're
  // identifyable because they have a specific tag. 
  const int kEncodingMenuTag = 10;
  if ( [aMenuItem tag] >= kEncodingMenuTag ) {
    if ( browserController ) {
      NSString* charset = [browserController currentCharset];
#if DEBUG_CHARSET
      NSLog(@"charset is %@", charset);
#endif
      // given the document's charset, check if it maps to the same int as the
      // current item's key. If yes, we select this item because it's our charset.
      // Note that this relies on the key in the nib mapping to the right integer
      // in the plist.
      NSNumber* tag = [mCharsets objectForKey:[charset lowercaseString]];
      if ( tag && [[NSNumber numberWithInt:[aMenuItem tag]] isEqualToNumber:tag] )
        [aMenuItem setState:NSOnState];
      else
        [aMenuItem setState:NSOffState];
      return YES;
    }
    return NO;
  }

  // disable items that aren't relevant if there's no main browser window open
  SEL action = [aMenuItem action];

  // NSLog(@"MainController validateMenuItem for %@ (%s)", [aMenuItem title], action);
  
  if (action == @selector(printPage:) ||
        /* ... many more items go here ... */
        /* action == @selector(goHome:) || */			// always enabled
        /* action == @selector(doSearch:) || */		// always enabled
        action == @selector(pageSetup:) ||
        action == @selector(findInPage:) ||
        action == @selector(doReload:) ||
        action == @selector(viewSource:) ||
        action == @selector(savePage:))
  {
    return (browserController != nil);
  }
  
  if (action == @selector(newTab:))
    return (browserController && [browserController newTabsAllowed]);
  
  // check if someone has previously done a find before allowing findAgain to be enabled
  if (action == @selector(findAgain:))
    return (browserController && [[browserController lastFindText] length] > 0);
  
  // check what the state of the personal toolbar should be, but only if there is a browser
  // window open. Popup windows that have the personal toolbar removed should always gray
  // out this menu.
  if (action == @selector(toggleBookmarksToolbar:)) {
    if (browserController) {
      NSView* bookmarkToolbar = [browserController bookmarkToolbar];
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

  if ( action == @selector(getInfo:) )
    return (browserController && [browserController canGetInfo]);

  // only activate if we've got multiple tabs open.
  if ((action == @selector(closeTab:) ||
       action == @selector(nextTab:) ||
       action == @selector(previousTab:)))
  {
    return (browserController && [[browserController getTabBrowser] numberOfTabViewItems] > 1);
  }

  if ( action == @selector(addBookmark:) )
    return (browserController && ![[browserController getBrowserWrapper] isEmpty]);
  
  if ( action == @selector(biggerTextSize:) )
    return (browserController &&
            ![[browserController getBrowserWrapper] isEmpty] &&
            [[[browserController getBrowserWrapper] getBrowserView] canMakeTextBigger]);

  if ( action == @selector(smallerTextSize:) )
    return (browserController &&
            ![[browserController getBrowserWrapper] isEmpty] &&
            [[[browserController getBrowserWrapper] getBrowserView] canMakeTextSmaller]);
  
  if ( action == @selector(doStop:) )
    return (browserController && [[browserController getBrowserWrapper] isBusy]);

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
  
  if ( action == @selector(sendURL:) )
  {
    NSString* titleString = nil;
    NSString* urlString = nil;
    [[[self getMainWindowBrowserController] getBrowserWrapper] getTitle:&titleString andHref:&urlString];
    return ![MainController isBlankURL:urlString];
  }

  // default return
  return YES;
}

-(IBAction) toggleBookmarksToolbar:(id)aSender
{
  BrowserWindowController* browserController = [self getMainWindowBrowserController];
  if (!browserController) return;

  float height = [[browserController bookmarkToolbar] frame].size.height;
  BOOL showToolbar = (BOOL)(!(height > 0));
  
  [[browserController bookmarkToolbar] showBookmarksToolbar: showToolbar];
  
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

-(IBAction) releaseNoteLink:(id)aSender;
{
  NSString *pageToLoad = NSLocalizedStringFromTable(@"ReleaseNotesDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"ReleaseNotesDefault"])
    [self openNewWindowOrTabWithURL:pageToLoad andReferrer:nil];
}

-(IBAction) tipsTricksLink:(id)aSender;
{
  NSString *pageToLoad = NSLocalizedStringFromTable(@"TipsTricksPageDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"TipsTricksPageDefault"])
    [self openNewWindowOrTabWithURL:pageToLoad andReferrer:nil];
}

-(IBAction) searchCustomizeLink:(id)aSender;
{
  NSString *pageToLoad = NSLocalizedStringFromTable(@"SearchCustomPageDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"SearchCustomPageDefault"])
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
  // ignore |hasVisibleWindows| because we always want to show a browser window when
  // the user clicks on the app icon, even if, say, prefs or the d/l window are open.
  // If there is no browser, create one. If there is one, unminimize it if it's in the dock.
  NSWindow* frontBrowser = [self getFrontmostBrowserWindow];
  if ( !frontBrowser )
    [self newWindow:self];
  else if ([frontBrowser isMiniaturized])
    [frontBrowser deminiaturize:self];
  
  return NO;
}

- (void) applicationDidChangeScreenParameters:(NSNotification *)aNotification
{
  [NSApp makeWindowsPerform:@selector(display) inOrder:YES];
}

- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
  [mFindDialog applicationWasActivated];
}

//
// -aboutWindow:
//
// Show the (slightly modified) standard about window, with the build id instead of the
// typical application version. It'll display like "2003120403 (v0.7+)". 
//
-(IBAction)aboutWindow:(id)sender
{
  NSString* version = [NSString stringWithFormat:@"%010u", NS_BUILD_ID];
  NSDictionary* d = [NSDictionary dictionaryWithObject:version forKey:@"ApplicationVersion"];
  [NSApp orderFrontStandardAboutPanelWithOptions:d];
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


#pragma mark -


static int SortByProtocolAndName(NSDictionary* item1, NSDictionary* item2, void *context)
{
  NSComparisonResult protocolCompare = [[item1 objectForKey:@"name"] compare:[item2 objectForKey:@"name"] options:NSCaseInsensitiveSearch];
  if (protocolCompare != NSOrderedSame)
    return protocolCompare;
    
  return [[item1 objectForKey:@"protocol"] compare:[item2 objectForKey:@"protocol"] options:NSCaseInsensitiveSearch];
}

// NetworkServicesClient implementation

- (void)availableServicesChanged:(NSNotification *)note
{
  // rebuild the submenu, leaving the first item
  while ([mServersSubmenu numberOfItems] > 1)
    [mServersSubmenu removeItemAtIndex:[mServersSubmenu numberOfItems] - 1];

  NetworkServices *netserv = [note object];

  NSEnumerator* keysEnumerator = [netserv serviceEnumerator];
  // build an array of dictionaries, so we can sort it

  NSMutableArray* servicesArray = [[NSMutableArray alloc] initWithCapacity:10];

  id key;
  while ((key = [keysEnumerator nextObject]))
  {
    NSDictionary* serviceDict = [NSDictionary dictionaryWithObjectsAndKeys:
      key, @"id",
      [netserv serviceName:[key intValue]], @"name",
      [netserv serviceProtocol:[key intValue]], @"protocol",
      nil];

    [servicesArray addObject:serviceDict];
  }

  if ([servicesArray count] == 0)
  {
    // add a separator
    [mServersSubmenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* newItem = [mServersSubmenu addItemWithTitle:NSLocalizedString(@"NoServicesFound", @"") action:nil keyEquivalent:@""];
    [newItem setTag:-1];
    [newItem setTarget:self];
  }
  else
  {
    // add a separator
    [mServersSubmenu addItem:[NSMenuItem separatorItem]];
    
    // sort on protocol, then name
    [servicesArray sortUsingFunction:SortByProtocolAndName context:NULL];
    
    unsigned count = [servicesArray count];
    for (unsigned int i = 0; i < count; i ++)
    {
      NSDictionary* serviceDict = [servicesArray objectAtIndex:i];
      NSString* itemName = [[serviceDict objectForKey:@"name"] stringByAppendingString:NSLocalizedString([serviceDict objectForKey:@"protocol"], @"")];
      
      id newItem = [mServersSubmenu addItemWithTitle:itemName action:@selector(connectToServer:) keyEquivalent:@""];
      [newItem setTag:[[serviceDict objectForKey:@"id"] intValue]];
      [newItem setTarget:self];
    }
  }
  // when you alloc, you've got to release . . .
  [servicesArray release];
}

- (void)serviceResolved:(NSNotification *)note
{
  NSDictionary *dict = [note userInfo];
  if ([dict objectForKey:NetworkServicesClientKey] == self)
    [self openNewWindowOrTabWithURL:[dict objectForKey:NetworkServicesResolvedURLKey] andReferrer:nil];
}

//
// handles resolution failure for everybody else
//
- (void)serviceResolutionFailed:(NSNotification *)note
{
  NSDictionary *dict = [note userInfo];
  NSString* serviceName = [dict objectForKey:NetworkServicesServiceKey];
  NSBeginAlertSheet(NSLocalizedString(@"ServiceResolutionFailedTitle", @""),
                    @"",               // default button
                    nil,               // cancel buttton
                    nil,               // other button
                    [NSApp mainWindow],                // window
                    nil,               // delegate
                    nil,               // end sel
                    nil,               // dismiss sel
                    NULL,              // context
                    [NSString stringWithFormat:NSLocalizedString(@"ServiceResolutionFailedMsgFormat", @""), serviceName]
                    );
}

-(IBAction) connectToServer:(id)aSender
{
  [[NetworkServices sharedNetworkServices] attemptResolveService:[aSender tag] forSender:self];
}

-(IBAction) aboutServers:(id)aSender
{
  NSString *pageToLoad = NSLocalizedStringFromTable(@"RendezvousPageDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"RendezvousPageDefault"])
    [self openNewWindowOrTabWithURL:pageToLoad andReferrer:nil];
}

// currently unused
- (void)pumpGeckoEventQueue
{
  nsCOMPtr<nsIEventQueueService> service = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
  if (!service) return;
  
  nsCOMPtr<nsIEventQueue> queue;
  service->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
  if (queue)
    queue->ProcessPendingEvents();
}

@end
