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

#import "NSString+Utils.h"

#import "BrowserWindowController.h"

#import "CHBrowserWrapper.h"
#import "CHIconTabViewItem.h"
#import "CHPreferenceManager.h"
#import "BookmarksDataSource.h"
#import "CHHistoryDataSource.h"
#import "CHExtendedTabView.h"
#import "CHUserDefaults.h"

#include "nsIWebNavigation.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIPrefBranch.h"
#include "nsIContextMenuListener.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsCocoaBrowserService.h"
#include "nsString.h"
#include "nsCRT.h"
#include "CHGeckoUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserChrome.h"

#include "nsIClipboardCommands.h"
#include "nsIWebBrowser.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManagerUtils.h"

#include <QuickTime/QuickTime.h>

static NSString *BrowserToolbarIdentifier	= @"Browser Window Toolbar";
static NSString *BackToolbarItemIdentifier	= @"Back Toolbar Item";
static NSString *ForwardToolbarItemIdentifier	= @"Forward Toolbar Item";
static NSString *ReloadToolbarItemIdentifier	= @"Reload Toolbar Item";
static NSString *StopToolbarItemIdentifier	= @"Stop Toolbar Item";
static NSString *HomeToolbarItemIdentifier	= @"Home Toolbar Item";
static NSString *LocationToolbarItemIdentifier	= @"Location Toolbar Item";
static NSString *SidebarToolbarItemIdentifier	= @"Sidebar Toolbar Item";
static NSString *PrintToolbarItemIdentifier	= @"Print Toolbar Item";
static NSString *ThrobberToolbarItemIdentifier = @"Throbber Toolbar Item";
static NSString *SearchToolbarItemIdentifier = @"Search Toolbar Item";

static NSString *NavigatorWindowFrameSaveName = @"NavigatorWindow";

// Cached toolbar defaults read in from a plist. If null, we'll use
// hardcoded defaults.
static NSArray* sToolbarDefaults = nil;


@interface BrowserWindowController(Private)
- (void)setupToolbar;
- (void)setupSidebarTabs;
@end

@implementation BrowserWindowController

-(void)enterModalSession
{
    mModalSession = [NSApp beginModalSessionForWindow: [self window]];
    [NSApp runModalSession: mModalSession];
    [NSApp endModalSession: mModalSession];
    mModalSession = nil;
}

- (BOOL)isResponderGeckoView:(NSResponder*) responder
{
  return ([responder isKindOfClass:[NSView class]] &&
          [(NSView*)responder isDescendantOf:[mBrowserView getBrowserView]]);
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
  BOOL windowWithMultipleTabs = ([mTabBrowser numberOfTabViewItems] > 1);
  // When this window gets focus, fix the Close Window modifiers depending
  // on whether we have multiple tabs
  [[NSApp delegate] adjustCloseTabMenuItemKeyEquivalent:windowWithMultipleTabs];
  [[NSApp delegate] adjustCloseWindowMenuItemKeyEquivalent:windowWithMultipleTabs];

  if ([self isResponderGeckoView:[[self window] firstResponder]]) {
    CHBrowserView* browserView = [mBrowserView getBrowserView];
    if (browserView)
      [browserView setActive:YES];
  }
}

- (void)windowDidResignKey:(NSNotification *)notification
{
  // when we are no longer the key window, set the Close shortcut back
  // to Command-W, for other windows.
  [[NSApp delegate] adjustCloseTabMenuItemKeyEquivalent:NO];
  [[NSApp delegate] adjustCloseWindowMenuItemKeyEquivalent:NO];

  if ([self isResponderGeckoView:[[self window] firstResponder]]) {
    CHBrowserView* browserView = [mBrowserView getBrowserView];
    if (browserView)
      [browserView setActive:NO];
  }
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
  // we have to manually enable/disable the bookmarks menu items, because we
  // turn autoenabling off for that menu
  [[NSApp delegate] adjustBookmarksMenuItemsEnabling:YES];
}

- (void)windowDidResignMain:(NSNotification *)notification
{
  // we have to manually enable/disable the bookmarks menu items, because we
  // turn autoenabling off for that menu
  [[NSApp delegate] adjustBookmarksMenuItemsEnabling:NO];
}


-(void)mouseMoved:(NSEvent*)aEvent
{
    if (mMoveReentrant)
        return;
        
    mMoveReentrant = YES;
    NSView* view = [[[self window] contentView] hitTest: [aEvent locationInWindow]];
    [view mouseMoved: aEvent];
    [super mouseMoved: aEvent];
    mMoveReentrant = NO;
}

- (id)initWithWindowNibName:(NSString *)windowNibName
{
    if ( (self = [super initWithWindowNibName:(NSString *)windowNibName]) ) {
        // this won't correctly cascade windows on multiple monitors. RADAR bug 2972893 
        // filed since it also happens in Terminal.app
        if ( nsCocoaBrowserService::sNumBrowsers == 0 )
            [self setShouldCascadeWindows:NO];
        else
            [self setShouldCascadeWindows:YES];
        mInitialized = NO;
        mMoveReentrant = NO;
        mShouldAutosave = YES;
        mShouldLoadHomePage = YES;
        mChromeMask = 0;
        mContextMenuFlags = 0;
        mContextMenuEvent = nsnull;
        mContextMenuNode = nsnull;
        mThrobberImages = nil;
        mThrobberHandler = nil;
    }
    return self;
}

-(void)autosaveWindowFrame
{
  if (mShouldAutosave)
    [[self window] saveFrameUsingName: NavigatorWindowFrameSaveName];
}

-(void)disableAutosave
{
  mShouldAutosave = NO;
}

-(void)disableLoadPage
{
  mShouldLoadHomePage = NO;
}

- (void)windowWillClose:(NSNotification *)notification
{
#if DEBUG
  NSLog(@"Window will close notification.");
#endif
  [mSidebarBookmarksDataSource windowClosing];

  [self autosaveWindowFrame];
  [self autorelease];
}

