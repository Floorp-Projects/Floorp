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

#import "BrowserWindowController.h"
#import "CHBrowserWrapper.h"
#import "CHIconTabViewItem.h"

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
#include "CHGeckoUtils.h"

static NSString *BrowserToolbarIdentifier	= @"Browser Window Toolbar";
static NSString *BackToolbarItemIdentifier	= @"Back Toolbar Item";
static NSString *ForwardToolbarItemIdentifier	= @"Forward Toolbar Item";
static NSString *ReloadToolbarItemIdentifier	= @"Reload Toolbar Item";
static NSString *StopToolbarItemIdentifier	= @"Stop Toolbar Item";
static NSString *HomeToolbarItemIdentifier	= @"Home Toolbar Item";
static NSString *LocationToolbarItemIdentifier	= @"Location Toolbar Item";
static NSString *SidebarToolbarItemIdentifier	= @"Sidebar Toolbar Item";
static NSString *PrintToolbarItemIdentifier	= @"Print Toolbar Item";

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

-(void)windowDidBecomeKey: (NSNotification*)aNotification
{
  // May become necessary later.
}

-(void)windowDidResignKey: (NSNotification*)aNotification
{
  // May be needed later.
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
        if ( nsCocoaBrowserService::sNumBrowsers == 0 ) {
            [self setShouldCascadeWindows:NO];
        } else {
            [self setShouldCascadeWindows:YES];
        }
        mInitialized = NO;
        mMoveReentrant = NO;
        mShouldAutosave = YES;
        mChromeMask = 0;
        mContextMenuFlags = 0;
        mContextMenuEvent = nsnull;
        mContextMenuNode = nsnull;
    }
    return self;
}

-(void)autosaveWindowFrame
{
  if (mShouldAutosave)
    [[self window] saveFrameUsingName: @"NavigatorWindow"];
}

-(void)disableAutosave
{
  mShouldAutosave = NO;
}

- (void)windowWillClose:(NSNotification *)notification
{
  printf("Window will close notification.\n");
  [mSidebarBookmarksDataSource windowClosing];

  [self autosaveWindowFrame];
  [self autorelease];
}

- (void)dealloc
{
  printf("Browser controller died.\n");

  // Loop over all tabs, and tell them that the window is closed.
  int numTabs = [mTabBrowser numberOfTabViewItems];
  for (int i = 0; i < numTabs; i++) {
    NSTabViewItem* item = [mTabBrowser tabViewItemAtIndex: i];
    [[item view] windowClosed];
  }
  [mSidebarBrowserView windowClosed];

  [mProgress release];
  
  [super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];

  [[mURLBar cell] setImage: [NSImage imageNamed:@"smallbookmark"]];
  
    // Get our saved dimensions.
    [[self window] setFrameUsingName: @"NavigatorWindow"];
    
    if (mModalSession)
      [NSApp stopModal];
      
    mInitialized = YES;

    mDrawerCachedFrame = NO;
    
    // Retain with a single extra refcount.  This allows the CHBrowserWrappers
    // to remove the progress meter from its superview without having to 
    // worry about retaining and releasing it.
    [mProgress retain];
    
    [[self window] setAcceptsMouseMovedEvents: YES];
    
    [self setupToolbar];
    
//  03/03/2002 mlj Changing strategy a bit here.  The addTab: method was
//	duplicating a lot of the code found here.  I have moved it to that method.
//	We now remove the IB tab, then add one of our own.

    [mTabBrowser removeTabViewItem:[mTabBrowser tabViewItemAtIndex:0]];
    [self newTab];
    
    if (mURL) {
      [self loadURL: mURL];
      [mURL release];
    }
    
    [mSidebarDrawer setDelegate: self];

    [self setupSidebarTabs];

    [mPersonalToolbar initializeToolbar];
}