- (void)dealloc
{
#if DEBUG
  NSLog(@"Browser controller died.");
#endif

  // Loop over all tabs, and tell them that the window is closed.
  int numTabs = [mTabBrowser numberOfTabViewItems];
  for (int i = 0; i < numTabs; i++) {
    NSTabViewItem* item = [mTabBrowser tabViewItemAtIndex: i];
    [[item view] windowClosed];
  }
  
  //if (mSidebarBrowserView)
  //  [mSidebarBrowserView windowClosed];

  [mProgress release];

  [self stopThrobber];
  [mThrobberImages release];
  
  [super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    // hide the resize control if specified by the chrome mask
    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) )
      [[self window] setShowsResizeIndicator:NO];
    
    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR) ) {
      // remove the status bar at the bottom and adjust the height of the content area. We
      // have to turn off the autoresizing min flag to correctly pull the tab browser to
      // the bottom of the window while we're resizing it.
      float height = [mStatusBar frame].size.height;
      [mStatusBar removeFromSuperview];
      int mask = [mTabBrowser autoresizingMask];
      [mTabBrowser setAutoresizingMask:(mask & !NSViewMinYMargin)];
      [mTabBrowser setFrame:NSMakeRect([mTabBrowser frame].origin.x, [mTabBrowser frame].origin.y - height,
                               [mTabBrowser frame].size.width, [mTabBrowser frame].size.height + height)];
      [mTabBrowser setAutoresizingMask:mask];

      // clear out everything in the status bar we were holding on to. This will cause us to
      // pass nil for these status items into the CHBrowserwWrapper which is what we want. We'll
      // crash if we give them things that have gone away.
      mProgress = nil;
      mStatus = nil;
      mLock = nil;
    }
    else {
      // Retain with a single extra refcount.  This allows the CHBrowserWrappers
      // to remove the progress meter from its superview without having to 
      // worry about retaining and releasing it.
      [mProgress retain];
    }

    // Get our saved dimensions.
    [[self window] setFrameUsingName: NavigatorWindowFrameSaveName];
    
    if (mModalSession)
      [NSApp stopModal];
      
    mInitialized = YES;

    mDrawerCachedFrame = NO;
        
    [[self window] setAcceptsMouseMovedEvents: YES];
    
    [self setupToolbar];
    
//  03/03/2002 mlj Changing strategy a bit here.  The addTab: method was
//  duplicating a lot of the code found here.  I have moved it to that method.
//  We now remove the IB tab, then add one of our own.

    [mTabBrowser removeTabViewItem:[mTabBrowser tabViewItemAtIndex:0]];
    
    // create ourselves a new tab and fill it with the appropriate content. If we
    // have a URL pending to be opened here, don't load anything in it, otherwise,
    // load the homepage if that's what the user wants (or about:blank).
    [self newTab:(!mPendingURL && mShouldLoadHomePage)];
    
    // we have a url "pending" from the "open new window with link" command. Deal
    // with it now that everything is loaded.
    if (mPendingURL) {
      if (mShouldLoadHomePage)
        [self loadURL: mPendingURL referrer:mPendingReferrer];
      [mPendingURL release];
      [mPendingReferrer release];
      mPendingURL = mPendingReferrer = nil;
    }
    
    [mSidebarDrawer setDelegate: self];

    [self setupSidebarTabs];

    [mPersonalToolbar initializeToolbar];
    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR) ) {
      // remove the personal toolbar and adjust the content area upwards. Removing it
      // from the parent view releases it, so we have to clear out the member var.
      float height = [mPersonalToolbar frame].size.height;
      [mPersonalToolbar removeFromSuperview];
      [mTabBrowser setFrame:NSMakeRect([mTabBrowser frame].origin.x, [mTabBrowser frame].origin.y,
                               [mTabBrowser frame].size.width, [mTabBrowser frame].size.height + height)];
      mPersonalToolbar = nil;      
    }
    else if (![self shouldShowBookmarkToolbar]) {
      [mPersonalToolbar showBookmarksToolbar:NO];
    }
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
	//if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) )
  //  return [[self window] frame].size;
	return proposedFrameSize;
}

- (void)drawerWillOpen: (NSNotification*)aNotification
{
  [mSidebarBookmarksDataSource ensureBookmarks];
  
  [mHistoryDataSource ensureDataSourceLoaded];

  // Force the window to shrink and move if necessary in order to accommodate the sidebar.
  NSRect screenFrame = [[[self window] screen] visibleFrame];
  NSRect windowFrame = [[self window] frame];
  NSSize drawerSize = [mSidebarDrawer contentSize];
  int fudgeFactor = 12; // Not sure how to get the drawer's border info, so we fudge it for now.
  drawerSize.width += fudgeFactor;
  if (windowFrame.origin.x + windowFrame.size.width + drawerSize.width >
       screenFrame.origin.x + screenFrame.size.width) {
    // We need to adjust the window so that it can fit.
    float shrinkDelta = (windowFrame.size.width + drawerSize.width) - screenFrame.size.width;
    if (shrinkDelta < 0) shrinkDelta = 0;
    float newWidth = (windowFrame.size.width - shrinkDelta);
    float newPosition = screenFrame.size.width - newWidth - drawerSize.width;
    if (newPosition < 0) newPosition = 0;
    mCachedFrameBeforeDrawerOpen = windowFrame;
    windowFrame.origin.x = newPosition;
    windowFrame.size.width = newWidth;
    mCachedFrameAfterDrawerOpen = windowFrame;
    [[self window] setFrame: windowFrame display: YES];
    mDrawerCachedFrame = YES;
  }
}

- (void)drawerDidOpen:(NSNotification *)aNotification
{
  // XXXdwh This is temporary.
  //  [[mSidebarBrowserView getBrowserView] loadURI: @"http://tinderbox.mozilla.org/SeaMonkey/panel.html" referrer: nil flags:NSLoadFlagsNone];

  // Toggle the sidebar icon.
  if(mSidebarToolbarItem)
    [mSidebarToolbarItem setImage:[NSImage imageNamed:@"sidebarOpened"]];
}

- (void)drawerDidClose:(NSNotification *)aNotification
{
  // Unload the Gecko web page in "My Panels" to save memory.
  if(mSidebarToolbarItem)
    [mSidebarToolbarItem setImage:[NSImage imageNamed:@"sidebarClosed"]];

  // XXXdwh ignore for now.
  //  [[mSidebarBrowserView getBrowserView] loadURI: @"about:blank" referrer:nil flags:NSLoadFlagsNone];

  if (mDrawerCachedFrame) {
    mDrawerCachedFrame = NO;
    NSRect frame = [[self window] frame];
    if (frame.origin.x == mCachedFrameAfterDrawerOpen.origin.x &&
        frame.origin.y == mCachedFrameAfterDrawerOpen.origin.y &&
        frame.size.width == mCachedFrameAfterDrawerOpen.size.width &&
        frame.size.height == mCachedFrameAfterDrawerOpen.size.height) {
#if 0
      printf("Got here too.\n");
      printf("Xes are %f %f\n", frame.origin.x, mCachedFrameAfterDrawerOpen.origin.x);
      printf("Widths are %f %f\n", frame.size.width, mCachedFrameAfterDrawerOpen.size.width);
#endif
      // Restore the original frame.
      [[self window] setFrame: mCachedFrameBeforeDrawerOpen display: YES];
    }
  }
}

- (void)setupToolbar
{
  if ( !mChromeMask || (mChromeMask & nsIWebBrowserChrome::CHROME_TOOLBAR) ) {  
    NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:BrowserToolbarIdentifier] autorelease];
    
    [toolbar setDisplayMode:NSToolbarDisplayModeDefault];
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:YES];
    [toolbar setDelegate:self];
    [[self window] setToolbar:toolbar];
  }
}


//
// toolbarWillAddItem: (toolbar delegate method)
//
// Called when a button is about to be added to a toolbar. This is where we should
// cache items we may need later. For instance, we want to hold onto the sidebar
// toolbar item so we can change it when the drawer opens and closes.
//
- (void)toolbarWillAddItem:(NSNotification *)notification
{
  NSToolbarItem* item = [[notification userInfo] objectForKey:@"item"];
  if ( [[item itemIdentifier] isEqual:SidebarToolbarItemIdentifier] )
    mSidebarToolbarItem = item;
}

//
// toolbarWillAddItem: (toolbar delegate method)
//
// Called when a button is about to be removed from a toolbar. This is where we should
// uncache items so we don't access them after they're gone. For instance, we want to
// clear our ref to the sidebar toolbar item.
//
- (void)toolbarDidRemoveItem:(NSNotification *)notification
{
  NSToolbarItem* item = [[notification userInfo] objectForKey:@"item"];
  if ( [[item itemIdentifier] isEqual:SidebarToolbarItemIdentifier] )
    mSidebarToolbarItem = nil;
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:   BackToolbarItemIdentifier,
                                        ForwardToolbarItemIdentifier,
                                        ReloadToolbarItemIdentifier,
                                        StopToolbarItemIdentifier,
                                        HomeToolbarItemIdentifier,
                                        LocationToolbarItemIdentifier,
                                        SidebarToolbarItemIdentifier,
                                        ThrobberToolbarItemIdentifier,
                                        SearchToolbarItemIdentifier,
                                        PrintToolbarItemIdentifier,
                                        NSToolbarCustomizeToolbarItemIdentifier,
                                        NSToolbarFlexibleSpaceItemIdentifier,
                                        NSToolbarSpaceItemIdentifier,
                                        NSToolbarSeparatorItemIdentifier,
                                        nil];
}


//
// + toolbarDefaults
//
// Parse a plist called "ToolbarDefaults.plist" in our Resources subfolder. This
// allows anyone to easily customize the default set w/out having to recompile. We
// hold onto the list for the duration of the app to avoid reparsing it every
// time.
//
+ (NSArray*) toolbarDefaults
{
  if ( !sToolbarDefaults ) {
    sToolbarDefaults = [NSArray arrayWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ToolbarDefaults" ofType:@"plist"]];
    [sToolbarDefaults retain];
  }
  return sToolbarDefaults;
}


- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
  // try to get the defaults from the plist, but if not, hardcode something so
  // the user always has a toolbar.
  NSArray* defaults = [BrowserWindowController toolbarDefaults];
  NS_ASSERTION(defaults, "Couldn't load toolbar defaults from plist");
  return ( defaults ? defaults : [NSArray arrayWithObjects:   BackToolbarItemIdentifier,
                                        ForwardToolbarItemIdentifier,
                                        ReloadToolbarItemIdentifier,
                                        StopToolbarItemIdentifier,
                                        LocationToolbarItemIdentifier,
                                        SidebarToolbarItemIdentifier,
                                        nil] );
}