- (void)drawerWillOpen: (NSNotification*)aNotification
{
  [mSidebarBookmarksDataSource ensureBookmarks];

  // Force the window to shrink and move if necessary in order to accommodate the sidebar.
  NSRect screenFrame = [[[self window] screen] visibleFrame];
  NSRect windowFrame = [[self window] frame];
  NSSize drawerSize = [mSidebarDrawer contentSize];
  int fudgeFactor = 12; // Not sure how to get the drawer's border info, so we fudge it for now.
  drawerSize.width += fudgeFactor;
  if (windowFrame.origin.x + windowFrame.size.width + drawerSize.width >
      screenFrame.size.width) {
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
  //  [[mSidebarBrowserView getBrowserView] loadURI: [NSURL URLWithString: @"http://tinderbox.mozilla.org/SeaMonkey/panel.html"] flags:NSLoadFlagsNone];

  // Toggle the sidebar icon.
  [mSidebarToolbarItem setImage:[NSImage imageNamed:@"sidebarOpened"]];
}

- (void)drawerDidClose:(NSNotification *)aNotification
{
  // Unload the Gecko web page in "My Panels" to save memory.
  [mSidebarToolbarItem setImage:[NSImage imageNamed:@"sidebarClosed"]];

  // XXXdwh ignore for now.
  //  [[mSidebarBrowserView getBrowserView] loadURI: [NSURL URLWithString: @"about:blank"] flags:NSLoadFlagsNone];

  if (mDrawerCachedFrame) {
    printf("Got here.\n");
    mDrawerCachedFrame = NO;
    NSRect frame = [[self window] frame];
    if (frame.origin.x == mCachedFrameAfterDrawerOpen.origin.x &&
        frame.origin.y == mCachedFrameAfterDrawerOpen.origin.y &&
        frame.size.width == mCachedFrameAfterDrawerOpen.size.width &&
        frame.size.height == mCachedFrameAfterDrawerOpen.size.height) {
      printf("Got here too.\n");
      printf("Xes are %f %f\n", frame.origin.x, mCachedFrameAfterDrawerOpen.origin.x);
      printf("Widths are %f %f\n", frame.size.width, mCachedFrameAfterDrawerOpen.size.width);
      // Restore the original frame.
      [[self window] setFrame: mCachedFrameBeforeDrawerOpen display: YES];
    }
  }
}

- (void)setupToolbar
{
  if (mChromeMask) {
    printf("Uh-oh. %d\n", mChromeMask);
  }
  
    NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:BrowserToolbarIdentifier] autorelease];
    
    [toolbar setDisplayMode:NSToolbarDisplayModeDefault];
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:YES];
    [toolbar setDelegate:self];
    [[self window] setToolbar:toolbar];
}


- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:	BackToolbarItemIdentifier,
                                        ForwardToolbarItemIdentifier,
                                        ReloadToolbarItemIdentifier,
                                        StopToolbarItemIdentifier,
                                        HomeToolbarItemIdentifier,
                                        LocationToolbarItemIdentifier,
                                        SidebarToolbarItemIdentifier,
                                        PrintToolbarItemIdentifier,
                                        NSToolbarCustomizeToolbarItemIdentifier,
                                        NSToolbarFlexibleSpaceItemIdentifier,
                                        NSToolbarSpaceItemIdentifier,
                                        NSToolbarSeparatorItemIdentifier,
                                        nil];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:	BackToolbarItemIdentifier,
                                        ForwardToolbarItemIdentifier,
                                        ReloadToolbarItemIdentifier,
                                        StopToolbarItemIdentifier,
                                        HomeToolbarItemIdentifier,
                                        LocationToolbarItemIdentifier,
                                        SidebarToolbarItemIdentifier,
                                        nil];
}

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
        mSidebarToolbarItem = toolbarItem;
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
    if ( [[[self window] toolbar] isVisible] ) {
        if ( ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconAndLabel) ||
             ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconOnly) ) {
            [self focusURLBar];
        } else {
            [self beginLocationSheet];
        }
    } else {
        [self beginLocationSheet];
    }
}

- (void)focusURLBar
{
    [mURLBar selectText: self];
}

- (void)beginLocationSheet
{
    [NSApp beginSheet:	mLocationSheetWindow
       modalForWindow:	[self window]
        modalDelegate:	nil
       didEndSelector:	nil
          contextInfo:	nil];
}