// XXX use a dictionary to speed up the following?

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar
      itemForItemIdentifier:(NSString *)itemIdent
  willBeInsertedIntoToolbar:(BOOL)willBeInserted
{
    NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdent] autorelease];
    if ( [itemIdent isEqual:BackToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Back"];
        [toolbarItem setPaletteLabel:@"Go Back"];
        [toolbarItem setToolTip:@"Go back one page"];
        [toolbarItem setImage:[NSImage imageNamed:@"back"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(back:)];
    } else if ( [itemIdent isEqual:ForwardToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Forward"];
        [toolbarItem setPaletteLabel:@"Go Forward"];
        [toolbarItem setToolTip:@"Go forward one page"];
        [toolbarItem setImage:[NSImage imageNamed:@"forward"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(forward:)];
    } else if ( [itemIdent isEqual:ReloadToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Reload"];
        [toolbarItem setPaletteLabel:@"Reload Page"];
        [toolbarItem setToolTip:@"Reload current page"];
        [toolbarItem setImage:[NSImage imageNamed:@"reload"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(reload:)];
    } else if ( [itemIdent isEqual:StopToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Stop"];
        [toolbarItem setPaletteLabel:@"Stop Loading"];
        [toolbarItem setToolTip:@"Stop loading this page"];
        [toolbarItem setImage:[NSImage imageNamed:@"stop"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(stop:)];
    } else if ( [itemIdent isEqual:HomeToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Home"];
        [toolbarItem setPaletteLabel:@"Go Home"];
        [toolbarItem setToolTip:@"Go to home page"];
        [toolbarItem setImage:[NSImage imageNamed:@"home"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(home:)];
    } else if ( [itemIdent isEqual:SidebarToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Sidebar"];
        [toolbarItem setPaletteLabel:@"Toggle Sidebar"];
        [toolbarItem setToolTip:@"Show or hide the Sidebar"];
        [toolbarItem setImage:[NSImage imageNamed:@"sidebarClosed"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(toggleSidebar:)];
    } else if ( [itemIdent isEqual:SearchToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Search"];
        [toolbarItem setPaletteLabel:@"Search"];
        [toolbarItem setToolTip:@"Search the Internet"];
        [toolbarItem setImage:[NSImage imageNamed:@"saveShowFile.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(performSearch)];
    } else if ( [itemIdent isEqual:ThrobberToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@""];
        [toolbarItem setPaletteLabel:@"Progress"];
        [toolbarItem setToolTip:NSLocalizedStringFromTable(@"ThrobberPageDefault", @"WebsiteDefaults", nil)];
        [toolbarItem setImage:[NSImage imageNamed:@"throbber-01"]];
        [toolbarItem setTarget:self];
        [toolbarItem setTag:'Thrb'];
        [toolbarItem setAction:@selector(clickThrobber:)];
    } else if ( [itemIdent isEqual:LocationToolbarItemIdentifier] ) {
        
        NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];
        
        [toolbarItem setLabel:@"Location"];
        [toolbarItem setPaletteLabel:@"Location"];
        [toolbarItem setImage:[NSImage imageNamed:@"Enter a web location."]];
        [toolbarItem setView:mLocationToolbarView];
        [toolbarItem setMinSize:NSMakeSize(128,32)];
        [toolbarItem setMaxSize:NSMakeSize(2560,32)];
        
        [menuFormRep setTarget:self];
        [menuFormRep setAction:@selector(performAppropriateLocationAction)];
        [menuFormRep setTitle:[toolbarItem label]];
        
        [toolbarItem setMenuFormRepresentation:menuFormRep];

        mLocationToolbarItem = toolbarItem;

    } else if ( [itemIdent isEqual:PrintToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Print"];
        [toolbarItem setPaletteLabel:@"Print"];
        [toolbarItem setToolTip:@"Print this page"];
        [toolbarItem setImage:[NSImage imageNamed:@"print"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(printDocument)];
    } else {
        toolbarItem = nil;
    }
    
    return toolbarItem;
}

// This method handles the enabling/disabling of the toolbar buttons.
- (BOOL)validateToolbarItem:(NSToolbarItem *)theItem
{
  // Check the action and see if it matches.
  SEL action = [theItem action];
  //NSLog(@"Validating toolbar item %@ with selector %s", [theItem label], action);
  if (action == @selector(back:))
    return [[mBrowserView getBrowserView] canGoBack];
  else if (action == @selector(forward:))
    return [[mBrowserView getBrowserView] canGoForward];
  else if (action == @selector(reload:))
    return [mBrowserView isBusy] == NO;
  else if (action == @selector(stop:))
    return [mBrowserView isBusy];
  else
    return YES;
}
   
- (void)updateToolbarItems
{
    [[[self window] toolbar] validateVisibleItems];
}

- (void)performAppropriateLocationAction
{
  NSToolbar *toolbar = [[self window] toolbar];
  if ( [toolbar isVisible] ) {
    if ( ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconAndLabel) ||
          ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconOnly) ) {
      NSArray *itemsWeCanSee = [toolbar visibleItems];
      
      for (unsigned int i=0;i<[itemsWeCanSee count];i++) {
        if ([[[itemsWeCanSee objectAtIndex:i] itemIdentifier] isEqual:LocationToolbarItemIdentifier]) {
          [self focusURLBar];
          return;
        }
      }
    }
  }
  
  [self beginLocationSheet];
}

- (void)focusURLBar
{
  [[self window] makeFirstResponder:mURLBar];
	[mURLBar selectAll: self];
}

- (void)beginLocationSheet
{
    [NSApp beginSheet:  mLocationSheetWindow
       modalForWindow:  [self window]
        modalDelegate:  nil
       didEndSelector:  nil
          contextInfo:  nil];
}

- (IBAction)endLocationSheet:(id)sender
{
  [mLocationSheetWindow orderOut:self];
  [NSApp endSheet:mLocationSheetWindow returnCode:1];
  [self loadURL:[mLocationSheetURLField stringValue] referrer:nil];
  
  // Focus and activate our content area.
  [[mBrowserView getBrowserView] setActive: YES];
}

- (IBAction)cancelLocationSheet:(id)sender
{
  [mLocationSheetWindow orderOut:self];
  [NSApp endSheet:mLocationSheetWindow returnCode:0];
}

-(IBAction)cancelAddBookmarkSheet:(id)sender
{
  [mAddBookmarkSheetWindow orderOut:self];
  [NSApp endSheet:mAddBookmarkSheetWindow returnCode:0];
  [mCachedBMDS endAddBookmark: 0];
}

-(IBAction)endAddBookmarkSheet:(id)sender
{
  [mAddBookmarkSheetWindow orderOut:self];
  [NSApp endSheet:mAddBookmarkSheetWindow returnCode:0];
  [mCachedBMDS endAddBookmark: 1];
}

- (void)cacheBookmarkDS: (id)aDS
{
  mCachedBMDS = aDS;
}

-(IBAction)manageBookmarks: (id)aSender
{
  if ([mSidebarDrawer state] == NSDrawerClosedState)
    [self toggleSidebar: self];

  [mSidebarTabView selectFirstTabViewItem:self];
}

- (void)importBookmarks: (NSString*)aURLSpec
{
  // Open the bookmarks sidebar.
  [self manageBookmarks: self];

  // Now do the importing.
  CHBrowserWrapper* newView = [[[CHBrowserWrapper alloc] initWithTab: nil andWindow: [self window]] autorelease];
  [newView setFrame: NSZeroRect];
  [newView setIsBookmarksImport: YES];
  [[[self window] contentView] addSubview: newView];
  [[newView getBrowserView] loadURI:aURLSpec referrer: nil flags:NSLoadFlagsNone];
}

- (IBAction)goToLocationFromToolbarURLField:(id)sender
{
  // trim off any whitespace around url
  NSMutableString *theURL = [[NSMutableString alloc] initWithString:[sender string]];
  CFStringTrimWhitespace((CFMutableStringRef)theURL);
  [self loadURL:theURL referrer:nil];
  [theURL release];
    
  // Focus and activate our content area.
  [[mBrowserView getBrowserView] setActive: YES];
}

- (void)saveDocument: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
{
  [[mBrowserView getBrowserView] saveDocument: aFilterView filterList: aFilterList];
}

- (void)saveURL: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
            url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename
{
  [[mBrowserView getBrowserView] saveURL: aFilterView filterList: aFilterList
                                     url: aURLSpec suggestedFilename: aFilename];
}

- (IBAction)viewSource:(id)aSender
{
  NSString* urlStr = [[mBrowserView getBrowserView] getFocusedURLString];
  NSString* viewSource = [@"view-source:" stringByAppendingString: urlStr];

  PRBool loadInBackground;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);
  [self openNewTabWithURL: viewSource referrer:nil loadInBackground: loadInBackground];
}

- (void)printDocument
{
  [[mBrowserView getBrowserView] printDocument];
}

- (void)printPreview
{
  NS_WARNING("Print Preview stopped in BrowserWindowController, not implemented");
  //XXX there is no printPreview on CHBrowserView...so this isn't implemented
  //[[mBrowserView getBrowserView] printPreview];
}

- (void)performSearch
{
  NSString *searchEngine = NSLocalizedStringFromTable(@"SearchPageDefault", @"WebsiteDefaults", nil);

  // Get the users preferred search engine from IC
  if (!searchEngine || [searchEngine isEqualToString:@"SearchPageDefault"]) {
    searchEngine = [[CHPreferenceManager sharedInstance] getICStringPref:kICWebSearchPagePrefs];
      if (!searchEngine || ([searchEngine length] == 0))
        searchEngine = @"http://dmoz.org/";
  }

  [[mBrowserView getBrowserView] loadURI:searchEngine  referrer: nil flags:NSLoadFlagsNone];
}


- (NSToolbarItem*)throbberItem
{
    // find our throbber toolbar item.
    NSToolbar* toolbar = [[self window] toolbar];
    NSArray* items = [toolbar visibleItems];
    unsigned count = [items count];
    for (unsigned i = 0; i < count; ++i) {
        NSToolbarItem* item = [items objectAtIndex: i];
        if ([item tag] == 'Thrb') {
            return item;
        }
    }
    return nil;
}

- (NSArray*)throbberImages
{
  // Simply load an array of NSImage objects from the files "throbber-NN.tif". I used "Quicktime Player" to
  // save all of the frames of the animated gif as individual .tif files for simplicity of implementation.
  if (mThrobberImages == nil) {
    NSImage* images[64];
    int i;
    for (i = 0;; ++i) {
      NSString* imageName = [NSString stringWithFormat: @"throbber-%02d", i + 1];
      images[i] = [NSImage imageNamed: imageName];
      if (images[i] == nil)
        break;
    }
    mThrobberImages = [[NSArray alloc] initWithObjects: images count: i];
  }
  return mThrobberImages;
}


- (void)clickThrobber:(id)aSender
{
  NSString *pageToLoad = NSLocalizedStringFromTable(@"ThrobberPageDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"ThrobberPageDefault"])
    [self loadURL:pageToLoad referrer:nil];
}

- (void)startThrobber
{
  // optimization:  only throb if the throbber toolbar item is visible.
  NSToolbarItem* throbberItem = [self throbberItem];
  if (throbberItem) {
    [self stopThrobber];
    mThrobberHandler = [[ThrobberHandler alloc] initWithToolbarItem:throbberItem 
                          images:[self throbberImages]];
  }
}

- (void)stopThrobber
{
  [mThrobberHandler stopThrobber];
  [mThrobberHandler release];
  mThrobberHandler = nil;
  [[self throbberItem] setImage: [[self throbberImages] objectAtIndex: 0]];
}


- (BOOL)findInPageWithPattern:(NSString*)text caseSensitive:(BOOL)inCaseSensitive
    wrap:(BOOL)inWrap backwards:(BOOL)inBackwards
{
  return [[mBrowserView getBrowserView] findInPageWithPattern:text caseSensitive:inCaseSensitive
    wrap:inWrap backwards:inBackwards];
}

- (void)addBookmarkExtended: (BOOL)aIsFromMenu isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle
{
  [mSidebarBookmarksDataSource ensureBookmarks];
  BOOL useSel = aIsFromMenu;
  if (aIsFromMenu) {
    // Use selection only if the sidebar is open and the bookmarks panel is displaying.
    if ([mSidebarDrawer state] == NSDrawerClosedState)
      useSel = NO;
    else
      if ([mSidebarTabView tabViewItemAtIndex: 0] != [mSidebarTabView selectedTabViewItem])
        useSel = NO;
  }
  
  [mSidebarBookmarksDataSource addBookmark: self useSelection: useSel isFolder: aIsFolder URL:aURL title:aTitle];
}


- (IBAction)bookmarkPage: (id)aSender
{
  [self addBookmarkExtended:YES isFolder:NO URL:nil title:nil];
}


- (IBAction)bookmarkLink: (id)aSender
{
  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  CHGeckoUtils::GetEnclosingLinkElementAndHref(mContextMenuNode, getter_AddRefs(linkContent), href);
  nsAutoString linkText;
  CHGeckoUtils::GatherTextUnder(linkContent, linkText);
  NSString* urlStr = [NSString stringWith_nsAString:href];
  NSString* titleStr = [NSString stringWith_nsAString:linkText];
  [self addBookmarkExtended:YES isFolder:NO URL:urlStr title:titleStr];
}

- (IBAction)back:(id)aSender
{
  [[mBrowserView getBrowserView] goBack];
}

- (IBAction)forward:(id)aSender
{
  [[mBrowserView getBrowserView] goForward];
}

- (IBAction)reload:(id)aSender
{
  [[mBrowserView getBrowserView] reload: 0];
}

- (IBAction)stop:(id)aSender
{
  [[mBrowserView getBrowserView] stop: nsIWebNavigation::STOP_ALL];
}

- (IBAction)home:(id)aSender
{
  [[mBrowserView getBrowserView] loadURI:[[CHPreferenceManager sharedInstance] homePage:NO] referrer: nil flags:NSLoadFlagsNone];
}

- (IBAction)toggleSidebar:(id)aSender
{
    NSResponder* resp = [[self window] firstResponder];
    [[self window] makeFirstResponder: nil];
    
    if ( ([mSidebarDrawer state] == NSDrawerClosedState) || ([mSidebarDrawer state] == NSDrawerClosingState) ) {
        // XXXHack to bypass sidebar crashes.
        [mSidebarDrawer openOnEdge: NSMaxXEdge];
    } else {
        [mSidebarDrawer close];
    }
    
    [[self window] makeFirstResponder: resp];
}

-(void)loadURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer
{
    if (mInitialized) {
        [[mBrowserView getBrowserView] loadURI:aURLSpec referrer:aReferrer flags:NSLoadFlagsNone];
    }
    else {
        // we haven't yet initialized all the browser machinery, stash the url and referrer
        // until we're ready in windowDidLoad:
        mPendingURL = aURLSpec;
        [mPendingURL retain];
        mPendingReferrer = aReferrer;
        [mPendingReferrer retain];
    }
}

- (void)updateLocationFields:(NSString *)locationString
{
    if ( [locationString isEqual:@"about:blank"] ) 			// don't show about:blank to users
      locationString = @"";
    [mURLBar setString:locationString];
    [mLocationSheetURLField setStringValue:locationString];

    // don't call [window display] here, no matter how much you might want
    // to, because it forces a redraw of every view in the window and with a lot
    // of tabs, it's dog slow.
    // [[self window] display];
}

-(void)newTab:(BOOL)allowHomepage
{
    NSTabViewItem* newTab = [[[NSTabViewItem alloc] initWithIdentifier: nil] autorelease];
    CHBrowserWrapper* newView = [[[CHBrowserWrapper alloc] initWithTab: newTab andWindow: [mTabBrowser window]] autorelease];

    PRInt32 newTabPage = 0;
    if (allowHomepage) {
      nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
      pref->GetIntPref("browser.tabs.startPage", &newTabPage);
    }

    [newTab setLabel: ((newTabPage == 1) ? NSLocalizedString(@"TabLoading", @"") : NSLocalizedString(@"UntitledPageTitle", @""))];
    [newTab setView: newView];
    [mTabBrowser addTabViewItem: newTab];

    if (allowHomepage)
      [[newView getBrowserView] loadURI: ((newTabPage == 1) ? [[CHPreferenceManager sharedInstance] homePage: NO] : @"about:blank") referrer:nil flags:NSLoadFlagsNone];

    [mTabBrowser selectLastTabViewItem: self];

    if ( [[[self window] toolbar] isVisible] ) {
        if ( ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconAndLabel) ||
             ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconOnly) ) {
          if ([mTabBrowser numberOfTabViewItems] > 1) {
            [self focusURLBar];
            [[mBrowserView getBrowserView] setActive: NO];
          }
          else if ([[self window] isKeyWindow])
            [[mBrowserView getBrowserView] setActive: YES];
          else
            [[mBrowserView getBrowserView] setActive: NO];
        }
    }
}

-(void)closeTab
{
  if ( [mTabBrowser numberOfTabViewItems] > 1 ) {
    [[[mTabBrowser selectedTabViewItem] view] windowClosed];
    [mTabBrowser removeTabViewItem:[mTabBrowser selectedTabViewItem]];
  }
}

- (void)previousTab
{
  if ([mTabBrowser indexOfTabViewItem:[mTabBrowser selectedTabViewItem]] == 0)
    [mTabBrowser selectLastTabViewItem:self];
  else
    [mTabBrowser selectPreviousTabViewItem:self];
}

- (void)nextTab
{
  if ([mTabBrowser indexOfTabViewItem:[mTabBrowser selectedTabViewItem]] == [mTabBrowser numberOfTabViewItems] - 1)
    [mTabBrowser selectFirstTabViewItem:self];
  else
    [mTabBrowser selectNextTabViewItem:self];
}

- (void)tabView:(NSTabView *)aTabView didSelectTabViewItem:(NSTabViewItem *)aTabViewItem
{
    // Disconnect the old view, if one has been designated.
    // If the window has just been opened, none has been.
    if ( mBrowserView ) {
        [mBrowserView disconnectView];
    }
    // Connect up the new view
    mBrowserView = [aTabViewItem view];
       
    // Make the new view the primary content area.
    [mBrowserView makePrimaryBrowserView: mURLBar status: mStatus
        progress: mProgress windowController: self];
}

- (void)tabViewDidChangeNumberOfTabViewItems:(NSTabView *)aTabView
{
  [[NSApp delegate] fixCloseMenuItemKeyEquivalents];
}

-(id)getTabBrowser
{
  return mTabBrowser;
}

-(CHBrowserWrapper*)getBrowserWrapper
{
  return mBrowserView;
}

-(void)openNewWindowWithURL: (NSString*)aURLSpec referrer: (NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG
{
  // Autosave our dimensions before we open a new window.  That ensures the size ends up matching.
  [self autosaveWindowFrame];

  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];
  [browser loadURL: aURLSpec referrer:aReferrer];
  if (aLoadInBG)
    [[browser window] orderWindow: NSWindowBelow relativeTo: [[self window] windowNumber]];
  else {
    // Focus the content area and show the window.
    [browser enterModalSession];
    [[[browser getBrowserWrapper] getBrowserView] setActive: YES];
  }
}

-(void)openNewWindowWithGroup: (nsIDOMElement*)aFolderElement loadInBackground: (BOOL)aLoadInBG
{
  // Autosave our dimensions before we open a new window.  That ensures the size ends up matching.
  [self autosaveWindowFrame];

  // Tell the Tab Browser in the newly created window to load the group
  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];
  if (aLoadInBG)
    [[browser window] orderWindow: NSWindowBelow relativeTo: [[self window] windowNumber]];
  else {
    // Focus the content area and show the window.
    [browser enterModalSession];
    [[[browser getBrowserWrapper] getBrowserView] setActive: YES];
  }

  id tabBrowser = [browser getTabBrowser]; 
  [mSidebarBookmarksDataSource openBookmarkGroup: tabBrowser groupElement: aFolderElement];
}

-(void)openNewTabWithURL: (NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG
{
    NSTabViewItem* newTab = [[[NSTabViewItem alloc] initWithIdentifier: nil] autorelease];
    
    NSTabViewItem* selectedTab = [mTabBrowser selectedTabViewItem];
    int index = [mTabBrowser indexOfTabViewItem: selectedTab];
    [mTabBrowser insertTabViewItem: newTab atIndex: index+1];

    CHBrowserWrapper* newView = [[[CHBrowserWrapper alloc] initWithTab: newTab andWindow: [mTabBrowser window]] autorelease];
    [newView setTab: newTab];
    
    [newTab setLabel: NSLocalizedString(@"TabLoading", @"")];
    [newTab setView: newView];

    [[newView getBrowserView] loadURI:aURLSpec referrer:aReferrer flags:NSLoadFlagsNone];

    if (!aLoadInBG) {
      [mTabBrowser selectTabViewItem: newTab];
      // Focus the content area.
      [[newView getBrowserView] setActive: YES];
    }
}

-(void)setupSidebarTabs
{
    CHIconTabViewItem   *bookItem = [[CHIconTabViewItem alloc] initWithIdentifier:@"bookmarkSidebarCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"bookicon"]];
    CHIconTabViewItem   *histItem = [[CHIconTabViewItem alloc] initWithIdentifier:@"historySidebarCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"historyicon"]];
#if USE_SEARCH_ITEM
    CHIconTabViewItem *searchItem = [[CHIconTabViewItem alloc] initWithIdentifier:@"searchSidebarCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"searchicon"]];
#endif
#if USE_PANELS_ITEM
    CHIconTabViewItem *panelsItem = [[CHIconTabViewItem alloc] initWithIdentifier:@"myPanelsCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"panel_icon"]];
#endif
    
    [bookItem   setView:[[mSidebarSourceTabView tabViewItemAtIndex:0] view]];
    [histItem   setView:[[mSidebarSourceTabView tabViewItemAtIndex:1] view]];
#if USE_SEARCH_ITEM
    [searchItem setView:[[mSidebarSourceTabView tabViewItemAtIndex:2] view]];
#endif
#if USE_PANELS_ITEM
    [panelsItem setView:[[mSidebarSourceTabView tabViewItemAtIndex:3] view]];
#endif

    // remove default tab from nib
    [mSidebarTabView removeTabViewItem:[mSidebarTabView tabViewItemAtIndex:0]];
    
    // insert the tabs we want
    [mSidebarTabView insertTabViewItem:bookItem   atIndex:0];
    [mSidebarTabView insertTabViewItem:histItem   atIndex:1];
#if USE_SEARCH_ITEM
    [mSidebarTabView insertTabViewItem:searchItem atIndex:2];
#endif
#if USE_PANELS_ITEM
    [mSidebarTabView insertTabViewItem:panelsItem atIndex:3];
#endif
    
    BOOL showHistory = NO;
    nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
    if (pref) {
      PRBool historyPref = PR_FALSE;
      if (NS_SUCCEEDED(pref->GetBoolPref("chimera.show_history", &historyPref)))
        showHistory = historyPref ? YES : NO;
    }
    
    if (!showHistory)
      [mSidebarTabView removeTabViewItem:[mSidebarTabView tabViewItemAtIndex:1]];
      
    [mSidebarTabView selectFirstTabViewItem:self];
}

-(void)setChromeMask:(unsigned int)aMask
{
  mChromeMask = aMask;
}

-(unsigned int)chromeMask
{
  return mChromeMask;
}

-(void) biggerTextSize
{
  nsCOMPtr<nsIDOMWindow> contentWindow = getter_AddRefs([[mBrowserView getBrowserView] getContentWindow]);
  nsCOMPtr<nsIScriptGlobalObject> global(do_QueryInterface(contentWindow));
  if (!global)
    return;
  nsCOMPtr<nsIDocShell> docShell;
  global->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return;
  nsCOMPtr<nsIContentViewer> cv;
  docShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
  if (!markupViewer)
    return;
  float zoom;
  markupViewer->GetTextZoom(&zoom);
  if (zoom >= 20)
    return;
  
  zoom += 0.25;
  if (zoom > 20)
    zoom = 20;
  
  markupViewer->SetTextZoom(zoom);
}

-(void) smallerTextSize
{
  nsCOMPtr<nsIDOMWindow> contentWindow = getter_AddRefs([[mBrowserView getBrowserView] getContentWindow]);
  nsCOMPtr<nsIScriptGlobalObject> global(do_QueryInterface(contentWindow));
  if (!global)
    return;
  nsCOMPtr<nsIDocShell> docShell;
  global->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return;
  nsCOMPtr<nsIContentViewer> cv;
  docShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
  if (!markupViewer)
    return;
  float zoom;
  markupViewer->GetTextZoom(&zoom);
  if (zoom <= 0.01)
    return;

  zoom -= 0.25;
  if (zoom < 0.01)
    zoom = 0.01;

  markupViewer->SetTextZoom(zoom);
}

- (BOOL)shouldShowBookmarkToolbar
{
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults integerForKey:USER_DEFAULTS_HIDE_PERS_TOOLBAR_KEY] == 1)
     return NO;

  return YES;
}

-(id)getAddBookmarkSheetWindow
{
  return mAddBookmarkSheetWindow;
}

-(id)getAddBookmarkTitle
{
  return mAddBookmarkTitleField;
}

-(id)getAddBookmarkFolder
{
  return mAddBookmarkFolderField;
}

-(id)getAddBookmarkCheckbox
{
  return mAddBookmarkCheckbox;
}

// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode
{
  mContextMenuFlags = flags;
  mContextMenuNode = aNode;
  mContextMenuEvent = aEvent;
}

- (NSMenu*)getContextMenu
{
  NSMenu* result = nil;
  if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_LINK) != 0) {
    if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_IMAGE) != 0) {
      result = mImageLinkMenu;
    }
    else
      result = mLinkMenu;
  }
  else if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_INPUT) != 0 ||
           (mContextMenuFlags & nsIContextMenuListener::CONTEXT_TEXT) != 0) {
    result = mInputMenu;
  }
  else if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_IMAGE) != 0) {
    result = mImageMenu;
  }
  else if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_DOCUMENT) != 0) {
    result = mPageMenu;
    [mBackItem setEnabled: [[mBrowserView getBrowserView] canGoBack]];
    [mForwardItem setEnabled: [[mBrowserView getBrowserView] canGoForward]];
  }
  
  return result;
}

// Context menu methods
- (IBAction)openLinkInNewWindow:(id)aSender
{
  [self openLinkInNewWindowOrTab: YES];
}

- (IBAction)openLinkInNewTab:(id)aSender
{
  [self openLinkInNewWindowOrTab: NO];
}

-(void)openLinkInNewWindowOrTab: (BOOL)aUseWindow
{
  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  CHGeckoUtils::GetEnclosingLinkElementAndHref(mContextMenuNode, getter_AddRefs(linkContent), href);

  // XXXdwh Handle simple XLINKs if we want to be compatible with Mozilla, but who
  // really uses these anyway? :)
  if (!linkContent || href.IsEmpty())
    return;

  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  if (!pref)
    return; // Something bad happened if we can't get prefs.

  NSString* hrefStr = [NSString stringWith_nsAString:href];

  PRBool loadInBackground;
  pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

  NSString* referrer = [[mBrowserView getBrowserView] getFocusedURLString];

  if (aUseWindow)
    [self openNewWindowWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground];
  else
    [self openNewTabWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground];
}

- (IBAction)savePageAs:(id)aSender
{
  [self saveDocument: nil filterList: nil];
}

- (IBAction)saveLinkAs:(id)aSender
{
  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  CHGeckoUtils::GetEnclosingLinkElementAndHref(mContextMenuNode, getter_AddRefs(linkContent), href);

  // XXXdwh Handle simple XLINKs if we want to be compatible with Mozilla, but who
  // really uses these anyway? :)
  if (!linkContent || href.IsEmpty())
    return;

  NSString* hrefStr = [NSString stringWith_nsAString: href];

  // The user wants to save this link.
  nsAutoString text;
  CHGeckoUtils::GatherTextUnder(mContextMenuNode, text);

  [self saveURL: nil filterList: nil
            url: hrefStr suggestedFilename: [NSString stringWith_nsAString: text]];
}

- (IBAction)saveImageAs:(id)aSender
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mContextMenuNode));
  if (imgElement) {
      nsAutoString text;
      imgElement->GetAttribute(NS_LITERAL_STRING("src"), text);
      nsAutoString url;
      imgElement->GetSrc(url);

      NSString* hrefStr = [NSString stringWith_nsAString: url];

      [self saveURL: nil filterList: nil
                url: hrefStr suggestedFilename: [NSString stringWith_nsAString: text]];
  }
}

- (IBAction)copyLinkLocation:(id)aSender
{
  CHBrowserView* view = [[self getBrowserWrapper] getBrowserView];
  if (!view) return;

  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([view getWebBrowser]);
  if (!webBrowser) return;

  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(webBrowser));
  if (clipboard)
    clipboard->CopyLinkLocation();
}



- (IBAction)viewOnlyThisImage:(id)aSender
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mContextMenuNode));
  if (imgElement) {
    nsAutoString url;
    imgElement->GetSrc(url);

    NSString* urlStr = [NSString stringWith_nsAString: url];
    NSString* referrer = [[mBrowserView getBrowserView] getFocusedURLString];
    
    [self loadURL: urlStr referrer:referrer];

    // Focus and activate our content area.
    [[mBrowserView getBrowserView] setActive: YES];
  }  
}

- (CHBookmarksToolbar*) bookmarksToolbar
{
  return mPersonalToolbar;
}


//
// updateLock:
//
// Sets the lock icon in the status bar to the appropriate image
//
- (void)updateLock:(unsigned int)inSecurityState
{
  switch ( inSecurityState & 0x000000FF ) {
    case nsIWebProgressListener::STATE_IS_INSECURE:
      [mLock setImage:[BrowserWindowController insecureIcon]];
      break;
    case nsIWebProgressListener::STATE_IS_SECURE:
      [mLock setImage:[BrowserWindowController secureIcon]];
      break;
    case nsIWebProgressListener::STATE_IS_BROKEN:
      [mLock setImage:[BrowserWindowController brokenIcon]];
      break;
  }
}