- (IBAction)endLocationSheet:(id)sender
{
    [mLocationSheetWindow orderOut:self];
    [NSApp endSheet:mLocationSheetWindow returnCode:1];
    [self loadURL:[NSURL URLWithString:[mLocationSheetURLField stringValue]]];
    
    // Focus and activate our content area.
    [[mBrowserView getBrowserView] setActive: YES];
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

- (IBAction)goToLocationFromToolbarURLField:(id)sender
{
    [self loadURL:[NSURL URLWithString:[sender stringValue]]];
    
    // Focus and activate our content area.
    [[mBrowserView getBrowserView] setActive: YES];
}

- (void)saveDocument: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
{
  [[mBrowserView getBrowserView] saveDocument: aFilterView filterList: aFilterList];
}

- (void)saveURL: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
            url: (NSURL*)aURL suggestedFilename: (NSString*)aFilename
{
  [[mBrowserView getBrowserView] saveURL: aFilterView filterList: aFilterList
                                     url: aURL suggestedFilename: aFilename];
}

- (void)printDocument
{
    [[mBrowserView getBrowserView] printDocument];
}

- (void)printPreview
{
    [[mBrowserView getBrowserView] printPreview];
}

- (BOOL)findInPage:(NSString*)text
{
    return [[mBrowserView getBrowserView] findInPage:text];
}

- (void)addBookmarkExtended: (BOOL)aIsFromMenu isFolder:(BOOL)aIsFolder
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
  
  [mSidebarBookmarksDataSource addBookmark: self useSelection: useSel isFolder: aIsFolder];
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
  [[mBrowserView getBrowserView] loadURI:[NSURL URLWithString:@"about:blank"] flags:NSLoadFlagsNone];
}

- (IBAction)toggleSidebar:(id)aSender
{
    NSResponder* resp = [[self window] firstResponder];
    [[self window] makeFirstResponder: nil];
    
    if ( ([mSidebarDrawer state] == NSDrawerClosedState) || ([mSidebarDrawer state] == NSDrawerClosingState) ) 	{
        // XXXHack to bypass sidebar crashes.
        [mSidebarDrawer openOnEdge: NSMaxXEdge];
    } else {
        [mSidebarDrawer close];
    }
    
    [[self window] makeFirstResponder: resp];
}


-(void)loadURL:(NSURL*)aURL
{
    if (mInitialized) {
        [[mBrowserView getBrowserView] loadURI:aURL flags:NSLoadFlagsNone];
    }
    else {
        mURL = aURL;
        [mURL retain];
    }
}

- (void)updateLocationFields:(NSString *)locationString
{
/* //commenting this out because it doesn't work right yet.
    if ( [locationString length] > 30 ) {
        [[mLocationToolbarItem menuFormRepresentation] setTitle:
            [NSString stringWithFormat:@"Location: %@...", [locationString substringToIndex:31]]];
    } else {
        [[mLocationToolbarItem menuFormRepresentation] setTitle:
            [NSString stringWithFormat:@"Location: %@", locationString]];
    }
*/

    [mURLBar setStringValue:locationString];
    [mLocationSheetURLField setStringValue:locationString];

    [[self window] update];
    [[self window] display];
}

-(void)newTab
{
    NSTabViewItem* newTab = [[[NSTabViewItem alloc] initWithIdentifier: nil] autorelease];
    CHBrowserWrapper* newView = [[[CHBrowserWrapper alloc] initWithTab: newTab andWindow: [mTabBrowser window]] autorelease];
    
    [newTab setLabel: @"Untitled"];
    [newTab setView: newView];
    [mTabBrowser addTabViewItem: newTab];
    
    [[newView getBrowserView] loadURI:[NSURL URLWithString:@"about:blank"] flags:NSLoadFlagsNone];

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
    [mTabBrowser selectPreviousTabViewItem:self];
}

- (void)nextTab
{
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

-(id)getTabBrowser
{
  return mTabBrowser;
}

-(CHBrowserWrapper*)getBrowserWrapper
{
  return mBrowserView;
}

-(void)openNewWindowWithURL: (NSURL*)aURL loadInBackground: (BOOL)aLoadInBG
{
  // Autosave our dimensions before we open a new window.  That ensures the size ends up matching.
  [self autosaveWindowFrame];

  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];
  [browser loadURL: aURL];
  if (aLoadInBG)
    [[browser window] orderWindow: NSWindowBelow relativeTo: [[self window] windowNumber]];
  else {
    // Focus the content area and show the window.
    [browser enterModalSession];
    [[[browser getBrowserWrapper] getBrowserView] setActive: YES];
  }
}