+ (NSImage*) insecureIcon
{
  static NSImage* sInsecureIcon = nil;
  if ( !sInsecureIcon )
    sInsecureIcon = [[NSImage imageNamed:@"globe_ico"] retain];
  return sInsecureIcon;
}

+ (NSImage*) secureIcon;
{
  static NSImage* sSecureIcon = nil;
  if ( !sSecureIcon )
    sSecureIcon = [[NSImage imageNamed:@"security_lock"] retain];
  return sSecureIcon;
}

+ (NSImage*) brokenIcon;
{
  static NSImage* sBrokenIcon = nil;
  if ( !sBrokenIcon )
    sBrokenIcon = [[NSImage imageNamed:@"security_broken"] retain];
  return sBrokenIcon;
}

- (void) focusChangedFrom:(NSResponder*) oldResponder to:(NSResponder*) newResponder
{
  BOOL oldResponderIsGecko = [self isResponderGeckoView:oldResponder];
  BOOL newResponderIsGecko = [self isResponderGeckoView:newResponder];

  if (oldResponderIsGecko != newResponderIsGecko)
    [[mBrowserView getBrowserView] setActive:newResponderIsGecko];
}

@end


@implementation ThrobberHandler

-(id)initWithToolbarItem:(NSToolbarItem*)inButton images:(NSArray*)inImages
{
  if ( (self = [super init]) ) {
    mImages = [inImages retain];
    mTimer = [[NSTimer scheduledTimerWithTimeInterval: 0.2
                        target: self selector: @selector(pulseThrobber:)
                        userInfo: inButton repeats: YES] retain];
    mFrame = 0;
    [self startThrobber];
  }
  return self;
}

-(void)dealloc
{
  [self stopThrobber];
  [mImages release];
}


// Called by an NSTimer.

- (void)pulseThrobber:(id)aSender
{
  // advance to next frame.
  if (++mFrame >= [mImages count])
      mFrame = 0;
  NSToolbarItem* toolbarItem = (NSToolbarItem*) [aSender userInfo];
  [toolbarItem setImage: [mImages objectAtIndex: mFrame]];
}

#define QUICKTIME_THROBBER 0

#if QUICKTIME_THROBBER
static Boolean movieControllerFilter(MovieController mc, short action, void *params, long refCon)
{
    if (action == mcActionMovieClick || action == mcActionMouseDown) {
        EventRecord* event = (EventRecord*) params;
        event->what = nullEvent;
        return true;
    }
    return false;
}
#endif

- (void)startThrobber
{
#if QUICKTIME_THROBBER
    // Use Quicktime to draw the frames from a single Animated GIF. This works fine for the animation, but
    // when the frames stop, the poster frame disappears.
    NSToolbarItem* throbberItem = [self throbberItem];
    if (throbberItem != nil && [throbberItem view] == nil) {
        NSSize minSize = [throbberItem minSize];
        NSLog(@"Origin minSize = %f X %f", minSize.width, minSize.height);
        NSSize maxSize = [throbberItem maxSize];
        NSLog(@"Origin maxSize = %f X %f", maxSize.width, maxSize.height);
        
        NSURL* throbberURL = [NSURL fileURLWithPath: [[NSBundle mainBundle] pathForResource:@"throbber" ofType:@"gif"]];
        NSLog(@"throbberURL = %@", throbberURL);
        NSMovie* throbberMovie = [[[NSMovie alloc] initWithURL: throbberURL byReference: YES] autorelease];
        NSLog(@"throbberMovie = %@", throbberMovie);
        
        if ([throbberMovie QTMovie] != nil) {
            NSMovieView* throbberView = [[[NSMovieView alloc] init] autorelease];
            [throbberView setMovie: throbberMovie];
            [throbberView showController: NO adjustingSize: NO];
            [throbberView setLoopMode: NSQTMovieLoopingPlayback];
            [throbberItem setView: throbberView];
            NSSize size = NSMakeSize(32, 32);
            [throbberItem setMinSize: size];
            [throbberItem setMaxSize: size];
            [throbberView gotoPosterFrame: self];
            [throbberView start: self];
    
            // experiment, veto mouse clicks in the movie controller by using an action filter.
            MCSetActionFilterWithRefCon((MovieController) [throbberView movieController],
                                        NewMCActionFilterWithRefConUPP(movieControllerFilter),
                                        0);
        }
    }
#else
#endif
}

- (void)stopThrobber
{
#if QUICKTIME_THROBBER
    // Stop the quicktime animation.
    NSToolbarItem* throbberItem = [self throbberItem];
    if (throbberItem != nil) {
        NSMovieView* throbberView = [throbberItem view];
        if ([throbberView isPlaying]) {
            [throbberView stop: self];
            [throbberView gotoPosterFrame: self];
        } else {
            [throbberView start: self];
        }
    }
#else
  if (mTimer) {
    [mTimer invalidate];
    [mTimer release];
    mTimer = nil;

    mFrame = 0;
  }
#endif
}

@end