-(void)openNewTabWithURL: (NSURL*)aURL loadInBackground: (BOOL)aLoadInBG
{
    NSTabViewItem* newTab = [[[NSTabViewItem alloc] initWithIdentifier: nil] autorelease];
    
    NSTabViewItem* selectedTab = [mTabBrowser selectedTabViewItem];
    int index = [mTabBrowser indexOfTabViewItem: selectedTab];
    [mTabBrowser insertTabViewItem: newTab atIndex: index+1];

    CHBrowserWrapper* newView = [[[CHBrowserWrapper alloc] initWithTab: newTab andWindow: [mTabBrowser window]] autorelease];
    [newView setTab: newTab];
    
    [newTab setLabel: @"Loading..."];
    [newTab setView: newView];

    [[newView getBrowserView] loadURI:aURL flags:NSLoadFlagsNone];

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
    CHIconTabViewItem *searchItem = [[CHIconTabViewItem alloc] initWithIdentifier:@"searchSidebarCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"searchicon"]];
    CHIconTabViewItem *panelsItem = [[CHIconTabViewItem alloc] initWithIdentifier:@"myPanelsCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"panel_icon"]];
    
    [bookItem   setView:[[mSidebarSourceTabView tabViewItemAtIndex:0] view]];
    [histItem   setView:[[mSidebarSourceTabView tabViewItemAtIndex:1] view]];
    [searchItem setView:[[mSidebarSourceTabView tabViewItemAtIndex:2] view]];
    [panelsItem setView:[[mSidebarSourceTabView tabViewItemAtIndex:3] view]];

    [mSidebarTabView removeTabViewItem:[mSidebarTabView tabViewItemAtIndex:0]];
    
    [mSidebarTabView insertTabViewItem:bookItem   atIndex:0];
    [mSidebarTabView insertTabViewItem:histItem   atIndex:1];
    [mSidebarTabView insertTabViewItem:searchItem atIndex:2];
    [mSidebarTabView insertTabViewItem:panelsItem atIndex:3];
    
    [mSidebarTabView selectFirstTabViewItem:self];
}

-(void)setChromeMask:(int)aMask
{
  mChromeMask = aMask;
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

  NSString* hrefStr = [NSString stringWithCharacters: href.get() length:nsCRT::strlen(href.get())];
  NSURL* urlToLoad = [NSURL URLWithString: hrefStr];

  PRBool loadInBackground;
  pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

  if (aUseWindow)
    [self openNewWindowWithURL: urlToLoad loadInBackground: loadInBackground];
  else
    [self openNewTabWithURL: urlToLoad loadInBackground: loadInBackground];
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

  NSString* hrefStr = [NSString stringWithCharacters: href.get() length:nsCRT::strlen(href.get())];
  NSURL* urlToSave = [NSURL URLWithString: hrefStr];

  // The user wants to save this link.
  nsAutoString text;
  CHGeckoUtils::GatherTextUnder(mContextMenuNode, text);

  [self saveURL: nil filterList: nil
            url: urlToSave suggestedFilename: [NSString stringWithCharacters: text.get()
                                                                      length:nsCRT::strlen(text.get())]];
}

- (IBAction)saveImageAs:(id)aSender
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mContextMenuNode));
  if (imgElement) {
      nsAutoString text;
      imgElement->GetAttribute(NS_LITERAL_STRING("src"), text);
      nsAutoString url;
      imgElement->GetSrc(url);

      NSString* hrefStr = [NSString stringWithCharacters: url.get() length:nsCRT::strlen(url.get())];
      NSURL* urlToSave = [NSURL URLWithString: hrefStr];

      [self saveURL: nil filterList: nil
                url: urlToSave suggestedFilename: [NSString stringWithCharacters: text.get()
                                                                          length:nsCRT::strlen(text.get())]];
  }
}

- (IBAction)viewOnlyThisImage:(id)aSender
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mContextMenuNode));
  if (imgElement) {
    nsAutoString url;
    imgElement->GetSrc(url);

    NSString* urlStr = [NSString stringWithCharacters: url.get() length:nsCRT::strlen(url.get())];
    NSURL* urlToView = [NSURL URLWithString: urlStr];

    [self loadURL: urlToView];

    // Focus and activate our content area.
    [[mBrowserView getBrowserView] setActive: YES];
  }  
}

- (NSView*) bookmarksToolbar
{
  return mPersonalToolbar;
}

@end

